/*
    The vectorized algorithm works on single SSE register i.e., it
    loads eight 16-bit code units.

    We consider three cases:
    1. an input register contains no surrogates and each value
       is in range 0x0000 .. 0x07ff.
    2. an input register contains no surrogates and values are
       is in range 0x0000 .. 0xffff.
    3. an input register contains surrogates --- i.e. codepoints
       can have 16 or 32 bits.

    Ad 1.

    When values are less than 0x0800, it means that a 16-bit code unit
    can be converted into: 1) single UTF8 byte (when it is an ASCII
    char) or 2) two UTF8 bytes.

    For this case we do only some shuffle to obtain these 2-byte
    codes and finally compress the whole SSE register with a single
    shuffle.

    We need 256-entry lookup table to get a compression pattern
    and the number of output bytes in the compressed vector register.
    Each entry occupies 17 bytes.

    Ad 2.

    When values fit in 16-bit code units, but are above 0x07ff, then
    a single word may produce one, two or three UTF8 bytes.

    We prepare data for all these three cases in two registers.
    The first register contains lower two UTF8 bytes (used in all
    cases), while the second one contains just the third byte for
    the three-UTF8-bytes case.

    Finally these two registers are interleaved forming eight-element
    array of 32-bit values. The array spans two SSE registers.
    The bytes from the registers are compressed using two shuffles.

    We need 256-entry lookup table to get a compression pattern
    and the number of output bytes in the compressed vector register.
    Each entry occupies 17 bytes.


    To summarize:
    - We need two 256-entry tables that have 8704 bytes in total.
*/
/*
  Returns a pair: the first unprocessed byte from buf and utf8_output
  A scalar routing should carry on the conversion of the tail.
*/
template <endianness big_endian>
std::pair<const char16_t*, char*> arm_convert_utf16_to_utf8(const char16_t* buf, size_t len, char* utf8_out) {
  uint8_t * utf8_output = reinterpret_cast<uint8_t*>(utf8_out);
  const char16_t* end = buf + len;

  const uint16x8_t v_f800 = vmovq_n_u16((uint16_t)0xf800);
  const uint16x8_t v_d800 = vmovq_n_u16((uint16_t)0xd800);
  const uint16x8_t v_c080 = vmovq_n_u16((uint16_t)0xc080);
  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92
  while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
    uint16x8_t in = vld1q_u16(reinterpret_cast<const uint16_t *>(buf));
    if (!match_system(big_endian)) { in = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(in))); }
    if(vmaxvq_u16(in) <= 0x7F) { // ASCII fast path!!!!
        // It is common enough that we have sequences of 16 consecutive ASCII characters.
        uint16x8_t nextin = vld1q_u16(reinterpret_cast<const uint16_t *>(buf) + 8);
        if (!match_system(big_endian)) { nextin = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(nextin))); }
        if(vmaxvq_u16(nextin) > 0x7F) {
          // 1. pack the bytes
          // obviously suboptimal.
          uint8x8_t utf8_packed = vmovn_u16(in);
          // 2. store (8 bytes)
          vst1_u8(utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 8;
          utf8_output += 8;
          in = nextin;
        } else {
          // 1. pack the bytes
          // obviously suboptimal.
          uint8x16_t utf8_packed = vmovn_high_u16(vmovn_u16(in), nextin);
          // 2. store (16 bytes)
          vst1q_u8(utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 16;
          utf8_output += 16;
          continue; // we are done for this round!
        }
    }

    if (vmaxvq_u16(in) <= 0x7FF) {
          // 1. prepare 2-byte values
          // input 16-bit word : [0000|0aaa|aabb|bbbb] x 8
          // expected output   : [110a|aaaa|10bb|bbbb] x 8
          const uint16x8_t v_1f00 = vmovq_n_u16((int16_t)0x1f00);
          const uint16x8_t v_003f = vmovq_n_u16((int16_t)0x003f);

          // t0 = [000a|aaaa|bbbb|bb00]
          const uint16x8_t t0 = vshlq_n_u16(in, 2);
          // t1 = [000a|aaaa|0000|0000]
          const uint16x8_t t1 = vandq_u16(t0, v_1f00);
          // t2 = [0000|0000|00bb|bbbb]
          const uint16x8_t t2 = vandq_u16(in, v_003f);
          // t3 = [000a|aaaa|00bb|bbbb]
          const uint16x8_t t3 = vorrq_u16(t1, t2);
          // t4 = [110a|aaaa|10bb|bbbb]
          const uint16x8_t t4 = vorrq_u16(t3, v_c080);
          // 2. merge ASCII and 2-byte codewords
          const uint16x8_t v_007f = vmovq_n_u16((uint16_t)0x007F);
          const uint16x8_t one_byte_bytemask = vcleq_u16(in, v_007f);
          const uint8x16_t utf8_unpacked = vreinterpretq_u8_u16(vbslq_u16(one_byte_bytemask, in, t4));
          // 3. prepare bitmask for 8-bit lookup
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
          const uint16x8_t mask = simdutf_make_uint16x8_t(0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0002, 0x0008,
                                    0x0020, 0x0080);
#else
          const uint16x8_t mask = { 0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0002, 0x0008,
                                    0x0020, 0x0080 };
#endif
          uint16_t m2 = vaddvq_u16(vandq_u16(one_byte_bytemask, mask));
          // 4. pack the bytes
          const uint8_t* row = &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[m2][0];
          const uint8x16_t shuffle = vld1q_u8(row + 1);
          const uint8x16_t utf8_packed = vqtbl1q_u8(utf8_unpacked, shuffle);

          // 5. store bytes
          vst1q_u8(utf8_output, utf8_packed);

          // 6. adjust pointers
          buf += 8;
          utf8_output += row[0];
          continue;

    }
    const uint16x8_t surrogates_bytemask = vceqq_u16(vandq_u16(in, v_f800), v_d800);
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (vmaxvq_u16(surrogates_bytemask) == 0) {
        // case: code units from register produce either 1, 2 or 3 UTF-8 bytes
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
        const uint16x8_t dup_even = simdutf_make_uint16x8_t(0x0000, 0x0202, 0x0404, 0x0606,
                                     0x0808, 0x0a0a, 0x0c0c, 0x0e0e);
#else
        const uint16x8_t dup_even = {0x0000, 0x0202, 0x0404, 0x0606,
                                     0x0808, 0x0a0a, 0x0c0c, 0x0e0e};
#endif
        /* In this branch we handle three cases:
           1. [0000|0000|0ccc|cccc] => [0ccc|cccc]                           - single UFT-8 byte
           2. [0000|0bbb|bbcc|cccc] => [110b|bbbb], [10cc|cccc]              - two UTF-8 bytes
           3. [aaaa|bbbb|bbcc|cccc] => [1110|aaaa], [10bb|bbbb], [10cc|cccc] - three UTF-8 bytes

          We expand the input word (16-bit) into two code units (32-bit), thus
          we have room for four bytes. However, we need five distinct bit
          layouts. Note that the last byte in cases #2 and #3 is the same.

          We precompute byte 1 for case #1 and the common byte for cases #2 & #3
          in register t2.

          We precompute byte 1 for case #3 and -- **conditionally** -- precompute
          either byte 1 for case #2 or byte 2 for case #3. Note that they
          differ by exactly one bit.

          Finally from these two code units we build proper UTF-8 sequence, taking
          into account the case (i.e, the number of bytes to write).
        */
        /**
         * Given [aaaa|bbbb|bbcc|cccc] our goal is to produce:
         * t2 => [0ccc|cccc] [10cc|cccc]
         * s4 => [1110|aaaa] ([110b|bbbb] OR [10bb|bbbb])
         */
#define simdutf_vec(x) vmovq_n_u16(static_cast<uint16_t>(x))
        // [aaaa|bbbb|bbcc|cccc] => [bbcc|cccc|bbcc|cccc]
        const uint16x8_t t0 = vreinterpretq_u16_u8(vqtbl1q_u8(vreinterpretq_u8_u16(in), vreinterpretq_u8_u16(dup_even)));
        // [bbcc|cccc|bbcc|cccc] => [00cc|cccc|0bcc|cccc]
        const uint16x8_t t1 = vandq_u16(t0, simdutf_vec(0b0011111101111111));
        // [00cc|cccc|0bcc|cccc] => [10cc|cccc|0bcc|cccc]
        const uint16x8_t t2 = vorrq_u16 (t1, simdutf_vec(0b1000000000000000));

        // s0: [aaaa|bbbb|bbcc|cccc] => [0000|0000|0000|aaaa]
        const uint16x8_t s0 = vshrq_n_u16(in, 12);
        // s1: [aaaa|bbbb|bbcc|cccc] => [0000|bbbb|bb00|0000]
        const uint16x8_t s1 = vandq_u16(in, simdutf_vec(0b0000111111000000));
        // [0000|bbbb|bb00|0000] => [00bb|bbbb|0000|0000]
        const uint16x8_t s1s = vshlq_n_u16(s1, 2);
        // [00bb|bbbb|0000|aaaa]
        const uint16x8_t s2 = vorrq_u16(s0, s1s);
        // s3: [00bb|bbbb|0000|aaaa] => [11bb|bbbb|1110|aaaa]
        const uint16x8_t s3 = vorrq_u16(s2, simdutf_vec(0b1100000011100000));
        const uint16x8_t v_07ff = vmovq_n_u16((uint16_t)0x07FF);
        const uint16x8_t one_or_two_bytes_bytemask = vcleq_u16(in, v_07ff);
        const uint16x8_t m0 = vbicq_u16(simdutf_vec(0b0100000000000000), one_or_two_bytes_bytemask);
        const uint16x8_t s4 = veorq_u16(s3, m0);
#undef simdutf_vec

        // 4. expand code units 16-bit => 32-bit
        const uint8x16_t out0 = vreinterpretq_u8_u16(vzip1q_u16(t2, s4));
        const uint8x16_t out1 = vreinterpretq_u8_u16(vzip2q_u16(t2, s4));

        // 5. compress 32-bit code units into 1, 2 or 3 bytes -- 2 x shuffle
        const uint16x8_t v_007f = vmovq_n_u16((uint16_t)0x007F);
        const uint16x8_t one_byte_bytemask = vcleq_u16(in, v_007f);
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
        const uint16x8_t onemask = simdutf_make_uint16x8_t(0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0100, 0x0400,
                                    0x1000, 0x4000 );
        const uint16x8_t twomask = simdutf_make_uint16x8_t(0x0002, 0x0008,
                                    0x0020, 0x0080,
                                    0x0200, 0x0800,
                                    0x2000, 0x8000 );
#else
        const uint16x8_t onemask = { 0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0100, 0x0400,
                                    0x1000, 0x4000 };
        const uint16x8_t twomask = { 0x0002, 0x0008,
                                    0x0020, 0x0080,
                                    0x0200, 0x0800,
                                    0x2000, 0x8000 };
#endif
        const uint16x8_t combined = vorrq_u16(vandq_u16(one_byte_bytemask, onemask), vandq_u16(one_or_two_bytes_bytemask, twomask));
        const uint16_t mask = vaddvq_u16(combined);
        // The following fast path may or may not be beneficial.
        /*if(mask == 0) {
          // We only have three-byte code units. Use fast path.
          const uint8x16_t shuffle = {2,3,1,6,7,5,10,11,9,14,15,13,0,0,0,0};
          const uint8x16_t utf8_0 = vqtbl1q_u8(out0, shuffle);
          const uint8x16_t utf8_1 = vqtbl1q_u8(out1, shuffle);
          vst1q_u8(utf8_output, utf8_0);
          utf8_output += 12;
          vst1q_u8(utf8_output, utf8_1);
          utf8_output += 12;
          buf += 8;
          continue;
        }*/
        const uint8_t mask0 = uint8_t(mask);

        const uint8_t* row0 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask0][0];
        const uint8x16_t shuffle0 = vld1q_u8(row0 + 1);
        const uint8x16_t utf8_0 = vqtbl1q_u8(out0, shuffle0);

        const uint8_t mask1 = static_cast<uint8_t>(mask >> 8);
        const uint8_t* row1 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask1][0];
        const uint8x16_t shuffle1 = vld1q_u8(row1 + 1);
        const uint8x16_t utf8_1 = vqtbl1q_u8(out1, shuffle1);

        vst1q_u8(utf8_output, utf8_0);
        utf8_output += row0[0];
        vst1q_u8(utf8_output, utf8_1);
        utf8_output += row1[0];

        buf += 8;
    // surrogate pair(s) in a register
    } else {
      // Let us do a scalar fallback.
      // It may seem wasteful to use scalar code, but being efficient with SIMD
      // in the presence of surrogate pairs may require non-trivial tables.
      size_t forward = 15;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint16_t word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if((word & 0xFF80)==0) {
          *utf8_output++ = char(word);
        } else if((word & 0xF800)==0) {
          *utf8_output++ = char((word>>6) | 0b11000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else if((word &0xF800 ) != 0xD800) {
          *utf8_output++ = char((word>>12) | 0b11100000);
          *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k + 1]) : buf[k + 1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(nullptr, reinterpret_cast<char*>(utf8_output)); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf8_output++ = char((value>>18) | 0b11110000);
          *utf8_output++ = char(((value>>12) & 0b111111) | 0b10000000);
          *utf8_output++ = char(((value>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((value & 0b111111) | 0b10000000);
        }
      }
      buf += k;
    }
  } // while

  return std::make_pair(buf, reinterpret_cast<char*>(utf8_output));
}


/*
  Returns a pair: a result struct and utf8_output.
  If there is an error, the count field of the result is the position of the error.
  Otherwise, it is the position of the first unprocessed byte in buf (even if finished).
  A scalar routing should carry on the conversion of the tail if needed.
*/
template <endianness big_endian>
std::pair<result, char*> arm_convert_utf16_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_out) {
  uint8_t * utf8_output = reinterpret_cast<uint8_t*>(utf8_out);
    const char16_t* start = buf;
  const char16_t* end = buf + len;

  const uint16x8_t v_f800 = vmovq_n_u16((uint16_t)0xf800);
  const uint16x8_t v_d800 = vmovq_n_u16((uint16_t)0xd800);
  const uint16x8_t v_c080 = vmovq_n_u16((uint16_t)0xc080);
  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92

  while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
    uint16x8_t in = vld1q_u16(reinterpret_cast<const uint16_t *>(buf));
    if (!match_system(big_endian)) { in = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(in))); }
    if(vmaxvq_u16(in) <= 0x7F) { // ASCII fast path!!!!
        // It is common enough that we have sequences of 16 consecutive ASCII characters.
        uint16x8_t nextin = vld1q_u16(reinterpret_cast<const uint16_t *>(buf) + 8);
        if (!match_system(big_endian)) { nextin = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(nextin))); }
        if(vmaxvq_u16(nextin) > 0x7F) {
          // 1. pack the bytes
          // obviously suboptimal.
          uint8x8_t utf8_packed = vmovn_u16(in);
          // 2. store (8 bytes)
          vst1_u8(utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 8;
          utf8_output += 8;
          in = nextin;
        } else {
          // 1. pack the bytes
          // obviously suboptimal.
          uint8x16_t utf8_packed = vmovn_high_u16(vmovn_u16(in), nextin);
          // 2. store (16 bytes)
          vst1q_u8(utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 16;
          utf8_output += 16;
          continue; // we are done for this round!
        }
    }

    if (vmaxvq_u16(in) <= 0x7FF) {
          // 1. prepare 2-byte values
          // input 16-bit word : [0000|0aaa|aabb|bbbb] x 8
          // expected output   : [110a|aaaa|10bb|bbbb] x 8
          const uint16x8_t v_1f00 = vmovq_n_u16((int16_t)0x1f00);
          const uint16x8_t v_003f = vmovq_n_u16((int16_t)0x003f);

          // t0 = [000a|aaaa|bbbb|bb00]
          const uint16x8_t t0 = vshlq_n_u16(in, 2);
          // t1 = [000a|aaaa|0000|0000]
          const uint16x8_t t1 = vandq_u16(t0, v_1f00);
          // t2 = [0000|0000|00bb|bbbb]
          const uint16x8_t t2 = vandq_u16(in, v_003f);
          // t3 = [000a|aaaa|00bb|bbbb]
          const uint16x8_t t3 = vorrq_u16(t1, t2);
          // t4 = [110a|aaaa|10bb|bbbb]
          const uint16x8_t t4 = vorrq_u16(t3, v_c080);
          // 2. merge ASCII and 2-byte codewords
          const uint16x8_t v_007f = vmovq_n_u16((uint16_t)0x007F);
          const uint16x8_t one_byte_bytemask = vcleq_u16(in, v_007f);
          const uint8x16_t utf8_unpacked = vreinterpretq_u8_u16(vbslq_u16(one_byte_bytemask, in, t4));
          // 3. prepare bitmask for 8-bit lookup
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
          const uint16x8_t mask = simdutf_make_uint16x8_t(0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0002, 0x0008,
                                    0x0020, 0x0080);
#else
          const uint16x8_t mask = { 0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0002, 0x0008,
                                    0x0020, 0x0080 };
#endif
          uint16_t m2 = vaddvq_u16(vandq_u16(one_byte_bytemask, mask));
          // 4. pack the bytes
          const uint8_t* row = &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[m2][0];
          const uint8x16_t shuffle = vld1q_u8(row + 1);
          const uint8x16_t utf8_packed = vqtbl1q_u8(utf8_unpacked, shuffle);

          // 5. store bytes
          vst1q_u8(utf8_output, utf8_packed);

          // 6. adjust pointers
          buf += 8;
          utf8_output += row[0];
          continue;

    }
    const uint16x8_t surrogates_bytemask = vceqq_u16(vandq_u16(in, v_f800), v_d800);
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (vmaxvq_u16(surrogates_bytemask) == 0) {
        // case: code units from register produce either 1, 2 or 3 UTF-8 bytes
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
        const uint16x8_t dup_even = simdutf_make_uint16x8_t(0x0000, 0x0202, 0x0404, 0x0606,
                                     0x0808, 0x0a0a, 0x0c0c, 0x0e0e);
#else
        const uint16x8_t dup_even = {0x0000, 0x0202, 0x0404, 0x0606,
                                     0x0808, 0x0a0a, 0x0c0c, 0x0e0e};
#endif
        /* In this branch we handle three cases:
           1. [0000|0000|0ccc|cccc] => [0ccc|cccc]                           - single UFT-8 byte
           2. [0000|0bbb|bbcc|cccc] => [110b|bbbb], [10cc|cccc]              - two UTF-8 bytes
           3. [aaaa|bbbb|bbcc|cccc] => [1110|aaaa], [10bb|bbbb], [10cc|cccc] - three UTF-8 bytes

          We expand the input word (16-bit) into two code units (32-bit), thus
          we have room for four bytes. However, we need five distinct bit
          layouts. Note that the last byte in cases #2 and #3 is the same.

          We precompute byte 1 for case #1 and the common byte for cases #2 & #3
          in register t2.

          We precompute byte 1 for case #3 and -- **conditionally** -- precompute
          either byte 1 for case #2 or byte 2 for case #3. Note that they
          differ by exactly one bit.

          Finally from these two code units we build proper UTF-8 sequence, taking
          into account the case (i.e, the number of bytes to write).
        */
        /**
         * Given [aaaa|bbbb|bbcc|cccc] our goal is to produce:
         * t2 => [0ccc|cccc] [10cc|cccc]
         * s4 => [1110|aaaa] ([110b|bbbb] OR [10bb|bbbb])
         */
#define simdutf_vec(x) vmovq_n_u16(static_cast<uint16_t>(x))
        // [aaaa|bbbb|bbcc|cccc] => [bbcc|cccc|bbcc|cccc]
        const uint16x8_t t0 = vreinterpretq_u16_u8(vqtbl1q_u8(vreinterpretq_u8_u16(in), vreinterpretq_u8_u16(dup_even)));
        // [bbcc|cccc|bbcc|cccc] => [00cc|cccc|0bcc|cccc]
        const uint16x8_t t1 = vandq_u16(t0, simdutf_vec(0b0011111101111111));
        // [00cc|cccc|0bcc|cccc] => [10cc|cccc|0bcc|cccc]
        const uint16x8_t t2 = vorrq_u16 (t1, simdutf_vec(0b1000000000000000));

        // s0: [aaaa|bbbb|bbcc|cccc] => [0000|0000|0000|aaaa]
        const uint16x8_t s0 = vshrq_n_u16(in, 12);
        // s1: [aaaa|bbbb|bbcc|cccc] => [0000|bbbb|bb00|0000]
        const uint16x8_t s1 = vandq_u16(in, simdutf_vec(0b0000111111000000));
        // [0000|bbbb|bb00|0000] => [00bb|bbbb|0000|0000]
        const uint16x8_t s1s = vshlq_n_u16(s1, 2);
        // [00bb|bbbb|0000|aaaa]
        const uint16x8_t s2 = vorrq_u16(s0, s1s);
        // s3: [00bb|bbbb|0000|aaaa] => [11bb|bbbb|1110|aaaa]
        const uint16x8_t s3 = vorrq_u16(s2, simdutf_vec(0b1100000011100000));
        const uint16x8_t v_07ff = vmovq_n_u16((uint16_t)0x07FF);
        const uint16x8_t one_or_two_bytes_bytemask = vcleq_u16(in, v_07ff);
        const uint16x8_t m0 = vbicq_u16(simdutf_vec(0b0100000000000000), one_or_two_bytes_bytemask);
        const uint16x8_t s4 = veorq_u16(s3, m0);
#undef simdutf_vec

        // 4. expand code units 16-bit => 32-bit
        const uint8x16_t out0 = vreinterpretq_u8_u16(vzip1q_u16(t2, s4));
        const uint8x16_t out1 = vreinterpretq_u8_u16(vzip2q_u16(t2, s4));

        // 5. compress 32-bit code units into 1, 2 or 3 bytes -- 2 x shuffle
        const uint16x8_t v_007f = vmovq_n_u16((uint16_t)0x007F);
        const uint16x8_t one_byte_bytemask = vcleq_u16(in, v_007f);
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
        const uint16x8_t onemask = simdutf_make_uint16x8_t(0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0100, 0x0400,
                                    0x1000, 0x4000 );
        const uint16x8_t twomask = simdutf_make_uint16x8_t(0x0002, 0x0008,
                                    0x0020, 0x0080,
                                    0x0200, 0x0800,
                                    0x2000, 0x8000 );
#else
        const uint16x8_t onemask = { 0x0001, 0x0004,
                                    0x0010, 0x0040,
                                    0x0100, 0x0400,
                                    0x1000, 0x4000 };
        const uint16x8_t twomask = { 0x0002, 0x0008,
                                    0x0020, 0x0080,
                                    0x0200, 0x0800,
                                    0x2000, 0x8000 };
#endif
        const uint16x8_t combined = vorrq_u16(vandq_u16(one_byte_bytemask, onemask), vandq_u16(one_or_two_bytes_bytemask, twomask));
        const uint16_t mask = vaddvq_u16(combined);
        // The following fast path may or may not be beneficial.
        /*if(mask == 0) {
          // We only have three-byte code units. Use fast path.
          const uint8x16_t shuffle = {2,3,1,6,7,5,10,11,9,14,15,13,0,0,0,0};
          const uint8x16_t utf8_0 = vqtbl1q_u8(out0, shuffle);
          const uint8x16_t utf8_1 = vqtbl1q_u8(out1, shuffle);
          vst1q_u8(utf8_output, utf8_0);
          utf8_output += 12;
          vst1q_u8(utf8_output, utf8_1);
          utf8_output += 12;
          buf += 8;
          continue;
        }*/
        const uint8_t mask0 = uint8_t(mask);

        const uint8_t* row0 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask0][0];
        const uint8x16_t shuffle0 = vld1q_u8(row0 + 1);
        const uint8x16_t utf8_0 = vqtbl1q_u8(out0, shuffle0);

        const uint8_t mask1 = static_cast<uint8_t>(mask >> 8);
        const uint8_t* row1 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask1][0];
        const uint8x16_t shuffle1 = vld1q_u8(row1 + 1);
        const uint8x16_t utf8_1 = vqtbl1q_u8(out1, shuffle1);

        vst1q_u8(utf8_output, utf8_0);
        utf8_output += row0[0];
        vst1q_u8(utf8_output, utf8_1);
        utf8_output += row1[0];

        buf += 8;
    // surrogate pair(s) in a register
    } else {
      // Let us do a scalar fallback.
      // It may seem wasteful to use scalar code, but being efficient with SIMD
      // in the presence of surrogate pairs may require non-trivial tables.
      size_t forward = 15;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint16_t word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if((word & 0xFF80)==0) {
          *utf8_output++ = char(word);
        } else if((word & 0xF800)==0) {
          *utf8_output++ = char((word>>6) | 0b11000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else if((word &0xF800 ) != 0xD800) {
          *utf8_output++ = char((word>>12) | 0b11100000);
          *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k + 1]) : buf[k + 1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(result(error_code::SURROGATE, buf - start + k - 1), reinterpret_cast<char*>(utf8_output)); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf8_output++ = char((value>>18) | 0b11110000);
          *utf8_output++ = char(((value>>12) & 0b111111) | 0b10000000);
          *utf8_output++ = char(((value>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((value & 0b111111) | 0b10000000);
        }
      }
      buf += k;
    }
  } // while

  return std::make_pair(result(error_code::SUCCESS, buf - start), reinterpret_cast<char*>(utf8_output));
}
