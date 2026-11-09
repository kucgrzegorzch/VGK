#include "vgk_memory.h"

#define VGK_MEMORY_MIN_ALLOCATABLE_ADDRESS UINT64_C(0x00100000)

typedef struct {
    uint64_t base;
    uint64_t page_count;
} VGK_MEMORY_RANGE;

static VGK_MEMORY_RANGE managed_ranges[VGK_MEMORY_MAX_RANGES];
static VGK_MEMORY_RANGE free_ranges[VGK_MEMORY_MAX_RANGES];
static size_t managed_range_count;
static size_t free_range_count;
static uint64_t total_pages;
static uint64_t available_pages;
static int allocator_initialized;

static int is_power_of_two(uint64_t value)
{
    return value != 0 && (value & (value - 1U)) == 0;
}

static int range_end(
    const VGK_MEMORY_RANGE *range,
    uint64_t *end)
{
    uint64_t bytes;

    if (range == 0 || end == 0 || range->page_count == 0 ||
        range->page_count > UINT64_MAX / VGK_MEMORY_PAGE_SIZE)
        return -1;

    bytes = range->page_count * VGK_MEMORY_PAGE_SIZE;
    if (range->base > UINT64_MAX - bytes)
        return -1;

    *end = range->base + bytes;
    return 0;
}

static void clear_allocator(void)
{
    managed_range_count = 0;
    free_range_count = 0;
    total_pages = 0;
    available_pages = 0;
    allocator_initialized = 0;
}

/* Add a usable interval, merging it with the previous interval if adjacent. */
static VGK_MEMORY_RESULT append_usable_range(
    uint64_t base,
    uint64_t page_count)
{
    uint64_t previous_end;

    if (page_count == 0)
        return VGK_MEMORY_OK;

    if (managed_range_count != 0) {
        VGK_MEMORY_RANGE *previous =
            &managed_ranges[managed_range_count - 1U];
        if (range_end(previous, &previous_end) != 0)
            return VGK_MEMORY_OVERFLOW;

        if (previous_end == base) {
            if (previous->page_count > UINT64_MAX - page_count)
                return VGK_MEMORY_OVERFLOW;
            previous->page_count += page_count;
            free_ranges[free_range_count - 1U].page_count += page_count;
            return VGK_MEMORY_OK;
        }
    }

    if (managed_range_count >= VGK_MEMORY_MAX_RANGES ||
        free_range_count >= VGK_MEMORY_MAX_RANGES)
        return VGK_MEMORY_RANGE_CAPACITY;

    managed_ranges[managed_range_count].base = base;
    managed_ranges[managed_range_count].page_count = page_count;
    ++managed_range_count;

    free_ranges[free_range_count].base = base;
    free_ranges[free_range_count].page_count = page_count;
    ++free_range_count;

    return VGK_MEMORY_OK;
}

typedef struct {
    uint64_t base;
    uint64_t end;
} VGK_RESERVED_RANGE;

static int reserve_range(
    VGK_RESERVED_RANGE *ranges,
    size_t *count,
    size_t capacity,
    uint64_t base,
    uint64_t length)
{
    uint64_t end;
    uint64_t aligned_end;

    if (length == 0)
        return 0;
    if (base > UINT64_MAX - length)
        return -1;
    end = base + length;
    if (end > UINT64_MAX - (VGK_MEMORY_PAGE_SIZE - 1U))
        return -1;
    aligned_end = (end + VGK_MEMORY_PAGE_SIZE - 1U) &
                  ~(VGK_MEMORY_PAGE_SIZE - 1U);
    base &= ~(VGK_MEMORY_PAGE_SIZE - 1U);
    if (base >= aligned_end)
        return -1;
    if (*count >= capacity)
        return -1;
    ranges[*count].base = base;
    ranges[*count].end = aligned_end;
    ++*count;
    return 0;
}

/* Add usable memory around reserved physical intervals, never inside them. */
static VGK_MEMORY_RESULT append_usable_excluding(
    uint64_t base,
    uint64_t end,
    const VGK_RESERVED_RANGE *reserved,
    size_t reserved_count)
{
    uint64_t cursor = base;

    for (size_t i = 0; i < reserved_count; ++i) {
        uint64_t cut_start = reserved[i].base;
        uint64_t cut_end = reserved[i].end;
        VGK_MEMORY_RESULT result;

        if (cut_end <= cursor || cut_start >= end)
            continue;
        if (cut_start > cursor) {
            uint64_t gap_pages = (cut_start - cursor) /
                                 VGK_MEMORY_PAGE_SIZE;
            result = append_usable_range(cursor, gap_pages);
            if (result != VGK_MEMORY_OK)
                return result;
            if (total_pages > UINT64_MAX - gap_pages)
                return VGK_MEMORY_OVERFLOW;
            total_pages += gap_pages;
        }
        if (cut_end > cursor)
            cursor = cut_end;
        if (cursor >= end)
            break;
    }

    if (cursor < end) {
        uint64_t gap_pages = (end - cursor) / VGK_MEMORY_PAGE_SIZE;
        VGK_MEMORY_RESULT result =
            append_usable_range(cursor, gap_pages);
        if (result != VGK_MEMORY_OK)
            return result;
        if (total_pages > UINT64_MAX - gap_pages)
            return VGK_MEMORY_OVERFLOW;
        total_pages += gap_pages;
    }
    return VGK_MEMORY_OK;
}

VGK_MEMORY_RESULT vgk_memory_init(const VGK_BOOT_INFO *boot_info)
{
    const VGK_MEMORY_MAP_ENTRY *entries;
    size_t entry_count;
    uint64_t previous_end = 0;
    VGK_RESERVED_RANGE reserved[VGK_BOOT_MAX_KERNEL_SEGMENTS + 2U];
    size_t reserved_count = 0;

    clear_allocator();

    if (boot_info == 0 || !vgk_boot_info_is_valid(boot_info))
        return VGK_MEMORY_INVALID_ARGUMENT;

    entry_count = (size_t)(
        boot_info->memory_map_size / sizeof(VGK_MEMORY_MAP_ENTRY));
    if (entry_count == 0 || entry_count > VGK_MEMORY_MAX_RANGES)
        return VGK_MEMORY_BAD_MAP;

    entries = (const VGK_MEMORY_MAP_ENTRY *)(uintptr_t)
        boot_info->memory_map_address;

    if (reserve_range(reserved, &reserved_count,
                      sizeof(reserved) / sizeof(reserved[0]),
                      (uint64_t)(uintptr_t)boot_info,
                      sizeof(*boot_info)) != 0 ||
        reserve_range(reserved, &reserved_count,
                      sizeof(reserved) / sizeof(reserved[0]),
                      boot_info->memory_map_address,
                      boot_info->memory_map_size) != 0) {
        clear_allocator();
        return VGK_MEMORY_BAD_MAP;
    }
    for (uint32_t i = 0; i < boot_info->kernel_segment_count; ++i) {
        const VGK_KERNEL_SEGMENT *segment = &boot_info->kernel_segments[i];
        if (reserve_range(reserved, &reserved_count,
                          sizeof(reserved) / sizeof(reserved[0]),
                          segment->physical_base,
                          segment->memory_size) != 0) {
            clear_allocator();
            return VGK_MEMORY_BAD_MAP;
        }
    }
    /* The exclusions are sorted to make interval subtraction deterministic. */
    for (size_t i = 1; i < reserved_count; ++i) {
        VGK_RESERVED_RANGE item = reserved[i];
        size_t j = i;
        while (j != 0 && reserved[j - 1U].base > item.base) {
            reserved[j] = reserved[j - 1U];
            --j;
        }
        reserved[j] = item;
    }

    for (size_t i = 0; i < entry_count; ++i) {
        const VGK_MEMORY_MAP_ENTRY *entry = &entries[i];
        uint64_t end;
        uint64_t page_count;
        VGK_MEMORY_RESULT result;

        if (entry->length == 0 ||
            entry->base > UINT64_MAX - entry->length ||
            (entry->base & (VGK_MEMORY_PAGE_SIZE - 1U)) != 0 ||
            (entry->length & (VGK_MEMORY_PAGE_SIZE - 1U)) != 0) {
            clear_allocator();
            return VGK_MEMORY_BAD_MAP;
        }

        end = entry->base + entry->length;
        if (i != 0 && entry->base < previous_end) {
            clear_allocator();
            return VGK_MEMORY_BAD_MAP;
        }
        previous_end = end;

        if (entry->type != VGK_MEMORY_USABLE ||
            end <= VGK_MEMORY_MIN_ALLOCATABLE_ADDRESS)
            continue;

        {
            uint64_t usable_base = entry->base;
            if (usable_base < VGK_MEMORY_MIN_ALLOCATABLE_ADDRESS)
                usable_base = VGK_MEMORY_MIN_ALLOCATABLE_ADDRESS;
            result = append_usable_excluding(
                usable_base, end, reserved, reserved_count);
        }
        if (result != VGK_MEMORY_OK) {
            clear_allocator();
            return result;
        }
    }

    available_pages = total_pages;
    allocator_initialized = 1;
    return VGK_MEMORY_OK;
}

VGK_MEMORY_RESULT vgk_memory_alloc_pages(
    uint64_t page_count,
    uint64_t alignment_pages,
    uint64_t *physical_address)
{
    uint64_t alignment_bytes;
    uint64_t allocation_bytes;

    if (physical_address == 0)
        return VGK_MEMORY_INVALID_ARGUMENT;
    *physical_address = 0;

    if (!allocator_initialized)
        return VGK_MEMORY_NOT_INITIALIZED;
    if (page_count == 0 || !is_power_of_two(alignment_pages) ||
        alignment_pages > UINT64_MAX / VGK_MEMORY_PAGE_SIZE ||
        page_count > UINT64_MAX / VGK_MEMORY_PAGE_SIZE)
        return VGK_MEMORY_INVALID_ARGUMENT;

    if (page_count > available_pages)
        return VGK_MEMORY_NO_MEMORY;

    alignment_bytes = alignment_pages * VGK_MEMORY_PAGE_SIZE;
    allocation_bytes = page_count * VGK_MEMORY_PAGE_SIZE;

    for (size_t i = 0; i < free_range_count; ++i) {
        VGK_MEMORY_RANGE current = free_ranges[i];
        uint64_t current_end;
        uint64_t aligned_base;
        uint64_t allocation_end;
        uint64_t before_pages;
        uint64_t after_pages;

        if (range_end(&current, &current_end) != 0)
            return VGK_MEMORY_BAD_MAP;
        if (current.base > UINT64_MAX - (alignment_bytes - 1U))
            continue;

        aligned_base =
            (current.base + alignment_bytes - 1U) & ~(alignment_bytes - 1U);
        if (aligned_base < current.base || aligned_base > current_end ||
            allocation_bytes > current_end - aligned_base)
            continue;

        allocation_end = aligned_base + allocation_bytes;
        before_pages =
            (aligned_base - current.base) / VGK_MEMORY_PAGE_SIZE;
        after_pages =
            (current_end - allocation_end) / VGK_MEMORY_PAGE_SIZE;

        if (before_pages != 0 && after_pages != 0) {
            if (free_range_count >= VGK_MEMORY_MAX_RANGES)
                return VGK_MEMORY_RANGE_CAPACITY;

            for (size_t j = free_range_count; j > i + 1U; --j)
                free_ranges[j] = free_ranges[j - 1U];

            free_ranges[i].page_count = before_pages;
            free_ranges[i + 1U].base = allocation_end;
            free_ranges[i + 1U].page_count = after_pages;
            ++free_range_count;
        } else if (before_pages != 0) {
            free_ranges[i].page_count = before_pages;
        } else if (after_pages != 0) {
            free_ranges[i].base = allocation_end;
            free_ranges[i].page_count = after_pages;
        } else {
            for (size_t j = i + 1U; j < free_range_count; ++j)
                free_ranges[j - 1U] = free_ranges[j];
            --free_range_count;
        }

        available_pages -= page_count;
        *physical_address = aligned_base;
        return VGK_MEMORY_OK;
    }

    return VGK_MEMORY_NO_MEMORY;
}

VGK_MEMORY_RESULT vgk_memory_free_pages(
    uint64_t physical_address,
    uint64_t page_count)
{
    uint64_t bytes;
    uint64_t end;
    size_t insert_at = 0;
    size_t managed_index = 0;
    int belongs_to_managed_range = 0;
    int merge_left;
    int merge_right;

    if (!allocator_initialized)
        return VGK_MEMORY_NOT_INITIALIZED;
    if (physical_address == 0 || page_count == 0 ||
        (physical_address & (VGK_MEMORY_PAGE_SIZE - 1U)) != 0 ||
        page_count > UINT64_MAX / VGK_MEMORY_PAGE_SIZE)
        return VGK_MEMORY_INVALID_ARGUMENT;

    if (page_count > total_pages)
        return VGK_MEMORY_OUT_OF_RANGE;

    bytes = page_count * VGK_MEMORY_PAGE_SIZE;
    if (physical_address > UINT64_MAX - bytes)
        return VGK_MEMORY_OVERFLOW;
    end = physical_address + bytes;

    for (managed_index = 0;
         managed_index < managed_range_count; ++managed_index) {
        uint64_t managed_end;
        if (range_end(&managed_ranges[managed_index], &managed_end) != 0)
            return VGK_MEMORY_BAD_MAP;
        if (physical_address >= managed_ranges[managed_index].base &&
            end <= managed_end) {
            belongs_to_managed_range = 1;
            break;
        }
    }
    if (!belongs_to_managed_range)
        return VGK_MEMORY_OUT_OF_RANGE;

    while (insert_at < free_range_count &&
           free_ranges[insert_at].base < physical_address)
        ++insert_at;

    if (insert_at != 0) {
        uint64_t previous_end;
        if (range_end(&free_ranges[insert_at - 1U], &previous_end) != 0)
            return VGK_MEMORY_BAD_MAP;
        if (previous_end > physical_address)
            return VGK_MEMORY_DOUBLE_FREE;
    }
    if (insert_at < free_range_count &&
        end > free_ranges[insert_at].base)
        return VGK_MEMORY_DOUBLE_FREE;

    if (page_count > total_pages ||
        available_pages > total_pages - page_count)
        return VGK_MEMORY_BAD_MAP;

    merge_left = 0;
    merge_right = 0;
    if (insert_at != 0) {
        uint64_t previous_end;
        if (range_end(&free_ranges[insert_at - 1U], &previous_end) != 0)
            return VGK_MEMORY_BAD_MAP;
        merge_left = previous_end == physical_address;
    }
    if (insert_at < free_range_count)
        merge_right = end == free_ranges[insert_at].base;

    if (merge_left && merge_right) {
        if (free_ranges[insert_at - 1U].page_count >
            UINT64_MAX - page_count - free_ranges[insert_at].page_count)
            return VGK_MEMORY_OVERFLOW;
        free_ranges[insert_at - 1U].page_count +=
            page_count + free_ranges[insert_at].page_count;
        for (size_t j = insert_at + 1U; j < free_range_count; ++j)
            free_ranges[j - 1U] = free_ranges[j];
        --free_range_count;
    } else if (merge_left) {
        if (free_ranges[insert_at - 1U].page_count >
            UINT64_MAX - page_count)
            return VGK_MEMORY_OVERFLOW;
        free_ranges[insert_at - 1U].page_count += page_count;
    } else if (merge_right) {
        if (free_ranges[insert_at].page_count >
            UINT64_MAX - page_count)
            return VGK_MEMORY_OVERFLOW;
        free_ranges[insert_at].base = physical_address;
        free_ranges[insert_at].page_count += page_count;
    } else {
        if (free_range_count >= VGK_MEMORY_MAX_RANGES)
            return VGK_MEMORY_RANGE_CAPACITY;
        for (size_t j = free_range_count; j > insert_at; --j)
            free_ranges[j] = free_ranges[j - 1U];
        free_ranges[insert_at].base = physical_address;
        free_ranges[insert_at].page_count = page_count;
        ++free_range_count;
    }

    available_pages += page_count;
    return VGK_MEMORY_OK;
}

VGK_MEMORY_RESULT vgk_memory_get_stats(VGK_MEMORY_STATS *stats)
{
    if (stats == 0)
        return VGK_MEMORY_INVALID_ARGUMENT;
    if (!allocator_initialized)
        return VGK_MEMORY_NOT_INITIALIZED;

    stats->total_pages = total_pages;
    stats->free_pages = available_pages;
    stats->managed_range_count = managed_range_count;
    stats->free_range_count = free_range_count;
    return VGK_MEMORY_OK;
}
