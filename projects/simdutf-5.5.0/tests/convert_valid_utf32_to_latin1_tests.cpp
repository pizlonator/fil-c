#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf8_to_utf16_test_base;
}

TEST(issue_convert_valid_utf32_to_latin1_2104d5a31440e3ed)
{
#if SIMDUTF_IS_BIG_ENDIAN
    alignas(4) const unsigned char data[] = {0x00, 0x05, 0x20, 0x20};
#else
    alignas(4) const unsigned char data[] = {0x20, 0x20, 0x05, 0x00};
#endif
    constexpr std::size_t data_len_bytes = sizeof(data);
    constexpr std::size_t data_len = data_len_bytes / sizeof(char32_t);
    const auto validation1 = implementation.validate_utf32_with_errors((const char32_t *) data,
                                                                       data_len);
    ASSERT_EQUAL(validation1.count, 1);
    ASSERT_EQUAL(validation1.error, simdutf::error_code::SUCCESS);

    const bool validation2 = implementation.validate_utf32((const char32_t *) data, data_len);
    ASSERT_EQUAL(validation1.error == simdutf::error_code::SUCCESS, validation2);

    const auto outlen = implementation.latin1_length_from_utf32(data_len);
    ASSERT_EQUAL(outlen, 1);
    std::vector<char> output(outlen);
    const auto r = implementation.convert_valid_utf32_to_latin1((const char32_t *) data,
                                                                data_len,
                                                                output.data());
    /*
     * fallback gets 1, the others 0
     */
    ASSERT_EQUAL(r, 0);
}

TEST(convert_latin1_only) {
  size_t counter = 0;
  auto generator = [&counter]() -> uint32_t {
    return counter++ & 0xFF;
  };

  auto procedure = [&implementation](const char32_t* utf32, size_t size, char* latin1) -> size_t {
    return implementation.convert_valid_utf32_to_latin1(utf32, size, latin1);
  };
  auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
    return implementation.latin1_length_from_utf32(size);
  };
  for (size_t size: input_size) {
    simdutf::tests::helpers::transcode_utf32_to_latin1_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
    ASSERT_TRUE(test.check_size(size_procedure));
  }
}

TEST_MAIN
