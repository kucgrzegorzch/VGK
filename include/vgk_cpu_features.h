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
} VGK_CPU_FEATURES;

/*
 * Decode CPUID results. Kept separate from the instruction so tests can
 * exercise missing features and invalid physical-address widths.
 */
VGK_CPU_RESULT vgk_cpu_decode_features(
    uint32_t max_basic_leaf,
    uint32_t max_extended_leaf,
    uint32_t extended_feature_edx,
    uint32_t address_size_eax,
    VGK_CPU_FEATURES *features);

/* Read CPUID on the current x86-64 processor and decode the required fields. */
VGK_CPU_RESULT vgk_cpu_detect_features(VGK_CPU_FEATURES *features);

#endif
