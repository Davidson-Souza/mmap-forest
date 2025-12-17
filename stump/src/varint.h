#ifndef VARINT_H
#define VARINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t bitcoin_varint_size(uint64_t value);
size_t bitcoin_write_varint(unsigned char* out, uint64_t value);
bool read_varint(const unsigned char* data, size_t len, size_t* offset, uint64_t* out);

#endif // VARINT_H
