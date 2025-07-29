#include <filc_test_support.h>
#include <stdfil.h>

int main()
{
    int*_Atomic* a = zgc_alloc(sizeof(int*) * 42);
    int** b = zgc_alloc(sizeof(int*) * 42);

    unsigned i;
    for (i = 42; i--;) {
        a[i] = zgc_alloc(sizeof(int));
        *a[i] = 666 + i;
    }

    zmemmove_union((char*)b + 1, (char*)a + 1, 41 * sizeof(int*));

    ZASSERT(!zhasvalidcap(b[0]));
    ZASSERT(!zhasvalidcap(b[41]));
    
    for (i = 41; i-->1;)
        ZASSERT(*b[i] == 666 + i);

    return 0;
}

