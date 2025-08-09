/*
* reads a vector of uint16 values
* bits after 11th are ignored
* first 11 bits are encoded into utf8
* !important! utf8_output must have at least 16 writable bytes
*/

inline void write_v_u16_11bits_to_utf8(
  const __m128i v_u16,
  char*& utf8_output,
  const __m128i one_byte_bytemask,
  const uint16_t one_byte_bitmask
) {
  // 0b1100_0000_1000_0000
  const __m128i v_c080 = _mm_set1_epi16((int16_t)0xc080);
  // 0b0001_1111_0000_0000
  const __m128i v_1f00 = _mm_set1_epi16((int16_t)0x1f00);
  // 0b0000_0000_0011_1111
  const __m128i v_003f = _mm_set1_epi16((int16_t)0x003f);

  // 1. prepare 2-byte values
          // input 16-bit word : [0000|0aaa|aabb|bbbb] x 8
          // expected output   : [110a|aaaa|10bb|bbbb] x 8

  // t0 = [000a|aaaa|bbbb|bb00]
  const __m128i t0 = _mm_slli_epi16(v_u16, 2);
  // t1 = [000a|aaaa|0000|0000]
  const __m128i t1 = _mm_and_si128(t0, v_1f00);
  // t2 = [0000|0000|00bb|bbbb]
  const __m128i t2 = _mm_and_si128(v_u16, v_003f);
  // t3 = [000a|aaaa|00bb|bbbb]
  const __m128i t3 = _mm_or_si128(t1, t2);
  // t4 = [110a|aaaa|10bb|bbbb]
  const __m128i t4 = _mm_or_si128(t3, v_c080);

  // 2. merge ASCII and 2-byte codewords
  const __m128i utf8_unpacked = _mm_blendv_epi8(t4, v_u16, one_byte_bytemask);

  // 3. prepare bitmask for 8-bit lookup
  //    one_byte_bitmask = hhggffeeddccbbaa -- the bits are doubled (h - MSB, a - LSB)
  const uint16_t m0 = one_byte_bitmask & 0x5555;  // m0 = 0h0g0f0e0d0c0b0a
  const uint16_t m1 = static_cast<uint16_t>(m0 >> 7);                    // m1 = 00000000h0g0f0e0
  const uint8_t  m2 = static_cast<uint8_t>((m0 | m1) & 0xff);           // m2 =         hdgcfbea
  // 4. pack the bytes
  const uint8_t* row = &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[m2][0];
  const __m128i shuffle = _mm_loadu_si128((__m128i*)(row + 1));
  const __m128i utf8_packed = _mm_shuffle_epi8(utf8_unpacked, shuffle);

  // 5. store bytes
  _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);

  // 6. adjust pointers
  utf8_output += row[0];
}

inline void write_v_u16_11bits_to_utf8(
  const __m128i v_u16,
  char*& utf8_output,
  const __m128i v_0000,
  const __m128i v_ff80
) {
  // no bits set above 7th bit
  const __m128i one_byte_bytemask = _mm_cmpeq_epi16(_mm_and_si128(v_u16, v_ff80), v_0000);
  const uint16_t one_byte_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(one_byte_bytemask));

  write_v_u16_11bits_to_utf8(
    v_u16, utf8_output, one_byte_bytemask, one_byte_bitmask);
}