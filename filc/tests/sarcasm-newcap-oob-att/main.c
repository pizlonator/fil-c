#include <stdio.h>
#include <stdlib.h>

void newcap_oob_store(unsigned long *z, unsigned long *buf, unsigned long v);

int main(void)
{
    unsigned long *z = malloc(32);
    unsigned long *buf = malloc(32);
    if (!z || !buf)
        return 1;
    printf("expect trap:\n");
    newcap_oob_store(z, buf, 0xAB); /* buf+100 is out of bounds */
    printf("SHOULD NOT PRINT\n");
    return 0;
}
