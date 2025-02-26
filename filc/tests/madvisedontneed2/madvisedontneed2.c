#include <sys/mman.h>
#include <stdfil.h>
#include <inttypes.h>
#include <stdlib.h>

int main()
{
    void* m = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    ZASSERT(m);
    ZASSERT(m != (void*)(intptr_t)-1);
    ZASSERT(!madvise(m, 4096, MADV_DONTNEED));
    return 0;
}
