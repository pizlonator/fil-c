#include <stdarg.h>
#include <stdlib.h>
#include <stdfil.h>

/* Aggregate varargs must copy the complete object, including capabilities
   hidden by a union's LLVM storage type. Packet alignment is relative to the
   snapshot base, not the absolute address or the C type's declared alignment. */
typedef union { long bits; int *pointers[1]; } Hidden;
typedef union {
    unsigned long bits[2];
    struct { int *first; int *second; } pointers;
} HiddenPair;
typedef union __attribute__((aligned(16))) { int *pointer; long bits; } Align16;
typedef union __attribute__((aligned(32))) { long bits; } Align32;
typedef union __attribute__((aligned(64))) { long bits; } Align64;
typedef union __attribute__((transparent_union, aligned(16))) {
    long bits;
    int *pointer;
} Transparent;
typedef struct { unsigned char bytes[3]; } Bytes3;
typedef struct { unsigned char bytes[9]; } Bytes9;
typedef struct { unsigned char bytes[12]; } Bytes12;
typedef double Vector32 __attribute__((vector_size(32)));
typedef double Vector64 __attribute__((vector_size(64)));
typedef struct { Vector32 vector; int *pointer; } Wide32;
typedef struct { Vector64 vector; int *pointer; } Wide64;

static Hidden make_hidden(int value)
{
    Hidden result;
    result.pointers[0] = malloc(sizeof(int));
    *result.pointers[0] = value;
    return result;
}

__attribute__((noinline)) static void read_args(va_list ap)
{
    zgc_request_and_wait();
    ZASSERT(va_arg(ap, int) == 29);
    Bytes9 b9 = va_arg(ap, Bytes9);
    ZASSERT(b9.bytes[0] == 4 && b9.bytes[8] == 12);
    Bytes12 b12 = va_arg(ap, Bytes12);
    ZASSERT(b12.bytes[0] == 13 && b12.bytes[11] == 24);
    Align16 a16 = va_arg(ap, Align16);
    ZASSERT(*a16.pointer == 73);
    ZASSERT(va_arg(ap, int) == 37);
    Align32 a32 = va_arg(ap, Align32);
    ZASSERT(a32.bits == 41);
    ZASSERT(va_arg(ap, int) == 43);
    Align64 a64 = va_arg(ap, Align64);
    ZASSERT(a64.bits == 47);
    ZASSERT(va_arg(ap, int) == 53);
    Wide32 w32 = va_arg(ap, Wide32);
    ZASSERT(w32.vector[0] == 2.5 && w32.vector[3] == 7.5);
    ZASSERT(*w32.pointer == 73);
    ZASSERT(va_arg(ap, int) == 59);
    Wide64 w64 = va_arg(ap, Wide64);
    ZASSERT(w64.vector[0] == 3.5 && w64.vector[7] == 11.5);
    ZASSERT(*w64.pointer == 97);
    ZASSERT(va_arg(ap, int) == 67);
    Transparent numeric = va_arg(ap, Transparent);
    ZASSERT(numeric.bits == 71);
    ZASSERT(va_arg(ap, int) == 79);
    Transparent pointer = va_arg(ap, Transparent);
    zgc_request_and_wait();
    ZASSERT(*pointer.pointer == 97);
    ZASSERT(va_arg(ap, int) == 83);
}

__attribute__((noinline)) static void check(int count, ...)
{
    va_list ap, copy;
    va_start(ap, count);
    zgc_request_and_wait();
    ZASSERT(va_arg(ap, int) == 19);
    Hidden hidden = va_arg(ap, Hidden);
    zgc_request_and_wait();
    ZASSERT(*hidden.pointers[0] == 61);
    ZASSERT(va_arg(ap, long) == 123456789);
    HiddenPair pair = va_arg(ap, HiddenPair);
    ZASSERT(*pair.pointers.first == 73 && *pair.pointers.second == 97);
    Bytes3 b3 = va_arg(ap, Bytes3);
    ZASSERT(b3.bytes[0] == 1 && b3.bytes[2] == 3);
    // Copy after partial consumption, then forward both cursors. Their lower
    // bounds must still refer to the start of the original packet.
    va_copy(copy, ap);
    read_args(copy);
    read_args(ap);
    va_end(copy);
    va_end(ap);
}

int main(void)
{
    int first = 73, second = 97;
    for (unsigned i = 0; i < 8; ++i)
        check(0, 19, make_hidden(61), 123456789L,
              (HiddenPair){ .pointers = { &first, &second } },
              (Bytes3){ { 1, 2, 3 } }, 29,
              (Bytes9){ { 4, 5, 6, 7, 8, 9, 10, 11, 12 } },
              (Bytes12){ { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 } },
              (Align16){ .pointer = &first }, 37,
              (Align32){ .bits = 41 }, 43, (Align64){ .bits = 47 }, 53,
              (Wide32){ { 2.5, 4.5, 6.5, 7.5 }, &first }, 59,
              (Wide64){ { 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 11.5 }, &second }, 67,
              (Transparent){ .bits = 71 }, 79,
              (Transparent){ .pointer = &second }, 83);
    return 0;
}
