#include <stddef.h>
#include "uefi_services.h"

int main(void)
{
    return offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, handle_protocol) == 152 &&
           offsetof(VGK_EFI_LOADED_IMAGE_PROTOCOL, device_handle) == 24 &&
           sizeof(VGK_EFI_GUID) == 16 ? 0 : 1;
}
