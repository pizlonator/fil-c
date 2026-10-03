#include <stdio.h>

long movbe_bytes(int* cell, long x);

int main(void)
{
    static int cell;
    cell = 0;
    /* store byteswapped through the pointer, then load it back with the
       byte-swapping load and read it plain: both reads report the swap */
    long got = movbe_bytes(&cell, 0x12345678L);
    if (got != (long)0x78563412u) {
        printf("movbe_bytes got: %lx\n", got);
        return 1;
    }
    if (cell != (int)__builtin_bswap32(0x12345678u)) {
        printf("movbe_bytes cell: %x\n", cell);
        return 1;
    }
    cell = 0;
    got = movbe_bytes(&cell, (long)0xdeadbeefu);
    if (got != (long)0xefbeaddeu || cell != (int)__builtin_bswap32(0xdeadbeefu)) {
        printf("movbe_bytes 2: %lx %x\n", got, cell);
        return 1;
    }
    printf("bytes movbe att ok\n");
    return 0;
}
