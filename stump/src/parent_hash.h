#include <string.h>

#include <openssl/evp.h>

#ifndef UTREEXO_PARENT_HASH
#define UTREEXO_PARENT_HASH

/* Computes the sha512_256 to of some chunk of data with length `len`
 *  this hash function is used to compute parent hashes inside the tree */
static inline void sha512_256(uint8_t hash[32], const void *data, size_t len) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha512_256(), NULL);
  EVP_DigestUpdate(ctx, data, len);
  EVP_DigestFinal_ex(ctx, hash, NULL);
  EVP_MD_CTX_free(ctx);
}
/* Used on tests only, computes the sha256 of a byte */
static inline void hash_from_u8(uint8_t hash[32], const uint8_t data) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
  EVP_DigestUpdate(ctx, &data, 1);
  EVP_DigestFinal_ex(ctx, hash, NULL);
  EVP_MD_CTX_free(ctx);
}

static uint8_t empty[32] = {0};

/* Computes the parent hash for two siblings */
static inline void parent_hash(uint8_t out[32], uint8_t left[32],
                               uint8_t right[32]) {
  if (memcmp(empty, left, 32) == 0) {
    memcpy(out, right, 32);
    return;
  }
  if (memcmp(empty, right, 32) == 0) {
    memcpy(out, left, 32);
    return;
  }
  uint8_t concat[64];
  memcpy(concat, left, 32);
  memcpy(concat + 32, right, 32);
  sha512_256(out, concat, 64);
}

/* Computes the parent hash for two siblings */
static inline void parent_hash1(uint8_t out[32], uint8_t left[32],
                               uint8_t right[32]) {
  uint8_t concat[64];
  memcpy(concat, left, 32);
  memcpy(concat + 32, right, 32);
  sha512_256(out, concat, 64);
}

/* An utility type that represents one hash, can be either sha256 or sha512_256
 */
struct utreexo_hash {
  uint8_t hash[32];
};

/* A sha256 hash */
typedef struct utreexo_hash utreexo_sha256;

/* A sha512_256 hash */
typedef struct utreexo_hash utreexo_sha512_256;

/* The hash associated with an internal node in a tree */
typedef utreexo_sha512_256 utreexo_node_hash;

/* A hash associated with a leaf */
typedef utreexo_sha512_256 utreexo_leaf_hash;

#endif
