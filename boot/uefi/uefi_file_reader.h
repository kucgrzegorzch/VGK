#ifndef VGK_UEFI_FILE_READER_H
#define VGK_UEFI_FILE_READER_H

#include "manifest.h"
#include "uefi_file.h"

/*
 * Adapter between the signed-manifest reader and UEFI File Protocol.
 * filesystem_context is a pointer to an already-open UEFI root directory.
 */
int vgk_uefi_manifest_open_file(
    void *filesystem_context,
    const uint8_t *path,
    uint16_t path_length,
    VGK_INTEGRITY_READ *read,
    void **file_context
);

void vgk_uefi_manifest_close_file(
    void *filesystem_context,
    void *file_context
);

/*
 * Read a complete file into a caller-owned buffer.
 * Returns 0 on success; rejects files larger than capacity.
 */
int vgk_uefi_read_file_bounded(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    const uint8_t *path,
    uint16_t path_length,
    uint8_t *buffer,
    size_t capacity,
    size_t *file_size
);

#endif
