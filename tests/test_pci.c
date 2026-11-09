#include <stdio.h>
#include <string.h>
#include "vgk_pci.h"

static unsigned failures;

#define CHECK(expr, msg) do { \
    if (!(expr)) { puts("FAIL: " msg); ++failures; } \
} while (0)

static VGK_PCI_DEVICE sample(uint8_t class_code, uint8_t subclass)
{
    VGK_PCI_DEVICE d;
    memset(&d, 0, sizeof(d));
    d.bus = 0;
    d.device = 2;
    d.function = 0;
    d.vendor_id = 0x8086;
    d.device_id = 0x1234;
    d.class_code = class_code;
    d.subclass = subclass;
    return d;
}

int main(void)
{
    VGK_PCI_DEVICE d = sample(0x03, 0x00);

    CHECK(vgk_pci_device_is_valid(&d), "valid PCI function rejected");
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_DISPLAY,
          "display controller classification failed");

    d = sample(0x02, 0x00);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_NETWORK,
          "network controller classification failed");
    d = sample(0x01, 0x08);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_STORAGE,
          "storage controller classification failed");
    d = sample(0x04, 0x01);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_MULTIMEDIA,
          "multimedia controller classification failed");
    d = sample(0x06, 0x04);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_BRIDGE,
          "bridge classification failed");
    d = sample(0x0C, 0x03);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_USB,
          "USB controller classification failed");
    d.subclass = 0x05;
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_SERIAL_BUS,
          "other serial bus classification failed");

    d = sample(0x03, 0x00);
    d.device = 32;
    CHECK(!vgk_pci_device_is_valid(&d), "device number 32 accepted");
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_OTHER,
          "invalid BDF classified");
    d = sample(0x03, 0x00);
    d.function = 8;
    CHECK(!vgk_pci_device_is_valid(&d), "function number 8 accepted");
    d = sample(0x03, 0x00);
    d.vendor_id = 0xFFFF;
    CHECK(!vgk_pci_device_is_valid(&d), "absent PCI function accepted");
    CHECK(!vgk_pci_device_is_valid(0), "null PCI record accepted");

    if (failures != 0)
        return 1;
    puts("OK: PCI function identity validation and class decoding");
    return 0;
}
