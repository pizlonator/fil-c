#include "value.h"
#include <assert.h>
#include <stdarg.h>

union Scalar scalar(union Scalar value) { return value; }
union WideValue wide_value(union WideValue value) { return value; }
union Numeric numeric(union Numeric value) { return value; }
union NumericPair numeric_pair(union NumericPair value) { return value; }
union Aligned aligned(union Aligned value) { return value; }
union MixedIS mixed_is(union MixedIS value) { return value; }
union MixedSI mixed_si(union MixedSI value) { return value; }
union HiddenLast hidden_last(union HiddenLast value) { return value; }
struct First first(struct First value) { return value; }
struct Last last(struct Last value) { return value; }
struct Array array(struct Array value) { return value; }
struct Hidden hidden(struct Hidden value) { return value; }
struct Wide wide(struct Wide value) { return value; }
struct Large large(struct Large value) { return value; }

void variadic(int* expected, ...)
{
    va_list args;
    va_start(args, expected);
    struct First a = va_arg(args, struct First);
    assert(a.tag == 17 && a.value.pointer == expected);
    assert(*(int*)a.value.pointer == 42);
    assert(va_arg(args, long) == 12345);
    struct Last b = va_arg(args, struct Last);
    assert(b.tag == 29 && b.value.number == -2.75);
    struct First c = va_arg(args, struct First);
    assert(c.tag == 31 && c.value.integer == -123456789);
    assert(va_arg(args, double) == 1.25);
    struct Last d = va_arg(args, struct Last);
    assert(d.tag == 37 && d.value.pointer == expected);
    *(int*)d.value.pointer = 73;
    va_end(args);
}
