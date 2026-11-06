#ifndef VGK_UEFI_RELEASE_VERIFY_H
#define VGK_UEFI_RELEASE_VERIFY_H

#include "manifest.h"
#include "uefi_file.h"

/*
 * Verify an already-read VGKM manifest and all files listed in it.
 * The caller must supply the trusted public key from a trusted source.
 * No private key is used by the boot program.
 */
VGK_MANIFEST_RESULT vgk_uefi_verify_manifest_buffer(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    const uint8_t *manifest_bytes,
    size_t manifest_size,
    const uint8_t signature[64],
    const uint8_t trusted_public_key[32]
);

#endif
