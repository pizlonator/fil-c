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

struct bar {
    char* a;
    char* b;
    int c;
    char* d;
};

int main()
{
    struct foo foo;
    struct bar bar;

    foo.a = 0;
    foo.b = "hello";
    foo.c = 42;
    foo.d = "world";

    zmemmove_union(&bar, &foo, sizeof(foo));

    zprintf("bar.a = %s\n", bar.a);
    zprintf("bar.b = %s\n", bar.b);
    zprintf("bar.c = %d\n", bar.c);
    zprintf("bar.d = %s\n", bar.d);

    return 0;
}

