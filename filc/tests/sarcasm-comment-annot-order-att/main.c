#include <stdio.h>
#include <stdlib.h>

void order_store(void **slot, void *v);
void *order_load(void **slot);
void *order_load_semi(void **slot);

int main(void)
{
    unsigned char *buf = malloc(16);
    void **slot = malloc(2 * sizeof(void *));
    if (!buf || !slot)
        return 1;
    for (int i = 0; i < 16; i++)
        buf[i] = (unsigned char)(i + 1);
    slot[0] = NULL;
    slot[1] = NULL;

    /* `#! store ptr # note` / `#! load ptr # note` round-trip the capability:
       the trailing `#` comment is stripped from the annotation body. */
    order_store(&slot[0], &buf[3]);
    unsigned char *p = (unsigned char *)order_load(&slot[0]);
    if (p != &buf[3] || *p != 4) {
        printf("comment-annot-order BAD\n");
        return 1;
    }
    order_store(&slot[1], NULL);
    if (order_load(&slot[1]) != NULL) {
        printf("comment-annot-order BAD null\n");
        return 1;
    }
    /* Same via the universal `;!` marker with a trailing `#` comment. */
    if (order_load_semi(&slot[0]) != &buf[3]) {
        printf("comment-annot-order BAD semi\n");
        return 1;
    }

    printf("comment annot order ok\n");
    return 0;
}
