#include <stdfil.h>

int main(void)
{
    /* STD (Set Direction Flag) sets the DF flag in EFLAGS to 1. Unlike the
       condition flags (CF, ZF, etc.), DF is NOT covered by the "cc" clobber
       in GCC/clang inline assembly. The System V x86-64 ABI requires DF to
       be 0 at all sequence points, and the compiler (and Fil-C runtime)
       generate string operations (rep movs/stos) that assume DF=0. Setting
       DF=1 causes those operations to run backwards, corrupting memory. There
       is no way to declare DF as a clobber, so STD is not safe for inline
       assembly. (CLD, which clears DF to the expected 0, is safe.) */
    asm volatile("std" ::: "cc");
    return 0;
}
