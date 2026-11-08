#include "vgk_cpu_features.h"

#define VGK_CPUID_EXT_FEATURES UINT32_C(0x80000001)
#define VGK_CPUID_ADDRESS_SIZE UINT32_C(0x80000008)
#define VGK_CPUID_NX_BIT (UINT32_C(1) << 20)

static void vgk_cpuid(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    __asm__ volatile (
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(leaf), "c"(subleaf)
    );

    if (eax != 0) *eax = a;
    if (ebx != 0) *ebx = b;
    if (ecx != 0) *ecx = c;
    if (edx != 0) *edx = d;
}

VGK_CPU_RESULT vgk_cpu_decode_features(
    uint32_t max_basic_leaf,
    uint32_t max_extended_leaf,
    uint32_t extended_feature_edx,
    uint32_t address_size_eax,
    VGK_CPU_FEATURES *features)
{
    uint32_t physical_bits;

    if (features == 0)
        return VGK_CPU_INVALID_ARGUMENT;

    features->max_basic_leaf = max_basic_leaf;
    features->max_extended_leaf = max_extended_leaf;
    features->physical_address_bits = 0;
    features->nx_supported = 0;
    features->reserved[0] = 0;
    features->reserved[1] = 0;

    if (max_extended_leaf < VGK_CPUID_EXT_FEATURES ||
        (extended_feature_edx & VGK_CPUID_NX_BIT) == 0 ||
        max_extended_leaf < VGK_CPUID_ADDRESS_SIZE)
        return VGK_CPU_UNSUPPORTED;

    physical_bits = address_size_eax & UINT32_C(0xFF);
    /* x86-64 paging supports at most 52 physical-address bits. */
    if (physical_bits < 32U || physical_bits > 52U)
        return VGK_CPU_UNSUPPORTED;

    features->physical_address_bits = (uint8_t)physical_bits;
    features->nx_supported = 1;
    return VGK_CPU_OK;
}

VGK_CPU_RESULT vgk_cpu_detect_features(VGK_CPU_FEATURES *features)
{
    uint32_t max_basic;
    uint32_t max_extended;
    uint32_t feature_edx = 0;
    uint32_t address_eax = 0;

    if (features == 0)
        return VGK_CPU_INVALID_ARGUMENT;

    vgk_cpuid(0, 0, &max_basic, 0, 0, 0);
    vgk_cpuid(UINT32_C(0x80000000), 0, &max_extended, 0, 0, 0);

    if (max_extended >= VGK_CPUID_EXT_FEATURES)
        vgk_cpuid(VGK_CPUID_EXT_FEATURES, 0, 0, 0, 0, &feature_edx);
    if (max_extended >= VGK_CPUID_ADDRESS_SIZE)
        vgk_cpuid(VGK_CPUID_ADDRESS_SIZE, 0, &address_eax, 0, 0, 0);

    return vgk_cpu_decode_features(
        max_basic, max_extended, feature_edx, address_eax, features);
}
