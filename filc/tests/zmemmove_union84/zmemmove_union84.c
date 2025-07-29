#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    int64_t dst[4];
    char* src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        src[i] = "hello";

    zmemmove_union((char*)dst + 5, src, 1);
    ZASSERT(!zhasvalidcap(((char**)dst)[0]));
    for (i = 1; i < 4; ++i)
        ZASSERT(!dst[i]);

    return 0;
}
