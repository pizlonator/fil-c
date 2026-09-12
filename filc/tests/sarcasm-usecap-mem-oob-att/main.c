#include <stdio.h>
#include <stdlib.h>

void usecapmem_oob_vstore(unsigned long *z, void *buf);

int main(void)
{
    unsigned long *z = malloc(32);
    unsigned char *buf = malloc(32);
    if (!z || !buf)
        return 1;
    printf("expect trap:\n");
    usecapmem_oob_vstore(z, buf); /* buf+100 is out of bounds */
    printf("SHOULD NOT PRINT\n");
    return 0;
}
