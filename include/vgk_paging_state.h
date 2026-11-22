#ifndef VGK_PAGING_STATE_H
#define VGK_PAGING_STATE_H

#include <stdint.h>

typedef enum {
    VGK_PAGING_STATE_OK = 0,
    VGK_PAGING_STATE_INVALID_ARGUMENT,
    VGK_PAGING_STATE_UNSUPPORTED,
    VGK_PAGING_STATE_INCONSISTENT
} VGK_PAGING_STATE_RESULT;

typedef struct {
    uint64_t cr0;
    uint64_t cr3;
    uint64_t cr4;
    uint64_t efer;
    uint8_t physical_address_bits;
    uint8_t pcid_supported;
    uint8_t invpcid_supported;
    uint8_t la57_supported;
    uint8_t smep_supported;
    uint8_t smap_supported;
    uint8_t paging_active;
    uint8_t write_protect_enabled;
    uint8_t nx_enabled;
    uint8_t pcid_enabled;
    uint8_t global_pages_enabled;
    uint8_t five_level_paging_active;
    uint16_t current_pcid;
    uint64_t root_physical;
} VGK_PAGING_STATE;

/*
 * Pure decoder for a read-only CPU paging-state snapshot.
 * It never executes privileged instructions or changes processor state.
 */
VGK_PAGING_STATE_RESULT vgk_paging_state_decode(
    uint64_t cr0,
    uint64_t cr3,
    uint64_t cr4,
    uint64_t efer,
    uint8_t physical_address_bits,
    uint8_t pcid_supported,
    uint8_t invpcid_supported,
    uint8_t la57_supported,
    uint8_t smep_supported,
    uint8_t smap_supported,
    VGK_PAGING_STATE *state);

/*
 * Capture the current x86-64 paging state using CPUID, MOV-from-CR and
 * RDMSR(EFER). This routine is read-only and must execute at CPL0.
 * It does not write CR0/CR3/CR4/EFER, change paging depth, or flush the TLB.
 */
VGK_PAGING_STATE_RESULT vgk_paging_state_capture(VGK_PAGING_STATE *state);

#endif
