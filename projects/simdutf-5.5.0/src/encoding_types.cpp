
namespace simdutf {
bool match_system(endianness e) {
#if SIMDUTF_IS_BIG_ENDIAN
    return e == endianness::BIG;
#else
    return e == endianness::LITTLE;
#endif
}

std::string to_string(encoding_type bom) {
  switch (bom) {
      case UTF16_LE:     return "UTF16 little-endian";
      case UTF16_BE:     return "UTF16 big-endian";
      case UTF32_LE:     return "UTF32 little-endian";
      case UTF32_BE:     return "UTF32 big-endian";
      case UTF8:         return "UTF8";
      case unspecified:  return "unknown";
      default:           return "error";
  }
}

namespace BOM {
// Note that BOM for UTF8 is discouraged.
encoding_type check_bom(const uint8_t* byte, size_t length) {
        if (length >= 2 && byte[0] == 0xff and byte[1] == 0xfe) {
            if (length >= 4 && byte[2] == 0x00 and byte[3] == 0x0) {
                return encoding_type::UTF32_LE;
            } else {
                return encoding_type::UTF16_LE;
            }
        } else if (length >= 2 && byte[0] == 0xfe and byte[1] == 0xff) {
            return encoding_type::UTF16_BE;
        } else if (length >= 4 && byte[0] == 0x00 and byte[1] == 0x00 and byte[2] == 0xfe and byte[3] == 0xff) {
            return encoding_type::UTF32_BE;
        } else if (length >= 4 && byte[0] == 0xef and byte[1] == 0xbb and byte[2] == 0xbf) {
            return encoding_type::UTF8;
        }
        return encoding_type::unspecified;
    }

encoding_type check_bom(const char* byte, size_t length) {
      return check_bom(reinterpret_cast<const uint8_t*>(byte), length);
 }

 size_t bom_byte_size(encoding_type bom) {
        switch (bom) {
            case UTF16_LE:     return 2;
            case UTF16_BE:     return 2;
            case UTF32_LE:     return 4;
            case UTF32_BE:     return 4;
            case UTF8:         return 3;
            case unspecified:  return 0;
            default:           return 0;
        }
}

}
}
