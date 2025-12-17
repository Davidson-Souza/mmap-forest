#include "utreexo_roots_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool parse_utreexo_roots_bytes(const unsigned char* data, size_t len, parsed_roots* out_roots) {
    if (out_roots == NULL) {
        return false;
    }
    out_roots->count = 0;
    out_roots->numleaves = 0;
    out_roots->hashes = NULL;

    if (data == NULL) {
        fprintf(stderr, "Missing Utreexo root bytes\n");
        return false;
    }

    if (len < 8) {
        fprintf(stderr, "Roots blob must include an 8-byte numleaves (got %zu bytes)\n", len);
        return false;
    }

    size_t roots_bytes = len - 8;
    if (roots_bytes % 32 != 0) {
        fprintf(stderr, "Roots data length must be a multiple of 32 (got %zu bytes)\n", roots_bytes);
        return false;
    }

    uint64_t numleaves = 0;
    for (int i = 0; i < 8; ++i) {
        numleaves |= ((uint64_t)data[i]) << (8 * i);
    }

    uint64_t count = roots_bytes / 32;
    if (count > SIZE_MAX / sizeof(utreexo_sha512_256)) {
        fprintf(stderr, "Roots count is too large\n");
        return false;
    }

    utreexo_sha512_256* hashes = (utreexo_sha512_256*)malloc(count * sizeof(utreexo_sha512_256));
    if (hashes == NULL) {
        fprintf(stderr, "Failed to allocate memory for roots\n");
        return false;
    }

    for (uint64_t i = 0; i < count; ++i) {
        memcpy(hashes[i].hash, data + 8 + (size_t)i * 32, 32);
    }

    out_roots->count = count;
    out_roots->numleaves = numleaves;
    out_roots->hashes = hashes;
    return true;
}

void free_utreexo_roots(parsed_roots* roots) {
    if (roots == NULL) {
        return;
    }
    free(roots->hashes);
    roots->hashes = NULL;
    roots->count = 0;
    roots->numleaves = 0;
}
