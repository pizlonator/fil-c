#include <stdio.h>
#include <stdlib.h>

void newcap_store(unsigned long *z, unsigned long *buf, unsigned long v);
unsigned long newcap_load(unsigned long *z, unsigned long *buf);
unsigned long newcap_typo(unsigned long *z, unsigned long *buf);

int main(void)
{
    unsigned long *z = malloc(32);
    unsigned long *buf = malloc(32);
    if (!z || !buf)
        return 1;
    buf[0] = 0;

    newcap_store(z, buf, 0xAB);
    if (buf[0] != 0xAB) {
        printf("FAIL newcap store\n");
        return 1;
    }
    if (newcap_load(z, buf) != 0xAB) {
        printf("FAIL newcap load\n");
        return 1;
    }
    if (newcap_typo(z, buf) != 0xAB) {
        printf("FAIL newcap typo\n");
        return 1;
    }

    printf("newcap int ok\n");
    return 0;
}
