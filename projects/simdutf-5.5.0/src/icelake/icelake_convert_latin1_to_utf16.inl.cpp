// file included directly
template <endianness big_endian>
size_t icelake_convert_latin1_to_utf16(const char *latin1_input, size_t len,
                                       char16_t *utf16_output) {
  size_t rounded_len = len & ~0x1F; // Round down to nearest multiple of 32

  __m512i byteflip = _mm512_setr_epi64(0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809);
  for (size_t i = 0; i < rounded_len; i += 32) {
    // Load 32 Latin1 characters into a 256-bit register
    __m256i in = _mm256_loadu_si256((__m256i *)&latin1_input[i]);
    // Zero extend each set of 8 Latin1 characters to 32 16-bit integers
    __m512i out = _mm512_cvtepu8_epi16(in);
    if (big_endian) {
      out = _mm512_shuffle_epi8(out, byteflip);
    }
    // Store the results back to memory
    _mm512_storeu_si512((__m512i *)&utf16_output[i], out);
  }
  if (rounded_len != len) {
    uint32_t mask = uint32_t(1 << (len - rounded_len)) - 1;
    __m256i in = _mm256_maskz_loadu_epi8(mask, latin1_input + rounded_len);

    // Zero extend each set of 8 Latin1 characters to 32 16-bit integers
    __m512i out = _mm512_cvtepu8_epi16(in);
    if (big_endian) {
      out = _mm512_shuffle_epi8(out, byteflip);
    }
    // Store the results back to memory
    _mm512_mask_storeu_epi16(utf16_output + rounded_len, mask, out);
  }

  return len;
}
