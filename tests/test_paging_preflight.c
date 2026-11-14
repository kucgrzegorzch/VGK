#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_paging_preflight.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static VGK_BOOT_INFO sample_info(void)
{
    VGK_BOOT_INFO info;
    memset(&info, 0, sizeof(info));
    info.memory_map_address = UINT64_C(0x100000);
    info.memory_map_size = 0x1000U;
    info.kernel_segment_count = 1U;
    info.kernel_segments[0].physical_base = UINT64_C(0x200000);
    info.kernel_segments[0].memory_size = 0x3000U;
    info.page_table_pool_base = UINT64_C(0x300000);
    info.page_table_pool_pages = 64U;
    return info;
}

static int requirements_are_zero(const VGK_PAGING_REQUIREMENTS *r)
{
    return r->pml4_pages == 0U && r->pdpt_pages == 0U &&
           r->pd_pages == 0U && r->pt_pages == 0U &&
           r->total_pages == 0U;
}

int main(void)
{
    VGK_BOOT_INFO info = sample_info();
    VGK_PAGING_REQUIREMENTS requirements;
    VGK_PAGING_REQUIREMENTS without_framebuffer;
    VGK_PAGING_REQUIREMENTS with_framebuffer;

    memset(&requirements, 0xA5, sizeof(requirements));
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    CHECK(requirements.pml4_pages == 1U);
    CHECK(requirements.total_pages >= requirements.pml4_pages);
    without_framebuffer = requirements;

    /* A validated ACPI length is included only with the ACPI boot flag. */
    info.flags = VGK_BOOT_FLAG_ACPI;
    info.acpi_rsdp_address = UINT64_C(0x500123);
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 36U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    CHECK(requirements.total_pages >= without_framebuffer.total_pages);

    /* A framebuffer contributes only when the caller confirms UC policy. */
    info = sample_info();
    info.flags = VGK_BOOT_FLAG_FRAMEBUFFER;
    info.framebuffer_base = UINT64_C(0x40000000);
    info.framebuffer_size = 0x200000U;
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    without_framebuffer = requirements;
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 1, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    with_framebuffer = requirements;
    CHECK(with_framebuffer.total_pages > without_framebuffer.total_pages);

    /* Invalid arguments and inconsistent ACPI metadata fail closed. */
    CHECK(vgk_paging_preflight_calculate(
              0, UINT64_C(0x600000), 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT);
    CHECK(requirements_are_zero(&requirements));
    CHECK(vgk_paging_preflight_calculate(
              &info, 0U, 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT);
    CHECK(requirements_are_zero(&requirements));
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 2, &requirements) ==
          VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT);
    CHECK(requirements_are_zero(&requirements));
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 36U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_RANGE_BAD_OBJECT);
    CHECK(requirements_are_zero(&requirements));

    /* Invalid object arithmetic cannot leak a partial calculation. */
    info = sample_info();
    info.kernel_segments[0].physical_base =
        UINT64_MAX - UINT64_C(0x7FF);
    info.kernel_segments[0].memory_size = 0x1000U;
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_RANGE_BAD_OBJECT);
    CHECK(requirements_are_zero(&requirements));

    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 0, 0) ==
          VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT);
    return 0;
}
