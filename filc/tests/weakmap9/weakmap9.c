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
        zweak_map_set(map, x, NULL);
        ZASSERT(!zweak_map_get(map, x));
    }

    return 0;
}

