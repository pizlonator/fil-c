#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    char* dst[4];
    int64_t src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        dst[i] = "hello";

    zmemmove_union(dst, src, 27);
    for (i = 3; i--;)
        ZASSERT(!dst[i]);
    ZASSERT(!zhasvalidcap(dst[3]));

    return 0;
}
