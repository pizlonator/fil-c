#include <stdfil.h>

int main(void)
{
    unsigned int sel;
    unsigned int limit;
    unsigned int ok = 0;

    /* Read the current code segment selector at runtime instead of assuming
       the Linux x86-64 GDT layout. */
    asm volatile("mov %%cs, %0"
                 : "=r"(sel));

    /* Load the segment limit for the current code selector. LSL sets ZF on
       success; use cmovz to capture that into ok. */
    asm volatile("lsl %2, %0\n\t"
                 "cmovz %3, %1"
                 : "=r"(limit), "+r"(ok)
                 : "r"(sel), "r"(1u)
                 : "cc");

    /* LSL should succeed and the user code descriptor has a page-granular
       limit of 0xfffff, which LSL scales to 0xffffffff. */
    ZASSERT(ok);
    ZASSERT(limit == 0xffffffffu);

    return 0;
}
