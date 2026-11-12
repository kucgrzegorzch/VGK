#include "vgk_paging_requirements.h"

#define PAGE_SIZE UINT64_C(4096)

typedef struct {
    uint64_t first;
    uint64_t last;
} PREFIX_INTERVAL;

static int canonical_address(uint64_t address)
{
    uint64_t upper = address >> 48;
    uint64_t sign = (address >> 47) & 1U;
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

static void sort_intervals(PREFIX_INTERVAL *intervals, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        PREFIX_INTERVAL value = intervals[i];
        size_t j = i;
        while (j > 0U && intervals[j - 1U].first > value.first) {
            intervals[j] = intervals[j - 1U];
            --j;
        }
        intervals[j] = value;
    }
}

/* Counts the union of inclusive, sorted prefix-key intervals. */
static uint64_t count_interval_union(
    PREFIX_INTERVAL *intervals,
    size_t count)
{
    uint64_t total = 0;
    uint64_t current_first;
    uint64_t current_last;

    if (count == 0U)
        return 0;

    sort_intervals(intervals, count);
    current_first = intervals[0].first;
    current_last = intervals[0].last;

    for (size_t i = 1U; i < count; ++i) {
        const PREFIX_INTERVAL next = intervals[i];
        if (next.first <= current_last ||
            next.first == current_last + 1U) {
            if (next.last > current_last)
                current_last = next.last;
        } else {
            total += current_last - current_first + 1U;
            current_first = next.first;
            current_last = next.last;
        }
    }

    total += current_last - current_first + 1U;
    return total;
}

VGK_PAGING_REQUIREMENTS_RESULT vgk_paging_requirements_calculate(
    const VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t range_count,
    VGK_PAGING_REQUIREMENTS *requirements)
{
    PREFIX_INTERVAL pdpt_intervals[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
    PREFIX_INTERVAL pd_intervals[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
    PREFIX_INTERVAL pt_intervals[VGK_PAGING_REQUIREMENTS_MAX_RANGES];

    if (requirements == 0)
        return VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT;

    requirements->pml4_pages = 0;
    requirements->pdpt_pages = 0;
    requirements->pd_pages = 0;
    requirements->pt_pages = 0;
    requirements->total_pages = 0;

    if (range_count > VGK_PAGING_REQUIREMENTS_MAX_RANGES)
        return VGK_PAGING_REQUIREMENTS_TOO_MANY_RANGES;
    if (range_count != 0U && ranges == 0)
        return VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT;

    for (size_t i = 0; i < range_count; ++i) {
        uint64_t rounded_length;
        uint64_t last_address;
        uint64_t first_page;
        uint64_t last_page;

        if (ranges[i].length == 0U ||
            (ranges[i].virtual_address & (PAGE_SIZE - 1U)) != 0U ||
            ranges[i].length > UINT64_MAX - (PAGE_SIZE - 1U))
            return VGK_PAGING_REQUIREMENTS_BAD_RANGE;

        rounded_length =
            (ranges[i].length + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
        if (rounded_length == 0U ||
            ranges[i].virtual_address > UINT64_MAX - (rounded_length - 1U))
            return VGK_PAGING_REQUIREMENTS_BAD_RANGE;

        last_address = ranges[i].virtual_address + rounded_length - 1U;
        if (!canonical_address(ranges[i].virtual_address) ||
            !canonical_address(last_address) ||
            (((ranges[i].virtual_address >> 47) & 1U) !=
             ((last_address >> 47) & 1U)))
            return VGK_PAGING_REQUIREMENTS_BAD_RANGE;

        first_page = ranges[i].virtual_address;
        last_page = last_address;

        /*
         * Mask away canonical sign-extension bits. These are actual paging
         * indices/prefixes, so low and high canonical addresses are counted
         * by their 9-, 18- and 27-bit index paths rather than raw VA>>shift.
         */
        pdpt_intervals[i].first =
            (first_page >> 39U) & UINT64_C(0x1FF);
        pdpt_intervals[i].last =
            (last_page >> 39U) & UINT64_C(0x1FF);
        pd_intervals[i].first =
            (first_page >> 30U) & UINT64_C(0x3FFFF);
        pd_intervals[i].last =
            (last_page >> 30U) & UINT64_C(0x3FFFF);
        pt_intervals[i].first =
            (first_page >> 21U) & UINT64_C(0x7FFFFFF);
        pt_intervals[i].last =
            (last_page >> 21U) & UINT64_C(0x7FFFFFF);

        if (pdpt_intervals[i].last < pdpt_intervals[i].first ||
            pd_intervals[i].last < pd_intervals[i].first ||
            pt_intervals[i].last < pt_intervals[i].first)
            return VGK_PAGING_REQUIREMENTS_BAD_RANGE;
    }

    requirements->pml4_pages = 1U;
    requirements->pdpt_pages =
        count_interval_union(pdpt_intervals, range_count);
    requirements->pd_pages =
        count_interval_union(pd_intervals, range_count);
    requirements->pt_pages =
        count_interval_union(pt_intervals, range_count);

    requirements->total_pages =
        requirements->pml4_pages + requirements->pdpt_pages +
        requirements->pd_pages + requirements->pt_pages;
    return VGK_PAGING_REQUIREMENTS_OK;
}
