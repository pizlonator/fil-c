#include <filc_test_support.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

struct foo {
    char* s;
    int v[3];
    char* t;
};

struct bar {
    char* a;
    char* b;
};

static void foo_bar(struct foo* f, struct bar* b,
                    struct foo* f2, struct bar* b2)
{
    size_t index;
    size_t index2;
    
    for (index = 10; index--;) {
        f[index].s = zasprintf("s = %zu", index);
        for (index2 = 3; index2--;)
            f[index].v[index2] = index + index2;
        f[index].t = zasprintf("t = %zu", index);
    }

    for (index = 15; index--;) {
        b[index].a = zasprintf("a = %zu", index);
        b[index].b = zasprintf("b = %zu", index);
    }

    __builtin_memcpy(f2, f, sizeof(struct foo) * 10);
    __builtin_memcpy(b2, b, sizeof(struct bar) * 20);

    zmemmove_union(b2, f, sizeof(struct foo) * 10);
    zmemmove_union(f2, b, sizeof(struct foo) * 10);

    struct foo* f3 = (struct foo*)b2;
    struct bar* b3 = (struct bar*)f2;
        
    for (index = 10; index--;) {
        ZASSERT(!strcmp(f3[index].s, zasprintf("s = %zu", index)));
        for (index2 = 3; index2--;)
            ZASSERT(f3[index].v[index2] == index + index2);
        ZASSERT(!strcmp(f3[index].t, zasprintf("t = %zu", index)));
    }
    for (index = 15; index--;) {
        ZASSERT(!strcmp(b3[index].a, zasprintf("a = %zu", index)));
        ZASSERT(!strcmp(b3[index].b, zasprintf("b = %zu", index)));
    }

    zmemmove_union(f2, f, sizeof(struct foo) * 10);
    zmemmove_union(b2, b, sizeof(struct bar) * 20);

    for (index = 10; index--;) {
        ZASSERT(!strcmp(f2[index].s, zasprintf("s = %zu", index)));
        for (index2 = 3; index2--;)
            ZASSERT(f2[index].v[index2] == index + index2);
        ZASSERT(!strcmp(f2[index].t, zasprintf("t = %zu", index)));
    }
    for (index = 15; index--;) {
        ZASSERT(!strcmp(b2[index].a, zasprintf("a = %zu", index)));
        ZASSERT(!strcmp(b2[index].b, zasprintf("b = %zu", index)));
    }
}

int main()
{
    struct foo* f = zgc_alloc(sizeof(struct foo) * 10);
    struct bar* b = zgc_alloc(sizeof(struct bar) * 20);
    struct foo* f2 = zgc_alloc(sizeof(struct foo) * 10);
    struct bar* b2 = zgc_alloc(sizeof(struct bar) * 20);

    foo_bar(f, b, f2, b2);

    zprintf("doing it again\n");
    memset(f, 0, sizeof(struct foo) * 10);
    memset(b, 0, sizeof(struct bar) * 20);
    memset(f2, 0, sizeof(struct foo) * 10);
    memset(b2, 0, sizeof(struct bar) * 20);
    
    foo_bar(f2, b2, f, b);

    char* buf = zgc_alloc(100);
    const char* str = "tam to tak tego tam";
    zmemmove_union(buf, str, strlen(str) + 1);
    ZASSERT(!strcmp(buf, str));

    char buf2[8];
    unsigned i;
    for (i = 8; i--;)
        buf2[i] = 0;
    char* ptr;
    zmemmove_union(&ptr, buf2, 8);
    zprintf("ptr = %P\n", ptr);
    zprintf("hello = %P\n", "hello");
    *(char**)opaque(&ptr) = "hello";
    zprintf("ptr = %P\n", ptr);
    ZASSERT(!strcmp(ptr, "hello"));

    char* src_ptr = "hello";
    char* dst_ptr;
    zmemmove_union(&dst_ptr, &src_ptr, sizeof(src_ptr));
    dst_ptr = "world";
    ZASSERT(!strcmp(*(char**)opaque(&dst_ptr), "world"));

    return 0;
}


