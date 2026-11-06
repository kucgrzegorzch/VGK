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


VGK_MANIFEST_RESULT vgk_uefi_load_and_verify_release(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    uint8_t *manifest_buffer,
    size_t manifest_capacity,
    size_t *manifest_size,
    uint8_t signature_buffer[VGK_RELEASE_SIGNATURE_SIZE],
    const uint8_t trusted_public_key[32])
{
    static const uint8_t manifest_path[] = VGK_RELEASE_MANIFEST_PATH;
    static const uint8_t signature_path[] = VGK_RELEASE_SIGNATURE_PATH;
    size_t signature_size = 0;

    if (root_directory == 0 || manifest_buffer == 0 ||
        manifest_capacity == 0 || manifest_capacity > VGK_MANIFEST_MAX_SIZE ||
        manifest_size == 0 || signature_buffer == 0 ||
        trusted_public_key == 0)
        return VGK_MANIFEST_INVALID_ARGUMENT;

    *manifest_size = 0;

    if (vgk_uefi_read_file_bounded(
            root_directory, manifest_path,
            (uint16_t)(sizeof(manifest_path) - 1U),
            manifest_buffer, manifest_capacity, manifest_size) != 0)
        return VGK_MANIFEST_BAD_FORMAT;

    if (*manifest_size == 0 || *manifest_size > VGK_MANIFEST_MAX_SIZE)
        return VGK_MANIFEST_BAD_FORMAT;

    if (vgk_uefi_read_file_bounded(
            root_directory, signature_path,
            (uint16_t)(sizeof(signature_path) - 1U),
            signature_buffer, VGK_RELEASE_SIGNATURE_SIZE,
            &signature_size) != 0 ||
        signature_size != VGK_RELEASE_SIGNATURE_SIZE)
        return VGK_MANIFEST_BAD_FORMAT;

    return vgk_uefi_verify_manifest_buffer(
        root_directory, manifest_buffer, *manifest_size,
        signature_buffer, trusted_public_key);
}
