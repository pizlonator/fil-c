#include <stdfil.h>
#include <stdlib.h>

int main()
{
    unsigned count;
    for (count = 10000; count--;) {
        zweak_map* map = zweak_map_new();
        int* x = zgc_alloc(sizeof(int));
        zweak_map_set(map, NULL, x);
        ZASSERT(zweak_map_get(map, NULL) == x);
    }
    return 0;
}

