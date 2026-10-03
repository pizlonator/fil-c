// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// AES/SHA/FMA smoke: prints hex; run-tests diffs native vs emulated output.
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
    uint8_t K[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                     0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t P[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                     0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint8_t O[64];
    __m128i k = _mm_loadu_si128((__m128i*)K), p = _mm_loadu_si128((__m128i*)P);
    __m128i o;
    o = _mm_aesenc_si128(p, k);
    _mm_storeu_si128((__m128i*)O, o);
    p64("enc", O, 16);
    o = _mm_aesdec_si128(o, k);
    _mm_storeu_si128((__m128i*)O, o);
    p64("dec", O, 16);
    o = _mm_aeskeygenassist_si128(k, 0x01);
    _mm_storeu_si128((__m128i*)O, o);
    p64("kga", O, 16);
    o = _mm_clmulepi64_si128(p, k, 0x00);
    _mm_storeu_si128((__m128i*)O, o);
    p64("pcl", O, 16);
    uint8_t D[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    __m128i d = _mm_loadu_si128((__m128i*)D);
    o = _mm_sha1msg1_epu32(p, d);
    _mm_storeu_si128((__m128i*)O, o);
    p64("sha1msg1", O, 16);
    o = _mm_sha256rnds2_epu32(p, d, k);
    _mm_storeu_si128((__m128i*)O, o);
    p64("sha256rnds2", O, 16);
    float fa[8] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};
    float fb[8] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f};
    float fc[8] = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f};
    float fo[8];
    _mm256_storeu_ps(fo, _mm256_fmadd_ps(_mm256_loadu_ps(fa),
                                         _mm256_loadu_ps(fb),
                                         _mm256_loadu_ps(fc)));
    printf("fmadd: %a %a %a %a\n", fo[0], fo[1], fo[2], fo[3]);
    printf("OK\n");
    return 0;
}
