#ifndef SIMDUTF_IMPLEMENTATION_H
#define SIMDUTF_IMPLEMENTATION_H
#include <string>
#if !defined(SIMDUTF_NO_THREADS)
#include <atomic>
#endif
#include <vector>
#include <tuple>
#include "simdutf/common_defs.h"
#include "simdutf/internal/isadetection.h"


namespace simdutf {

/**
 * Autodetect the encoding of the input, a single encoding is recommended.
 * E.g., the function might return simdutf::encoding_type::UTF8,
 * simdutf::encoding_type::UTF16_LE, simdutf::encoding_type::UTF16_BE, or
 * simdutf::encoding_type::UTF32_LE.
 *
 * @param input the string to analyze.
 * @param length the length of the string in bytes.
 * @return the detected encoding type
 */
simdutf_warn_unused simdutf::encoding_type autodetect_encoding(const char * input, size_t length) noexcept;
simdutf_really_inline simdutf_warn_unused simdutf::encoding_type autodetect_encoding(const uint8_t * input, size_t length) noexcept {
  return autodetect_encoding(reinterpret_cast<const char *>(input), length);
}

/**
 * Autodetect the possible encodings of the input in one pass.
 * E.g., if the input might be UTF-16LE or UTF-8, this function returns
 * the value (simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE).
 *
 * Overridden by each implementation.
 *
 * @param input the string to analyze.
 * @param length the length of the string in bytes.
 * @return the detected encoding type
 */
simdutf_warn_unused int detect_encodings(const char * input, size_t length) noexcept;
simdutf_really_inline simdutf_warn_unused int detect_encodings(const uint8_t * input, size_t length) noexcept {
  return detect_encodings(reinterpret_cast<const char *>(input), length);
}

/**
 * Validate the UTF-8 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf8_with_errors.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid UTF-8.
 */
simdutf_warn_unused bool validate_utf8(const char *buf, size_t len) noexcept;

/**
 * Validate the UTF-8 string and stop on error.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_utf8_with_errors(const char *buf, size_t len) noexcept;

/**
 * Validate the ASCII string.
 *
 * Overridden by each implementation.
 *
 * @param buf the ASCII string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid ASCII.
 */
simdutf_warn_unused bool validate_ascii(const char *buf, size_t len) noexcept;

/**
 * Validate the ASCII string and stop on error. It might be faster than
 * validate_utf8 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * @param buf the ASCII string to validate.
 * @param len the length of the string in bytes.
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_ascii_with_errors(const char *buf, size_t len) noexcept;

/**
 * Using native endianness; Validate the UTF-16 string.
 * This function may be best when you expect the input to be almost always valid.
 * Otherwise, consider using validate_utf16_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return true if and only if the string is valid UTF-16.
 */
simdutf_warn_unused bool validate_utf16(const char16_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-16LE string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf16le_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16LE string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return true if and only if the string is valid UTF-16LE.
 */
simdutf_warn_unused bool validate_utf16le(const char16_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-16BE string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf16be_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16BE string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return true if and only if the string is valid UTF-16BE.
 */
simdutf_warn_unused bool validate_utf16be(const char16_t *buf, size_t len) noexcept;

/**
 * Using native endianness; Validate the UTF-16 string and stop on error.
 * It might be faster than validate_utf16 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_utf16_with_errors(const char16_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-16LE string and stop on error. It might be faster than
 * validate_utf16le when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16LE string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_utf16le_with_errors(const char16_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-16BE string and stop on error. It might be faster than
 * validate_utf16be when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16BE string to validate.
 * @param len the length of the string in number of 2-byte code units (char16_t).
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_utf16be_with_errors(const char16_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-32 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf32_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units (char32_t).
 * @return true if and only if the string is valid UTF-32.
 */
simdutf_warn_unused bool validate_utf32(const char32_t *buf, size_t len) noexcept;

/**
 * Validate the UTF-32 string and stop on error. It might be faster than
 * validate_utf32 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units (char32_t).
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
 */
simdutf_warn_unused result validate_utf32_with_errors(const char32_t *buf, size_t len) noexcept;

/**
 * Convert Latin1 string into UTF8 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf8_output   the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf8(const char * input, size_t length, char* utf8_output) noexcept;

/**
 * Convert Latin1 string into UTF8 string with output limit.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf8_output  	the pointer to buffer that can hold conversion result
 * @param utf8_len      the maximum output length
 * @return the number of written char; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf8_safe(const char * input, size_t length, char* utf8_output, size_t utf8_len) noexcept;

/**
 * Convert possibly Latin1 string into UTF-16LE string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1  string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf16le(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert Latin1 string into UTF-16BE string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf16be(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert Latin1 string into UTF-32 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf32(const char * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-8 string into latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param latin1_output  the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if the input was not valid UTF-8 string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf8_to_latin1(const char * input, size_t length, char* latin1_output) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-8 string into a UTF-16 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Using native endianness, convert a Latin1 string into a UTF-16 string.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t.
 */
simdutf_warn_unused size_t convert_latin1_to_utf16(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-16LE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-16BE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_output) noexcept;


  /**
   * Convert possibly broken UTF-8 string into latin1 string with errors.
   * If the string cannot be represented as Latin1, an error
   * code is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused result convert_utf8_to_latin1_with_errors(const char * input, size_t length, char* latin1_output) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-8 string into UTF-16
 * string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-16LE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16le_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-16BE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16be_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_output) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
 */
simdutf_warn_unused result convert_utf8_to_utf32_with_errors(const char * input, size_t length, char32_t* utf32_output) noexcept;

/**
 * Convert valid UTF-8 string into latin1 string.
 *
 * This function assumes that the input string is valid UTF-8 and that it can be represented as Latin1.
 * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
 *
 * This function is for expert users only and not part of our public API. Use convert_utf8_to_latin1 instead.
 * The function may be removed from the library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param latin1_output  the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_valid_utf8_to_latin1(const char * input, size_t length, char* latin1_output) noexcept;


/**
 * Using native endianness, convert valid UTF-8 string into a UTF-16 string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16(const char * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert valid UTF-8 string into UTF-16LE string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert valid UTF-8 string into UTF-16BE string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert valid UTF-8 string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_buffer) noexcept;


/**
 * Return the number of bytes that this Latin1 string would require in UTF-8 format.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string bytes
 * @return the number of bytes required to encode the Latin1 string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_latin1(const char * input, size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-8 string would require in Latin1 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in byte
 * @return the number of bytes required to encode the UTF-8 string as Latin1
 */
simdutf_warn_unused size_t latin1_length_from_utf8(const char * input, size_t length) noexcept;

/**
 * Compute the number of 2-byte code units that this UTF-8 string would require in UTF-16LE format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-8 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char16_t code units required to encode the UTF-8 string as UTF-16LE
 */
simdutf_warn_unused size_t utf16_length_from_utf8(const char * input, size_t length) noexcept;

/**
 * Compute the number of 4-byte code units that this UTF-8 string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf8
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-8 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char32_t code units required to encode the UTF-8 string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf8(const char * input, size_t length) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;



/**
 * Using native endianness, convert possibly broken UTF-16 string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16 string  or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert possibly broken UTF-16LE string into Latin1 string.
 * If the string cannot be represented as Latin1, an error
 * is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16le_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16BE string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16be_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;


/**
 * Convert possibly broken UTF-16LE string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16le_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16be_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16_to_latin1_with_errors(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert possibly broken UTF-16LE string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16le_to_latin1_with_errors(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into Latin1 string.
 * If the string cannot be represented as Latin1, an error
 * is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16be_to_latin1_with_errors(const char16_t * input, size_t length, char* latin1_buffer) noexcept;


/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16_to_utf8_with_errors(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert possibly broken UTF-16LE string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16le_to_utf8_with_errors(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf16be_to_utf8_with_errors(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert valid UTF-16 string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16LE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;


/**
 * Using native endianness, convert UTF-16 string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16 and that it can be represented as Latin1.
 * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
 *
 * This function is for expert users only and not part of our public API. Use convert_utf16_to_latin1 instead.
 * The function may be removed from the library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert valid UTF-16LE string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16LE and that it can be represented as Latin1.
 * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
 *
 * This function is for expert users only and not part of our public API. Use convert_utf16le_to_latin1 instead.
 * The function may be removed from the library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert valid UTF-16BE string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16BE and that it can be represented as Latin1.
 * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
 *
 * This function is for expert users only and not part of our public API. Use convert_utf16be_to_latin1 instead.
 * The function may be removed from the library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) noexcept;


/**
 * Convert valid UTF-16LE string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16LE and that it can be represented as Latin1.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert valid UTF-16BE string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16BE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-16LE string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16le_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE string
 */
simdutf_warn_unused size_t convert_utf16be_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into
 * UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
 */
simdutf_warn_unused result convert_utf16_to_utf32_with_errors(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-16LE string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
 */
simdutf_warn_unused result convert_utf16le_to_utf32_with_errors(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-16BE string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
 */
simdutf_warn_unused result convert_utf16be_to_utf32_with_errors(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Using native endianness, convert valid UTF-16 string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16 (native endianness).
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert valid UTF-16LE string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16LE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert valid UTF-16BE string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16LE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) noexcept;


/*
 * Compute the number of bytes that this UTF-16LE/BE string would require in Latin1 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as Latin1
 */
simdutf_warn_unused size_t latin1_length_from_utf16(size_t length) noexcept;


/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16(const char16_t * input, size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-16LE string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16le(const char16_t * input, size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-16BE string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16BE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16be(const char16_t * input, size_t length) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf8(const char32_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf32_to_utf8_with_errors(const char32_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Convert valid UTF-32 string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf8(const char32_t * input, size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-32 string into a UTF-16 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-16LE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16le(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf32_to_latin1(const char32_t * input, size_t length, char* latin1_buffer) noexcept;


/**
 * Convert possibly broken UTF-32 string into Latin1 string and stop on error.
 * If the string cannot be represented as Latin1, an error is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
 */
simdutf_warn_unused result convert_utf32_to_latin1_with_errors(const char32_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert valid UTF-32 string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-32 and that it can be represented as Latin1.
 * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
 *
 * This function is for expert users only and not part of our public API. Use convert_utf32_to_latin1 instead.
 * The function may be removed from the library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_latin1(const char32_t * input, size_t length, char* latin1_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-16BE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16be(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-32 string into UTF-16
 * string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16_with_errors(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-16LE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16le_with_errors(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-16BE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16be_with_errors(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Using native endianness, convert valid UTF-32 string into a UTF-16 string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert valid UTF-32 string into UTF-16LE string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16le(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert valid UTF-32 string into UTF-16BE string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold the conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16be(const char32_t * input, size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Change the endianness of the input. Can be used to go from UTF-16LE to UTF-16BE or
 * from UTF-16BE to UTF-16LE.
 *
 * This function does not validate the input.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param output        the pointer to buffer that can hold the conversion result
 */
void change_endianness_utf16(const char16_t * input, size_t length, char16_t * output) noexcept;

/**
 * Compute the number of bytes that this UTF-32 string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-32 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf32(const char32_t * input, size_t length) noexcept;

/**
 * Compute the number of two-byte code units that this UTF-32 string would require in UTF-16 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-32 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-16
 */
simdutf_warn_unused size_t utf16_length_from_utf32(const char32_t * input, size_t length) noexcept;

/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf16.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16(const char16_t * input, size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-16LE string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf16le.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16le(const char16_t * input, size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-16BE string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf16be.
 *
 * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16BE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16be(const char16_t * input, size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16 (native endianness).
 * It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16(const char16_t * input, size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16LE.
 * It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16le(const char16_t * input, size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16BE.
 * It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16be(const char16_t * input, size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-8.
 * It is acceptable to pass invalid UTF-8 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf8(const char * input, size_t length) noexcept;

/**
 * Given a valid UTF-8 string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated (or partial),
 * then it returns a shorter length (shorter by 1 to 3 bytes) so that the short UTF-8
 * strings only contain complete characters. If there is no truncated character,
 * the original length is returned.
 *
 * This function assumes that the input string is valid UTF-8, but possibly truncated.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in bytes, possibly shorter by 1 to 3 bytes
 */
simdutf_warn_unused size_t trim_partial_utf8(const char *input, size_t length);

/**
 * Given a valid UTF-16BE string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated (or partial),
 * then it returns a shorter length (shorter by 1 unit) so that the short UTF-16BE
 * strings only contain complete characters. If there is no truncated character,
 * the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16BE, but possibly truncated.
 *
 * @param input         the UTF-16BE string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in bytes, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16be(const char16_t* input, size_t length);

/**
 * Given a valid UTF-16LE string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated (or partial),
 * then it returns a shorter length (shorter by 1 unit) so that the short UTF-16LE
 * strings only contain complete characters. If there is no truncated character,
 * the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16LE, but possibly truncated.
 *
 * @param input         the UTF-16LE string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in unit, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16le(const char16_t* input, size_t length);


/**
 * Given a valid UTF-16 string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated (or partial),
 * then it returns a shorter length (shorter by 1 unit) so that the short UTF-16
 * strings only contain complete characters. If there is no truncated character,
 * the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16, but possibly truncated.
 * We use the native endianness.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in unit, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16(const char16_t* input, size_t length);

// base64_options are used to specify the base64 encoding options.
using base64_options = uint64_t;
enum : base64_options {
  base64_default = 0, /* standard base64 format (with padding) */
  base64_url = 1, /* base64url format (no padding) */
  base64_reverse_padding = 2, /* modifier for base64_default and base64_url */
  base64_default_no_padding = base64_default | base64_reverse_padding, /* standard base64 format without padding */
  base64_url_with_padding = base64_url | base64_reverse_padding, /* base64url with padding */
};

/**
 * Provide the maximal binary length in bytes given the base64 input.
 * In general, if the input contains ASCII spaces, the result will be less than
 * the maximum length.
 *
 * @param input         the base64 input to process
 * @param length        the length of the base64 input in bytes
 * @return maximum number of binary bytes
 */
simdutf_warn_unused size_t maximal_binary_length_from_base64(const char * input, size_t length) noexcept;

/**
 * Provide the maximal binary length in bytes given the base64 input.
 * In general, if the input contains ASCII spaces, the result will be less than
 * the maximum length.
 *
 * @param input         the base64 input to process, in ASCII stored as 16-bit units
 * @param length        the length of the base64 input in 16-bit units
 * @return maximal number of binary bytes
 */
simdutf_warn_unused size_t maximal_binary_length_from_base64(const char16_t * input, size_t length) noexcept;

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it will
 * ignore any ASCII spaces in the input. You may provide a padded input (with one or two
 * equal signs at the end) or an unpadded input (without any equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. There are two possible reasons for
 * failure: the input contains a number of base64 characters that when divided by 4, leaves
 * a single remainder character (BASE64_INPUT_REMAINDER), or the input contains a character
 * that is not a valid base64 character (INVALID_BASE64_CHARACTER).
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the input
 * where the invalid character was found. When the error is BASE64_INPUT_REMAINDER, then
 * r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and `/` as part of its alphabet.
 * The URL option (simdutf::base64_url) expects the characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding characters at the end of the input.
 * If there are any padding characters, the total number of characters (excluding spaces but including padding characters) must be divisible by four.
 *
 * You should call this function with a buffer that is at least maximal_binary_length_from_base64(input, length) bytes long.
 * If you fail to provide that much space, the function may cause a buffer overflow.
 *
 * @param input         the base64 string to process
 * @param length        the length of the string in bytes
 * @param output        the pointer to buffer that can hold the conversion result (should be at least maximal_binary_length_from_base64(input, length) bytes long).
 * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in bytes) if any, or the number of bytes written if successful.
 */
simdutf_warn_unused result base64_to_binary(const char * input, size_t length, char* output, base64_options options = base64_default) noexcept;

/**
 * Provide the base64 length in bytes given the length of a binary input.
 *
 * @param length        the length of the input in bytes
 * @return number of base64 bytes
 */
simdutf_warn_unused size_t base64_length_from_binary(size_t length, base64_options options = base64_default) noexcept;

/**
 * Convert a binary input to a base64 output.
 *
 * The default option (simdutf::base64_default) uses the characters `+` and `/` as part of its alphabet.
 * Further, it adds padding (`=`) at the end of the output to ensure that the output length is a multiple of four.
 *
 * The URL option (simdutf::base64_url) uses the characters `-` and `_` as part of its alphabet. No padding
 * is added at the end of the output.
 *
 * This function always succeeds.
 *
 * @param input         the binary to process
 * @param length        the length of the input in bytes
 * @param output        the pointer to buffer that can hold the conversion result (should be at least base64_length_from_binary(length) bytes long)
 * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
 * @return number of written bytes, will be equal to base64_length_from_binary(length, options)
 */
size_t binary_to_base64(const char * input, size_t length, char* output, base64_options options = base64_default) noexcept;

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it will
 * ignore any ASCII spaces in the input. You may provide a padded input (with one or two
 * equal signs at the end) or an unpadded input (without any equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. There are two possible reasons for
 * failure: the input contains a number of base64 characters that when divided by 4, leaves
 * a single remainder character (BASE64_INPUT_REMAINDER), or the input contains a character
 * that is not a valid base64 character (INVALID_BASE64_CHARACTER).
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the input
 * where the invalid character was found. When the error is BASE64_INPUT_REMAINDER, then
 * r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and `/` as part of its alphabet.
 * The URL option (simdutf::base64_url) expects the characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding characters at the end of the input.
 * If there are any padding characters, the total number of characters (excluding spaces but including padding characters) must be divisible by four.
 *
 * You should call this function with a buffer that is at least maximal_binary_length_from_utf6_base64(input, length) bytes long.
 * If you fail to provide that much space, the function may cause a buffer overflow.
 *
 * @param input         the base64 string to process, in ASCII stored as 16-bit units
 * @param length        the length of the string in 16-bit units
 * @param output        the pointer to buffer that can hold the conversion result (should be at least maximal_binary_length_from_base64(input, length) bytes long).
 * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and position of the INVALID_BASE64_CHARACTER error (in the input in units) if any, or the number of bytes written if successful.
 */
simdutf_warn_unused result base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options = base64_default)  noexcept;

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it will
 * ignore any ASCII spaces in the input. You may provide a padded input (with one or two
 * equal signs at the end) or an unpadded input (without any equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. There are three possible reasons for
 * failure: the input contains a number of base64 characters that when divided by 4, leaves
 * a single remainder character (BASE64_INPUT_REMAINDER), the input contains a character
 * that is not a valid base64 character (INVALID_BASE64_CHARACTER), or the output buffer
 * is too small (OUTPUT_BUFFER_TOO_SMALL).
 *
 * When OUTPUT_BUFFER_TOO_SMALL, we return both the number of bytes written
 * and the number of units processed, see description of the parameters and returned value.
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the input
 * where the invalid character was found. When the error is BASE64_INPUT_REMAINDER, then
 * r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and `/` as part of its alphabet.
 * The URL option (simdutf::base64_url) expects the characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding characters at the end of the input.
 * If there are any padding characters, the total number of characters (excluding spaces but including padding characters) must be divisible by four.
 *
 * The INVALID_BASE64_CHARACTER cases are considered fatal and you are expected to discard
 * the output.
 *
 * @param input         the base64 string to process, in ASCII stored as 8-bit or 16-bit units
 * @param length        the length of the string in 8-bit or 16-bit units.
 * @param output        the pointer to buffer that can hold the conversion result.
 * @param outlen        the number of bytes that can be written in the output buffer. Upon return, it is modified to reflect how many bytes were written.
 * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
 * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and position of the INVALID_BASE64_CHARACTER error (in the input in units) if any, or the number of units processed if successful.
 */
simdutf_warn_unused result base64_to_binary_safe(const char * input, size_t length, char* output, size_t& outlen, base64_options options = base64_default) noexcept;
simdutf_warn_unused result base64_to_binary_safe(const char16_t * input, size_t length, char* output, size_t& outlen, base64_options options = base64_default) noexcept;

/**
 * An implementation of simdutf for a particular CPU architecture.
 *
 * Also used to maintain the currently active implementation. The active implementation is
 * automatically initialized on first use to the most advanced implementation supported by the host.
 */
class implementation {
public:

  /**
   * The name of this implementation.
   *
   *     const implementation *impl = simdutf::active_implementation;
   *     cout << "simdutf is optimized for " << impl->name() << "(" << impl->description() << ")" << endl;
   *
   * @return the name of the implementation, e.g. "haswell", "westmere", "arm64"
   */
  virtual std::string name() const { return std::string(_name); }

  /**
   * The description of this implementation.
   *
   *     const implementation *impl = simdutf::active_implementation;
   *     cout << "simdutf is optimized for " << impl->name() << "(" << impl->description() << ")" << endl;
   *
   * @return the name of the implementation, e.g. "haswell", "westmere", "arm64"
   */
  virtual std::string description() const { return std::string(_description); }

  /**
   * The instruction sets this implementation is compiled against
   * and the current CPU match. This function may poll the current CPU/system
   * and should therefore not be called too often if performance is a concern.
   *
   *
   * @return true if the implementation can be safely used on the current system (determined at runtime)
   */
  bool supported_by_runtime_system() const;

  /**
   * This function will try to detect the encoding
   * @param input the string to identify
   * @param length the length of the string in bytes.
   * @return the encoding type detected
   */
  virtual encoding_type autodetect_encoding(const char * input, size_t length) const noexcept;

  /**
   * This function will try to detect the possible encodings in one pass
   * @param input the string to identify
   * @param length the length of the string in bytes.
   * @return the encoding type detected
   */
  virtual int detect_encodings(const char * input, size_t length) const noexcept = 0;

  /**
   * @private For internal implementation use
   *
   * The instruction sets this implementation is compiled against.
   *
   * @return a mask of all required `internal::instruction_set::` values
   */
  virtual uint32_t required_instruction_sets() const { return _required_instruction_sets; }


  /**
   * Validate the UTF-8 string.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-8 string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid UTF-8.
   */
  simdutf_warn_unused virtual bool validate_utf8(const char *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-8 string and stop on errors.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-8 string to validate.
   * @param len the length of the string in bytes.
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result validate_utf8_with_errors(const char *buf, size_t len) const noexcept = 0;

  /**
   * Validate the ASCII string.
   *
   * Overridden by each implementation.
   *
   * @param buf the ASCII string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid ASCII.
   */
  simdutf_warn_unused virtual bool validate_ascii(const char *buf, size_t len) const noexcept = 0;

  /**
   * Validate the ASCII string and stop on error.
   *
   * Overridden by each implementation.
   *
   * @param buf the ASCII string to validate.
   * @param len the length of the string in bytes.
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result validate_ascii_with_errors(const char *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16LE string.This function may be best when you expect
   * the input to be almost always valid. Otherwise, consider using
   * validate_utf16le_with_errors.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16LE string to validate.
   * @param len the length of the string in number of 2-byte code units (char16_t).
   * @return true if and only if the string is valid UTF-16LE.
   */
  simdutf_warn_unused virtual bool validate_utf16le(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16BE string. This function may be best when you expect
   * the input to be almost always valid. Otherwise, consider using
   * validate_utf16be_with_errors.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16BE string to validate.
   * @param len the length of the string in number of 2-byte code units (char16_t).
   * @return true if and only if the string is valid UTF-16BE.
   */
  simdutf_warn_unused virtual bool validate_utf16be(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16LE string and stop on error.  It might be faster than
 * validate_utf16le when an error is expected to occur early.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16LE string to validate.
   * @param len the length of the string in number of 2-byte code units (char16_t).
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result validate_utf16le_with_errors(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16BE string and stop on error. It might be faster than
   * validate_utf16be when an error is expected to occur early.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16BE string to validate.
   * @param len the length of the string in number of 2-byte code units (char16_t).
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result validate_utf16be_with_errors(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-32 string.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-32 string to validate.
   * @param len the length of the string in number of 4-byte code units (char32_t).
   * @return true if and only if the string is valid UTF-32.
   */
  simdutf_warn_unused virtual bool validate_utf32(const char32_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-32 string and stop on error.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-32 string to validate.
   * @param len the length of the string in number of 4-byte code units (char32_t).
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result validate_utf32_with_errors(const char32_t *buf, size_t len) const noexcept = 0;

  /**
   * Convert Latin1 string into UTF8 string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf8_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_latin1_to_utf8(const char * input, size_t length, char* utf8_output) const noexcept = 0;

  /**
   * Convert possibly Latin1 string into UTF-16LE string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1  string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_latin1_to_utf16le(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert Latin1 string into UTF-16BE string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_latin1_to_utf16be(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert Latin1 string into UTF-32 string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char32_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_latin1_to_utf32(const char * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

 /**
   * Convert possibly broken UTF-8 string into latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if the input was not valid UTF-8 string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t convert_utf8_to_latin1(const char * input, size_t length, char* latin1_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into latin1 string with errors.
   * If the string cannot be represented as Latin1, an error
   * code is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_latin1_with_errors(const char * input, size_t length, char* latin1_output) const noexcept = 0;

  /**
   * Convert valid UTF-8 string into latin1 string.
   *
   * This function assumes that the input string is valid UTF-8 and that it can be represented as Latin1.
   * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
   *
   * This function is for expert users only and not part of our public API. Use convert_utf8_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if the input was not valid UTF-8 string
   */
  simdutf_warn_unused virtual size_t convert_valid_utf8_to_latin1(const char * input, size_t length, char* latin1_output) const noexcept = 0;


  /**
   * Convert possibly broken UTF-8 string into UTF-16LE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
   */
  simdutf_warn_unused virtual size_t convert_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16BE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
   */
  simdutf_warn_unused virtual size_t convert_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16LE string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_utf16le_with_errors(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16BE string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of code units validated if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_utf16be_with_errors(const char * input, size_t length, char16_t* utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8 string
   */
  simdutf_warn_unused virtual size_t convert_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-32 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_buffer  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_utf32_with_errors(const char * input, size_t length, char32_t* utf32_output) const noexcept = 0;

  /**
   * Convert valid UTF-8 string into UTF-16LE string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t
   */
  simdutf_warn_unused virtual size_t convert_valid_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

/**
   * Convert valid UTF-8 string into UTF-16BE string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t
   */
  simdutf_warn_unused virtual size_t convert_valid_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-8 string into UTF-32 string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char32_t
   */
  simdutf_warn_unused virtual size_t convert_valid_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Compute the number of 2-byte code units that this UTF-8 string would require in UTF-16LE format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return the number of char16_t code units required to encode the UTF-8 string as UTF-16LE
   */
  simdutf_warn_unused virtual size_t utf16_length_from_utf8(const char * input, size_t length) const noexcept = 0;

   /**
   * Compute the number of 4-byte code units that this UTF-8 string would require in UTF-32 format.
   *
   * This function is equivalent to count_utf8. It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
   *
   * This function does not validate the input.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return the number of char32_t code units required to encode the UTF-8 string as UTF-32
   */
  simdutf_warn_unused virtual size_t utf32_length_from_utf8(const char * input, size_t length) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16LE string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t convert_utf16le_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16BE string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t convert_utf16be_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into Latin1 string.
   * If the string cannot be represented as Latin1, an error
   * is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16le_to_latin1_with_errors(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into Latin1 string.
   * If the string cannot be represented as Latin1, an error
   * is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16be_to_latin1_with_errors(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF-L16LE and that it can be represented as Latin1.
   * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
   *
   * This function is for expert users only and not part of our public API. Use convert_utf16le_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16le_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16BE string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF16-BE and that it can be represented as Latin1.
   * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
   *
   * This function is for expert users only and not part of our public API. Use convert_utf16be_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16be_to_latin1(const char16_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16LE string
   */
  simdutf_warn_unused virtual size_t convert_utf16le_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16BE string
   */
  simdutf_warn_unused virtual size_t convert_utf16be_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-8 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16le_to_utf8_with_errors(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-8 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16be_to_utf8_with_errors(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-16LE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16le_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16BE string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-16BE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16be_to_utf8(const char16_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16LE string
   */
  simdutf_warn_unused virtual size_t convert_utf16le_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16BE string
   */
  simdutf_warn_unused virtual size_t convert_utf16be_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-32 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16le_to_utf32_with_errors(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-32 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char32_t written if successful.
   */
  simdutf_warn_unused virtual result convert_utf16be_to_utf32_with_errors(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-32 string.
   *
   * This function assumes that the input string is valid UTF-16LE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16le_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-32BE string.
   *
   * This function assumes that the input string is valid UTF-16BE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf16be_to_utf32(const char16_t * input, size_t length, char32_t* utf32_buffer) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-16LE string would require in UTF-8 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as UTF-8
   */
  simdutf_warn_unused virtual size_t utf8_length_from_utf16le(const char16_t * input, size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-16BE string would require in UTF-8 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16BE string as UTF-8
   */
  simdutf_warn_unused virtual size_t utf8_length_from_utf16be(const char16_t * input, size_t length) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32 string
   */

  simdutf_warn_unused virtual size_t convert_utf32_to_latin1(const char32_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into Latin1 string and stop on error.
   * If the string cannot be represented as Latin1, an error is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_latin1_with_errors(const char32_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF-32 and can be represented as Latin1.
   * If you violate this assumption, the result is implementation defined and may include system-dependent behavior such as crashes.
   *
   * This function is for expert users only and not part of our public API. Use convert_utf32_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param latin1_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf32_to_latin1(const char32_t * input, size_t length, char* latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32 string
   */
  simdutf_warn_unused virtual size_t convert_utf32_to_utf8(const char32_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-8 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char written if successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_utf8_with_errors(const char32_t * input, size_t length, char* utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf8_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf32_to_utf8(const char32_t * input, size_t length, char* utf8_buffer) const noexcept = 0;


  /**
   * Return the number of bytes that this UTF-16 string would require in Latin1 format.
   *
   *
   * @param input         the UTF-16 string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16 string as Latin1
   */
    simdutf_warn_unused virtual size_t utf16_length_from_latin1(size_t length) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16LE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32 string
   */
  simdutf_warn_unused virtual size_t convert_utf32_to_utf16le(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16BE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32 string
   */
  simdutf_warn_unused virtual size_t convert_utf32_to_utf16be(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16LE string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_utf16le_with_errors(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16BE string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in code units) if any, or the number of char16_t written if successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_utf16be_with_errors(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-16LE string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf32_to_utf16le(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-16BE string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold the conversion result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t convert_valid_utf32_to_utf16be(const char32_t * input, size_t length, char16_t* utf16_buffer) const noexcept = 0;

  /**
   * Change the endianness of the input. Can be used to go from UTF-16LE to UTF-16BE or
   * from UTF-16BE to UTF-16LE.
   *
   * This function does not validate the input.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16 string to process
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @param output        the pointer to buffer that can hold the conversion result
   */
  virtual void change_endianness_utf16(const char16_t * input, size_t length, char16_t * output) const noexcept = 0;

 /**
   * Return the number of bytes that this Latin1 string would require in UTF-8 format.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string bytes
   * @return the number of bytes required to encode the Latin1 string as UTF-8
   */
    simdutf_warn_unused virtual size_t utf8_length_from_latin1(const char * input, size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-32 string would require in UTF-8 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-32 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as UTF-8
   */
  simdutf_warn_unused virtual size_t utf8_length_from_utf32(const char32_t * input, size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-32 string would require in Latin1 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-32 strings but in such cases
   * the result is implementation defined.
   *
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as Latin1
   */
  simdutf_warn_unused virtual size_t latin1_length_from_utf32(size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-8 string would require in Latin1 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in byte
   * @return the number of bytes required to encode the UTF-8 string as Latin1
   */
  simdutf_warn_unused virtual size_t latin1_length_from_utf8(const char * input, size_t length) const noexcept = 0;

  /*
   * Compute the number of bytes that this UTF-16LE/BE string would require in Latin1 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as Latin1
   */
  simdutf_warn_unused virtual size_t latin1_length_from_utf16(size_t length) const noexcept = 0;

  /**
   * Compute the number of two-byte code units that this UTF-32 string would require in UTF-16 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-32 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as UTF-16
   */
  simdutf_warn_unused virtual size_t utf16_length_from_utf32(const char32_t * input, size_t length) const noexcept = 0;


  /**
   * Return the number of bytes that this UTF-32 string would require in Latin1 format.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as Latin1
   */
    simdutf_warn_unused virtual size_t utf32_length_from_latin1(size_t length) const noexcept = 0;

  /*
   * Compute the number of bytes that this UTF-16LE string would require in UTF-32 format.
   *
   * This function is equivalent to count_utf16le.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as UTF-32
   */
  simdutf_warn_unused virtual size_t utf32_length_from_utf16le(const char16_t * input, size_t length) const noexcept = 0;

  /*
   * Compute the number of bytes that this UTF-16BE string would require in UTF-32 format.
   *
   * This function is equivalent to count_utf16be.
   *
   * This function does not validate the input. It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return the number of bytes required to encode the UTF-16BE string as UTF-32
   */
  simdutf_warn_unused virtual size_t utf32_length_from_utf16be(const char16_t * input, size_t length) const noexcept = 0;

  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-16LE.
   * It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to process
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t count_utf16le(const char16_t * input, size_t length) const noexcept = 0;

  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-16BE.
   * It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to process
   * @param length        the length of the string in 2-byte code units (char16_t)
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t count_utf16be(const char16_t * input, size_t length) const noexcept = 0;


  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-8.
   * It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t count_utf8(const char * input, size_t length) const noexcept = 0;

  /**
   * Provide the maximal binary length in bytes given the base64 input.
   * In general, if the input contains ASCII spaces, the result will be less than
   * the maximum length. It is acceptable to pass invalid base64 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the base64 input to process
   * @param length        the length of the base64 input in bytes
   * @return maximal number of binary bytes
   */
  simdutf_warn_unused virtual size_t maximal_binary_length_from_base64(const char * input, size_t length) const noexcept = 0;

  /**
   * Provide the maximal binary length in bytes given the base64 input.
   * In general, if the input contains ASCII spaces, the result will be less than
   * the maximum length. It is acceptable to pass invalid base64 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the base64 input to process, in ASCII stored as 16-bit units
   * @param length        the length of the base64 input in 16-bit units
   * @return maximal number of binary bytes
   */
  simdutf_warn_unused virtual size_t maximal_binary_length_from_base64(const char16_t * input, size_t length) const noexcept = 0;

  /**
   * Convert a base64 input to a binary output.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that it will
   * ignore any ASCII spaces in the input. You may provide a padded input (with one or two
   * equal signs at the end) or an unpadded input (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. There are two possible reasons for
   * failure: the input contains a number of base64 characters that when divided by 4, leaves
   * a single remainder character (BASE64_INPUT_REMAINDER), or the input contains a character
   * that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least maximal_binary_length_from_base64(input, length) bytes long.
   * If you fail to provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process
   * @param length        the length of the string in bytes
   * @param output        the pointer to buffer that can hold the conversion result (should be at least maximal_binary_length_from_base64(input, length) bytes long).
   * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and either position of the error (in the input in bytes) if any, or the number of bytes written if successful.
   */
  simdutf_warn_unused virtual result base64_to_binary(const char * input, size_t length, char* output, base64_options options = base64_default) const noexcept = 0;

  /**
   * Convert a base64 input to a binary output.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that it will
   * ignore any ASCII spaces in the input. You may provide a padded input (with one or two
   * equal signs at the end) or an unpadded input (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. There are two possible reasons for
   * failure: the input contains a number of base64 characters that when divided by 4, leaves
   * a single remainder character (BASE64_INPUT_REMAINDER), or the input contains a character
   * that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least maximal_binary_length_from_utf6_base64(input, length) bytes long.
   * If you fail to provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process, in ASCII stored as 16-bit units
   * @param length        the length of the string in 16-bit units
   * @param output        the pointer to buffer that can hold the conversion result (should be at least maximal_binary_length_from_base64(input, length) bytes long).
   * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
   * @return a result pair struct (of type simdutf::error containing the two fields error and count) with an error code and position of the INVALID_BASE64_CHARACTER error (in the input in units) if any, or the number of bytes written if successful.
   */
  simdutf_warn_unused virtual result base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options = base64_default) const noexcept = 0;

  /**
   * Provide the base64 length in bytes given the length of a binary input.
   *
   * @param length        the length of the input in bytes
   * @parem options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
   * @return number of base64 bytes
   */
  simdutf_warn_unused virtual size_t base64_length_from_binary(size_t length, base64_options options = base64_default) const noexcept = 0;

  /**
   * Convert a binary input to a base64 output.
   *
   * The default option (simdutf::base64_default) uses the characters `+` and `/` as part of its alphabet.
   * Further, it adds padding (`=`) at the end of the output to ensure that the output length is a multiple of four.
   *
   * The URL option (simdutf::base64_url) uses the characters `-` and `_` as part of its alphabet. No padding
   * is added at the end of the output.
   *
   * This function always succeeds.
   *
   * @param input         the binary to process
   * @param length        the length of the input in bytes
   * @param output        the pointer to buffer that can hold the conversion result (should be at least base64_length_from_binary(length) bytes long)
   * @param options       the base64 options to use, can be base64_default or base64_url, is base64_default by default.
   * @return number of written bytes, will be equal to base64_length_from_binary(length, options)
   */
  virtual size_t binary_to_base64(const char * input, size_t length, char* output, base64_options options = base64_default) const noexcept = 0;


protected:
  /** @private Construct an implementation with the given name and description. For subclasses. */
  simdutf_really_inline implementation(
    const char* name,
    const char* description,
    uint32_t required_instruction_sets
  ) :
    _name(name),
    _description(description),
    _required_instruction_sets(required_instruction_sets)
  {
  }
protected:
  ~implementation() = default;
private:
  /**
   * The name of this implementation.
   */
  const char* _name;

  /**
   * The description of this implementation.
   */
  const char* _description;

  /**
   * Instruction sets required for this implementation.
   */
  const uint32_t _required_instruction_sets;
};

/** @private */
namespace internal {

/**
 * The list of available implementations compiled into simdutf.
 */
class available_implementation_list {
public:
  /** Get the list of available implementations compiled into simdutf */
  simdutf_really_inline available_implementation_list() {}
  /** Number of implementations */
  size_t size() const noexcept;
  /** STL const begin() iterator */
  const implementation * const *begin() const noexcept;
  /** STL const end() iterator */
  const implementation * const *end() const noexcept;

  /**
   * Get the implementation with the given name.
   *
   * Case sensitive.
   *
   *     const implementation *impl = simdutf::available_implementations["westmere"];
   *     if (!impl) { exit(1); }
   *     if (!imp->supported_by_runtime_system()) { exit(1); }
   *     simdutf::active_implementation = impl;
   *
   * @param name the implementation to find, e.g. "westmere", "haswell", "arm64"
   * @return the implementation, or nullptr if the parse failed.
   */
  const implementation * operator[](const std::string &name) const noexcept {
    for (const implementation * impl : *this) {
      if (impl->name() == name) { return impl; }
    }
    return nullptr;
  }

  /**
   * Detect the most advanced implementation supported by the current host.
   *
   * This is used to initialize the implementation on startup.
   *
   *     const implementation *impl = simdutf::available_implementation::detect_best_supported();
   *     simdutf::active_implementation = impl;
   *
   * @return the most advanced supported implementation for the current host, or an
   *         implementation that returns UNSUPPORTED_ARCHITECTURE if there is no supported
   *         implementation. Will never return nullptr.
   */
  const implementation *detect_best_supported() const noexcept;
};

template<typename T>
class atomic_ptr {
public:
  atomic_ptr(T *_ptr) : ptr{_ptr} {}

#if defined(SIMDUTF_NO_THREADS)
  operator const T*() const { return ptr; }
  const T& operator*() const { return *ptr; }
  const T* operator->() const { return ptr; }

  operator T*() { return ptr; }
  T& operator*() { return *ptr; }
  T* operator->() { return ptr; }
  atomic_ptr& operator=(T *_ptr) { ptr = _ptr; return *this; }

#else
  operator const T*() const { return ptr.load(); }
  const T& operator*() const { return *ptr; }
  const T* operator->() const { return ptr.load(); }

  operator T*() { return ptr.load(); }
  T& operator*() { return *ptr; }
  T* operator->() { return ptr.load(); }
  atomic_ptr& operator=(T *_ptr) { ptr = _ptr; return *this; }

#endif

private:
#if defined(SIMDUTF_NO_THREADS)
  T* ptr;
#else
  std::atomic<T*> ptr;
#endif
};

class detect_best_supported_implementation_on_first_use;

} // namespace internal

/**
 * The list of available implementations compiled into simdutf.
 */
extern SIMDUTF_DLLIMPORTEXPORT const internal::available_implementation_list& get_available_implementations();

/**
  * The active implementation.
  *
  * Automatically initialized on first use to the most advanced implementation supported by this hardware.
  */
extern SIMDUTF_DLLIMPORTEXPORT internal::atomic_ptr<const implementation>& get_active_implementation();


} // namespace simdutf

#endif // SIMDUTF_IMPLEMENTATION_H
