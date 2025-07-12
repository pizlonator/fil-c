#include <stdbool.h>

struct foo {
    int a;
    int b;
    int c;
    int d;
};

bool bar(void);

int foo(struct foo *restrict const x)
{
    if (bar())
        goto label2;

label1:
    for (;;) {
        bar();
        if (!x->a) goto label3;
        if (!bar()) break;
        if (x->b) return 42;
    }

label2:
    if (!x->c || !x->d)
        goto label1;
    bar();
    return 666;

label3:
    return 1410;
}
