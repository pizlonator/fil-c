#include "simdutf.h"
#include <memory>
#include <iostream>

#include <tests/helpers/test.h>
TEST(test_empty) {
    // zero storage size gives a nullptr as input which is of interest to test
    for (std::size_t inputstoragesize : {0, 1}) {
        //inputs
        const std::vector<char> i8data(inputstoragesize);
        const std::vector<char16_t> i16data(inputstoragesize);
        const std::vector<char32_t> i32data(inputstoragesize);
        //outputs
        std::vector<char> o8data;
        std::vector<char16_t> o16data;
        std::vector<char32_t> o32data;

        const auto *i8 = i8data.data();
        const auto *i16 = i16data.data();
        const auto *i32 = i32data.data();
        auto *o8 = o8data.data();
        auto *o16 = o16data.data();
        auto *o32 = o32data.data();

        // empty input is interpreted as utf-8
        ASSERT_EQUAL(simdutf::encoding_type::UTF8, implementation.autodetect_encoding(i8, 0));
        ASSERT_EQUAL(0, implementation.base64_length_from_binary(0));

        auto test_base64 = [](const simdutf::result &r) {
            ASSERT_EQUAL(r.count, 0);
            ASSERT_EQUAL(r.error, simdutf::error_code::SUCCESS);
        };

        test_base64(implementation.base64_to_binary(i16, 0, o8));
        test_base64(implementation.base64_to_binary(i8, 0, o8));
        ASSERT_EQUAL(0, implementation.binary_to_base64(i8, 0, o8));

        /* returns void */ implementation.change_endianness_utf16(i16, 0, o16);
        ASSERT_EQUAL(0, implementation.convert_latin1_to_utf16be(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_latin1_to_utf16le(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_utf32_to_utf16be(i32, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_utf32_to_utf16le(i32, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_utf8_to_utf16be(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_utf8_to_utf16le(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16be_to_latin1(i16, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16be_to_utf32(i16, 0, o32));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16be_to_utf8(i16, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16le_to_latin1(i16, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16le_to_utf32(i16, 0, o32));
        ASSERT_EQUAL(0, implementation.convert_valid_utf16le_to_utf8(i16, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf32_to_latin1(i32, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf32_to_utf16be(i32, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_valid_utf32_to_utf16le(i32, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_valid_utf32_to_utf8(i32, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf8_to_latin1(i8, 0, o8));
        ASSERT_EQUAL(0, implementation.convert_valid_utf8_to_utf16be(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_valid_utf8_to_utf16le(i8, 0, o16));
        ASSERT_EQUAL(0, implementation.convert_valid_utf8_to_utf32(i8, 0, o32));
        ASSERT_EQUAL(0, implementation.count_utf16be(i16, 0));
        ASSERT_EQUAL(0, implementation.count_utf16le(i16, 0));
        ASSERT_EQUAL(0, implementation.count_utf8(i8, 0));
        ASSERT_EQUAL(0, implementation.latin1_length_from_utf16(0));
        ASSERT_EQUAL(0, implementation.latin1_length_from_utf32(0));
        ASSERT_EQUAL(0, implementation.latin1_length_from_utf8(i8, 0));
        ASSERT_EQUAL(0, implementation.maximal_binary_length_from_base64(i16, 0));
        ASSERT_EQUAL(0, implementation.utf16_length_from_latin1(0));
        ASSERT_EQUAL(0, implementation.utf16_length_from_utf32(i32, 0));
        ASSERT_EQUAL(0, implementation.utf16_length_from_utf8(i8, 0));
        ASSERT_EQUAL(0, implementation.utf32_length_from_latin1(0));
        ASSERT_EQUAL(0, implementation.utf32_length_from_utf16be(i16, 0));
        ASSERT_EQUAL(0, implementation.utf32_length_from_utf16le(i16, 0));
        ASSERT_EQUAL(0, implementation.utf32_length_from_utf8(i8, 0));
        ASSERT_EQUAL(0, implementation.utf8_length_from_latin1(i8, 0));
        ASSERT_EQUAL(0, implementation.utf8_length_from_utf16be(i16, 0));
        ASSERT_EQUAL(0, implementation.utf8_length_from_utf16le(i16, 0));
        ASSERT_EQUAL(0, implementation.utf8_length_from_utf32(i32, 0));

        auto is_valid = [](const simdutf::result &r) {
            return r.count == 0 && r.error == simdutf::error_code::SUCCESS;
        };

        ASSERT_TRUE(implementation.validate_utf16be(i16, 0));
        ASSERT_TRUE(is_valid(implementation.validate_utf16be_with_errors(i16, 0)));
        ASSERT_TRUE(implementation.validate_utf16le(i16, 0));
        ASSERT_TRUE(is_valid(implementation.validate_utf16le_with_errors(i16, 0)));
        ASSERT_TRUE(implementation.validate_utf32(i32, 0));
        ASSERT_TRUE(is_valid(implementation.validate_utf32_with_errors(i32, 0)));
        ASSERT_TRUE(implementation.validate_utf8(i8, 0));
        ASSERT_TRUE(is_valid(implementation.validate_utf8_with_errors(i8, 0)));
    }
}

TEST_MAIN
