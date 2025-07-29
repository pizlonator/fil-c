#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    char dst[64];

    zmemmove_union(dst, (void*)666, 64);

    return 0;
}

