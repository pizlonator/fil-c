#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

unsigned long dec_select(unsigned long *table, unsigned long *key);
unsigned long shr_ptr_kill(unsigned long *table, unsigned long *key);
unsigned long sub_ptr_zero_add(unsigned long *base, unsigned long *other);

int main(void)
{
    unsigned long *table = malloc(4096);
    unsigned long *key = malloc(256);
    unsigned long *base = malloc(128);
    unsigned long *other = malloc(32);
    if (!table || !key || !base || !other)
        return 1;
    for (int i = 0; i < 512; i++)
        table[i] = 1000 + i;
    for (int i = 0; i < 32; i++)
        key[i] = 5000 + i;
    for (int i = 0; i < 16; i++)
        base[i] = 2000 + i;
    other[0] = 777;

    /* The AES_decrypt Td-select: table + (diff&0x300) + ((diff&0x300)>>3). */
    {
        uintptr_t diff = (uintptr_t)key - (uintptr_t)table;
        unsigned long masked = diff & 0x300;
        unsigned long want =
            *(unsigned long *)((char *)table + masked + (masked >> 3));
        unsigned long got = dec_select(table, key);
        if (got != want) {
            printf("FAIL dec_select: got %lu want %lu\n", got, want);
            return 1;
        }
    }
    /* Shift kills the key-derived web: table + ((key&0x3f0)>>4). */
    {
        unsigned long off = ((uintptr_t)key & 0x3f0) >> 4;
        unsigned long want = *(unsigned long *)((char *)table + off);
        unsigned long got = shr_ptr_kill(table, key);
        if (got != want) {
            printf("FAIL shr_ptr_kill: got %lu want %lu\n", got, want);
            return 1;
        }
    }
    /* key-key is integer zero: the add keeps base's capability. */
    if (sub_ptr_zero_add(base, other) != 2000) {
        printf("FAIL sub_ptr_zero_add\n");
        return 1;
    }

    printf("sub kill att ok\n");
    return 0;
}
