#include <immintrin.h>
#include <stdfil.h>

int main() {
    char buf[64];
    
    // Test 1: Basic masked store - only store upper 8 bytes
    for (int i = 0; i < 64; i++) buf[i] = 0xAA;
    
    __m128i value1 = _mm_set_epi8(
        0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
    );
    __m128i mask1 = _mm_set_epi8(
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    );
    _mm_maskmoveu_si128(value1, mask1, &buf[8]);
    _mm_sfence();
    
    // Verify unmasked elements unchanged (buf[8..15] should be 0xAA)
    for (int i = 0; i < 8; i++) {
        if (buf[8 + i] != (char)0xAA) {
            zprint("FAILED: Test 1 - unmasked elements were modified");
            return 1;
        }
    }
    // Verify masked elements stored (buf[16..23] should have values 8..15)
    for (int i = 0; i < 8; i++) {
        if (buf[16 + i] != 8 + i) {
            zprint("FAILED: Test 1 - masked elements not stored correctly");
            return 1;
        }
    }
    
    // Test 2: Masked access below bounds (should succeed due to mask)
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;
    
    __m128i value2 = _mm_set_epi8(
        0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
    );
    __m128i mask2 = _mm_set_epi8(
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    );
    _mm_maskmoveu_si128(value2, mask2, buf - 8);
    _mm_sfence();
    
    // Verify buf[0..7] have values 8..15
    for (int i = 0; i < 8; i++) {
        if (buf[i] != (char)(8 + i)) {
            zprint("FAILED: Test 2 - masked below bounds");
            return 1;
        }
    }
    // Verify buf[8..31] unchanged
    for (int i = 8; i < 32; i++) {
        if (buf[i] != (char)0xAA) {
            zprint("FAILED: Test 2 - elements after store range modified");
            return 1;
        }
    }
    
    // Test 3: Masked access above bounds (should succeed due to mask)
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;
    
    __m128i value3 = _mm_set_epi8(
        0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
    );
    __m128i mask3 = _mm_set_epi8(
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    );
    _mm_maskmoveu_si128(value3, mask3, buf + 16);
    _mm_sfence();
    
    // Verify buf[0..15] unchanged
    for (int i = 0; i < 16; i++) {
        if (buf[i] != (char)0xAA) {
            zprint("FAILED: Test 3 - elements before store range modified");
            return 1;
        }
    }
    // Verify buf[16..23] have values 0..7
    for (int i = 0; i < 8; i++) {
        if (buf[16 + i] != (char)i) {
            zprint("FAILED: Test 3 - masked above bounds");
            return 1;
        }
    }
    // Verify buf[24..31] unchanged
    for (int i = 24; i < 32; i++) {
        if (buf[i] != (char)0xAA) {
            zprint("FAILED: Test 3 - elements after store range modified");
            return 1;
        }
    }
    
    zprint("SUCCESS: All maskmov_dqu tests passed");
    return 0;
}
