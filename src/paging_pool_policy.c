#include "vgk_paging_pool_policy.h"

VGK_PAGING_POOL_POLICY_RESULT vgk_paging_pool_next_size(
    uint64_t current_pages,
    uint64_t required_pages,
    uint64_t reserve_pages,
    uint64_t max_pages,
    uint64_t *next_pages)
{
    uint64_t minimum;
    uint64_t doubled;
    uint64_t candidate;

    if (next_pages == 0 || current_pages == 0 || required_pages == 0 ||
        max_pages == 0 || current_pages > max_pages)
        return VGK_PAGING_POOL_POLICY_INVALID_ARGUMENT;

    *next_pages = current_pages;
    if (required_pages <= current_pages)
        return VGK_PAGING_POOL_POLICY_OK;

    if (required_pages > UINT64_MAX - reserve_pages)
        return VGK_PAGING_POOL_POLICY_LIMIT_REACHED;

    minimum = required_pages + reserve_pages;
    if (minimum > max_pages || current_pages == max_pages)
        return VGK_PAGING_POOL_POLICY_LIMIT_REACHED;

    doubled = current_pages > UINT64_MAX / 2U
        ? UINT64_MAX : current_pages * 2U;
    candidate = minimum > doubled ? minimum : doubled;

    if (candidate > max_pages)
        candidate = max_pages;
    if (candidate <= current_pages)
        return VGK_PAGING_POOL_POLICY_LIMIT_REACHED;

    *next_pages = candidate;
    return VGK_PAGING_POOL_POLICY_GROW;
}
