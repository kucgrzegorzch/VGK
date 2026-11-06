#ifndef VGK_INTEGRITY_H
#define VGK_INTEGRITY_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    VGK_INTEGRITY_OK = 0,
    VGK_INTEGRITY_INVALID_ARGUMENT = 1,
    VGK_INTEGRITY_READ_ERROR = 2,
    VGK_INTEGRITY_HASH_MISMATCH = 3,
    VGK_INTEGRITY_OFFSET_OVERFLOW = 4
} VGK_INTEGRITY_RESULT;

/*
 * Reader contract:
 * - return 0 when the read operation succeeded;
 * - set *bytes_read to the number of bytes copied (0 means end of file);
 * - never write more than capacity bytes to buffer;
 * - a short read is allowed and is not necessarily end of file.
 */
typedef int (*VGK_INTEGRITY_READ)(
    void *context,
    uint64_t offset,
    uint8_t *buffer,
    size_t capacity,
    size_t *bytes_read
);

VGK_INTEGRITY_RESULT vgk_verify_buffer_sha256(
    const void *data,
    size_t length,
    const uint8_t expected_digest[32]
);

VGK_INTEGRITY_RESULT vgk_verify_reader_sha256(
    VGK_INTEGRITY_READ read,
    void *context,
    uint64_t expected_size,
    const uint8_t expected_digest[32]
);

#endif
