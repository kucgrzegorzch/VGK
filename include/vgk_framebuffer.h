#ifndef VGK_FRAMEBUFFER_H
#define VGK_FRAMEBUFFER_H

#include <stdint.h>

/* VGK's normalized 32-bit framebuffer pixel layouts. */
#define VGK_FRAMEBUFFER_FORMAT_RGBX8 UINT32_C(1)
#define VGK_FRAMEBUFFER_FORMAT_BGRX8 UINT32_C(2)
#define VGK_FRAMEBUFFER_BYTES_PER_PIXEL UINT64_C(4)

/*
 * Return the byte span needed for all framebuffer scanlines, including
 * per-row stride padding. A zero result is never published on failure.
 */
static inline int vgk_framebuffer_required_size(
    uint32_t height,
    uint32_t stride,
    uint64_t *required_size)
{
    uint64_t row_bytes;

    if (required_size == 0)
        return 0;
    *required_size = 0;

    if (height == 0 || stride == 0)
        return 0;

    /* A uint32_t stride multiplied by four fits in uint64_t. */
    row_bytes = (uint64_t)stride * VGK_FRAMEBUFFER_BYTES_PER_PIXEL;
    if ((uint64_t)height > UINT64_MAX / row_bytes)
        return 0;

    *required_size = row_bytes * (uint64_t)height;
    return *required_size != 0;
}

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
    uint64_t required_bytes;

    if (base == 0 || size == 0 || width == 0 || height == 0 ||
        stride < width ||
        (format != VGK_FRAMEBUFFER_FORMAT_RGBX8 &&
         format != VGK_FRAMEBUFFER_FORMAT_BGRX8) ||
        !vgk_framebuffer_required_size(height, stride, &required_bytes))
        return 0;

    if (required_bytes > size || base > UINT64_MAX - size)
        return 0;

    return 1;
}

#endif
