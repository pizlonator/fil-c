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
    foo1.b = NULL;
    foo1.c = 666;
    foo1.d = NULL;

    char* buf = malloc(3 + sizeof(foo1));
    zmemmove_union(buf + 3, &foo1, sizeof(foo1));

    zmemmove_union(&foo2, buf + 3, sizeof(foo1));

    zprintf("foo2.a = %d\n", foo2.a);
    zprintf("foo2.b = %s\n", foo2.b);
    zprintf("foo2.c = %d\n", foo2.c);
    zprintf("foo2.d = %s\n", foo2.d);

    return 0;
}

