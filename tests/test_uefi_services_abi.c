#include <stddef.h>
#include "uefi_services.h"

int main(void)
{
    return
        offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, allocate_pages) == 40 &&
        offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, get_memory_map) == 56 &&
        offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, handle_protocol) == 152 &&
        offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, exit_boot_services) == 232 &&
        offsetof(VGK_EFI_LOADED_IMAGE_PROTOCOL, device_handle) == 24 &&
        sizeof(VGK_EFI_MEMORY_DESCRIPTOR) == 40 &&
        sizeof(VGK_EFI_GUID) == 16 ? 0 : 1;
}
