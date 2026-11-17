#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "vgk_exception.h"

int main(void)
{
    static const uint8_t with_error[] = {
        8U, 10U, 11U, 12U, 13U, 14U, 17U, 20U, 21U, 29U, 30U
    };
    uint8_t expected[32] = {0};

    for (size_t i = 0; i < sizeof(with_error) / sizeof(with_error[0]); ++i)
        expected[with_error[i]] = 1U;

    for (uint8_t vector = 0U; vector < 32U; ++vector)
        assert(vgk_exception_vector_has_error_code(vector) == expected[vector]);

    assert(sizeof(VGK_EXCEPTION_CONTEXT) == 23U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, vector) == 15U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, error_code) == 16U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, rip) == 17U * sizeof(uint64_t));

    puts("VGK exception context and error-code classification tests: OK");
    return 0;
}
