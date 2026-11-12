#include <stdio.h>
#include <stdint.h>

#include "vgk_framebuffer.h"

int main(void)
{
    unsigned failures = 0;

#define CHECK(condition, message) do { \
    if (!(condition)) { puts("FAIL: " message); ++failures; } \
} while (0)

    CHECK(vgk_framebuffer_is_valid(
        UINT64_C(0xE0000000), UINT64_C(0x400000),
        1024, 768, 1024, VGK_FRAMEBUFFER_FORMAT_RGBX8),
        "valid RGBX framebuffer rejected");

    CHECK(vgk_framebuffer_is_valid(
        UINT64_C(0xE0000000), UINT64_C(0x500000),
        1280, 720, 1344, VGK_FRAMEBUFFER_FORMAT_BGRX8),
        "valid padded BGRX framebuffer rejected");

    CHECK(!vgk_framebuffer_is_valid(0, 4096, 32, 32, 32,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "zero base accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), 4096, 0, 32, 32,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "zero width accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), 4096, 32, 0, 32,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "zero height accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), 4096, 32, 32, 31,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "stride smaller than width accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), 4095, 32, 32, 32,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "undersized framebuffer accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), 4096, 32, 32, 32, 0),
        "unknown pixel format accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_MAX - 7U, 16, 1, 1, 1,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "address overflow accepted");
    CHECK(!vgk_framebuffer_is_valid(UINT64_C(0x1000), UINT64_MAX,
        UINT32_MAX, UINT32_MAX, UINT32_MAX,
        VGK_FRAMEBUFFER_FORMAT_RGBX8), "size multiplication overflow accepted");

#undef CHECK

    if (failures != 0)
        return 1;

    puts("OK: framebuffer dimensions, format, stride and address bounds");
    return 0;
}
