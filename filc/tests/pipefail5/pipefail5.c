#include <unistd.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int* p = malloc(0);  // Zero-sized allocation, not enough space for 2 ints
    pipe(p);
    return 0;
}