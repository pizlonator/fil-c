#include <unistd.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Cast an 8-byte string literal to int* (readonly memory)
    pipe((int*)"12345678");
    return 0;
}