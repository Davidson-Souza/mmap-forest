#include "varint.h"

size_t bitcoin_varint_size(uint64_t value) {
    if (value < 0xfd) {
        return 1;
    }
    if (value <= 0xffff) {
        return 3;
    }
    if (value <= 0xffffffffULL) {
        return 5;
    }
    return 9;
}

size_t bitcoin_write_varint(unsigned char* out, uint64_t value) {
    if (value < 0xfd) {
        out[0] = (unsigned char)value;
        return 1;
    }
    if (value <= 0xffff) {
        out[0] = 0xfd;
        out[1] = (unsigned char)(value & 0xff);
        out[2] = (unsigned char)((value >> 8) & 0xff);
        return 3;
    }
    if (value <= 0xffffffffULL) {
        out[0] = 0xfe;
        for (int i = 0; i < 4; ++i) {
            out[1 + i] = (unsigned char)((value >> (8 * i)) & 0xff);
        }
        return 5;
    }
    out[0] = 0xff;
    for (int i = 0; i < 8; ++i) {
        out[1 + i] = (unsigned char)((value >> (8 * i)) & 0xff);
    }
    return 9;
}

bool read_varint(const unsigned char* data, size_t len, size_t* offset, uint64_t* out) {
    if (data == NULL || offset == NULL || out == NULL) {
        return false;
    }
    if (*offset >= len) {
        return false;
    }

    uint8_t prefix = data[*offset];
    (*offset)++;

    if (prefix < 0xfd) {
        *out = prefix;
        return true;
    }

    size_t needed = 0;
    if (prefix == 0xfd) {
        needed = 2;
    } else if (prefix == 0xfe) {
        needed = 4;
    } else {
        needed = 8;
    }

    if (*offset > len || len - *offset < needed) {
        return false;
    }

    uint64_t value = 0;
    for (size_t i = 0; i < needed; ++i) {
        value |= ((uint64_t)data[*offset + i]) << (8 * i);
    }
    *offset += needed;
    *out = value;
    return true;
}
