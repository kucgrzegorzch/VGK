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
