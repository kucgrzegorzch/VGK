#include "vgk_paging.h"

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_WRITE   UINT64_C(0x002)
#define PTE_LARGE   UINT64_C(0x080)
#define PTE_NX      (UINT64_C(1) << 63)
#define PTE_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

static int canonical_address(uint64_t address)
{
    uint64_t upper = address >> 48;
    uint64_t sign = (address >> 47) & 1U;
    return sign ? upper == UINT64_C(0xFFFF) : upper == 0;
}

static uint64_t *table_from_entry(const VGK_PAGE_TABLES *tables, uint64_t entry)
{
    uint64_t address = entry & tables->physical_mask;
    return (uint64_t *)(uintptr_t)address;
}

static uint64_t *allocate_table(VGK_PAGE_TABLES *tables)
{
    uint64_t *page;
    uint64_t address;

    if (tables->used_pages >= tables->pool_pages)
        return 0;
    page = tables->pool + tables->used_pages * 512U;
    ++tables->used_pages;
    address = (uint64_t)(uintptr_t)page;
    if ((address & (VGK_PAGING_PAGE_SIZE - 1U)) != 0 ||
        (address & ~tables->physical_mask) != 0)
        return 0;
    for (size_t i = 0; i < 512U; ++i)
        page[i] = 0;
    return page;
}

VGK_PAGING_RESULT vgk_paging_init(
    VGK_PAGE_TABLES *tables,
    void *page_pool,
    size_t pool_pages,
    uint8_t physical_bits,
    uint8_t nx_supported)
{
    uint64_t address;

    if (tables == 0 || page_pool == 0 || pool_pages == 0)
        return VGK_PAGING_INVALID_ARGUMENT;
    tables->pool = 0;
    tables->pool_pages = 0;
    tables->used_pages = 0;
    tables->physical_mask = 0;
    tables->physical_bits = 0;
    tables->nx_supported = 0;

    if (((uintptr_t)page_pool & (VGK_PAGING_PAGE_SIZE - 1U)) != 0 ||
        physical_bits < 32U || physical_bits > VGK_PAGING_MAX_PHYSICAL_BITS ||
        nx_supported > 1U ||
        pool_pages > SIZE_MAX / (size_t)VGK_PAGING_PAGE_SIZE)
        return VGK_PAGING_INVALID_ARGUMENT;

    address = (uint64_t)(uintptr_t)page_pool;
    tables->physical_mask =
        ((UINT64_C(1) << physical_bits) - 1U) &
        ~(VGK_PAGING_PAGE_SIZE - 1U);
    if ((address & ~tables->physical_mask) != 0 ||
        pool_pages * (size_t)VGK_PAGING_PAGE_SIZE >
            (size_t)(tables->physical_mask - address + VGK_PAGING_PAGE_SIZE))
        return VGK_PAGING_BAD_ADDRESS;

    tables->pool = (uint64_t *)page_pool;
    tables->pool_pages = pool_pages;
    tables->physical_bits = physical_bits;
    tables->nx_supported = nx_supported;
    if (allocate_table(tables) == 0) {
        tables->pool = 0;
        tables->pool_pages = 0;
        tables->used_pages = 0;
        return VGK_PAGING_BAD_ADDRESS;
    }
    return VGK_PAGING_OK;
}

VGK_PAGING_RESULT vgk_paging_map(
    VGK_PAGE_TABLES *tables,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint32_t permissions)
{
    static const unsigned shifts[4] = {39U, 30U, 21U, 12U};
    uint64_t *current;
    uint64_t leaf_flags = PTE_PRESENT;
    uint64_t physical_limit;

    if (tables == 0 || tables->pool == 0)
        return VGK_PAGING_INVALID_ARGUMENT;
    if ((virtual_address & (VGK_PAGING_PAGE_SIZE - 1U)) != 0 ||
        (physical_address & (VGK_PAGING_PAGE_SIZE - 1U)) != 0 ||
        (permissions & ~(VGK_PAGE_WRITE | VGK_PAGE_EXEC)) != 0 ||
        !canonical_address(virtual_address))
        return VGK_PAGING_BAD_ADDRESS;
    physical_limit = tables->physical_mask | (VGK_PAGING_PAGE_SIZE - 1U);
    if (physical_address > physical_limit)
        return VGK_PAGING_BAD_ADDRESS;
    if ((permissions & (VGK_PAGE_WRITE | VGK_PAGE_EXEC)) ==
        (VGK_PAGE_WRITE | VGK_PAGE_EXEC))
        return VGK_PAGING_UNSUPPORTED;
    if ((permissions & VGK_PAGE_EXEC) == 0U) {
        if (tables->nx_supported == 0U)
            return VGK_PAGING_UNSUPPORTED;
        leaf_flags |= PTE_NX;
    }
    if ((permissions & VGK_PAGE_WRITE) != 0U)
        leaf_flags |= PTE_WRITE;

    current = tables->pool;
    for (size_t level = 0; level < 3U; ++level) {
        size_t index = (size_t)((virtual_address >> shifts[level]) & 0x1FFU);
        uint64_t entry = current[index];

        if ((entry & PTE_PRESENT) == 0U) {
            uint64_t *next = allocate_table(tables);
            uint64_t next_address;
            if (next == 0)
                return VGK_PAGING_NO_MEMORY;
            next_address = (uint64_t)(uintptr_t)next;
            current[index] = (next_address & tables->physical_mask) |
                             PTE_PRESENT | PTE_WRITE;
            current = next;
        } else {
            if ((entry & PTE_LARGE) != 0U)
                return VGK_PAGING_UNSUPPORTED;
            current = table_from_entry(tables, entry);
        }
    }

    {
        size_t index = (size_t)((virtual_address >> shifts[3]) & 0x1FFU);
        if ((current[index] & PTE_PRESENT) != 0U)
            return VGK_PAGING_ALREADY_MAPPED;
        current[index] = (physical_address & tables->physical_mask) | leaf_flags;
    }
    return VGK_PAGING_OK;
}

uint64_t vgk_paging_root_physical(const VGK_PAGE_TABLES *tables)
{
    if (tables == 0 || tables->pool == 0)
        return 0;
    return (uint64_t)(uintptr_t)tables->pool;
}
