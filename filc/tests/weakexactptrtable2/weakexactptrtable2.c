#include <stdfil.h>
#include <inttypes.h>

static __attribute__((noinline)) uintptr_t add(zexact_ptrtable* table, int value)
{
    int* ptr = zgc_alloc(sizeof(int));
    *ptr = value;
    return zexact_ptrtable_encode(table, ptr);
}

static __attribute__((noinline)) void check(zexact_ptrtable* table, uintptr_t intptr)
{
    int* ptr = zexact_ptrtable_decode(table, intptr);
    ZASSERT(!zhasvalidcap(ptr));
}

int main()
{
    unsigned i;
    for (i = 10; i--;) {
        zexact_ptrtable* table = zexact_ptrtable_new_weak();
        unsigned j;
        unsigned n = 10000;
        uintptr_t* array = zgc_alloc(sizeof(uintptr_t) * n);
        for (j = n; j--;)
            array[j] = add(table, j * 666);
        zgc_request_and_wait();
        for (j = n; j--;)
            check(table, array[j]);
    }
    return 0;
}

