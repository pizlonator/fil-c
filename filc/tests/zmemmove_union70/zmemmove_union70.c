#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    char dst[32];
    char src[64];

    zmemmove_union(dst, src, (intptr_t)-1);

    return 0;
}

