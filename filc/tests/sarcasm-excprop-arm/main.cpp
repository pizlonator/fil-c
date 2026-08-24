#include <cstdio>

extern "C" long maythrow(long x);
extern "C" long wrapper(long x);

/* The C++ callee throws; the exception must unwind through the sarcasm
   asm frame of wrapper() (which has no landing pad) into main's handler. */
long maythrow(long x)
{
    if (x == 1)
        throw 42;
    return x * 10;
}

int main()
{
    try {
        printf("%ld\n", wrapper(2));   /* (2*10)+1 = 21, no throw */
        wrapper(1);                    /* throws 42 through the asm frame */
        printf("no throw (bad)\n");
    } catch (int e) {
        printf("caught %d\n", e);
    }
    return 0;
}
