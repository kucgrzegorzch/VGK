#ifndef VGK_UEFI_SERVICES_H
#define VGK_UEFI_SERVICES_H

#include <stddef.h>
#include <stdint.h>

#include "uefi_file.h"

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} VGK_EFI_GUID;

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_HANDLE_PROTOCOL)(
    VGK_EFI_HANDLE handle,
    const VGK_EFI_GUID *protocol,
    void **interface);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_ALLOCATE_PAGES)(
    uint32_t allocate_type,
    uint32_t memory_type,
    VGK_EFI_UINTN pages,
    uint64_t *memory);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FREE_PAGES)(
    uint64_t memory,
    VGK_EFI_UINTN pages);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_GET_MEMORY_MAP)(
    VGK_EFI_UINTN *memory_map_size,
    void *memory_map,
    VGK_EFI_UINTN *map_key,
    VGK_EFI_UINTN *descriptor_size,
    uint32_t *descriptor_version);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_EXIT_BOOT_SERVICES)(
    VGK_EFI_HANDLE image_handle,
    VGK_EFI_UINTN map_key);

typedef struct {
    uint32_t revision;
    uint32_t padding;
    VGK_EFI_HANDLE parent_handle;
    void *system_table;
    VGK_EFI_HANDLE device_handle;
} VGK_EFI_LOADED_IMAGE_PROTOCOL;

/*
 * UEFI Boot Services table prefix.
 * The first three 64-bit words are EFI_TABLE_HEADER.
 * Explicit slots preserve the firmware-defined offsets.
 */
typedef struct {
    uint64_t header[3];
    void *before_allocate_pages[2];
    VGK_EFI_ALLOCATE_PAGES allocate_pages;
    VGK_EFI_FREE_PAGES free_pages;
    VGK_EFI_GET_MEMORY_MAP get_memory_map;
    void *before_handle_protocol[11];
    VGK_EFI_HANDLE_PROTOCOL handle_protocol;
    void *before_exit_boot_services[9];
    VGK_EFI_EXIT_BOOT_SERVICES exit_boot_services;
} VGK_EFI_BOOT_SERVICES_PREFIX;

/* UEFI memory descriptor; firmware may report a larger stride. */
typedef struct {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} VGK_EFI_MEMORY_DESCRIPTOR;

enum {
    VGK_EFI_ALLOCATE_ANY_PAGES = 0,
    VGK_EFI_ALLOCATE_MAX_ADDRESS = 1,
    VGK_EFI_ALLOCATE_ADDRESS = 2
};

enum {
    VGK_EFI_LOADER_CODE = 1,
    VGK_EFI_LOADER_DATA = 2,
    VGK_EFI_BOOT_SERVICES_CODE = 3,
    VGK_EFI_BOOT_SERVICES_DATA = 4
};

_Static_assert(sizeof(VGK_EFI_GUID) == 16, "UEFI GUID must be 16 bytes");
_Static_assert(offsetof(VGK_EFI_LOADED_IMAGE_PROTOCOL, device_handle) == 24,
               "unexpected Loaded Image device handle offset");
_Static_assert(offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, allocate_pages) == 40,
               "unexpected Boot Services AllocatePages offset");
_Static_assert(offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, get_memory_map) == 56,
               "unexpected Boot Services GetMemoryMap offset");
_Static_assert(offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, handle_protocol) == 152,
               "unexpected Boot Services HandleProtocol offset");
_Static_assert(offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, exit_boot_services) == 232,
               "unexpected Boot Services ExitBootServices offset");
_Static_assert(sizeof(VGK_EFI_MEMORY_DESCRIPTOR) == 40,
               "unexpected UEFI memory descriptor size");

#endif
