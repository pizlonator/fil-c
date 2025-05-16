#include <stdfil.h>
#include <stdlib.h>

int main()
{
    unsigned count;
    for (count = 10000; count--;) {
        zweak_map* map = zweak_map_new();
        int* x = zgc_alloc(sizeof(int));
        zweak_map_set(map, x, main);
        ZASSERT(zweak_map_get(map, x) == main);
        int* y = zgc_alloc(sizeof(int));
        zweak_map_set(map, x, y);
        ZASSERT(zweak_map_get(map, x) == y);
        int* z = zgc_alloc(sizeof(int));
        zweak_map_set(map, x, z);
        ZASSERT(zweak_map_get(map, x) == z);
        zweak_map_set(map, x, (void*)666);
        ZASSERT(zweak_map_get(map, x) == (void*)666);
        zweak_map_set(map, x, NULL);
        ZASSERT(!zweak_map_get(map, x));
    }

    return 0;
}

