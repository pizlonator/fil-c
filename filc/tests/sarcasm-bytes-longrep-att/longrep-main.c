#include <stdio.h>
#include <string.h>

void longrep_movsb(char* dst, char* src, size_t len);
void longrep_movsl(char* dst, char* src, size_t len);
void longrep_stosb(char* dst, long fill);

int main(void)
{
    /* `.long 0x9066A4F3` copies exactly like `rep movsb`. */
    static char src[64];
    static char dst[64];
    memset(src, 0x5a, sizeof(src));
    longrep_movsb(dst, src, 64);
    for (int i = 0; i < 64; i++) {
        if (dst[i] != 0x5a) {
            printf("longrep_movsb: dst[%d]=%d\n", i, dst[i]);
            return 1;
        }
    }
    /* the qword form (48 F3 A5 = rep movsq) copies 8 bytes per count */
    memset(dst, 0, sizeof(dst));
    longrep_movsl(dst, src, 8);
    for (int i = 0; i < 8; i++) {
        if (dst[i] != 0x5a) {
            printf("longrep_movsl: dst[%d]=%d\n", i, dst[i]);
            return 1;
        }
    }
    /* `.long 0x9066AAF3` = rep stosb: rax (al) is the fill value */
    memset(dst, 0, sizeof(dst));
    longrep_stosb(dst, 0x3c);
    for (int i = 0; i < 32; i++) {
        if (dst[i] != 0x3c) {
            printf("longrep_stosb: dst[%d]=%d\n", i, dst[i]);
            return 1;
        }
    }
    printf("bytes longrep att ok\n");
    return 0;
}
