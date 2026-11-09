#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vgk_paging.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static uint64_t *walk(VGK_PAGE_TABLES *t, uint64_t va)
{
    const unsigned shifts[3] = {39U, 30U, 21U};
    uint64_t *p = t->pool;
    for (size_t l = 0; l < 3; ++l) {
        uint64_t e = p[(va >> shifts[l]) & 0x1FFU];
        if ((e & 1U) == 0U || (e & 0x80U) != 0U) return 0;
        p = (uint64_t *)(uintptr_t)(e & t->physical_mask);
    }
    return &p[(va >> 12) & 0x1FFU];
}

int main(void)
{
    VGK_PAGE_TABLES t;
    void *pool = 0;
    CHECK(posix_memalign(&pool, 4096U, 8U * 4096U) == 0);
    memset(pool, 0xA5, 8U * 4096U);
    CHECK(vgk_paging_init(&t, pool, 8U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_root_physical(&t) == (uint64_t)(uintptr_t)pool);
    CHECK(vgk_paging_map(&t, 0x200000U, 0x200000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x201000U, 0x201000U,
                         VGK_PAGE_WRITE) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x202000U, 0x202000U,
                         VGK_PAGE_WRITE | VGK_PAGE_EXEC) ==
          VGK_PAGING_UNSUPPORTED);
    CHECK(vgk_paging_map(&t, 0x200000U, 0x300000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_ALREADY_MAPPED);
    CHECK(((*walk(&t, 0x200000U)) & (UINT64_C(1) << 63)) == 0U);
    CHECK(((*walk(&t, 0x200000U)) & 2U) == 0U);
    CHECK(((*walk(&t, 0x201000U)) & (1U << 63)) != 0U);
    CHECK(((*walk(&t, 0x201000U)) & 2U) != 0U);
    CHECK(vgk_paging_map(&t, UINT64_C(0x0000800000000000), 0x300000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    free(pool);

    CHECK(posix_memalign(&pool, 4096U, 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 1U, 48U, 0U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x400000U, 0x400000U,
                         VGK_PAGE_WRITE) == VGK_PAGING_UNSUPPORTED);
    free(pool);
    puts("OK: czteropoziomowe tablice stron – uprawnienia, NX, W^X i walidacja adresow.");
    return 0;
}
