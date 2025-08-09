#ifndef SIMDUTF_ERROR_H
#define SIMDUTF_ERROR_H
namespace simdutf {

enum error_code {
  SUCCESS = 0,
  HEADER_BITS,  // Any byte must have fewer than 5 header bits.
  TOO_SHORT,    // The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length
                // This is also the error when the input is truncated.
  TOO_LONG,     // We either have too many consecutive continuation bytes or the string starts with a continuation byte.
  OVERLONG,     // The decoded character must be above U+7F for two-byte characters, U+7FF for three-byte characters,
                // and U+FFFF for four-byte characters.
  TOO_LARGE,    // The decoded character must be less than or equal to U+10FFFF,less than or equal than U+7F for ASCII OR less than equal than U+FF for Latin1
  SURROGATE,    // The decoded character must be not be in U+D800...DFFF (UTF-8 or UTF-32) OR
                // a high surrogate must be followed by a low surrogate and a low surrogate must be preceded by a high surrogate (UTF-16) OR
                // there must be no surrogate at all (Latin1)
  INVALID_BASE64_CHARACTER, // Found a character that cannot be part of a valid base64 string.
  BASE64_INPUT_REMAINDER, // The base64 input terminates with a single character, excluding padding (=).
  OUTPUT_BUFFER_TOO_SMALL, // The provided buffer is too small.
  OTHER         // Not related to validation/transcoding.
};

struct result {
  error_code error;
  size_t count;     // In case of error, indicates the position of the error. In case of success, indicates the number of code units validated/written.

  simdutf_really_inline result() : error{error_code::SUCCESS}, count{0} {}

  simdutf_really_inline result(error_code _err, size_t _pos) : error{_err}, count{_pos} {}
};

}
#endif