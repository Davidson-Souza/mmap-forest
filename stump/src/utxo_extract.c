#include "utxo_extract.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hex.h"
#include "util.h"
#include "utreexo_proof.h"
#include "varint.h"

typedef struct {
    unsigned char* data;
    size_t len;
    size_t cap;
} byte_buffer;

static int buffer_writer_callback(const void* bytes, size_t size, void* userdata) {
    if (bytes == NULL || userdata == NULL) {
        return 1;
    }
    byte_buffer* buffer = (byte_buffer*)userdata;
    if (size == 0) {
        return 0;
    }
    if (SIZE_MAX - buffer->len < size) {
        return 1;
    }
    size_t needed = buffer->len + size;
    if (needed > buffer->cap) {
        size_t new_cap = buffer->cap ? buffer->cap * 2 : 64;
        if (new_cap < needed) {
            new_cap = needed;
        }
        unsigned char* new_data = (unsigned char*)realloc(buffer->data, new_cap);
        if (new_data == NULL) {
            return 1;
        }
        buffer->data = new_data;
        buffer->cap = new_cap;
    }
    memcpy(buffer->data + buffer->len, bytes, size);
    buffer->len += size;
    return 0;
}

static int collect_script_pubkey_bytes(const btck_ScriptPubkey* script_pubkey, unsigned char** out_bytes, size_t* out_len) {
    if (script_pubkey == NULL || out_bytes == NULL || out_len == NULL) {
        return 1;
    }
    byte_buffer buffer = {
        .data = NULL,
        .len = 0,
        .cap = 0,
    };
    if (btck_script_pubkey_to_bytes(script_pubkey, buffer_writer_callback, &buffer) != 0) {
        free(buffer.data);
        return 1;
    }
    *out_bytes = buffer.data;
    *out_len = buffer.len;
    return 0;
}

bool build_utxo_preimage_bytes(
    const unsigned char block_hash[32],
    const unsigned char txid[32],
    uint32_t vout,
    uint32_t header_code,
    uint64_t amount,
    const unsigned char* script,
    size_t script_len,
    unsigned char** out_bytes,
    size_t* out_len) {
    if (!block_hash || !txid || !script || !out_bytes || !out_len) {
        return false;
    }
    size_t varint_len = bitcoin_varint_size((uint64_t)script_len);
    size_t preimage_len = (sizeof(UTREEXO_TAG_V1) * 2) + 32 + 32 + 4 + 4 + 8 + varint_len + script_len;
    unsigned char* preimage = (unsigned char*)malloc(preimage_len);
    if (preimage == NULL) {
        return false;
    }

    size_t offset = 0;
    memcpy(preimage + offset, UTREEXO_TAG_V1, sizeof(UTREEXO_TAG_V1));
    offset += sizeof(UTREEXO_TAG_V1);
    memcpy(preimage + offset, UTREEXO_TAG_V1, sizeof(UTREEXO_TAG_V1));
    offset += sizeof(UTREEXO_TAG_V1);
    memcpy(preimage + offset, block_hash, 32);
    offset += 32;
    memcpy(preimage + offset, txid, 32);
    offset += 32;
    for (size_t i = 0; i < 4; ++i) {
        preimage[offset + i] = (unsigned char)((vout >> (8 * i)) & 0xff);
    }
    offset += 4;
    for (size_t i = 0; i < 4; ++i) {
        preimage[offset + i] = (unsigned char)((header_code >> (8 * i)) & 0xff);
    }
    offset += 4;
    for (size_t i = 0; i < 8; ++i) {
        preimage[offset + i] = (unsigned char)((amount >> (8 * i)) & 0xff);
    }
    offset += 8;
    offset += bitcoin_write_varint(preimage + offset, (uint64_t)script_len);
    memcpy(preimage + offset, script, script_len);
    offset += script_len;

    *out_bytes = preimage;
    *out_len = preimage_len;
    return true;
}

bool compute_utxo_hash(
    const unsigned char block_hash[32],
    const unsigned char txid[32],
    uint32_t vout,
    uint32_t header_code,
    uint64_t amount,
    const unsigned char* script,
    size_t script_len,
    unsigned char out_hash[32]) {
    if (!block_hash || !txid || !script || !out_hash) {
        return false;
    }

    unsigned char* preimage = NULL;
    size_t preimage_len = 0;
    if (!build_utxo_preimage_bytes(
            block_hash,
            txid,
            vout,
            header_code,
            amount,
            script,
            script_len,
            &preimage,
            &preimage_len)) {
        return false;
    }

    sha512_256(out_hash, preimage, preimage_len);
    free(preimage);
    return true;
}

static bool append_utxo_hash(utxo_hashes* result, size_t* capacity, const utreexo_sha512_256* hash) {
    if (result == NULL || capacity == NULL || hash == NULL) {
        return false;
    }
    if (result->count == *capacity) {
        size_t new_cap = (*capacity == 0) ? 64 : (*capacity * 2);
        utreexo_sha512_256* new_hashes = (utreexo_sha512_256*)realloc(result->hashes, new_cap * sizeof(utreexo_sha512_256));
        if (new_hashes == NULL) {
            return false;
        }
        result->hashes = new_hashes;
        *capacity = new_cap;
    }
    result->hashes[result->count++] = *hash;
    return true;
}

utxo_hashes extract_utreexo_add_hashes(
    btck_Block* block,
    uint32_t block_height,
    const unsigned char block_hash_bytes[32]) {
    utxo_hashes result = {
        .hashes = NULL,
        .count = 0,
    };
    if (block == NULL || block_hash_bytes == NULL) {
        return result;
    }

    size_t capacity = 0;
    size_t tx_count = btck_block_count_transactions(block);
    for (size_t i = 0; i < tx_count; ++i) {
        const btck_Transaction* tx = btck_block_get_transaction_at(block, i);
        if (tx == NULL) {
            fprintf(stderr, "Failed to read transaction %zu\n", i);
            continue;
        }

        const btck_Txid* txid = btck_transaction_get_txid(tx);
        if (txid == NULL) {
            fprintf(stderr, "Transaction %zu is missing a txid\n", i);
            continue;
        }

        unsigned char txid_bytes[32];
        btck_txid_to_bytes(txid, txid_bytes);

        unsigned char txid_bytes_reversed[32];
        for (size_t j = 0; j < sizeof(txid_bytes); ++j) {
            txid_bytes_reversed[j] = txid_bytes[sizeof(txid_bytes) - 1 - j];
        }

        char* txid_hex = uint8_array_to_hex_string(txid_bytes_reversed);
        if (txid_hex == NULL) {
            fprintf(stderr, "Failed to format txid for transaction %zu\n", i);
            continue;
        }

        size_t output_count = btck_transaction_count_outputs(tx);
        bool is_coinbase = (i == 0);
        uint32_t header_code = (block_height << 1) | (is_coinbase ? 1U : 0U);
        for (size_t output_index = 0; output_index < output_count; ++output_index) {
            if (output_index > UINT32_MAX) {
                fprintf(stderr, "Output index too large for transaction %zu\n", i);
                break;
            }
            const btck_TransactionOutput* output = btck_transaction_get_output_at(tx, output_index);
            if (output == NULL) {
                fprintf(stderr, "Failed to fetch output %zu for transaction %zu\n", output_index, i);
                continue;
            }

            int64_t amount = btck_transaction_output_get_amount(output);
            if (amount < 0) {
                fprintf(stderr, "Encountered negative amount for transaction %zu output %zu\n", i, output_index);
                continue;
            }

            const btck_ScriptPubkey* script_pubkey = btck_transaction_output_get_script_pubkey(output);
            unsigned char* script_bytes = NULL;
            size_t script_len = 0;
            if (collect_script_pubkey_bytes(script_pubkey, &script_bytes, &script_len) != 0) {
                fprintf(stderr, "Failed to serialize script pubkey for transaction %zu output %zu\n", i, output_index);
                continue;
            }

            if (script_len > 10000 || (script_len > 0 && script_bytes[0] == 0x6a)) {
                free(script_bytes);
                continue;
            }

            unsigned char utxo_hash[32];
            bool hash_ok = compute_utxo_hash(
                block_hash_bytes,
                txid_bytes,
                (uint32_t)output_index,
                header_code,
                (uint64_t)amount,
                script_bytes,
                script_len,
                utxo_hash);
            free(script_bytes);
            if (!hash_ok) {
                fprintf(stderr, "Failed to compute UTXO hash for transaction %zu output %zu\n", i, output_index);
                continue;
            }

            utreexo_sha512_256 utxo_hash_struct;
            memcpy(utxo_hash_struct.hash, utxo_hash, sizeof(utxo_hash_struct.hash));

            if (!append_utxo_hash(&result, &capacity, &utxo_hash_struct)) {
                fprintf(stderr, "Failed to record UTXO hash for transaction %zu output %zu\n", i, output_index);
                continue;
            }

            char* utxo_hex = uint8_array_to_hex_string(utxo_hash);
            if (utxo_hex == NULL) {
                fprintf(stderr, "Failed to format UTXO hash for transaction %zu output %zu\n", i, output_index);
                continue;
            }

            printf("tx %zu output %zu: txhash=%s utxo_hash=%s\n",
                   i,
                   output_index,
                   txid_hex,
                   utxo_hex);
            free(utxo_hex);
        }

        free(txid_hex);
    }

    return result;
}

void free_utxo_hashes(utxo_hashes* hashes) {
    if (hashes == NULL) {
        return;
    }
    free(hashes->hashes);
    hashes->hashes = NULL;
    hashes->count = 0;
}
