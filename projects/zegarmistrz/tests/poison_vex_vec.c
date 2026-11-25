// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: VEX vector operations storing through vec_store_dst (vpcmpeqb, vpand)
// must preserve register taint into vpmovmskb -> branch. Regression test for
// const VecVal& silently decaying to const uint8_t* with a default clean Taint.
// Expect exit 139 with cannot-branch.
#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char a[64];
static char b[64];

__attribute__((noinline))
static int cmp64_avx2(const char* p, const char* q) {
    __m256i x0 = _mm256_loadu_si256((const __m256i*)p);
    __m256i y0 = _mm256_loadu_si256((const __m256i*)q);
    __m256i x1 = _mm256_loadu_si256((const __m256i*)(p + 32));
    __m256i y1 = _mm256_loadu_si256((const __m256i*)(q + 32));
    __m256i c0 = _mm256_cmpeq_epi8(x0, y0);
    __m256i c1 = _mm256_cmpeq_epi8(x1, y1);
    __m256i both = _mm256_and_si256(c0, c1);
    int mask = _mm256_movemask_epi8(both);
    return mask != -1;
}

int main(void) {
    // Poison a[0..63] with CANNOT_BRANCH (bit 2).
    magic(1, a, sizeof(a), 4u);
    if (cmp64_avx2(a, b))
        abort();
    if (write(1, "missed-branch\n", 14) < 0) return 2;
    return 0;
}
