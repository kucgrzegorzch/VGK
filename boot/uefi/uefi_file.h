#ifndef VGK_UEFI_FILE_H
#define VGK_UEFI_FILE_H

/*
 * Minimal UEFI file-system ABI declarations used by the VGK loader.
 * Definitions follow the UEFI Simple File System and File Protocol layouts.
 * This header declares firmware interfaces only; it does not yet open files.
 */
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64)
#  if defined(__GNUC__) || defined(__clang__)
#    define VGK_EFIAPI __attribute__((ms_abi))
#  else
#    define VGK_EFIAPI
#  endif
#else
#  error "VGK UEFI file access currently supports x86-64 only"
#endif

typedef uint64_t VGK_EFI_STATUS;
typedef uint64_t VGK_EFI_UINTN;
typedef uint64_t VGK_EFI_LBA;
typedef uint16_t VGK_EFI_CHAR16;
typedef void *VGK_EFI_HANDLE;
typedef uint8_t VGK_EFI_BOOLEAN;

typedef struct VGK_EFI_FILE_PROTOCOL VGK_EFI_FILE_PROTOCOL;
typedef struct VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_OPEN)(
    VGK_EFI_FILE_PROTOCOL *self,
    VGK_EFI_FILE_PROTOCOL **new_handle,
    VGK_EFI_CHAR16 *file_name,
    uint64_t open_mode,
    uint64_t attributes);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_CLOSE)(
    VGK_EFI_FILE_PROTOCOL *self);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_DELETE)(
    VGK_EFI_FILE_PROTOCOL *self);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_READ)(
    VGK_EFI_FILE_PROTOCOL *self,
    VGK_EFI_UINTN *buffer_size,
    void *buffer);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_WRITE)(
    VGK_EFI_FILE_PROTOCOL *self,
    VGK_EFI_UINTN *buffer_size,
    const void *buffer);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_GET_POSITION)(
    VGK_EFI_FILE_PROTOCOL *self,
    uint64_t *position);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_SET_POSITION)(
    VGK_EFI_FILE_PROTOCOL *self,
    uint64_t position);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_GET_INFO)(
    VGK_EFI_FILE_PROTOCOL *self,
    const void *information_type,
    VGK_EFI_UINTN *buffer_size,
    void *buffer);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_SET_INFO)(
    VGK_EFI_FILE_PROTOCOL *self,
    const void *information_type,
    VGK_EFI_UINTN buffer_size,
    const void *buffer);

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_FILE_FLUSH)(
    VGK_EFI_FILE_PROTOCOL *self);

struct VGK_EFI_FILE_PROTOCOL {
    uint64_t revision;
    VGK_EFI_FILE_OPEN open;
    VGK_EFI_FILE_CLOSE close;
    VGK_EFI_FILE_DELETE delete_file;
    VGK_EFI_FILE_READ read;
    VGK_EFI_FILE_WRITE write;
    VGK_EFI_FILE_GET_POSITION get_position;
    VGK_EFI_FILE_SET_POSITION set_position;
    VGK_EFI_FILE_GET_INFO get_info;
    VGK_EFI_FILE_SET_INFO set_info;
    VGK_EFI_FILE_FLUSH flush;
    void *open_ex;
    void *read_ex;
    void *write_ex;
    void *flush_ex;
};

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_OPEN_VOLUME)(
    VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *self,
    VGK_EFI_FILE_PROTOCOL **root);

struct VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    VGK_EFI_OPEN_VOLUME open_volume;
};

#define VGK_EFI_FILE_MODE_READ UINT64_C(0x0000000000000001)
#define VGK_EFI_FILE_MODE_WRITE UINT64_C(0x0000000000000002)
#define VGK_EFI_FILE_MODE_CREATE UINT64_C(0x8000000000000000)
#define VGK_EFI_FILE_PROTOCOL_REVISION UINT64_C(0x00010000)
#define VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION UINT64_C(0x00010000)

/* Catch ABI drift at compile time before any firmware function is called. */
_Static_assert(sizeof(VGK_EFI_STATUS) == 8, "UEFI status must be 64-bit");
_Static_assert(sizeof(VGK_EFI_CHAR16) == 2, "UEFI CHAR16 must be 16-bit");
_Static_assert(offsetof(VGK_EFI_FILE_PROTOCOL, open) == 8,
               "unexpected EFI_FILE_PROTOCOL Open offset");
_Static_assert(offsetof(VGK_EFI_FILE_PROTOCOL, read) == 32,
               "unexpected EFI_FILE_PROTOCOL Read offset");
_Static_assert(offsetof(VGK_EFI_FILE_PROTOCOL, get_position) == 48,
               "unexpected EFI_FILE_PROTOCOL GetPosition offset");
_Static_assert(offsetof(VGK_EFI_FILE_PROTOCOL, flush) == 80,
               "unexpected EFI_FILE_PROTOCOL Flush offset");
_Static_assert(offsetof(VGK_EFI_FILE_PROTOCOL, open_ex) == 88,
               "unexpected EFI_FILE_PROTOCOL OpenEx offset");
_Static_assert(sizeof(VGK_EFI_FILE_PROTOCOL) == 120,
               "unexpected EFI_FILE_PROTOCOL size");
_Static_assert(offsetof(VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL, open_volume) == 8,
               "unexpected Simple File System OpenVolume offset");
_Static_assert(sizeof(VGK_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL) == 16,
               "unexpected Simple File System protocol size");

#endif
