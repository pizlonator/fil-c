#include <stdfil.h>
#include <filc_test_support.h>

__attribute__((__noinline__)) static void doit(void)
{
    int* object1 = zgc_alloc(sizeof(int));
    int* object2 = zgc_alloc(sizeof(int));
    if (zis_runtime_testing_enabled())
        ZASSERT(!ztesting_get_num_ptrtables());
    zptrtable* table = zptrtable_new();
    if (zis_runtime_testing_enabled())
        ZASSERT(ztesting_get_num_ptrtables() == 1);
    ZASSERT(!zptrtable_encode(table, 0));
    ZASSERT(!zptrtable_decode(table, 0));
    unsigned long index1 = zptrtable_encode(table, object1);
    unsigned long index2 = zptrtable_encode(table, object2);
    ZASSERT(index1);
    ZASSERT(index1 >= 65536);
    ZASSERT(!(index1 & 15));
    ZASSERT(index2);
    ZASSERT(index2 >= 65536);
    ZASSERT(!(index2 & 15));
    ZASSERT(index1 != index2);
    ZASSERT(zptrtable_encode(table, object1) == index1);
    ZASSERT(zptrtable_encode(table, object2) == index2);
    ZASSERT(zptrtable_decode(table, index1) == object1);
    ZASSERT(zptrtable_decode(table, index2) == object2);
    zgc_free(object1);
    ZASSERT(!zptrtable_encode(table, object1));
    ZASSERT(!zptrtable_decode(table, index1));
    zgc_request_and_wait();
    object1 = zgc_alloc(sizeof(int));
    unsigned long index1_2 = zptrtable_encode(table, object1);
    ZASSERT(index1_2 == index1);
    ZASSERT(zptrtable_decode(table, index1_2) == object1);
}

int main()
{
    doit(); /* gotta be in a noinline function because the filc_frame isn't fully optimized for
               liveness - it may have stale entries in some cases. maybe I'll fix that eventually. */
    zgc_request_and_wait();
    if (zis_runtime_testing_enabled()) {
        ZASSERT(!ztesting_get_num_ptrtables());
        zprintf("We destructed the last table!\n");
    }
    zprintf("Success\n");
    return 0;
}

