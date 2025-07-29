#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    zptrtable* src = zptrtable_new();
    __int128 dst = 666;

    zmemmove_union(&dst, src, 16);

    return 0;
}

