#include <stddef.h>
#include <stdint.h>

#include "uefi_file.h"
#include "uefi_services.h"

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#error "VGK UEFI Stage 0 requires x86-64"
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
#define EFI_ERROR(status) (((status) & UINT64_C(0x8000000000000000)) != 0)

static const VGK_EFI_GUID loaded_image_guid = {
    UINT32_C(0x5B1B31A1), UINT16_C(0x9562), UINT16_C(0x11D2),
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const VGK_EFI_GUID simple_file_system_guid = {
    UINT32_C(0x964E5B22), UINT16_C(0x6459), UINT16_C(0x11D2),
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static void print_text(EFI_SYSTEM_TABLE *system_table, CHAR16 *message)
{
    if (system_table != 0 && system_table->con_out != 0 &&
        system_table->con_out->output_string != 0) {
        (void)system_table->con_out->output_string(system_table->con_out, message);
    }
}

/*
 * Read two bytes from the EFI application on the same firmware volume.
 * This is a deliberately small first proof of real file access. It does not
 * yet load or execute the VGK kernel and does not authenticate the file.
 */
static EFI_STATUS check_boot_volume(EFI_SYSTEM_TABLE *system_table,
                                    VGK_EFI_HANDLE image_handle)
{
    VGK_EFI_LOADED_IMAGE_PROTOCOL *loaded_image = 0;
    VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *file_system = 0;
    VGK_EFI_FILE_PROTOCOL *root = 0;
    VGK_EFI_FILE_PROTOCOL *image_file = 0;
    EFI_STATUS status;
    uint8_t signature[2] = {0, 0};
    VGK_EFI_UINTN signature_size = sizeof(signature);
    static VGK_EFI_CHAR16 self_path[] = {
        '\\', 'E','F','I','\\','B','O','O','T','\\',
        'B','O','O','T','X','6','4','.','E','F','I', 0
    };

    if (system_table == 0 || system_table->boot_services == 0 ||
        system_table->boot_services->handle_protocol == 0 ||
        image_handle == 0) {
        return EFI_INVALID_PARAMETER;
    }

    status = system_table->boot_services->handle_protocol(
        image_handle, &loaded_image_guid, (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (loaded_image == 0 || loaded_image->device_handle == 0) {
        return EFI_LOAD_ERROR;
    }

    status = system_table->boot_services->handle_protocol(
        loaded_image->device_handle, &simple_file_system_guid,
        (void **)&file_system);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (file_system == 0 || file_system->open_volume == 0) {
        return EFI_LOAD_ERROR;
    }

    status = file_system->open_volume(file_system, &root);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (root == 0 || root->open == 0 || root->close == 0) {
        if (root != 0 && root->close != 0) {
            (void)root->close(root);
        }
        return EFI_LOAD_ERROR;
    }

    status = root->open(root, &image_file, self_path,
                        VGK_EFI_FILE_MODE_READ, 0);
    (void)root->close(root);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (image_file == 0 || image_file->read == 0 || image_file->close == 0) {
        if (image_file != 0 && image_file->close != 0) {
            (void)image_file->close(image_file);
        }
        return EFI_LOAD_ERROR;
    }

    status = image_file->read(image_file, &signature_size, signature);
    (void)image_file->close(image_file);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (signature_size != sizeof(signature) ||
        signature[0] != 'M' || signature[1] != 'Z') {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(void *image_handle, EFI_SYSTEM_TABLE *system_table)
{
    static CHAR16 banner[] = {
        'V','G','K',' ','w','l','a','s','n','y',' ','m','e','c','h','a','n','i','z','m',
        ' ','u','r','u','c','h','a','m','i','a','n','i','a','\r','\n',
        'P','o','d','s','t','a','w','o','w','e',' ','w','e','j','s','c','i','e',' ',
        'U','E','F','I',' ','d','z','i','a','l','a','.','\r','\n',0
    };
    static CHAR16 volume_ok[] = {
        'O','d','c','z','y','t',' ','p','l','i','k','u',' ','z',' ','n','o','s','n','i','k','a',
        ' ','p','o','w','i','o','d','l',' ','s','i','e','.','\r','\n',0
    };
    static CHAR16 volume_error[] = {
        'N','i','e',' ','u','d','a','l','o',' ','o','d','c','z','y','t','a','c',' ',
        'p','l','i','k','u',' ','z',' ','n','o','s','n','i','k','a','.','\r','\n',0
    };

    if (system_table == 0 || system_table->con_out == 0 ||
        system_table->con_out->output_string == 0) {
        return EFI_INVALID_PARAMETER;
    }

    print_text(system_table, banner);
    EFI_STATUS status = check_boot_volume(system_table, image_handle);
    if (EFI_ERROR(status)) {
        print_text(system_table, volume_error);
        return status;
    }

    print_text(system_table, volume_ok);
    return EFI_SUCCESS;
}
