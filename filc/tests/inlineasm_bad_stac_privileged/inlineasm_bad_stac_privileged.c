#include <stdfil.h>

int main(void)
{
    /* STAC (Set AC Flag) is a privileged instruction that sets the alignment
       check flag in EFLAGS. It requires CPL=0 and the SMAP feature; attempts
       to execute it in user mode generate #UD. Like CLAC, it manipulates
       supervisor-mode access prevention state and is not safe for inline
       assembly. */
    asm volatile("stac");
    return 0;
}
