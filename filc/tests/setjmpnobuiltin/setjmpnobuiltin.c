/* With -fno-builtin, clang does not mark setjmp returns_twice; the pass must
   recognize it by name. */
#include <setjmp.h>
#include <stdfil.h>
#include <stdio.h>

static jmp_buf jb;
static sigjmp_buf sjb;

static __attribute__((noinline)) void jump(int value)
{
    longjmp(jb, value);
}

static __attribute__((noinline)) void sigjump(int value)
{
    siglongjmp(sjb, value);
}

int main()
{
    volatile int count = 0;
    int result = setjmp(jb);
    if (result < 3) {
        count++;
        jump(result + 1);
    }
    ZASSERT(result == 3);
    ZASSERT(count == 3);

    volatile int reached = 0;
    if (!sigsetjmp(sjb, 1)) {
        reached = 1;
        sigjump(7);
    }
    ZASSERT(reached == 1);

    int value = _setjmp(jb);
    if (!value)
        _longjmp(jb, 5);
    ZASSERT(value == 5);

    printf("setjmp nobuiltin ok\n");
    return 0;
}
