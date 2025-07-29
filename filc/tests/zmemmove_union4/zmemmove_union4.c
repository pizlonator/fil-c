#include <filc_test_support.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdfil.h>
#include <string.h>

struct foo {
    int a;
    char* b;
    int c;
    char* d;
};

int main()
{
    struct foo foo1;
    struct foo foo2;

    foo1.a = 42;
    foo1.b = "hello";
    foo1.c = 666;
    foo1.d = "hello";

    char* buf = malloc(3 + sizeof(foo1));
    zmemmove_union(buf + 3, &foo1, sizeof(foo1));

    zmemmove_union(&foo2, buf + 3, sizeof(foo1));

    zprintf("foo2.a = %d\n", foo2.a);
    ZASSERT(!zhasvalidcap(foo2.b));
    zprintf("foo2.c = %d\n", foo2.c);
    ZASSERT(!zhasvalidcap(foo2.d));

    return 0;
}

