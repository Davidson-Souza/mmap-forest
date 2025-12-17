#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utreexo_proof_io.h"
#include "varint.h"

static bool read_uint64_le(const unsigned char* data, size_t len, size_t* offset, uint64_t* out) {
    if (data == NULL || offset == NULL || out == NULL) {
        return false;
    }
    if (*offset > len || len - *offset < sizeof(uint64_t)) {
        return false;
    }

    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        value |= ((uint64_t)data[*offset + i]) << (8 * i);
    }
    *offset += sizeof(uint64_t);
    *out = value;
    return true;
}

bool parse_utreexo_proof_bytes(const unsigned char* data, size_t len, proof* parsed_proof) {
    if (parsed_proof == NULL) {
        return false;
    }
    memset(parsed_proof, 0, sizeof(*parsed_proof));

    size_t offset = 0;
    uint64_t target_count64 = 0;
    if (!read_varint(data, len, &offset, &target_count64)) {
        fprintf(stderr, "Failed to read proof target count\n");
        return false;
    }
    if (target_count64 > UINT32_MAX) {
        fprintf(stderr, "Proof target count exceeds 32-bit limit\n");
        return false;
    }

    if (target_count64 > 0) {
        if (target_count64 > SIZE_MAX / sizeof(uint64_t)) {
            fprintf(stderr, "Proof target allocation too large\n");
            return false;
        }
        parsed_proof->targets = (uint64_t*)malloc(target_count64 * sizeof(uint64_t));
        if (parsed_proof->targets == NULL) {
            fprintf(stderr, "Failed to allocate proof targets\n");
            return false;
        }
        for (uint64_t i = 0; i < target_count64; ++i) {
            if (!read_varint(data, len, &offset, &parsed_proof->targets[i])) {
                fprintf(stderr, "Insufficient bytes reading proof target %llu\n", (unsigned long long)i);
                free(parsed_proof->targets);
                parsed_proof->targets = NULL;
                return false;
            }
            printf("read %ld", parsed_proof->targets[i]);
        }
    }
    parsed_proof->target_count = (uint32_t)target_count64;

    uint64_t proof_hash_count64 = 0;
    if (!read_varint(data, len, &offset, &proof_hash_count64)) {
        fprintf(stderr, "Failed to read proof hash count\n");
        free(parsed_proof->targets);
        parsed_proof->targets = NULL;
        return false;
    }
    if (proof_hash_count64 > UINT32_MAX) {
        fprintf(stderr, "Proof hash count exceeds 32-bit limit\n");
        free(parsed_proof->targets);
        parsed_proof->targets = NULL;
        return false;
    }

    if (proof_hash_count64 > 0) {
        if (proof_hash_count64 > SIZE_MAX / sizeof(utreexo_sha512_256)) {
            fprintf(stderr, "Proof hash allocation too large\n");
            free(parsed_proof->targets);
            parsed_proof->targets = NULL;
            return false;
        }
        parsed_proof->proof_hashes = (utreexo_sha512_256*)malloc(proof_hash_count64 * sizeof(utreexo_sha512_256));
        if (parsed_proof->proof_hashes == NULL) {
            fprintf(stderr, "Failed to allocate proof hashes\n");
            free(parsed_proof->targets);
            parsed_proof->targets = NULL;
            return false;
        }
        size_t hash_bytes_needed = proof_hash_count64 * 32;
        if (offset > len || len - offset < hash_bytes_needed) {
            fprintf(stderr, "Not enough bytes for %llu proof hashes\n", (unsigned long long)proof_hash_count64);
            free(parsed_proof->targets);
            parsed_proof->targets = NULL;
            free(parsed_proof->proof_hashes);
            parsed_proof->proof_hashes = NULL;
            return false;
        }
        for (uint64_t i = 0; i < proof_hash_count64; ++i) {
            memcpy(parsed_proof->proof_hashes[i].hash, data + offset + (size_t)i * 32, 32);
        }
        offset += hash_bytes_needed;
    }
    parsed_proof->proof_count = (uint32_t)proof_hash_count64;

    if (offset != len) {
        fprintf(stderr, "Unexpected extra bytes after proof encoding\n");
        free(parsed_proof->targets);
        parsed_proof->targets = NULL;
        free(parsed_proof->proof_hashes);
        parsed_proof->proof_hashes = NULL;
        parsed_proof->target_count = 0;
        parsed_proof->proof_count = 0;
        return false;
    }

    return true;
}

void free_utreexo_proof(proof* parsed_proof) {
    if (parsed_proof == NULL) {
        return;
    }
    free(parsed_proof->targets);
    free(parsed_proof->proof_hashes);
    parsed_proof->targets = NULL;
    parsed_proof->proof_hashes = NULL;
    parsed_proof->target_count = 0;
    parsed_proof->proof_count = 0;
}
