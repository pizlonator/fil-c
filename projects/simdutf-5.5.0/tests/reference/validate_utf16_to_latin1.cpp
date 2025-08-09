#include "validate_utf16.h"

#ifndef SIMDUTF_IS_BIG_ENDIAN
#error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif

namespace simdutf {
namespace tests {
namespace reference {

simdutf_warn_unused bool validate_utf16_to_latin1(const char16_t *buf, size_t len) noexcept {
  const char16_t* curr = buf;
  const char16_t* end = buf + len;

  while (curr != end) {
      #if SIMDUTF_IS_BIG_ENDIAN 
            // By convention, we always take as an input an UTF-16LE.
            const uint16_t W1 = uint16_t((uint16_t(*curr) << 8) | (uint16_t(*curr) >> 8));
      #else
            const uint16_t W1 = *curr;
      #endif

     curr += 1;
     if ( 0xff < W1 ) {
        return false;
      }
  }
  return true;
}

} // namespace reference
} // namespace tests
} // namespace simdutf


