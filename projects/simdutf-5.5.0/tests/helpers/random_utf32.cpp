#include "random_utf32.h"

#include <stdexcept>

namespace simdutf {
namespace tests {
namespace helpers {

  std::vector<char32_t> random_utf32::generate(size_t size) {

    std::vector<char32_t> result;
    result.reserve(size);

    size_t count{0};
    for(;count < size; count++) {
      const uint32_t value = generate();
      result.push_back(value);
    }

    return result;
  }


  std::vector<char32_t> random_utf32::generate(size_t size, long seed) {
    gen.seed(seed);
    return generate(size);
  }

  uint32_t random_utf32::generate() {
    switch (range(gen)) {
      case 0:
        return first_range(gen);
      case 1:
        return second_range(gen);
      default:
        abort();
    }
  }

} // namespace helpers
} // namespace tests
} // namespace simdutf
