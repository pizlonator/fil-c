#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

namespace simdutf {
namespace tests {
namespace helpers {

  constexpr int32_t number_code_points = 0x0010ffff - (0xdfff - 0xd800);
  constexpr int32_t length_first_range = 0x0000d7ff;
  constexpr int32_t length_second_range = 0x0010ffff - 0x0000e000;
  /*
    Generates valid random UTF-32
  */
  class random_utf32 {
    std::mt19937 gen;

  public:
    random_utf32(uint32_t seed)
      : gen{seed},
        range({double(length_first_range) / double(number_code_points), double(length_second_range) / double(number_code_points)}) {}
    // Uniformly randomize over the two ranges

    std::vector<char32_t> generate(size_t size);
    std::vector<char32_t> generate(size_t size, long seed);
  private:
    std::discrete_distribution<> range;
    std::uniform_int_distribution<uint32_t> first_range{0x00000000, 0x0000d7ff};
    std::uniform_int_distribution<uint32_t> second_range{0x0000e000, 0x0010ffff};
    uint32_t generate();
  };

} // namespace helpers
} // namespace tests
} // namespace simdutf
