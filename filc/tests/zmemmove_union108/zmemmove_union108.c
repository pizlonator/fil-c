#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    char* dst = "hello";
    int64_t src[2];
    src[0] = 666;
    src[1] = 1410;

    zmemmove_union(&dst, (char*)src + 1, 8);

    ZASSERT(!zhasvalidcap(dst));
    ZASSERT(dst == 0x8200000000000002lu);

    return 0;
}
