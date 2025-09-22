#include <inttypes.h>

struct state {
    uint32_t b, a;
};

void fib_init(struct state *s);
void fib_init(struct state *s)
{
    s->a = 0;
    s->b = 1;
}

void fib_next(struct state *s);
void fib_next(struct state *s)
{
    uint32_t next = s->a + s->b;
    s->a = s->b;
    s->b = next;
}
