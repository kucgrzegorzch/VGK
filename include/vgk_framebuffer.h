#ifndef VGK_FRAMEBUFFER_H
#define VGK_FRAMEBUFFER_H

#include <stdint.h>

/* VGK's normalized 32-bit framebuffer pixel layouts. */
#define VGK_FRAMEBUFFER_FORMAT_RGBX8 UINT32_C(1)
#define VGK_FRAMEBUFFER_FORMAT_BGRX8 UINT32_C(2)
#define VGK_FRAMEBUFFER_BYTES_PER_PIXEL UINT64_C(4)

/*
 * Validate a linear 32-bit framebuffer before VGK passes it to the kernel.
 * The firmware-reported allocation may be larger than the visible image,
 * but it must cover every scanline including stride padding.
 */
static inline int vgk_framebuffer_is_valid(
    uint64_t base,
    uint64_t size,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t format)
{
    uint64_t row_bytes;
    uint64_t required_bytes;

    if (base == 0 || size == 0 || width == 0 || height == 0 ||
        stride < width ||
        (format != VGK_FRAMEBUFFER_FORMAT_RGBX8 &&
         format != VGK_FRAMEBUFFER_FORMAT_BGRX8))
        return 0;

    if ((uint64_t)stride > UINT64_MAX / VGK_FRAMEBUFFER_BYTES_PER_PIXEL)
        return 0;
    row_bytes = (uint64_t)stride * VGK_FRAMEBUFFER_BYTES_PER_PIXEL;

    if ((uint64_t)height > UINT64_MAX / row_bytes)
        return 0;
    required_bytes = row_bytes * (uint64_t)height;

    if (required_bytes > size || base > UINT64_MAX - size)
        return 0;

    return 1;
}

#endif
