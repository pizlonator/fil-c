#include "scalar/utf8_to_utf32/valid_utf8_to_utf32.h"

namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
namespace utf8_to_utf32 {

using namespace simd;


simdutf_warn_unused size_t convert_valid(const char* input, size_t size,
    char32_t* utf32_output) noexcept {
  size_t pos = 0;
  char32_t* start{utf32_output};
  const size_t safety_margin = 16; // to avoid overruns!
  while(pos + 64 + safety_margin <= size) {
    simd8x64<int8_t> in(reinterpret_cast<const int8_t *>(input + pos));
    if(in.is_ascii()) {
      in.store_ascii_as_utf32(utf32_output);
      utf32_output += 64;
      pos += 64;
    } else {
    // -65 is 0b10111111 in two-complement's, so largest possible continuation byte
    uint64_t utf8_continuation_mask = in.lt(-65 + 1);
    uint64_t utf8_leading_mask = ~utf8_continuation_mask;
    uint64_t utf8_end_of_code_point_mask = utf8_leading_mask>>1;
    size_t max_starting_point = (pos + 64) - 12;
    while(pos < max_starting_point) {
      size_t consumed = convert_masked_utf8_to_utf32(input + pos,
                          utf8_end_of_code_point_mask, utf32_output);
      pos += consumed;
      utf8_end_of_code_point_mask >>= consumed;
      }
    }
  }
  utf32_output += scalar::utf8_to_utf32::convert_valid(input + pos, size - pos, utf32_output);
  return utf32_output - start;
}


} // namespace utf8_to_utf32
} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf
