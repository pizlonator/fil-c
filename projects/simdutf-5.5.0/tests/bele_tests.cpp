/**
 * Big-Endian/Little-endian tests.
 */
#include "simdutf.h"
#include <tests/helpers/test.h>

// We use explicit arrays so that no funny business is possible.
const unsigned char utf8_string[] = {0x40,0xc2,0xa7,0xe2,0x88,0x88,0xf0,0x9d,0x92,0xaa};
const char *utf8 = reinterpret_cast<const char*>(utf8_string);
const size_t utf8_size = sizeof(utf8_string)/sizeof(uint8_t);

alignas(char16_t) const unsigned char utf16le_string[] = {0x40,0x00,0xa7,0x00,0x08,0x22,0x35,0xd8,0xaa,0xdc};
const char16_t *utf16le = reinterpret_cast<const char16_t*>(utf16le_string);
const size_t utf16_size = sizeof(utf16le_string)/sizeof(uint16_t);


alignas(char16_t) const unsigned char utf16be_string[] = {0x00,0x40,0x00,0xa7,0x22,0x08,0xd8,0x35,0xdc,0xaa};
const char16_t *utf16be = reinterpret_cast<const char16_t*>(utf16be_string);
#if SIMDUTF_IS_BIG_ENDIAN
const char16_t *utf16 = utf16be;
#else
const char16_t *utf16 = utf16le;
#endif

// Native order
#if SIMDUTF_IS_BIG_ENDIAN
alignas(char32_t) const unsigned char utf32_string[] = {0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xa7,0x00,0x00,0x22,0x08,0x00,0x01,0xd4,0xaa};
const char32_t *utf32 = reinterpret_cast<const char32_t*>(utf32_string);
#else
alignas(char32_t) const unsigned char utf32_string[] = {0x40,0x00,0x00,0x00,0xa7,0x00,0x00,0x00,0x08,0x22,0x00,0x00,0xaa,0xd4,0x01,0x00};
const char32_t *utf32 = reinterpret_cast<const char32_t*>(utf32_string);
#endif
const size_t utf32_size = sizeof(utf32_string)/sizeof(char32_t);
const size_t number_of_code_points = utf32_size;


TEST(validate_utf8) {
    simdutf::result res = implementation.validate_utf8_with_errors(utf8, utf8_size);
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
}

TEST(validate_utf16le) {
    simdutf::result res = implementation.validate_utf16le_with_errors(utf16le, utf16_size);
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
}

TEST(validate_utf16be) {
    simdutf::result res = implementation.validate_utf16be_with_errors(utf16be, utf16_size);
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
}

TEST(validate_utf32) {
    simdutf::result res = implementation.validate_utf32_with_errors(utf32, utf32_size);
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
}

TEST(count_utf8) {
    size_t count = implementation.count_utf8(utf8, utf8_size);
    ASSERT_EQUAL(count, number_of_code_points);
}

TEST(count_utf16le) {
    size_t count = implementation.count_utf16le(utf16le, utf16_size);
    ASSERT_EQUAL(count, number_of_code_points);
}

TEST(count_utf16be) {
    size_t count = implementation.count_utf16be(reinterpret_cast<const char16_t*>(utf16be_string), sizeof(utf16be_string)/sizeof(uint16_t));
    ASSERT_EQUAL(count, number_of_code_points);
}

TEST(convert_utf8_to_utf16le) {
    char16_t buffer[utf16_size];
    size_t count = implementation.convert_utf8_to_utf16le(utf8, utf8_size, buffer);
    ASSERT_EQUAL(count, utf16_size);
    for(size_t i = 0; i < utf16_size; i++) {
        ASSERT_EQUAL(uint16_t(buffer[i]), uint16_t(utf16le[i]));
    }
}

TEST(convert_utf8_to_utf16be) {
    char16_t buffer[utf16_size];
    size_t count = implementation.convert_utf8_to_utf16be(utf8, utf8_size, buffer);
    ASSERT_EQUAL(count, utf16_size);
    for(size_t i = 0; i < utf16_size; i++) {
        ASSERT_EQUAL(uint16_t(buffer[i]), uint16_t(utf16be[i]));
    }
}


TEST(convert_utf8_to_utf32) {
    char32_t buffer[utf32_size];
    size_t count = implementation.convert_utf8_to_utf32(utf8, utf8_size, buffer);
    ASSERT_EQUAL(count, utf32_size);
    for(size_t i = 0; i < utf32_size; i++) {
        ASSERT_EQUAL(uint32_t(buffer[i]), uint32_t(utf32[i]));
    }
}

TEST(convert_utf32_to_utf8) {
    char buffer[utf8_size];
    size_t count = implementation.convert_utf32_to_utf8(utf32, utf32_size, buffer);
    ASSERT_EQUAL(count, utf8_size);
    for(size_t i = 0; i < utf8_size; i++) {
        ASSERT_EQUAL(buffer[i], utf8[i]);
    }
}

TEST(convert_utf32_to_utf16be) {
    char buffer[utf8_size];
    size_t count = implementation.convert_utf32_to_utf8(utf32, utf32_size, buffer);
    ASSERT_EQUAL(count, utf8_size);
    for(size_t i = 0; i < utf8_size; i++) {
        ASSERT_EQUAL(buffer[i], utf8[i]);
    }
}

TEST(convert_utf32_to_utf16le) {
    char buffer[utf8_size];
    size_t count = implementation.convert_utf32_to_utf8(utf32, utf32_size, buffer);
    ASSERT_EQUAL(count, utf8_size);
    for(size_t i = 0; i < utf8_size; i++) {
        ASSERT_EQUAL(buffer[i], utf8[i]);
    }
}


TEST(convert_utf16le_to_utf8) {
    char buffer[utf8_size];
    size_t count = implementation.convert_utf16le_to_utf8(utf16le, utf16_size, buffer);
    ASSERT_EQUAL(count, utf8_size);
    for(size_t i = 0; i < utf8_size; i++) {
        ASSERT_EQUAL(buffer[i], utf8[i]);
    }
}


TEST(convert_utf16le_to_utf32) {
    char32_t buffer[utf32_size];
    size_t count = implementation.convert_utf16le_to_utf32(utf16le, utf16_size, buffer);
    ASSERT_EQUAL(count, utf32_size);
    for(size_t i = 0; i < utf32_size; i++) {
        ASSERT_EQUAL(uint32_t(buffer[i]), uint32_t(utf32[i]));
    }
}

TEST(convert_utf16be_to_utf8) {
    char buffer[utf8_size];
    size_t count = implementation.convert_utf16be_to_utf8(utf16be, utf16_size, buffer);
    ASSERT_EQUAL(count, utf8_size);
    for(size_t i = 0; i < utf8_size; i++) {
        ASSERT_EQUAL(buffer[i], utf8[i]);
    }
}

TEST(convert_utf16be_to_utf32) {
    char32_t buffer[utf32_size];
    size_t count = implementation.convert_utf16be_to_utf32(utf16be, utf16_size, buffer);
    ASSERT_EQUAL(count, utf32_size);
    for(size_t i = 0; i < utf32_size; i++) {
        ASSERT_EQUAL(uint32_t(buffer[i]), uint32_t(utf32[i]));
    }
}

TEST_MAIN
