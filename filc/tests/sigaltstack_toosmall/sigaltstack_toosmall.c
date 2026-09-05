/* An alternate signal stack too small for Fil-C must be rejected rather than
   silently registered: Fil-C runs the handler on it with its own stack-height
   checking, which reserves a slack region at the base, so a bare-minimum kernel
   size leaves no usable room. */

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>

int main(void)
{
    void* mem = malloc(1 << 15); /* 32 KiB: below Fil-C's alternate-stack minimum. */
    ZASSERT(mem);
    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = mem;
    ss.ss_size = 1 << 15;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL); /* must trap: too small for Fil-C */
    return 0;
}
