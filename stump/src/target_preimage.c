#include "target_preimage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "varint.h"

#define TARGET_PREIMAGE_MAX_SCRIPT_SIZE 10000

static bool read_uint32_le_field(const unsigned char* data, size_t len, size_t* offset, uint32_t* out) {
    if (data == NULL || offset == NULL || out == NULL) {
        return false;
    }
    if (*offset > len || len - *offset < sizeof(uint32_t)) {
        return false;
    }
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        value |= ((uint32_t)data[*offset + i]) << (8 * i);
    }
    *offset += sizeof(uint32_t);
    *out = value;
    return true;
}

static bool read_uint64_le_field(const unsigned char* data, size_t len, size_t* offset, uint64_t* out) {
    if (data == NULL || offset == NULL || out == NULL) {
        return false;
    }
    if (*offset > len || len - *offset < sizeof(uint64_t)) {
        return false;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        value |= ((uint64_t)data[*offset + i]) << (8 * i);
    }
    *offset += sizeof(uint64_t);
    *out = value;
    return true;
}

bool parse_target_preimages_blob(
    const unsigned char* data,
    size_t len,
    uint32_t expected_count,
    target_preimages* out_preimages) {
    if (out_preimages == NULL) {
        return false;
    }
    out_preimages->entries = NULL;
    out_preimages->count = 0;

    if (data == NULL) {
        fprintf(stderr, "Missing target preimage blob\n");
        return false;
    }

    size_t offset = 0;
    uint64_t count64 = 0;
    if (!read_varint(data, len, &offset, &count64)) {
        fprintf(stderr, "Failed to read target preimage count\n");
        return false;
    }
    if (count64 > UINT32_MAX) {
        fprintf(stderr, "Target preimage count exceeds 32-bit limit\n");
        return false;
    }
    if (expected_count != 0 && count64 != expected_count) {
        fprintf(stderr, "Target preimage count (%llu) does not match proof target count (%u)\n",
                (unsigned long long)count64,
                expected_count);
        return false;
    }
    if (count64 == 0) {
        if (offset != len) {
            fprintf(stderr, "Unexpected bytes after empty target preimage set\n");
            return false;
        }
        return true;
    }

    target_preimage* entries = (target_preimage*)calloc(count64, sizeof(target_preimage));
    if (entries == NULL) {
        fprintf(stderr, "Failed to allocate memory for %llu target preimage(s)\n", (unsigned long long)count64);
        return false;
    }

    size_t filled = 0;
    for (uint64_t i = 0; i < count64; ++i) {
        if (len - offset < 32 + 32 + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t)) {
            fprintf(stderr, "Insufficient data for target preimage %llu\n", (unsigned long long)i);
            goto parse_fail;
        }

        const unsigned char* block_hash = data + offset;
        offset += 32;
        const unsigned char* txid = data + offset;
        offset += 32;

        uint32_t vout = 0;
        if (!read_uint32_le_field(data, len, &offset, &vout)) {
            fprintf(stderr, "Failed to read vout for target preimage %llu\n", (unsigned long long)i);
            goto parse_fail;
        }

        uint32_t header_code = 0;
        if (!read_uint32_le_field(data, len, &offset, &header_code)) {
            fprintf(stderr, "Failed to read header code for target preimage %llu\n", (unsigned long long)i);
            goto parse_fail;
        }

        uint64_t amount = 0;
        if (!read_uint64_le_field(data, len, &offset, &amount)) {
            fprintf(stderr, "Failed to read amount for target preimage %llu\n", (unsigned long long)i);
            goto parse_fail;
        }

        uint64_t script_len = 0;
        if (!read_varint(data, len, &offset, &script_len)) {
            fprintf(stderr, "Failed to read script length for target preimage %llu\n", (unsigned long long)i);
            goto parse_fail;
        }
        if (script_len > TARGET_PREIMAGE_MAX_SCRIPT_SIZE) {
            fprintf(stderr, "Target preimage %llu script too large (%llu bytes)\n",
                    (unsigned long long)i,
                    (unsigned long long)script_len);
            goto parse_fail;
        }
        if (script_len > len - offset) {
            fprintf(stderr, "Insufficient bytes for target preimage %llu script\n", (unsigned long long)i);
            goto parse_fail;
        }

        const unsigned char* script = data + offset;
        offset += (size_t)script_len;

        unsigned char* script_copy = NULL;
        if (script_len > 0) {
            script_copy = (unsigned char*)malloc((size_t)script_len);
            if (script_copy == NULL) {
                fprintf(stderr, "Failed to allocate script buffer for target preimage %llu\n", (unsigned long long)i);
                goto parse_fail;
            }
            memcpy(script_copy, script, (size_t)script_len);
        }

        memcpy(entries[i].block_hash, block_hash, sizeof(entries[i].block_hash));
        memcpy(entries[i].txid, txid, sizeof(entries[i].txid));
        entries[i].vout = vout;
        entries[i].header_code = header_code;
        entries[i].amount = amount;
        entries[i].script = script_copy;
        entries[i].script_len = (size_t)script_len;
        filled++;
    }

    if (offset != len) {
        fprintf(stderr, "Unexpected %zu extra byte(s) after reading target preimages\n", len - offset);
        goto parse_fail;
    }

    out_preimages->entries = entries;
    out_preimages->count = (size_t)count64;
    return true;

parse_fail:
    for (size_t j = 0; j < filled; ++j) {
        free(entries[j].script);
        entries[j].script = NULL;
        entries[j].script_len = 0;
    }
    free(entries);
    return false;
}

void free_target_preimages(target_preimages* preimages) {
    if (preimages == NULL) {
        return;
    }
    for (size_t i = 0; i < preimages->count; ++i) {
        free(preimages->entries[i].script);
        preimages->entries[i].script = NULL;
        preimages->entries[i].script_len = 0;
    }
    free(preimages->entries);
    preimages->entries = NULL;
    preimages->count = 0;
}
