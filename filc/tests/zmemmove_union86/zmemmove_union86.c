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

    zmemmove_union(dst, (char*)src + 5, 1);

    for (i = 1; i < 4; ++i)
        ZASSERT(!dst[i]);
    for (i = 4; i--;)
        ZASSERT(!zhasvalidcap(((char**)dst)[i]));

    return 0;
}
