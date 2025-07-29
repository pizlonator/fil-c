#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char dst[64];
    char src[64];

    zmemmove_union(dst, src + 64, 64);

    return 0;
}

