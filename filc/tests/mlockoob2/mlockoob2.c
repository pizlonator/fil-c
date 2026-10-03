#include <sys/mman.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    char* buf = malloc(100);
    ZASSERT(buf);
    mlock(buf - 1000, 16);
    return 0;
}
