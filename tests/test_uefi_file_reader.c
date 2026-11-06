#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uefi_file_reader.h"

static const uint8_t source_data[] = "VGK-reader-test";
static uint64_t current_offset;
static unsigned close_count;
static unsigned open_count;
static size_t max_read_chunk = (size_t)-1;
static VGK_EFI_CHAR16 captured_path[VGK_MANIFEST_MAX_PATH + 2U];

static VGK_EFI_FILE_PROTOCOL fake_file;
static VGK_EFI_FILE_PROTOCOL fake_root;

static VGK_EFI_STATUS VGK_EFIAPI fake_set_position(
    VGK_EFI_FILE_PROTOCOL *self, uint64_t position)
{
    (void)self;
    current_offset = position;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI fake_read(
    VGK_EFI_FILE_PROTOCOL *self, VGK_EFI_UINTN *size, void *buffer)
{
    size_t remaining;
    size_t amount;
    (void)self;

    if (current_offset > sizeof(source_data) - 1U)
        return UINT64_C(0x8000000000000002);
    remaining = sizeof(source_data) - 1U - (size_t)current_offset;
    amount = *size < remaining ? (size_t)*size : remaining;
    if (amount > max_read_chunk)
        amount = max_read_chunk;
    memcpy(buffer, source_data + current_offset, amount);
    *size = amount;
    current_offset += amount;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI fake_close(VGK_EFI_FILE_PROTOCOL *self)
{
    (void)self;
    ++close_count;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI fake_open(
    VGK_EFI_FILE_PROTOCOL *self,
    VGK_EFI_FILE_PROTOCOL **new_handle,
    VGK_EFI_CHAR16 *name,
    uint64_t mode,
    uint64_t attributes)
{
    size_t i = 0;
    (void)self;
    (void)attributes;
    assert(mode == VGK_EFI_FILE_MODE_READ);

    do {
        assert(i < sizeof(captured_path) / sizeof(captured_path[0]));
        captured_path[i] = name[i];
    } while (name[i++] != 0);

    ++open_count;
    *new_handle = &fake_file;
    return 0;
}

int main(void)
{
    VGK_INTEGRITY_READ read = 0;
    void *file_context = 0;
    uint8_t output[5] = {0};
    size_t received = 0;
    static const uint8_t relative_path[] = "boot/kernel.elf";
    static const uint8_t traversal_path[] = "../kernel.elf";

    fake_root.open = fake_open;
    fake_file.read = fake_read;
    fake_file.set_position = fake_set_position;
    fake_file.close = fake_close;

    assert(vgk_uefi_manifest_open_file(
        &fake_root, relative_path, (uint16_t)(sizeof(relative_path) - 1U),
        &read, &file_context) == 0);
    assert(read != 0);
    assert(file_context == &fake_file);
    assert(open_count == 1U);

    assert(captured_path[0] == (VGK_EFI_CHAR16)'\\');
    assert(captured_path[5] == (VGK_EFI_CHAR16)'\\');
    assert(captured_path[sizeof(relative_path)] == 0);

    assert(read(file_context, 4, output, sizeof(output), &received) == 0);
    assert(received == sizeof(output));
    assert(memcmp(output, "reade", sizeof(output)) == 0);

    vgk_uefi_manifest_close_file(&fake_root, file_context);
    assert(close_count == 1U);

    assert(vgk_uefi_manifest_open_file(
        &fake_root, traversal_path, (uint16_t)(sizeof(traversal_path) - 1U),
        &read, &file_context) != 0);
    assert(open_count == 1U);

    {
        uint8_t whole[sizeof(source_data) - 1U];
        size_t whole_size = 0;
        static const uint8_t path[] = "boot/data.bin";
        max_read_chunk = 3U;
        assert(vgk_uefi_read_file_bounded(
            &fake_root, path, (uint16_t)(sizeof(path) - 1U),
            whole, sizeof(whole), &whole_size) == 0);
        assert(whole_size == sizeof(source_data) - 1U);
        assert(memcmp(whole, source_data, whole_size) == 0);
        assert(close_count == 2U);

        {
            uint8_t too_small[4];
            size_t rejected_size = 99U;
            assert(vgk_uefi_read_file_bounded(
                &fake_root, path, (uint16_t)(sizeof(path) - 1U),
                too_small, sizeof(too_small), &rejected_size) != 0);
            assert(rejected_size == 0U);
            assert(close_count == 3U);
        }
        max_read_chunk = (size_t)-1;
    }

    return 0;
}
