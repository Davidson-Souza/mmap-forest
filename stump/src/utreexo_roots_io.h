#ifndef UTREEXO_ROOTS_IO_H
#define UTREEXO_ROOTS_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stump.h"

typedef struct {
    uint64_t numleaves;
    uint64_t count;
    utreexo_sha512_256* hashes;
} parsed_roots;

bool parse_utreexo_roots_bytes(const unsigned char* data, size_t len, parsed_roots* out_roots);
void free_utreexo_roots(parsed_roots* roots);

#endif /* UTREEXO_ROOTS_IO_H */
