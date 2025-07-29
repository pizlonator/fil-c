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
    struct foo foo2;

    char* buf = malloc(3 + sizeof(foo2));
    memset(buf, 42, 3 + sizeof(foo2));

    zmemmove_union(&foo2, buf + 3, sizeof(foo2));

    zprintf("foo2.a = %d\n", foo2.a);
    zprintf("foo2.b = %s\n", foo2.b);
    zprintf("foo2.c = %d\n", foo2.c);
    zprintf("foo2.d = %s\n", foo2.d);

    return 0;
}

