/* Packed global and thread-local initializers whose pointers sit at word
 * offsets still initialize normally, as does a null pointer at byte 1 of an
 * ordinary global (it needs no relocation). */
#include <stdfil.h>

static int anchor = 42;

struct __attribute__((packed)) Aligned { int *pointer; char tail[3]; };
union __attribute__((packed)) Odd { int *pointer; char bytes[9]; };
struct __attribute__((packed)) NullAtOne { char prefix; int *pointer; };

struct Aligned aligned_global = { &anchor, { 1, 2, 3 } };
_Thread_local struct Aligned aligned_tls = { &anchor, { 4, 5, 6 } };
union Odd odd_global = { .pointer = &anchor };
_Thread_local union Odd odd_tls = { .pointer = &anchor };
struct NullAtOne null_at_one = { 7, 0 };

int main(void)
{
    ZASSERT(*aligned_global.pointer == 42 && aligned_global.tail[2] == 3);
    ZASSERT(*aligned_tls.pointer == 42 && aligned_tls.tail[2] == 6);
    ZASSERT(*odd_global.pointer == 42 && *odd_tls.pointer == 42);
    ZASSERT(null_at_one.prefix == 7);
    return 0;
}
