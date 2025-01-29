#include <stdfil.h>
#include <string.h>

void foo(int argc, char** argv, char** environ) __attribute__((constructor));
void foo(int argc, char** argv, char** environ)
{
    ZASSERT(argc == 1);
    ZASSERT(strstr(argv[0], "globalctor"));
    zprintf("Hello ");
}

int main(int argc, char** argv)
{
    ZASSERT(argc == 1);
    ZASSERT(strstr(argv[0], "globalctor"));
    zprintf("World!\n");
}

