#include <immintrin.h>
#include <stdfil.h>

int main() {
    char buf[32];
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;
    
    // Store below allocation with all mask bits set - should fail
    __m128i value = _mm_set1_epi8(0x42);
    __m128i mask = _mm_set1_epi8(-1);  // All bits set - all elements enabled
    _mm_maskmoveu_si128(value, mask, buf - 16);  // Below bounds - should trigger safety error
    
    return 0;
}
