/* Auto-generated test for sntrup761 x86_64 inline assembly snippets. */

#include <stdint.h>
#include <stdfil.h>
#include <limits.h>

typedef int16_t crypto_int16;
typedef uint16_t crypto_int16_unsigned;
typedef int32_t crypto_int32;
typedef uint32_t crypto_int32_unsigned;
typedef int64_t crypto_int64;
typedef uint64_t crypto_int64_unsigned;

/* Portable arithmetic right-shift helpers. */
static int16_t asr16(int16_t x, int s)
{
    s &= 15;
    if (!s) return x;
    uint16_t ux = (uint16_t)x;
    uint16_t r = ux >> s;
    if (x < 0) r |= (uint16_t)~0 << (16 - s);
    return (int16_t)r;
}

static int32_t asr32(int32_t x, int s)
{
    s &= 31;
    if (!s) return x;
    uint32_t ux = (uint32_t)x;
    uint32_t r = ux >> s;
    if (x < 0) r |= (uint32_t)~0 << (32 - s);
    return (int32_t)r;
}

static int64_t asr64(int64_t x, int s)
{
    s &= 63;
    if (!s) return x;
    uint64_t ux = (uint64_t)x;
    uint64_t r = ux >> s;
    if (x < 0) r |= (uint64_t)~0 << (64 - s);
    return (int64_t)r;
}

/* Portable trailing-zero helpers matching bsf/cmove fallback semantics. */
static int tz16(int16_t x)
{
    uint16_t ux = (uint16_t)x;
    if (!ux) return 16;
    return __builtin_ctz((unsigned)ux);
}

static int tz32(int32_t x)
{
    uint32_t ux = (uint32_t)x;
    if (!ux) return 32;
    return __builtin_ctz((unsigned)ux);
}

static int tz64(int64_t x)
{
    uint64_t ux = (uint64_t)x;
    if (!ux) return 64;
    return __builtin_ctzll(ux);
}
/* ---------- 16-bit operations ---------- */

static int16_t asm_int16_negative_mask(int16_t x)
{
    __asm__ ("sarw $15,%0" : "+r"(x) : : "cc");
    return x;
}

static int16_t c_int16_negative_mask(int16_t x)
{
    return asr16(x, 15);
}

static int16_t asm_int16_unsigned_topbit_01(uint16_t x)
{
    __asm__ ("shrw $15,%0" : "+r"(x) : : "cc");
    return x;
}

static int16_t c_int16_unsigned_topbit_01(uint16_t x)
{
    return (uint16_t)(x >> 15);
}

static int16_t asm_int16_bottombit_mask(int16_t x)
{
    __asm__ ("andw $1,%0" : "+r"(x) : : "cc");
    return -x;
}

static int16_t c_int16_bottombit_mask(int16_t x)
{
    return -(x & 1);
}

static int16_t asm_int16_bottombit_01(int16_t x)
{
    __asm__ ("andw $1,%0" : "+r"(x) : : "cc");
    return x;
}

static int16_t c_int16_bottombit_01(int16_t x)
{
    return x & 1;
}

static int16_t asm_int16_bitinrangepublicpos_mask(int16_t x, int16_t s)
{
    __asm__ ("sarw %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return -(x & 1);
}

static int16_t c_int16_bitinrangepublicpos_mask(int16_t x, int16_t s)
{
    return -((asr16(x, s)) & 1);
}

static int16_t asm_int16_bitinrangepublicpos_01(int16_t x, int16_t s)
{
    __asm__ ("sarw %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x & 1;
}

static int16_t c_int16_bitinrangepublicpos_01(int16_t x, int16_t s)
{
    return asr16(x, s) & 1;
}

static int16_t asm_int16_shlmod(int16_t x, int16_t s)
{
    s &= 15;
    __asm__ ("shlw %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int16_t c_int16_shlmod(int16_t x, int16_t s)
{
    return (int16_t)((uint16_t)x << (s & 15));
}

static int16_t asm_int16_shrmod(int16_t x, int16_t s)
{
    s &= 15;
    __asm__ ("sarw %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int16_t c_int16_shrmod(int16_t x, int16_t s)
{
    return asr16(x, s);
}

static int16_t asm_int16_nonzero_mask(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovnew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_nonzero_mask(int16_t x)
{
    return -(x != 0);
}

static int16_t asm_int16_nonzero_01(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovnew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_nonzero_01(int16_t x)
{
    return x != 0;
}

static int16_t asm_int16_positive_mask(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovgw %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_positive_mask(int16_t x)
{
    return -(x > 0);
}

static int16_t asm_int16_positive_01(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovgw %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_positive_01(int16_t x)
{
    return x > 0;
}

static int16_t asm_int16_zero_mask(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_zero_mask(int16_t x)
{
    return -(x == 0);
}

static int16_t asm_int16_zero_01(int16_t x)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int16_t c_int16_zero_01(int16_t x)
{
    return x == 0;
}

static int16_t asm_int16_unequal_mask(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovnew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_unequal_mask(int16_t x, int16_t y)
{
    return -(x != y);
}

static int16_t asm_int16_unequal_01(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovnew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_unequal_01(int16_t x, int16_t y)
{
    return x != y;
}

static int16_t asm_int16_equal_mask(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_equal_mask(int16_t x, int16_t y)
{
    return -(x == y);
}

static int16_t asm_int16_equal_01(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_equal_01(int16_t x, int16_t y)
{
    return x == y;
}

static int16_t asm_int16_min(int16_t x, int16_t y)
{
    __asm__ ("cmpw %1,%0\n cmovgw %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int16_t c_int16_min(int16_t x, int16_t y)
{
    return x < y ? x : y;
}

static int16_t asm_int16_max(int16_t x, int16_t y)
{
    __asm__ ("cmpw %1,%0\n cmovlw %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int16_t c_int16_max(int16_t x, int16_t y)
{
    return x > y ? x : y;
}

static void asm_int16_minmax(int16_t *p, int16_t *q)
{
    int16_t x = *p;
    int16_t y = *q;
    int16_t z;
    __asm__ ("cmpw %2,%1\n movw %1,%0\n cmovgw %2,%1\n cmovgw %0,%2" : "=&r"(z), "+&r"(x), "+r"(y) : : "cc");
    *p = x;
    *q = y;
}

static void c_int16_minmax(int16_t *p, int16_t *q)
{
    int16_t x = *p;
    int16_t y = *q;
    if (x > y) { *p = y; *q = x; }
}

static int16_t asm_int16_smaller_mask(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovlw %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_smaller_mask(int16_t x, int16_t y)
{
    return -(x < y);
}

static int16_t asm_int16_smaller_01(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovlw %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_smaller_01(int16_t x, int16_t y)
{
    return x < y;
}

static int16_t asm_int16_leq_mask(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovlew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_leq_mask(int16_t x, int16_t y)
{
    return -(x <= y);
}

static int16_t asm_int16_leq_01(int16_t x, int16_t y)
{
    int16_t q, z;
    __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovlew %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int16_t c_int16_leq_01(int16_t x, int16_t y)
{
    return x <= y;
}

static int16_t asm_int16_bottomzeros_num(int16_t x)
{
    int16_t fallback = 16;
    __asm__ ("bsfw %0,%0\n cmovew %1,%0" : "+&r"(x) : "r"(fallback) : "cc");
    return x;
}

static int16_t c_int16_bottomzeros_num(int16_t x)
{
    return tz16(x);
}
/* ---------- 32-bit operations ---------- */

static int32_t asm_int32_negative_mask(int32_t x)
{
    __asm__ ("sarl $31,%0" : "+r"(x) : : "cc");
    return x;
}

static int32_t c_int32_negative_mask(int32_t x)
{
    return asr32(x, 31);
}

static int32_t asm_int32_unsigned_topbit_01(uint32_t x)
{
    __asm__ ("shrl $31,%0" : "+r"(x) : : "cc");
    return x;
}

static int32_t c_int32_unsigned_topbit_01(uint32_t x)
{
    return (uint32_t)(x >> 31);
}

static int32_t asm_int32_bottombit_mask(int32_t x)
{
    __asm__ ("andl $1,%0" : "+r"(x) : : "cc");
    return -x;
}

static int32_t c_int32_bottombit_mask(int32_t x)
{
    return -(x & 1);
}

static int32_t asm_int32_bottombit_01(int32_t x)
{
    __asm__ ("andl $1,%0" : "+r"(x) : : "cc");
    return x;
}

static int32_t c_int32_bottombit_01(int32_t x)
{
    return x & 1;
}

static int32_t asm_int32_bitinrangepublicpos_mask(int32_t x, int32_t s)
{
    __asm__ ("sarl %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return -(x & 1);
}

static int32_t c_int32_bitinrangepublicpos_mask(int32_t x, int32_t s)
{
    return -((asr32(x, s)) & 1);
}

static int32_t asm_int32_bitinrangepublicpos_01(int32_t x, int32_t s)
{
    __asm__ ("sarl %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x & 1;
}

static int32_t c_int32_bitinrangepublicpos_01(int32_t x, int32_t s)
{
    return asr32(x, s) & 1;
}

static int32_t asm_int32_shlmod(int32_t x, int32_t s)
{
    __asm__ ("shll %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int32_t c_int32_shlmod(int32_t x, int32_t s)
{
    return (int32_t)((uint32_t)x << (s & 31));
}

static int32_t asm_int32_shrmod(int32_t x, int32_t s)
{
    __asm__ ("sarl %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int32_t c_int32_shrmod(int32_t x, int32_t s)
{
    return asr32(x, s);
}

static int32_t asm_int32_nonzero_mask(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n testl %2,%2\n cmovnel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_nonzero_mask(int32_t x)
{
    return -(x != 0);
}

static int32_t asm_int32_nonzero_01(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n testl %2,%2\n cmovnel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_nonzero_01(int32_t x)
{
    return x != 0;
}

static int32_t asm_int32_positive_mask(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n testl %2,%2\n cmovgl %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_positive_mask(int32_t x)
{
    return -(x > 0);
}

static int32_t asm_int32_positive_01(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n testl %2,%2\n cmovgl %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_positive_01(int32_t x)
{
    return x > 0;
}

static int32_t asm_int32_zero_mask(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n testl %2,%2\n cmovel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_zero_mask(int32_t x)
{
    return -(x == 0);
}

static int32_t asm_int32_zero_01(int32_t x)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n testl %2,%2\n cmovel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int32_t c_int32_zero_01(int32_t x)
{
    return x == 0;
}

static int32_t asm_int32_unequal_mask(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovnel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_unequal_mask(int32_t x, int32_t y)
{
    return -(x != y);
}

static int32_t asm_int32_unequal_01(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovnel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_unequal_01(int32_t x, int32_t y)
{
    return x != y;
}

static int32_t asm_int32_equal_mask(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_equal_mask(int32_t x, int32_t y)
{
    return -(x == y);
}

static int32_t asm_int32_equal_01(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_equal_01(int32_t x, int32_t y)
{
    return x == y;
}

static int32_t asm_int32_min(int32_t x, int32_t y)
{
    __asm__ ("cmpl %1,%0\n cmovgl %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int32_t c_int32_min(int32_t x, int32_t y)
{
    return x < y ? x : y;
}

static int32_t asm_int32_max(int32_t x, int32_t y)
{
    __asm__ ("cmpl %1,%0\n cmovll %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int32_t c_int32_max(int32_t x, int32_t y)
{
    return x > y ? x : y;
}

static void asm_int32_minmax(int32_t *p, int32_t *q)
{
    int32_t x = *p;
    int32_t y = *q;
    int32_t z;
    __asm__ ("cmpl %2,%1\n movl %1,%0\n cmovgl %2,%1\n cmovgl %0,%2" : "=&r"(z), "+&r"(x), "+r"(y) : : "cc");
    *p = x;
    *q = y;
}

static void c_int32_minmax(int32_t *p, int32_t *q)
{
    int32_t x = *p;
    int32_t y = *q;
    if (x > y) { *p = y; *q = x; }
}

static int32_t asm_int32_smaller_mask(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovll %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_smaller_mask(int32_t x, int32_t y)
{
    return -(x < y);
}

static int32_t asm_int32_smaller_01(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovll %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_smaller_01(int32_t x, int32_t y)
{
    return x < y;
}

static int32_t asm_int32_leq_mask(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovlel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_leq_mask(int32_t x, int32_t y)
{
    return -(x <= y);
}

static int32_t asm_int32_leq_01(int32_t x, int32_t y)
{
    int32_t q, z;
    __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovlel %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int32_t c_int32_leq_01(int32_t x, int32_t y)
{
    return x <= y;
}

static int32_t asm_int32_bottomzeros_num(int32_t x)
{
    int32_t fallback = 32;
    __asm__ ("bsfl %0,%0\n cmovel %1,%0" : "+&r"(x) : "r"(fallback) : "cc");
    return x;
}

static int32_t c_int32_bottomzeros_num(int32_t x)
{
    return tz32(x);
}
/* ---------- 64-bit operations ---------- */

static int64_t asm_int64_negative_mask(int64_t x)
{
    __asm__ ("sarq $63,%0" : "+r"(x) : : "cc");
    return x;
}

static int64_t c_int64_negative_mask(int64_t x)
{
    return asr64(x, 63);
}

static int64_t asm_int64_unsigned_topbit_01(uint64_t x)
{
    __asm__ ("shrq $63,%0" : "+r"(x) : : "cc");
    return x;
}

static int64_t c_int64_unsigned_topbit_01(uint64_t x)
{
    return (uint64_t)(x >> 63);
}

static int64_t asm_int64_bottombit_mask(int64_t x)
{
    __asm__ ("andq $1,%0" : "+r"(x) : : "cc");
    return -x;
}

static int64_t c_int64_bottombit_mask(int64_t x)
{
    return -(x & 1);
}

static int64_t asm_int64_bottombit_01(int64_t x)
{
    __asm__ ("andq $1,%0" : "+r"(x) : : "cc");
    return x;
}

static int64_t c_int64_bottombit_01(int64_t x)
{
    return x & 1;
}

static int64_t asm_int64_bitinrangepublicpos_mask(int64_t x, int64_t s)
{
    __asm__ ("sarq %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return -(x & 1);
}

static int64_t c_int64_bitinrangepublicpos_mask(int64_t x, int64_t s)
{
    return -((asr64(x, s)) & 1);
}

static int64_t asm_int64_bitinrangepublicpos_01(int64_t x, int64_t s)
{
    __asm__ ("sarq %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x & 1;
}

static int64_t c_int64_bitinrangepublicpos_01(int64_t x, int64_t s)
{
    return asr64(x, s) & 1;
}

static int64_t asm_int64_shlmod(int64_t x, int64_t s)
{
    __asm__ ("shlq %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int64_t c_int64_shlmod(int64_t x, int64_t s)
{
    return (int64_t)((uint64_t)x << (s & 63));
}

static int64_t asm_int64_shrmod(int64_t x, int64_t s)
{
    __asm__ ("sarq %%cl,%0" : "+r"(x) : "c"(s) : "cc");
    return x;
}

static int64_t c_int64_shrmod(int64_t x, int64_t s)
{
    return asr64(x, s);
}

static int64_t asm_int64_nonzero_mask(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n testq %2,%2\n cmovneq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_nonzero_mask(int64_t x)
{
    return -(x != 0);
}

static int64_t asm_int64_nonzero_01(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n testq %2,%2\n cmovneq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_nonzero_01(int64_t x)
{
    return x != 0;
}

static int64_t asm_int64_positive_mask(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n testq %2,%2\n cmovgq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_positive_mask(int64_t x)
{
    return -(x > 0);
}

static int64_t asm_int64_positive_01(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n testq %2,%2\n cmovgq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_positive_01(int64_t x)
{
    return x > 0;
}

static int64_t asm_int64_zero_mask(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n testq %2,%2\n cmoveq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_zero_mask(int64_t x)
{
    return -(x == 0);
}

static int64_t asm_int64_zero_01(int64_t x)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n testq %2,%2\n cmoveq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x) : "cc");
    return z;
}

static int64_t c_int64_zero_01(int64_t x)
{
    return x == 0;
}

static int64_t asm_int64_unequal_mask(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n cmpq %3,%2\n cmovneq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_unequal_mask(int64_t x, int64_t y)
{
    return -(x != y);
}

static int64_t asm_int64_unequal_01(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n cmpq %3,%2\n cmovneq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_unequal_01(int64_t x, int64_t y)
{
    return x != y;
}

static int64_t asm_int64_equal_mask(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n cmpq %3,%2\n cmoveq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_equal_mask(int64_t x, int64_t y)
{
    return -(x == y);
}

static int64_t asm_int64_equal_01(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n cmpq %3,%2\n cmoveq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_equal_01(int64_t x, int64_t y)
{
    return x == y;
}

static int64_t asm_int64_min(int64_t x, int64_t y)
{
    __asm__ ("cmpq %1,%0\n cmovgq %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int64_t c_int64_min(int64_t x, int64_t y)
{
    return x < y ? x : y;
}

static int64_t asm_int64_max(int64_t x, int64_t y)
{
    __asm__ ("cmpq %1,%0\n cmovlq %1,%0" : "+r"(x) : "r"(y) : "cc");
    return x;
}

static int64_t c_int64_max(int64_t x, int64_t y)
{
    return x > y ? x : y;
}

static void asm_int64_minmax(int64_t *p, int64_t *q)
{
    int64_t x = *p;
    int64_t y = *q;
    int64_t z;
    __asm__ ("cmpq %2,%1\n movq %1,%0\n cmovgq %2,%1\n cmovgq %0,%2" : "=&r"(z), "+&r"(x), "+r"(y) : : "cc");
    *p = x;
    *q = y;
}

static void c_int64_minmax(int64_t *p, int64_t *q)
{
    int64_t x = *p;
    int64_t y = *q;
    if (x > y) { *p = y; *q = x; }
}

static int64_t asm_int64_smaller_mask(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n cmpq %3,%2\n cmovlq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_smaller_mask(int64_t x, int64_t y)
{
    return -(x < y);
}

static int64_t asm_int64_smaller_01(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n cmpq %3,%2\n cmovlq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_smaller_01(int64_t x, int64_t y)
{
    return x < y;
}

static int64_t asm_int64_leq_mask(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $-1,%1\n cmpq %3,%2\n cmovleq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_leq_mask(int64_t x, int64_t y)
{
    return -(x <= y);
}

static int64_t asm_int64_leq_01(int64_t x, int64_t y)
{
    int64_t q, z;
    __asm__ ("xorq %0,%0\n movq $1,%1\n cmpq %3,%2\n cmovleq %1,%0" : "=&r"(z), "=&r"(q) : "r"(x), "r"(y) : "cc");
    return z;
}

static int64_t c_int64_leq_01(int64_t x, int64_t y)
{
    return x <= y;
}

static int64_t asm_int64_bottomzeros_num(int64_t x)
{
    int64_t fallback = 64;
    __asm__ ("bsfq %0,%0\n cmoveq %1,%0" : "+&r"(x) : "r"(fallback) : "cc");
    return x;
}

static int64_t c_int64_bottomzeros_num(int64_t x)
{
    return tz64(x);
}
int main(void)
{
    /* 16-bit test inputs */
    int16_t vals16[] = {0, 1, -1, 2, -2, 3, -3, 3, -3, INT16_MIN, INT16_MAX, INT16_MIN + 1, INT16_MAX - 1, 0x1234, 0xABCD, 0x55AA, 0x8001, 0x7FFF, 0x0001};
    unsigned n16 = sizeof(vals16) / sizeof(vals16[0]);

    /* 32-bit test inputs */
    int32_t vals32[] = {0, 1, -1, 2, -2, 3, -3, 7, -7, INT32_MIN, INT32_MAX, INT32_MIN + 1, INT32_MAX - 1, 0x12345678, 0xABCDEF01, 0x55555555, 0x80000001, 0x7FFFFFFF, 0x00000001};
    unsigned n32 = sizeof(vals32) / sizeof(vals32[0]);

    /* 64-bit test inputs */
    int64_t vals64[] = {0, 1, -1, 2, -2, 3, -3, 15LL, -15LL, INT64_MIN, INT64_MAX, INT64_MIN + 1, INT64_MAX - 1, 0x123456789ABCDEF0LL, 0xFEDCBA9876543210LL, 0x5555555555555555LL, 0x8000000000000001LL, 0x7FFFFFFFFFFFFFFFLL, 0x0000000000000001LL};
    unsigned n64 = sizeof(vals64) / sizeof(vals64[0]);

    for (unsigned i = 0; i < n16; ++i) {
        int16_t x = vals16[i], y = vals16[(i * 7 + 3) % n16];
        int16_t s = (int16_t)((i * 13) % 16);
        if (asm_int16_negative_mask(x) != c_int16_negative_mask(x)) {
            zprintf("mismatch int16_negative_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_negative_mask(x), (int)c_int16_negative_mask(x));
            return 1;
        }
        if (asm_int16_unsigned_topbit_01(x) != c_int16_unsigned_topbit_01(x)) {
            zprintf("mismatch int16_unsigned_topbit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_unsigned_topbit_01(x), (int)c_int16_unsigned_topbit_01(x));
            return 1;
        }
        if (asm_int16_bottombit_mask(x) != c_int16_bottombit_mask(x)) {
            zprintf("mismatch int16_bottombit_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_bottombit_mask(x), (int)c_int16_bottombit_mask(x));
            return 1;
        }
        if (asm_int16_bottombit_01(x) != c_int16_bottombit_01(x)) {
            zprintf("mismatch int16_bottombit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_bottombit_01(x), (int)c_int16_bottombit_01(x));
            return 1;
        }
        if (asm_int16_bitinrangepublicpos_mask(x, s) != c_int16_bitinrangepublicpos_mask(x, s)) {
            zprintf("mismatch int16_bitinrangepublicpos_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_bitinrangepublicpos_mask(x, s), (int)c_int16_bitinrangepublicpos_mask(x, s));
            return 1;
        }
        if (asm_int16_bitinrangepublicpos_01(x, s) != c_int16_bitinrangepublicpos_01(x, s)) {
            zprintf("mismatch int16_bitinrangepublicpos_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_bitinrangepublicpos_01(x, s), (int)c_int16_bitinrangepublicpos_01(x, s));
            return 1;
        }
        if (asm_int16_shlmod(x, s) != c_int16_shlmod(x, s)) {
            zprintf("mismatch int16_shlmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_shlmod(x, s), (int)c_int16_shlmod(x, s));
            return 1;
        }
        if (asm_int16_shrmod(x, s) != c_int16_shrmod(x, s)) {
            zprintf("mismatch int16_shrmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_shrmod(x, s), (int)c_int16_shrmod(x, s));
            return 1;
        }
        if (asm_int16_nonzero_mask(x) != c_int16_nonzero_mask(x)) {
            zprintf("mismatch int16_nonzero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_nonzero_mask(x), (int)c_int16_nonzero_mask(x));
            return 1;
        }
        if (asm_int16_nonzero_01(x) != c_int16_nonzero_01(x)) {
            zprintf("mismatch int16_nonzero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_nonzero_01(x), (int)c_int16_nonzero_01(x));
            return 1;
        }
        if (asm_int16_positive_mask(x) != c_int16_positive_mask(x)) {
            zprintf("mismatch int16_positive_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_positive_mask(x), (int)c_int16_positive_mask(x));
            return 1;
        }
        if (asm_int16_positive_01(x) != c_int16_positive_01(x)) {
            zprintf("mismatch int16_positive_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_positive_01(x), (int)c_int16_positive_01(x));
            return 1;
        }
        if (asm_int16_zero_mask(x) != c_int16_zero_mask(x)) {
            zprintf("mismatch int16_zero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_zero_mask(x), (int)c_int16_zero_mask(x));
            return 1;
        }
        if (asm_int16_zero_01(x) != c_int16_zero_01(x)) {
            zprintf("mismatch int16_zero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_zero_01(x), (int)c_int16_zero_01(x));
            return 1;
        }
        if (asm_int16_unequal_mask(x, y) != c_int16_unequal_mask(x, y)) {
            zprintf("mismatch int16_unequal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_unequal_mask(x, y), (int)c_int16_unequal_mask(x, y));
            return 1;
        }
        if (asm_int16_unequal_01(x, y) != c_int16_unequal_01(x, y)) {
            zprintf("mismatch int16_unequal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_unequal_01(x, y), (int)c_int16_unequal_01(x, y));
            return 1;
        }
        if (asm_int16_equal_mask(x, y) != c_int16_equal_mask(x, y)) {
            zprintf("mismatch int16_equal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_equal_mask(x, y), (int)c_int16_equal_mask(x, y));
            return 1;
        }
        if (asm_int16_equal_01(x, y) != c_int16_equal_01(x, y)) {
            zprintf("mismatch int16_equal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_equal_01(x, y), (int)c_int16_equal_01(x, y));
            return 1;
        }
        if (asm_int16_min(x, y) != c_int16_min(x, y)) {
            zprintf("mismatch int16_min x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_min(x, y), (int)c_int16_min(x, y));
            return 1;
        }
        if (asm_int16_max(x, y) != c_int16_max(x, y)) {
            zprintf("mismatch int16_max x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_max(x, y), (int)c_int16_max(x, y));
            return 1;
        }
        if (asm_int16_smaller_mask(x, y) != c_int16_smaller_mask(x, y)) {
            zprintf("mismatch int16_smaller_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_smaller_mask(x, y), (int)c_int16_smaller_mask(x, y));
            return 1;
        }
        if (asm_int16_smaller_01(x, y) != c_int16_smaller_01(x, y)) {
            zprintf("mismatch int16_smaller_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_smaller_01(x, y), (int)c_int16_smaller_01(x, y));
            return 1;
        }
        if (asm_int16_leq_mask(x, y) != c_int16_leq_mask(x, y)) {
            zprintf("mismatch int16_leq_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_leq_mask(x, y), (int)c_int16_leq_mask(x, y));
            return 1;
        }
        if (asm_int16_leq_01(x, y) != c_int16_leq_01(x, y)) {
            zprintf("mismatch int16_leq_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_leq_01(x, y), (int)c_int16_leq_01(x, y));
            return 1;
        }
        if (asm_int16_bottomzeros_num(x) != c_int16_bottomzeros_num(x)) {
            zprintf("mismatch int16_bottomzeros_num x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int16_bottomzeros_num(x), (int)c_int16_bottomzeros_num(x));
            return 1;
        }
    }

    for (unsigned i = 0; i < n32; ++i) {
        int32_t x = vals32[i], y = vals32[(i * 7 + 3) % n32];
        int32_t s = (int32_t)((i * 13) % 32);
        if (asm_int32_negative_mask(x) != c_int32_negative_mask(x)) {
            zprintf("mismatch int32_negative_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_negative_mask(x), (int)c_int32_negative_mask(x));
            return 1;
        }
        if (asm_int32_unsigned_topbit_01(x) != c_int32_unsigned_topbit_01(x)) {
            zprintf("mismatch int32_unsigned_topbit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_unsigned_topbit_01(x), (int)c_int32_unsigned_topbit_01(x));
            return 1;
        }
        if (asm_int32_bottombit_mask(x) != c_int32_bottombit_mask(x)) {
            zprintf("mismatch int32_bottombit_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_bottombit_mask(x), (int)c_int32_bottombit_mask(x));
            return 1;
        }
        if (asm_int32_bottombit_01(x) != c_int32_bottombit_01(x)) {
            zprintf("mismatch int32_bottombit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_bottombit_01(x), (int)c_int32_bottombit_01(x));
            return 1;
        }
        if (asm_int32_bitinrangepublicpos_mask(x, s) != c_int32_bitinrangepublicpos_mask(x, s)) {
            zprintf("mismatch int32_bitinrangepublicpos_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_bitinrangepublicpos_mask(x, s), (int)c_int32_bitinrangepublicpos_mask(x, s));
            return 1;
        }
        if (asm_int32_bitinrangepublicpos_01(x, s) != c_int32_bitinrangepublicpos_01(x, s)) {
            zprintf("mismatch int32_bitinrangepublicpos_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_bitinrangepublicpos_01(x, s), (int)c_int32_bitinrangepublicpos_01(x, s));
            return 1;
        }
        if (asm_int32_shlmod(x, s) != c_int32_shlmod(x, s)) {
            zprintf("mismatch int32_shlmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_shlmod(x, s), (int)c_int32_shlmod(x, s));
            return 1;
        }
        if (asm_int32_shrmod(x, s) != c_int32_shrmod(x, s)) {
            zprintf("mismatch int32_shrmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_shrmod(x, s), (int)c_int32_shrmod(x, s));
            return 1;
        }
        if (asm_int32_nonzero_mask(x) != c_int32_nonzero_mask(x)) {
            zprintf("mismatch int32_nonzero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_nonzero_mask(x), (int)c_int32_nonzero_mask(x));
            return 1;
        }
        if (asm_int32_nonzero_01(x) != c_int32_nonzero_01(x)) {
            zprintf("mismatch int32_nonzero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_nonzero_01(x), (int)c_int32_nonzero_01(x));
            return 1;
        }
        if (asm_int32_positive_mask(x) != c_int32_positive_mask(x)) {
            zprintf("mismatch int32_positive_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_positive_mask(x), (int)c_int32_positive_mask(x));
            return 1;
        }
        if (asm_int32_positive_01(x) != c_int32_positive_01(x)) {
            zprintf("mismatch int32_positive_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_positive_01(x), (int)c_int32_positive_01(x));
            return 1;
        }
        if (asm_int32_zero_mask(x) != c_int32_zero_mask(x)) {
            zprintf("mismatch int32_zero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_zero_mask(x), (int)c_int32_zero_mask(x));
            return 1;
        }
        if (asm_int32_zero_01(x) != c_int32_zero_01(x)) {
            zprintf("mismatch int32_zero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_zero_01(x), (int)c_int32_zero_01(x));
            return 1;
        }
        if (asm_int32_unequal_mask(x, y) != c_int32_unequal_mask(x, y)) {
            zprintf("mismatch int32_unequal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_unequal_mask(x, y), (int)c_int32_unequal_mask(x, y));
            return 1;
        }
        if (asm_int32_unequal_01(x, y) != c_int32_unequal_01(x, y)) {
            zprintf("mismatch int32_unequal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_unequal_01(x, y), (int)c_int32_unequal_01(x, y));
            return 1;
        }
        if (asm_int32_equal_mask(x, y) != c_int32_equal_mask(x, y)) {
            zprintf("mismatch int32_equal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_equal_mask(x, y), (int)c_int32_equal_mask(x, y));
            return 1;
        }
        if (asm_int32_equal_01(x, y) != c_int32_equal_01(x, y)) {
            zprintf("mismatch int32_equal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_equal_01(x, y), (int)c_int32_equal_01(x, y));
            return 1;
        }
        if (asm_int32_min(x, y) != c_int32_min(x, y)) {
            zprintf("mismatch int32_min x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_min(x, y), (int)c_int32_min(x, y));
            return 1;
        }
        if (asm_int32_max(x, y) != c_int32_max(x, y)) {
            zprintf("mismatch int32_max x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_max(x, y), (int)c_int32_max(x, y));
            return 1;
        }
        if (asm_int32_smaller_mask(x, y) != c_int32_smaller_mask(x, y)) {
            zprintf("mismatch int32_smaller_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_smaller_mask(x, y), (int)c_int32_smaller_mask(x, y));
            return 1;
        }
        if (asm_int32_smaller_01(x, y) != c_int32_smaller_01(x, y)) {
            zprintf("mismatch int32_smaller_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_smaller_01(x, y), (int)c_int32_smaller_01(x, y));
            return 1;
        }
        if (asm_int32_leq_mask(x, y) != c_int32_leq_mask(x, y)) {
            zprintf("mismatch int32_leq_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_leq_mask(x, y), (int)c_int32_leq_mask(x, y));
            return 1;
        }
        if (asm_int32_leq_01(x, y) != c_int32_leq_01(x, y)) {
            zprintf("mismatch int32_leq_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_leq_01(x, y), (int)c_int32_leq_01(x, y));
            return 1;
        }
        if (asm_int32_bottomzeros_num(x) != c_int32_bottomzeros_num(x)) {
            zprintf("mismatch int32_bottomzeros_num x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int32_bottomzeros_num(x), (int)c_int32_bottomzeros_num(x));
            return 1;
        }
    }

    for (unsigned i = 0; i < n64; ++i) {
        int64_t x = vals64[i], y = vals64[(i * 7 + 3) % n64];
        int64_t s = (int64_t)((i * 13) % 64);
        if (asm_int64_negative_mask(x) != c_int64_negative_mask(x)) {
            zprintf("mismatch int64_negative_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_negative_mask(x), (int)c_int64_negative_mask(x));
            return 1;
        }
        if (asm_int64_unsigned_topbit_01(x) != c_int64_unsigned_topbit_01(x)) {
            zprintf("mismatch int64_unsigned_topbit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_unsigned_topbit_01(x), (int)c_int64_unsigned_topbit_01(x));
            return 1;
        }
        if (asm_int64_bottombit_mask(x) != c_int64_bottombit_mask(x)) {
            zprintf("mismatch int64_bottombit_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_bottombit_mask(x), (int)c_int64_bottombit_mask(x));
            return 1;
        }
        if (asm_int64_bottombit_01(x) != c_int64_bottombit_01(x)) {
            zprintf("mismatch int64_bottombit_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_bottombit_01(x), (int)c_int64_bottombit_01(x));
            return 1;
        }
        if (asm_int64_bitinrangepublicpos_mask(x, s) != c_int64_bitinrangepublicpos_mask(x, s)) {
            zprintf("mismatch int64_bitinrangepublicpos_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_bitinrangepublicpos_mask(x, s), (int)c_int64_bitinrangepublicpos_mask(x, s));
            return 1;
        }
        if (asm_int64_bitinrangepublicpos_01(x, s) != c_int64_bitinrangepublicpos_01(x, s)) {
            zprintf("mismatch int64_bitinrangepublicpos_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_bitinrangepublicpos_01(x, s), (int)c_int64_bitinrangepublicpos_01(x, s));
            return 1;
        }
        if (asm_int64_shlmod(x, s) != c_int64_shlmod(x, s)) {
            zprintf("mismatch int64_shlmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_shlmod(x, s), (int)c_int64_shlmod(x, s));
            return 1;
        }
        if (asm_int64_shrmod(x, s) != c_int64_shrmod(x, s)) {
            zprintf("mismatch int64_shrmod x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_shrmod(x, s), (int)c_int64_shrmod(x, s));
            return 1;
        }
        if (asm_int64_nonzero_mask(x) != c_int64_nonzero_mask(x)) {
            zprintf("mismatch int64_nonzero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_nonzero_mask(x), (int)c_int64_nonzero_mask(x));
            return 1;
        }
        if (asm_int64_nonzero_01(x) != c_int64_nonzero_01(x)) {
            zprintf("mismatch int64_nonzero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_nonzero_01(x), (int)c_int64_nonzero_01(x));
            return 1;
        }
        if (asm_int64_positive_mask(x) != c_int64_positive_mask(x)) {
            zprintf("mismatch int64_positive_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_positive_mask(x), (int)c_int64_positive_mask(x));
            return 1;
        }
        if (asm_int64_positive_01(x) != c_int64_positive_01(x)) {
            zprintf("mismatch int64_positive_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_positive_01(x), (int)c_int64_positive_01(x));
            return 1;
        }
        if (asm_int64_zero_mask(x) != c_int64_zero_mask(x)) {
            zprintf("mismatch int64_zero_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_zero_mask(x), (int)c_int64_zero_mask(x));
            return 1;
        }
        if (asm_int64_zero_01(x) != c_int64_zero_01(x)) {
            zprintf("mismatch int64_zero_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_zero_01(x), (int)c_int64_zero_01(x));
            return 1;
        }
        if (asm_int64_unequal_mask(x, y) != c_int64_unequal_mask(x, y)) {
            zprintf("mismatch int64_unequal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_unequal_mask(x, y), (int)c_int64_unequal_mask(x, y));
            return 1;
        }
        if (asm_int64_unequal_01(x, y) != c_int64_unequal_01(x, y)) {
            zprintf("mismatch int64_unequal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_unequal_01(x, y), (int)c_int64_unequal_01(x, y));
            return 1;
        }
        if (asm_int64_equal_mask(x, y) != c_int64_equal_mask(x, y)) {
            zprintf("mismatch int64_equal_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_equal_mask(x, y), (int)c_int64_equal_mask(x, y));
            return 1;
        }
        if (asm_int64_equal_01(x, y) != c_int64_equal_01(x, y)) {
            zprintf("mismatch int64_equal_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_equal_01(x, y), (int)c_int64_equal_01(x, y));
            return 1;
        }
        if (asm_int64_min(x, y) != c_int64_min(x, y)) {
            zprintf("mismatch int64_min x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_min(x, y), (int)c_int64_min(x, y));
            return 1;
        }
        if (asm_int64_max(x, y) != c_int64_max(x, y)) {
            zprintf("mismatch int64_max x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_max(x, y), (int)c_int64_max(x, y));
            return 1;
        }
        if (asm_int64_smaller_mask(x, y) != c_int64_smaller_mask(x, y)) {
            zprintf("mismatch int64_smaller_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_smaller_mask(x, y), (int)c_int64_smaller_mask(x, y));
            return 1;
        }
        if (asm_int64_smaller_01(x, y) != c_int64_smaller_01(x, y)) {
            zprintf("mismatch int64_smaller_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_smaller_01(x, y), (int)c_int64_smaller_01(x, y));
            return 1;
        }
        if (asm_int64_leq_mask(x, y) != c_int64_leq_mask(x, y)) {
            zprintf("mismatch int64_leq_mask x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_leq_mask(x, y), (int)c_int64_leq_mask(x, y));
            return 1;
        }
        if (asm_int64_leq_01(x, y) != c_int64_leq_01(x, y)) {
            zprintf("mismatch int64_leq_01 x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_leq_01(x, y), (int)c_int64_leq_01(x, y));
            return 1;
        }
        if (asm_int64_bottomzeros_num(x) != c_int64_bottomzeros_num(x)) {
            zprintf("mismatch int64_bottomzeros_num x=%d s=%d y=%d asm=%d c=%d\n",
                    (int)x, (int)s, (int)y, (int)asm_int64_bottomzeros_num(x), (int)c_int64_bottomzeros_num(x));
            return 1;
        }
    }

    for (unsigned i = 0; i < n16; ++i) {
        int16_t a = vals16[i], b = vals16[(i * 7 + 3) % n16];
        int16_t a1 = a, b1 = b, a2 = a, b2 = b;
        asm_int16_minmax(&a1, &b1);
        c_int16_minmax(&a2, &b2);
        if (a1 != a2 || b1 != b2) {
            zprintf("mismatch int16_minmax a=%d b=%d asm=(%d,%d) c=(%d,%d)\n",
                    (int)a, (int)b, (int)a1, (int)b1, (int)a2, (int)b2);
            return 1;
        }
    }

    for (unsigned i = 0; i < n32; ++i) {
        int32_t a = vals32[i], b = vals32[(i * 7 + 3) % n32];
        int32_t a1 = a, b1 = b, a2 = a, b2 = b;
        asm_int32_minmax(&a1, &b1);
        c_int32_minmax(&a2, &b2);
        if (a1 != a2 || b1 != b2) {
            zprintf("mismatch int32_minmax a=%d b=%d asm=(%d,%d) c=(%d,%d)\n",
                    (int)a, (int)b, (int)a1, (int)b1, (int)a2, (int)b2);
            return 1;
        }
    }

    for (unsigned i = 0; i < n64; ++i) {
        int64_t a = vals64[i], b = vals64[(i * 7 + 3) % n64];
        int64_t a1 = a, b1 = b, a2 = a, b2 = b;
        asm_int64_minmax(&a1, &b1);
        c_int64_minmax(&a2, &b2);
        if (a1 != a2 || b1 != b2) {
            zprintf("mismatch int64_minmax a=%d b=%d asm=(%d,%d) c=(%d,%d)\n",
                    (int)a, (int)b, (int)a1, (int)b1, (int)a2, (int)b2);
            return 1;
        }
    }

    return 0;
}