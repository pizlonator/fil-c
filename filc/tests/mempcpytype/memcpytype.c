#include <stdlib.h>
#include <stdfil.h>

void foo(void* a, void* b);

int main()
{
    int** a;
    int** b;
    a = malloc(sizeof(int*) * 2);
    a[0] = malloc(sizeof(int));
    *a[0] = 42;
    a[1] = malloc(sizeof(int));
    *a[1] = 666;
    b = malloc(sizeof(int*) * 2);
    foo(b, a);
    ZASSERT(*b[0] == 42);
    ZASSERT(*b[1] == 666);
    return 0;
}


