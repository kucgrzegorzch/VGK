#include "uefi_release_verify.h"
#include "uefi_file_reader.h"

/*
 * This is the UEFI-side connection between signature verification,
 * the signed VGKM parser, and reads from the firmware-provided volume.
 * It deliberately does not obtain or invent a trusted key.
 */
VGK_MANIFEST_RESULT vgk_uefi_verify_manifest_buffer(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    const uint8_t *manifest_bytes,
    size_t manifest_size,
    const uint8_t signature[64],
    const uint8_t trusted_public_key[32])
{
    VGK_MANIFEST manifest;
    VGK_MANIFEST_RESULT result;

    if (root_directory == 0 || manifest_bytes == 0 ||
        signature == 0 || trusted_public_key == 0)
        return VGK_MANIFEST_INVALID_ARGUMENT;

    result = vgk_manifest_open(&manifest, manifest_bytes, manifest_size,
                               trusted_public_key, signature);
    if (result != VGK_MANIFEST_OK)
        return result;

    return vgk_manifest_verify_files(
        &manifest,
        vgk_uefi_manifest_open_file,
        vgk_uefi_manifest_close_file,
        root_directory);
}
