#include <stdfil.h>
#include <filc_test_support.h>

int main()
{
    int a;
    ZASSERT(zgc_get_allocation_size(&a) == 32);
    int b[4];
    ZASSERT(zgc_get_allocation_size(b) == 32);
    int c[5];
    ZASSERT(zgc_get_allocation_size(c) == 48);
    int d[8];
    ZASSERT(zgc_get_allocation_size(d) == 48);
    int e[9];
    ZASSERT(zgc_get_allocation_size(e) == 64);
    return 0;
}

