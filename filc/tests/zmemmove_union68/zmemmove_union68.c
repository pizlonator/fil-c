#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char dst[64];
    char src[32];

    zmemmove_union(dst, src, 33);

    return 0;
}

