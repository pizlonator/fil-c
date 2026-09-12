#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long alias_add(unsigned long *a, unsigned long *b);
unsigned long alias_sub_typo(unsigned long *p, unsigned long off);
unsigned long alias_mov_new(unsigned long *z, unsigned long *buf);
void alias_mov_new_typo(unsigned long *z, unsigned long *buf, unsigned long v);
unsigned long alias_mov_use_typo(unsigned long *z, unsigned long *buf);
void alias_vcopy_new(unsigned long *z, void *src, void *dst);
void alias_vstore_new_typo(unsigned long *z, void *dst, void *src);
void alias_vcopy_use_typo(unsigned long *z, void *src, void *dst);
unsigned long dual_usecap(unsigned long *z, unsigned long *buf);
unsigned long dual_newcap(unsigned long *z, unsigned long *buf);

int main(void)
{
    unsigned long *a = malloc(128);
    unsigned long *b = malloc(32);
    unsigned long *t = malloc(64);
    unsigned long *z = malloc(32);
    unsigned long *buf = malloc(32);
    unsigned char *src = malloc(32);
    unsigned char *dst = malloc(32);
    unsigned i;
    if (!a || !b || !t || !z || !buf || !src || !dst)
        return 1;
    for (i = 0; i < 16; ++i)
        a[i] = 1000 + i;
    b[0] = 222;
    for (i = 0; i < 8; ++i)
        t[i] = 100 + i;
    buf[0] = 0xAB;

    /* rax = b-b = 0, so rdi stays a: the alias keeps a's cap. */
    if (alias_add(a, b) != 1000) {
        printf("FAIL alias_add\n");
        return 1;
    }
    /* &t[2]-16 is t[0]; the typo-spelled alias is redundant but valid. */
    if (alias_sub_typo(&t[2], 16) != 100) {
        printf("FAIL alias_sub_typo\n");
        return 1;
    }
    if (alias_mov_new(z, buf) != 0xAB) {
        printf("FAIL alias_mov_new\n");
        return 1;
    }
    alias_mov_new_typo(z, buf, 0xCD);
    if (buf[0] != 0xCD) {
        printf("FAIL alias_mov_new_typo\n");
        return 1;
    }
    if (alias_mov_use_typo(z, buf) != 0xCD) {
        printf("FAIL alias_mov_use_typo\n");
        return 1;
    }

    for (i = 0; i < 32; ++i)
        src[i] = (unsigned char)(i * 3 + 1);
    memset(dst, 0, 32);
    alias_vcopy_new(z, src, dst);
    if (memcmp(src, dst, 32) != 0) {
        printf("FAIL alias_vcopy_new\n");
        return 1;
    }
    memset(dst, 0, 32);
    alias_vstore_new_typo(z, dst, src);
    if (memcmp(src, dst, 32) != 0) {
        printf("FAIL alias_vstore_new_typo\n");
        return 1;
    }
    memset(dst, 0, 32);
    alias_vcopy_use_typo(z, src, dst);
    if (memcmp(src, dst, 32) != 0) {
        printf("FAIL alias_vcopy_use_typo\n");
        return 1;
    }

    /* buf[0] holds the address of buf[2] for the dual tests: the add
       loads it, so the result points in bounds of buf's object. */
    buf[0] = (unsigned long)&buf[2];
    buf[2] = 0xAB;
    if (dual_usecap(z, buf) != 0xAB) {
        printf("FAIL dual_usecap\n");
        return 1;
    }
    if (dual_newcap(z, buf) != 0xAB) {
        printf("FAIL dual_newcap\n");
        return 1;
    }

    printf("usecap alias att ok\n");
    return 0;
}
