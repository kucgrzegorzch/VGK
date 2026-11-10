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
    info.page_table_pool_base = UINT64_C(0x300000);
    info.page_table_pool_pages = VGK_BOOT_PAGE_TABLE_POOL_PAGES;
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
    info.page_table_pool_pages = 0;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano pusta pule tablic stron");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_base++;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano niewyrownana pule tablic stron");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_base = UINT64_MAX - UINT64_C(0xFFF);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano przepelnienie zakresu puli tablic stron");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_base = UINT64_C(0x220000);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano pule nachodzaca na zakres jadra");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_base = UINT64_C(0x100000);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano pule nachodzaca na mape pamieci");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_pages--;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano nieoczekiwany rozmiar puli tablic stron");
        ++failures;
    }

    info = valid_info();
    info.page_table_pool_base =
        (uint64_t)(uintptr_t)&info & ~(VGK_BOOT_PAGE_SIZE - 1U);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano pule nachodzaca na strukture startowa");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_FRAMEBUFFER;
    info.framebuffer_base = UINT64_C(0x300000);
    info.framebuffer_size = UINT64_C(0x400000);
    info.framebuffer_width = 1024;
    info.framebuffer_height = 768;
    info.framebuffer_stride = 1024;
    info.framebuffer_format = VGK_FRAMEBUFFER_FORMAT_RGBX8;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano pule nachodzaca na framebuffer");
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
    info.kernel_segment_count = 0;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano brak opisu segmentow jadra");
        ++failures;
    }

    info = valid_info();
    info.kernel_segments[1].physical_base = UINT64_C(0x300000);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano dane w nieuzywanym slocie segmentu");
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
    info.kernel_segment_count = 2;
    info.kernel_segments[1].physical_base = UINT64_C(0x200000);
    info.kernel_segments[1].memory_size = UINT64_C(0x1000);
    info.kernel_segments[1].flags =
        VGK_KERNEL_SEGMENT_READ | VGK_KERNEL_SEGMENT_WRITE;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano dwa segmenty w tych samych stronach");
        ++failures;
    }

    info = valid_info();
    info.kernel_segments[0].flags = VGK_KERNEL_SEGMENT_READ;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano punkt wejscia w niewykonywalnym segmencie");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_PCI;
    info.pci_device_count = 1;
    info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
    info.pci_devices[0].vendor_id = UINT16_C(0x1234);
    info.pci_devices[0].device_id = UINT16_C(0x5678);
    info.pci_devices[0].bus = 0;
    info.pci_devices[0].device = 2;
    info.pci_devices[0].function = 0;
    if (!vgk_boot_info_is_valid(&info)) {
        puts("FAIL: odrzucono poprawny inwentarz PCI");
        ++failures;
    }

    info.pci_devices[0].device = 32;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano niepoprawny numer urzadzenia PCI");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_PCI;
    info.pci_device_count = 1;
    info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
    info.pci_devices[0].vendor_id = UINT16_C(0xFFFF);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano nieobecne urzadzenie PCI");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_PCI | VGK_BOOT_FLAG_PCI_TRUNCATED;
    info.pci_device_count = 1;
    info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
    info.pci_devices[0].vendor_id = UINT16_C(0x1234);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano bledny znacznik obciecia listy PCI");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_PCI;
    info.pci_device_count = 1;
    info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
    info.pci_devices[0].vendor_id = UINT16_C(0x1234);
    info.pci_devices[1].vendor_id = UINT16_C(0x1234);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano dane poza zakresem listy PCI");
        ++failures;
    }

    info = valid_info();
    info.flags = VGK_BOOT_FLAG_PCI;
    info.pci_device_count = 2;
    info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
    info.pci_devices[0].vendor_id = UINT16_C(0x1234);
    info.pci_devices[0].device_id = UINT16_C(0x1111);
    info.pci_devices[0].device = 3;
    info.pci_devices[1] = info.pci_devices[0];
    info.pci_devices[1].device_id = UINT16_C(0x2222);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano powtorzony adres funkcji PCI");
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
    info.framebuffer_size = UINT64_C(1024) * 768U * 4U - 1U;
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano bufor mniejszy od pelnego obrazu");
        ++failures;
    }

    info.framebuffer_size = UINT64_C(0x400000);
    info.framebuffer_format = UINT32_C(99);
    if (vgk_boot_info_is_valid(&info)) {
        puts("FAIL: zaakceptowano nieznany format obrazu");
        ++failures;
    }

    info.framebuffer_format = VGK_FRAMEBUFFER_FORMAT_RGBX8;
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
