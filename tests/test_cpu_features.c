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
#define SSE2_BIT (UINT32_C(1) << 26)
#define SSE3_BIT (UINT32_C(1) << 0)
#define SSSE3_BIT (UINT32_C(1) << 9)
#define SSE41_BIT (UINT32_C(1) << 19)
#define SSE42_BIT (UINT32_C(1) << 20)
#define AVX_BIT (UINT32_C(1) << 28)
#define OSXSAVE_BIT (UINT32_C(1) << 27)
#define XSAVE_BIT (UINT32_C(1) << 26)
#define AVX2_BIT (UINT32_C(1) << 5)
#define APIC_BIT (UINT32_C(1) << 9)
#define X2APIC_BIT (UINT32_C(1) << 21)

int main(void)
{
    VGK_CPU_FEATURES features;
    uint32_t all_sse = SSE2_BIT;
    uint32_t all_sse_ecx = SSE3_BIT | SSSE3_BIT | SSE41_BIT | SSE42_BIT;
    int failures = 0;

    memset(&features, 0xA5, sizeof(features));

    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, NX_BIT, 48U, &features) == VGK_CPU_OK);
    EXPECT(features.max_basic_leaf == 0x16U);
    EXPECT(features.max_extended_leaf == 0x80000008U);
    EXPECT(features.physical_address_bits == 48U);
    EXPECT(features.nx_supported == 1U);
    EXPECT(features.reserved[0] == 0U && features.reserved[1] == 0U);
    EXPECT(features.local_apic_supported == 0U);
    EXPECT(features.x2apic_supported == 0U);
    EXPECT(features.apic_base_valid == 0U);
    EXPECT(features.local_apic_enabled == 0U);
    EXPECT(features.x2apic_enabled == 0U);
    EXPECT(features.local_apic_base == 0U);
    EXPECT(features.vendor[0] == '\0' && features.vendor[12] == '\0');

    EXPECT(vgk_cpu_decode_features(
        0x16U, 0x80000008U, 0U, 48U, &features) == VGK_CPU_UNSUPPORTED);
    EXPECT(features.nx_supported == 0U);
    EXPECT(features.physical_address_bits == 0U);
    EXPECT(features.avx2_usable == 0U);

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

    /* PAT entry 3 must be UC for the PCD+PWT MMIO mapping convention. */
    vgk_cpu_decode_pat_features(
        UINT32_C(1) << 16, UINT64_C(0x0007040600070406), 1U, &features);
    EXPECT(features.pat_supported == 1U);
    EXPECT(features.mmio_uc_supported == 1U);

    /* A PAT layout with entry 3 set to write-combining is not accepted. */
    vgk_cpu_decode_pat_features(
        UINT32_C(1) << 16, UINT64_C(0x0000000001070406), 1U, &features);
    EXPECT(features.pat_supported == 1U);
    EXPECT(features.mmio_uc_supported == 0U);

    /* Without PAT, legacy PCD=1/PWT=1 still means uncached. */
    vgk_cpu_decode_pat_features(0U, UINT64_MAX, 1U, &features);
    EXPECT(features.pat_supported == 0U);
    EXPECT(features.mmio_uc_supported == 1U);

    /* No CPUID leaf 1 likewise uses the legacy UC encoding. */
    vgk_cpu_decode_pat_features(UINT32_MAX, UINT64_MAX, 0U, &features);
    EXPECT(features.pat_supported == 0U);
    EXPECT(features.mmio_uc_supported == 1U);
    vgk_cpu_decode_pat_features(0U, 0U, 1U, 0);

    /* x2APIC is usable only when CPUID also advertises the local APIC. */
    vgk_cpu_decode_apic_features(X2APIC_BIT, APIC_BIT, 1U, &features);
    EXPECT(features.local_apic_supported == 1U);
    EXPECT(features.x2apic_supported == 1U);

    vgk_cpu_decode_apic_features(0U, APIC_BIT, 1U, &features);
    EXPECT(features.local_apic_supported == 1U);
    EXPECT(features.x2apic_supported == 0U);

    vgk_cpu_decode_apic_features(X2APIC_BIT, 0U, 1U, &features);
    EXPECT(features.local_apic_supported == 0U);
    EXPECT(features.x2apic_supported == 0U);

    vgk_cpu_decode_apic_features(UINT32_MAX, UINT32_MAX, 0U, &features);
    EXPECT(features.local_apic_supported == 0U);
    EXPECT(features.x2apic_supported == 0U);
    vgk_cpu_decode_apic_features(0U, 0U, 1U, 0);

    /* Read-only IA32_APIC_BASE decoding: enabled xAPIC. */
    vgk_cpu_decode_apic_features(0U, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00900), 48U, &features);
    EXPECT(features.apic_base_valid == 1U);
    EXPECT(features.local_apic_base == UINT64_C(0xFEE00000));
    EXPECT(features.local_apic_enabled == 1U);
    EXPECT(features.x2apic_enabled == 0U);

    /* Firmware may already have enabled x2APIC mode. */
    vgk_cpu_decode_apic_features(X2APIC_BIT, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00D00), 48U, &features);
    EXPECT(features.apic_base_valid == 1U);
    EXPECT(features.local_apic_enabled == 1U);
    EXPECT(features.x2apic_enabled == 1U);

    /* A valid base can be reported while the local APIC is disabled. */
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00100), 48U, &features);
    EXPECT(features.apic_base_valid == 1U);
    EXPECT(features.local_apic_enabled == 0U);
    EXPECT(features.x2apic_enabled == 0U);

    /* Reject an APIC base above the processor's physical-address limit. */
    vgk_cpu_decode_apic_base(UINT64_C(0x1FEE00900), 32U, &features);
    EXPECT(features.apic_base_valid == 0U);
    EXPECT(features.local_apic_base == 0U);

    /* x2APIC mode is invalid without CPUID x2APIC support. */
    vgk_cpu_decode_apic_features(0U, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00D00), 48U, &features);
    EXPECT(features.apic_base_valid == 0U);

    /* x2APIC enable without the global APIC enable bit is invalid. */
    vgk_cpu_decode_apic_features(X2APIC_BIT, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00500), 48U, &features);
    EXPECT(features.apic_base_valid == 0U);

    /* An enabled APIC cannot use a zero physical base. */
    vgk_cpu_decode_apic_features(0U, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0x00000900), 48U, &features);
    EXPECT(features.apic_base_valid == 0U);

    /* Reserved IA32_APIC_BASE bits and missing APIC hardware are rejected. */
    vgk_cpu_decode_apic_features(0U, APIC_BIT, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00B00), 48U, &features);
    EXPECT(features.apic_base_valid == 0U);
    vgk_cpu_decode_apic_features(0U, 0U, 1U, &features);
    vgk_cpu_decode_apic_base(UINT64_C(0xFEE00900), 48U, &features);
    EXPECT(features.apic_base_valid == 0U);

    /* An old CPU with no CPUID leaf 1 has no optional vector features. */
    vgk_cpu_decode_vector_features(0, 0, UINT32_MAX, UINT32_MAX,
                                   0U, 0U, &features);
    EXPECT(features.sse2_supported == 0U);
    EXPECT(features.avx_hardware_supported == 0U);
    EXPECT(features.avx_state_supported == 0U);
    EXPECT(features.avx2_hardware_supported == 0U);
    EXPECT(features.avx2_usable == 0U);

    /* SSE generations are decoded independently of AVX state. */
    vgk_cpu_decode_vector_features(all_sse_ecx, all_sse, 0, 0,
                                   1U, 0U, &features);
    EXPECT(features.sse2_supported == 1U);
    EXPECT(features.sse3_supported == 1U);
    EXPECT(features.ssse3_supported == 1U);
    EXPECT(features.sse4_1_supported == 1U);
    EXPECT(features.sse4_2_supported == 1U);
    EXPECT(features.avx_hardware_supported == 0U);
    EXPECT(features.avx_state_supported == 0U);

    /* AVX hardware without OSXSAVE must never be considered usable. */
    vgk_cpu_decode_vector_features(AVX_BIT, 0, AVX2_BIT, 0x6U,
                                   1U, 1U, &features);
    EXPECT(features.avx_hardware_supported == 1U);
    EXPECT(features.avx_state_supported == 0U);
    EXPECT(features.avx2_hardware_supported == 1U);
    EXPECT(features.avx2_usable == 0U);

    /* OSXSAVE without the hardware XSAVE capability is not sufficient. */
    vgk_cpu_decode_vector_features(AVX_BIT | OSXSAVE_BIT, 0, AVX2_BIT, 0x6U,
                                   1U, 1U, &features);
    EXPECT(features.avx_state_supported == 0U);
    EXPECT(features.avx2_usable == 0U);

    /* AVX + OSXSAVE still requires both XMM and YMM state in XCR0. */
    vgk_cpu_decode_vector_features(AVX_BIT | OSXSAVE_BIT | XSAVE_BIT, 0, AVX2_BIT, 0x2U,
                                   1U, 1U, &features);
    EXPECT(features.avx_state_supported == 0U);
    EXPECT(features.avx2_usable == 0U);

    /* With XCR0 bits 1 and 2 enabled, AVX and AVX2 become usable. */
    vgk_cpu_decode_vector_features(AVX_BIT | OSXSAVE_BIT | XSAVE_BIT, 0, AVX2_BIT, 0x6U,
                                   1U, 1U, &features);
    EXPECT(features.avx_hardware_supported == 1U);
    EXPECT(features.avx_state_supported == 1U);
    EXPECT(features.avx2_hardware_supported == 1U);
    EXPECT(features.avx2_usable == 1U);

    /* A CPU without CPUID leaf 7 cannot expose AVX2, even with AVX state. */
    vgk_cpu_decode_vector_features(AVX_BIT | OSXSAVE_BIT | XSAVE_BIT, 0, AVX2_BIT, 0x6U,
                                   1U, 0U, &features);
    EXPECT(features.avx_state_supported == 1U);
    EXPECT(features.avx2_hardware_supported == 0U);
    EXPECT(features.avx2_usable == 0U);

    /* AVX2 hardware without AVX hardware/state is not usable. */
    vgk_cpu_decode_vector_features(OSXSAVE_BIT | XSAVE_BIT, 0, AVX2_BIT, 0x6U,
                                   1U, 1U, &features);
    EXPECT(features.avx2_hardware_supported == 1U);
    EXPECT(features.avx2_usable == 0U);

    if (failures != 0) {
        fprintf(stderr, "VGK CPU feature tests: %d failure(s)\n", failures);
        return 1;
    }

    puts("OK: NX, adresowanie fizyczne, SSE, AVX i AVX2.");
    return 0;
}
