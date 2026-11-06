#include "integrity.h"
#include "sha256.h"

#include <string.h>

static int digest_equal_constant_time(const uint8_t left[32], const uint8_t right[32])
{
    uint8_t difference = 0;
    for (size_t i = 0; i < 32; ++i)
        difference |= (uint8_t)(left[i] ^ right[i]);
    return difference == 0;
}

VGK_INTEGRITY_RESULT vgk_verify_buffer_sha256(
    const void *data,
    size_t length,
    const uint8_t expected_digest[32])
{
    uint8_t actual[32];

    if (expected_digest == NULL || (data == NULL && length != 0))
        return VGK_INTEGRITY_INVALID_ARGUMENT;

    vgk_sha256(data, length, actual);
    return digest_equal_constant_time(actual, expected_digest)
        ? VGK_INTEGRITY_OK : VGK_INTEGRITY_HASH_MISMATCH;
}

VGK_INTEGRITY_RESULT vgk_verify_reader_sha256(
    VGK_INTEGRITY_READ read,
    void *context,
    uint64_t expected_size,
    const uint8_t expected_digest[32])
{
    uint8_t buffer[4096];
    uint8_t actual[32];
    uint64_t offset = 0;
    VGK_SHA256_CONTEXT hash;

    if (read == NULL || expected_digest == NULL)
        return VGK_INTEGRITY_INVALID_ARGUMENT;

    vgk_sha256_init(&hash);
    while (offset < expected_size) {
        size_t received = 0;
        size_t capacity = sizeof(buffer);
        uint64_t remaining = expected_size - offset;
        if (remaining < (uint64_t)capacity)
            capacity = (size_t)remaining;

        if (read(context, offset, buffer, capacity, &received) != 0)
            return VGK_INTEGRITY_READ_ERROR;
        if (received == 0 || received > capacity)
            return VGK_INTEGRITY_READ_ERROR;
        if ((uint64_t)received > UINT64_MAX - offset)
            return VGK_INTEGRITY_OFFSET_OVERFLOW;

        vgk_sha256_update(&hash, buffer, received);
        offset += (uint64_t)received;
    }

    vgk_sha256_finish(&hash, actual);
    return digest_equal_constant_time(actual, expected_digest)
        ? VGK_INTEGRITY_OK : VGK_INTEGRITY_HASH_MISMATCH;
}
