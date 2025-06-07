#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = malloc(0);  // Zero-sized allocation
    stat("filc/tests/statfail7/statfail7.c", st);
    return 0;
}