#include "random_utf16.h"
#include "simdutf.h"
#include "../reference/encode_utf16.h"

#include <stdexcept>

namespace simdutf {
namespace tests {
namespace helpers {

  std::vector<char16_t> random_utf16::generate(size_t size) {
    return generate_counted(size).first;
  }

  std::pair<std::vector<char16_t>,size_t> random_utf16::generate_counted(size_t size) {
    std::vector<char16_t> result;
    result.reserve(size);

    char16_t W1;
    char16_t W2;
    size_t count{0};
    while (result.size() < size) {
      count++;
      const uint32_t value = generate();
      switch (simdutf::tests::reference::utf16::encode(value, W1, W2)) {
        case 0:
          throw std::runtime_error("Random UTF-16 generator is broken");
        case 1:
          result.push_back(W1);
          break;
        case 2:
          result.push_back(W1);
          result.push_back(W2);
          break;
      }
    }
#ifndef SIMDUTF_IS_BIG_ENDIAN
#error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif
#if SIMDUTF_IS_BIG_ENDIAN
    change_endianness_utf16(result.data(), result.size(), result.data());
#endif
    return make_pair(result,count);
  }


  std::vector<char16_t> random_utf16::generate(size_t size, long seed) {
    gen.seed(seed);
    return generate(size);
  }

  uint32_t random_utf16::generate() {
    switch (utf16_length(gen)) {
      case 0:
        return single_word0(gen);
      case 1:
        return single_word1(gen);
      case 2:
        return two_words(gen);
      default:
        abort();
    }
  }

} // namespace helpers
} // namespace tests
} // namespace simdutf
