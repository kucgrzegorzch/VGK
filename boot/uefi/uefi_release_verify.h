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


#define VGK_RELEASE_MANIFEST_PATH "EFI/VGK/VGKM"
#define VGK_RELEASE_SIGNATURE_PATH "EFI/VGK/VGKM.sig"
#define VGK_RELEASE_SIGNATURE_SIZE 64U

/*
 * Load the signed manifest and its detached Ed25519 signature from the
 * firmware volume, then verify every listed file. Buffers belong to caller.
 * The caller must provide a trusted public key obtained from a trusted source.
 * The manifest buffer capacity is the hard upper bound; no allocation occurs.
 */
VGK_MANIFEST_RESULT vgk_uefi_load_and_verify_release(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    uint8_t *manifest_buffer,
    size_t manifest_capacity,
    size_t *manifest_size,
    uint8_t signature_buffer[VGK_RELEASE_SIGNATURE_SIZE],
    const uint8_t trusted_public_key[32]
);

#endif
