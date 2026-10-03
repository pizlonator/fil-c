#include <stdio.h>

long branch_je(long x);
long branch_jmp(long x);

int main(void)
{
    /* je taken (x == 0): the in-run ret returns x unchanged */
    if (branch_je(0) != 0) {
        printf("branch_je(0)=%ld\n", branch_je(0));
        return 1;
    }
    /* je not taken: addl $10 + inc => x + 11 */
    if (branch_je(31) != 42) {
        printf("branch_je(31)=%ld\n", branch_je(31));
        return 1;
    }
    /* the jmp variant always skips: returns x unchanged */
    if (branch_jmp(42) != 42 || branch_jmp(0) != 0) {
        printf("branch_jmp: %ld %ld\n", branch_jmp(42), branch_jmp(0));
        return 1;
    }
    printf("bytes branch att ok\n");
    return 0;
}
