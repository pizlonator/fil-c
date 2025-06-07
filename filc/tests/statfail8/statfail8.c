#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = (struct stat*)zweak_map_new();
    stat("filc/tests/statfail8/statfail8.c", st);
    return 0;
}