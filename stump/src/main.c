#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "hex.h"
#include "stump.h"
#include "parent_hash.h"
#include "util.h"
#include "utreexo_proof.h"
#include "utreexo_proof_io.h"
#include "utreexo_roots_io.h"
#include "utxo_extract.h"
#include "target_preimage.h"
#include <kernel/bitcoinkernel.h>
#include "stddef.h"

typedef struct {
    const target_preimage* entry;
    btck_ScriptPubkey* script_pubkey;
    btck_TransactionOutput* tx_output;
    bool matched;
} target_preimage_context;

static void destroy_target_preimage_contexts(target_preimage_context* contexts, size_t count) {
    if (contexts == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (contexts[i].tx_output != NULL) {
            btck_transaction_output_destroy(contexts[i].tx_output);
            contexts[i].tx_output = NULL;
        }
        if (contexts[i].script_pubkey != NULL) {
            btck_script_pubkey_destroy(contexts[i].script_pubkey);
            contexts[i].script_pubkey = NULL;
        }
        contexts[i].entry = NULL;
        contexts[i].matched = false;
    }
    free(contexts);
}

static target_preimage_context* find_target_preimage_context(
    const unsigned char txid[32],
    uint32_t vout,
    target_preimage_context* contexts,
    size_t count) {
    if (txid == NULL || contexts == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        if (contexts[i].entry == NULL) {
            continue;
        }
        if (contexts[i].entry->vout == vout &&
                memcmp(contexts[i].entry->txid, txid, 32) == 0) {
            return &contexts[i];
        }
    }
    return NULL;
}

static const char* script_verify_status_to_string(btck_ScriptVerifyStatus status) {
    switch (status) {
        case btck_ScriptVerifyStatus_OK:
            return "OK";
        case btck_ScriptVerifyStatus_ERROR_INVALID_FLAGS_COMBINATION:
            return "ERROR_INVALID_FLAGS_COMBINATION";
        case btck_ScriptVerifyStatus_ERROR_SPENT_OUTPUTS_REQUIRED:
            return "ERROR_SPENT_OUTPUTS_REQUIRED";
        default:
            return "UNKNOWN";
    }
}

static bool initialize_target_preimage_contexts(
    const target_preimages* preimages,
    target_preimage_context** out_contexts) {
    if (preimages == NULL || out_contexts == NULL) {
        return false;
    }
    *out_contexts = NULL;
    if (preimages->count == 0) {
        return true;
    }

    target_preimage_context* contexts =
        (target_preimage_context*)calloc(preimages->count, sizeof(target_preimage_context));
    if (contexts == NULL) {
        fprintf(stderr, "Failed to allocate target preimage context array\n");
        return false;
    }

    for (size_t i = 0; i < preimages->count; ++i) {
        contexts[i].entry = &preimages->entries[i];
        contexts[i].matched = false;

        if (preimages->entries[i].amount > INT64_MAX) {
            fprintf(stderr, "Target preimage %zu amount exceeds int64 range\n", i);
            destroy_target_preimage_contexts(contexts, preimages->count);
            return false;
        }

        contexts[i].script_pubkey = btck_script_pubkey_create(
            preimages->entries[i].script,
            preimages->entries[i].script_len);
        if (contexts[i].script_pubkey == NULL) {
            fprintf(stderr, "Failed to build script pubkey for target preimage %zu\n", i);
            destroy_target_preimage_contexts(contexts, preimages->count);
            return false;
        }

        contexts[i].tx_output = btck_transaction_output_create(
            contexts[i].script_pubkey,
            (int64_t)preimages->entries[i].amount);
        if (contexts[i].tx_output == NULL) {
            fprintf(stderr, "Failed to create transaction output for target preimage %zu\n", i);
            destroy_target_preimage_contexts(contexts, preimages->count);
            return false;
        }
    }

    *out_contexts = contexts;
    return true;
}

static bool verify_target_preimage_scripts(
    btck_Block* block,
    const target_preimages* preimages,
    target_preimage_context* contexts) {
    if (block == NULL || preimages == NULL) {
        return false;
    }
    if (preimages->count == 0) {
        return true;
    }

    bool overall_success = true;
    size_t verified_scripts = 0;
    size_t tx_count = btck_block_count_transactions(block);
    for (size_t tx_index = 0; tx_index < tx_count; ++tx_index) {
        if (tx_index == 0) {
            // Coinbase has no corresponding target preimages.
            continue;
        }

        const btck_Transaction* tx = btck_block_get_transaction_at(block, tx_index);
        if (tx == NULL) {
            fprintf(stderr, "Failed to access transaction %zu while verifying scripts\n", tx_index);
            overall_success = false;
            continue;
        }

        size_t input_count = btck_transaction_count_inputs(tx);
        if (input_count == 0) {
            continue;
        }

        target_preimage_context** ctx_per_input =
            (target_preimage_context**)calloc(input_count, sizeof(target_preimage_context*));
        const btck_TransactionOutput** spent_outputs =
            (const btck_TransactionOutput**)calloc(input_count, sizeof(btck_TransactionOutput*));
        if (ctx_per_input == NULL || spent_outputs == NULL) {
            fprintf(stderr, "Failed to allocate verification buffers for transaction %zu\n", tx_index);
            free(ctx_per_input);
            free(spent_outputs);
            return false;
        }

        bool tx_has_all_preimages = true;
        for (size_t input_index = 0; input_index < input_count; ++input_index) {
            const btck_TransactionInput* input = btck_transaction_get_input_at(tx, input_index);
            if (input == NULL) {
                fprintf(stderr, "Failed to read transaction %zu input %zu\n", tx_index, input_index);
                tx_has_all_preimages = false;
                break;
            }
            const btck_TransactionOutPoint* prevout = btck_transaction_input_get_out_point(input);
            if (prevout == NULL) {
                fprintf(stderr, "Failed to access prevout for transaction %zu input %zu\n", tx_index, input_index);
                tx_has_all_preimages = false;
                break;
            }
            const btck_Txid* prev_txid = btck_transaction_out_point_get_txid(prevout);
            if (prev_txid == NULL) {
                fprintf(stderr, "Missing prev txid for transaction %zu input %zu\n", tx_index, input_index);
                tx_has_all_preimages = false;
                break;
            }

            unsigned char prev_txid_bytes[32];
            btck_txid_to_bytes(prev_txid, prev_txid_bytes);
            uint32_t prev_index = btck_transaction_out_point_get_index(prevout);

            target_preimage_context* ctx = find_target_preimage_context(
                prev_txid_bytes,
                prev_index,
                contexts,
                preimages->count);
            if (ctx == NULL) {
                fprintf(stderr,
                        "Missing target preimage for transaction %zu input %zu (vout=%u)\n",
                        tx_index,
                        input_index,
                        prev_index);
                tx_has_all_preimages = false;
                break;
            }
            ctx->matched = true;
            ctx_per_input[input_index] = ctx;
            spent_outputs[input_index] = ctx->tx_output;
        }

        if (tx_has_all_preimages) {
            for (size_t input_index = 0; input_index < input_count; ++input_index) {
                target_preimage_context* ctx = ctx_per_input[input_index];
                if (ctx == NULL) {
                    overall_success = false;
                    continue;
                }
                btck_ScriptVerifyStatus status = btck_ScriptVerifyStatus_OK;
                int verify_result = btck_script_pubkey_verify(
                    ctx->script_pubkey,
                    (int64_t)ctx->entry->amount,
                    tx,
                    spent_outputs,
                    input_count,
                    (unsigned int)input_index,
                    btck_ScriptVerificationFlags_ALL,
                    &status);
                if (verify_result != 1) {
                    overall_success = false;
                    fprintf(stderr,
                            "Script verification failed for transaction %zu input %zu (status=%s)\n",
                            tx_index,
                            input_index,
                            script_verify_status_to_string(status));
                } else {
                    verified_scripts++;
                }
            }
        } else {
            overall_success = false;
        }

        free(spent_outputs);
        free(ctx_per_input);
    }

    for (size_t i = 0; i < preimages->count; ++i) {
        if (!contexts[i].matched) {
            overall_success = false;
            fprintf(stderr, "Target preimage %zu was not matched to any transaction input\n", i);
        }
    }

    if (overall_success) {
        printf("Verified %zu target preimage script(s)\n", verified_scripts);
    }

    return overall_success;
}

static bool build_deletion_hashes_from_contexts(
    const target_preimage_context* contexts,
    size_t context_count,
    utreexo_sha512_256** out_hashes) {
    if (out_hashes == NULL) {
        return false;
    }
    *out_hashes = NULL;
    if (context_count == 0) {
        return true;
    }
    utreexo_sha512_256* hashes =
        (utreexo_sha512_256*)calloc(context_count, sizeof(utreexo_sha512_256));
    if (hashes == NULL) {
        fprintf(stderr, "Failed to allocate deletion hash buffer\n");
        return false;
    }
    static const unsigned char empty_script_placeholder = 0;
    for (size_t i = 0; i < context_count; ++i) {
        const target_preimage_context* ctx = &contexts[i];
        if (ctx->entry == NULL) {
            fprintf(stderr, "Target preimage context %zu is missing an entry\n", i);
            free(hashes);
            return false;
        }
        const unsigned char* script_bytes =
            (ctx->entry->script_len == 0) ? &empty_script_placeholder : ctx->entry->script;
        unsigned char hash_bytes[32];
        if (!compute_utxo_hash(
                ctx->entry->block_hash,
                ctx->entry->txid,
                ctx->entry->vout,
                ctx->entry->header_code,
                ctx->entry->amount,
                script_bytes,
                ctx->entry->script_len,
                hash_bytes)) {
            fprintf(stderr, "Failed to compute deletion hash for target preimage %zu\n", i);
            free(hashes);
            return false;
        }
        memcpy(hashes[i].hash, hash_bytes, sizeof(hash_bytes));
    }
    *out_hashes = hashes;
    return true;
}


void makeforest(utreexo_sha512_256* forest, uint64_t numleaves) {
    int total = (numleaves*2)-1;
    uint8_t total_rows = tree_rows(numleaves);
    for (uint64_t i = 0; i < total; i++) {
        if (i < numleaves) {
            hash_from_u8(forest[i].hash, i);
            continue;
        }

        utreexo_sha512_256 left = forest[left_child(i, total_rows)];
        utreexo_sha512_256 right = forest[right_child(i, total_rows)];
        parent_hash(forest[i].hash, left.hash, right.hash);
    }
}

void prove(utreexo_sha512_256* forest, proof* my_proof, uint64_t num_leaves) {
    uint8_t total_rows = tree_rows(num_leaves);
    uint64_t* proof_pos = (uint64_t*)malloc((my_proof->target_count*(total_rows+1)) * sizeof(uint64_t));
    int proof_count = proof_positions(my_proof->target_count, my_proof->targets, num_leaves, total_rows, proof_pos);

    printf("proof count %d\n", proof_count);
    for (uint64_t i = 0; i < proof_count; i++) {
        printf("proof pos %ld\n", proof_pos[i]);
    }

    my_proof->proof_count = proof_count;
    my_proof->proof_hashes = (utreexo_sha512_256*)malloc(proof_count * sizeof(utreexo_sha512_256));
    for (size_t i = 0; i < proof_count; i++) {
        my_proof->proof_hashes[i] = forest[proof_pos[i]];
    }
    free(proof_pos);
}

void printforest(utreexo_sha512_256* forest, uint64_t numleaves) {
    int total = (numleaves*2)-1;

    printf("total: %d\n", total);
    for (uint64_t i = 0; i < total; i++) {
        printf("%ld. %s\n", i,
                uint8_array_to_hex_string(forest[i].hash));
    }
}


static bool parse_chain_type(const char* name, btck_ChainType* out_type) {
    if (name == NULL || out_type == NULL) {
        return false;
    }

    const char* chains[][3] = {
        {"mainnet", "main", NULL},
        {"testnet", "test", NULL},
        {"testnet4", "testnet-4", "test4"},
        {"signet", NULL, NULL},
        {"regtest", "reg", NULL},
    };
    const btck_ChainType types[] = {
        btck_ChainType_MAINNET,
        btck_ChainType_TESTNET,
        btck_ChainType_TESTNET_4,
        btck_ChainType_SIGNET,
        btck_ChainType_REGTEST,
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        for (size_t j = 0; j < 3; ++j) {
            const char* candidate = (j == 0) ? chains[i][0] : chains[i][j];
            if (candidate == NULL) {
                continue;
            }
            const char* ptr_name = name;
            const char* ptr_candidate = candidate;
            bool match = true;
            while (*ptr_name != '\0' && *ptr_candidate != '\0') {
                if (tolower((unsigned char)*ptr_name) != tolower((unsigned char)*ptr_candidate)) {
                    match = false;
                    break;
                }
                ++ptr_name;
                ++ptr_candidate;
            }
            if (match && *ptr_name == '\0' && *ptr_candidate == '\0') {
                *out_type = types[i];
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    btck_ChainType chain_type = btck_ChainType_MAINNET;
    int arg_index = 1;

    if (argc > arg_index && strncmp(argv[arg_index], "--chain", 7) == 0) {
        const char* chain_value = NULL;
        if (argv[arg_index][7] == '=') {
            chain_value = argv[arg_index] + 8;
            if (*chain_value == '\0') {
                chain_value = NULL;
            }
        } else {
            if (argc <= arg_index + 1) {
                fprintf(stderr, "Usage: %s [--chain=<chain>] <block height> <hex-encoded block> <hex roots blob> <hex proof blob> <hex target preimages blob>\n", argv[0]);
                return 1;
            }
            chain_value = argv[arg_index + 1];
            arg_index++;
        }
        arg_index++;

        if (chain_value == NULL || !parse_chain_type(chain_value, &chain_type)) {
            fprintf(stderr, "Unknown chain type: %s\n", chain_value ? chain_value : "(empty)");
            fprintf(stderr, "Supported chains: mainnet, testnet, testnet4, signet, regtest\n");
            return 1;
        }
    }

    if (argc - arg_index < 5) {
        fprintf(stderr, "Usage: %s [--chain=<chain>] <block height> <hex-encoded block> <hex roots blob> <hex proof blob> <hex target preimages blob>\n", argv[0]);
        return 1;
    }

    const char* height_arg = argv[arg_index];
    const char* block_arg = argv[arg_index + 1];
    const char* roots_arg = argv[arg_index + 2];
    const char* proof_arg = argv[arg_index + 3];
    const char* target_preimages_arg = argv[arg_index + 4];

    errno = 0;
    char* height_end = NULL;
    unsigned long long parsed_height = strtoull(height_arg, &height_end, 10);
    if (errno != 0 || height_end == height_arg || *height_end != '\0' || parsed_height > UINT32_MAX) {
        fprintf(stderr, "Invalid block height: %s\n", height_arg);
        return 1;
    }
    uint32_t block_height = (uint32_t)parsed_height;

    size_t block_len = 0;
    unsigned char* block_data = hex_to_bytes(block_arg, &block_len);
    if (block_data == NULL) {
        return 1;
    }

    size_t roots_len = 0;
    unsigned char* roots_blob = hex_to_bytes(roots_arg, &roots_len);
    if (roots_blob == NULL) {
        fprintf(stderr, "Failed to decode roots blob\n");
        free(block_data);
        return 1;
    }
    parsed_roots roots_data;
    if (!parse_utreexo_roots_bytes(roots_blob, roots_len, &roots_data)) {
        free(block_data);
        free(roots_blob);
        return 1;
    }
    free(roots_blob);

    size_t proof_blob_len = 0;
    unsigned char* proof_blob = hex_to_bytes(proof_arg, &proof_blob_len);
    if (proof_blob == NULL) {
        fprintf(stderr, "Failed to decode proof blob\n");
        free(block_data);
        free_utreexo_roots(&roots_data);
        return 1;
    }
    proof parsed_proof;
    target_preimages parsed_target_preimages = {
        .entries = NULL,
        .count = 0,
    };
    target_preimage_context* target_contexts = NULL;
    utreexo_sha512_256* del_hashes = NULL;
    if (!parse_utreexo_proof_bytes(proof_blob, proof_blob_len, &parsed_proof)) {
        free(block_data);
        free(proof_blob);
        free_utreexo_roots(&roots_data);
        return 1;
    }
    free(proof_blob);

    size_t target_preimages_len = 0;
    unsigned char* target_preimages_blob = hex_to_bytes(target_preimages_arg, &target_preimages_len);
    if (target_preimages_blob == NULL) {
        fprintf(stderr, "Failed to decode target preimages blob\n");
        free(block_data);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        return 1;
    }
    if (!parse_target_preimages_blob(
            target_preimages_blob,
            target_preimages_len,
            parsed_proof.target_count,
            &parsed_target_preimages)) {
        free(block_data);
        free(target_preimages_blob);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        return 1;
    }
    free(target_preimages_blob);

    if (!initialize_target_preimage_contexts(&parsed_target_preimages, &target_contexts)) {
        free(block_data);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }

    printf("Loaded %llu Utreexo root(s), %ld numleaves, and proof with %u target(s) / %u hash(es)\n",
            (unsigned long long)roots_data.count,
            roots_data.numleaves,
            parsed_proof.target_count,
            parsed_proof.proof_count);
    printf("Loaded %zu target preimage(s)\n", parsed_target_preimages.count);

    btck_Block* block = btck_block_create(block_data, block_len);
    free(block_data);
    if (block == NULL) {
        fprintf(stderr, "Failed to create block from provided data\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }

    btck_ChainParameters* chain_params = btck_chain_parameters_create(chain_type);
    if (chain_params == NULL) {
        fprintf(stderr, "Failed to create chain parameters for validation\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }
    const btck_ConsensusParams* consensus_params = btck_chain_parameters_get_consensus_params(chain_params);
    if (consensus_params == NULL) {
        fprintf(stderr, "Failed to fetch consensus parameters\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_chain_parameters_destroy(chain_params);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }
    btck_BlockValidationState* validation_state = btck_block_validation_state_create();
    if (validation_state == NULL) {
        fprintf(stderr, "Failed to create validation state\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_chain_parameters_destroy(chain_params);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }

    if (btck_block_check(block, consensus_params, btck_BlockCheckFlags_ALL, validation_state) != 1) {
        btck_ValidationMode mode = btck_block_validation_state_get_validation_mode(validation_state);
        btck_BlockValidationResult result =
            btck_block_validation_state_get_block_validation_result(validation_state);
        fprintf(stderr, "Block validation failed (mode=%u result=%u)\n", (unsigned)mode, (unsigned)result);
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_block_validation_state_destroy(validation_state);
        btck_chain_parameters_destroy(chain_params);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }
    btck_block_validation_state_destroy(validation_state);
    btck_chain_parameters_destroy(chain_params);

    btck_BlockHash* block_hash_obj = btck_block_get_hash(block);
    if (block_hash_obj == NULL) {
        fprintf(stderr, "Failed to compute block hash\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }
    unsigned char block_hash_bytes[32];
    btck_block_hash_to_bytes(block_hash_obj, block_hash_bytes);
    btck_block_hash_destroy(block_hash_obj);

    stump s;
    s.merkle_roots.size = roots_data.count;
    for (size_t i = 0; i < roots_data.count && i < sizeof(s.merkle_roots.roots) / sizeof(s.merkle_roots.roots[0]); i++) {
        s.merkle_roots.roots[i] = roots_data.hashes[i];
    }
    s.num_leaves = roots_data.numleaves;

    size_t tx_count = btck_block_count_transactions(block);
    printf("Parsed block containing %zu transaction(s)\n", tx_count);

    if (!verify_target_preimage_scripts(block, &parsed_target_preimages, target_contexts)) {
        fprintf(stderr, "Failed to verify at least one target preimage script\n");
        destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
        btck_block_destroy(block);
        free_utreexo_roots(&roots_data);
        free_utreexo_proof(&parsed_proof);
        free_target_preimages(&parsed_target_preimages);
        return 1;
    }

    if (parsed_target_preimages.count > 0) {
        if (!build_deletion_hashes_from_contexts(target_contexts, parsed_target_preimages.count, &del_hashes)) {
            fprintf(stderr, "Failed to build deletion hashes from target preimages\n");
            destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
            btck_block_destroy(block);
            free_utreexo_roots(&roots_data);
            free_utreexo_proof(&parsed_proof);
            free_target_preimages(&parsed_target_preimages);
            return 1;
        }
        s = del(s, parsed_proof, parsed_target_preimages.count, del_hashes);
        free(del_hashes);
        del_hashes = NULL;
    }

    utxo_hashes utxo_hashes_result = extract_utreexo_add_hashes(
        block,
        block_height,
        block_hash_bytes);

    for (int i = 0; i < utxo_hashes_result.count; i++) {
        s = add(s, utxo_hashes_result.hashes[i]);
    }

    printf("stump numleaves(%ld):\n", s.num_leaves);
    for (int i = 0; i < s.merkle_roots.size; i++) {
        printf("    %d %s\n", i, uint8_array_to_hex_string(s.merkle_roots.roots[i].hash));
    }

    free_utxo_hashes(&utxo_hashes_result);
    destroy_target_preimage_contexts(target_contexts, parsed_target_preimages.count);
    free_target_preimages(&parsed_target_preimages);

    btck_block_destroy(block);
    free_utreexo_roots(&roots_data);
    free_utreexo_proof(&parsed_proof);
}
