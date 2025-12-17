#include <stdlib.h>

#include "util.h"

typedef struct {
    utreexo_sha512_256 hash;
    uint64_t position;
} hashpos;

static int compare_uint64_t(const void *x, const void *y) {
    uint64_t a = *(uint64_t*)x;
    uint64_t b = *(uint64_t*)y;

    return (a > b) - (a < b);  // return -1, 0, or 1
}

int proof_positions(size_t target_count, uint64_t* orig_targets, uint64_t num_leaves, uint8_t total_rows, uint64_t* proof_positions) {
    size_t count = 0;
    uint64_t* targets = (uint64_t*)malloc(target_count * sizeof(uint64_t));
    memcpy(targets, orig_targets, target_count * sizeof(uint64_t));

    qsort(targets, target_count, sizeof(uint64_t), compare_uint64_t);
    for (uint8_t row = 0; row <= total_rows; row++) {
        for (size_t i = 0; i < target_count; i++) {
            uint64_t target = targets[i];
            if (row != detect_row(target, total_rows)) {
                continue;
            }

            if (is_root(target, num_leaves, total_rows)) {
                continue;
            }

            if (i+1 < target_count && right_sibling(target) == targets[i+1]) {
                targets[i] = parent(target, total_rows);
                i++;
                continue;
            }

            targets[i] = parent(target, total_rows);
            proof_positions[count] = sibling(target);
            count++;
        }

        qsort(targets, target_count, sizeof(uint64_t), compare_uint64_t);
    }

    free(targets);
    return count;
}

char* uint8_array_to_hex_string(const uint8_t array[32]) {
    // Allocate memory for 64 hex characters + 1 null terminator
    char *hex_string = (char*)malloc(65 * sizeof(char));

    if (hex_string == NULL) {
        // Handle memory allocation failure
        return NULL;
    }

    // Convert each byte to a 2-character hex representation
    for (int i = 0; i < 32; i++) {
        sprintf(hex_string + (i * 2), "%02x", array[i]);
    }

    hex_string[64] = '\0'; // Null-terminate the string

    return hex_string;
}

int compare_ints(const void *x, const void *y) {
    hashpos a = *(hashpos*)x;
    hashpos b = *(hashpos*)y;

    return (a.position > b.position) - (a.position < b.position);  // return -1, 0, or 1
}

int calculate_roots(utreexo_sha512_256 calculated_roots[64], uint64_t num_leaves, size_t delhashes_count, utreexo_sha256* del_hashes, proof proof) {
    if (proof.target_count == 0) {
        return 0;
    }

    if (delhashes_count != 0 && proof.target_count != delhashes_count) {
        return 0;
    }

    hashpos* posandhash = (hashpos*)malloc(proof.target_count * sizeof(hashpos));
    for (size_t i = 0; i < proof.target_count; i++) {
        posandhash[i].position = proof.targets[i];
        if (del_hashes == NULL) {
            memcpy(posandhash[i].hash.hash, empty, 32);
        } else {
            posandhash[i].hash = del_hashes[i];
        }
    }
    qsort(posandhash, proof.target_count, sizeof(hashpos), compare_ints);

    int count = proof.target_count;
    int roots_num = 0;
    int proof_idx = 0;
    for (size_t i = 0; i < count; i++) {
        hashpos cur = posandhash[i];

        printf("i %ld on pos %ld, hash %s\n",
                i, cur.position, uint8_array_to_hex_string(cur.hash.hash));

        if (is_root(cur.position, num_leaves, tree_rows(num_leaves))) {
            calculated_roots[roots_num] = cur.hash;
            roots_num++;
            continue;
        }

        utreexo_sha512_256 phash;
        if (i+1 < count && right_sibling(cur.position) == posandhash[i+1].position) {
            parent_hash(phash.hash, cur.hash.hash, posandhash[i+1].hash.hash);
            i++;
        } else {
            if (proof_idx >= proof.proof_count) {
                // We should have a proof hash but it's missing.
                return 0;
            }

            utreexo_sha512_256 proofhash = proof.proof_hashes[proof_idx];
            proof_idx++;
            if (is_right_sibling(cur.position)) {
                parent_hash(phash.hash, proofhash.hash, cur.hash.hash);
            } else {
                parent_hash(phash.hash, cur.hash.hash, proofhash.hash);
            }
        }

        posandhash[i].position = parent(cur.position, tree_rows(num_leaves));
        posandhash[i].hash = phash;
        //printf("i %ld on pos %ld, hash %s calculated parent %ld %s\n",
        //        i, cur.position, uint8_array_to_hex_string(cur.hash.hash),
        //        posandhash[i].position, uint8_array_to_hex_string(posandhash[i].hash.hash));
        i--;
        qsort(posandhash, proof.target_count, sizeof(hashpos), compare_ints);
    }

    free(posandhash);
    return roots_num;
}
