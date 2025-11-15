#include <stdfil.h>
#include <sys/mman.h>
#include "utils.h"

int main()
{
    unsigned i;
    for (i = 10; i--;) {
        int* ptr = opaque(mmap(0, 16000000, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0));
        unsigned j;
        for (j = 4000000; j--;)
            ZASSERT(!ptr[j]);
    }
    return 0;
}

