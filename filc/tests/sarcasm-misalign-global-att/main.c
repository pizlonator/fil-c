#include <stdio.h>

unsigned long cap_load_mis(void);
unsigned long extcap_load_mis(void);
void cap_store_mis(unsigned long v);
void extcap_store_mis(unsigned long v);

int main(void)
{
    /* Same-file .data object: bytes 4..11 are 0x3333333322222222. */
    if (cap_load_mis() != 0x3333333322222222UL) {
        printf("FAIL same-file misaligned global %lx\n", cap_load_mis());
        return 1;
    }
    /* Extern object: bytes 4..11 are 0x2222222211111111. */
    if (extcap_load_mis() != 0x2222222211111111UL) {
        printf("FAIL extern misaligned global %lx\n", extcap_load_mis());
        return 1;
    }
    /* Misaligned stores land the same bytes back. */
    cap_store_mis(0xAAAAAAAAAAAAAAAAUL);
    if (cap_load_mis() != 0xAAAAAAAAAAAAAAAAUL) {
        printf("FAIL same-file misaligned store %lx\n", cap_load_mis());
        return 1;
    }
    extcap_store_mis(0xBBBBBBBBBBBBBBBBUL);
    if (extcap_load_mis() != 0xBBBBBBBBBBBBBBBBUL) {
        printf("FAIL extern misaligned store %lx\n", extcap_load_mis());
        return 1;
    }
    printf("misalign global att ok\n");
    return 0;
}
