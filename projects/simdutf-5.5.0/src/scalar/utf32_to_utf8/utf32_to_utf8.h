#ifndef SIMDUTF_UTF32_TO_UTF8_H
#define SIMDUTF_UTF32_TO_UTF8_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_utf8 {

inline size_t convert(const char32_t* buf, size_t len, char* utf8_output) {
  const uint32_t *data = reinterpret_cast<const uint32_t *>(buf);
  size_t pos = 0;
  char* start{utf8_output};
  while (pos < len) {
    // try to convert the next block of 2 ASCII characters
    if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that they are ascii
      uint64_t v;
      ::memcpy(&v, data + pos, sizeof(uint64_t));
      if ((v & 0xFFFFFF80FFFFFF80) == 0) {
        *utf8_output++ = char(buf[pos]);
				*utf8_output++ = char(buf[pos+1]);
        pos += 2;
        continue;
      }
    }
    uint32_t word = data[pos];
    if((word & 0xFFFFFF80)==0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if((word & 0xFFFFF800)==0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word>>6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if((word & 0xFFFF0000)==0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
			if (word >= 0xD800 && word <= 0xDFFF) { return 0; }
      *utf8_output++ = char((word>>12) | 0b11100000);
      *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
			if (word > 0x10FFFF) { return 0; }
      *utf8_output++ = char((word>>18) | 0b11110000);
      *utf8_output++ = char(((word>>12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos ++;
    }
  }
  return utf8_output - start;
}

inline result convert_with_errors(const char32_t* buf, size_t len, char* utf8_output) {
  const uint32_t *data = reinterpret_cast<const uint32_t *>(buf);
  size_t pos = 0;
  char* start{utf8_output};
  while (pos < len) {
    // try to convert the next block of 2 ASCII characters
    if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that they are ascii
      uint64_t v;
      ::memcpy(&v, data + pos, sizeof(uint64_t));
      if ((v & 0xFFFFFF80FFFFFF80) == 0) {
        *utf8_output++ = char(buf[pos]);
				*utf8_output++ = char(buf[pos+1]);
        pos += 2;
        continue;
      }
    }
    uint32_t word = data[pos];
    if((word & 0xFFFFFF80)==0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if((word & 0xFFFFF800)==0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word>>6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if((word & 0xFFFF0000)==0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
			if (word >= 0xD800 && word <= 0xDFFF) { return result(error_code::SURROGATE, pos); }
      *utf8_output++ = char((word>>12) | 0b11100000);
      *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
			if (word > 0x10FFFF) { return result(error_code::TOO_LARGE, pos); }
      *utf8_output++ = char((word>>18) | 0b11110000);
      *utf8_output++ = char(((word>>12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos ++;
    }
  }
  return result(error_code::SUCCESS, utf8_output - start);
}

} // utf32_to_utf8 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif