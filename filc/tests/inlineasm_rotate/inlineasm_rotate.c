#include <stdfil.h>

int main(void)
{
    unsigned int x;
    unsigned char c;

    /* ROL by immediate: 0x1 -> 0x10 */
    x = 1;
    asm("roll $4, %0" : "+r"(x) : : "cc");
    zprintf("rol = %x\n", x);

    /* ROR by immediate: 0x10 -> 0x1 */
    x = 0x10;
    asm("rorl $4, %0" : "+r"(x) : : "cc");
    zprintf("ror = %x\n", x);

    /* ROL by CL count: 0x1 -> 0x10 */
    x = 1;
    c = 4;
    asm("roll %1, %0" : "+r"(x) : "c"(c) : "cc");
    zprintf("rol cl = %x\n", x);

    /* RCL by 1 with CF cleared (clc): 0x1 -> 0x2 */
    x = 1;
    asm("clc\n\trcll $1, %0" : "+r"(x) : : "cc");
    zprintf("rcl = %x\n", x);

    /* RCR by 1 with CF cleared (clc): 0x1 -> 0x0 (CF becomes 1) */
    x = 1;
    asm("clc\n\trcrl $1, %0" : "+r"(x) : : "cc");
    zprintf("rcr = %x\n", x);

    /* One-operand form (rotate by 1): 0x1 -> 0x2 */
    x = 1;
    asm("roll %0" : "+r"(x) : : "cc");
    zprintf("rol one = %x\n", x);

    return 0;
}
