#include <filc_test_support.h>
#include <stdfil.h>
#include <string.h>

int main()
{
    char* dst[4];
    __int128 src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        dst[i] = "hello";

    zmemmove_union(dst, (char*)src + 5, 1);

    ZASSERT(!zhasvalidcap(dst[0]));
    for (i = 1; i < 4; ++i)
        ZASSERT(!strcmp(dst[i], "hello"));

    return 0;
}
