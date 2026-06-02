#include <xmmintrin.h>
#include <immintrin.h>
#include <stdfil.h>

int main() {
    char buf[32];
    
    // Test 1: Basic masked store - only store upper 4 bytes
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;
    
    __m64 value1 = _mm_set_pi8(0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00);
    __m64 mask1 = _mm_set_pi8(0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00);
    _mm_maskmove_si64(value1, mask1, &buf[4]);
    _mm_sfence();
    
    // Verify unmasked elements unchanged (buf[4..7] should be 0xAA)
    for (int i = 0; i < 4; i++) {
        if (buf[4 + i] != (char)0xAA) {
            zprint("FAILED: Test 1 - unmasked elements were modified");
            return 1;
        }
    }
    // Verify masked elements stored (buf[8..11] should have values 4..7)
    for (int i = 0; i < 4; i++) {
        if (buf[8 + i] != 4 + i) {
            zprint("FAILED: Test 1 - masked elements not stored correctly");
            return 1;
        }
    }
    
    // Test 2: Masked access below bounds (should succeed due to mask)
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;
    
    __m64 value2 = _mm_set_pi8(0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00);
    __m64 mask2 = _mm_set_pi8(0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00);
    _mm_maskmove_si64(value2, mask2, buf - 4);
    _mm_sfence();
    
    // Verify buf[0..3] have values 4..7
    for (int i = 0; i < 4; i++) {
        if (buf[i] != (char)(4 + i)) {
            zprint("FAILED: Test 2 - masked below bounds");
            return 1;
        }
    }
    // Verify buf[4..15] unchanged
    for (int i = 4; i < 16; i++) {
        if (buf[i] != (char)0xAA) {
            zprint("FAILED: Test 2 - elements after store range modified");
            return 1;
        }
    }
    
    // Test 3: Masked access above bounds (should succeed due to mask)
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;
    
    __m64 value3 = _mm_set_pi8(0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00);
    __m64 mask3 = _mm_set_pi8(0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF);
    _mm_maskmove_si64(value3, mask3, buf + 12);
    _mm_sfence();
    
    // Verify buf[0..11] unchanged
    for (int i = 0; i < 12; i++) {
        if (buf[i] != (char)0xAA) {
            zprint("FAILED: Test 3 - elements before store range modified");
            return 1;
        }
    }
    // Verify buf[12..15] have values 0..3
    for (int i = 0; i < 4; i++) {
        if (buf[12 + i] != (char)i) {
            zprint("FAILED: Test 3 - masked above bounds");
            return 1;
        }
    }
    
    zprint("SUCCESS: All maskmovq tests passed");
    return 0;
}
