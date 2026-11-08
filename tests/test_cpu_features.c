#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vgk_cpu_features.h"

#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

#define NX_BIT (UINT32_C(1) << 20)

int main(void)
{
    VGK_CPU_FEATURES features;
    int failures = 0;

    memset(&features, 0xA5, sizeof(features));

    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 48U, &features) == VGK_CPU_OK);
    EXPECT(features.max_basic_leaf == 0x16U);
    EXPECT(features.max_extended_leaf == 0x80000008U);
    EXPECT(features.physical_address_bits == 48U);
    EXPECT(features.nx_supported == 1U);
    EXPECT(features.reserved[0] == 0U && features.reserved[1] == 0U);

    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, 0U, 48U, &features) == VGK_CPU_UNSUPPORTED);
    EXPECT(features.nx_supported == 0U);
    EXPECT(features.physical_address_bits == 0U);

    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000001U, NX_BIT, 48U, &features) == VGK_CPU_UNSUPPORTED);
    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 31U, &features) == VGK_CPU_UNSUPPORTED);
    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 53U, &features) == VGK_CPU_UNSUPPORTED);
    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 52U, &features) == VGK_CPU_OK);
    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 32U, &features) == VGK_CPU_OK);
    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 48U, 0) == VGK_CPU_INVALID_ARGUMENT);

    if (failures != 0) {
        fprintf(stderr, "VGK CPU feature tests: %d failure(s)\n", failures);
        return 1;
    }

    puts("OK: wykrywanie NX i szerokosci adresu fizycznego.");
    return 0;
}
