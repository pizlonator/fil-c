#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    zptrtable* dst = zptrtable_new();
    int64_t src;

    zmemmove_union(dst, &src, 16);

    return 0;
}

