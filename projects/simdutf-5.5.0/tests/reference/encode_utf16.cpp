#include "encode_utf16.h"

namespace simdutf { namespace tests { namespace reference { namespace utf16 {

  // returns whether the value can be represented in the UTF-16
  bool valid_value(uint32_t value) {
    /*
      RFC-2781 (2. UTF-16 definition):

      Characters with values greater than 0x10FFFF cannot be encoded in UTF-16.
    */
    if (value > 0x10FFFF)
      return false;

    /*
      RFC-2781 (2. UTF-16 definition):

      Note: Values between 0xD800 and 0xDFFF are specifically reserved for
      use with UTF-16, and don't have any characters assigned to them.
    */
    if ((value >= 0xD800) && (value <= 0xDFFF))
      return false;

    return true;
  }

  // Encodes the value using either one or two code units (returns 1 or 2 respectively)
  // Returns 0 if the value cannot be encoded
  int encode(uint32_t value, char16_t& W1, char16_t& W2) {
    if (!valid_value(value))
      return 0;

    if (value <= 0xffff) {
      W1 = char16_t(value);
      return 1;
    } else {
      value -= 0x10000;
      W1 = char16_t(0xd800 | ((value >> 10) & 0x03ff));
      W2 = char16_t(0xdc00 | (value & 0x03ff));
      return 2;
    }
  }
}}}} // namespace utf16
