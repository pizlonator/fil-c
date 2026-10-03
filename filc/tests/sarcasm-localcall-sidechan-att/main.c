#include <stdio.h>
#include <stdlib.h>

long set_dec_alias(long* a, long b, long* c);
long set_enc_alias(long* a, long b, long* c);

int main(void)
{
    long* k = malloc(256);
    if (!k)
        return 1;
    for (int i = 0; i < 32; i++)
        k[i] = 1000 + i;
    /* The alias callee sets esi=9; the caller shifts it to 144 and returns
       c+16+144. A signature-marshalled call would lose %esi. */
    long r = set_dec_alias(k, 0, k);
    long want = (long)k + 16 + 144;
    if (r != want) {
        printf("FAIL: r=%ld want=%ld\n", r, want);
        return 1;
    }
    /* The exported entry still works as an ordinary Fil-C call. */
    long e = set_enc_alias(k, 0, k);
    if (e != 0) {
        printf("FAIL: enc=%ld\n", e);
        return 1;
    }
    printf("localcall sidechan att ok\n");
    return 0;
}
