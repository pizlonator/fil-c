#include <stdio.h>
#include <stdlib.h>

void *hidden_roundtrip(void **slot, void *v);

int main(void)
{
    unsigned char *buf = malloc(16);
    void **slot = malloc(sizeof(void *));
    if (!buf || !slot)
        return 1;
    for (int i = 0; i < 16; i++)
        buf[i] = (unsigned char)(i + 1);

    /* The store's `#! store ptr` sits inside a `#` comment, so sarcasm never
       sees it: the round-tripped value carries no capability and the load
       below (or the dereference) must trap. */
    void *p = hidden_roundtrip(&slot[0], &buf[3]);
    printf("expect trap:\n");
    printf("%d SHOULD NOT PRINT\n", *(unsigned char *)p);
    return 0;
}
