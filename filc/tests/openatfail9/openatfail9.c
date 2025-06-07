#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char* p = malloc(64);
    strcpy(p, "filc/tests/openatfail9/test.txt");
    free(p);
    openat(AT_FDCWD, p, O_RDONLY);  // Use after free
    return 0;
}