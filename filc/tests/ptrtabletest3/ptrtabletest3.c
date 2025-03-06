#include <stdfil.h>
#include <filc_test_support.h>
#include <stdbool.h>

static const unsigned num_objects = 1000;

__attribute__((__noinline__)) static void doit(void)
{
    if (zis_runtime_testing_enabled())
        ZASSERT(!ztesting_get_num_ptrtables());
    zptrtable* table = zptrtable_new();
    if (zis_runtime_testing_enabled())
        ZASSERT(ztesting_get_num_ptrtables() == 1);
    int** objects = zgc_alloc(sizeof(int*) * num_objects);
    unsigned long* indices = zgc_alloc(sizeof(unsigned long) * num_objects);
    unsigned index;
    for (index = num_objects; index--;) {
        zprintf("First run, index = %u\n", index);
        objects[index] = zgc_alloc(sizeof(int));
        indices[index] = zptrtable_encode(table, objects[index]);
        ZASSERT(indices[index]);
        unsigned index2;
        for (index2 = index + 1; index2 < num_objects; ++index2)
            ZASSERT(indices[index] != indices[index2]);
    }
    for (index = num_objects; index--;) {
        ZASSERT(zptrtable_encode(table, objects[index]) == indices[index]);
        ZASSERT(zptrtable_decode(table, indices[index]) == objects[index]);
    }
    zgc_request_and_wait();
    for (index = num_objects; index--;) {
        ZASSERT(zptrtable_encode(table, objects[index]) == indices[index]);
        ZASSERT(zptrtable_decode(table, indices[index]) == objects[index]);
    }
    for (index = 0; index < num_objects; index += 2)
        zgc_free(objects[index]);
    zgc_request_and_wait();
    for (index = 0; index < num_objects; index += 2) {
        zprintf("Second run, index = %u\n", index);
        int* object = zgc_alloc(sizeof(int));
        unsigned long object_index = zptrtable_encode(table, object);
        ZASSERT(object_index);
        ZASSERT(zptrtable_decode(table, object_index) == object);
        unsigned index2;
        bool found = false;
        for (index2 = 0; index2 < num_objects; index2 += 2) {
            if (indices[index2] == object_index) {
                indices[index2] = 0;
                found = true;
                break;
            }
        }
        ZASSERT(found);
    }
    for (index = 1; index < num_objects; index += 2) {
        ZASSERT(zptrtable_encode(table, objects[index]) == indices[index]);
        ZASSERT(zptrtable_decode(table, indices[index]) == objects[index]);
    }
    for (index = 0; index < num_objects; index += 2) {
        zprintf("Second run, index = %u\n", index);
        int* object = zgc_alloc(sizeof(int));
        unsigned long object_index = zptrtable_encode(table, object);
        ZASSERT(object_index);
        ZASSERT(zptrtable_decode(table, object_index) == object);
        unsigned index2;
        for (index2 = 0; index2 < num_objects; ++index2)
            ZASSERT(object_index != indices[index2]);
    }
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

