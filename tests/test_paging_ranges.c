#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_paging_ranges.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static VGK_BOOT_INFO sample_info(void)
{
    VGK_BOOT_INFO info;
    memset(&info, 0, sizeof(info));
    info.flags = VGK_BOOT_FLAG_FRAMEBUFFER | VGK_BOOT_FLAG_ACPI;
    info.memory_map_address = UINT64_C(0x100123);
    info.memory_map_size = 0x2100U;
    info.kernel_segment_count = 2U;
    info.kernel_segments[0].physical_base = UINT64_C(0x200000);
    info.kernel_segments[0].memory_size = 0x1800U;
    info.kernel_segments[1].physical_base = UINT64_C(0x204123);
    info.kernel_segments[1].memory_size = 0x2000U;
    info.page_table_pool_base = UINT64_C(0x300000);
    info.page_table_pool_pages = 64U;
    info.acpi_rsdp_address = UINT64_C(0x500123);
    info.framebuffer_base = UINT64_C(0x800123);
    info.framebuffer_size = 0x10000U;
    info.framebuffer_width = 500U;
    info.framebuffer_height = 8U;
    info.framebuffer_stride = 512U;
    info.framebuffer_format = VGK_FRAMEBUFFER_FORMAT_RGBX8;
    return info;
}

int main(void)
{
    VGK_BOOT_INFO info = sample_info();
    VGK_PAGING_VIRTUAL_RANGE ranges[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
    size_t count = 99U;
    VGK_PAGING_REQUIREMENTS requirements;

    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600123), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_OK);
    CHECK(count == 7U);
    CHECK(vgk_paging_requirements_calculate(ranges, count, &requirements) ==
          VGK_PAGING_REQUIREMENTS_OK);
    CHECK(requirements.pml4_pages == 1U);
    CHECK(requirements.total_pages >= requirements.pml4_pages);
    CHECK(ranges[0].virtual_address == UINT64_C(0x200000));
    CHECK(ranges[0].length == 0x1800U);
    CHECK(ranges[1].virtual_address == UINT64_C(0x204000));
    CHECK(ranges[1].length == 0x2123U);
    CHECK(ranges[2].virtual_address == UINT64_C(0x600000));
    CHECK(ranges[2].length == sizeof(info) + 0x123U);
    CHECK(ranges[3].virtual_address == UINT64_C(0x100000));
    CHECK(ranges[3].length == 0x2223U);
    CHECK(ranges[4].virtual_address == UINT64_C(0x300000));
    CHECK(ranges[4].length == 64U * VGK_BOOT_PAGE_SIZE);
    CHECK(ranges[5].virtual_address == UINT64_C(0x500000));
    CHECK(ranges[5].length == 36U + 0x123U);
    CHECK(ranges[6].virtual_address == UINT64_C(0x800000));
    CHECK(ranges[6].length == 0x4123U); /* 512 * 8 * 4 + 0x123 offset */

    /* Framebuffer is omitted unless the caller proves its UC policy. */
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600123), 36U, 0,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_OK);
    CHECK(count == 6U); /* ACPI remains; framebuffer is omitted. */

    /* The protocol maximum is 16 ELF segments plus five shared objects. */
    info = sample_info();
    info.kernel_segment_count = VGK_BOOT_MAX_KERNEL_SEGMENTS;
    for (uint32_t i = 0U; i < VGK_BOOT_MAX_KERNEL_SEGMENTS; ++i) {
        info.kernel_segments[i].physical_base =
            UINT64_C(0x200000) + (uint64_t)i * UINT64_C(0x10000);
        info.kernel_segments[i].memory_size = 0x1000U;
    }
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_OK);
    CHECK(count == VGK_BOOT_MAX_KERNEL_SEGMENTS + 5U);
    CHECK(ranges[VGK_BOOT_MAX_KERNEL_SEGMENTS].virtual_address ==
          UINT64_C(0x600000));
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_BOOT_MAX_KERNEL_SEGMENTS + 4U, &count) ==
          VGK_PAGING_RANGES_TOO_MANY);
    CHECK(count == 0U);

    /* Without the ACPI flag, a zero RSDP length is required. */
    info = sample_info();
    info.flags = 0U;
    info.acpi_rsdp_address = 0U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 0U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_OK);
    CHECK(count == 5U);

    /* Required objects and sizes must be present and arithmetic must not wrap. */
    info.memory_map_address = 0U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 0U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);
    info = sample_info();
    info.kernel_segment_count = VGK_BOOT_MAX_KERNEL_SEGMENTS + 1U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);

    info = sample_info();
    info.kernel_segments[0].memory_size = 0U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);

    info = sample_info();
    info.page_table_pool_pages = UINT64_MAX;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);

    info = sample_info();
    info.flags = 0U;
    info.acpi_rsdp_address = 0U;
    info.kernel_segments[0].physical_base = UINT64_MAX - UINT64_C(0x7FF);
    info.kernel_segments[0].memory_size = 0x1000U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 0U, 0,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);

    info = sample_info();
    info.memory_map_address = UINT64_MAX - UINT64_C(0x7FF);
    info.memory_map_size = 0x1000U;
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);

    info = sample_info();
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 1,
              ranges, 5U, &count) == VGK_PAGING_RANGES_TOO_MANY);
    CHECK(count == 0U);

    CHECK(vgk_paging_ranges_build(&info, 0U, 36U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_INVALID_ARGUMENT);
    CHECK(count == 0U);
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 0U, 1,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_BAD_OBJECT);
    CHECK(count == 0U);
    CHECK(vgk_paging_ranges_build(&info, UINT64_C(0x600000), 36U, 2,
              ranges, VGK_PAGING_REQUIREMENTS_MAX_RANGES, &count) ==
          VGK_PAGING_RANGES_INVALID_ARGUMENT);
    CHECK(count == 0U);

    puts("OK: wspolny builder zakresow tablic stron");
    return 0;
}
