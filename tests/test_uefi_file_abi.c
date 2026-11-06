#include "uefi_file.h"

#include <stdio.h>

int main(void)
{
    /* Compile-time assertions in the header validate the firmware ABI layout. */
    if (VGK_EFI_FILE_MODE_READ != UINT64_C(1) ||
        VGK_EFI_FILE_MODE_WRITE != UINT64_C(2) ||
        VGK_EFI_FILE_MODE_CREATE != UINT64_C(0x8000000000000000)) {
        fputs("UEFI file mode constants: FAIL\n", stderr);
        return 1;
    }

    puts("UEFI file protocol ABI layout: OK");
    return 0;
}
