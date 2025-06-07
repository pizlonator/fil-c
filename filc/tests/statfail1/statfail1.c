#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    stat("filc/tests/statfail1/statfail1.c", NULL);
    return 0;
}