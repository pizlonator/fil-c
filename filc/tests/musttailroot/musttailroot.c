/* A musttail call pops the caller's Fil-C frame, and the callee roots its own
   incoming arguments by recording them in its own prologue. So even though
   this musttail call pops f's frame, p stays live while g runs. */
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>

struct obj { long tag; long pad[7]; };
static zweak* w;

__attribute__((noinline)) long g(struct obj* p)
{
    zgc_request_and_wait();
    zgc_request_and_wait();
    int dead = zweak_get(w) == NULL;
    for (int i = 0; i < 200000; i++) {
        struct obj* q = malloc(sizeof(struct obj));
        q->tag = 1337;
    }
    printf("p is %s, tag %ld\n", dead ? "dead" : "live", p->tag);
    return p->tag;
}

__attribute__((noinline)) long f(struct obj* unused)
{
    struct obj* p = malloc(sizeof(struct obj));
    p->tag = 42;
    w = zweak_new(p);
    __attribute__((musttail)) return g(p);
}

int main(void)
{
    return f(NULL) == 42 ? 0 : 1;
}
