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
    CHECK(((*walk(&t, 0x200000U)) & UINT64_C(0x8000000000000000)) == 0U);
    CHECK(((*walk(&t, 0x200000U)) & 2U) == 0U);
    CHECK(((*walk(&t, 0x201000U)) & (UINT64_C(1) << 63)) != 0U);
    CHECK(((*walk(&t, 0x201000U)) & 2U) != 0U);
    /* A physical page cannot become writable through an executable alias. */
    CHECK(vgk_paging_map(&t, 0x203000U, 0x200000U,
                         VGK_PAGE_WRITE) == VGK_PAGING_UNSUPPORTED);
    CHECK(vgk_paging_map(&t, 0x203000U, 0x200000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    /* Nor may a writable physical page gain an executable alias. */
    CHECK(vgk_paging_map(&t, 0x204000U, 0x201000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);
    CHECK(vgk_paging_map(&t, UINT64_C(0x0000800000000000), 0x300000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_map(&t, 0x300000U, UINT64_C(0x0001000000000000),
                         VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_map(&t, UINT64_C(0xFFFF800000000000), 0x400000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    CHECK(((*walk(&t, UINT64_C(0xFFFF800000000000))) & 1U) != 0U);
    CHECK(vgk_paging_map_range(&t, 0x400000U, 0x500000U, 4097U,
                               VGK_PAGE_WRITE) == VGK_PAGING_OK);
    CHECK(((*walk(&t, 0x400000U)) & 2U) != 0U);
    CHECK(((*walk(&t, 0x401000U)) & 2U) != 0U);
    CHECK(vgk_paging_map_range(&t, UINT64_C(0x00007FFFFFFFF000),
                               UINT64_C(0x600000), 8192U, VGK_PAGE_EXEC) ==
          VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_map_range(&t, 0x600000U,
                               UINT64_C(0x0000FFFFFFFFF000), 8192U,
                               VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    free(pool);

    CHECK(posix_memalign(&pool, 4096U, 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 1U, 48U, 0U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x400000U, 0x400000U,
                         VGK_PAGE_WRITE) == VGK_PAGING_UNSUPPORTED);
    CHECK(vgk_paging_init(&t, pool, 1U, 31U, 1U) ==
          VGK_PAGING_INVALID_ARGUMENT);
    CHECK(vgk_paging_init(&t,
          (void *)(uintptr_t)UINT64_C(0x0001000000000000),
          1U, 48U, 1U) == VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_init(&t,
          (void *)(uintptr_t)UINT64_C(0x0000FFFFFFFFF000),
          2U, 48U, 1U) == VGK_PAGING_BAD_ADDRESS);
    free(pool);

    CHECK(posix_memalign(&pool, 4096U, 4U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 4U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x200000U, 0x200000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    CHECK(t.used_pages == 4U);
    CHECK(vgk_paging_map(&t, UINT64_C(0x0000008000000000),
                         UINT64_C(0x80000000), VGK_PAGE_EXEC) ==
          VGK_PAGING_NO_MEMORY);
    free(pool);
    /* Range mapping must report partial exhaustion without hiding it. */
    CHECK(posix_memalign(&pool, 4096U, 4U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 4U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map_range(&t, 0x1FF000U, 0x5FF000U, 8192U,
                               VGK_PAGE_EXEC) == VGK_PAGING_NO_MEMORY);
    CHECK((*walk(&t, 0x1FF000U) & 1U) != 0U);
    CHECK(walk(&t, 0x200000U) == 0);
    free(pool);

    CHECK(posix_memalign(&pool, 4096U, 8U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 8U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map_range(&t, 0x400001U, 0x500000U, 4096U,
                               VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_map_range(&t, 0x400000U, 0x500000U, 0U,
                               VGK_PAGE_EXEC) == VGK_PAGING_INVALID_ARGUMENT);
    CHECK(vgk_paging_map_range(&t, UINT64_C(0xFFFFFFFFFFFFF000),
                               0x600000U, 8192U, VGK_PAGE_EXEC) ==
          VGK_PAGING_BAD_ADDRESS);
    CHECK(vgk_paging_map_range(&t, 0x600000U,
                               UINT64_C(0xFFFFFFFFFFFFF000), 8192U,
                               VGK_PAGE_EXEC) == VGK_PAGING_BAD_ADDRESS);
    free(pool);

    /* Corrupt intermediate entries must not be dereferenced outside the pool. */
    CHECK(posix_memalign(&pool, 4096U, 4U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 4U, 48U, 1U) == VGK_PAGING_OK);
    t.used_pages = 5U;
    CHECK(vgk_paging_map(&t, 0x700000U, 0x700000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_INVALID_ARGUMENT);
    t.used_pages = 1U;
    /* NX is not a valid flag on an intermediate table entry. */
    t.pool[0] = ((uint64_t)(uintptr_t)pool + 4096U) |
                UINT64_C(0x003) | (UINT64_C(1) << 63);
    CHECK(vgk_paging_map(&t, 0x700000U, 0x700000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);
    t.pool[0] = UINT64_C(0x1000) | UINT64_C(0x003);
    CHECK(vgk_paging_map(&t, 0x700000U, 0x700000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);
    /* Reserved capacity is not enough: the target page must be in use. */
    t.pool[0] = ((uint64_t)(uintptr_t)pool + UINT64_C(3) * 4096U) |
                UINT64_C(0x003);
    CHECK(vgk_paging_map(&t, 0x701000U, 0x701000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);

    /* A corrupted intermediate entry must not create a self-referential tree. */
    CHECK(vgk_paging_init(&t, pool, 4U, 48U, 1U) == VGK_PAGING_OK);
    t.pool[0] = (uint64_t)(uintptr_t)pool | UINT64_C(0x003);
    CHECK(vgk_paging_map(&t, 0x700000U, 0x700000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);
    CHECK(t.used_pages == 1U);

    /* Nor may a child table point back to its root ancestor. */
    memset(pool, 0, 4U * 4096U);
    t.used_pages = 2U;
    t.pool[0] = ((uint64_t)(uintptr_t)pool + 4096U) | UINT64_C(0x003);
    t.pool[512] = (uint64_t)(uintptr_t)pool | UINT64_C(0x003);
    CHECK(vgk_paging_map(&t, 0x700000U, 0x700000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_UNSUPPORTED);
    CHECK(t.used_pages == 2U);
    free(pool);

    /* Alias policy must also cover a different PML4 branch. */
    CHECK(posix_memalign(&pool, 4096U, 8U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 8U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0x200000U, 0x200000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, UINT64_C(0xFFFF800000000000), 0x200000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    {
        size_t used_before = t.used_pages;
        CHECK(vgk_paging_map(&t, UINT64_C(0xFFFF800000001000), 0x200000U,
                             VGK_PAGE_WRITE) == VGK_PAGING_UNSUPPORTED);
        /* A rejected alias must not allocate tables or create a leaf. */
        CHECK(t.used_pages == used_before);
        CHECK(walk(&t, UINT64_C(0xFFFF800000001000)) != 0);
        CHECK((*walk(&t, UINT64_C(0xFFFF800000001000)) & 1U) == 0U);
    }
    free(pool);

    /* A W^X conflict on the second page of a range is reported explicitly. */
    CHECK(posix_memalign(&pool, 4096U, 8U * 4096U) == 0);
    CHECK(vgk_paging_init(&t, pool, 8U, 48U, 1U) == VGK_PAGING_OK);
    CHECK(vgk_paging_map(&t, 0xA01000U, 0x901000U,
                         VGK_PAGE_EXEC) == VGK_PAGING_OK);
    CHECK(vgk_paging_map_range(&t, 0x800000U, 0x900000U, 8192U,
                               VGK_PAGE_WRITE) == VGK_PAGING_UNSUPPORTED);
    CHECK((*walk(&t, 0x800000U) & 1U) != 0U);
    CHECK(walk(&t, 0x801000U) != 0);
    CHECK((*walk(&t, 0x801000U) & 1U) == 0U);
    free(pool);

    puts("OK: tablice stron – uprawnienia, NX, W^X, granice i brak pamięci.");
    return 0;
}
