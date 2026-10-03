// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// VEX-classic smoke: K ops, PMADD/PHADD/PSADBW/PAVG/PMULH, PCMPESTRI,
// HADD/ADDSUB, EXTRACTPS/INSERTPS, BEXTR, gathers. Prints hex; run-tests
// diffs native vs emulated output.
#include <immintrin.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static void p64(const char* n, uint8_t* p, int x) { printf("%s:", n); for (int i = 0; i < x; i++) printf(" %02x", p[i]); printf("\n"); }
int main() {
    uint8_t A[64], B[64], O[64];
    for (int i = 0; i < 64; i++) { A[i] = (uint8_t)(i + 1); B[i] = (uint8_t)(0x80 + i); }
    memset(O, 0, sizeof O);
    __m512i a = _mm512_loadu_si512(A), b = _mm512_loadu_si512(B);
    // K ops
    __mmask16 k1 = 0x1234, k2 = 0x00FF;
    printf("kor=%04x kand=%04x kxor=%04x knot=%04x kadd=%04x\n",
        _kor_mask16(k1, k2), _kand_mask16(k1, k2), _kxor_mask16(k1, k2),
        (unsigned)_knot_mask16(k1), (unsigned)_kadd_mask16(k1, k2));
    printf("kortest_z=%d kortest_c=%d\n", _kortestz_mask16_u8(k1, k2), _kortestc_mask16_u8(k1, k2));
    // pmaddwd / pmaddubsw / psadbw / pavg
    __m128i s1 = _mm_loadu_si128((__m128i*)A), s2 = _mm_loadu_si128((__m128i*)B);
    _mm_storeu_si128((__m128i*)O, _mm_madd_epi16(s1, s2)); p64("pmaddwd", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_maddubs_epi16(s1, s2)); p64("pmaddubsw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_sad_epu8(s1, s2)); p64("psadbw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_avg_epu8(s1, s2)); p64("pavgb", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_mulhi_epu16(s1, s2)); p64("pmulhuw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_mulhi_epi16(s1, s2)); p64("pmulhw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_mul_epu32(s1, s2)); p64("pmuludq", O, 16);
    // phadd
    _mm_storeu_si128((__m128i*)O, _mm_hadd_epi16(s1, s2)); p64("phaddw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_hadd_epi32(s1, s2)); p64("phaddd", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_hsub_epi16(s1, s2)); p64("phsubw", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_hadds_epi16(s1, s2)); p64("phaddsw", O, 16);
    // pcmpestri/m
    const char* hay = "hello world, testing 123";
    const char* ndl = "o";
    int idx = _mm_cmpestri(_mm_loadu_si128((__m128i*)"world.........."), 5,
                           _mm_loadu_si128((__m128i*)hay), 24, _SIDD_CMP_EQUAL_EACH);
    printf("estri=%d\n", idx);
    __m128i mres = _mm_cmpestrm(_mm_loadu_si128((__m128i*)"o...."), 1,
                                _mm_loadu_si128((__m128i*)hay), 24, _SIDD_CMP_EQUAL_ANY);
    _mm_storeu_si128((__m128i*)O, mres); p64("estrm", O, 16);
    // haddps/addsubps
    float fa[4] = {1,2,3,4}, fb[4] = {5,6,7,8}, fo[4];
    _mm_storeu_ps(fo, _mm_hadd_ps(_mm_loadu_ps(fa), _mm_loadu_ps(fb))); p64("haddps", (uint8_t*)fo, 16);
    _mm_storeu_ps(fo, _mm_addsub_ps(_mm_loadu_ps(fa), _mm_loadu_ps(fb))); p64("addsubps", (uint8_t*)fo, 16);
    // extractps/insertps
    printf("ext=%08x\n", (unsigned)_mm_extract_ps(_mm_castsi128_ps(s1), 2));
    __m128i ins = _mm_castps_si128(_mm_insert_ps(_mm_castsi128_ps(s1), _mm_castsi128_ps(s2), 0x1E));
    _mm_storeu_si128((__m128i*)O, ins); p64("insertps", O, 16);
    // bextr
    printf("bextr=%lx\n", (unsigned long)_bextr_u64(0x123456789ABCDEFULL, 4, 12));
    // gathers
    int32_t base[16] = {10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160};
    __m256i vidx = _mm256_set_epi32(7,6,5,4,3,2,1,0);
    __m256i g = _mm256_i32gather_epi32(base, vidx, 4);
    _mm256_storeu_si256((__m256i*)O, g); p64("gather", O, 32);
    // evec gather
    __m512i vidx512 = _mm512_set_epi32(15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0);
    __m512i g2 = _mm512_i32gather_epi32(vidx512, base, 4);
    _mm512_storeu_si512(O, g2); p64("egather", O, 64);
    // converts
    _mm_storeu_si128((__m128i*)O, _mm_packs_epi16(s1, s2)); p64("packsswb", O, 16);
    _mm_storeu_si128((__m128i*)O, _mm_packus_epi16(s1, s2)); p64("packuswb", O, 16);
    printf("OK\n");
    (void)ndl;
    return 0;
}
