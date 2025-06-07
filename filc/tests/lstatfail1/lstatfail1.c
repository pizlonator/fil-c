#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    lstat("filc/tests/lstatfail1/lstatfail1.c", NULL);
    return 0;
}