#include <stdfil.h>
#include "utils.h"

int main()
{
    unsigned i;
    for (i = 100; i--;) {
        int* ptr = opaque(zgc_alloc(160000));
        unsigned j;
        for (j = 40000; j--;)
            ZASSERT(!ptr[j]);
    }
    return 0;
}

