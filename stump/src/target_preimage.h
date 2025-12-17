#ifndef TARGET_PREIMAGE_H
#define TARGET_PREIMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char block_hash[32];
    unsigned char txid[32];
    uint32_t vout;
    uint32_t header_code;
    uint64_t amount;
    unsigned char* script;
    size_t script_len;
} target_preimage;

typedef struct {
    target_preimage* entries;
    size_t count;
} target_preimages;

bool parse_target_preimages_blob(
    const unsigned char* data,
    size_t len,
    uint32_t expected_count,
    target_preimages* out_preimages);

void free_target_preimages(target_preimages* preimages);

#endif /* TARGET_PREIMAGE_H */
