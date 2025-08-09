// Convert up to 12 bytes from utf8 to utf32 using a mask indicating the
// end of the code points. Only the least significant 12 bits of the mask
// are accessed.
// It returns how many bytes were consumed (up to 12).
size_t convert_masked_utf8_to_utf32(const char *input,
                           uint64_t utf8_end_of_code_point_mask,
                           char32_t *&utf32_out) {
  // we use an approach where we try to process up to 12 input bytes.
  // Why 12 input bytes and not 16? Because we are concerned with the size of
  // the lookup tables. Also 12 is nicely divisible by two and three.
  //
  uint32_t*& utf32_output = reinterpret_cast<uint32_t*&>(utf32_out);
  uint8x16_t in = vld1q_u8(reinterpret_cast<const uint8_t*>(input));
  const uint16_t input_utf8_end_of_code_point_mask =
      utf8_end_of_code_point_mask & 0xFFF;
  //
  // Optimization note: our main path below is load-latency dependent. Thus it is maybe
  // beneficial to have fast paths that depend on branch prediction but have less latency.
  // This results in more instructions but, potentially, also higher speeds.
  //
  // We first try a few fast paths.
  if(utf8_end_of_code_point_mask == 0xfff) {
    // We process in chunks of 12 bytes.
    // use fast implementation in src/simdutf/arm64/simd.h
    // Ideally the compiler can keep the tables in registers.
    simd8<int8_t> temp{vreinterpretq_s8_u8(in)};
    temp.store_ascii_as_utf32_tbl(utf32_out);
    utf32_output += 12; // We wrote 12 32-bit characters.
    return 12; // We consumed 12 bytes.
  }
  if(input_utf8_end_of_code_point_mask == 0x924) {
    // We want to take 4 3-byte UTF-8 code units and turn them into 4 4-byte UTF-32 code units.
    // Convert to UTF-16
    uint16x4_t composed_utf16 = convert_utf8_3_byte_to_utf16(in);
    // Zero extend and store via ST2 with a zero.
    uint16x4x2_t interleaver = {{ composed_utf16, vmov_n_u16(0) }};
    vst2_u16(reinterpret_cast<uint16_t *>(utf32_output), interleaver);
    utf32_output += 4; // We wrote 4 32-bit characters.
    return 12; // We consumed 12 bytes.
  }

  // 2 byte sequences occur in short bursts in languages like Greek and Russian.
  if(input_utf8_end_of_code_point_mask == 0xaaa) {
    // We want to take 6 2-byte UTF-8 code units and turn them into 6 4-byte UTF-32 code units.
    // Convert to UTF-16
    uint16x8_t composed_utf16 = convert_utf8_2_byte_to_utf16(in);
    // Zero extend and store via ST2 with a zero.
    uint16x8x2_t interleaver = {{ composed_utf16, vmovq_n_u16(0) }};
    vst2q_u16(reinterpret_cast<uint16_t *>(utf32_output), interleaver);
    utf32_output += 6; // We wrote 6 32-bit characters.
    return 12; // We consumed 12 bytes.
  }
  /// Either no fast path or an unimportant fast path.

  const uint8_t idx =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][0];
  const uint8_t consumed =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][1];


  if (idx < 64) {
    // SIX (6) input code-code units
    // Convert to UTF-16
    uint16x8_t composed_utf16 = convert_utf8_1_to_2_byte_to_utf16(in, idx);
    // Zero extend and store with ST2 and zero
    uint16x8x2_t interleaver = {{ composed_utf16, vmovq_n_u16(0) }};
    vst2q_u16(reinterpret_cast<uint16_t *>(utf32_output), interleaver);
    utf32_output += 6; // We wrote 6 32-bit characters.
    return consumed;
  } else if (idx < 145) {
    // FOUR (4) input code-code units
    // UTF-16 and UTF-32 use similar algorithms, but UTF-32 skips the narrowing.
    uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[idx]));
    // Shuffle
    // 1 byte: 00000000 00000000 0ccccccc
    // 2 byte: 00000000 110bbbbb 10cccccc
    // 3 byte: 1110aaaa 10bbbbbb 10cccccc
    uint32x4_t perm = vreinterpretq_u32_u8(vqtbl1q_u8(in, sh));
    // Split
    // 00000000 00000000 0ccccccc
    uint32x4_t ascii = vandq_u32(perm, vmovq_n_u32(0x7F));    // 6 or 7 bits
    // Note: unmasked
    // xxxxxxxx aaaaxxxx xxxxxxxx
    uint32x4_t high = vshrq_n_u32(perm, 4);                   // 4 bits
    // Use 16 bit bic instead of and.
    // The top bits will be corrected later in the bsl
    // 00000000 10bbbbbb 00000000
    uint32x4_t middle =
        vreinterpretq_u32_u16(vbicq_u16(vreinterpretq_u16_u32(perm), vmovq_n_u16(uint16_t(~0xff00)))); // 5 or 6 bits
    // Combine low and middle with shift right accumulate
    // 00000000 00xxbbbb bbcccccc
    uint32x4_t lowmid = vsraq_n_u32(ascii, middle, 2);
    // Insert top 4 bits from high byte with bitwise select
    // 00000000 aaaabbbb bbcccccc
    uint32x4_t composed = vbslq_u32(vmovq_n_u32(0x0000F000), high, lowmid);
    vst1q_u32(utf32_output, composed);
    utf32_output += 4; // We wrote 4 32-bit characters.
    return consumed;
  } else if (idx < 209) {
    // THREE (3) input code-code units
    if (input_utf8_end_of_code_point_mask == 0x888) {
      // We want to take 3 4-byte UTF-8 code units and turn them into 3 4-byte UTF-32 code units.
      // This uses the same method as the fixed 3 byte version, reversing and shift left insert.
      // However, there is no need for a shuffle mask now, just rev16 and rev32.
      //
      // This version does not use the LUT, but 4 byte sequences are less common and the
      // overhead of the extra memory access is less important than the early branch overhead
      // in shorter sequences, so it comes last.

      // Swap pairs of bytes
      // 10dddddd|10cccccc|10bbbbbb|11110aaa
      // 10cccccc 10dddddd|11110aaa 10bbbbbb
      uint16x8_t swap1 = vreinterpretq_u16_u8(vrev16q_u8(in));
      // Shift left and insert
      // xxxxcccc ccdddddd|xxxxxxxa aabbbbbb
      uint16x8_t merge1 = vsliq_n_u16(swap1, vreinterpretq_u16_u8(in), 6);
      // Swap 16-bit lanes
      // xxxxcccc ccdddddd xxxxxxxa aabbbbbb
      // xxxxxxxa aabbbbbb xxxxcccc ccdddddd
      uint32x4_t swap2 = vreinterpretq_u32_u16(vrev32q_u16(merge1));
      // Shift insert again
      // xxxxxxxx xxxaaabb bbbbcccc ccdddddd
      uint32x4_t merge2 = vsliq_n_u32(swap2, vreinterpretq_u32_u16(merge1), 12);
      // Clear the garbage
      // 00000000 000aaabb bbbbcccc ccdddddd
      uint32x4_t composed = vandq_u32(merge2, vmovq_n_u32(0x1FFFFF));
      // Store
      vst1q_u32(utf32_output, composed);

      utf32_output += 3; // We wrote 3 32-bit characters.
      return 12; // We consumed 12 bytes.
    }
    // Unlike UTF-16, doing a fast codepath doesn't have nearly as much benefit due to
    // surrogates no longer being involved.
    uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[idx]));
    // 1 byte: 00000000 00000000 00000000 0ddddddd
    // 2 byte: 00000000 00000000 110ccccc 10dddddd
    // 3 byte: 00000000 1110bbbb 10cccccc 10dddddd
    // 4 byte: 11110aaa 10bbbbbb 10cccccc 10dddddd
    uint32x4_t perm = vreinterpretq_u32_u8(vqtbl1q_u8(in, sh));
    // Ascii
    uint32x4_t ascii = vandq_u32(perm, vmovq_n_u32(0x7F));
    uint32x4_t middle = vandq_u32(perm, vmovq_n_u32(0x3f00));
    // When converting the way we do, the 3 byte prefix will be interpreted as the
    // 18th bit being set, since the code would interpret the lead byte (0b1110bbbb)
    // as a continuation byte (0b10bbbbbb). To fix this, we can either xor or do an
    // 8 bit add of the 6th bit shifted right by 1. Since NEON has shift right accumulate,
    // we use that.
    //  4 byte   3 byte
    // 10bbbbbb 1110bbbb
    // 00000000 01000000 6th bit
    // 00000000 00100000 shift right
    // 10bbbbbb 0000bbbb add
    // 00bbbbbb 0000bbbb mask
    uint8x16_t correction =
        vreinterpretq_u8_u32(vandq_u32(perm, vmovq_n_u32(0x00400000)));
    uint32x4_t corrected =
        vreinterpretq_u32_u8(vsraq_n_u8(vreinterpretq_u8_u32(perm), correction, 1));
    // 00000000 00000000 0000cccc ccdddddd
    uint32x4_t cd = vsraq_n_u32(ascii, middle, 2);
    // Insert twice
    // xxxxxxxx xxxaaabb bbbbxxxx xxxxxxxx
    uint32x4_t ab = vbslq_u32(vmovq_n_u32(0x01C0000), vshrq_n_u32(corrected, 6), vshrq_n_u32(corrected, 4));
    // 00000000 000aaabb bbbbcccc ccdddddd
    uint32x4_t composed = vbslq_u32(vmovq_n_u32(0xFFE00FFF), cd, ab);
    // Store
    vst1q_u32(utf32_output, composed);
    utf32_output += 3; // We wrote 3 32-bit characters.
    return consumed;
  } else {
    // here we know that there is an error but we do not handle errors
    return 12;
  }
}
