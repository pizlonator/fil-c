#include <sys/mman.h>

int main()
{
    msync((void*)666, 4096, MS_SYNC);
    return 0;
}
