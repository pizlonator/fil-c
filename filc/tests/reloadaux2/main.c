#include <stdfil.h>

int foo(int** p, int** q, int* r);

int main()
{
    int* array[2];
    int x = 1410;
    ZASSERT(foo(array, array + 1, &x) == 1410);
    return 0;
}

