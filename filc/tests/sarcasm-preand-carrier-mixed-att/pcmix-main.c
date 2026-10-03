#include <stdio.h>

extern long pcmix(long x);

int main(void)
{
    if (pcmix(40) != 80) {
        printf("FAIL pcmix %ld\n", pcmix(40));
        return 1;
    }
    if (pcmix(-3) != -6) {
        printf("FAIL pcmix neg\n");
        return 1;
    }
    printf("preand carrier mixed att ok\n");
    return 0;
}
