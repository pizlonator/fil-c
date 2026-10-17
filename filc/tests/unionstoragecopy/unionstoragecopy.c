#include "unionstoragecopy.h"
#include <stdarg.h>

/* An integer/FP observation must not make SROA integerize the copies around it.
 * The separately compiled caller verifies the shadow capability by dereference. */
void copy_observed(Choice *out, const Choice *in)
{
    Choice temporary = *in;
    observe(temporary.bits);
    *out = temporary;
}

void copy_number_observed(Choice *out, const Choice *in)
{
    Choice temporary = *in;
    observe_number(temporary.number);
    *out = temporary;
}

void copy_written(Choice *out, const Choice *in, int byte_write)
{
    Choice temporary = *in;
    if (byte_write)
        temporary.bytes[0] = in->bytes[0];
    else
        temporary.bits = in->bits;
    *out = temporary;
}

void copy_bytes(Bytes *out, const Bytes *in)
{
    Bytes temporary = *in;
    observe(temporary.bits);
    *out = temporary;
}

void copy_chain(Choice *out, const Choice *in)
{
    Choice first = *in;
    Choice second = first;
    observe(second.bits);
    *out = second;
}

Choice round_trip(Choice value) { return value; }

int check_varargs(int tag, ...)
{
    va_list args;
    va_start(args, tag);
    Choice value = va_arg(args, Choice);
    long first = va_arg(args, long);
    double second = va_arg(args, double);
    va_end(args);
    return tag == 17 && *value.pointer == 97 && first == 0x123456789L &&
        second == -19.75;
}
