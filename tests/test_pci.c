#include <stdio.h>
#include <string.h>
#include "vgk_pci.h"

static unsigned failures;
#define CHECK(x, m) do { if (!(x)) { puts("FAIL: " m); ++failures; } } while (0)

static VGK_PCI_DEVICE sample(uint8_t class_code, uint8_t subclass)
{
    VGK_PCI_DEVICE d;
    memset(&d, 0, sizeof(d));
    d.device = 2U;
    d.vendor_id = UINT16_C(0x8086);
    d.device_id = UINT16_C(0x1234);
    d.class_code = class_code;
    d.subclass = subclass;
    return d;
}

int main(void)
{
    VGK_PCI_DEVICE d = sample(0x03U, 0x00U);

    CHECK(vgk_pci_device_is_valid(&d), "valid PCI identity rejected");
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_DISPLAY,
          "display classification failed");

    d = sample(0x02U, 0x00U);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_NETWORK,
          "network classification failed");
    d = sample(0x01U, 0x08U);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_STORAGE,
          "storage classification failed");
    d = sample(0x04U, 0x01U);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_MULTIMEDIA,
          "multimedia classification failed");
    d = sample(0x06U, 0x04U);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_BRIDGE,
          "bridge classification failed");
    d = sample(0x0CU, 0x03U);
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_USB,
          "USB classification failed");
    d.subclass = 0x05U;
    CHECK(vgk_pci_classify(&d) == VGK_PCI_CLASS_SERIAL_BUS,
          "serial bus classification failed");

    d = sample(0x03U, 0x00U);
    d.device = 32U;
    CHECK(!vgk_pci_device_is_valid(&d), "device number 32 accepted");
    d = sample(0x03U, 0x00U);
    d.function = 8U;
    CHECK(!vgk_pci_device_is_valid(&d), "function number 8 accepted");
    d = sample(0x03U, 0x00U);
    d.vendor_id = UINT16_C(0xFFFF);
    CHECK(!vgk_pci_device_is_valid(&d), "absent PCI function accepted");
    CHECK(!vgk_pci_device_is_valid(0), "null PCI record accepted");

    if (failures != 0U)
        return 1;
    puts("OK: PCI identity validation and device class classification");
    return 0;
}
