#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fstatat(AT_FDCWD, "filc/tests/fstatatfail1/fstatatfail1.c", NULL, 0);
    return 0;
}