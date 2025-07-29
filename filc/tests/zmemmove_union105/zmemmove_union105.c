#include <filc_test_support.h>
#include <stdfil.h>
#include <inttypes.h>
#include <string.h>

int main()
{
    char* src = "hello";
    int64_t dst = 666;

    zmemmove_union(&dst, &src, 8);

    ZASSERT(!strcmp(*(char**)&dst, "hello"));

    return 0;
}
