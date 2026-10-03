#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long reuse_alias(long* key, long mode, unsigned char* inp, long ahead, long count);

int main(void)
{
    long* key = malloc(64);
    unsigned char* inp = malloc(512);
    if (!key || !inp)
        return 1;
    /* Key bytes are deliberately unlike input bytes: a load checked against
       the key's object would trap, and a load of key bytes would mismatch. */
    for (int i = 0; i < 8; i++)
        key[i] = 0x5A5A5A5A5A5A5A5AL + i;
    for (int i = 0; i < 512; i++)
        inp[i] = (unsigned char)(i + 1);
    /* Fast path (mode != 0), three blocks, ahead=0x160: iterations 1-2 see
       ahead >= 0x70 (cf=0, rbp = inp+128), iteration 3 sees ahead < 0x70
       (cf=1, rbp = advanced inp+0). All three loads must come from the
       input buffer: inp+128, inp+256, inp+256. */
    long v1, v2;
    memcpy(&v1, inp + 128, 8);
    memcpy(&v2, inp + 256, 8);
    long want = v1 | v2;
    long v = reuse_alias(key, 1, inp, 0x160, 3);
    if (v != want) {
        printf("FAIL: v=%lx want %lx\n", v, want);
        return 1;
    }
    /* Slow path (mode == 0): plain input load, no rbp reuse. */
    memcpy(&want, inp, 8);
    v = reuse_alias(key, 0, inp, 0, 0);
    if (v != want) {
        printf("FAIL2: v=%lx want %lx\n", v, want);
        return 1;
    }
    printf("alias reuse att ok\n");
    return 0;
}
