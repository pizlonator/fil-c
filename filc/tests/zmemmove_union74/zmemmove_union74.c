#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>
#include <string.h>

int main()
{
    char* dst[4];
    int64_t src[4];

    unsigned i = 0;
    for (i = 4; i--;) {
        dst[i] = "hello";
        src[i] = 666;
    }

    zmemmove_union((char*)dst + 5, src, 1);
    ZASSERT(!zhasvalidcap(dst[0]));
    for (i = 1; i < 4; ++i)
        ZASSERT(!strcmp(dst[i], "hello"));

    return 0;
}
