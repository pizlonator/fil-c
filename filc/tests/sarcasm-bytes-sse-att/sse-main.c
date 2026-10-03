#include <stdio.h>
#include <string.h>

void sse_copy16(char* dst, char* src);
void sse_copyd(double* dst, double* src);
void sse_add4(unsigned* dst, unsigned* src);

int main(void)
{
    /* F3 0F 6F (movdqu load) + 66 0F 7F (movdqa store), byte-encoded */
    static char src[16];
    static char dst[16];
    memset(src, 0x77, sizeof(src));
    memset(dst, 0, sizeof(dst));
    sse_copy16(dst, src);
    for (int i = 0; i < 16; i++) {
        if ((unsigned char)dst[i] != 0x77) {
            printf("sse_copy16: dst[%d]=%d\n", i, dst[i]);
            return 1;
        }
    }
    /* F2 0F 10 / F2 0F 11: the scalar-double pair */
    static double dsrc = 3.5, ddst = 0.0;
    sse_copyd(&ddst, &dsrc);
    if (ddst != 3.5) {
        printf("sse_copyd=%f\n", ddst);
        return 1;
    }
    /* F3 0F 6F load + 66 0F FE (paddd) + 66 0F 7F store: four lanes */
    static unsigned isrc[4] = { 1, 2, 3, 4 };
    static unsigned idst[4] = { 10, 20, 30, 40 };
    sse_add4(idst, isrc);
    if (idst[0] != 11 || idst[1] != 22 || idst[2] != 33 || idst[3] != 44) {
        printf("sse_add4: %u %u %u %u\n", idst[0], idst[1], idst[2], idst[3]);
        return 1;
    }
    printf("bytes sse att ok\n");
    return 0;
}
