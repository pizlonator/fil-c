#include <stdfil.h>
#include <string.h>

void thingy(int argc, char** argv, char** environ) __attribute__((constructor));
void thingy(int argc, char** argv, char** environ)
{
    ZASSERT(argc == 4);
    ZASSERT(!strcmp(argv[1], "42"));
    ZASSERT(!strcmp(argv[2], "hello"));
    ZASSERT(!strcmp(argv[3], "1410"));
}

int main(int argc, char** argv)
{
    ZASSERT(argc == 4);
    ZASSERT(!strcmp(argv[1], "42"));
    ZASSERT(!strcmp(argv[2], "hello"));
    ZASSERT(!strcmp(argv[3], "1410"));
    return 0;
}

