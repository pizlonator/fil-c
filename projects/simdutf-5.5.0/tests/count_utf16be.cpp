#include "simdutf.h"

#include <array>
#include <random>

#include <tests/helpers/random_int.h>
#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_utf16.h>
#include <tests/helpers/test.h>


namespace {
std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

constexpr size_t trials = 10000;

using simdutf::tests::helpers::transcode_utf8_to_utf16_test_base;
} // namespace

TEST_LOOP(trials, count_just_one_word) {
    simdutf::tests::helpers::random_utf16 random(seed, 1, 0);

    for (size_t size : input_size) {
      auto generated = random.generate_counted(size);
      std::vector<char16_t> utf16be(generated.first.size());
      implementation.change_endianness_utf16(reinterpret_cast<const char16_t *>(generated.first.data()), generated.first.size(), utf16be.data());
      size_t count = implementation.count_utf16be(utf16be.data(), size);
      ASSERT_EQUAL(count, generated.second);
    }
}

TEST_LOOP(trials, count_1_or_2_UTF16_words) {
    simdutf::tests::helpers::random_utf16 random(seed, 1, 1);

    for (size_t size : input_size) {
      auto generated = random.generate_counted(size);
      std::vector<char16_t> utf16be(generated.first.size());
      implementation.change_endianness_utf16(reinterpret_cast<const char16_t *>(generated.first.data()), generated.first.size(), utf16be.data());
      size_t count = implementation.count_utf16be(utf16be.data(), size);
      ASSERT_EQUAL(count, generated.second);
    }
}

TEST_LOOP(trials, count_2_UTF16_words) {
    simdutf::tests::helpers::random_utf16 random(seed, 0, 1);

    for (size_t size : input_size) {

      auto generated = random.generate_counted(size);
      std::vector<char16_t> utf16be(generated.first.size());
      implementation.change_endianness_utf16(reinterpret_cast<const char16_t *>(generated.first.data()), generated.first.size(), utf16be.data());
      size_t count = implementation.count_utf16be(utf16be.data(), size);
      ASSERT_EQUAL(count, generated.second);
    }
}

TEST_MAIN
