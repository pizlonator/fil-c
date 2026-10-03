#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdfil.h>

static char global_memory[4096];

int main(void)
{
    char stack_memory[4096];
    void* malloc_memory;
    void* gc_memory;
    void* mmap_memory;

    /* mlock on memory that was not mmapped must return an error instead of panicking.
       See https://github.com/pizlonator/fil-c/issues/329. */

    errno = 0;
    ZASSERT(mlock(stack_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    errno = 0;
    ZASSERT(munlock(stack_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    errno = 0;
    ZASSERT(mlock(global_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    errno = 0;
    ZASSERT(munlock(global_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    malloc_memory = malloc(4096);
    ZASSERT(malloc_memory);
    errno = 0;
    ZASSERT(mlock(malloc_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    errno = 0;
    ZASSERT(munlock(malloc_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    gc_memory = zgc_aligned_alloc(4096, 4096);
    ZASSERT(gc_memory);
    errno = 0;
    ZASSERT(mlock(gc_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    errno = 0;
    ZASSERT(munlock(gc_memory, 4096) == -1);
    ZASSERT(errno == ENOMEM);

    /* Any nonzero length gets the check, even a sub-page one. */
    errno = 0;
    ZASSERT(mlock(global_memory, 100) == -1);
    ZASSERT(errno == ENOMEM);

    /* mlock on actually mmapped memory still goes to the kernel. */
    mmap_memory = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ZASSERT(mmap_memory != MAP_FAILED);
    if (!mlock(mmap_memory, 4096)) {
        ZASSERT(!munlock(mmap_memory, 4096));
    } else {
        /* The kernel may refuse for resource-limit reasons (EAGAIN, or ENOMEM when the
           RLIMIT_MEMLOCK soft limit would be exceeded).  That's OK; the point is that this
           goes to the kernel instead of being intercepted by the runtime. */
        ZASSERT(errno == EAGAIN || errno == ENOMEM);
    }

    zprintf("mlocknotmapped passed\n");
    return 0;
}
