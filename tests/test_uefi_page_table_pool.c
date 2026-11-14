#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uefi_page_table_pool.h"
#include "vgk_boot_info.h"

typedef struct {
    uint64_t old_base;
    uint64_t new_base;
    uint64_t allocated_pages;
    uint64_t zeroed_base;
    uint64_t zeroed_pages;
    unsigned allocate_calls;
    unsigned zero_calls;
    unsigned release_old_calls;
    unsigned release_new_calls;
    int allocate_result;
    int zero_result;
    int release_old_result;
    int release_new_result;
} FAKE_POOL;

static int fake_allocate(void *context, uint64_t pages, uint64_t *base)
{
    FAKE_POOL *fake = (FAKE_POOL *)context;
    ++fake->allocate_calls;
    fake->allocated_pages = pages;
    *base = fake->new_base;
    return fake->allocate_result;
}

static int fake_release(void *context, uint64_t base, uint64_t pages)
{
    FAKE_POOL *fake = (FAKE_POOL *)context;
    (void)pages;

    if (base == fake->old_base) {
        ++fake->release_old_calls;
        return fake->release_old_result;
    }
    if (base == fake->new_base) {
        ++fake->release_new_calls;
        return fake->release_new_result;
    }
    return -1;
}

static int fake_zero(void *context, uint64_t base, uint64_t pages)
{
    FAKE_POOL *fake = (FAKE_POOL *)context;
    ++fake->zero_calls;
    fake->zeroed_base = base;
    fake->zeroed_pages = pages;
    return fake->zero_result;
}

static VGK_UEFI_PAGE_TABLE_POOL_SERVICES fake_services(FAKE_POOL *fake)
{
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    services.context = fake;
    services.allocate = fake_allocate;
    services.release = fake_release;
    services.zero = fake_zero;
    return services;
}

static void test_success_allocates_zeros_then_releases_old(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_OK);
    assert(base == UINT64_C(0x200000));
    assert(pages == 736U);
    assert(fake.allocate_calls == 1U);
    assert(fake.allocated_pages == 736U);
    assert(fake.zero_calls == 1U);
    assert(fake.zeroed_base == base);
    assert(fake.zeroed_pages == pages);
    assert(fake.release_old_calls == 1U);
    assert(fake.release_new_calls == 0U);
}

static void test_allocation_failure_preserves_old_pool(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    fake.allocate_result = -1;
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_ALLOCATION_FAILED);
    assert(base == UINT64_C(0x100000) && pages == 64U);
    assert(fake.zero_calls == 0U && fake.release_old_calls == 0U);
}

static void test_invalid_alignment_releases_candidate(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200001);
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_INVALID_ALLOCATION);
    assert(base == UINT64_C(0x100000) && pages == 64U);
    assert(fake.release_new_calls == 1U);
    assert(fake.release_old_calls == 0U);
}

static void test_physical_limit_releases_candidate(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = (UINT64_C(1) << 36U) - UINT64_C(0x100000);
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_INVALID_ALLOCATION);
    assert(base == UINT64_C(0x100000) && pages == 64U);
    assert(fake.release_new_calls == 1U);
    assert(fake.release_old_calls == 0U);
}

static void test_zero_failure_releases_candidate(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    fake.zero_result = -1;
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_ZERO_FAILED);
    assert(base == UINT64_C(0x100000) && pages == 64U);
    assert(fake.release_new_calls == 1U);
    assert(fake.release_old_calls == 0U);
}

static void test_old_release_failure_rolls_back_candidate(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    fake.release_old_result = -1;
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_OLD_RELEASE_FAILED);
    assert(base == UINT64_C(0x100000) && pages == 64U);
    assert(fake.release_old_calls == 1U);
    assert(fake.release_new_calls == 1U);
}

static void test_cleanup_failure_is_reported(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    fake.release_old_result = -1;
    fake.release_new_result = -1;
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 672U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_CLEANUP_FAILED);
    assert(base == UINT64_C(0x100000) && pages == 64U);
}

static void test_no_growth_and_limit_paths_do_not_allocate(void)
{
    FAKE_POOL fake;
    VGK_UEFI_PAGE_TABLE_POOL_SERVICES services;
    uint64_t base = UINT64_C(0x100000);
    uint64_t pages = 64U;

    memset(&fake, 0, sizeof(fake));
    fake.old_base = base;
    fake.new_base = UINT64_C(0x200000);
    services = fake_services(&fake);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 60U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_NOT_NEEDED);
    assert(fake.allocate_calls == 0U);

    base = UINT64_C(0x100000);
    pages = VGK_BOOT_PAGE_TABLE_POOL_MAX_PAGES;
    fake.old_base = base;
    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 1024U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_NOT_NEEDED);
    assert(fake.allocate_calls == 0U);

    assert(vgk_uefi_page_table_pool_grow(
               &services, &base, &pages, 1025U, 64U, 36U) ==
           VGK_UEFI_POOL_GROW_LIMIT_REACHED);
    assert(fake.allocate_calls == 0U);
}

int main(void)
{
    test_success_allocates_zeros_then_releases_old();
    test_allocation_failure_preserves_old_pool();
    test_invalid_alignment_releases_candidate();
    test_physical_limit_releases_candidate();
    test_zero_failure_releases_candidate();
    test_old_release_failure_rolls_back_candidate();
    test_cleanup_failure_is_reported();
    test_no_growth_and_limit_paths_do_not_allocate();
    puts("UEFI page-table pool growth tests: OK");
    return 0;
}
