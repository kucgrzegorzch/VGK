#ifndef VGK_PAGING_POOL_POLICY_H
#define VGK_PAGING_POOL_POLICY_H

#include <stdint.h>

typedef enum {
    VGK_PAGING_POOL_POLICY_OK = 0,
    VGK_PAGING_POOL_POLICY_GROW,
    VGK_PAGING_POOL_POLICY_INVALID_ARGUMENT,
    VGK_PAGING_POOL_POLICY_LIMIT_REACHED
} VGK_PAGING_POOL_POLICY_RESULT;

/*
 * Select the next pool size after preflight reports its requirement.
 * required_pages already includes the candidate pool's own mappings.
 * reserve_pages is additional headroom; max_pages is a hard upper bound.
 */
VGK_PAGING_POOL_POLICY_RESULT vgk_paging_pool_next_size(
    uint64_t current_pages,
    uint64_t required_pages,
    uint64_t reserve_pages,
    uint64_t max_pages,
    uint64_t *next_pages);

#endif
