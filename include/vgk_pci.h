#ifndef VGK_PCI_H
#define VGK_PCI_H

#include <stdint.h>

/* A normalized PCI function identity; it contains no live hardware pointers. */
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t revision;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t programming_interface;
    uint8_t header_type;
    uint16_t segment;
} VGK_PCI_DEVICE;

typedef enum {
    VGK_PCI_CLASS_OTHER = 0,
    VGK_PCI_CLASS_STORAGE,
    VGK_PCI_CLASS_NETWORK,
    VGK_PCI_CLASS_DISPLAY,
    VGK_PCI_CLASS_MULTIMEDIA,
    VGK_PCI_CLASS_BRIDGE,
    VGK_PCI_CLASS_SERIAL_BUS,
    VGK_PCI_CLASS_USB
} VGK_PCI_DEVICE_CLASS;

/* Reject impossible BDF values and the PCI-defined absent-function ID. */
int vgk_pci_device_is_valid(const VGK_PCI_DEVICE *device);

/* Classify by the standard PCI class/subclass fields, not vendor names. */
VGK_PCI_DEVICE_CLASS vgk_pci_classify(const VGK_PCI_DEVICE *device);

#endif
