#include <unistd.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    pipe((int*)0x1010101010101010UL);
    return 0;
}