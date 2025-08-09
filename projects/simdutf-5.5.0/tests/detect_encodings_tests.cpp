#include "simdutf.h"

#include <array>

#include <tests/reference/encode_utf16.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/random_utf8.h>
#include <tests/helpers/random_utf16.h>
#include <tests/helpers/test.h>

namespace {
std::array<size_t, 7> input_size{8, 16, 12, 64, 68, 128, 256};

constexpr size_t trials = 10000;

template <typename Pointer>
bool has_bom(Pointer* data, size_t size) {
    return (simdutf::BOM::check_bom(data, size) != simdutf::encoding_type::unspecified);
}

template <typename RandomGenerator>
std::vector<uint8_t> generate_utf8(RandomGenerator random, size_t size) {
    // This is quite ugly workaround, but we expect almost none repeats
    while (true) {
        const auto generated = random.generate(size);
        if (!has_bom(generated.data(), generated.size())) {
            return generated;
        }
    }
}

template <typename RandomGenerator>
std::vector<char16_t> generate_utf16(RandomGenerator random, size_t size) {
    // This is quite ugly workaround, but we expect almost none repeats
    while (true) {
        const auto generated = random.generate(size);
        if (!has_bom(reinterpret_cast<const char*>(generated.data()), generated.size())) {
            return generated;
        }
    }
}

template <typename RandomGenerator>
std::vector<uint16_t> generate_u16(RandomGenerator& random, size_t count) {
    std::vector<uint16_t> result;
    result.reserve(count);

    union {
        uint16_t word[2];
        char     string[4];
    } first;

    static_assert(sizeof(first) == 4, "union got an unexpected size");

    // Make sure our random data does not start with a BOM marker.
    do  {
        first.word[0] = random();
        first.word[1] = random();
    } while (has_bom(first.string, 4));

    result.push_back(first.word[0]);
    result.push_back(first.word[1]);
    for (size_t i=2; i < count; i++) {
        result.push_back(random());
    }

    return result;
}

template <typename RandomGenerator>
std::vector<uint32_t> generate_u32(RandomGenerator& random, size_t count) {
    std::vector<uint32_t> result;
    result.reserve(count);

    union {
        uint32_t word;
        char     string[4];
    } first;

    static_assert(sizeof(first) == 4, "union got an unexpected size");

    // Make sure our random data does not start with a BOM marker.
    do  {
        first.word = random();
    } while (has_bom(first.string, 4));

    result.push_back(first.word);
    for (size_t i=1; i < count; i++) {
        result.push_back(random());
    }

    return result;
}

} // namespace


TEST(issue_519)
{
    // implementation icelake gave 0
    // implementation haswell gave 0
    // implementation westmere gave 0
    // implementation fallback gave 1
    std::vector<unsigned char> data{
        32, 32, 32, 32, 32, 32, 32, 32,  32,  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        32, 32, 32, 32, 32, 32, 32, 223, 164, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        32, 32, 32, 32, 32, 32, 32, 32,  32,  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    };
    const auto r = implementation.detect_encodings((const char *) data.data(), data.size());
    ASSERT_EQUAL(r, 1);
}

TEST(issue_516)
{
    std::vector<unsigned char> data{0x20, 0xd8, 0x00, 0x00};
    const auto r = implementation.detect_encodings((const char *) data.data(), data.size());
    ASSERT_EQUAL(r, 0);
}

TEST(boommmmm) {
  const char* utf8_bom = "\xef\xbb\xbf"; 
  const char* utf16be_bom = "\xfe\xff"; 
  const char* utf16le_bom = "\xff\xfe"; 
  ASSERT_EQUAL(implementation.detect_encodings(utf8_bom, 3), simdutf::encoding_type::UTF8);
  ASSERT_EQUAL(implementation.detect_encodings(utf16be_bom, 2), simdutf::encoding_type::UTF16_BE);
  ASSERT_EQUAL(implementation.detect_encodings(utf16le_bom, 2), simdutf::encoding_type::UTF16_LE);
}

TEST_LOOP(trials, pure_utf8_ASCII) {
    simdutf::tests::helpers::random_utf8 random(seed, 1, 0, 0, 0);

    for (size_t size : input_size) {
      const auto generated = generate_utf8(random, size);
      auto expected = simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_EQUAL(actual, expected);
    }
}

TEST_LOOP(trials, pure_utf16_ASCII) {
    simdutf::tests::helpers::RandomInt random(0, 127, seed);

    for (size_t size : input_size) {
      const auto generated = generate_u16(random, size/2);
      auto expected = simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF8 and UTF16_LE.
    }
}

TEST_LOOP(trials, pure_utf32_ASCII) {
    simdutf::tests::helpers::RandomInt random(0,0x7f, seed);

    for (size_t size : input_size) {
      const auto generated = generate_u32(random, size/4);
      auto expected = simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE | simdutf::encoding_type::UTF32_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF8 and UTF16_LE and UTF32_LE.
    }
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST_LOOP(trials, no_utf8_bytes_no_surrogates) {
    simdutf::tests::helpers::RandomIntRanges random({{0x007f, 0xd800-1},
                                                     {0xe000, 0xffff}}, seed);

    for (size_t size : input_size) {
      const auto generated = generate_u32(random, size/4);
      auto expected = simdutf::encoding_type::UTF16_LE | simdutf::encoding_type::UTF32_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_EQUAL((actual & expected), expected); // Must be at least UTF16_LE and UTF32_LE.
    }
}
#endif

TEST_LOOP(trials, two_utf8_bytes) {
    simdutf::tests::helpers::random_utf8 random(seed, 0, 1, 0, 0);

    for (size_t size : input_size) {
      const auto generated = generate_utf8(random, size);
      auto expected = simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      if (actual != expected) {
        if ((actual & simdutf::encoding_type::UTF8) == 0) {
          puts("failed to detect valid UTF-8.");
        }
        if ((actual & simdutf::encoding_type::UTF16_LE) == 0) {
          puts("failed to detect valid UTF-16LE.");
        }
      }
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF8 and UTF16_LE.
    }
}

TEST_LOOP(trials, utf_16_surrogates) {
    simdutf::tests::helpers::random_utf16 random(seed, 0, 1);

    for (size_t size : input_size) {
      const auto generated = generate_utf16(random, size/2);
      auto expected = simdutf::encoding_type::UTF16_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF16_LE.
    }
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST_LOOP(trials, utf32_surrogates) {
    simdutf::tests::helpers::RandomInt random_prefix(0x10000, 0x10ffff, seed);
    simdutf::tests::helpers::RandomInt random_suffix(0xd800, 0xdfff, seed);

    for (size_t size : input_size) {
      std::vector<uint32_t> generated;
      for (int i = 0; i < size/4; i++) {
        generated.push_back((random_prefix() & 0xffff0000) + random_suffix());
      }
      auto expected = simdutf::encoding_type::UTF32_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF32_LE.
    }
}
#endif


#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST_LOOP(trials, edge_surrogate) {
    simdutf::tests::helpers::RandomInt random(0x10000, 0x10ffff, seed);

    const size_t size = 512;
    std::vector<uint16_t> generated(size/2,0);
    int i = 31;
    while (i + 32 < (size/2) - 1) {
      char16_t W1;
      char16_t W2;
      ASSERT_EQUAL(simdutf::tests::reference::utf16::encode(random(), W1, W2), 2);
      generated[i] = W1;
      generated[i+1] = W2;
      i += 32;
    }
    auto expected = simdutf::encoding_type::UTF16_LE;
    auto actual = implementation.detect_encodings(
                    reinterpret_cast<const char *>(generated.data()),
                    size);
    ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF16_LE.
}
#endif

TEST_LOOP(trials, tail_utf8) {
    simdutf::tests::helpers::random_utf8 random(seed, 0, 0, 1, 0);
    std::array<size_t, 5> multiples_three{12, 54, 66, 126, 252};
    for (size_t size : multiples_three) {
      const auto generated = generate_utf8(random, size);
      auto expected = simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE;
      auto actual = implementation.detect_encodings(
                      reinterpret_cast<const char *>(generated.data()),
                      size);
      ASSERT_TRUE((actual & expected) == expected); // Must be at least UTF8 and UTF16_LE.
    }
}

TEST_MAIN
