#include "vgk_exception.h"

uint8_t vgk_exception_vector_has_error_code(uint8_t vector)
{
    switch (vector) {
    case 8U:  /* #DF: procesor dostarcza kod bledu o wartosci zero. */
    case 10U: /* #TS */
    case 11U: /* #NP */
    case 12U: /* #SS */
    case 13U: /* #GP */
    case 14U: /* #PF */
    case 17U: /* #AC */
    case 20U: /* #VE */
    case 21U: /* #CP */
    case 29U: /* #VC */
    case 30U: /* #SX */
        return 1U;
    default:
        return 0U;
    }
}

_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, vector) == 15U * sizeof(uint64_t),
               "VGK exception vector offset mismatch");
_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, error_code) == 16U * sizeof(uint64_t),
               "VGK exception error-code offset mismatch");
_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, rip) == 17U * sizeof(uint64_t),
               "VGK exception RIP offset mismatch");
_Static_assert(sizeof(VGK_EXCEPTION_CONTEXT) == 23U * sizeof(uint64_t),
               "VGK exception context size mismatch");
