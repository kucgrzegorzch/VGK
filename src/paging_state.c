#include "vgk_paging_state.h"

#define CR0_PE (UINT64_C(1) << 0)
#define CR0_WP (UINT64_C(1) << 16)
#define CR0_PG (UINT64_C(1) << 31)
#define CR4_PAE (UINT64_C(1) << 5)
#define CR4_PGE (UINT64_C(1) << 7)
#define CR4_LA57 (UINT64_C(1) << 12)
#define CR4_PCIDE (UINT64_C(1) << 17)
#define EFER_LME (UINT64_C(1) << 8)
#define EFER_LMA (UINT64_C(1) << 10)
#define EFER_NXE (UINT64_C(1) << 11)
#define CPUID_PCID (UINT32_C(1) << 17)
#define CPUID_INVPCID (UINT32_C(1) << 10)
#define CPUID_LA57 (UINT32_C(1) << 16)
#define CPUID_SMEP (UINT32_C(1) << 7)
#define CPUID_SMAP (UINT32_C(1) << 20)

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
    VGK_PAGING_STATE *state)
{
    uint64_t physical_mask;
    uint64_t allowed_cr3_bits;
    uint8_t pcide;
    uint8_t la57;

    if (state == 0)
        return VGK_PAGING_STATE_INVALID_ARGUMENT;

    *state = (VGK_PAGING_STATE){0};

    if (physical_address_bits < 32U || physical_address_bits > 52U ||
        pcid_supported > 1U || invpcid_supported > 1U ||
        la57_supported > 1U || smep_supported > 1U || smap_supported > 1U)
        return VGK_PAGING_STATE_INVALID_ARGUMENT;

    pcide = (uint8_t)((cr4 & CR4_PCIDE) != 0U);
    la57 = (uint8_t)((cr4 & CR4_LA57) != 0U);

    if ((cr0 & (CR0_PE | CR0_PG)) != (CR0_PE | CR0_PG) ||
        (cr4 & CR4_PAE) == 0U ||
        (efer & (EFER_LME | EFER_LMA)) != (EFER_LME | EFER_LMA))
        return VGK_PAGING_STATE_INCONSISTENT;

    if ((pcide != 0U && pcid_supported == 0U) ||
        (la57 != 0U && la57_supported == 0U))
        return VGK_PAGING_STATE_UNSUPPORTED;

    physical_mask = ((UINT64_C(1) << physical_address_bits) - 1U) &
                    ~UINT64_C(0xFFF);
    allowed_cr3_bits = physical_mask |
        (pcide != 0U ? UINT64_C(0xFFF) : UINT64_C(0x018));
    if ((cr3 & ~allowed_cr3_bits) != 0U)
        return VGK_PAGING_STATE_INCONSISTENT;

    state->cr0 = cr0;
    state->cr3 = cr3;
    state->cr4 = cr4;
    state->efer = efer;
    state->physical_address_bits = physical_address_bits;
    state->pcid_supported = pcid_supported;
    state->invpcid_supported = invpcid_supported;
    state->la57_supported = la57_supported;
    state->smep_supported = smep_supported;
    state->smap_supported = smap_supported;
    state->paging_active = 1U;
    state->write_protect_enabled = (uint8_t)((cr0 & CR0_WP) != 0U);
    state->nx_enabled = (uint8_t)((efer & EFER_NXE) != 0U);
    state->pcid_enabled = pcide;
    state->global_pages_enabled = (uint8_t)((cr4 & CR4_PGE) != 0U);
    state->five_level_paging_active = la57;
    state->current_pcid = pcide != 0U ? (uint16_t)(cr3 & 0xFFFU) : 0U;
    state->root_physical = cr3 & physical_mask;
    return VGK_PAGING_STATE_OK;
}

static void vgk_paging_state_cpuid(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx)
{
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(leaf), "c"(subleaf));
    if (eax != 0) *eax = a;
    if (ebx != 0) *ebx = b;
    if (ecx != 0) *ecx = c;
    if (edx != 0) *edx = d;
}

VGK_PAGING_STATE_RESULT vgk_paging_state_capture(VGK_PAGING_STATE *state)
{
    uint64_t cr0, cr3, cr4, efer;
    uint32_t max_basic, max_extended, address_size = 0U;
    uint32_t leaf1_ecx = 0U, leaf7_ebx = 0U, leaf7_ecx = 0U;
    uint8_t physical_bits;
    uint32_t low, high;

    if (state == 0)
        return VGK_PAGING_STATE_INVALID_ARGUMENT;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(UINT32_C(0xC0000080)));
    efer = ((uint64_t)high << 32) | low;

    vgk_paging_state_cpuid(0U, 0U, &max_basic, 0, 0, 0);
    vgk_paging_state_cpuid(UINT32_C(0x80000000), 0U, &max_extended, 0, 0, 0);
    if (max_extended < UINT32_C(0x80000008))
        return VGK_PAGING_STATE_UNSUPPORTED;
    vgk_paging_state_cpuid(UINT32_C(0x80000008), 0U, &address_size, 0, 0, 0);
    physical_bits = (uint8_t)(address_size & UINT32_C(0xFF));
    if (max_basic >= 1U)
        vgk_paging_state_cpuid(1U, 0U, 0, 0, &leaf1_ecx, 0);
    if (max_basic >= 7U)
        vgk_paging_state_cpuid(7U, 0U, 0, &leaf7_ebx, &leaf7_ecx, 0);

    return vgk_paging_state_decode(
        cr0, cr3, cr4, efer, physical_bits,
        (uint8_t)((leaf1_ecx & CPUID_PCID) != 0U),
        (uint8_t)((leaf7_ebx & CPUID_INVPCID) != 0U),
        (uint8_t)((leaf7_ecx & CPUID_LA57) != 0U),
        (uint8_t)((leaf7_ebx & CPUID_SMEP) != 0U),
        (uint8_t)((leaf7_ebx & CPUID_SMAP) != 0U),
        state);
}
