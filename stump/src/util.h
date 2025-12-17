#ifndef STUMP_UTIL_H
#define STUMP_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "parent_hash.h"

static inline uint64_t sibling(uint64_t position) {
    return position ^ 1;
}

static inline uint64_t right_sibling(uint64_t position) {
    return position | 1;
}

static inline bool is_right_sibling(uint64_t position) {
    return (position & 1) == 1;
}

static inline bool root_present(uint64_t num_leaves, uint8_t row) {
    return (num_leaves & (1 << row)) != 0;
}

static inline uint64_t parent(uint64_t position, uint8_t total_rows) {
    return (position >> 1) | (1 << total_rows);
}

inline uint64_t left_child(uint64_t position, uint8_t total_rows) {
	uint64_t mask = (2 << total_rows) - 1;
	return (position << 1) & mask;
}

inline uint64_t right_child(uint64_t position, uint8_t total_rows) {
	uint64_t mask = (2 << total_rows) - 1;
        return ((position << 1) & mask) | 1;
}

static inline uint8_t bit_length(uint64_t x) {
    uint8_t n = 0;
    while (x != 0) {
            x >>= 1;
            n++;
    }

    return n;
}

inline uint8_t tree_rows(uint64_t num_leaves) {
    if (num_leaves == 0) {
        return 0;
    }
    return bit_length(num_leaves-1);
}

static inline uint64_t root_position(uint64_t num_leaves, uint8_t row, uint8_t total_rows) {
    uint64_t mask = (2 << total_rows) - 1;
    uint64_t before = num_leaves & (mask << (row + 1));
    uint64_t shifted = (before >> row) | (mask << (total_rows + 1 - row));
    return shifted & mask;
}

static inline uint8_t detect_row(uint64_t position, uint8_t total_rows) {
    for (uint8_t row = total_rows; row >= 0; row--) {
        uint8_t rowbit = 1 << row;
        if ((rowbit & position) == 0) {
            return total_rows-row;
        }
    }

    return 0;
}

static inline bool is_root(uint64_t position, uint64_t num_leaves, uint8_t total_rows) {
    uint8_t row = detect_row(position, total_rows);
    uint64_t rootpos = root_position(num_leaves, row, total_rows);
    return root_present(num_leaves, row) && rootpos == position;
}

static uint8_t root_idx(uint64_t num_leaves, uint64_t position) {
    uint8_t total_rows = tree_rows(num_leaves);
    uint8_t idx = 0;
    for (int row = total_rows; row >= 0; row--) {
        if (!root_present(num_leaves, row)) {
            continue;
        }
        uint64_t pos = position;
        for (uint8_t r = detect_row(position, total_rows); r < row; r++) {
            pos = parent(pos, total_rows);
        }
        printf("pos %ld\n", pos);
        if (is_root(pos, num_leaves, total_rows)) {
            return idx;
        }
        idx += 1;
    }

    return idx;
}

static inline int deduplicate(size_t *arr, size_t size) {
    if (size == 0) return 0;

    int count = 1; // Start with the first element as unique
    for (int i = 1; i < size; i++) {
        int j;
        for (j = 0; j < count; j++) {
            if (arr[i] == arr[j]) {
                break; // Found a duplicate
            }
        }
        if (j == count) {
            arr[count] = arr[i]; // Add unique element
            count++;
        }
    }
    return count; // Return the count of unique elements
}

static inline uint8_t root_idxs(uint64_t num_leaves, size_t* root_indexes, uint32_t pos_count, uint64_t* positions) {
    for (int i = 0; i < pos_count; i++) {
        root_indexes[i] = root_idx(num_leaves, positions[i]);
    }

    int c = deduplicate(root_indexes, pos_count);
    return c;
}

int proof_positions(size_t target_count, uint64_t* targets, uint64_t num_leaves, uint8_t total_rows, uint64_t* proof_positions);

typedef struct {
    uint32_t target_count;
    uint64_t* targets;
    uint32_t proof_count;
    utreexo_sha512_256* proof_hashes;
} proof;

char* uint8_array_to_hex_string(const uint8_t array[32]);

int calculate_roots(utreexo_sha512_256 calculated_roots[64], uint64_t num_leaves, size_t delhashes_count, utreexo_sha256* del_hashes, proof proof);

#endif // STUMP_UTIL_H
