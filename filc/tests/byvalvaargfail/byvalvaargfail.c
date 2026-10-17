#include <stdarg.h>
#include <stdfil.h>

typedef struct { int *pointer; long value; } Pair;

__attribute__((noinline)) static void consume(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    Pair pair = va_arg(ap, Pair);
    ZASSERT(pair.value == 41);
    va_end(ap);
}

int main(void)
{
    // Only one word remains: extraction must not copy past the packet's upper
    // bound even though the first word could be a valid pointer.
    int value = 37;
    consume(0, &value);
    zprint("nono\n");
    return 0;
}
