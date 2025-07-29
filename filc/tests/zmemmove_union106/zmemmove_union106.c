#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>

int main()
{
    char* dst = "hello";
    int64_t src = 666;

    zmemmove_union(&dst, &src, 8);
    ZASSERT(dst == (char*)666);
    ZASSERT(!zhasvalidcap(dst));

    return 0;
}
