#include "random_int.h"

#include <stdexcept>

namespace simdutf { namespace tests { namespace helpers {

    RandomInt::RandomInt(uint64_t lo, uint64_t hi, uint64_t seed) noexcept
      : gen(std::mt19937::result_type(seed)), distr{lo, hi} {}

    uint32_t RandomInt::operator()() noexcept {
      return uint32_t(distr(gen));
    }

    RandomIntRanges::RandomIntRanges(std::initializer_list<std::pair<uint64_t, uint64_t>> ranges_, uint64_t seed) noexcept
      : gen(std::mt19937::result_type(seed)) {

      for (const auto& lohi: ranges_) {
        ranges.emplace_back(lohi.first, lohi.second);
      }

      range_index = Distribution(0, ranges.size() - 1);
    }

    uint32_t RandomIntRanges::operator()() noexcept {
      const size_t index = size_t(range_index(gen));
      return uint32_t(ranges[index](gen));
    }
}}}
