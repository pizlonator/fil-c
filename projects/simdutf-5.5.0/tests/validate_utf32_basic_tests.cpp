#include "simdutf.h"

#include <array>

#include <tests/helpers/random_utf32.h>
#include <tests/helpers/test.h>

TEST_LOOP(1000, validate_utf32__returns_true_for_valid_input) {
    simdutf::tests::helpers::random_utf32 generator{seed};
    const auto utf32{generator.generate(256, seed)};

    ASSERT_TRUE(implementation.validate_utf32(
              reinterpret_cast<const char32_t*>(utf32.data()), utf32.size()));
}

TEST(validate_utf32__returns_true_for_empty_string) {
  const char32_t* buf = (char32_t*)"";

  ASSERT_TRUE(implementation.validate_utf32(buf, 0));
}

TEST_LOOP(10, validate_utf32__returns_false_when_input_in_forbidden_range) {
    simdutf::tests::helpers::random_utf32 generator{seed};
    auto utf32{generator.generate(128)};
    const char32_t*  buf = reinterpret_cast<const char32_t*>(utf32.data());
    const size_t len = utf32.size();

    for (char32_t wrong_value = 0xd800; wrong_value <= 0xdfff; wrong_value++) {
      for (size_t i=0; i < utf32.size(); i++) {
        const char32_t old = utf32[i];
        utf32[i] = wrong_value;

        ASSERT_FALSE(implementation.validate_utf32(buf, len));

        utf32[i] = old;
      }
    }
}

TEST_LOOP(1000, validate_utf32__returns_false_when_input_too_large) {
    simdutf::tests::helpers::random_utf32 generator{seed};

    std::uniform_int_distribution<uint32_t> bad_range{0x110000, 0xffffffff};
    std::mt19937 gen{seed};

    auto utf32{generator.generate(128)};
    const char32_t*  buf = reinterpret_cast<const char32_t*>(utf32.data());
    const size_t len = utf32.size();

    for (size_t r = 0; r < 10; r++) {
      uint32_t wrong_value = bad_range(gen);
      for (size_t i = 0; i < utf32.size(); i++) {
        const char32_t old = utf32[i];
        utf32[i] = wrong_value;

        ASSERT_FALSE(implementation.validate_utf32(buf, len));

        utf32[i] = old;
      }
    }
}

TEST_MAIN
