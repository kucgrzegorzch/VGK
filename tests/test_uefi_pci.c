#include <stdio.h>
#include <string.h>
#include "uefi_pci.h"
static unsigned failures;
#define CHECK(x,m) do { if(!(x)){puts("FAIL: " m);++failures;} } while(0)
static VGK_EFI_STATUS VGK_EFIAPI mock_read(VGK_EFI_PCI_IO_PROTOCOL *p,uint32_t w,uint32_t o,VGK_EFI_UINTN n,void *b)
{
 uint32_t *v=b;(void)p;if(w!=2||o!=0||n!=16||!b)return UINT64_C(0x8000000000000002);
 memset(v,0,16*sizeof(*v));v[0]=UINT32_C(0x12348086);v[2]=UINT32_C(0x03000002);v[3]=UINT32_C(0x00010000);return 0;
}
static VGK_EFI_STATUS VGK_EFIAPI mock_loc(VGK_EFI_PCI_IO_PROTOCOL*p,VGK_EFI_UINTN*s,VGK_EFI_UINTN*b,VGK_EFI_UINTN*d,VGK_EFI_UINTN*f)
{(void)p;*s=2;*b=3;*d=4;*f=1;return 0;}
int main(void)
{
 VGK_EFI_PCI_IO_PROTOCOL p;VGK_PCI_DEVICE d;memset(&p,0,sizeof(p));p.pci.read=mock_read;p.get_location=mock_loc;
 CHECK(vgk_uefi_pci_read_identity(&p,&d)==0,"UEFI PCI read failed");
 CHECK(d.segment==2&&d.bus==3&&d.device==4&&d.function==1,"BDF decode failed");
 CHECK(d.vendor_id==0x8086&&d.device_id==0x1234,"IDs decode failed");
 CHECK(d.class_code==3&&d.subclass==0&&d.revision==2,"class decode failed");
 CHECK(vgk_pci_classify(&d)==VGK_PCI_CLASS_DISPLAY,"display class failed");
 if (failures != 0) return 1;
 puts("OK: UEFI PCI configuration read and identity decoding");
 return 0;
}
