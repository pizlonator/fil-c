#include "simdutf.h"

#ifndef SIMDUTF_IS_BIG_ENDIAN
#error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif

#include <array>
#include <fstream>
#include <memory>

#include <tests/helpers/random_utf16.h>
#include <tests/helpers/test.h>

constexpr size_t trials = 1000;

TEST_LOOP(trials, validate_utf16le_with_errors__returns_success_for_valid_input__single_words) {
    simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
    const auto utf16{generator.generate(512, seed)};

    simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(utf16.data()), utf16.size());

    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    ASSERT_EQUAL(res.count, utf16.size());
}

TEST_LOOP(trials, validate_utf16le_with_errors__returns_success_for_valid_input__surrogate_pairs_short) {
    simdutf::tests::helpers::random_utf16 generator{seed, 0, 1};
    const auto utf16{generator.generate(8)};

    simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(utf16.data()), utf16.size());

    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    ASSERT_EQUAL(res.count, utf16.size());
}


TEST_LOOP(trials, validate_utf16le_with_errors__returns_success_for_valid_input__surrogate_pairs) {
    simdutf::tests::helpers::random_utf16 generator{seed, 0, 1};
    const auto utf16{generator.generate(512)};

    simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(utf16.data()), utf16.size());

    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    ASSERT_EQUAL(res.count, utf16.size());
}

TEST(provoke_integer_wraparound_in_icelake)
{
    // this is to prove signed integer wraparound in the icelake implementation
    unsigned char cleaned_crash[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
    unsigned int cleaned_crash_len = 62;
    assert(reinterpret_cast<std::uintptr_t>(cleaned_crash) % alignof(char16_t) == 0);

    const auto size = cleaned_crash_len / sizeof(char16_t);

    auto r = simdutf::validate_utf16le_with_errors((const char16_t *) cleaned_crash, size);
    assert(r.error == simdutf::error_code::SUCCESS);
}

// mixed = either 16-bit or 32-bit codewords
TEST(validate_utf16le_with_errors__returns_success_for_valid_input__mixed) {
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 1};
  const auto utf16{generator.generate(512)};

  simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(utf16.data()), utf16.size());

  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, utf16.size());
}

TEST(validate_utf16le_with_errors__returns_success_for_empty_string) {
  const char16_t* buf = (char16_t*)"";

  simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(buf), 0);

  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, 0);
}

// The first word must not be in range [0xDC00 .. 0xDFFF]
/*
2.2 Decoding UTF-16

   [...]

   1) If W1 < 0xD800 or W1 > 0xDFFF, the character value U is the value
      of W1. Terminate.

   2) Determine if W1 is between 0xD800 and 0xDBFF. If not, the sequence
      is in error [...]
*/
#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST_LOOP(10, validate_utf16le_with_errors__returns_error_when_input_has_wrong_first_word_value) {
    simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};

    auto utf16{generator.generate(128)};
    const char16_t*  buf = reinterpret_cast<const char16_t*>(utf16.data());
    const size_t len = utf16.size();

    for (char16_t wrong_value = 0xdc00; wrong_value <= 0xdfff; wrong_value++) {
      for (size_t i=0; i < utf16.size(); i++) {
        const char16_t old = utf16[i];
        utf16[i] = wrong_value;

        simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(buf), len);

        ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
        ASSERT_EQUAL(res.count, i);

        utf16[i] = old;
      }
    }
}
#endif

/*
 RFC-2781:

 3) [..] if W2 is not between 0xDC00 and 0xDFFF, the sequence is in error.
    Terminate.
*/
#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST(validate_utf16le_with_errors__returns_error_when_input_has_wrong_second_word_value) {
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  auto utf16{generator.generate(128)};
  const char16_t*  buf = reinterpret_cast<const char16_t*>(utf16.data());
  const size_t len = utf16.size();

  const std::array<char16_t, 5> sample_wrong_second_word{
    0x0000, 0x1000, 0xdbff, 0xe000, 0xffff
  };

  const char16_t valid_surrogate_W1 = 0xd800;
  for (char16_t W2: sample_wrong_second_word) {
    for (size_t i=0; i < utf16.size() - 1; i++) {
      const char16_t old_W1 = utf16[i + 0];
      const char16_t old_W2 = utf16[i + 1];

      utf16[i + 0] = valid_surrogate_W1;
      utf16[i + 1] = W2;

      simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(buf), len);

      ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
      ASSERT_EQUAL(res.count, i);

      utf16[i + 0] = old_W1;
      utf16[i + 1] = old_W2;
    }
  }
}
#endif

/*
 RFC-2781:

 3) If there is no W2 (that is, the sequence ends with W1) [...]
    the sequence is in error. Terminate.
*/
#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST(validate_utf16le_with_errors__returns_error_when_input_is_truncated) {
  const char16_t valid_surrogate_W1 = 0xd800;
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  for (size_t size = 1; size < 128; size++) {
    auto utf16{generator.generate(128)};
    const char16_t*  buf = reinterpret_cast<const char16_t*>(utf16.data());
    const size_t len = utf16.size();

    utf16[size - 1] = valid_surrogate_W1;

    simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(buf), len);

    ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
    ASSERT_EQUAL(res.count, size - 1);
  }
}
#endif

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST(validate_utf16le_with_errors__extensive_tests) {
#ifdef RUN_IN_SPIKE_SIMULATOR
  printf("skipping, cannot be run under Spike");
  return;
#endif
  const std::string path{"validate_utf16_testcases.txt"};
  std::ifstream file{path};
  if (not file) {
    printf("File '%s' cannot be open, skipping test\n", path.c_str());
    return;
  }

  constexpr uint16_t V = 0xfaea;
  constexpr uint16_t L = 0xd852;
  constexpr uint16_t H = 0xde12;

  constexpr size_t len = 32;
  char16_t buf[len];

  long lineno = 0;
  while (file) {
    std::string line;
    std::getline(file, line);
    lineno += 1;
    if (line.empty() or line[0] == '#')
      continue;

    // format: [TF][VLH]{16}
    simdutf::error_code valid = simdutf::error_code::SURROGATE;
    switch (line[0]) {
      case 'T':
        valid = simdutf::error_code::SUCCESS;
        break;
      case 'F':
        valid = simdutf::error_code::SURROGATE;
        break;
      default:
        throw std::invalid_argument("Error at line #" + std::to_string(lineno) +
                                    ": the first character must be either 'T' or 'F'");
    }

    // prepare input
    for (size_t i = 0; i < len; i++) {
      buf[i] = V;
    }

    for (size_t i = 1; i < line.size(); i++) {
      switch (line[i]) {
        case 'L':
          buf[i - 1] = L;
          break;
        case 'H':
          buf[i - 1] = H;
          break;
        case 'V':
          buf[i - 1] = V;
          break;
        default:
          throw std::invalid_argument("Error at line #" + std::to_string(lineno) +
                                      ": allowed characters are 'L', 'H' and 'V'");
      }
    }

    // check
    simdutf::result res = implementation.validate_utf16le_with_errors(reinterpret_cast<const char16_t*>(buf), len);

    ASSERT_EQUAL(res.error, valid);
  }
}
#endif

TEST_MAIN
