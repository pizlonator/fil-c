#include <stdfil.h>

int main(void)
{
    unsigned short sel;
    unsigned int ar;

    /* Read the current code segment selector at runtime. */
    asm volatile("mov %%cs, %0" : "=r"(sel));

    /* Load access rights for the current code selector. */
    asm volatile("lar %1, %0"
                 : "=r"(ar)
                 : "r"(sel)
                 : "cc");

    /* LAR places descriptor bytes 6:5 in bits 23:8 of the 32-bit result and
       clears bits 31:24 and 7:0. For a valid code segment, the present bit
       (AR bit 7, result bit 15) and the S bit (AR bit 4, result bit 12) are
       both set. */
    ZASSERT((ar & 0xff0000ff) == 0);
    ZASSERT((ar & 0x9000) == 0x9000);

    return 0;
}
