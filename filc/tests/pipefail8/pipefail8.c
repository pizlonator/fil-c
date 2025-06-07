#include <unistd.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int* p = (int*)zweak_map_new();
    pipe(p);
    return 0;
}