#include "scalar/utf8_to_utf16/utf8_to_utf16.h"


namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
namespace utf8_to_utf16 {
using namespace simd;


  simdutf_really_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdutf_really_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }


  struct validating_transcoder {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;

    validating_transcoder() : error(uint8_t(0)) {}
    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdutf_really_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }


    template <endianness endian>
    simdutf_really_inline size_t convert(const char* in, size_t size, char16_t* utf16_output) {
      size_t pos = 0;
      char16_t* start{utf16_output};
      // In the worst case, we have the haswell kernel which can cause an overflow of
      // 8 bytes when calling convert_masked_utf8_to_utf16. If you skip the last 16 bytes,
      // and if the data is valid, then it is entirely safe because 16 UTF-8 bytes generate
      // much more than 8 bytes. However, you cannot generally assume that you have valid
      // UTF-8 input, so we are going to go back from the end counting 8 leading bytes,
      // to give us a good margin.
      size_t leading_byte = 0;
      size_t margin = size;
      for(; margin > 0 && leading_byte < 8; margin--) {
        leading_byte += (int8_t(in[margin-1]) > -65);
      }
      // If the input is long enough, then we have that margin-1 is the eight last leading byte.
      const size_t safety_margin = size - margin + 1; // to avoid overruns!
      while(pos + 64 + safety_margin <= size) {
        simd8x64<int8_t> input(reinterpret_cast<const int8_t *>(in + pos));
        if(input.is_ascii()) {
          input.store_ascii_as_utf16<endian>(utf16_output);
          utf16_output += 64;
          pos += 64;
        } else {
          // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
          static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 2) || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
              "We support either two or four chunks per 64-byte block.");
          auto zero = simd8<uint8_t>{uint8_t(0)};
          if(simd8x64<uint8_t>::NUM_CHUNKS == 2) {
            this->check_utf8_bytes(input.chunks[0], zero);
            this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          } else if(simd8x64<uint8_t>::NUM_CHUNKS == 4) {
            this->check_utf8_bytes(input.chunks[0], zero);
            this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
            this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
            this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
          }
          uint64_t utf8_continuation_mask = input.lt(-65 + 1);
          if(utf8_continuation_mask & 1) {
            return 0; // error
          }
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
            size_t consumed = convert_masked_utf8_to_utf16<endian>(in + pos,
                            utf8_end_of_code_point_mask, utf16_output);
            pos += consumed;
            utf8_end_of_code_point_mask >>= consumed;
          }
          // At this point there may remain between 0 and 12 bytes in the
          // 64-byte block. These bytes will be processed again. So we have an
          // 80% efficiency (in the worst case). In practice we expect an
          // 85% to 90% efficiency.
        }
      }
      if(errors()) { return 0; }
      if(pos < size) {
        size_t howmany  = scalar::utf8_to_utf16::convert<endian>(in + pos, size - pos, utf16_output);
        if(howmany == 0) { return 0; }
        utf16_output += howmany;
      }
      return utf16_output - start;
    }

    template <endianness endian>
    simdutf_really_inline result convert_with_errors(const char* in, size_t size, char16_t* utf16_output) {
      size_t pos = 0;
      char16_t* start{utf16_output};
      // In the worst case, we have the haswell kernel which can cause an overflow of
      // 8 bytes when calling convert_masked_utf8_to_utf16. If you skip the last 16 bytes,
      // and if the data is valid, then it is entirely safe because 16 UTF-8 bytes generate
      // much more than 8 bytes. However, you cannot generally assume that you have valid
      // UTF-8 input, so we are going to go back from the end counting 8 leading bytes,
      // to give us a good margin.
      size_t leading_byte = 0;
      size_t margin = size;
      for(; margin > 0 && leading_byte < 8; margin--) {
        leading_byte += (int8_t(in[margin-1]) > -65);
      }
      // If the input is long enough, then we have that margin-1 is the eight last leading byte.
      const size_t safety_margin = size - margin + 1; // to avoid overruns!
      while(pos + 64 + safety_margin <= size) {
        simd8x64<int8_t> input(reinterpret_cast<const int8_t *>(in + pos));
        if(input.is_ascii()) {
          input.store_ascii_as_utf16<endian>(utf16_output);
          utf16_output += 64;
          pos += 64;
        } else {
          // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
          static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 2) || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
              "We support either two or four chunks per 64-byte block.");
          auto zero = simd8<uint8_t>{uint8_t(0)};
          if(simd8x64<uint8_t>::NUM_CHUNKS == 2) {
            this->check_utf8_bytes(input.chunks[0], zero);
            this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          } else if(simd8x64<uint8_t>::NUM_CHUNKS == 4) {
            this->check_utf8_bytes(input.chunks[0], zero);
            this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
            this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
            this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
          }
          uint64_t utf8_continuation_mask = input.lt(-65 + 1);
          if (errors() || (utf8_continuation_mask & 1)) {
            // rewind_and_convert_with_errors will seek a potential error from in+pos onward,
            // with the ability to go back up to pos bytes, and read size-pos bytes forward.
            result res = scalar::utf8_to_utf16::rewind_and_convert_with_errors<endian>(pos, in + pos, size - pos, utf16_output);
            res.count += pos;
            return res;
          }
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
            size_t consumed = convert_masked_utf8_to_utf16<endian>(in + pos,
                            utf8_end_of_code_point_mask, utf16_output);
            pos += consumed;
            utf8_end_of_code_point_mask >>= consumed;
          }
          // At this point there may remain between 0 and 12 bytes in the
          // 64-byte block. These bytes will be processed again. So we have an
          // 80% efficiency (in the worst case). In practice we expect an
          // 85% to 90% efficiency.
        }
      }
      if(errors()) {
        // rewind_and_convert_with_errors will seek a potential error from in+pos onward,
        // with the ability to go back up to pos bytes, and read size-pos bytes forward.
        result res = scalar::utf8_to_utf16::rewind_and_convert_with_errors<endian>(pos, in + pos, size - pos, utf16_output);
        res.count += pos;
        return res;
      }
      if(pos < size) {
        // rewind_and_convert_with_errors will seek a potential error from in+pos onward,
        // with the ability to go back up to pos bytes, and read size-pos bytes forward.
        result res = scalar::utf8_to_utf16::rewind_and_convert_with_errors<endian>(pos, in + pos, size - pos, utf16_output);
        if (res.error) {    // In case of error, we want the error position
          res.count += pos;
          return res;
        } else {    // In case of success, we want the number of word written
          utf16_output += res.count;
        }
      }
      return result(error_code::SUCCESS, utf16_output - start);
    }

    simdutf_really_inline bool errors() const {
      return this->error.any_bits_set_anywhere();
    }

  }; // struct utf8_checker
} // utf8_to_utf16 namespace
} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf
