#include <stdio.h>
#include <stdfil.h>

struct bad_args {
    char* x;
    char* y;
};
static __attribute__((__noinline__)) void thingy(char* x)
{
    struct bad_args* args = zargs();
    ZASSERT(args->x == x);
    printf("%s\n", args->x);
    printf("%s\n", args->y);
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    thingy("hello");
    return 0;
}

