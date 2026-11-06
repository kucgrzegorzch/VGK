#include <stddef.h>
#include <stdint.h>

#include "uefi_file.h"
#include "uefi_services.h"
#include "uefi_release_verify.h"

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

static const VGK_EFI_GUID loaded_image_guid = {
    UINT32_C(0x5B1B31A1), UINT16_C(0x9562), UINT16_C(0x11D2),
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const VGK_EFI_GUID simple_file_system_guid = {
    UINT32_C(0x964E5B22), UINT16_C(0x6459), UINT16_C(0x11D2),
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* Static storage avoids placing a 64 KiB manifest on the firmware stack. */
static uint8_t manifest_buffer[VGK_MANIFEST_MAX_SIZE];
static uint8_t signature_buffer[VGK_RELEASE_SIGNATURE_SIZE];

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
        'J','a','d','r','o',' ','n','i','e',' ','j','e','s','z','c','z','e',' ','j','e','s','t',' ',
        'u','r','u','c','h','a','m','i','a','n','e','.','\r','\n',0
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
        (void)manifest_size;
        (void)root->close(root);
        if (result != VGK_MANIFEST_OK) {
            print_text(system_table, verify_error);
            return EFI_SECURITY_VIOLATION;
        }
        print_text(system_table, verified);
        return EFI_SUCCESS;
    }
#else
    (void)root->close(root);
    print_text(system_table, no_key);
    return EFI_SECURITY_VIOLATION;
#endif
}
