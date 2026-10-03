#include <stdio.h>
#include <string.h>

void annot_rep(char* dst, char* src);

int main(void)
{
    static char src[16];
    static char dst[16];
    memset(src, 0, sizeof(src));
    memset(dst, 0, sizeof(dst));
    for (int i = 0; i < 16; i++)
        src[i] = (char)('A' + i);
    annot_rep(dst, src);
    if (memcmp(dst, src, 16) != 0) {
        printf("annot_rep: copy mismatch\n");
        return 1;
    }
    printf("bytes annot-rep att ok\n");
    return 0;
}
