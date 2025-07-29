#include <filc_test_support.h>
#include "utils.h"

static void reset(char* array)
{
    unsigned i;
    for (i = 128; i--;)
        array[i] = i;
}

int main()
{
    char* array = opaque(zgc_alloc(128));
    char* array2 = opaque(array);

    reset(array);
    zmemmove_union(array + 5, array + 7, 7);

    unsigned i;
    for (i = 128; i--;) {
        if (i >= 5 && i < 5 + 7)
            ZASSERT(array[i] == i + 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array + 7, array + 5, 7);

    for (i = 128; i--;) {
        if (i >= 7 && i < 7 + 7)
            ZASSERT(array[i] == i - 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array + 5, array + 7, 32);

    for (i = 128; i--;) {
        if (i >= 5 && i < 5 + 32)
            ZASSERT(array[i] == i + 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array + 7, array + 5, 32);

    for (i = 128; i--;) {
        if (i >= 7 && i < 7 + 32)
            ZASSERT(array[i] == i - 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array + 8, array + 16, 32);

    for (i = 128; i--;) {
        if (i >= 8 && i < 8 + 32)
            ZASSERT(array[i] == i + 8);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array + 16, array + 8, 32);

    for (i = 128; i--;) {
        if (i >= 16 && i < 16 + 32)
            ZASSERT(array[i] == i - 8);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 5, array + 7, 7);

    for (i = 128; i--;) {
        if (i >= 5 && i < 5 + 7)
            ZASSERT(array[i] == i + 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 7, array + 5, 7);

    for (i = 128; i--;) {
        if (i >= 7 && i < 7 + 7)
            ZASSERT(array[i] == i - 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 5, array + 7, 32);

    for (i = 128; i--;) {
        if (i >= 5 && i < 5 + 32)
            ZASSERT(array[i] == i + 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 7, array + 5, 32);

    for (i = 128; i--;) {
        if (i >= 7 && i < 7 + 32)
            ZASSERT(array[i] == i - 2);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 8, array + 16, 32);

    for (i = 128; i--;) {
        if (i >= 8 && i < 8 + 32)
            ZASSERT(array[i] == i + 8);
        else
            ZASSERT(array[i] == i);
    }

    reset(array);
    zmemmove_union(array2 + 16, array + 8, 32);

    for (i = 128; i--;) {
        if (i >= 16 && i < 16 + 32)
            ZASSERT(array[i] == i - 8);
        else
            ZASSERT(array[i] == i);
    }

    return 0;
}

