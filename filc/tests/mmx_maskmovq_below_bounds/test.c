#include <xmmintrin.h>
#include <stdfil.h>

int main() {
    char buf[16];
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;
    
    // Store below allocation with all mask bits set - should fail
    __m64 value = _mm_set1_pi8(0x42);
    __m64 mask = _mm_set1_pi8(-1);  // All bits set - all elements enabled
    _mm_maskmove_si64(value, mask, buf - 8);  // Below bounds - should trigger safety error
    
    return 0;
}
