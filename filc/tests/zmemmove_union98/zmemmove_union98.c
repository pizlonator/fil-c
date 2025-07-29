#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    zptrtable* src = zptrtable_new();
    int64_t dst;

    zmemmove_union(&dst, src, 8);

    return 0;
}

