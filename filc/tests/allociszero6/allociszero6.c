#include <stdfil.h>
#include "utils.h"

int main()
{
    unsigned i;
    for (i = 10; i--;) {
        int* ptr = opaque(zgc_alloc(16000000));
        unsigned j;
        for (j = 4000000; j--;)
            ZASSERT(!ptr[j]);
    }
    return 0;
}

