#include "value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(int value) { return value + 13; }

static void aligned_volatile_values(int* first, int* second)
{
    struct __attribute__((packed, aligned(8))) Packed {
        union Scalar value;
        unsigned char suffix;
    };
    // Packing is allowed when all pointer offsets and array strides are aligned.
    volatile struct Packed* p = malloc(sizeof(struct Packed));
    p->value.pointer = first;
    p->suffix = 97;
    union Scalar got = scalar(p->value);
    assert(*(int*)got.pointer == 42);
    union Scalar replacement = { .pointer = second };
    p->value = scalar(replacement);
    assert(*(int*)p->value.pointer == 11 && p->suffix == 97);
    free((void*)p);
}

static void nested_values(void)
{
    struct {
        unsigned char prefix[7];
        union Scalar one;
        unsigned short gap;
        union WideValue two;
        unsigned char suffix;
    } p = { .prefix = { 0, 0, 0, 19 }, .one.integer = -123456789,
            .gap = 73, .two.bits = { 0x0123456789abcdefUL, 0xfedcba9876543210UL },
            .suffix = 97 };
    // Nested scalar alternatives keep all bits through pointer-shaped carriers.
    union Scalar one = scalar(p.one);
    assert(one.integer == -123456789);
    union WideValue two = wide_value(p.two);
    assert(two.bits[0] == 0x0123456789abcdefUL && two.bits[1] == 0xfedcba9876543210UL);
    p.one = scalar(p.one);
    p.two = wide_value(p.two);
    assert(p.one.integer == -123456789);
    assert(p.two.bits[0] == 0x0123456789abcdefUL && p.two.bits[1] == 0xfedcba9876543210UL);
    assert(p.prefix[3] == 19 && p.gap == 73 && p.suffix == 97);

    volatile struct {
        unsigned char prefix[7];
        union Scalar value;
        unsigned char suffix;
    } v;
    v.prefix[3] = 19;
    v.value.integer = -876543210;
    v.suffix = 73;
    v.value = scalar(v.value);
    assert(v.value.integer == -876543210);
    assert(v.prefix[3] == 19 && v.suffix == 73);
}

int main(void)
{
    int x = 42, y = 7;
    nested_values();
    union Numeric n = { .bits = 0xfedcba9876543210UL };
    n = numeric(n);
    assert(n.bits == 0xfedcba9876543210UL);
    union NumericPair np = { .bits = { 0x0123456789abcdefUL, 0xfedcba9876543210UL } };
    np = numeric_pair(np);
    assert(np.bits[0] == 0x0123456789abcdefUL && np.bits[1] == 0xfedcba9876543210UL);
    union Aligned over = { .pointer = &x };
    over = aligned(over);
    assert(*over.pointer == 42);
    union Scalar bare = { .pointer = &x };
    union Scalar (*volatile indirect_bare)(union Scalar) = scalar;
    bare = indirect_bare(bare);
    assert(*(int*)bare.pointer == 42);
    bare.function = add;
    bare = scalar(bare);
    assert(bare.function(29) == 42);
    struct First a = { .value.pointer = &x, .tag = 17 };
    struct Last b = { .tag = 19, .value.pointer = &y };
    a = first(a);
    b = last(b);
    assert(a.tag == 17 && *(int*)a.value.pointer == 42);
    assert(b.tag == 19 && *(int*)b.value.pointer == 7);
    *(int*)b.value.pointer = 11;
    assert(y == 11);
    aligned_volatile_values(&x, &y);

    struct Array nested = { .values = { { .pointer = &y } }, .tag = 47 };
    nested = array(nested);
    assert(nested.tag == 47 && *(int*)nested.values[0].pointer == 11);

    struct First (*volatile indirect)(struct First) = first;
    a = indirect(a);
    assert(a.tag == 17 && *(int*)a.value.pointer == 42);
    a.value.function = add;
    a = first(a);
    assert(a.value.function(7) == 20);
    b.value.function = add;
    b = last(b);
    assert(b.value.function(29) == 42);

    a.value.integer = -123456789;
    a = first(a);
    assert(a.value.integer == -123456789 && a.tag == 17);
    b.value.number = -2.75;
    b = last(b);
    assert(b.value.number == -2.75 && b.tag == 19);
    // Returning via a pointer-shaped word must preserve bits, not convert the
    // floating-point value numerically (including signed zero and NaN payload).
    unsigned long bits[] = { 0x8000000000000000UL, 0x7ff8000000000042UL };
    for (unsigned i = 0; i < 2; ++i) {
        memcpy(&a.value.number, &bits[i], sizeof(double));
        a = first(a);
        unsigned long got;
        memcpy(&got, &a.value.number, sizeof(double));
        assert(got == bits[i]);
        memcpy(&bare.number, &bits[i], sizeof(double));
        bare = scalar(bare);
        memcpy(&got, &bare.number, sizeof(double));
        assert(got == bits[i]);
        union MixedIS is = { .value.pointer = &x };
        union MixedSI si = { .value.pointer = &y };
        memcpy(&is.value.number, &bits[i], sizeof(double));
        memcpy(&si.value.number, &bits[i], sizeof(double));
        is = mixed_is(is);
        si = mixed_si(si);
        assert(*is.value.pointer == 42 && *si.value.pointer == 11);
        memcpy(&got, &is.value.number, sizeof(double));
        assert(got == bits[i]);
        memcpy(&got, &si.value.number, sizeof(double));
        assert(got == bits[i]);
    }

    // ABI carriers must preserve capability-bearing array alternatives and
    // the full numeric payload, not just the first pointer-sized word.
    struct Hidden h = { .value.pointers = { &x }, .tag = 23 };
    h = hidden(h);
    assert(h.tag == 23 && *h.value.pointers[0] == 42);
    struct Wide w = { .value.pointers = { &x, &y } };
    w = wide(w);
    assert(*w.value.pointers[0] == 42 && *w.value.pointers[1] == 11);
    union HiddenLast last_word = { .value = { 47, &y } };
    union HiddenLast (*volatile indirect_last)(union HiddenLast) = hidden_last;
    last_word = indirect_last(last_word);
    assert(last_word.value.tag == 47 && *last_word.value.pointer == 11);
    struct Large l = { .value.pointer = &y };
    l = large(l);
    assert(*(int*)l.value.pointer == 11);
    l.value.integer = ((__int128)12345 << 80) + 6789;
    l = large(l);
    assert(l.value.integer == ((__int128)12345 << 80) + 6789);

    a.value.pointer = &x;
    b.tag = 29;
    struct First c = { .value.integer = -123456789, .tag = 31 };
    struct Last d = { .tag = 37, .value.pointer = &x };
    variadic(&x, a, 12345L, b, c, 1.25, d);
    assert(x == 73);
    puts("union scalar ABI ok");
    return 0;
}
