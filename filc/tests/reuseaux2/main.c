#include <stdfil.h>

int foo(int** p, int* r);

static void test1(void)
{
    int* array[3];
    int x = 1410;
    ZASSERT(foo(array, &x) == 42 + 666);
}

static void test2(void)
{
    int* array[3];
    int x = 1;
    int y = 2;
    int z = 4;
    array[0] = &x;
    array[1] = &y;
    ZASSERT(foo(array, &z) == 3);
}

int main()
{
    test1();
    test2();
    return 0;
}

