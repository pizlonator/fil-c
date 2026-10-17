#ifndef UNIONSTORAGECOPY_H
#define UNIONSTORAGECOPY_H

#include <stdint.h>

typedef union Choice {
    int *pointer;
    uintptr_t bits;
    double number;
    unsigned char bytes[sizeof(void *)];
} Choice;

/* No declared pointer member, but bytewise copies may still carry a capability. */
typedef union Bytes {
    uintptr_t bits;
    unsigned char bytes[sizeof(void *)];
} Bytes;

void observe(uintptr_t value);
void observe_number(double value);
void copy_observed(Choice *out, const Choice *in);
void copy_number_observed(Choice *out, const Choice *in);
void copy_written(Choice *out, const Choice *in, int byte_write);
void copy_bytes(Bytes *out, const Bytes *in);
void copy_chain(Choice *out, const Choice *in);
Choice round_trip(Choice value);
int check_varargs(int tag, ...);

#endif
