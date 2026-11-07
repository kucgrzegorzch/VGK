#ifndef VGK_BOOT_INFO_H
#define VGK_BOOT_INFO_H

/*
 * Wspolny, wersjonowany protokol przekazania informacji startowych do jadra.
 *
 * Struktura nie jest jeszcze podlaczona do programu UEFI ani do jadra.
 * Jej uklad jest niezalezny od struktur firmware. Ladowacz UEFI i przyszly
 * ladowacz BIOS musza wypelnic ja zgodnie z tym samym kontraktem.
 */
#include <stddef.h>
#include <stdint.h>

#define VGK_BOOT_INFO_MAGIC UINT64_C(0x314F464E494B4756) /* "VGKINFO1" */
#define VGK_BOOT_INFO_VERSION UINT32_C(1)

#define VGK_BOOT_FLAG_FRAMEBUFFER UINT64_C(0x0000000000000001)
#define VGK_BOOT_FLAG_ACPI        UINT64_C(0x0000000000000002)
#define VGK_BOOT_KNOWN_FLAGS \
    (VGK_BOOT_FLAG_FRAMEBUFFER | VGK_BOOT_FLAG_ACPI)

typedef enum {
    VGK_FIRMWARE_UEFI = 1,
    VGK_FIRMWARE_BIOS = 2
} VGK_FIRMWARE_TYPE;

/* Znormalizowany wpis mapy pamieci VGK, wspolny dla UEFI i BIOS. */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint64_t type;
    uint64_t attributes;
} VGK_MEMORY_MAP_ENTRY;

typedef enum {
    VGK_MEMORY_USABLE = 1,
    VGK_MEMORY_RESERVED = 2,
    VGK_MEMORY_FIRMWARE = 3,
    VGK_MEMORY_ACPI_RECLAIM = 4,
    VGK_MEMORY_ACPI_NVS = 5,
    VGK_MEMORY_BAD = 6,
    VGK_MEMORY_BOOTLOADER = 7,
    VGK_MEMORY_KERNEL = 8,
    VGK_MEMORY_FRAMEBUFFER = 9
} VGK_MEMORY_TYPE;

/*
 * Pola adresowe sa 64-bitowe, aby ten sam naglowek mogl byc uzyty
 * przez ladowacz i jadro bez zaleznosci od rozmiaru typow wskaznikowych.
 */
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t structure_size;
    uint64_t flags;

    uint32_t firmware_type;
    uint32_t reserved0;

    uint64_t memory_map_address;
    uint64_t memory_map_size;
    uint32_t memory_map_entry_size;
    uint32_t reserved1;

    uint64_t kernel_physical_base;
    uint64_t kernel_size;
    uint64_t kernel_entry;

    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_stride;
    uint32_t framebuffer_format;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;

    uint64_t acpi_rsdp_address;

    uint64_t reserved[4];
} VGK_BOOT_INFO;

static inline int vgk_boot_info_is_valid(const VGK_BOOT_INFO *info)
{
    uint64_t entry_count;

    if (info == NULL ||
        info->magic != VGK_BOOT_INFO_MAGIC ||
        info->version != VGK_BOOT_INFO_VERSION ||
        info->structure_size < sizeof(VGK_BOOT_INFO) ||
        (info->flags & ~VGK_BOOT_KNOWN_FLAGS) != 0 ||
        (info->firmware_type != VGK_FIRMWARE_UEFI &&
         info->firmware_type != VGK_FIRMWARE_BIOS) ||
        info->reserved0 != 0 || info->reserved1 != 0 ||
        info->memory_map_address == 0 || info->memory_map_size == 0 ||
        info->memory_map_entry_size != sizeof(VGK_MEMORY_MAP_ENTRY) ||
        info->memory_map_size % sizeof(VGK_MEMORY_MAP_ENTRY) != 0 ||
        (info->memory_map_address & 7U) != 0 ||
        info->kernel_physical_base == 0 || info->kernel_size == 0 ||
        info->kernel_entry == 0)
        return 0;

    for (size_t i = 0; i < sizeof(info->reserved) / sizeof(info->reserved[0]); ++i) {
        if (info->reserved[i] != 0)
            return 0;
    }

    if ((info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0) {
        if (info->framebuffer_base == 0 || info->framebuffer_size == 0 ||
            info->framebuffer_width == 0 || info->framebuffer_height == 0 ||
            info->framebuffer_stride < info->framebuffer_width)
            return 0;
    } else if (info->framebuffer_base != 0 || info->framebuffer_size != 0 ||
               info->framebuffer_width != 0 || info->framebuffer_height != 0 ||
               info->framebuffer_stride != 0 || info->framebuffer_format != 0 ||
               info->red_mask != 0 || info->green_mask != 0 ||
               info->blue_mask != 0 || info->reserved_mask != 0) {
        return 0;
    }

    if ((info->flags & VGK_BOOT_FLAG_ACPI) != 0) {
        if (info->acpi_rsdp_address == 0)
            return 0;
    } else if (info->acpi_rsdp_address != 0) {
        return 0;
    }

    entry_count = info->memory_map_size / sizeof(VGK_MEMORY_MAP_ENTRY);
    if (entry_count == 0)
        return 0;

    return 1;
}

_Static_assert(sizeof(VGK_MEMORY_MAP_ENTRY) == 32,
               "VGK memory map entry layout changed");
_Static_assert(offsetof(VGK_BOOT_INFO, flags) == 16,
               "VGK boot info flags offset changed");
_Static_assert(offsetof(VGK_BOOT_INFO, memory_map_address) == 40,
               "VGK boot info memory map offset changed");
_Static_assert(offsetof(VGK_BOOT_INFO, kernel_physical_base) == 64,
               "VGK boot info kernel offset changed");
_Static_assert(offsetof(VGK_BOOT_INFO, framebuffer_base) == 88,
               "VGK boot info framebuffer offset changed");
_Static_assert(offsetof(VGK_BOOT_INFO, acpi_rsdp_address) == 136,
               "VGK boot info ACPI offset changed");
_Static_assert(sizeof(VGK_BOOT_INFO) == 176,
               "VGK boot info size changed");

#endif
