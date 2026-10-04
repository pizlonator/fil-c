#include "unionstoragecopy.h"
#include <assert.h>
#include <string.h>

static volatile uintptr_t observed_bits;
static volatile double observed_number;
void observe(uintptr_t value) { observed_bits = value; }
void observe_number(double value) { observed_number = value; }

static int global_value = 42;
static Choice relocated = { .pointer = &global_value };
static _Thread_local Choice tls_relocated = { .pointer = &global_value };

int main(void)
{
    int target = 97;
    Choice in = { .pointer = &target }, out;
    copy_observed(&out, &in);
    assert(*out.pointer == 97);
    assert(observed_bits == (uintptr_t)&target);
    copy_number_observed(&out, &in);
    assert(*out.pointer == 97);
    copy_chain(&out, &in);
    assert(*out.pointer == 97);

    /* Numeric stores preserve existing shadow state in Fil-C. These same-bit
     * writes are defined C union operations and keep the pointer payload intact. */
    for (int byte_write = 0; byte_write != 2; ++byte_write) {
        copy_written(&out, &in, byte_write);
        assert(*out.pointer == 97);
    }

    /* Absence of pointer members is not proof that an aggregate copy may drop
     * capabilities. The explicit memcpy calls use Fil-C's existing byte-copy
     * operation; the aggregate copies in copy_bytes are the SROA regression. */
    Bytes bytes_in, bytes_out;
    memcpy(&bytes_in, &in.pointer, sizeof(in.pointer));
    copy_bytes(&bytes_out, &bytes_in);
    int *recovered;
    memcpy(&recovered, &bytes_out, sizeof(recovered));
    assert(*recovered == 97);

    out = round_trip(in);
    assert(*out.pointer == 97);
    Choice (*volatile indirect)(Choice) = round_trip;
    out = indirect(in);
    assert(*out.pointer == 97);
    assert(check_varargs(17, in, 0x123456789L, -19.75));

    volatile Choice volatile_in = in;
    volatile Choice volatile_out = volatile_in;
    assert(*volatile_out.pointer == 97);
    assert(*relocated.pointer == 42 && *tls_relocated.pointer == 42);
    return 0;
}
