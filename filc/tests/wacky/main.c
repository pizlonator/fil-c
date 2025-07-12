#include <stdbool.h>
#include <stdfil.h>

bool bar(void)
{
    return true;
}

struct foo {
    int a;
    int b;
    int c;
    int d;
};


int foo(struct foo *restrict const x);

int main()
{
    struct foo x;
    x.a = 1;
    x.b = 2;
    x.c = 3;
    x.d = 4;
    ZASSERT(foo(&x) == 666);
    return 0;
}


