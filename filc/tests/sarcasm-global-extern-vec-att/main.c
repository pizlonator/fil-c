/* Extern globals with `#! global ptr`: the flight pointer materializes by
   calling pizlonated_g@PLT (getter; rdi=myth, rsi=0 -> rax=iv, rdx=lo).
   Vector (movdqa/movdqu) loads and stores, both the lea form and the
   direct form, plus a scalar RMW op on the same global. */
#include <stdio.h>
#include <string.h>
void vec_lea_load(void *buf);
void vec_direct_load(void *buf);
void vec_direct_store(void *buf);
long vec_rmw(void);
extern int g[8];

int main()
{
    int buf[8];
    int src[4];

    vec_lea_load(buf);
    for (int i = 0; i < 8; i++) {
        if (buf[i] != i + 1) {
            printf("FAIL vec_lea_load\n");
            return 1;
        }
    }

    vec_direct_load(buf);
    for (int i = 0; i < 8; i++) {
        if (buf[i] != i + 1) {
            printf("FAIL vec_direct_load\n");
            return 1;
        }
    }

    for (int i = 0; i < 4; i++) src[i] = 41 + i;
    vec_direct_store(src);
    for (int i = 0; i < 4; i++) {
        if (g[i] != 41 + i) {
            printf("FAIL vec_direct_store\n");
            return 1;
        }
    }

    g[2] = 0x0f1e2d3b;
    if (vec_rmw() != 1 || g[2] != 0x0f1e2d3c) {
        printf("FAIL vec_rmw\n");
        return 1;
    }

    printf("extern vec att ok\n");
    return 0;
}
