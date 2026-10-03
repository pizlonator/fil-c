/* fflush after every line: the final SIGILL cannot flush a fully-buffered
 * stdout, so without this the harness would lose the proof line printed
 * before the trap. */
#include <stdio.h>
#include <string.h>

void pd_wrap(void *src, void *dst, unsigned long n);

int main() {
    static unsigned char inb[32], outb[32];
    for (int i = 0; i < 32; i++)
        inb[i] = (unsigned char)(i * 3 + 1);
    memset(outb, 0, sizeof outb);

    printf("pushfq df=1 wrap (SHOULD TRAP):\n");
    fflush(stdout);
    pd_wrap(inb, outb, 16);
    printf("NOT REACHED\n");
    return 0;
}
