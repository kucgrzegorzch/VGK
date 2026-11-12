#include "vgk_pci.h"

int vgk_pci_device_is_valid(const VGK_PCI_DEVICE *device)
{
    if (device == 0)
        return 0;
    if (device->device > 31U || device->function > 7U)
        return 0;
    if (device->vendor_id == UINT16_C(0xFFFF))
        return 0;
    return 1;
}

VGK_PCI_DEVICE_CLASS vgk_pci_classify(const VGK_PCI_DEVICE *device)
{
    if (!vgk_pci_device_is_valid(device))
        return VGK_PCI_CLASS_OTHER;

    switch (device->class_code) {
    case 0x01: return VGK_PCI_CLASS_STORAGE;
    case 0x02: return VGK_PCI_CLASS_NETWORK;
    case 0x03: return VGK_PCI_CLASS_DISPLAY;
    case 0x04: return VGK_PCI_CLASS_MULTIMEDIA;
    case 0x06: return VGK_PCI_CLASS_BRIDGE;
    case 0x0C:
        return device->subclass == 0x03
            ? VGK_PCI_CLASS_USB : VGK_PCI_CLASS_SERIAL_BUS;
    default: return VGK_PCI_CLASS_OTHER;
    }
}
