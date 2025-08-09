#include "scalar/utf8_to_latin1/utf8_to_latin1.h"


namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
namespace utf8_to_latin1 {
using namespace simd;


    simdutf_really_inline size_t convert_valid(const char* in, size_t size, char* latin1_output) {
      size_t pos = 0;
      char* start{latin1_output};
      // In the worst case, we have the haswell kernel which can cause an overflow of
      // 8 bytes when calling convert_masked_utf8_to_latin1. If you skip the last 16 bytes,
      // and if the data is valid, then it is entirely safe because 16 UTF-8 bytes generate
      // much more than 8 bytes. However, you cannot generally assume that you have valid
      // UTF-8 input, so we are going to go back from the end counting 8 leading bytes,
      // to give us a good margin.
      size_t leading_byte = 0;
      size_t margin = size;
      for(; margin > 0 && leading_byte < 8; margin--) {
        leading_byte += (int8_t(in[margin-1]) > -65); //twos complement of -65 is 1011 1111 ...
      }
      // If the input is long enough, then we have that margin-1 is the eight last leading byte.
      const size_t safety_margin = size - margin + 1; // to avoid overruns!
      while(pos + 64 + safety_margin <= size) {
        simd8x64<int8_t> input(reinterpret_cast<const int8_t *>(in + pos));
        if(input.is_ascii()) {
          input.store((int8_t*)latin1_output);
          latin1_output += 64;
          pos += 64;
        } else {
          // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
          uint64_t utf8_continuation_mask = input.lt(-65 + 1); // -64 is 1100 0000 in twos complement. Note: in this case, we also have ASCII to account for.
          uint64_t utf8_leading_mask = ~utf8_continuation_mask;
          uint64_t utf8_end_of_code_point_mask = utf8_leading_mask>>1;
          // We process in blocks of up to 12 bytes except possibly
          // for fast paths which may process up to 16 bytes. For the
          // slow path to work, we should have at least 12 input bytes left.
          size_t max_starting_point = (pos + 64) - 12;
          // Next loop is going to run at least five times.
          while(pos < max_starting_point) {
            // Performance note: our ability to compute 'consumed' and
            // then shift and recompute is critical. If there is a
            // latency of, say, 4 cycles on getting 'consumed', then
            // the inner loop might have a total latency of about 6 cycles.
            // Yet we process between 6 to 12 inputs bytes, thus we get
            // a speed limit between 1 cycle/byte and 0.5 cycle/byte
            // for this section of the code. Hence, there is a limit
            // to how much we can further increase this latency before
            // it seriously harms performance.
            size_t consumed = convert_masked_utf8_to_latin1(in + pos,
                            utf8_end_of_code_point_mask, latin1_output);
            pos += consumed;
            utf8_end_of_code_point_mask >>= consumed;
          }
          // At this point there may remain between 0 and 12 bytes in the
          // 64-byte block. These bytes will be processed again. So we have an
          // 80% efficiency (in the worst case). In practice we expect an
          // 85% to 90% efficiency.
        }
      }
      if(pos < size) {
        size_t howmany  = scalar::utf8_to_latin1::convert_valid(in + pos, size - pos, latin1_output);
        latin1_output += howmany;
      }
      return latin1_output - start;
    }

  }
}   // utf8_to_latin1 namespace
}   // unnamed namespace
}   // namespace SIMDUTF_IMPLEMENTATION
 // namespace simdutf
