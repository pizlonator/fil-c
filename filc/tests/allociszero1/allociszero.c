#include <stdfil.h>

int main()
{
    unsigned i;
    for (i = 100000; i--;) {
        int* ptr = zgc_alloc(16);
        unsigned j;
        for (j = 4; j--;)
            ZASSERT(!ptr[j]);
    }
    return 0;
}

