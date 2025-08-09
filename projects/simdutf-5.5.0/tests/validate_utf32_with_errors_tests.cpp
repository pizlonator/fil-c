#include "simdutf.h"

#include <tests/helpers/random_utf32.h>
#include <tests/helpers/test.h>

TEST(issue_531)
{
    const char32_t data[] = {0xdbdb, 0xff380000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
                            0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
                            0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
    constexpr std::size_t data_len = sizeof(data);
    const auto validation1 = implementation.validate_utf32_with_errors((const char32_t *) data,
                                                                       data_len);
    // got return [count=1, error=TOO_LARGE] from implementation rvv
    // got return [count=0, error=SURROGATE] from implementation fallback
    ASSERT_EQUAL(validation1.error, simdutf::error_code::SURROGATE);
    ASSERT_EQUAL(validation1.count, 0);
}

TEST_LOOP(1000, validate_utf32_with_errors__returns_success_for_valid_input) {
   simdutf::tests::helpers::random_utf32 generator{seed};
    const auto utf32{generator.generate(256, seed)};

    simdutf::result res = implementation.validate_utf32_with_errors(reinterpret_cast<const char32_t*>(utf32.data()), utf32.size());

    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    ASSERT_EQUAL(res.count, utf32.size());
}

TEST(validate_utf32_with_errors__returns_success_for_empty_string) {
  const char32_t* buf = (char32_t*)"";

  simdutf::result res = implementation.validate_utf32_with_errors(buf, 0);

  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, 0);
}

TEST_LOOP(10, validate_utf32_with_errors__returns_error_when_input_in_forbidden_range) {
    simdutf::tests::helpers::random_utf32 generator{seed};

    auto utf32{generator.generate(128)};
    const char32_t*  buf = reinterpret_cast<const char32_t*>(utf32.data());
    const size_t len = utf32.size();

    for (char32_t wrong_value = 0xd800; wrong_value <= 0xdfff; wrong_value++) {
      for (size_t i=0; i < utf32.size(); i++) {
        const char32_t old = utf32[i];
        utf32[i] = wrong_value;

        simdutf::result res = implementation.validate_utf32_with_errors(buf, len);

        ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
        ASSERT_EQUAL(res.count, i);

        utf32[i] = old;
      }
    }
}

TEST_LOOP(10, validate_utf32_with_errors__returns_error_when_input_too_large) {
    simdutf::tests::helpers::random_utf32 generator{seed};

    std::uniform_int_distribution<uint32_t> bad_range{0x110000, 0xffffffff};
    std::mt19937 gen{seed};

    auto utf32{generator.generate(128)};
    const char32_t*  buf = reinterpret_cast<const char32_t*>(utf32.data());
    const size_t len = utf32.size();

    for (size_t r = 0; r < 1000; r++) {
      uint32_t wrong_value = bad_range(gen);
      for (size_t i = 0; i < utf32.size(); i++) {
        const char32_t old = utf32[i];
        utf32[i] = wrong_value;

        simdutf::result res = implementation.validate_utf32_with_errors(buf, len);

        ASSERT_EQUAL(res.error, simdutf::error_code::TOO_LARGE);
        ASSERT_EQUAL(res.count, i);

        utf32[i] = old;
      }
    }
}

TEST_MAIN
