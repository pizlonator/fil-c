#pragma once

#include <cstdint>

namespace simdutf { namespace tests { namespace reference { namespace latin1 {

  // returns whether the value can be represented in the latin1
  bool valid_value(uint32_t value) {
    if (value > 0xFF) { return false; }
    return true; // Each possible combination in a bit represent a latin1 value
  }

  // Encodes the value in UTF-32
  // Returns 1 if the value can be encoded
  // Returns 0 if the value cannot be encoded
  template<typename CONSUMER>
  int encode(uint8_t value, CONSUMER consumer) {
    if (!valid_value(value)) {
      return 0;
    } 
    consumer(value);
    return 1;
  }

}}}} // namespace latin