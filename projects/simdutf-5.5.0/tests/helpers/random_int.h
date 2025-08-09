#pragma once

#include <random>
#include <vector>
#include <initializer_list>
#include <utility>
#include <cstdint>

namespace simdutf { namespace tests { namespace helpers {

  class RandomInt {
    std::mt19937 gen;
    std::uniform_int_distribution<uint64_t> distr;

  public:
    RandomInt(uint64_t lo, uint64_t hi, uint64_t seed) noexcept;

    uint32_t operator()() noexcept;
  };

  class RandomIntRanges {
    std::mt19937 gen;
    using Distribution = std::uniform_int_distribution<uint64_t>;

    Distribution range_index;
    std::vector<Distribution> ranges;

  public:
    RandomIntRanges(std::initializer_list<std::pair<uint64_t, uint64_t>> ranges, uint64_t seed) noexcept;

    uint32_t operator()() noexcept;
  };

}}}
