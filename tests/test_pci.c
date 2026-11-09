#include <stdio.h>
#include <string.h>
#include "uefi_pci.h"

static unsigned failures;
#define CHECK(x, m) do { if (!(x)) { puts("FAIL: " m); ++failures; } } while (0)

static VGK_EFI_STATUS VGK_EFIAPI mock_read(
    VGK_EFI_PCI_IO_PROTOCOL *self, uint32_t width, uint32_t offset,
    VGK_EFI_UINTN count, void *buffer)
{
    uint32_t *words = (uint32_t *)buffer;
    (void)self;
    if (width != 2 || offset != 0 || count != 16 || buffer == 0)
        return UINT64_C(0x8000000000000002);
    memset(words, 0, 16 * sizeof(*words));
    words[0] = UINT32_C(0x12348086);
    words[2] = UINT32_C(0x03000002);
    words[3] = UINT32_C(0x00010000);
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_location(
    VGK_EFI_PCI_IO_PROTOCOL *self, VGK_EFI_UINTN *segment,
    VGK_EFI_UINTN *bus, VGK_EFI_UINTN *device, VGK_EFI_UINTN *function)
{
    (void)self;
    *segment = 2; *bus = 3; *device = 4; *function = 1;
    return 0;
}

static VGK_PCI_DEVICE sample(uint8_t class_code, uint8_t subclass)
{
    VGK_PCI_DEVICE d;
    memset(&d, 0, sizeof(d));
    d.device = 2; d.vendor_id = 0x8086; d.device_id = 0x1234;
    d.class_code = class_code; d.subclass = subclass;
    return d;
}

int main(void)
{
    VGK_EFI_PCI_IO_PROTOCOL protocol;
    VGK_PCI_DEVICE d;
    memset(&protocol, 0, sizeof(protocol));
    protocol.pci.read = mock_read;
    protocol.get_location = mock_location;

    CHECK(vgk_uefi_pci_read_identity(&protocol, &d) == 0,
          "UEFI PCI identity read failed");
    CHECK(d.segment == 2 && d.bus == 3 && d.device == 4 && d.function == 1,
          "PCI location was not preserved");
    CHECK(d.vendor_id == 0x8086 && d.device_id == 0x1234,
          "vendor/device IDs decoded incorrectly");
    CHECK(d.class_code == 0x03 && d.subclass == 0x00 &&
          d.programming_interface == 0x00 && d.revision == 2,
          "PCI class header decoded incorrectly");
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_DISPLAY,
          "display controller classification failed");

    d = sample(0x02, 0x00);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_NETWORK, "network classification failed");
    d = sample(0x01, 0x08);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_STORAGE, "storage classification failed");
    d = sample(0x04, 0x01);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_MULTIMEDIA, "multimedia classification failed");
    d = sample(0x06, 0x04);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_BRIDGE, "bridge classification failed");
    d = sample(0x0C, 0x03);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_USB, "USB classification failed");
    d.subclass = 0x05;
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_SERIAL_BUS, "serial bus classification failed");

    d = sample(0x03, 0x00); d.device = 32;
    CHECK(!vgk_pci_device_is_valid(&d), "device number 32 accepted");
    d = sample(0x03, 0x00); d.function = 8;
    CHECK(!vgk_pci_device_is_valid(&d), "function number 8 accepted");
    d = sample(0x03, 0x00); d.vendor_id = 0xFFFF;
    CHECK(!vgk_pci_device_is_valid(&d), "absent PCI function accepted");
    CHECK(!vgk_pci_device_is_valid(0), "null PCI record accepted");
    CHECK(vgk_uefi_pci_read_identity(0, &d) != 0, "null protocol accepted");
    CHECK(vgk_uefi_pci_read_identity(&protocol, 0) != 0, "null output accepted");

    if (failures != 0) return 1;
    puts("OK: UEFI PCI configuration read, BDF and identity decoding");
    return 0;
}
