#ifndef UTREEXO_EXTRACT_H
#define UTREEXO_EXTRACT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/bitcoinkernel.h>

#include "parent_hash.h"

typedef struct {
    utreexo_sha512_256* hashes;
    size_t count;
} utxo_hashes;

utxo_hashes extract_utreexo_add_hashes(
    btck_Block* block,
    uint32_t block_height,
    const unsigned char block_hash_bytes[32]);

void free_utxo_hashes(utxo_hashes* hashes);

bool compute_utxo_hash(
    const unsigned char block_hash[32],
    const unsigned char txid[32],
    uint32_t vout,
    uint32_t header_code,
    uint64_t amount,
    const unsigned char* script,
    size_t script_len,
    unsigned char out_hash[32]);

bool build_utxo_preimage_bytes(
    const unsigned char block_hash[32],
    const unsigned char txid[32],
    uint32_t vout,
    uint32_t header_code,
    uint64_t amount,
    const unsigned char* script,
    size_t script_len,
    unsigned char** out_bytes,
    size_t* out_len);

#endif /* UTREEXO_EXTRACT_H */
