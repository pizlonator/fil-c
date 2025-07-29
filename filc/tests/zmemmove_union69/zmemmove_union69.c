#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char dst[32];
    char src[64];

    zmemmove_union(dst, src, 33);

    return 0;
}

