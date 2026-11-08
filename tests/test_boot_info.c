#include <stdio.h>
#include <string.h>

#include "vgk_boot_info.h"

static VGK_BOOT_INFO valid_info(void)
{
    VGK_BOOT_INFO info;
    memset(&info, 0, sizeof(info));
    info.magic = VGK_BOOT_INFO_MAGIC;
    info.version = VGK_BOOT_INFO_VERSION;
    info.structure_size = sizeof(info);
    info.firmware_type = VGK_FIRMWARE_UEFI;
    info.memory_map_address = UINT64_C(0x100000);
    info.memory_map_size = sizeof(VGK_MEMORY_MAP_ENTRY);
    info.memory_map_entry_size = sizeof(VGK_MEMORY_MAP_ENTRY);
    info.kernel_physical_base = UINT64_C(0x200000);
    info.kernel_size = UINT64_C(0x40000);
    info.kernel_entry = UINT64_C(0x200100);
    info.kernel_segment_count = 1;
    info.kernel_segments[0].physical_base = UINT64_C(0x200000);
    info.kernel_segments[0].memory_size = UINT64_C(0x1000);
    info.kernel_segments[0].flags =
        VGK_KERNEL_SEGMENT_READ | VGK_KERNEL_SEGMENT_EXEC;
    return info;
}

int main(void)
{
    VGK_BOOT_INFO info = valid_info();
    unsigned failures = 0;

    if (!vgk_boot_info_is_valid(&info)) {
        puts("FAIL: poprawny protokol zostal odrzucony");
        ++failures;
    }

    info.magic ^= 1U;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano niepoprawny znacznik");
        ++failures;
    }
    info = valid_info();

    info.version++;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano nieznana wersje");
        ++failures;
    }
    info = valid_info();

    info.memory_map_size++;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano niepelny wpis mapy pamieci");
        ++failures;
    }
    info = valid_info();
    info.kernel_entry = info.kernel_physical_base - 1U;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano punkt wejscia przed jadrem");
        ++failures;
    }

    info = valid_info();
    info.kernel_entry = info.kernel_physical_base + info.kernel_size;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano punkt wejscia poza jadrem");
        ++failures;
    }

    info = valid_info();
    info.memory_map_address = UINT64_MAX - 15U;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano przepełnienie zakresu mapy pamieci");
        ++failures;
    }

    info = valid_info();
    info.kernel_physical_base = UINT64_MAX - 15U;
    info.kernel_size = 64U;
    info.kernel_entry = info.kernel_physical_base;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano przepełnienie zakresu jadra");
        ++failures;
    }

    info = valid_info();
    info.kernel_segments[0].flags =
        VGK_KERNEL_SEGMENT_READ | VGK_KERNEL_SEGMENT_WRITE |
        VGK_KERNEL_SEGMENT_EXEC;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano segment jednoczesnie zapisywalny i wykonywalny");
        ++failures;
    }

    info = valid_info();
    info.kernel_segments[0].physical_base += UINT64_C(0x1000);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano segment poza zakresem jadra");
        ++failures;
    }

    info = valid_info();
    info.kernel_segments[0].flags = VGK_KERNEL_SEGMENT_READ;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano punkt wejscia w niewykonywalnym segmencie");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_FRAMEBUFFER;
    info.framebuffer_base = UINT64_C(0xE0000000);
    info.framebuffer_size = UINT64_C(0x400000);
    info.framebuffer_width = 1024;
    info.framebuffer_height = 768;
    info.framebuffer_stride = 1024;
    info.framebuffer_format = 1;
    if (!vgk_boot_info_is_valid(&info)) {
        puts("FAIL: odrzucono poprawny opis obrazu");
        ++failures;
    }

    info.framebuffer_stride = 1000;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano za krotki wiersz obrazu");
        ++failures;
    }

    info.framebuffer_stride = 1024;
    info.framebuffer_base = UINT64_MAX - 15U;
    info.framebuffer_size = 64U;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano przepełnienie zakresu obrazu");
        ++failures;
    }
    info = valid_info();

    info.flags = VGK_BOOT_FLAG_ACPI;
    info.acpi_rsdp_address = UINT64_C(0x7F000);
    if (!vgk_boot_info_is_valid(&info)) {
        puts("FAIL: odrzucono poprawny adres ACPI");
        ++failures;
    }

    info.reserved[2] = 1;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano niezerowe pole zarezerwowane");
        ++failures;
    }

    if (failures != 0)
        return 1;

    puts("OK: protokol startowy VGK i przypadki bledne sprawdzone");
    return 0;
}
