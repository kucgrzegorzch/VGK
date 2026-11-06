#include <stddef.h>
#include <stdint.h>

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
    void *boot_services;
    uint64_t number_of_table_entries;
    void *configuration_table;
} EFI_SYSTEM_TABLE;

/*
 * These checks protect the firmware ABI layout. A wrong offset here can make
 * the loader call an unrelated firmware pointer instead of OutputString.
 */
_Static_assert(sizeof(CHAR16) == 2, "UEFI CHAR16 must be 16 bits");
_Static_assert(sizeof(EFI_TABLE_HEADER) == 24, "unexpected EFI table header size");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, firmware_vendor) == 24,
               "unexpected EFI_SYSTEM_TABLE firmware_vendor offset");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, con_out) == 64,
               "unexpected EFI_SYSTEM_TABLE con_out offset");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, boot_services) == 96,
               "unexpected EFI_SYSTEM_TABLE boot_services offset");
_Static_assert(sizeof(EFI_SYSTEM_TABLE) == 120,
               "unexpected EFI_SYSTEM_TABLE size");
_Static_assert(offsetof(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL, output_string) == 8,
               "unexpected text-output protocol layout");

#define EFI_SUCCESS UINT64_C(0)
#define EFI_INVALID_PARAMETER UINT64_C(0x8000000000000002)

EFI_STATUS EFIAPI efi_main(void *image_handle, EFI_SYSTEM_TABLE *system_table)
{
    (void)image_handle;

    static CHAR16 banner[] = {
        'V','G','K',' ','N','a','t','i','v','e',' ','U','E','F','I',
        ' ','S','t','a','g','e',' ','0','\r','\n',
        'V','G','K',' ','f','i','r','m','w','a','r','e',' ',
        'e','n','t','r','y',' ','s','u','c','c','e','s','s','f','u','l','.',
        '\r','\n',0
    };

    if (system_table == 0 || system_table->con_out == 0 ||
        system_table->con_out->output_string == 0) {
        return EFI_INVALID_PARAMETER;
    }

    return system_table->con_out->output_string(system_table->con_out, banner);
}
