#pragma once

#include <cstdint>

namespace simdutf { namespace tests { namespace reference { namespace utf32 {

  // returns whether the value can be represented in the UTF-32
  bool valid_value(uint32_t value) {

    if (value > 0x10FFFF)
      return false;

    if ((value >= 0xD800) && (value <= 0xDFFF))
      return false;

    return true;
  }

  // Encodes the value in UTF-32
  // Returns 1 if the value can be encoded
  // Returns 0 if the value cannot be encoded
  template<typename CONSUMER>
  int encode(uint32_t value, CONSUMER consumer) {
    if (!valid_value(value))
      return 0;

    consumer(value);
    return 1;
  }

}}}} // namespace utf32