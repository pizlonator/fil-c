#include <sys/mman.h>
#include <stdfil.h>
#include <inttypes.h>
#include <stdlib.h>

int main()
{
    void* m = zgc_aligned_alloc(4096, 4096);
    ZASSERT(m);
    ZASSERT(m != (void*)(intptr_t)-1);
    ZASSERT(!madvise(m, 4096, MADV_DONTDUMP));
    return 0;
}
