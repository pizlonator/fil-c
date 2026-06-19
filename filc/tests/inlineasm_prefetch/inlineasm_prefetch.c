#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    char buf[256];
    for (int i = 0; i < 256; ++i)
        buf[i] = (char)i;

    /* PREFETCH* instructions are non-faulting cache hints that do not affect
       program behavior. They take a single memory-addressing expression, but
       the address is passed as an integer register: the pointer is cast to
       intptr_t because Fil-C's safe inline asm does not yet accept pointer
       operands directly (see the FIXME in validateSafeInlineAsm). No
       load/store is performed, so the buffer is left untouched. */
    intptr_t addr = (intptr_t)buf;

    asm volatile("prefetchw (%0)" : : "r"(addr));
    asm volatile("prefetcht0 (%0)" : : "r"(addr));
    asm volatile("prefetcht1 (%0)" : : "r"(addr));
    asm volatile("prefetcht2 (%0)" : : "r"(addr));
    asm volatile("prefetchnta (%0)" : : "r"(addr));

    /* Addressing expressions with an immediate displacement and a scaled
       index, exactly like lea's source. Both placeholders must be input
       registers. */
    intptr_t off = 64;
    asm volatile("prefetchw 8(%0)" : : "r"(addr));
    asm volatile("prefetcht0 16(%0, %1, 2)" : : "r"(addr), "r"(off));

    /* Verify the buffer was not modified by any of the prefetch hints. */
    for (int i = 0; i < 256; ++i)
        ZASSERT(buf[i] == (char)i);

    return 0;
}
