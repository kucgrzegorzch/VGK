#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_paging_requirements.h"
#include "vgk_boot_info.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int expect(
    const VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t count,
    uint64_t pdpt,
    uint64_t pd,
    uint64_t pt,
    uint64_t total)
{
    VGK_PAGING_REQUIREMENTS result;
    CHECK(vgk_paging_requirements_calculate(ranges, count, &result) ==
          VGK_PAGING_REQUIREMENTS_OK);
    CHECK(result.pml4_pages == 1U);
    CHECK(result.pdpt_pages == pdpt);
    CHECK(result.pd_pages == pd);
    CHECK(result.pt_pages == pt);
    CHECK(result.total_pages == total);
    return 0;
}

static int test_worst_case_page_table_requirement(void)
{
    const uint64_t pml4_span = UINT64_C(1) << 39U;
    const VGK_PAGING_VIRTUAL_RANGE ranges[] = {
        /* Kernel can occupy the entire allowed 2 MiB..1 GiB load window. */
        { UINT64_C(0x00200000), UINT64_C(0x3FE00000) },
        /* 256 MiB framebuffer straddling a 512 GiB PML4 boundary. */
        { (UINT64_C(2) * pml4_span) - (UINT64_C(128) << 20U) +
              UINT64_C(4096),
          UINT64_C(256) << 20U },
        /* 4 MiB pool, page-aligned but offset from a 2 MiB boundary. */
        { (UINT64_C(4) * pml4_span) - (UINT64_C(2) << 20U) +
              UINT64_C(4096),
          UINT64_C(4) << 20U },
        /* Maximum normalized map buffer, crossing 2 MiB/1 GiB/512 GiB. */
        { (UINT64_C(6) * pml4_span) - UINT64_C(4096),
          (UINT64_C(512) << 10U) + UINT64_C(4080) },
        /* VGK_BOOT_INFO is 8-byte aligned and may straddle the same boundaries. */
        { (UINT64_C(8) * pml4_span) - UINT64_C(4096),
          (uint64_t)sizeof(VGK_BOOT_INFO) + UINT64_C(4088) },
        /* The 16-byte-aligned ACPI RSDP may straddle a page boundary. */
        { (UINT64_C(10) * pml4_span) - UINT64_C(4096),
          UINT64_C(36) + UINT64_C(4080) }
    };
    VGK_PAGING_REQUIREMENTS result;

    CHECK(sizeof(VGK_BOOT_INFO) == 2376U);
    CHECK(vgk_paging_requirements_calculate(
              ranges, sizeof(ranges) / sizeof(ranges[0]), &result) ==
          VGK_PAGING_REQUIREMENTS_OK);
    CHECK(result.pml4_pages == 1U);
    CHECK(result.pt_pages == 649U);
    CHECK(result.pd_pages == 11U);
    CHECK(result.pdpt_pages == 11U);
    CHECK(result.total_pages == 672U);
    CHECK(result.total_pages < 1024U);
    return 0;
}

int main(void)
{
    CHECK(test_worst_case_page_table_requirement() == 0);
    VGK_PAGING_REQUIREMENTS result;
    VGK_PAGING_VIRTUAL_RANGE ranges[VGK_PAGING_REQUIREMENTS_MAX_RANGES];

    CHECK(expect(0, 0, 0, 0, 0, 1) == 0);

    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x200000), 4096U };
    CHECK(expect(ranges, 1, 1, 1, 1, 4) == 0);

    /* Multiple pages in one 2 MiB PT coverage share all structures. */
    ranges[0].length = UINT64_C(0x1FF000);
    CHECK(expect(ranges, 1, 1, 1, 1, 4) == 0);

    /* Crossing a 2 MiB boundary creates a second PT. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x1FF000), 8192U };
    CHECK(expect(ranges, 1, 1, 1, 2, 5) == 0);

    /* Crossing a 1 GiB boundary creates a second PD and PT. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x3FFFF000), 8192U };
    CHECK(expect(ranges, 1, 1, 2, 2, 6) == 0);

    /* Crossing a 512 GiB PML4 slot boundary creates two branches. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x7FFFFFF000), 8192U };
    CHECK(expect(ranges, 1, 2, 2, 2, 7) == 0);

    /* High canonical addresses use masked page-table indices too. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0xFFFF800000000000), UINT64_C(0x201000) };
    CHECK(expect(ranges, 1, 1, 1, 2, 5) == 0);

    /* Duplicate and overlapping ranges must not double-count structures. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x400000), 0x5000U };
    ranges[1] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x402000), 0x5000U };
    ranges[2] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x400000), 0x5000U };
    CHECK(expect(ranges, 3, 1, 1, 1, 4) == 0);

    /* Distinct PML4 branches count separately even when each range is tiny. */
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x200000), 4096U };
    ranges[1] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0xFFFF800000000000), 4096U };
    CHECK(expect(ranges, 2, 2, 2, 2, 7) == 0);

    memset(&result, 0xA5, sizeof(result));
    CHECK(vgk_paging_requirements_calculate(0, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT);
    CHECK(vgk_paging_requirements_calculate(ranges,
          VGK_PAGING_REQUIREMENTS_MAX_RANGES + 1U, &result) ==
          VGK_PAGING_REQUIREMENTS_TOO_MANY_RANGES);
    CHECK(vgk_paging_requirements_calculate(ranges, 1, 0) ==
          VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT);

    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x200001), 4096U };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){ UINT64_C(0x200000), 0U };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0x00007FFFFFFFF000), 8192U };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0x0000800000000000), 4096U };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0xFFFFFFFFFFFFF000), 8192U };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);
    ranges[0] = (VGK_PAGING_VIRTUAL_RANGE){
        UINT64_C(0x200000), UINT64_MAX };
    CHECK(vgk_paging_requirements_calculate(ranges, 1, &result) ==
          VGK_PAGING_REQUIREMENTS_BAD_RANGE);

    puts("OK: kalkulator zapotrzebowania tablic stron");
    return 0;
}
