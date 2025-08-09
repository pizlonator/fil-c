#include "validate_utf8_to_latin1.h"

namespace simdutf {
namespace tests {
namespace reference {

// credit: based on code from Google Fuchsia (Apache Licensed)
simdutf_warn_unused bool validate_utf8_to_latin1(const char *buf, size_t len) noexcept {
  const uint8_t *data = (const uint8_t *)buf;
  uint64_t pos = 0;
  uint64_t next_pos = 0;
  uint32_t code_point = 0;
  while (pos < len) {
    unsigned char byte = data[pos];
    if (byte < 0b10000000) { // ASCII
      pos++;
      continue;
    } else if ((byte & 0b11100000) == 0b11000000) { // two bytes
      next_pos = pos + 2;
      if (next_pos > len) { return false; } // EOF thus no continuation byte :(
      if ((data[pos + 1] & 0b11000000) != 0b10000000) { // No continuation byte
        return false;
      }
      // range check
      code_point = (byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
      if (code_point < 0x80 || 0xff < code_point) { return false; }
    } else {
      // we may have a continuation
      return false;
    } 
    pos = next_pos;
  }
  return true;
}

} // namespace reference
} // namespace tests
} // namespace simdutf

