#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>
#include <string.h>

int main()
{
    int64_t dst[4];
    char* src[4];

    unsigned i = 0;
    for (i = 4; i--;)
        src[i] = "hello";

    zmemmove_union(dst, src, 27);
    for (i = 3; i--;)
        ZASSERT(!strcmp(((char**)dst)[i], "hello"));
    ZASSERT(!zhasvalidcap(((char**)dst)[3]));

    return 0;
}
