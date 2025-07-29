#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>
#include <string.h>

int main()
{
    intptr_t dst[4];
    char* src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        src[i] = "hello";

    zmemmove_union((char*)dst + 5, (char*)src + 5, 27);

    for (i = 1; i < 4; ++i)
        ZASSERT(dst[i] == (intptr_t)src[i]);
    ZASSERT(!zhasvalidcap(((char**)dst)[0]));
    for (i = 1; i < 4; ++i)
        ZASSERT(!strcmp(((char**)dst)[i], "hello"));

    return 0;
}
