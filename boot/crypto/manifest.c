#include "manifest.h"
#include "ed25519.h"

#define VGK_MANIFEST_HEADER_SIZE 8U
#define VGK_MANIFEST_ENTRY_FIXED_SIZE 42U

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t read_u64_le(const uint8_t *p)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i)
        value |= (uint64_t)p[i] << (i * 8U);
    return value;
}

static int safe_relative_path(const uint8_t *path, uint16_t length)
{
    uint16_t component_start = 0;

    if (path == 0 || length == 0 || length > VGK_MANIFEST_MAX_PATH)
        return 0;
    if (path[0] == '/' || path[length - 1] == '/')
        return 0;

    for (uint16_t i = 0; i <= length; ++i) {
        if (i == length || path[i] == '/') {
            uint16_t component_length = (uint16_t)(i - component_start);
            if (component_length == 0 ||
                (component_length == 1 && path[component_start] == '.') ||
                (component_length == 2 && path[component_start] == '.' &&
                 path[component_start + 1] == '.'))
                return 0;
            component_start = (uint16_t)(i + 1);
            continue;
        }

        uint8_t ch = path[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '.' || ch == '_' || ch == '-' ))
            return 0;
    }
    return 1;
}

static int paths_compare(const VGK_MANIFEST_ENTRY *a,
                         const VGK_MANIFEST_ENTRY *b)
{
    uint16_t common = a->path_length < b->path_length
        ? a->path_length : b->path_length;
    for (uint16_t i = 0; i < common; ++i) {
        if (a->path[i] < b->path[i])
            return -1;
        if (a->path[i] > b->path[i])
            return 1;
    }
    if (a->path_length < b->path_length)
        return -1;
    if (a->path_length > b->path_length)
        return 1;
    return 0;
}

VGK_MANIFEST_RESULT vgk_manifest_get(
    const VGK_MANIFEST *manifest,
    uint16_t index,
    VGK_MANIFEST_ENTRY *entry)
{
    size_t offset;

    if (manifest == 0 || entry == 0 || manifest->encoded == 0)
        return VGK_MANIFEST_INVALID_ARGUMENT;
    if (index >= manifest->entry_count)
        return VGK_MANIFEST_BAD_FORMAT;

    offset = manifest->entries_offset;
    for (uint16_t i = 0; i <= index; ++i) {
        uint16_t path_length;
        if (offset > manifest->encoded_size ||
            manifest->encoded_size - offset < VGK_MANIFEST_ENTRY_FIXED_SIZE)
            return VGK_MANIFEST_BAD_FORMAT;
        path_length = read_u16_le(manifest->encoded + offset);
        if (path_length == 0 || path_length > VGK_MANIFEST_MAX_PATH ||
            (size_t)path_length > manifest->encoded_size - offset - VGK_MANIFEST_ENTRY_FIXED_SIZE)
            return VGK_MANIFEST_BAD_FORMAT;
        if (i == index) {
            entry->path_length = path_length;
            entry->file_size = read_u64_le(manifest->encoded + offset + 2);
            entry->sha256 = manifest->encoded + offset + 10;
            entry->path = manifest->encoded + offset + VGK_MANIFEST_ENTRY_FIXED_SIZE;
            return VGK_MANIFEST_OK;
        }
        offset += VGK_MANIFEST_ENTRY_FIXED_SIZE + path_length;
    }
    return VGK_MANIFEST_BAD_FORMAT;
}

VGK_MANIFEST_RESULT vgk_manifest_open(
    VGK_MANIFEST *manifest,
    const uint8_t *encoded,
    size_t encoded_size,
    const uint8_t public_key[32],
    const uint8_t signature[64])
{
    uint16_t count;
    size_t offset = VGK_MANIFEST_HEADER_SIZE;

    if (manifest == 0 || encoded == 0 || public_key == 0 || signature == 0)
        return VGK_MANIFEST_INVALID_ARGUMENT;
    manifest->encoded = 0;
    manifest->encoded_size = 0;
    manifest->entry_count = 0;
    manifest->entries_offset = 0;

    if (encoded_size > VGK_MANIFEST_MAX_SIZE)
        return VGK_MANIFEST_TOO_LARGE;
    if (encoded_size < VGK_MANIFEST_HEADER_SIZE)
        return VGK_MANIFEST_BAD_FORMAT;

    if (vgk_ed25519_verify(public_key, encoded, encoded_size, signature)
        != VGK_ED25519_VALID)
        return VGK_MANIFEST_BAD_SIGNATURE;

    if (encoded[0] != 'V' || encoded[1] != 'G' ||
        encoded[2] != 'K' || encoded[3] != 'M' ||
        read_u16_le(encoded + 4) != 1U)
        return VGK_MANIFEST_BAD_FORMAT;

    count = read_u16_le(encoded + 6);
    if (count == 0 || count > VGK_MANIFEST_MAX_ENTRIES)
        return VGK_MANIFEST_BAD_FORMAT;

    manifest->encoded = encoded;
    manifest->encoded_size = encoded_size;
    manifest->entry_count = count;
    manifest->entries_offset = offset;

    for (uint16_t i = 0; i < count; ++i) {
        VGK_MANIFEST_ENTRY current;
        if (vgk_manifest_get(manifest, i, &current) != VGK_MANIFEST_OK ||
            !safe_relative_path(current.path, current.path_length)) {
            manifest->encoded = 0;
            return VGK_MANIFEST_BAD_FORMAT;
        }
        offset += VGK_MANIFEST_ENTRY_FIXED_SIZE + current.path_length;
        for (uint16_t j = 0; j < i; ++j) {
            VGK_MANIFEST_ENTRY previous;
            if (vgk_manifest_get(manifest, j, &previous) != VGK_MANIFEST_OK) {
                manifest->encoded = 0;
                return VGK_MANIFEST_BAD_FORMAT;
            }
            int order = paths_compare(&previous, &current);
            if (order == 0) {
                manifest->encoded = 0;
                return VGK_MANIFEST_DUPLICATE_PATH;
            }
            if (order > 0) {
                manifest->encoded = 0;
                return VGK_MANIFEST_BAD_FORMAT;
            }
        }
    }

    if (offset != encoded_size) {
        manifest->encoded = 0;
        return VGK_MANIFEST_BAD_FORMAT;
    }
    return VGK_MANIFEST_OK;
}

VGK_MANIFEST_RESULT vgk_manifest_verify_files(
    const VGK_MANIFEST *manifest,
    VGK_MANIFEST_OPEN_FILE open_file,
    VGK_MANIFEST_CLOSE_FILE close_file,
    void *filesystem_context)
{
    if (manifest == 0 || manifest->encoded == 0 ||
        open_file == 0 || close_file == 0)
        return VGK_MANIFEST_INVALID_ARGUMENT;

    for (uint16_t i = 0; i < manifest->entry_count; ++i) {
        VGK_MANIFEST_ENTRY entry;
        VGK_INTEGRITY_READ read = 0;
        void *file_context = 0;
        VGK_INTEGRITY_RESULT integrity_result;

        if (vgk_manifest_get(manifest, i, &entry) != VGK_MANIFEST_OK)
            return VGK_MANIFEST_BAD_FORMAT;

        if (open_file(filesystem_context, entry.path, entry.path_length,
                      &read, &file_context) != 0 ||
            read == 0 || file_context == 0) {
            if (file_context != 0)
                close_file(filesystem_context, file_context);
            return VGK_MANIFEST_FILE_OPEN_ERROR;
        }

        integrity_result = vgk_verify_reader_sha256(
            read, file_context, entry.file_size, entry.sha256);
        close_file(filesystem_context, file_context);

        if (integrity_result != VGK_INTEGRITY_OK)
            return VGK_MANIFEST_FILE_INTEGRITY_ERROR;
    }

    return VGK_MANIFEST_OK;
}
