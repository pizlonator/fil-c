#include <stdfil.h>
#include <inttypes.h>
#include "utils.h"
#include <stdbool.h>
#include <stdlib.h>

enum foo {
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t1,
    t2,
    t3,
    u
};

union wacky {
    enum foo thingy : 8;
    struct {
        enum foo thingy : 8;
        void (*a)(void*);
        void* b;
        intptr_t c;
    } m;
    struct {
        enum foo thingy : 8;
        intptr_t a;
        void* b;
    } n;
    struct {
        enum foo thingy : 8;
        void (*a)(void*);
        void* b;
        void (*c)(void*);
    } o;
    struct {
        enum foo thingy : 8;
        void (*a)(int);
        int b;
    } p;
    struct {
        enum foo thingy : 8;
        void (*a)(intmax_t);
        intmax_t b;
    } q;
    struct {
        enum foo thingy : 8;
        void* a;
        void* b;
    } r;
    struct {
        enum foo thingy : 8;
        void (*a)(void);
    } s;
    struct {
        enum foo thingy : 8;
        void* a;
        void* b;
        void* c;
    } t;
    struct {
        enum foo thingy : 8;
        bool a : 1;
        void* b;
        void* c;
        intptr_t d;
    } u;
};

struct bar {
    int x;
    int y;
    void* z;
};

intptr_t g;

static __attribute__((noinline)) void op1(void* x)
{
    opaque(x);
}

static __attribute__((noinline)) void op2(void* x, void* y)
{
    opaque(x);
    opaque(y);
}

static __attribute__((noinline)) void op3(void* x, void* y)
{
    opaque(x);
    opaque(y);
}

static __attribute__((noinline)) void op4(void* x, void* y, void* z)
{
    opaque(x);
    opaque(y);
    opaque(z);
}

static __attribute__((noinline)) bool op5(void* x, void* y)
{
    ZASSERT(zinbounds(x));
    ZASSERT(zinbounds(y));
    opaque(x);
    opaque(y);
    return true;
}

static __attribute__((noinline)) void op6(void* x, void* y, void* z)
{
    opaque(x);
    opaque(y);
    opaque(z);
}

static inline void domorestuff(union wacky* w)
{
    void* tud = NULL;
    switch (w->thingy) {
    case m:
        g = w->m.c;
        w->m.a(w->m.b);
        break;
    case n:
        op1(w->n.b);
        break;
    case o:
        w->o.a(w->o.b);
        break;
    case p:
        w->p.a(w->p.b);
        break;
    case q:
        w->q.a(w->q.b);
        break;
    case s:
        w->s.a();
        break;
    case r:
        op2(w->r.a, w->r.b);
        break;
    case t1: {
        void* a = w->t.a;
        if (!((uintptr_t)a & 0xf) && ((struct bar*)a)->x == 42) {
            if (((struct bar*)a)->y == 666)
                ((struct bar*)a)->z = w->t.b;
            else
                op3(a, w->t.b);
        }
        break;
    }
        tud = w->t.c;
    case t2:
        op4(w->t.a, w->t.b, tud);
        break;
    case t3: {
        void* a = w->t.a;
        void* b = w->t.c;
        void* c = w->t.b;
        if (op5(a, b))
            op6(a, c, b);
        break;
    }
    default:
        break;
    }
}

static __attribute__((noinline)) void dostuff(union wacky* w, unsigned n)
{
    while (n--) {
        union wacky x;
        x = *w++;
        domorestuff(&x);
    }
}

int main()
{
    union wacky w;
    w.t.thingy = t3;
    w.t.a = "w";
    w.t.b = "t";
    w.t.c = "f";
    dostuff(&w, 1);
    return 0;
}

