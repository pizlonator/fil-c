#include <sys/mman.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    char* buf = malloc(100);
    ZASSERT(buf);
    free(buf);
    mlock(buf, 16);
    return 0;
}
