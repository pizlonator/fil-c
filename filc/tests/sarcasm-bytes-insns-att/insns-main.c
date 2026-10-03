#include <stdio.h>

long byte_seq(long x);
long byte_sib(long* arr, long i);

int main(void)
{
    /* leaq 7(%rdi),%rax; addq $3,%rax; store/load through the frame slot;
       movl %ebx,%eax => 32-bit truncation of x + 10 */
    if (byte_seq(100) != 110) {
        printf("byte_seq=%ld want 110\n", byte_seq(100));
        return 1;
    }
    /* the final movl %ebx,%eax zero-extends: the result is (x+10) mod 2^32 */
    if (byte_seq(0x123456789abcdef0L) != (long)0x9abcdefaL) {
        printf("byte_seq truncation wrong: %lx\n", byte_seq(0x123456789abcdef0L));
        return 1;
    }
    /* leaq (%rsi,%rsi,2),%rax; movq 8(%rdi,%rax,8),%rax; addq $1,%rax
       => arr[3i + 1] + 1 */
    static long arr[32];
    for (long i = 0; i < 32; i++)
        arr[i] = i * 1000;
    if (byte_sib(arr, 2) != arr[7] + 1) {
        printf("byte_sib=%ld want %ld\n", byte_sib(arr, 2), arr[7] + 1);
        return 1;
    }
    if (byte_sib(arr, 9) != arr[28] + 1) {
        printf("byte_sib=%ld want %ld\n", byte_sib(arr, 9), arr[28] + 1);
        return 1;
    }
    printf("bytes insns att ok\n");
    return 0;
}
