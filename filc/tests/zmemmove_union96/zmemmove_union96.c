#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    char* dst[4];
    uint64_t src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        dst[i] = "hello";

    zmemmove_union((char*)dst + 5, src, 27);

    for (i = 4; i--;)
        ZASSERT(!zhasvalidcap(dst[i]));

    return 0;
}
