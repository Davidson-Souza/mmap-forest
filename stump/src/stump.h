#ifndef STUMP_H
#define STUMP_H

#include <stdint.h>

#include "parent_hash.h"
#include "util.h"

typedef struct {
    utreexo_sha512_256 roots[64];
    uint8_t size;
} roots;

typedef struct {
    uint64_t num_leaves;
    roots merkle_roots;
} stump;

stump add(stump s, utreexo_sha512_256 add);
stump del(stump s, proof hash_proof, size_t delhashes_count, utreexo_sha512_256* del_hashes);
int verify(stump s, size_t hashes_count, utreexo_sha512_256* hashes, proof hash_proof);

#endif /* STUMP_H */
