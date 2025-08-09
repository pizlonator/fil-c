// Convert up to 16 bytes from utf8 to utf16 using a mask indicating the
// end of the code points. Only the least significant 12 bits of the mask
// are accessed.
// It returns how many bytes were consumed (up to 16, usually 12).
size_t convert_masked_utf8_to_latin1(const char *input,
                           uint64_t utf8_end_of_code_point_mask,
                           char *&latin1_output) {
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
  if(utf8_end_of_code_point_mask == 0xfff) {
    // We process in chunks of 12 bytes
    vst1q_u8(reinterpret_cast<uint8_t*>(latin1_output), in);
    latin1_output += 12; // We wrote 12 18-bit characters.
    return 12; // We consumed 12 bytes.
  }
  /// We do not have a fast path available, or the fast path is unimportant, so we fallback.
  const uint8_t idx =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][0];

  const uint8_t consumed =
      simdutf::tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][1];
  // this indicates an invalid input:
  if(idx >= 64) { return consumed; }
  // Here we should have (idx < 64), if not, there is a bug in the validation or elsewhere.
  // SIX (6) input code-code units
  // this is a relatively easy scenario
  // we process SIX (6) input code-code units. The max length in bytes of six code
  // code units spanning between 1 and 2 bytes each is 12 bytes.
  // Converts 6 1-2 byte UTF-8 characters to 6 UTF-16 characters.
  // This is a relatively easy scenario
  // we process SIX (6) input code-code units. The max length in bytes of six code
  // code units spanning between 1 and 2 bytes each is 12 bytes.
  uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[idx]));
  // Shuffle
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 110aaaaa 10bbbbbb
  uint16x8_t perm = vreinterpretq_u16_u8(vqtbl1q_u8(in, sh));
  // Mask
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 00000000 00bbbbbb
  uint16x8_t ascii = vandq_u16(perm, vmovq_n_u16(0x7f)); // 6 or 7 bits
  // 1 byte: 00000000 00000000
  // 2 byte: 000aaaaa 00000000
  uint16x8_t highbyte = vandq_u16(perm, vmovq_n_u16(0x1f00)); // 5 bits
  // Combine with a shift right accumulate
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 00000aaa aabbbbbb
  uint16x8_t composed = vsraq_n_u16(ascii, highbyte, 2);
  // writing 8 bytes even though we only care about the first 6 bytes.
  uint8x8_t latin1_packed = vmovn_u16(composed);
  vst1_u8(reinterpret_cast<uint8_t*>(latin1_output), latin1_packed);
  latin1_output += 6; // We wrote 6 bytes.
  return consumed;
}

