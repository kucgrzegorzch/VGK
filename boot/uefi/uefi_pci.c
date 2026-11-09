#include "uefi_pci.h"
static const VGK_EFI_GUID pci_io_protocol_guid = {
    UINT32_C(0x4CF5B200), UINT16_C(0x68B8), UINT16_C(0x4CA5),
    {0x9E,0xEC,0xB2,0x3E,0x3F,0x50,0x02,0x9A}
};
enum { VGK_EFI_PCI_WIDTH_UINT32 = 2, VGK_EFI_PCI_CONFIG_DWORDS = 16 };
int vgk_uefi_pci_read_identity(VGK_EFI_PCI_IO_PROTOCOL *p, VGK_PCI_DEVICE *out)
{
    uint32_t cfg[VGK_EFI_PCI_CONFIG_DWORDS];
    VGK_EFI_UINTN seg=0,bus=0,dev=0,fn=0;
    VGK_EFI_STATUS s; VGK_PCI_DEVICE d;
    if (!p || !out || !p->pci.read || !p->get_location) return -1;
    s=p->pci.read(p,VGK_EFI_PCI_WIDTH_UINT32,0,VGK_EFI_PCI_CONFIG_DWORDS,cfg);
    if (s & UINT64_C(0x8000000000000000)) return -1;
    s=p->get_location(p,&seg,&bus,&dev,&fn);
    if ((s & UINT64_C(0x8000000000000000)) || seg>UINT16_MAX ||
        bus>UINT8_MAX || dev>31U || fn>7U) return -1;
    for (size_t i=0;i<sizeof(d);++i) ((uint8_t *)&d)[i]=0;
    d.segment=(uint16_t)seg; d.bus=(uint8_t)bus; d.device=(uint8_t)dev; d.function=(uint8_t)fn;
    d.vendor_id=(uint16_t)cfg[0]; d.device_id=(uint16_t)(cfg[0]>>16);
    d.revision=(uint8_t)cfg[2]; d.programming_interface=(uint8_t)(cfg[2]>>8);
    d.subclass=(uint8_t)(cfg[2]>>16); d.class_code=(uint8_t)(cfg[2]>>24);
    d.header_type=(uint8_t)(cfg[3]>>16);
    if (!vgk_pci_device_is_valid(&d)) return -1;
    *out=d; return 0;
}
int vgk_uefi_pci_enumerate(VGK_EFI_BOOT_SERVICES_PREFIX *bs,
    VGK_PCI_DEVICE *out,size_t cap,size_t *count)
{
    VGK_EFI_HANDLE *handles=0; VGK_EFI_UINTN n=0; size_t found=0;
    VGK_EFI_STATUS s; int result=0;
    if (!count || !bs || !bs->locate_handle_buffer || !bs->handle_protocol ||
        !bs->free_pool || (cap && !out)) return -1;
    *count=0;
    s=bs->locate_handle_buffer(VGK_EFI_LOCATE_BY_PROTOCOL,&pci_io_protocol_guid,0,&n,&handles);
    if (s & UINT64_C(0x8000000000000000)) return -1;
    if (n && !handles) return -1;
    for (VGK_EFI_UINTN i=0;i<n;++i) {
        VGK_EFI_PCI_IO_PROTOCOL *p=0; VGK_PCI_DEVICE d;
        s=bs->handle_protocol(handles[i],&pci_io_protocol_guid,(void **)&p);
        if ((s & UINT64_C(0x8000000000000000)) || !p ||
            vgk_uefi_pci_read_identity(p,&d)!=0) continue;
        if (found==cap) { result=-2; break; }
        out[found++]=d;
    }
    if (handles) {
        s = bs->free_pool(handles);
        if ((s & UINT64_C(0x8000000000000000)) != 0)
            result = -1;
    }
    if (result == -1) {
        /* Never expose a partial inventory after a firmware-service error. */
        for (size_t i = 0; i < found; ++i) {
            uint8_t *bytes = (uint8_t *)&out[i];
            for (size_t j = 0; j < sizeof(*out); ++j)
                bytes[j] = 0;
        }
        found = 0;
    }
    *count=found;
    return result;
}
