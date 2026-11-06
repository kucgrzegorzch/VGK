#ifndef VGK_MANIFEST_H
#define VGK_MANIFEST_H

#include "integrity.h"

#include <stddef.h>
#include <stdint.h>

#define VGK_MANIFEST_MAX_ENTRIES 128U
#define VGK_MANIFEST_MAX_PATH 240U
#define VGK_MANIFEST_MAX_SIZE 65536U

typedef enum {
    VGK_MANIFEST_OK = 0,
    VGK_MANIFEST_INVALID_ARGUMENT = 1,
    VGK_MANIFEST_BAD_SIGNATURE = 2,
    VGK_MANIFEST_BAD_FORMAT = 3,
    VGK_MANIFEST_TOO_LARGE = 4,
    VGK_MANIFEST_DUPLICATE_PATH = 5,
    VGK_MANIFEST_FILE_OPEN_ERROR = 6,
    VGK_MANIFEST_FILE_INTEGRITY_ERROR = 7
} VGK_MANIFEST_RESULT;

typedef struct {
    const uint8_t *path;
    uint16_t path_length;
    uint64_t file_size;
    const uint8_t *sha256;
} VGK_MANIFEST_ENTRY;

typedef struct {
    const uint8_t *encoded;
    size_t encoded_size;
    uint16_t entry_count;
    size_t entries_offset;
} VGK_MANIFEST;

typedef int (*VGK_MANIFEST_OPEN_FILE)(
    void *filesystem_context,
    const uint8_t *path,
    uint16_t path_length,
    VGK_INTEGRITY_READ *read,
    void **file_context
);

typedef void (*VGK_MANIFEST_CLOSE_FILE)(
    void *filesystem_context,
    void *file_context
);

VGK_MANIFEST_RESULT vgk_manifest_open(
    VGK_MANIFEST *manifest,
    const uint8_t *encoded,
    size_t encoded_size,
    const uint8_t public_key[32],
    const uint8_t signature[64]
);

VGK_MANIFEST_RESULT vgk_manifest_get(
    const VGK_MANIFEST *manifest,
    uint16_t index,
    VGK_MANIFEST_ENTRY *entry
);

VGK_MANIFEST_RESULT vgk_manifest_verify_files(
    const VGK_MANIFEST *manifest,
    VGK_MANIFEST_OPEN_FILE open_file,
    VGK_MANIFEST_CLOSE_FILE close_file,
    void *filesystem_context
);

#endif
