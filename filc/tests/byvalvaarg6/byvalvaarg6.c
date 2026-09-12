#include <stdarg.h>
#include <stdbool.h>
#include <stdfil.h>

/* Regression test: multiple small aggregates passed through varargs and read
   back-to-back. This mirrors how gas's insert_fields (opcodes/aarch64-opc.h)
   passes aarch64_field structs through varargs. Small aggregates must occupy
   slots in the varargs snapshot that the va_arg site's stride agrees with;
   every va_arg after the first one read garbage when the call site coerced
   these aggregates to [2 x ptr]/[2 x i64]. */

typedef struct {
    unsigned width:8;
    unsigned num:7;
    bool is_const:1;
} F;

typedef struct {
    int i;
    int j;
    int k;
} S;

/* Pointer-containing aggregates through varargs.  Both are 8-byte aligned and
   ≤16 bytes.  P1 is the sharpest regression case: the call site used to coerce
   it to a 16-byte [2 x ptr] slot while va_arg only read 8 bytes, so the
   varargs stride mismatched and everything after the first P1 read garbage. */

typedef struct {
    void *p;
} P1;

typedef struct {
    void *p;
    void *q;
} P2;

static F get_field(unsigned width, unsigned num, bool is_const)
{
    F f;
    f.width = width;
    f.num = num;
    f.is_const = is_const;
    return f;
}

static unsigned fn(int cnt, ...)
{
    va_list ap;
    unsigned result = 0;
    va_start(ap, cnt);
    while (cnt--) {
        F f = va_arg(ap, F);
        result = result * 10 + f.width + f.num + f.is_const;
    }
    S s = va_arg(ap, S);
    va_end(ap);
    return result + s.i + s.j + s.k;
}

/* Pointer-containing aggregates mixed with other args.  The P1s after the
   first vararg position exercise stride accounting, and the scalar vararg in
   the middle makes sure aggregate and non-aggregate slots stay in sync.  Every
   pointer is dereferenced (read and written) in the callee and the writes are
   verified back in the caller so pointer metadata propagation through varargs
   is exercised, too. */

static unsigned pfn(int cnt, ...)
{
    va_list ap;
    unsigned result = 0;
    va_start(ap, cnt);

    P1 first = va_arg(ap, P1);
    ZASSERT(first.p);
    ZASSERT(*(unsigned*)first.p == 100);
    *(unsigned*)first.p = 200;
    result += *(unsigned*)first.p;

    unsigned scalar = va_arg(ap, unsigned);
    ZASSERT(scalar == 10);
    result += scalar;

    while (cnt--) {
        P1 p1 = va_arg(ap, P1);
        ZASSERT(p1.p);
        ZASSERT(*(unsigned*)p1.p == 300);
        *(unsigned*)p1.p = 400;
        result += *(unsigned*)p1.p;
    }

    P2 p2 = va_arg(ap, P2);
    ZASSERT(p2.p && p2.q);
    ZASSERT(*(unsigned*)p2.p == 1000);
    ZASSERT(*(unsigned*)p2.q == 2000);
    *(unsigned*)p2.p = 3000;
    *(unsigned*)p2.q = 4000;
    result += *(unsigned*)p2.p + *(unsigned*)p2.q;

    va_end(ap);
    return result;
}

int main()
{
    ZASSERT(fn(3, get_field(1, 22, false), get_field(6, 16, false),
               get_field(6, 10, false),
               (S){666, 1410, 9000}) ==
           (1 + 22) * 100 + (6 + 16) * 10 + (6 + 10) + 666 + 1410 + 9000);

    unsigned v1 = 100, v2 = 300, v3 = 300, v4 = 1000, v5 = 2000;
    ZASSERT(pfn(2, (P1){&v1}, 10, (P1){&v2}, (P1){&v3}, (P2){&v4, &v5}) ==
           200 + 10 + 400 + 400 + 3000 + 4000);
    ZASSERT(v1 == 200);
    ZASSERT(v2 == 400 && v3 == 400);
    ZASSERT(v4 == 3000 && v5 == 4000);

    return 0;
}
