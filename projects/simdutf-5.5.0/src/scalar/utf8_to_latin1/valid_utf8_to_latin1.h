#ifndef SIMDUTF_VALID_UTF8_TO_LATIN1_H
#define SIMDUTF_VALID_UTF8_TO_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_latin1 {

inline size_t convert_valid(const char* buf, size_t len, char* latin_output) {
 const uint8_t *data = reinterpret_cast<const uint8_t *>(buf);

  size_t pos = 0;
  char* start{latin_output};

  while (pos < len) {
    // try to convert the next block of 16 ASCII bytes
    if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that they are ascii
      uint64_t v1;
      ::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2}; // We are only interested in these bits: 1000 1000 1000 1000, so it makes sense to concatenate everything
      if ((v & 0x8080808080808080) == 0) { // if NONE of these are set, e.g. all of them are zero, then everything is ASCII
        size_t final_pos = pos + 16;
        while(pos < final_pos) {
          *latin_output++ = char(buf[pos]);
          pos++;
        }
        continue;
      }
    }

    // suppose it is not an all ASCII byte sequence
    uint8_t leading_byte = data[pos]; // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *latin_output++ = char(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) { // the first three bits indicate:
      // We have a two-byte UTF-8
      if(pos + 1 >= len) { break; } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) { return 0; } // checks if the next byte is a valid continuation byte in UTF-8. A valid continuation byte starts with 10.
      // range check -
      uint32_t code_point = (leading_byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111); // assembles the Unicode code point from the two bytes. It does this by discarding the leading 110 and 10 bits from the two bytes, shifting the remaining bits of the first byte, and then combining the results with a bitwise OR operation.
      *latin_output++ = char(code_point);
      pos += 2;
    } else {
      // we may have a continuation but we do not do error checking
      return 0;
    }
  }
  return latin_output - start;
}

} // utf8_to_latin1 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
