#include "simdutf.h"

#include <array>

#include <tests/reference/validate_utf16.h>
#include <tests/reference/decode_utf16.h>
#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf16_to_latin1_test_base;

  constexpr int trials = 1000;
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(issue_convert_utf16le_to_latin1_with_errors_38c7a3a4c673f504) {
 alignas(2) const unsigned char data[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0xee, 0xf6};
 constexpr std::size_t data_len_bytes=sizeof(data);
 constexpr std::size_t data_len=data_len_bytes/sizeof(char16_t);
const auto validation1=implementation.validate_utf16le_with_errors((const char16_t*) data,
 data_len);
   ASSERT_EQUAL(validation1.count, 69);
   ASSERT_EQUAL(validation1.error, simdutf::error_code::SUCCESS);

const bool validation2=implementation.validate_utf16le((const char16_t*) data,
 data_len);
   ASSERT_EQUAL(validation1.error==simdutf::error_code::SUCCESS,validation2);

 if(validation1.error!= simdutf::error_code::SUCCESS) {return;}
const auto outlen=implementation.latin1_length_from_utf16(data_len);
std::vector<char> output(outlen);
const auto r = implementation.convert_utf16le_to_latin1_with_errors((const char16_t*) data
, data_len
, output.data());
 ASSERT_EQUAL(r.error,simdutf::error_code::TOO_LARGE);
 ASSERT_EQUAL(r.count,67);
}
// end testcase

TEST(issue_convert_utf16le_to_latin1_with_errors_cbf29ce48422238a) {
   const unsigned char data[] = {0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20,
                                 0x00,
                                 0x20};
   constexpr std::size_t data_len_bytes = sizeof(data);
   constexpr std::size_t data_len = data_len_bytes / sizeof(char16_t);
   std::vector<char> output(4 * data_len);
   const auto r = implementation.convert_utf16le_to_latin1_with_errors((const char16_t *) data,
                                                                       data_len,
                                                                       output.data());
   /*
   got return [count=0, error=TOO_LARGE] from implementation icelake
   got return [count=0, error=TOO_LARGE] from implementation haswell
   got return [count=8, error=SUCCESS] from implementation westmere
   got return [count=0, error=TOO_LARGE] from implementation fallback
   */
   ASSERT_EQUAL(r.count, 0);
   ASSERT_EQUAL(r.error, simdutf::error_code::TOO_LARGE);
}
#endif

TEST_LOOP(trials, convert_2_UTF16_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00ff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* latin1) -> size_t {
      simdutf::result res = implementation.convert_utf16le_to_latin1_with_errors(utf16, size, latin1);
      ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
      return res.count;
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.latin1_length_from_utf16(size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_latin1_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST(convert_fails_if_input_too_large) {
  uint32_t seed{1234};
  simdutf::tests::helpers::RandomInt generator(0xff, 0xffff, seed);
  const size_t size = 64;
  transcode_utf16_to_latin1_test_base test([](){ return '*'; }, size+32);

  for (size_t j = 0; j < 1000; j++) {

    uint16_t wrong_value = generator();
    #if SIMDUTF_IS_BIG_ENDIAN // Big endian systems invert the declared generator's numbers when committed to memory.
    // Each codepoints above 255 are thus mirrored.
    // e.g. abcd becomes cdab, and vice versa. This is for most codepoints,not a cause for concern.
    // One case is however problematic, that of the numbers in the BE format 0xYY00 where the mirror image indicates a number beneath 255
    // which is undesirable in this particular test.
    if ((wrong_value & 0xFF00) != 0){
      // In this case, we swap bytes of the generated value:
      wrong_value = uint16_t((wrong_value >> 8) | (wrong_value << 8));
    }
    #endif

    for (size_t i=0; i < size; i++) {

  auto procedure = [&implementation,&i](const char16_t* utf16, size_t size, char* latin1) -> size_t {
    simdutf::result res = implementation.convert_utf16le_to_latin1_with_errors(utf16, size, latin1);
    ASSERT_EQUAL(res.error,5);
    ASSERT_EQUAL(res.count,i);
    return 0;
    
  };

      auto old = test.input_utf16[i];
      test.input_utf16[i] = wrong_value;
      ASSERT_TRUE(test(procedure));
      test.input_utf16[i] = old;
    }
  }
}

TEST_MAIN
