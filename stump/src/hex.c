#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hex.h"

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

unsigned char* hex_to_bytes(const char* hex, size_t* out_len) {
    if (hex == NULL || out_len == NULL) return NULL;

    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }

    size_t hex_len = strlen(hex);
    if (hex_len == 0 || hex_len % 2 != 0) {
        fprintf(stderr, "Hex input must be a non-empty even-length string\n");
        return NULL;
    }

    size_t byte_len = hex_len / 2;
    unsigned char* bytes = (unsigned char*)malloc(byte_len);
    if (bytes == NULL) {
        fprintf(stderr, "Failed to allocate memory for decoded hex data\n");
        return NULL;
    }

    for (size_t i = 0; i < byte_len; ++i) {
        int high = hex_char_to_int(hex[2 * i]);
        int low = hex_char_to_int(hex[2 * i + 1]);
        if (high < 0 || low < 0) {
            fprintf(stderr, "Invalid hex character in input\n");
            free(bytes);
            return NULL;
        }
        bytes[i] = (unsigned char)((high << 4) | low);
    }

    *out_len = byte_len;
    return bytes;
}

