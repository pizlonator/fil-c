
template <endianness big_endian>
std::pair<const char16_t*, char*> arm_convert_utf16_to_latin1(const char16_t* buf, size_t len, char* latin1_output) {
  const char16_t* end = buf + len;
  while (end - buf >= 8) {
    uint16x8_t in = vld1q_u16(reinterpret_cast<const uint16_t *>(buf));
    if (!match_system(big_endian)) { in = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(in))); }
    if (vmaxvq_u16(in) <= 0xff) {
        // 1. pack the bytes
        uint8x8_t latin1_packed = vmovn_u16(in);
        // 2. store (8 bytes)
        vst1_u8(reinterpret_cast<uint8_t*>(latin1_output), latin1_packed);
        // 3. adjust pointers
        buf += 8;
        latin1_output += 8;
    } else {
      return std::make_pair(nullptr, reinterpret_cast<char*>(latin1_output));
    }
  } // while
  return std::make_pair(buf, latin1_output);
}

template <endianness big_endian>
std::pair<result, char*> arm_convert_utf16_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) {
  const char16_t* start = buf;
  const char16_t* end = buf + len;
  while (end - buf >= 8) {
    uint16x8_t in = vld1q_u16(reinterpret_cast<const uint16_t *>(buf));
    if (!match_system(big_endian)) { in = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(in))); }
    if (vmaxvq_u16(in) <= 0xff) {
        // 1. pack the bytes
        uint8x8_t latin1_packed = vmovn_u16(in);
        // 2. store (8 bytes)
        vst1_u8(reinterpret_cast<uint8_t*>(latin1_output), latin1_packed);
        // 3. adjust pointers
        buf += 8;
        latin1_output += 8;
    } else {
      // Let us do a scalar fallback.
      for(int k = 0; k < 8; k++) {
        uint16_t word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if(word <= 0xff) {
          *latin1_output++ = char(word);
        } else {
          return std::make_pair(result(error_code::TOO_LARGE, buf - start + k), latin1_output);
        }
      }
    }
  } // while
  return std::make_pair(result(error_code::SUCCESS, buf - start), latin1_output);
}