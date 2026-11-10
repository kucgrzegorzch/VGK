#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vgk_memory.h"

#define PAGE VGK_MEMORY_PAGE_SIZE
#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static VGK_MEMORY_MAP_ENTRY memory_map[4];

int main(void)
{
    VGK_BOOT_INFO boot_info;
    VGK_MEMORY_STATS stats;
    uint64_t first_allocation = 0;
    uint64_t second_allocation = 0;
    int failures = 0;

    memset(&boot_info, 0, sizeof(boot_info));
    /* Low memory is deliberately reported usable; the allocator must skip it. */
    memory_map[0].base = UINT64_C(0x1000);
    memory_map[0].length = UINT64_C(0xFF000);
    memory_map[0].type = VGK_MEMORY_USABLE;
    memory_map[1].base = UINT64_C(0x101000);
    memory_map[1].length = UINT64_C(0xF000);
    memory_map[1].type = VGK_MEMORY_USABLE;
    memory_map[2].base = UINT64_C(0x110000);
    memory_map[2].length = UINT64_C(0x40000);
    memory_map[2].type = VGK_MEMORY_BOOTLOADER;
    memory_map[3].base = UINT64_C(0x150000);
    memory_map[3].length = UINT64_C(0x13000);
    memory_map[3].type = VGK_MEMORY_USABLE;

    boot_info.magic = VGK_BOOT_INFO_MAGIC;
    boot_info.version = VGK_BOOT_INFO_VERSION;
    boot_info.structure_size = sizeof(boot_info);
    boot_info.firmware_type = VGK_FIRMWARE_UEFI;
    boot_info.memory_map_address = (uint64_t)(uintptr_t)memory_map;
    boot_info.memory_map_size = sizeof(memory_map);
    boot_info.memory_map_entry_size = sizeof(VGK_MEMORY_MAP_ENTRY);
    boot_info.kernel_physical_base = UINT64_C(0x161000);
    boot_info.kernel_size = PAGE;
    boot_info.kernel_entry = UINT64_C(0x161000);
    boot_info.page_table_pool_base = UINT64_C(0x110000);
    boot_info.page_table_pool_pages = VGK_BOOT_PAGE_TABLE_POOL_PAGES;
    boot_info.kernel_segment_count = 1;
    boot_info.kernel_segments[0].physical_base = UINT64_C(0x161000);
    boot_info.kernel_segments[0].memory_size = PAGE;
    boot_info.kernel_segments[0].flags =
        VGK_KERNEL_SEGMENT_READ | VGK_KERNEL_SEGMENT_EXEC;

    EXPECT(vgk_memory_get_stats(&stats) == VGK_MEMORY_NOT_INITIALIZED);
    EXPECT(vgk_boot_info_is_valid(&boot_info));

    /* A sorted map containing an overlap must be rejected. */
    memory_map[2].base = UINT64_C(0x10F000);
    EXPECT(vgk_memory_init(&boot_info) == VGK_MEMORY_BAD_MAP);
    memory_map[2].base = UINT64_C(0x110000);

    memory_map[2].type = VGK_MEMORY_USABLE;
    EXPECT(vgk_memory_init(&boot_info) == VGK_MEMORY_BAD_MAP);
    memory_map[2].type = VGK_MEMORY_BOOTLOADER;

    EXPECT(vgk_memory_init(&boot_info) == VGK_MEMORY_OK);
    EXPECT(vgk_memory_get_stats(&stats) == VGK_MEMORY_OK);
    EXPECT(stats.total_pages == 33U);
    EXPECT(stats.free_pages == 33U);
    EXPECT(stats.managed_range_count == 3U);
    EXPECT(vgk_memory_free_pages(UINT64_C(0x161000), 1U) ==
           VGK_MEMORY_OUT_OF_RANGE);

    /* Kernel pages stay reserved even if firmware labels them usable. */
    EXPECT(vgk_memory_alloc_pages(1U, 1U, &first_allocation) ==
           VGK_MEMORY_OK);
    EXPECT(first_allocation == UINT64_C(0x101000));
    EXPECT(vgk_memory_free_pages(first_allocation, 1U) == VGK_MEMORY_OK);

    EXPECT(vgk_memory_alloc_pages(1U, 3U, &first_allocation) ==
           VGK_MEMORY_INVALID_ARGUMENT);
    EXPECT(vgk_memory_alloc_pages(2U, 4U, &first_allocation) ==
           VGK_MEMORY_OK);
    EXPECT(first_allocation == UINT64_C(0x104000));

    EXPECT(vgk_memory_alloc_pages(3U, 1U, &second_allocation) ==
           VGK_MEMORY_OK);
    EXPECT(second_allocation == UINT64_C(0x101000));

    EXPECT(vgk_memory_get_stats(&stats) == VGK_MEMORY_OK);
    EXPECT(stats.free_pages == 28U);

    /* Adjacent frees must merge back into the original free interval. */
    EXPECT(vgk_memory_free_pages(first_allocation, 2U) == VGK_MEMORY_OK);
    EXPECT(vgk_memory_free_pages(second_allocation, 3U) == VGK_MEMORY_OK);
    EXPECT(vgk_memory_get_stats(&stats) == VGK_MEMORY_OK);
    EXPECT(stats.free_pages == 33U);
    EXPECT(stats.free_range_count == 3U);

    EXPECT(vgk_memory_free_pages(second_allocation, 3U) ==
           VGK_MEMORY_DOUBLE_FREE);
    EXPECT(vgk_memory_free_pages(UINT64_C(0x200000), 1U) ==
           VGK_MEMORY_OUT_OF_RANGE);
    EXPECT(vgk_memory_free_pages(UINT64_C(0x101000), 20U) ==
           VGK_MEMORY_OUT_OF_RANGE);

    first_allocation = UINT64_MAX;
    EXPECT(vgk_memory_alloc_pages(34U, 1U, &first_allocation) ==
           VGK_MEMORY_NO_MEMORY);
    EXPECT(first_allocation == 0U);

    if (failures != 0) {
        fprintf(stderr, "VGK memory allocator: %d test(s) failed\n", failures);
        return 1;
    }

    /* ACPI RSDP may straddle a page boundary: reserve both pages. */
    boot_info.flags = VGK_BOOT_FLAG_ACPI;
    boot_info.acpi_rsdp_address = UINT64_C(0x150FF0);
    EXPECT(vgk_memory_init(&boot_info) == VGK_MEMORY_OK);
    EXPECT(vgk_memory_get_stats(&stats) == VGK_MEMORY_OK);
    EXPECT(stats.total_pages == 31U);
    EXPECT(vgk_memory_alloc_pages(15U, 1U, &first_allocation) ==
           VGK_MEMORY_OK);
    EXPECT(first_allocation == UINT64_C(0x101000));
    EXPECT(vgk_memory_alloc_pages(1U, 1U, &second_allocation) ==
           VGK_MEMORY_OK);
    EXPECT(second_allocation == UINT64_C(0x152000));
    EXPECT(vgk_memory_alloc_pages(16U, 1U, &first_allocation) ==
           VGK_MEMORY_NO_MEMORY);

    puts("OK: allocator stron pamieci – zakresy, wyrownanie, scalanie i ochrona przed podwojnym zwolnieniem.");
    return 0;
}
