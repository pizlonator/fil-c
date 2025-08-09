// Convert up to 16 bytes from utf8 to utf16 using a mask indicating the
// end of the code points. Only the least significant 12 bits of the mask
// are accessed.
// It returns how many bytes were consumed (up to 16, usually 12).
template <endianness big_endian>
size_t convert_masked_utf8_to_utf16(const char *input,
                           uint64_t utf8_end_of_code_point_mask,
                           char16_t *&utf16_output) {
  // we use an approach where we try to process up to 12 input bytes.
  // Why 12 input bytes and not 16? Because we are concerned with the size of
  // the lookup tables. Also 12 is nicely divisible by two and three.
  //
  uint8x16_t in = vld1q_u8(reinterpret_cast<const uint8_t*>(input));
  const uint16_t input_utf8_end_of_code_point_mask =
      utf8_end_of_code_point_mask & 0xfff;
  //
  // Optimization note: our main path below is load-latency dependent. Thus it is maybe
  // beneficial to have fast paths that depend on branch prediction but have less latency.
  // This results in more instructions but, potentially, also higher speeds.

  // We first try a few fast paths.
  // The obvious first test is ASCII, which actually consumes the full 16.
  if((utf8_end_of_code_point_mask & 0xFFFF) == 0xffff) {
    // We process in chunks of 16 bytes
    // The routine in simd.h is reused.
    simd8<int8_t> temp{vreinterpretq_s8_u8(in)};
    temp.store_ascii_as_utf16<big_endian>(utf16_output);
    utf16_output += 16; // We wrote 16 16-bit characters.
    return 16; // We consumed 16 bytes.
  }

  // 3 byte sequences are the next most common, as seen in CJK, which has long sequences
  // of these.
  if (input_utf8_end_of_code_point_mask == 0x924) {
    // We want to take 4 3-byte UTF-8 code units and turn them into 4 2-byte UTF-16 code units.
    uint16x4_t composed = convert_utf8_3_byte_to_utf16(in);
    // Byte swap if necessary
    if (!match_system(big_endian)) {
      composed = vreinterpret_u16_u8(vrev16_u8(vreinterpret_u8_u16(composed)));
    }
    vst1_u16(reinterpret_cast<uint16_t*>(utf16_output), composed);
    utf16_output += 4; // We wrote 4 16-bit characters.
    return 12; // We consumed 12 bytes.
  }

  // 2 byte sequences occur in short bursts in languages like Greek and Russian.
  if ((utf8_end_of_code_point_mask & 0xFFF) == 0xaaa) {
    // We want to take 6 2-byte UTF-8 code units and turn them into 6 2-byte UTF-16 code units.
    uint16x8_t composed = convert_utf8_2_byte_to_utf16(in);
    // Byte swap if necessary
    if (!match_system(big_endian)) {
      composed = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(composed)));
    }
    vst1q_u16(reinterpret_cast<uint16_t *>(utf16_output), composed);

    utf16_output += 6; // We wrote 6 16-bit characters.
    return 12; // We consumed 12 bytes.
  }

  /// We do not have a fast path available, or the fast path is unimportant, so we fallback.
  const uint8_t idx =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][0];

  const uint8_t consumed =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][1];

  if (idx < 64) {
    // SIX (6) input code-code units
    // Convert to UTF-16
    uint16x8_t composed = convert_utf8_1_to_2_byte_to_utf16(in, idx);
    // Byte swap if necessary
    if (!match_system(big_endian)) {
      composed = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(composed)));
    }
    // Store
    vst1q_u16(reinterpret_cast<uint16_t*>(utf16_output), composed);
    utf16_output += 6; // We wrote 6 16-bit characters.
    return consumed;
  } else if (idx < 145) {
    // FOUR (4) input code-code units
    // UTF-16 and UTF-32 use similar algorithms, but UTF-32 skips the narrowing.
    uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[idx]));
    // XXX: depending on the system scalar instructions might be faster.
    // 1 byte: 00000000 00000000 0ccccccc
    // 2 byte: 00000000 110bbbbb 10cccccc
    // 3 byte: 1110aaaa 10bbbbbb 10cccccc
    uint32x4_t perm = vreinterpretq_u32_u8(vqtbl1q_u8(in, sh));
    // 1 byte: 00000000 0ccccccc
    // 2 byte: xx0bbbbb x0cccccc
    // 3 byte: xxbbbbbb x0cccccc
    uint16x4_t lowperm = vmovn_u32(perm);
    // Partially mask with bic (doesn't require a temporary register unlike and)
    // The shift left insert below will clear the top bits.
    // 1 byte: 00000000 00000000
    // 2 byte: xx0bbbbb 00000000
    // 3 byte: xxbbbbbb 00000000
    uint16x4_t middlebyte = vbic_u16(lowperm, vmov_n_u16(uint16_t(~0xFF00)));
    // ASCII
    // 1 byte: 00000000 0ccccccc
    // 2+byte: 00000000 00cccccc
    uint16x4_t ascii = vand_u16(lowperm, vmov_n_u16(0x7F));
    // Split into narrow vectors.
    // 2 byte: 00000000 00000000
    // 3 byte: 00000000 xxxxaaaa
    uint16x4_t highperm = vshrn_n_u32(perm, 16);
    // Shift right accumulate the middle byte
    // 1 byte: 00000000 0ccccccc
    // 2 byte: 00xx0bbb bbcccccc
    // 3 byte: 00xxbbbb bbcccccc
    uint16x4_t middlelow = vsra_n_u16(ascii, middlebyte, 2);
    // Shift left and insert the top 4 bits, overwriting the garbage
    // 1 byte: 00000000 0ccccccc
    // 2 byte: 00000bbb bbcccccc
    // 3 byte: aaaabbbb bbcccccc
    uint16x4_t composed = vsli_n_u16(middlelow, highperm, 12);
    // Byte swap if necessary
    if (!match_system(big_endian)) {
      composed = vreinterpret_u16_u8(vrev16_u8(vreinterpret_u8_u16(composed)));
    }
    vst1_u16(reinterpret_cast<uint16_t*>(utf16_output), composed);

    utf16_output += 4; // We wrote 4 16-bit codepoints
    return consumed;
  } else if (idx < 209) {
    // THREE (3) input code-code units
    if (input_utf8_end_of_code_point_mask == 0x888) {
      // We want to take 3 4-byte UTF-8 code units and turn them into 3 4-byte UTF-16 pairs.
      // Generating surrogate pairs is a little tricky though, but it is easier when we
      // can assume they are all pairs.
      // This version does not use the LUT, but 4 byte sequences are less common and the
      // overhead of the extra memory access is less important than the early branch overhead
      // in shorter sequences.

      // Swap byte pairs
      // 10dddddd 10cccccc|10bbbbbb 11110aaa
      // 10cccccc 10dddddd|11110aaa 10bbbbbb
      uint8x16_t swap = vrev16q_u8(in);
      // Shift left 2 bits
      // cccccc00 dddddd00 xxxxxxxx bbbbbb00
      uint32x4_t shift = vreinterpretq_u32_u8(vshlq_n_u8(swap, 2));
      // Create a magic number containing the low 2 bits of the trail surrogate and all the
      // corrections needed to create the pair.
      // UTF-8 4b prefix   = -0x0000|0xF000
      // surrogate offset  = -0x0000|0x0040 (0x10000 << 6)
      // surrogate high    = +0x0000|0xD800
      // surrogate low     = +0xDC00|0x0000
      // -------------------------------
      //                   = +0xDC00|0xE7C0
      uint32x4_t magic = vmovq_n_u32(0xDC00E7C0);
      // Generate unadjusted trail surrogate minus lowest 2 bits
      // xxxxxxxx xxxxxxxx|11110aaa bbbbbb00
      uint32x4_t trail = vbslq_u32(vmovq_n_u32(0x0000FF00), vreinterpretq_u32_u8(swap), shift);
      // Insert low 2 bits of trail surrogate to magic number for later
      // 11011100 00000000 11100111 110000cc
      uint16x8_t magic_with_low_2 = vreinterpretq_u16_u32(vsraq_n_u32(magic, shift, 30));
      // Generate lead surrogate
      // xxxxcccc ccdddddd|xxxxxxxx xxxxxxxx
      uint32x4_t lead = vreinterpretq_u32_u16(vsliq_n_u16(vreinterpretq_u16_u8(swap), vreinterpretq_u16_u8(in), 6));
      // Mask out lead
      // 000000cc ccdddddd|xxxxxxxx xxxxxxxx
      lead = vbicq_u32(lead, vmovq_n_u32(uint32_t(~0x03FFFFFF)));
      // Blend pairs
      // 000000cc ccdddddd|11110aaa bbbbbb00
      uint16x8_t blend = vreinterpretq_u16_u32(vbslq_u32(vmovq_n_u32(0x0000FFFF), trail, lead));
      // Add magic number to finish the result
      // 110111CC CCDDDDDD|110110AA BBBBBBCC
      uint16x8_t composed = vaddq_u16(blend, magic_with_low_2);
      // Byte swap if necessary
      if (!match_system(big_endian)) {
        composed = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(composed)));
      }
      uint16_t buffer[8];
      vst1q_u16(reinterpret_cast<uint16_t *>(buffer), composed);
      for(int k = 0; k < 6; k++) { utf16_output[k] = buffer[k]; } // the loop might compiler to a couple of instructions.
      utf16_output += 6; // We wrote 3 32-bit surrogate pairs.
      return 12; // We consumed 12 bytes.
    }
    // 3 1-4 byte sequences
    uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[idx]));

    // 1 byte: 00000000 00000000 00000000 0ddddddd
    // 3 byte: 00000000 00000000 110ccccc 10dddddd
    // 3 byte: 00000000 1110bbbb 10cccccc 10dddddd
    // 4 byte: 11110aaa 10bbbbbb 10cccccc 10dddddd
    uint32x4_t perm = vreinterpretq_u32_u8(vqtbl1q_u8(in, sh));
    // added to fix issue https://github.com/simdutf/simdutf/issues/514
    // We only want to write 2 * 16-bit code units when that is actually what we have.
    // Unfortunately, we cannot trust the input. So it is possible to get 0xff as an input byte
    // and it should not result in a surrogate pair. We need to check for that.
    uint32_t permbuffer[4];
    vst1q_u32(permbuffer, perm);
    // Mask the low and middle bytes
    // 00000000 00000000 00000000 0ddddddd
    uint32x4_t ascii = vandq_u32(perm, vmovq_n_u32(0x7f));
    // Because the surrogates need more work, the high surrogate is computed first.
    uint32x4_t middlehigh = vshlq_n_u32(perm, 2);
    // 00000000 00000000 00cccccc 00000000
    uint32x4_t middlebyte = vandq_u32(perm, vmovq_n_u32(0x3F00));
    // Start assembling the sequence. Since the 4th byte is in the same position as it
    // would be in a surrogate and there is no dependency, shift left instead of right.
    // 3 byte: 00000000 10bbbbxx xxxxxxxx xxxxxxxx
    // 4 byte: 11110aaa bbbbbbxx xxxxxxxx xxxxxxxx
    uint32x4_t ab = vbslq_u32(vmovq_n_u32(0xFF000000), perm, middlehigh);
    // Top 16 bits contains the high ten bits of the surrogate pair before correction
    // 3 byte: 00000000 10bbbbcc|cccc0000 00000000
    // 4 byte: 11110aaa bbbbbbcc|cccc0000 00000000 - high 10 bits correct w/o correction
    uint32x4_t abc = vbslq_u32(vmovq_n_u32(0xFFFC0000), ab, vshlq_n_u32(middlebyte, 4));
    // Combine the low 6 or 7 bits by a shift right accumulate
    // 3 byte: 00000000 00000010|bbbbcccc ccdddddd - low 16 bits correct
    // 4 byte: 00000011 110aaabb|bbbbcccc ccdddddd - low 10 bits correct w/o correction
    uint32x4_t composed = vsraq_n_u32(ascii, abc, 6);
    // After this is for surrogates
    // Blend the low and high surrogates
    // 4 byte: 11110aaa bbbbbbcc|bbbbcccc ccdddddd
    uint32x4_t mixed = vbslq_u32(vmovq_n_u32(0xFFFF0000), abc, composed);
    // Clear the upper 6 bits of the low surrogate. Don't clear the upper bits yet as
    // 0x10000 was not subtracted from the codepoint yet.
    // 4 byte: 11110aaa bbbbbbcc|000000cc ccdddddd
    uint16x8_t masked_pair =
        vreinterpretq_u16_u32(vbicq_u32(mixed, vmovq_n_u32(uint32_t(~0xFFFF03FF))));
    // Correct the remaining UTF-8 prefix, surrogate offset, and add the surrogate prefixes
    // in one magic 16-bit addition.
    // similar magic number but without the continue byte adjust and halfword swapped
    // UTF-8 4b prefix   = -0xF000|0x0000
    // surrogate offset  = -0x0040|0x0000 (0x10000 << 6)
    // surrogate high    = +0xD800|0x0000
    // surrogate low     = +0x0000|0xDC00
    // -----------------------------------
    //                   = +0xE7C0|0xDC00
    uint16x8_t magic = vreinterpretq_u16_u32(vmovq_n_u32(0xE7C0DC00));
    // 4 byte: 110110AA BBBBBBCC|110111CC CCDDDDDD - surrogate pair complete
    uint32x4_t surrogates = vreinterpretq_u32_u16(vaddq_u16(masked_pair, magic));
    // If the high bit is 1 (s32 less than zero), this needs a surrogate pair
    uint32x4_t is_pair = vcltzq_s32(vreinterpretq_s32_u32(perm));

    // Select either the 4 byte surrogate pair or the 2 byte solo codepoint
    // 3 byte: 0xxxxxxx xxxxxxxx|bbbbcccc ccdddddd
    // 4 byte: 110110AA BBBBBBCC|110111CC CCDDDDDD
    uint32x4_t selected = vbslq_u32(is_pair, surrogates, composed);
    // Byte swap if necessary
    if (!match_system(big_endian)) {
      selected = vreinterpretq_u32_u8(vrev16q_u8(vreinterpretq_u8_u32(selected)));
    }
    // Attempting to shuffle and store would be complex, just scalarize.
    uint32_t buffer[4];
    vst1q_u32(buffer, selected);
    // Test for the top bit of the surrogate mask. Remove due to issue 514
    // const uint32_t SURROGATE_MASK = match_system(big_endian) ? 0x80000000 : 0x00800000;
    for (size_t i = 0; i < 3; i++) {
      // Surrogate
      // Used to be if (buffer[i] & SURROGATE_MASK) {
      // See discussion above.
      // patch for issue https://github.com/simdutf/simdutf/issues/514
      if((permbuffer[i] & 0xf8000000) == 0xf0000000) {
        utf16_output[0] = uint16_t(buffer[i] >> 16);
        utf16_output[1] = uint16_t(buffer[i] & 0xFFFF);
        utf16_output += 2;
      } else {
        utf16_output[0] = uint16_t(buffer[i] & 0xFFFF);
        utf16_output++;
      }
    }
    return consumed;
  } else {
    // here we know that there is an error but we do not handle errors
    return 12;
  }
}

