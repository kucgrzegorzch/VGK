#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uefi_page_table_pool.h"

typedef struct {
    void *blocks[8];
    uint64_t pages[8];
    unsigned active;
    unsigned fail_allocations;
    unsigned fail_frees;
} TEST_ALLOCATOR;

static int test_allocate(void *context, uint64_t pages, uint64_t *address)
{
    TEST_ALLOCATOR *allocator = (TEST_ALLOCATOR *)context;
    void *block;
    size_t bytes;

    if (allocator->fail_allocations != 0U) {
        --allocator->fail_allocations;
        return 1;
    }
    if (pages > SIZE_MAX / 4096U || allocator->active >= 8U)
        return 1;
    bytes = (size_t)pages * 4096U;
    block = aligned_alloc(4096U, bytes);
    if (block == 0)
        return 1;

    for (unsigned i = 0; i < 8U; ++i) {
        if (allocator->blocks[i] == 0) {
            allocator->blocks[i] = block;
            allocator->pages[i] = pages;
            ++allocator->active;
            *address = (uint64_t)(uintptr_t)block;
            return 0;
        }
    }
    free(block);
    return 1;
}

static int test_free(void *context, uint64_t address, uint64_t pages)
{
    TEST_ALLOCATOR *allocator = (TEST_ALLOCATOR *)context;

    if (allocator->fail_frees != 0U) {
        --allocator->fail_frees;
        return 1;
    }
    for (unsigned i = 0; i < 8U; ++i) {
        if (allocator->blocks[i] == (void *)(uintptr_t)address) {
            if (allocator->pages[i] != pages)
                return 1;
            free(allocator->blocks[i]);
            allocator->blocks[i] = 0;
            allocator->pages[i] = 0;
            --allocator->active;
            return 0;
        }
    }
    return 1;
}

static int check(int condition, const char *message)
{
    if (!condition)
        printf("FAIL: %s\n", message);
    return condition ? 0 : 1;
}

int main(void)
{
    TEST_ALLOCATOR allocator = {{0}, {0}, 0U, 0U, 0U};
    VGK_UEFI_PAGE_TABLE_POOL pool = {0, 0};
    uint64_t orphan_address = 0;
    uint64_t orphan_pages = 0;
    unsigned failures = 0;

    failures += check(test_allocate(&allocator, 64U, &pool.address) == 0,
                      "initial allocation failed");
    pool.pages = 64U;
    memset((void *)(uintptr_t)pool.address, 0xA5, 64U * 4096U);

    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 0U, 0U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) == VGK_UEFI_PAGE_POOL_INVALID_ARGUMENT &&
        pool.pages == 64U,
        "zero-page growth request was not rejected");

    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 70U, 8U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) == VGK_UEFI_PAGE_POOL_OK,
        "growth from 64 to 128 pages failed");
    failures += check(pool.pages == 128U && allocator.active == 1U,
                      "successful growth did not replace the old allocation");
    failures += check(orphan_address == 0U && orphan_pages == 0U,
                      "successful growth reported an orphan");
    {
        const volatile uint64_t *words =
            (const volatile uint64_t *)(uintptr_t)pool.address;
        const uint64_t count = pool.pages * 512U;
        for (uint64_t i = 0; i < count; ++i) {
            if (words[i] != 0U) {
                failures += check(0, "replacement pool was not fully zeroed");
                break;
            }
        }
    }

    {
        const uint64_t old_address = pool.address;
        failures += check(vgk_uefi_page_table_pool_grow(
            &pool, 100U, 8U, 1024U, test_allocate, test_free, &allocator,
            &orphan_address, &orphan_pages) == VGK_UEFI_PAGE_POOL_OK &&
            pool.address == old_address && allocator.active == 1U,
            "no-op growth changed the allocation");
    }

    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 1025U, 0U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) ==
            VGK_UEFI_PAGE_POOL_LIMIT_EXCEEDED && pool.pages == 128U,
        "request above maximum was not rejected");

    allocator.fail_allocations = 1U;
    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 129U, 8U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) ==
            VGK_UEFI_PAGE_POOL_ALLOCATE_FAILED && pool.pages == 128U &&
            allocator.active == 1U,
        "allocation failure did not preserve the old pool");

    allocator.fail_frees = 1U;
    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 129U, 8U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) ==
            VGK_UEFI_PAGE_POOL_FREE_FAILED && pool.pages == 128U &&
            allocator.active == 1U && orphan_address == 0U,
        "failed release of old pool did not roll back replacement");

    allocator.fail_frees = 2U;
    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 129U, 8U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) ==
            VGK_UEFI_PAGE_POOL_ORPHANED_ALLOCATION &&
            pool.pages == 128U && allocator.active == 2U &&
            orphan_address != 0U && orphan_pages == 256U,
        "unreleased replacement was not reported to the caller");

    if (orphan_address != 0U) {
        failures += check(test_free(&allocator, orphan_address,
                                    orphan_pages) == 0,
                          "test could not release reported orphan");
    }
    failures += check(test_free(&allocator, pool.address, pool.pages) == 0 &&
                      allocator.active == 0U,
                      "final pool cleanup failed");

    failures += check(test_allocate(&allocator, 64U, &pool.address) == 0,
                      "second initial allocation failed");
    pool.pages = 64U;
    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 672U, 0U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) == VGK_UEFI_PAGE_POOL_OK &&
        pool.pages == 672U && allocator.active == 1U,
        "growth to the audited 672-page requirement failed");
    failures += check(test_free(&allocator, pool.address, pool.pages) == 0 &&
                      allocator.active == 0U,
                      "672-page pool cleanup failed");

    failures += check(test_allocate(&allocator, 64U, &pool.address) == 0,
                      "third initial allocation failed");
    pool.pages = 64U;
    failures += check(vgk_uefi_page_table_pool_grow(
        &pool, 100U, 80U, 1024U, test_allocate, test_free, &allocator,
        &orphan_address, &orphan_pages) == VGK_UEFI_PAGE_POOL_OK &&
        pool.pages == 180U && allocator.active == 1U,
        "shared reserve policy was not applied to the replacement size");
    failures += check(test_free(&allocator, pool.address, pool.pages) == 0 &&
                      allocator.active == 0U,
                      "reserve-sized pool cleanup failed");

    if (failures != 0U) {
        printf("UEFI page-table pool tests failed: %u\n", failures);
        return 1;
    }
    puts("UEFI page-table pool tests passed");
    return 0;
}
