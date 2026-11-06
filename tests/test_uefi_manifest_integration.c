#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uefi_release_verify.h"
#include "monocypher/monocypher-ed25519.h"

static const uint8_t expected_digest[32] = {
    0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
    0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
    0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
    0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
};
static const uint8_t good_file[] = "abc";
static const uint8_t changed_file[] = "abd";
static const uint8_t *active_bytes = good_file;
static const uint8_t *kernel_bytes = good_file;
static size_t active_size = sizeof(good_file) - 1U;
static uint64_t active_offset;
static unsigned open_count;
static unsigned close_count;
static uint8_t volume_manifest[128];
static size_t volume_manifest_size;
static uint8_t volume_signature[64];
static VGK_EFI_FILE_PROTOCOL root;
static VGK_EFI_FILE_PROTOCOL file;

static int path_is(VGK_EFI_CHAR16 *path, const char *expected)
{
    size_t i;
    for (i = 0; expected[i] != 0; ++i)
        if (path[i] != (VGK_EFI_CHAR16)(uint8_t)expected[i])
            return 0;
    return path[i] == 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_open(
    VGK_EFI_FILE_PROTOCOL *self,
    VGK_EFI_FILE_PROTOCOL **new_handle,
    VGK_EFI_CHAR16 *path,
    uint64_t mode,
    uint64_t attributes)
{
    (void)self;
    (void)attributes;
    if (mode != VGK_EFI_FILE_MODE_READ)
        return UINT64_C(0x8000000000000002);

    if (path_is(path, "\\boot\\kernel.elf")) {
        active_bytes = kernel_bytes;
        active_size = sizeof(good_file) - 1U;
    } else if (path_is(path, "\\EFI\\VGK\\VGKM")) {
        active_bytes = volume_manifest;
        active_size = volume_manifest_size;
    } else if (path_is(path, "\\EFI\\VGK\\VGKM.sig")) {
        active_bytes = volume_signature;
        active_size = sizeof(volume_signature);
    } else {
        return UINT64_C(0x8000000000000002);
    }

    ++open_count;
    active_offset = 0;
    *new_handle = &file;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_set_position(
    VGK_EFI_FILE_PROTOCOL *self, uint64_t position)
{
    (void)self;
    active_offset = position;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_read(
    VGK_EFI_FILE_PROTOCOL *self, VGK_EFI_UINTN *size, void *buffer)
{
    size_t remaining;
    size_t amount;
    (void)self;

    if (active_offset > active_size)
        return UINT64_C(0x8000000000000002);
    remaining = active_size - (size_t)active_offset;
    amount = *size < remaining ? (size_t)*size : remaining;
    if (amount > 1U)
        amount = 1U; /* Exercise valid short reads. */
    if (amount != 0)
        memcpy(buffer, active_bytes + active_offset, amount);
    *size = amount;
    active_offset += amount;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI mock_close(VGK_EFI_FILE_PROTOCOL *self)
{
    (void)self;
    ++close_count;
    return 0;
}

static size_t make_manifest(uint8_t *encoded)
{
    static const char path[] = "boot/kernel.elf";
    const size_t path_length = sizeof(path) - 1U;
    const size_t size = 8U + 42U + path_length;

    memset(encoded, 0, size);
    encoded[0] = 'V'; encoded[1] = 'G';
    encoded[2] = 'K'; encoded[3] = 'M';
    encoded[4] = 1; encoded[6] = 1;
    encoded[8] = (uint8_t)path_length;
    encoded[10] = 3;
    memcpy(encoded + 18, expected_digest, sizeof(expected_digest));
    memcpy(encoded + 50, path, path_length);
    return size;
}

int main(void)
{
    uint8_t seed[32] = {7};
    uint8_t secret_key[64], public_key[32], signature[64];
    uint8_t altered_signature[64];
    uint8_t encoded[128];
    uint8_t manifest_buffer[VGK_MANIFEST_MAX_SIZE];
    uint8_t signature_buffer[VGK_RELEASE_SIGNATURE_SIZE];
    size_t encoded_size;
    size_t loaded_size = 0;
    int failures = 0;

    root.open = mock_open;
    file.read = mock_read;
    file.set_position = mock_set_position;
    file.close = mock_close;

    crypto_ed25519_key_pair(secret_key, public_key, seed);
    encoded_size = make_manifest(encoded);
    crypto_ed25519_sign(signature, secret_key, encoded, encoded_size);

    if (vgk_uefi_verify_manifest_buffer(
            &root, encoded, encoded_size, signature, public_key)
            != VGK_MANIFEST_OK)
        ++failures;
    if (open_count != 1U || close_count != 1U)
        ++failures;

    kernel_bytes = changed_file;
    if (vgk_uefi_verify_manifest_buffer(
            &root, encoded, encoded_size, signature, public_key)
            != VGK_MANIFEST_FILE_INTEGRITY_ERROR)
        ++failures;
    if (open_count != 2U || close_count != 2U)
        ++failures;

    memcpy(altered_signature, signature, sizeof(signature));
    altered_signature[0] ^= 1U;
    kernel_bytes = good_file;
    if (vgk_uefi_verify_manifest_buffer(
            &root, encoded, encoded_size, altered_signature, public_key)
            != VGK_MANIFEST_BAD_SIGNATURE)
        ++failures;
    if (open_count != 2U || close_count != 2U)
        ++failures;

    memcpy(volume_manifest, encoded, encoded_size);
    volume_manifest_size = encoded_size;
    memcpy(volume_signature, signature, sizeof(signature));
    if (vgk_uefi_load_and_verify_release(
            &root, manifest_buffer, sizeof(manifest_buffer), &loaded_size,
            signature_buffer, public_key) != VGK_MANIFEST_OK)
        ++failures;
    if (loaded_size != encoded_size || open_count != 5U || close_count != 5U)
        ++failures;

    volume_signature[0] ^= 1U;
    if (vgk_uefi_load_and_verify_release(
            &root, manifest_buffer, sizeof(manifest_buffer), &loaded_size,
            signature_buffer, public_key) != VGK_MANIFEST_BAD_SIGNATURE)
        ++failures;
    /* Invalid signature must stop before opening the kernel listed in VGKM. */
    if (open_count != 7U || close_count != 7U)
        ++failures;

    if (failures != 0) {
        fprintf(stderr, "BLAD: integracja podpisu, wykazu i odczytu UEFI (%d)\n",
                failures);
        return 1;
    }
    puts("OK: UEFI wczytuje wykaz i podpis z woluminu, odrzuca zly podpis i wykrywa podmiane.");
    return 0;
}
