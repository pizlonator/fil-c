#include <stdfil.h>
#include <string.h>
#include "utils.h"

static __attribute__((noinline)) void test1(zweak_map* map)
{
    char* hello3 = strdup("hello3");
    zweak_map_set(map, hello3, "world");
    ZASSERT(!strcmp(zweak_map_get(map, hello3), "world"));
    char* hello4 = strdup("hello4");
    zweak_map_set(map, hello4, strdup("world"));
    ZASSERT(!strcmp(zweak_map_get(map, hello4), "world"));
    char* hello5 = strdup("hello4");
    zweak_map_set(map, hello5, strdup("world"));
    ZASSERT(!strcmp(zweak_map_get(map, hello5), "world"));
    ZASSERT(zweak_map_size(map) == 6);
    zweak_map_set(map, hello5, NULL);
    ZASSERT(zweak_map_size(map) == 5);
    ZASSERT(!zweak_map_get(map, hello5));
    opaque(hello3);
    opaque(hello4);
}

static __attribute__((noinline)) void doit(void)
{
    zweak_map* map = zweak_map_new();
    zweak_map_set(map, "hello", "world");
    ZASSERT(!strcmp(zweak_map_get(map, "hello"), "world"));
    zweak_map_set(map, "hello2", strdup("world"));
    ZASSERT(!strcmp(zweak_map_get(map, "hello2"), "world"));
    zweak_map_set(map, (void*)666, (void*)1410);
    ZASSERT(zweak_map_get(map, (void*)666) == (void*)1410);
    test1(map);
    zgc_request_and_wait();
    ZASSERT(zweak_map_size(map) == 3);
    ZASSERT(!strcmp(zweak_map_get(map, "hello"), "world"));
    ZASSERT(!strcmp(zweak_map_get(map, "hello2"), "world"));
    ZASSERT(zweak_map_get(map, (void*)666) == (void*)1410);
    zweak_map_set(map, "hello", NULL);
    ZASSERT(zweak_map_size(map) == 2);
    ZASSERT(!zweak_map_get(map, "hello"));
}

int main()
{
    unsigned count;
    for (count = 10000; count--;)
        doit();
    return 0;
}

