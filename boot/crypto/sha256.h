#ifndef VGK_SHA256_H
#define VGK_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_used;
} VGK_SHA256_CONTEXT;

void vgk_sha256_init(VGK_SHA256_CONTEXT *context);
void vgk_sha256_update(VGK_SHA256_CONTEXT *context, const void *data, size_t length);
void vgk_sha256_finish(VGK_SHA256_CONTEXT *context, uint8_t digest[32]);
void vgk_sha256(const void *data, size_t length, uint8_t digest[32]);

#endif
