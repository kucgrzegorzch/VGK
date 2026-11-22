#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "vgk_paging_state.h"

#define EXPECT(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

#define CR0_BASE ((UINT64_C(1) << 0) | (UINT64_C(1) << 31))
#define CR4_BASE (UINT64_C(1) << 5)
#define EFER_BASE ((UINT64_C(1) << 8) | (UINT64_C(1) << 10))
int main(void)
{
    VGK_PAGING_STATE state;
    int failures = 0;
    VGK_PAGING_STATE_RESULT result;

    memset(&state, 0xA5, sizeof(state));
    result = vgk_paging_state_decode(
        CR0_BASE | (UINT64_C(1) << 16),
        UINT64_C(0x12345000), CR4_BASE | (UINT64_C(1) << 7),
        EFER_BASE | (UINT64_C(1) << 11), 48U,
        1U, 1U, 1U, 1U, 1U, &state);
    EXPECT(result == VGK_PAGING_STATE_OK);
    EXPECT(state.paging_active == 1U);
    EXPECT(state.write_protect_enabled == 1U);
    EXPECT(state.nx_enabled == 1U);
    EXPECT(state.global_pages_enabled == 1U);
    EXPECT(state.pcid_enabled == 0U);
    EXPECT(state.five_level_paging_active == 0U);
    EXPECT(state.root_physical == UINT64_C(0x12345000));
    EXPECT(state.current_pcid == 0U);
    EXPECT(state.invpcid_supported == 1U);

    result = vgk_paging_state_decode(
        CR0_BASE, UINT64_C(0xABCDF123), CR4_BASE | (UINT64_C(1) << 17),
        EFER_BASE, 48U, 1U, 1U, 1U, 0U, 0U, &state);
    EXPECT(result == VGK_PAGING_STATE_OK);
    EXPECT(state.pcid_enabled == 1U);
    EXPECT(state.current_pcid == 0x123U);
    EXPECT(state.root_physical == UINT64_C(0xABCDF000));

    result = vgk_paging_state_decode(
        CR0_BASE, UINT64_C(0x12345000), CR4_BASE | (UINT64_C(1) << 12),
        EFER_BASE, 48U, 0U, 0U, 0U, 0U, 0U, &state);
    EXPECT(result == VGK_PAGING_STATE_UNSUPPORTED);

    result = vgk_paging_state_decode(
        CR0_BASE, UINT64_C(0x12345000), CR4_BASE | (UINT64_C(1) << 17),
        EFER_BASE, 48U, 0U, 0U, 1U, 0U, 0U, &state);
    EXPECT(result == VGK_PAGING_STATE_UNSUPPORTED);

    EXPECT(vgk_paging_state_decode(
        CR0_BASE & ~(UINT64_C(1) << 31), 0x1000U, CR4_BASE,
        EFER_BASE, 48U, 0U, 0U, 0U, 0U, 0U, &state) ==
        VGK_PAGING_STATE_INCONSISTENT);
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, 0x1000U, 0U, EFER_BASE, 48U,
        0U, 0U, 0U, 0U, 0U, &state) == VGK_PAGING_STATE_INCONSISTENT);
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, 0x1000U, CR4_BASE, EFER_BASE & ~(UINT64_C(1) << 10),
        48U, 0U, 0U, 0U, 0U, 0U, &state) == VGK_PAGING_STATE_INCONSISTENT);

    /* With PCIDE clear, CR3 low bits other than PWT/PCD are reserved. */
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, 0x1001U, CR4_BASE, EFER_BASE, 48U,
        0U, 0U, 0U, 0U, 0U, &state) == VGK_PAGING_STATE_INCONSISTENT);
    /* A CR3 address outside the reported physical width is rejected. */
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, UINT64_C(0x0001000000000000), CR4_BASE,
        EFER_BASE, 48U, 0U, 0U, 0U, 0U, 0U, &state) ==
        VGK_PAGING_STATE_INCONSISTENT);
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, 0x1000U, CR4_BASE, EFER_BASE, 53U,
        0U, 0U, 0U, 0U, 0U, &state) == VGK_PAGING_STATE_INVALID_ARGUMENT);
    EXPECT(vgk_paging_state_decode(
        CR0_BASE, 0x1000U, CR4_BASE, EFER_BASE, 48U,
        0U, 0U, 0U, 0U, 0U, 0) == VGK_PAGING_STATE_INVALID_ARGUMENT);

    if (failures != 0) return 1;
    puts("OK: read-only paging state decoder");
    return 0;
}
