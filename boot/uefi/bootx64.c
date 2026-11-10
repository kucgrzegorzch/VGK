#include <stddef.h>
#include <stdint.h>

#include "uefi_file.h"
#include "uefi_services.h"
#include "uefi_graphics.h"
#include "vgk_framebuffer.h"
#include "uefi_release_verify.h"
#include "uefi_file_reader.h"
#include "elf_loader.h"
#include "uefi_elf_allocator.h"
#include "uefi_memory_map.h"
#include "uefi_pci.h"
#include "uefi_kernel_entry.h"
#include "integrity.h"

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#error "VGK UEFI Stage 0 requires x86-64"
#endif

#if defined(__has_include)
#  if __has_include("vgk_trusted_public_key.h")
#    include "vgk_trusted_public_key.h"
#    define VGK_HAS_TRUSTED_PUBLIC_KEY 1
#  endif
#endif
#ifndef VGK_HAS_TRUSTED_PUBLIC_KEY
#define VGK_HAS_TRUSTED_PUBLIC_KEY 0
#endif

typedef uint16_t CHAR16;
typedef uint64_t EFI_STATUS;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, uint8_t);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_OUTPUT_STRING)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, CHAR16 *);

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET reset;
    EFI_TEXT_OUTPUT_STRING output_string;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER header;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t padding;
    void *console_in_handle;
    void *con_in;
    void *console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
    void *standard_error_handle;
    void *std_err;
    void *runtime_services;
    VGK_EFI_BOOT_SERVICES_PREFIX *boot_services;
    uint64_t number_of_table_entries;
    void *configuration_table;
} EFI_SYSTEM_TABLE;

_Static_assert(sizeof(CHAR16) == 2, "UEFI CHAR16 must be 16 bits");
_Static_assert(sizeof(EFI_TABLE_HEADER) == 24, "unexpected EFI table header size");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, con_out) == 64,
               "unexpected EFI_SYSTEM_TABLE con_out offset");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, boot_services) == 96,
               "unexpected EFI_SYSTEM_TABLE boot_services offset");
_Static_assert(sizeof(EFI_SYSTEM_TABLE) == 120,
               "unexpected EFI_SYSTEM_TABLE size");

#define EFI_SUCCESS UINT64_C(0)
#define EFI_INVALID_PARAMETER UINT64_C(0x8000000000000002)
#define EFI_LOAD_ERROR UINT64_C(0x8000000000000001)
#define EFI_SECURITY_VIOLATION UINT64_C(0x800000000000001A)
#define EFI_ERROR(status) (((status) & UINT64_C(0x8000000000000000)) != 0)

#define VGK_KERNEL_FILE_MAX_SIZE (UINT64_C(64) * 1024U * 1024U)
#define VGK_UEFI_PAGE_SIZE UINT64_C(4096)
#define VGK_MEMORY_MAP_BUFFER_SIZE (512U * 1024U)
#define VGK_MEMORY_MAP_ENTRY_CAPACITY 16384U
#define VGK_EXIT_BOOT_SERVICES_ATTEMPTS 4U

static const VGK_EFI_GUID loaded_image_guid = {
    UINT32_C(0x5B1B31A1), UINT16_C(0x9562), UINT16_C(0x11D2),
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const VGK_EFI_GUID simple_file_system_guid = {
    UINT32_C(0x964E5B22), UINT16_C(0x6459), UINT16_C(0x11D2),
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* Fixed storage is reserved before GetMemoryMap/ExitBootServices. */
#if VGK_HAS_TRUSTED_PUBLIC_KEY
static const VGK_EFI_GUID graphics_output_protocol_guid = {
    UINT32_C(0x9042A9DE), UINT16_C(0x23DC), UINT16_C(0x4A38),
    {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}
};

static uint8_t manifest_buffer[VGK_MANIFEST_MAX_SIZE];
static uint8_t signature_buffer[VGK_RELEASE_SIGNATURE_SIZE];
static uint8_t firmware_memory_map[VGK_MEMORY_MAP_BUFFER_SIZE]
    __attribute__((aligned(16)));
static VGK_MEMORY_MAP_ENTRY vgk_memory_map[VGK_MEMORY_MAP_ENTRY_CAPACITY]
    __attribute__((aligned(16)));
static VGK_BOOT_INFO boot_info;
#endif

static void print_text(EFI_SYSTEM_TABLE *system_table, CHAR16 *message)
{
    if (system_table != 0 && system_table->con_out != 0 &&
        system_table->con_out->output_string != 0) {
        (void)system_table->con_out->output_string(system_table->con_out, message);
    }
}

static EFI_STATUS open_boot_volume(
    EFI_SYSTEM_TABLE *system_table,
    VGK_EFI_HANDLE image_handle,
    VGK_EFI_FILE_PROTOCOL **root_out)
{
    VGK_EFI_LOADED_IMAGE_PROTOCOL *loaded_image = 0;
    VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *file_system = 0;
    VGK_EFI_FILE_PROTOCOL *root = 0;
    EFI_STATUS status;

    if (root_out == 0)
        return EFI_INVALID_PARAMETER;
    *root_out = 0;

    if (system_table == 0 || system_table->boot_services == 0 ||
        system_table->boot_services->handle_protocol == 0 ||
        image_handle == 0)
        return EFI_INVALID_PARAMETER;

    status = system_table->boot_services->handle_protocol(
        image_handle, &loaded_image_guid, (void **)&loaded_image);
    if (EFI_ERROR(status))
        return status;
    if (loaded_image == 0 || loaded_image->device_handle == 0)
        return EFI_LOAD_ERROR;

    status = system_table->boot_services->handle_protocol(
        loaded_image->device_handle, &simple_file_system_guid,
        (void **)&file_system);
    if (EFI_ERROR(status))
        return status;
    if (file_system == 0 || file_system->open_volume == 0)
        return EFI_LOAD_ERROR;

    status = file_system->open_volume(file_system, &root);
    if (EFI_ERROR(status))
        return status;
    if (root == 0 || root->open == 0 || root->close == 0) {
        if (root != 0 && root->close != 0)
            (void)root->close(root);
        return EFI_LOAD_ERROR;
    }

    *root_out = root;
    return EFI_SUCCESS;
}

#if VGK_HAS_TRUSTED_PUBLIC_KEY
static int path_is_kernel(const VGK_MANIFEST_ENTRY *entry)
{
    static const uint8_t expected[] = "boot/kernel.elf";
    if (entry == 0 || entry->path == 0 ||
        entry->path_length != sizeof(expected) - 1U)
        return 0;
    for (size_t i = 0; i < sizeof(expected) - 1U; ++i) {
        if (entry->path[i] != expected[i])
            return 0;
    }
    return 1;
}

static int find_kernel_entry(
    const uint8_t *manifest_bytes,
    size_t manifest_size,
    const uint8_t public_key[32],
    const uint8_t signature[VGK_RELEASE_SIGNATURE_SIZE],
    VGK_MANIFEST_ENTRY *kernel_entry)
{
    VGK_MANIFEST manifest;
    VGK_MANIFEST_RESULT result;

    result = vgk_manifest_open(&manifest, manifest_bytes, manifest_size,
                               public_key, signature);
    if (result != VGK_MANIFEST_OK)
        return -1;

    for (uint16_t i = 0; i < manifest.entry_count; ++i) {
        VGK_MANIFEST_ENTRY entry;
        if (vgk_manifest_get(&manifest, i, &entry) != VGK_MANIFEST_OK)
            return -1;
        if (path_is_kernel(&entry)) {
            *kernel_entry = entry;
            return 0;
        }
    }
    return -1;
}


/*
 * Capture the firmware-selected GOP framebuffer while Boot Services are
 * available. Unsupported pixel layouts leave the flag clear so the kernel
 * can use its non-graphical fallback.
 */
static void capture_framebuffer(EFI_SYSTEM_TABLE *system_table)
{
    VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics = 0;
    VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
    VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    uint32_t normalized_format;
    EFI_STATUS status;

    if (system_table == 0 || system_table->boot_services == 0 ||
        system_table->boot_services->handle_protocol == 0 ||
        system_table->console_out_handle == 0)
        return;

    status = system_table->boot_services->handle_protocol(
        system_table->console_out_handle,
        &graphics_output_protocol_guid, (void **)&graphics);
    if (EFI_ERROR(status) || graphics == 0 || graphics->mode == 0)
        return;

    mode = graphics->mode;
    info = mode->info;
    if (info == 0 ||
        mode->size_of_info < sizeof(VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION) ||
        info->version != 0)
        return;

    if (info->pixel_format == VGK_EFI_GRAPHICS_PIXEL_RGB_RESERVED_8) {
        normalized_format = VGK_FRAMEBUFFER_FORMAT_RGBX8;
    } else if (info->pixel_format == VGK_EFI_GRAPHICS_PIXEL_BGR_RESERVED_8) {
        normalized_format = VGK_FRAMEBUFFER_FORMAT_BGRX8;
    } else {
        /* Bit-mask and BLT-only modes need separate, explicit support. */
        return;
    }

    if (!vgk_framebuffer_is_valid(
            mode->framebuffer_base, mode->framebuffer_size,
            info->horizontal_resolution, info->vertical_resolution,
            info->pixels_per_scan_line, normalized_format))
        return;

    boot_info.framebuffer_base = mode->framebuffer_base;
    boot_info.framebuffer_size = mode->framebuffer_size;
    boot_info.framebuffer_width = info->horizontal_resolution;
    boot_info.framebuffer_height = info->vertical_resolution;
    boot_info.framebuffer_stride = info->pixels_per_scan_line;
    boot_info.framebuffer_format = normalized_format;
    boot_info.red_mask = 0;
    boot_info.green_mask = 0;
    boot_info.blue_mask = 0;
    boot_info.reserved_mask = 0;
    boot_info.flags |= VGK_BOOT_FLAG_FRAMEBUFFER;
}

static void release_loaded_kernel(
    VGK_UEFI_ELF_ALLOCATOR *allocator,
    const VGK_ELF_LOADED_IMAGE *loaded)
{
    if (allocator == 0 || loaded == 0)
        return;
    for (uint32_t i = 0; i < loaded->range_count; ++i) {
        vgk_uefi_elf_release(allocator,
            loaded->ranges[i].physical_address,
            loaded->ranges[i].page_count);
    }
}

/*
 * ExitBootServices may partially shut down firmware even when it fails.
 * After the first attempt, do not free pages, close protocols, print, or
 * return to firmware. A failed retry path must stop without further services.
 */
static _Noreturn void halt_after_exit_boot_services_failure(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static EFI_STATUS start_verified_kernel(
    EFI_SYSTEM_TABLE *system_table,
    VGK_EFI_HANDLE image_handle,
    VGK_EFI_FILE_PROTOCOL *root,
    size_t manifest_size,
    const uint8_t trusted_public_key[32])
{
    VGK_EFI_BOOT_SERVICES_PREFIX *services = system_table->boot_services;
    VGK_MANIFEST_ENTRY kernel_file;
    VGK_ELF_LOAD_PLAN load_plan;
    VGK_ELF_LOADED_IMAGE loaded_image;
    VGK_UEFI_ELF_ALLOCATOR allocator;
    uint64_t kernel_buffer_address = 0;
    uint64_t page_table_pool_address = 0;
    uint64_t kernel_buffer_pages;
    size_t kernel_bytes_read = 0;
    uint64_t kernel_min = UINT64_MAX;
    uint64_t kernel_max = 0;
    VGK_EFI_UINTN memory_map_size;
    VGK_EFI_UINTN map_key = 0;
    VGK_EFI_UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;
    size_t normalized_count = 0;
    size_t pci_device_count = 0;
    EFI_STATUS status;

    if (services == 0 || services->allocate_pages == 0 ||
        services->free_pages == 0 || services->get_memory_map == 0 ||
        services->exit_boot_services == 0 ||
        find_kernel_entry(manifest_buffer, manifest_size,
                          trusted_public_key, signature_buffer,
                          &kernel_file) != 0 ||
        kernel_file.file_size == 0 ||
        kernel_file.file_size > VGK_KERNEL_FILE_MAX_SIZE ||
        kernel_file.file_size > (uint64_t)SIZE_MAX)
        return EFI_LOAD_ERROR;

    kernel_buffer_pages =
        (kernel_file.file_size + VGK_UEFI_PAGE_SIZE - 1U) /
        VGK_UEFI_PAGE_SIZE;
    if (kernel_buffer_pages == 0 ||
        kernel_buffer_pages > UINT64_MAX / VGK_UEFI_PAGE_SIZE)
        return EFI_LOAD_ERROR;

    status = services->allocate_pages(
        VGK_EFI_ALLOCATE_ANY_PAGES, VGK_EFI_LOADER_DATA,
        (VGK_EFI_UINTN)kernel_buffer_pages, &kernel_buffer_address);
    if (EFI_ERROR(status) || kernel_buffer_address == 0)
        return EFI_LOAD_ERROR;

    if (vgk_uefi_read_file_bounded(
            root, kernel_file.path, kernel_file.path_length,
            (uint8_t *)(uintptr_t)kernel_buffer_address,
            (size_t)kernel_file.file_size, &kernel_bytes_read) != 0 ||
        kernel_bytes_read != (size_t)kernel_file.file_size ||
        vgk_verify_buffer_sha256(
            (const void *)(uintptr_t)kernel_buffer_address,
            kernel_bytes_read, kernel_file.sha256) != VGK_INTEGRITY_OK) {
        (void)services->free_pages(kernel_buffer_address,
                                   (VGK_EFI_UINTN)kernel_buffer_pages);
        return EFI_SECURITY_VIOLATION;
    }

    if (vgk_elf64_make_load_plan(
            (const uint8_t *)(uintptr_t)kernel_buffer_address,
            kernel_bytes_read, &load_plan) != VGK_ELF_OK) {
        (void)services->free_pages(kernel_buffer_address,
                                   (VGK_EFI_UINTN)kernel_buffer_pages);
        return EFI_LOAD_ERROR;
    }

    allocator.allocate_pages = services->allocate_pages;
    allocator.free_pages = services->free_pages;
    if (vgk_elf64_load_image(
            (const uint8_t *)(uintptr_t)kernel_buffer_address,
            kernel_bytes_read, &load_plan,
            vgk_uefi_elf_allocate, vgk_uefi_elf_release,
            &allocator, &loaded_image) != VGK_ELF_OK) {
        (void)services->free_pages(kernel_buffer_address,
                                   (VGK_EFI_UINTN)kernel_buffer_pages);
        return EFI_LOAD_ERROR;
    }

    (void)services->free_pages(kernel_buffer_address,
                               (VGK_EFI_UINTN)kernel_buffer_pages);

    for (uint32_t i = 0; i < load_plan.segment_count; ++i) {
        const VGK_ELF_LOAD_SEGMENT *segment = &load_plan.segments[i];
        uint64_t end = segment->physical_address + segment->memory_size;
        if (segment->physical_address < kernel_min)
            kernel_min = segment->physical_address;
        if (end > kernel_max)
            kernel_max = end;
    }
    if (kernel_min == UINT64_MAX || kernel_max <= kernel_min) {
        release_loaded_kernel(&allocator, &loaded_image);
        return EFI_LOAD_ERROR;
    }

    /*
     * Reserve the page-table pool while Boot Services are available.
     * This allocation must precede the final GetMemoryMap/ExitBootServices
     * sequence; the pool remains loader-owned and is never freed on success.
     */
    status = services->allocate_pages(
        VGK_EFI_ALLOCATE_ANY_PAGES, VGK_EFI_LOADER_DATA,
        (VGK_EFI_UINTN)VGK_BOOT_PAGE_TABLE_POOL_PAGES,
        &page_table_pool_address);
    if (EFI_ERROR(status) || page_table_pool_address == 0) {
        release_loaded_kernel(&allocator, &loaded_image);
        return EFI_LOAD_ERROR;
    }
    {
        volatile uint64_t *pool_words =
            (volatile uint64_t *)(uintptr_t)page_table_pool_address;
        const uint64_t word_count =
            VGK_BOOT_PAGE_TABLE_POOL_PAGES *
            (VGK_BOOT_PAGE_SIZE / sizeof(uint64_t));
        for (uint64_t i = 0; i < word_count; ++i)
            pool_words[i] = 0;
    }

    /* Close the volume before taking the final firmware memory map. */
    if (root != 0 && root->close != 0)
        (void)root->close(root);

    boot_info.magic = VGK_BOOT_INFO_MAGIC;
    boot_info.version = VGK_BOOT_INFO_VERSION;
    boot_info.structure_size = sizeof(boot_info);
    boot_info.flags = 0;
    boot_info.firmware_type = VGK_FIRMWARE_UEFI;
    boot_info.memory_map_address = (uint64_t)(uintptr_t)vgk_memory_map;
    boot_info.memory_map_entry_size = sizeof(VGK_MEMORY_MAP_ENTRY);
    boot_info.kernel_physical_base = kernel_min;
    boot_info.kernel_size = kernel_max - kernel_min;
    boot_info.kernel_entry = load_plan.entry;
    boot_info.page_table_pool_base = page_table_pool_address;
    boot_info.page_table_pool_pages = VGK_BOOT_PAGE_TABLE_POOL_PAGES;
    boot_info.kernel_segment_count = load_plan.segment_count;
    boot_info.reserved2 = 0;
    for (uint32_t i = 0; i < load_plan.segment_count; ++i) {
        const VGK_ELF_LOAD_SEGMENT *source = &load_plan.segments[i];
        VGK_KERNEL_SEGMENT *destination = &boot_info.kernel_segments[i];
        destination->physical_base = source->physical_address;
        destination->memory_size = source->memory_size;
        destination->flags = source->flags;
        destination->reserved = 0;
    }

    capture_framebuffer(system_table);

    /*
     * Enumerate PCI while firmware services are still available. Only
     * normalized IDs are copied into the boot protocol; no UEFI handles or
     * protocol pointers survive ExitBootServices.
     */
    {
        int pci_status = vgk_uefi_pci_enumerate(
            services, boot_info.pci_devices,
            VGK_BOOT_MAX_PCI_DEVICES, &pci_device_count);
        if (pci_status == 0 || pci_status == -2) {
            boot_info.pci_device_count = (uint32_t)pci_device_count;
            boot_info.pci_device_entry_size = sizeof(VGK_PCI_DEVICE);
            boot_info.flags |= VGK_BOOT_FLAG_PCI;
            if (pci_status == -2)
                boot_info.flags |= VGK_BOOT_FLAG_PCI_TRUNCATED;
        }
        /* PCI inventory is diagnostic; failure must not block kernel boot. */
    }

    for (uint32_t attempt = 0;
         attempt < VGK_EXIT_BOOT_SERVICES_ATTEMPTS; ++attempt) {
        memory_map_size = sizeof(firmware_memory_map);
        status = services->get_memory_map(
            &memory_map_size, firmware_memory_map, &map_key,
            &descriptor_size, &descriptor_version);
        if (EFI_ERROR(status) ||
            memory_map_size > sizeof(firmware_memory_map) ||
            descriptor_size > SIZE_MAX ||
            vgk_uefi_normalize_memory_map(
                firmware_memory_map, (size_t)memory_map_size,
                (size_t)descriptor_size, descriptor_version,
                vgk_memory_map, VGK_MEMORY_MAP_ENTRY_CAPACITY,
                &normalized_count) != 0) {
            if (attempt != 0)
                halt_after_exit_boot_services_failure();
            (void)services->free_pages(
                page_table_pool_address,
                (VGK_EFI_UINTN)VGK_BOOT_PAGE_TABLE_POOL_PAGES);
            release_loaded_kernel(&allocator, &loaded_image);
            return EFI_LOAD_ERROR;
        }

        boot_info.memory_map_size =
            normalized_count * sizeof(VGK_MEMORY_MAP_ENTRY);
        if (!vgk_boot_info_is_valid(&boot_info)) {
            if (attempt != 0)
                halt_after_exit_boot_services_failure();
            (void)services->free_pages(
                page_table_pool_address,
                (VGK_EFI_UINTN)VGK_BOOT_PAGE_TABLE_POOL_PAGES);
            release_loaded_kernel(&allocator, &loaded_image);
            return EFI_LOAD_ERROR;
        }

        status = services->exit_boot_services(image_handle, map_key);
        if (status == EFI_SUCCESS) {
            vgk_uefi_enter_kernel(load_plan.entry, &boot_info);
        }
        if (status != EFI_INVALID_PARAMETER)
            halt_after_exit_boot_services_failure();
    }

    /* Boot Services are no longer safe to call after any failed attempt. */
    halt_after_exit_boot_services_failure();
}
#endif

EFI_STATUS EFIAPI efi_main(void *image_handle, EFI_SYSTEM_TABLE *system_table)
{
    static CHAR16 banner[] = {
        'V','G','K',' ','w','l','a','s','n','y',' ','m','e','c','h','a','n','i','z','m',
        ' ','u','r','u','c','h','a','m','i','a','n','i','a','\r','\n',
        'S','p','r','a','w','d','z','a','n','i','e',' ','z','a','u','f','a','n','i','a',
        ' ','p','r','z','e','d',' ','d','a','l','s','z','y','m',' ','s','t','a','r','t','e','m','.',
        '\r','\n',0
    };
#if VGK_HAS_TRUSTED_PUBLIC_KEY
    static CHAR16 verify_error[] = {
        'N','I','E','P','O','P','R','A','W','N','Y',' ','P','O','D','P','I','S',' ',
        'L','U','B',' ','P','L','I','K','I','.',' ','S','t','a','r','t',' ','p','r','z','e','r','w','a','n','y','.',
        '\r','\n',0
    };
    static CHAR16 verified[] = {
        'P','o','d','p','i','s',' ','i',' ','p','l','i','k','i',' ','w','y','d','a','n','i','a',
        ' ','s','p','r','a','w','d','z','o','n','e','.','\r','\n',
        'L','a','d','o','w','a','n','i','e',' ','i',' ','u','r','u','c','h','a','m','i','a','n','i','e',' ','j','a','d','r','a','.','\r','\n',0
    };
#else
    static CHAR16 no_key[] = {
        'B','R','A','K',' ','K','L','U','C','Z','A',' ','Z','A','U','F','A','N','I','A','.',
        ' ','S','t','a','r','t',' ','z','a','b','l','o','k','o','w','a','n','y','.',
        '\r','\n',0
    };
#endif

    VGK_EFI_FILE_PROTOCOL *root = 0;
    EFI_STATUS status;

    if (system_table == 0 || system_table->con_out == 0 ||
        system_table->con_out->output_string == 0)
        return EFI_INVALID_PARAMETER;

    print_text(system_table, banner);
    status = open_boot_volume(system_table, image_handle, &root);
    if (EFI_ERROR(status))
        return status;

#if VGK_HAS_TRUSTED_PUBLIC_KEY
    {
        static const uint8_t trusted_public_key[32] =
            VGK_TRUSTED_PUBLIC_KEY_INITIALIZER;
        size_t manifest_size = 0;
        VGK_MANIFEST_RESULT result = vgk_uefi_load_and_verify_release(
            root, manifest_buffer, sizeof(manifest_buffer), &manifest_size,
            signature_buffer, trusted_public_key);
        if (result != VGK_MANIFEST_OK) {
            (void)root->close(root);
            print_text(system_table, verify_error);
            return EFI_SECURITY_VIOLATION;
        }
        print_text(system_table, verified);
        status = start_verified_kernel(
            system_table, image_handle, root, manifest_size,
            trusted_public_key);
        /* start_verified_kernel closes root before ExitBootServices. */
        return status;
    }
#else
    (void)root->close(root);
    print_text(system_table, no_key);
    return EFI_SECURITY_VIOLATION;
#endif
}
