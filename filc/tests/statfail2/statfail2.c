#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    stat("filc/tests/statfail2/statfail2.c", (struct stat*)0x1010101010101010UL);
    return 0;
}