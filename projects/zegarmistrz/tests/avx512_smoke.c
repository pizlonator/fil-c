// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// AVX512 smoke: masked ops, K ops, broadcast, vpcmp, vptestm. Prints hex;
// run-tests diffs native vs emulated output.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void p64(const char* n, uint8_t* p, int x) {
    printf("%s:", n);
    for (int i = 0; i < x; i++)
        printf(" %02x", p[i]);
    printf("\n");
}

int main(void) {
    uint8_t A[64], B[64], C[64], O[64];
    for (int i = 0; i < 64; i++) {
        A[i] = (uint8_t)(i * 3 + 1);
        B[i] = (uint8_t)(i * 5 + 2);
        C[i] = (uint8_t)(0xA0 + i);
    }
    memset(O, 0xCC, sizeof(O));
    __m512i a = _mm512_loadu_si512(A), b = _mm512_loadu_si512(B);
    printf("cmpeq_b: %016llx\n",
           (unsigned long long)_mm512_cmpeq_epi8_mask(a, b));
    printf("cmpb_gt: %016llx\n",
           (unsigned long long)_mm512_cmp_epi8_mask(a, b, _MM_CMPINT_GT));
    printf("kortest_c=%d\n",
           _kortestc_mask64_u8(_mm512_cmpeq_epi8_mask(a, b),
                               _mm512_cmpeq_epi8_mask(a, a)));
    __m512i bc = _mm512_broadcastb_epi8(_mm_cvtsi32_si128(0x5a));
    _mm512_storeu_si512(O, bc);
    p64("bcastb", O, 64);
    _mm512_storeu_si512(O, _mm512_min_epu8(a, b));
    p64("minub", O, 64);
    printf("testnm: %016llx\n",
           (unsigned long long)_mm512_testn_epi8_mask(a, b));
    __m512i madd = _mm512_maskz_add_epi32(0x5555, a, b);
    _mm512_storeu_si512(O, madd);
    p64("add_z", O, 64);
    memset(O, 0xCC, sizeof(O));
    _mm512_mask_storeu_epi64(O, 0xFF, a);
    p64("mov64m", O, 64);
    __m512i tl = _mm512_ternarylogic_epi32(a, b, _mm512_loadu_si512(C), 0x96);
    _mm512_storeu_si512(O, tl);
    p64("ternlog", O, 64);
    printf("OK\n");
    return 0;
}
