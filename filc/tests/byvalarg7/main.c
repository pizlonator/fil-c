#include "structs.h"
#include <string.h>

void foo(struct foo f, struct bar b, unsigned depth);

int main()
{
    unsigned count;
    for (count = 1000; count--;) {
        struct foo f;
        struct bar b;
        f.a = strdup("f.a");
        f.b = strdup("f.b");
        f.c = strdup("f.c");
        f.d = strdup("f.d");
        f.e = strdup("f.e");
        b.a = strdup("b.a");
        b.b = strdup("b.b");
        b.c = strdup("b.c");
        b.d = strdup("b.d");
        foo(f, b, 0);
    }
    return 0;
}

