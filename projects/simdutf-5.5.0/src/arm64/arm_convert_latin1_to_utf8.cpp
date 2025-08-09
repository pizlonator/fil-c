/*
  Returns a pair: the first unprocessed byte from buf and utf8_output
  A scalar routing should carry on the conversion of the tail.
*/
std::pair<const char *, char *>
arm_convert_latin1_to_utf8(const char *latin1_input, size_t len,
                           char *utf8_out) {
  uint8_t *utf8_output = reinterpret_cast<uint8_t *>(utf8_out);
  const char *end = latin1_input + len;
  const uint16x8_t v_c080 = vmovq_n_u16((uint16_t)0xc080);
  // We always write 16 bytes, of which more than the first 8 bytes
  // are valid. A safety margin of 8 is more than sufficient.
  while (end - latin1_input >= 16 + 8) {
    uint8x16_t in8 = vld1q_u8(reinterpret_cast<const uint8_t *>(latin1_input));
    if (vmaxvq_u8(in8) <= 0x7F) { // ASCII fast path!!!!
      vst1q_u8(utf8_output, in8);
      utf8_output += 16;
      latin1_input += 16;
      continue;
    }

    // We just fallback on UTF-16 code. This could be optimized/simplified
    // further.
    uint16x8_t in16 = vmovl_u8(vget_low_u8(in8));
    // 1. prepare 2-byte values
    // input 8-bit word : [aabb|bbbb] x 8
    // expected output   : [1100|00aa|10bb|bbbb] x 8
    const uint16x8_t v_1f00 = vmovq_n_u16((int16_t)0x1f00);
    const uint16x8_t v_003f = vmovq_n_u16((int16_t)0x003f);

    // t0 = [0000|00aa|bbbb|bb00]
    const uint16x8_t t0 = vshlq_n_u16(in16, 2);
    // t1 = [0000|00aa|0000|0000]
    const uint16x8_t t1 = vandq_u16(t0, v_1f00);
    // t2 = [0000|0000|00bb|bbbb]
    const uint16x8_t t2 = vandq_u16(in16, v_003f);
    // t3 = [0000|00aa|00bb|bbbb]
    const uint16x8_t t3 = vorrq_u16(t1, t2);
    // t4 = [1100|00aa|10bb|bbbb]
    const uint16x8_t t4 = vorrq_u16(t3, v_c080);
    // 2. merge ASCII and 2-byte codewords
    const uint16x8_t v_007f = vmovq_n_u16((uint16_t)0x007F);
    const uint16x8_t one_byte_bytemask = vcleq_u16(in16, v_007f);
    const uint8x16_t utf8_unpacked =
        vreinterpretq_u8_u16(vbslq_u16(one_byte_bytemask, in16, t4));
    // 3. prepare bitmask for 8-bit lookup
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
    const uint16x8_t mask = simdutf_make_uint16x8_t(0x0001, 0x0004, 0x0010, 0x0040,
                                            0x0002, 0x0008, 0x0020, 0x0080);
#else
    const uint16x8_t mask = {0x0001, 0x0004, 0x0010, 0x0040,
                             0x0002, 0x0008, 0x0020, 0x0080};
#endif
    uint16_t m2 = vaddvq_u16(vandq_u16(one_byte_bytemask, mask));
    // 4. pack the bytes
    const uint8_t *row =
        &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[m2][0];
    const uint8x16_t shuffle = vld1q_u8(row + 1);
    const uint8x16_t utf8_packed = vqtbl1q_u8(utf8_unpacked, shuffle);

    // 5. store bytes
    vst1q_u8(utf8_output, utf8_packed);
    // 6. adjust pointers
    latin1_input += 8;
    utf8_output += row[0];

  } // while

  return std::make_pair(latin1_input, reinterpret_cast<char *>(utf8_output));
}
