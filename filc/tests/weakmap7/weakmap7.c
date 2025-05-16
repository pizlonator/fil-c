#include <stdfil.h>

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
    }

    return 0;
}

