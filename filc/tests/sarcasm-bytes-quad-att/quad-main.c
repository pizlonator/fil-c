#include <stdio.h>

long quad_two(long x);
long quad_mixed(long x);
long quad_seq(long x);

int main(void)
{
    /* the quad's bytes decode to `movq %rbx,%rax; ret` (+ nop padding) */
    if (quad_two(42) != 42) {
        printf("quad_two=%ld\n", quad_two(42));
        return 1;
    }
    /* one quad encoding three instructions (leaq/addl/ret), then a .byte run
       holding a dead-but-decoded addq: one run across directive sizes */
    if (quad_mixed(35) != 42) {
        printf("quad_mixed=%ld\n", quad_mixed(35));
        return 1;
    }
    /* spelled movq $0,%rax; byte-encoded addq %rdi,%rax; addq $2,%rax; ret */
    if (quad_seq(40) != 42) {
        printf("quad_seq=%ld\n", quad_seq(40));
        return 1;
    }
    printf("bytes quad att ok\n");
    return 0;
}
