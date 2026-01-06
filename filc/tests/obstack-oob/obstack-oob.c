#define _GNU_SOURCE
#include <features.h>

#include <stdlib.h>

#ifdef __USE_GNU

#include <stdfil.h>
#include <obstack.h>
#include <stddef.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

int main()
{
    // Ensure we get different allocations per each object allocated from an obstack.
    struct obstack o;
    obstack_init(&o);

    char* p1 = obstack_alloc(&o, 1);
    char* p2 = obstack_alloc(&o, 1);
    ptrdiff_t delta = p2 - p1;
    *(p1 + delta) = 'z';

    return 0;
}
#else
int main()
{
    char* p1 = malloc(1);
    p1[10000] = 'f';
    return 0;
}
#endif // __USE_GNU
