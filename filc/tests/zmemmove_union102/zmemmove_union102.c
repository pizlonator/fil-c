#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    zptrtable* dst = zptrtable_new();
    __int128 src = 666;

    zmemmove_union(dst, &src, 16);

    return 0;
}

