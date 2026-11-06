#include "uefi_file_reader.h"

#define VGK_UEFI_PATH_CAPACITY (VGK_MANIFEST_MAX_PATH + 2U)

static int valid_manifest_path(const uint8_t *path, uint16_t length)
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
            component_start = (uint16_t)(i + 1U);
            continue;
        }

        uint8_t ch = path[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '.' || ch == '_' || ch == '-'))
            return 0;
    }
    return 1;
}

static int uefi_file_read_at(
    void *file_context,
    uint64_t offset,
    uint8_t *buffer,
    size_t capacity,
    size_t *bytes_read)
{
    VGK_EFI_FILE_PROTOCOL *file = (VGK_EFI_FILE_PROTOCOL *)file_context;
    VGK_EFI_UINTN amount;
    VGK_EFI_STATUS status;

    if (file == 0 || file->set_position == 0 || file->read == 0 ||
        buffer == 0 || bytes_read == 0)
        return -1;

    *bytes_read = 0;
    if (capacity == 0)
        return 0;

    amount = (VGK_EFI_UINTN)capacity;
    if ((size_t)amount != capacity)
        return -1;

    status = file->set_position(file, offset);
    if (status != 0)
        return -1;

    status = file->read(file, &amount, buffer);
    if (status != 0 || amount > (VGK_EFI_UINTN)capacity)
        return -1;

    *bytes_read = (size_t)amount;
    return 0;
}

int vgk_uefi_manifest_open_file(
    void *filesystem_context,
    const uint8_t *path,
    uint16_t path_length,
    VGK_INTEGRITY_READ *read,
    void **file_context)
{
    VGK_EFI_FILE_PROTOCOL *root =
        (VGK_EFI_FILE_PROTOCOL *)filesystem_context;
    VGK_EFI_FILE_PROTOCOL *file = 0;
    VGK_EFI_CHAR16 firmware_path[VGK_UEFI_PATH_CAPACITY];
    VGK_EFI_STATUS status;

    if (read == 0 || file_context == 0 || root == 0 || root->open == 0 ||
        !valid_manifest_path(path, path_length))
        return -1;

    *read = 0;
    *file_context = 0;

    firmware_path[0] = (VGK_EFI_CHAR16)'\\';
    for (uint16_t i = 0; i < path_length; ++i) {
        uint8_t ch = path[i];
        firmware_path[i + 1U] =
            (VGK_EFI_CHAR16)(ch == '/' ? '\\' : ch);
    }
    firmware_path[path_length + 1U] = 0;

    status = root->open(root, &file, firmware_path,
                        VGK_EFI_FILE_MODE_READ, 0);
    if (status != 0 || file == 0 || file->read == 0 ||
        file->set_position == 0 || file->close == 0) {
        if (file != 0 && file->close != 0)
            (void)file->close(file);
        return -1;
    }

    *read = uefi_file_read_at;
    *file_context = file;
    return 0;
}

void vgk_uefi_manifest_close_file(
    void *filesystem_context,
    void *file_context)
{
    VGK_EFI_FILE_PROTOCOL *file = (VGK_EFI_FILE_PROTOCOL *)file_context;
    (void)filesystem_context;

    if (file != 0 && file->close != 0)
        (void)file->close(file);
}


int vgk_uefi_read_file_bounded(
    VGK_EFI_FILE_PROTOCOL *root_directory,
    const uint8_t *path,
    uint16_t path_length,
    uint8_t *buffer,
    size_t capacity,
    size_t *file_size)
{
    VGK_INTEGRITY_READ read = 0;
    void *file_context = 0;
    size_t total = 0;
    int result = -1;

    if (root_directory == 0 || buffer == 0 || capacity == 0 ||
        file_size == 0)
        return -1;

    *file_size = 0;
    if (vgk_uefi_manifest_open_file(root_directory, path, path_length,
                                    &read, &file_context) != 0)
        return -1;

    while (total < capacity) {
        size_t received = 0;
        if (read(file_context, (uint64_t)total, buffer + total,
                 capacity - total, &received) != 0)
            goto done;
        if (received > capacity - total)
            goto done;
        if (received == 0) {
            *file_size = total;
            result = 0;
            goto done;
        }
        total += received;
    }

    /* Capacity is full: probe one byte to distinguish exact fit from overflow. */
    {
        uint8_t extra;
        size_t received = 0;
        if (read(file_context, (uint64_t)total, &extra, 1U, &received) != 0)
            goto done;
        if (received != 0)
            goto done;
    }

    *file_size = total;
    result = 0;

done:
    vgk_uefi_manifest_close_file(root_directory, file_context);
    return result;
}
