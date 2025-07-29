#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char dst[64];
    char src[64];

    zmemmove_union(dst, src + 0x8000000000000000, 0x8000000000000000);

    return 0;
}

