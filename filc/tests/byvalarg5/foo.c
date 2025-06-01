#include "structs.h"
#include <stdfil.h>
#include <string.h>

void foo(struct foo f, struct bar b)
{
    ZASSERT(!strcmp(f.a, "f.a"));
    ZASSERT(!strcmp(f.b, "f.b"));
    ZASSERT(!strcmp(f.c, "f.c"));
    ZASSERT(!strcmp(f.d, "f.d"));
    ZASSERT(!strcmp(f.e, "f.e"));
    ZASSERT(!strcmp(b.a, "b.a"));
    ZASSERT(!strcmp(b.b, "b.b"));
    ZASSERT(!strcmp(b.c, "b.c"));
    ZASSERT(!strcmp(b.d, "b.d"));
    zgc_request_and_wait();
    ZASSERT(!strcmp(f.a, "f.a"));
    ZASSERT(!strcmp(f.b, "f.b"));
    ZASSERT(!strcmp(f.c, "f.c"));
    ZASSERT(!strcmp(f.d, "f.d"));
    ZASSERT(!strcmp(f.e, "f.e"));
    ZASSERT(!strcmp(b.a, "b.a"));
    ZASSERT(!strcmp(b.b, "b.b"));
    ZASSERT(!strcmp(b.c, "b.c"));
    ZASSERT(!strcmp(b.d, "b.d"));
}

