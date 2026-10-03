#include <stdio.h>

extern long idxmask_oob(unsigned long idx);

int main(void)
{
    printf("so far so good\n");
    fflush(stdout);
    // The 128-byte buffer with a 64-byte masked access allows idx in
    // [0, 64]; 65 puts the access's last byte one past the buffer.
    long v = idxmask_oob(65);
    printf("unexpectedly returned %ld\n", v);
    return 1;
}
