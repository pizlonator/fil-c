#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usecapmem_store(unsigned long *z, unsigned long *buf, unsigned long v);
unsigned long usecapmem_load(unsigned long *z, unsigned long *buf);
void usecapmem_disp_store(unsigned long *z, unsigned long *buf, unsigned long v);
unsigned long usecapmem_disp_load(unsigned long *z, unsigned long *buf);
void usecapmem_vcopy(unsigned long *z, void *src, void *dst);
void usecapmem_vstore_base(unsigned long *z, void *dst, void *src);

int main(void)
{
    unsigned long *z = malloc(32);
    unsigned long *buf = malloc(32);
    unsigned char *src = malloc(32);
    unsigned char *dst = malloc(32);
    unsigned i;
    if (!z || !buf || !src || !dst)
        return 1;
    buf[0] = 0;

    usecapmem_store(z, buf, 0xAB);
    if (buf[0] != 0xAB) {
        printf("FAIL usecapmem store\n");
        return 1;
    }
    if (usecapmem_load(z, buf) != 0xAB) {
        printf("FAIL usecapmem load\n");
        return 1;
    }

    usecapmem_disp_store(z, buf, 0xCD);
    if (buf[2] != 0xCD) {
        printf("FAIL usecapmem disp store\n");
        return 1;
    }
    if (usecapmem_disp_load(z, buf) != 0xCD) {
        printf("FAIL usecapmem disp load\n");
        return 1;
    }

    for (i = 0; i < 32; ++i)
        src[i] = (unsigned char)(i * 3 + 1);
    memset(dst, 0, 32);
    usecapmem_vcopy(z, src, dst);
    if (memcmp(src, dst, 32) != 0) {
        printf("FAIL usecapmem vcopy\n");
        return 1;
    }

    memset(dst, 0, 32);
    usecapmem_vstore_base(z, dst, src);
    if (memcmp(src, dst, 32) != 0) {
        printf("FAIL usecapmem vstore base\n");
        return 1;
    }

    printf("usecapmem int ok\n");
    return 0;
}
