#include <stdio.h>
#include <string.h>
#include "uefi_pci.h"

static unsigned failures;
#define CHECK(x, m) do { if (!(x)) { puts("FAIL: " m); ++failures; } } while (0)

static VGK_EFI_PCI_IO_PROTOCOL protocols[2];
static VGK_EFI_HANDLE handles[2] = {(void *)1, (void *)2};
static uint32_t free_pool_calls;
static int fail_free_pool;

static VGK_EFI_STATUS VGK_EFIAPI mock_read(
    VGK_EFI_PCI_IO_PROTOCOL *p, uint32_t width, uint32_t offset,
    VGK_EFI_UINTN count, void *buffer)
{
    uint32_t *words = (uint32_t *)buffer;
    unsigned index = (p == &protocols[1]) ? 1U : 0U;
    if (width != 2U || offset != 0U || count != 16U || buffer == 0)
        return UINT64_C(0x8000000000000002);
    memset(words, 0, 16U * sizeof(*words));
    words[0] = index == 0U ? UINT32_C(0x12348086) : UINT32_C(0x567810EC);
    words[2] = index == 0U ? UINT32_C(0x03000002) : UINT32_C(0x02000001);
    words[3] = UINT32_C(0x00010000);
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_location(
    VGK_EFI_PCI_IO_PROTOCOL *p, VGK_EFI_UINTN *segment,
    VGK_EFI_UINTN *bus, VGK_EFI_UINTN *device, VGK_EFI_UINTN *function)
{
    unsigned index = (p == &protocols[1]) ? 1U : 0U;
    *segment = index;
    *bus = 3U + index;
    *device = 4U + index;
    *function = 1U;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_locate(
    uint32_t search_type, const VGK_EFI_GUID *guid, void *key,
    VGK_EFI_UINTN *count, VGK_EFI_HANDLE **result)
{
    (void)guid;
    if (search_type != VGK_EFI_LOCATE_BY_PROTOCOL || key != 0)
        return UINT64_C(0x8000000000000002);
    *count = 2U;
    *result = handles;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_handle_protocol(
    VGK_EFI_HANDLE handle, const VGK_EFI_GUID *guid, void **interface)
{
    (void)guid;
    if (handle == handles[0])
        *interface = &protocols[0];
    else if (handle == handles[1])
        *interface = &protocols[1];
    else
        return UINT64_C(0x8000000000000002);
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_free_pool(void *buffer)
{
    if (buffer != handles)
        return UINT64_C(0x8000000000000002);
    ++free_pool_calls;
    return fail_free_pool ? UINT64_C(0x8000000000000002) : 0;
}

int main(void)
{
    VGK_EFI_PCI_IO_PROTOCOL p;
    VGK_PCI_DEVICE device;
    VGK_PCI_DEVICE inventory[2];
    VGK_PCI_DEVICE one_slot[1];
    VGK_EFI_BOOT_SERVICES_PREFIX services;
    size_t count = 0;

    memset(&p, 0, sizeof(p));
    p.pci.read = mock_read;
    p.get_location = mock_location;
    CHECK(vgk_uefi_pci_read_identity(&p, &device) == 0,
          "UEFI PCI identity read failed");
    CHECK(device.segment == 0U && device.bus == 3U &&
          device.device == 4U && device.function == 1U,
          "BDF decode failed");
    CHECK(device.vendor_id == 0x8086U && device.device_id == 0x1234U,
          "vendor/device decode failed");
    CHECK(device.class_code == 3U && device.subclass == 0U &&
          device.revision == 2U, "class/revision decode failed");
    CHECK(vgk_pci_classify(&device) == VGK_PCI_CLASS_DISPLAY,
          "display class failed");

    memset(protocols, 0, sizeof(protocols));
    protocols[0].pci.read = mock_read;
    protocols[0].get_location = mock_location;
    protocols[1].pci.read = mock_read;
    protocols[1].get_location = mock_location;
    memset(&services, 0, sizeof(services));
    services.locate_handle_buffer = mock_locate;
    services.handle_protocol = mock_handle_protocol;
    services.free_pool = mock_free_pool;

    CHECK(vgk_uefi_pci_enumerate(&services, inventory, 2U, &count) == 0,
          "two-device enumeration failed");
    CHECK(count == 2U, "enumeration returned wrong device count");
    CHECK(inventory[0].vendor_id == 0x8086U &&
          inventory[1].vendor_id == 0x10ECU,
          "enumeration lost device identities");
    CHECK(inventory[1].segment == 1U && inventory[1].bus == 4U,
          "enumeration lost second device location");
    CHECK(free_pool_calls == 1U, "firmware handle buffer was not freed");

    count = 99U;
    CHECK(vgk_uefi_pci_enumerate(&services, one_slot, 1U, &count) == -2,
          "undersized inventory was not rejected");
    CHECK(count == 1U, "partial count on capacity error is incorrect");
    CHECK(free_pool_calls == 2U, "handle buffer leaked on capacity error");

    count = 99U;
    CHECK(vgk_uefi_pci_enumerate(&services, 0, 0U, &count) == -2,
          "zero-capacity inventory was not rejected");
    CHECK(count == 0U, "zero-capacity result count is incorrect");
    CHECK(free_pool_calls == 3U, "handle buffer leaked on zero capacity");

    fail_free_pool = 1;
    count = 99U;
    CHECK(vgk_uefi_pci_enumerate(&services, inventory, 2U, &count) == -1,
          "FreePool failure was not propagated");
    CHECK(free_pool_calls == 4U, "FreePool failure path was not exercised");
    CHECK(count == 0U && inventory[0].vendor_id == 0U,
          "firmware error exposed a partial PCI inventory");

    if (failures != 0U)
        return 1;
    puts("OK: UEFI PCI identity decoding and bounded enumeration");
    return 0;
}
