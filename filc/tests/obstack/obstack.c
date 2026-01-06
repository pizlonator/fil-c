#define _GNU_SOURCE
#include <features.h>

#ifdef __USE_GNU

#include <stdfil.h>
#include <stdlib.h>
#include <obstack.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

int main()
{
    // This test models code that needs to work this way for binutils to work.
    struct obstack o;
    obstack_init(&o);

    size_t a = 42;
    size_t b = 666;

    obstack_blank(&o, a + b);
    void* ptr = obstack_base(&o);
    ZASSERT(!!ptr);
    o.next_free -= b;
    obstack_finish(&o);
    ZASSERT(obstack_room(&o) >= b);

    return 0;
}
#else
int main()
{
    return 0;
}

#endif // __USE_GNU
