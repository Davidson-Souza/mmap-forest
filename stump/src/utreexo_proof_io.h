#ifndef UTREEXO_PROOF_IO_H
#define UTREEXO_PROOF_IO_H

#include <stdbool.h>
#include <stddef.h>

#include "util.h"

bool parse_utreexo_proof_bytes(const unsigned char* data, size_t len, proof* parsed_proof);
void free_utreexo_proof(proof* parsed_proof);

#endif // UTREEXO_PROOF_IO_H
