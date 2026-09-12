#include <stdio.h>

extern unsigned char aesni_mb_sink[4096];
void sink_store(long off, long v);
unsigned long sink_load(long off);

int main(void)
{
    sink_store(0, 0x41);
    sink_store(4095, 0x42);
    sink_store(2048, 0x43);
    if (sink_load(0) != 0x41 || sink_load(4095) != 0x42 || sink_load(2048) != 0x43) {
        printf("FAIL asm-side readback\n");
        return 1;
    }
    /* C sees the same object through the emitted getter. */
    if (aesni_mb_sink[0] != 0x41 || aesni_mb_sink[4095] != 0x42
        || aesni_mb_sink[2048] != 0x43) {
        printf("FAIL C-side read\n");
        return 1;
    }
    aesni_mb_sink[100] = 0x44;
    if (sink_load(100) != 0x44) {
        printf("FAIL C-side write\n");
        return 1;
    }
    printf("comm4096 att ok\n");
    return 0;
}
