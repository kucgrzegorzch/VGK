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
    info.framebuffer_size = 0x400000U;
    info.framebuffer_width = 640U;
    info.framebuffer_height = 480U;
    info.framebuffer_stride = 640U;
    info.framebuffer_format = VGK_FRAMEBUFFER_FORMAT_RGBX8;
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 0, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    without_framebuffer = requirements;
    CHECK(vgk_paging_preflight_calculate(
              &info, UINT64_C(0x600000), 0U, 1, &requirements) ==
          VGK_PAGING_PREFLIGHT_OK);
    with_framebuffer = requirements;
    CHECK(with_framebuffer.total_pages == without_framebuffer.total_pages + 2U);

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

    /*
     * Validated device windows are included before page-table allocation.
     * The combined result must equal the direct calculation over the
     * standard object list plus the two additional identity ranges.
     */
    {
        VGK_PAGING_VIRTUAL_RANGE extra[2] = {
            {UINT64_C(0xFEE01000), VGK_BOOT_PAGE_SIZE},
            {UINT64_C(0xFEC00000), VGK_BOOT_PAGE_SIZE}
        };
        VGK_PAGING_VIRTUAL_RANGE all[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
        size_t all_count = 0U;
        VGK_PAGING_REQUIREMENTS base;
        VGK_PAGING_REQUIREMENTS combined;
        VGK_PAGING_REQUIREMENTS direct;

        info = sample_info();
        CHECK(vgk_paging_preflight_calculate(
                  &info, UINT64_C(0x600000), 0U, 0, &base) ==
              VGK_PAGING_PREFLIGHT_OK);
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  extra, 2U, &combined) == VGK_PAGING_PREFLIGHT_OK);
        CHECK(combined.total_pages > base.total_pages);

        CHECK(vgk_paging_ranges_build(
                  &info, UINT64_C(0x600000), 0U, 0, all,
                  VGK_PAGING_REQUIREMENTS_MAX_RANGES, &all_count) ==
              VGK_PAGING_RANGES_OK);
        CHECK(all_count + 2U <= VGK_PAGING_REQUIREMENTS_MAX_RANGES);
        all[all_count++] = extra[0];
        all[all_count++] = extra[1];
        CHECK(vgk_paging_requirements_calculate(
                  all, all_count, &direct) == VGK_PAGING_REQUIREMENTS_OK);
        CHECK(combined.pml4_pages == direct.pml4_pages);
        CHECK(combined.pdpt_pages == direct.pdpt_pages);
        CHECK(combined.pd_pages == direct.pd_pages);
        CHECK(combined.pt_pages == direct.pt_pages);
        CHECK(combined.total_pages == direct.total_pages);

        /* An extra range already covered by a kernel segment adds no pages. */
        extra[0].virtual_address = UINT64_C(0x200000);
        extra[0].length = VGK_BOOT_PAGE_SIZE;
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  extra, 1U, &combined) == VGK_PAGING_PREFLIGHT_OK);
        CHECK(combined.total_pages == base.total_pages);

        /* Null, malformed, noncanonical and over-capacity extras fail closed. */
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  0, 1U, &requirements) ==
              VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT);
        CHECK(requirements_are_zero(&requirements));

        extra[0].length = 0U;
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  extra, 1U, &requirements) ==
              VGK_PAGING_PREFLIGHT_REQUIREMENTS_BAD_RANGE);
        CHECK(requirements_are_zero(&requirements));

        extra[0].virtual_address = UINT64_C(0x0000800000000000);
        extra[0].length = VGK_BOOT_PAGE_SIZE;
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  extra, 1U, &requirements) ==
              VGK_PAGING_PREFLIGHT_REQUIREMENTS_BAD_RANGE);
        CHECK(requirements_are_zero(&requirements));

        for (size_t i = 0U; i < VGK_PAGING_REQUIREMENTS_MAX_RANGES; ++i) {
            extra[i % 2U].virtual_address =
                UINT64_C(0x100000000) + (uint64_t)i * VGK_BOOT_PAGE_SIZE;
            extra[i % 2U].length = VGK_BOOT_PAGE_SIZE;
        }
        CHECK(vgk_paging_preflight_calculate_with_additional_ranges(
                  &info, UINT64_C(0x600000), 0U, 0,
                  extra, VGK_PAGING_REQUIREMENTS_MAX_RANGES,
                  &requirements) == VGK_PAGING_PREFLIGHT_RANGE_TOO_MANY);
        CHECK(requirements_are_zero(&requirements));
    }
    return 0;
}
