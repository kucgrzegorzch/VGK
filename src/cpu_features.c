#include "vgk_cpu_features.h"

#define VGK_CPUID_EXT_FEATURES UINT32_C(0x80000001)
#define VGK_CPUID_ADDRESS_SIZE UINT32_C(0x80000008)
#define VGK_CPUID_NX_BIT (UINT32_C(1) << 20)
#define VGK_CPUID_SSE2_BIT (UINT32_C(1) << 26)
#define VGK_CPUID_SSE3_BIT (UINT32_C(1) << 0)
#define VGK_CPUID_SSSE3_BIT (UINT32_C(1) << 9)
#define VGK_CPUID_SSE41_BIT (UINT32_C(1) << 19)
#define VGK_CPUID_SSE42_BIT (UINT32_C(1) << 20)
#define VGK_CPUID_AVX_BIT (UINT32_C(1) << 28)
#define VGK_CPUID_OSXSAVE_BIT (UINT32_C(1) << 27)
#define VGK_CPUID_AVX2_BIT (UINT32_C(1) << 5)

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

static void vgk_cpu_clear_optional_features(VGK_CPU_FEATURES *features)
{
    uint32_t i;
    for (i = 0; i < 13U; ++i)
        features->vendor[i] = '\0';
    features->family = 0;
    features->model = 0;
    features->stepping = 0;
    features->sse2_supported = 0;
    features->sse3_supported = 0;
    features->ssse3_supported = 0;
    features->sse4_1_supported = 0;
    features->sse4_2_supported = 0;
    features->avx_hardware_supported = 0;
    features->avx_state_supported = 0;
    features->avx2_hardware_supported = 0;
    features->avx2_usable = 0;
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
    vgk_cpu_clear_optional_features(features);

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

void vgk_cpu_decode_vector_features(
    uint32_t leaf1_ecx,
    uint32_t leaf1_edx,
    uint32_t leaf7_ebx,
    uint32_t xcr0_low,
    uint8_t has_leaf1,
    uint8_t has_leaf7,
    VGK_CPU_FEATURES *features)
{
    uint8_t avx_os_state_enabled;

    if (features == 0)
        return;

    features->sse2_supported = 0;
    features->sse3_supported = 0;
    features->ssse3_supported = 0;
    features->sse4_1_supported = 0;
    features->sse4_2_supported = 0;
    features->avx_hardware_supported = 0;
    features->avx_state_supported = 0;
    features->avx2_hardware_supported = 0;
    features->avx2_usable = 0;

    if (has_leaf1 == 0U)
        return;

    features->sse2_supported =
        (uint8_t)((leaf1_edx & VGK_CPUID_SSE2_BIT) != 0U);
    features->sse3_supported =
        (uint8_t)((leaf1_ecx & VGK_CPUID_SSE3_BIT) != 0U);
    features->ssse3_supported =
        (uint8_t)((leaf1_ecx & VGK_CPUID_SSSE3_BIT) != 0U);
    features->sse4_1_supported =
        (uint8_t)((leaf1_ecx & VGK_CPUID_SSE41_BIT) != 0U);
    features->sse4_2_supported =
        (uint8_t)((leaf1_ecx & VGK_CPUID_SSE42_BIT) != 0U);
    features->avx_hardware_supported =
        (uint8_t)((leaf1_ecx & VGK_CPUID_AVX_BIT) != 0U);

    avx_os_state_enabled = (uint8_t)(
        (leaf1_ecx & (VGK_CPUID_AVX_BIT | VGK_CPUID_OSXSAVE_BIT)) ==
        (VGK_CPUID_AVX_BIT | VGK_CPUID_OSXSAVE_BIT));
    features->avx_state_supported = (uint8_t)(
        avx_os_state_enabled != 0U && (xcr0_low & 0x6U) == 0x6U);

    if (has_leaf7 != 0U) {
        features->avx2_hardware_supported =
            (uint8_t)((leaf7_ebx & VGK_CPUID_AVX2_BIT) != 0U);
    }

    features->avx2_usable = (uint8_t)(
        features->avx2_hardware_supported != 0U &&
        features->avx_state_supported != 0U);
}

static void vgk_cpu_read_identity_and_optional_features(
    VGK_CPU_FEATURES *features)
{
    uint32_t eax0, ebx0, ecx0, edx0;
    uint32_t eax1 = 0, ebx1 = 0, ecx1 = 0, edx1 = 0;
    uint32_t eax7 = 0, ebx7 = 0;
    uint32_t xcr0_low = 0;
    uint32_t base_family, base_model, extended_family, extended_model;
    uint8_t has_leaf1;
    uint8_t has_leaf7;

    vgk_cpuid(0, 0, &eax0, &ebx0, &ecx0, &edx0);
    features->max_basic_leaf = eax0;
    features->vendor[0] = (char)(ebx0 & 0xFFU);
    features->vendor[1] = (char)((ebx0 >> 8) & 0xFFU);
    features->vendor[2] = (char)((ebx0 >> 16) & 0xFFU);
    features->vendor[3] = (char)((ebx0 >> 24) & 0xFFU);
    features->vendor[4] = (char)(edx0 & 0xFFU);
    features->vendor[5] = (char)((edx0 >> 8) & 0xFFU);
    features->vendor[6] = (char)((edx0 >> 16) & 0xFFU);
    features->vendor[7] = (char)((edx0 >> 24) & 0xFFU);
    features->vendor[8] = (char)(ecx0 & 0xFFU);
    features->vendor[9] = (char)((ecx0 >> 8) & 0xFFU);
    features->vendor[10] = (char)((ecx0 >> 16) & 0xFFU);
    features->vendor[11] = (char)((ecx0 >> 24) & 0xFFU);
    features->vendor[12] = '\\0';

    has_leaf1 = (uint8_t)(eax0 >= 1U);
    has_leaf7 = (uint8_t)(eax0 >= 7U);

    if (has_leaf1 != 0U) {
        vgk_cpuid(1, 0, &eax1, &ebx1, &ecx1, &edx1);
        (void)ebx1;
        base_family = (eax1 >> 8) & 0xFU;
        base_model = (eax1 >> 4) & 0xFU;
        extended_family = (eax1 >> 20) & 0xFFU;
        extended_model = (eax1 >> 16) & 0xFU;
        features->family = (uint16_t)(base_family == 0xFU
            ? base_family + extended_family : base_family);
        features->model = (uint16_t)((base_family == 0x6U || base_family == 0xFU)
            ? (extended_model << 4) | base_model : base_model);
        features->stepping = (uint8_t)(eax1 & 0xFU);

        /*
         * XGETBV is executed only when both AVX and OSXSAVE are advertised.
         * The pure decoder independently checks these bits before accepting
         * the supplied XCR0 value.
         */
        if ((ecx1 & (VGK_CPUID_AVX_BIT | VGK_CPUID_OSXSAVE_BIT)) ==
            (VGK_CPUID_AVX_BIT | VGK_CPUID_OSXSAVE_BIT)) {
            uint32_t xcr0_high;
            __asm__ volatile (
                "xgetbv"
                : "=a"(xcr0_low), "=d"(xcr0_high)
                : "c"(0)
            );
            (void)xcr0_high;
        }
    } else {
        features->family = 0;
        features->model = 0;
        features->stepping = 0;
    }

    if (has_leaf7 != 0U)
        vgk_cpuid(7, 0, &eax7, &ebx7, 0, 0);

    vgk_cpu_decode_vector_features(
        ecx1, edx1, ebx7, xcr0_low, has_leaf1, has_leaf7, features);
}

VGK_CPU_RESULT vgk_cpu_detect_features(VGK_CPU_FEATURES *features)
{
    uint32_t max_basic;
    uint32_t max_extended;
    uint32_t feature_edx = 0;
    uint32_t address_eax = 0;
    VGK_CPU_RESULT result;

    if (features == 0)
        return VGK_CPU_INVALID_ARGUMENT;

    vgk_cpuid(0, 0, &max_basic, 0, 0, 0);
    vgk_cpuid(UINT32_C(0x80000000), 0, &max_extended, 0, 0, 0);

    if (max_extended >= VGK_CPUID_EXT_FEATURES)
        vgk_cpuid(VGK_CPUID_EXT_FEATURES, 0, 0, 0, 0, &feature_edx);
    if (max_extended >= VGK_CPUID_ADDRESS_SIZE)
        vgk_cpuid(VGK_CPUID_ADDRESS_SIZE, 0, &address_eax, 0, 0, 0);

    result = vgk_cpu_decode_features(
        max_basic, max_extended, feature_edx, address_eax, features);
    vgk_cpu_read_identity_and_optional_features(features);
    features->max_extended_leaf = max_extended;
    return result;
}
