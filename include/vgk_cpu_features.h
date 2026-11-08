#ifndef VGK_CPU_FEATURES_H
#define VGK_CPU_FEATURES_H

#include <stdint.h>

typedef enum {
    VGK_CPU_OK = 0,
    VGK_CPU_INVALID_ARGUMENT,
    VGK_CPU_UNSUPPORTED
} VGK_CPU_RESULT;

typedef struct {
    uint32_t max_basic_leaf;
    uint32_t max_extended_leaf;
    uint8_t physical_address_bits;
    uint8_t nx_supported;
    uint8_t reserved[2];

    /* Identity is diagnostic only; VGK must not whitelist CPU model names. */
    char vendor[13];
    uint16_t family;
    uint16_t model;
    uint8_t stepping;

    /* Hardware capabilities and OS-usable vector state are kept separate. */
    uint8_t sse2_supported;
    uint8_t sse3_supported;
    uint8_t ssse3_supported;
    uint8_t sse4_1_supported;
    uint8_t sse4_2_supported;
    uint8_t avx_hardware_supported;
    uint8_t avx_state_supported;
    uint8_t avx2_hardware_supported;
    uint8_t avx2_usable;
} VGK_CPU_FEATURES;

/*
 * Decode CPUID results for the minimum memory-protection requirements.
 * Kept separate from the instruction so tests can exercise edge cases.
 */
VGK_CPU_RESULT vgk_cpu_decode_features(
    uint32_t max_basic_leaf,
    uint32_t max_extended_leaf,
    uint32_t extended_feature_edx,
    uint32_t address_size_eax,
    VGK_CPU_FEATURES *features);

/* Read CPUID and identify capabilities on the current x86-64 processor. */
VGK_CPU_RESULT vgk_cpu_detect_features(VGK_CPU_FEATURES *features);

#endif
