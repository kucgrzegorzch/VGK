#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "vgk_paging_pool_policy.h"

static void test_sufficient_pool_is_unchanged(void)
{
    uint64_t next = 0;
    assert(vgk_paging_pool_next_size(64, 60, 8, 4096, &next) ==
           VGK_PAGING_POOL_POLICY_OK);
    assert(next == 64);
}

static void test_growth_uses_geometric_and_reserve_target(void)
{
    uint64_t next = 0;
    assert(vgk_paging_pool_next_size(64, 70, 8, 4096, &next) ==
           VGK_PAGING_POOL_POLICY_GROW);
    assert(next == 128);

    assert(vgk_paging_pool_next_size(64, 100, 80, 4096, &next) ==
           VGK_PAGING_POOL_POLICY_GROW);
    assert(next == 180);
}

static void test_growth_is_capped_without_exceeding_limit(void)
{
    uint64_t next = 0;
    assert(vgk_paging_pool_next_size(64, 70, 8, 100, &next) ==
           VGK_PAGING_POOL_POLICY_GROW);
    assert(next == 100);
}

static void test_required_capacity_over_limit_fails(void)
{
    uint64_t next = 0;
    assert(vgk_paging_pool_next_size(64, 95, 8, 100, &next) ==
           VGK_PAGING_POOL_POLICY_LIMIT_REACHED);
    assert(next == 64);
}

static void test_overflow_and_invalid_arguments(void)
{
    uint64_t next = 0;
    assert(vgk_paging_pool_next_size(64, UINT64_MAX, 1, UINT64_MAX,
                                    &next) ==
           VGK_PAGING_POOL_POLICY_LIMIT_REACHED);
    assert(vgk_paging_pool_next_size(0, 1, 0, 100, &next) ==
           VGK_PAGING_POOL_POLICY_INVALID_ARGUMENT);
    assert(vgk_paging_pool_next_size(10, 1, 0, 9, &next) ==
           VGK_PAGING_POOL_POLICY_INVALID_ARGUMENT);
    assert(vgk_paging_pool_next_size(10, 11, 0, 100, 0) ==
           VGK_PAGING_POOL_POLICY_INVALID_ARGUMENT);
}

int main(void)
{
    test_sufficient_pool_is_unchanged();
    test_growth_uses_geometric_and_reserve_target();
    test_growth_is_capped_without_exceeding_limit();
    test_required_capacity_over_limit_fails();
    test_overflow_and_invalid_arguments();
    puts("paging pool growth policy tests: OK");
    return 0;
}
