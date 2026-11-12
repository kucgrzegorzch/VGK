#ifndef VGK_UEFI_GRAPHICS_H
#define VGK_UEFI_GRAPHICS_H

#include <stddef.h>
#include <stdint.h>

#include "uefi_services.h"

typedef enum {
    VGK_EFI_GRAPHICS_PIXEL_RGB_RESERVED_8 = 0,
    VGK_EFI_GRAPHICS_PIXEL_BGR_RESERVED_8 = 1,
    VGK_EFI_GRAPHICS_PIXEL_BIT_MASK = 2,
    VGK_EFI_GRAPHICS_PIXEL_BLT_ONLY = 3
} VGK_EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    struct {
        uint32_t red_mask;
        uint32_t green_mask;
        uint32_t blue_mask;
        uint32_t reserved_mask;
    } pixel_information;
    uint32_t pixels_per_scan_line;
} VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    VGK_EFI_UINTN size_of_info;
    uint64_t framebuffer_base;
    VGK_EFI_UINTN framebuffer_size;
} VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_GRAPHICS_QUERY_MODE)(
    void *self, uint32_t mode_number,
    VGK_EFI_UINTN *size_of_info,
    VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **info);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_GRAPHICS_SET_MODE)(
    void *self, uint32_t mode_number);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_GRAPHICS_BLT)(
    void *self, void *blt_buffer, uint32_t blt_operation,
    VGK_EFI_UINTN source_x, VGK_EFI_UINTN source_y,
    VGK_EFI_UINTN destination_x, VGK_EFI_UINTN destination_y,
    VGK_EFI_UINTN width, VGK_EFI_UINTN height,
    VGK_EFI_UINTN delta);

typedef struct {
    VGK_EFI_GRAPHICS_QUERY_MODE query_mode;
    VGK_EFI_GRAPHICS_SET_MODE set_mode;
    VGK_EFI_GRAPHICS_BLT blt;
    VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
} VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL;

_Static_assert(sizeof(VGK_EFI_GRAPHICS_OUTPUT_MODE_INFORMATION) == 36,
               "unexpected GOP mode information size");
_Static_assert(offsetof(VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE,
                        framebuffer_base) == 24,
               "unexpected GOP framebuffer base offset");
_Static_assert(sizeof(VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE) == 40,
               "unexpected GOP mode size");
_Static_assert(offsetof(VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL, mode) == 24,
               "unexpected GOP protocol mode offset");
_Static_assert(sizeof(VGK_EFI_GRAPHICS_OUTPUT_PROTOCOL) == 32,
               "unexpected GOP protocol size");

#endif
