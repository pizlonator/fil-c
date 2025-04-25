#include <stdlib.h>

int* allocx(void)
{
    int* x = malloc(sizeof(int));
    *x = 666;
    return x;
}
