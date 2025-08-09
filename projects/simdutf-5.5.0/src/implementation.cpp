#include "simdutf.h"
#include <initializer_list>
#include <climits>
#include <type_traits>

// Useful for debugging purposes
namespace simdutf {
namespace {

template <typename T>
std::string toBinaryString(T b) {
   std::string binary = "";
   T mask = T(1) << (sizeof(T) * CHAR_BIT - 1);
   while (mask > 0) {
    binary += ((b & mask) == 0) ? '0' : '1';
    mask >>= 1;
  }
  return binary;
}
}
}

// Implementations
// The best choice should always come first!
#include "simdutf/arm64.h"
#include "simdutf/icelake.h"
#include "simdutf/haswell.h"
#include "simdutf/westmere.h"
#include "simdutf/ppc64.h"
#include "simdutf/rvv.h"
#include "simdutf/fallback.h" // have it always last.

#include "scalar/utf8.h"
#include "scalar/utf16.h"
#include "scalar/utf32.h"
#include "scalar/base64.h"
#include "scalar/latin1_to_utf8/latin1_to_utf8.h"

namespace simdutf {
bool implementation::supported_by_runtime_system() const {
  uint32_t required_instruction_sets = this->required_instruction_sets();
  uint32_t supported_instruction_sets = internal::detect_supported_architectures();
  return ((supported_instruction_sets & required_instruction_sets) == required_instruction_sets);
}

simdutf_warn_unused encoding_type implementation::autodetect_encoding(const char * input, size_t length) const noexcept {
    // If there is a BOM, then we trust it.
    auto bom_encoding = simdutf::BOM::check_bom(input, length);
    if(bom_encoding != encoding_type::unspecified) { return bom_encoding; }
    // UTF8 is common, it includes ASCII, and is commonly represented
    // without a BOM, so if it fits, go with that. Note that it is still
    // possible to get it wrong, we are only 'guessing'. If some has UTF-16
    // data without a BOM, it could pass as UTF-8.
    //
    // An interesting twist might be to check for UTF-16 ASCII first (every
    // other byte is zero).
    if(validate_utf8(input, length)) { return encoding_type::UTF8; }
    // The next most common encoding that might appear without BOM is probably
    // UTF-16LE, so try that next.
    if((length % 2) == 0) {
      // important: we need to divide by two
      if(validate_utf16le(reinterpret_cast<const char16_t*>(input), length/2)) { return encoding_type::UTF16_LE; }
    }
    if((length % 4) == 0) {
      if(validate_utf32(reinterpret_cast<const char32_t*>(input), length/4)) { return encoding_type::UTF32_LE; }
    }
    return encoding_type::unspecified;
}

namespace internal {
// When there is a single implementation, we should not pay a price
 // for dispatching to the best implementation. We should just use the
 // one we have. This is a compile-time check.
 #define SIMDUTF_SINGLE_IMPLEMENTATION (SIMDUTF_IMPLEMENTATION_ICELAKE \
              + SIMDUTF_IMPLEMENTATION_HASWELL + SIMDUTF_IMPLEMENTATION_WESTMERE \
              + SIMDUTF_IMPLEMENTATION_ARM64 + SIMDUTF_IMPLEMENTATION_PPC64 \
              + SIMDUTF_IMPLEMENTATION_FALLBACK == 1)

// Static array of known implementations. We are hoping these get baked into the executable
// without requiring a static initializer.


#if SIMDUTF_IMPLEMENTATION_ICELAKE
static const icelake::implementation* get_icelake_singleton() {
  static const icelake::implementation icelake_singleton{};
  return &icelake_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_HASWELL
static const haswell::implementation* get_haswell_singleton() {
  static const haswell::implementation haswell_singleton{};
  return &haswell_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_WESTMERE
static const westmere::implementation* get_westmere_singleton() {
  static const westmere::implementation westmere_singleton{};
  return &westmere_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_ARM64
static const arm64::implementation* get_arm64_singleton() {
  static const arm64::implementation arm64_singleton{};
  return &arm64_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_PPC64
static const ppc64::implementation* get_ppc64_singleton() {
  static const ppc64::implementation ppc64_singleton{};
  return &ppc64_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_RVV
static const rvv::implementation* get_rvv_singleton() {
  static const rvv::implementation rvv_singleton{};
  return &rvv_singleton;
}
#endif
#if SIMDUTF_IMPLEMENTATION_FALLBACK
static const fallback::implementation* get_fallback_singleton() {
  static const fallback::implementation fallback_singleton{};
  return &fallback_singleton;
}
#endif

#if SIMDUTF_SINGLE_IMPLEMENTATION
static const implementation* get_single_implementation() {
    return
#if SIMDUTF_IMPLEMENTATION_ICELAKE
    get_icelake_singleton();
#endif
#if SIMDUTF_IMPLEMENTATION_HASWELL
    get_haswell_singleton();
#endif
#if SIMDUTF_IMPLEMENTATION_WESTMERE
    get_westmere_singleton();
#endif
#if SIMDUTF_IMPLEMENTATION_ARM64
    get_arm64_singleton();
#endif
#if SIMDUTF_IMPLEMENTATION_PPC64
    get_ppc64_singleton();
#endif
#if SIMDUTF_IMPLEMENTATION_FALLBACK
    get_fallback_singleton();
#endif
}
#endif

/**
 * @private Detects best supported implementation on first use, and sets it
 */
class detect_best_supported_implementation_on_first_use final : public implementation {
public:
  std::string name() const noexcept final { return set_best()->name(); }
  std::string description() const noexcept final { return set_best()->description(); }
  uint32_t required_instruction_sets() const noexcept final { return set_best()->required_instruction_sets(); }

  simdutf_warn_unused int detect_encodings(const char * input, size_t length) const noexcept override {
    return set_best()->detect_encodings(input, length);
  }

  simdutf_warn_unused bool validate_utf8(const char * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf8(buf, len);
  }

  simdutf_warn_unused result validate_utf8_with_errors(const char * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf8_with_errors(buf, len);
  }

  simdutf_warn_unused bool validate_ascii(const char * buf, size_t len) const noexcept final override {
    return set_best()->validate_ascii(buf, len);
  }

  simdutf_warn_unused result validate_ascii_with_errors(const char * buf, size_t len) const noexcept final override {
    return set_best()->validate_ascii_with_errors(buf, len);
  }

  simdutf_warn_unused bool validate_utf16le(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf16le(buf, len);
  }

  simdutf_warn_unused bool validate_utf16be(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf16be(buf, len);
  }

  simdutf_warn_unused result validate_utf16le_with_errors(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf16le_with_errors(buf, len);
  }

  simdutf_warn_unused result validate_utf16be_with_errors(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf16be_with_errors(buf, len);
  }

  simdutf_warn_unused bool validate_utf32(const char32_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf32(buf, len);
  }

  simdutf_warn_unused result validate_utf32_with_errors(const char32_t * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf32_with_errors(buf, len);
  }

  simdutf_warn_unused size_t convert_latin1_to_utf8(const char * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_latin1_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_latin1_to_utf16le(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_latin1_to_utf16le(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_latin1_to_utf16be(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_latin1_to_utf16be(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_latin1_to_utf32(const char * buf, size_t len, char32_t * latin1_output) const noexcept final override {
    return set_best()->convert_latin1_to_utf32(buf, len,latin1_output);
  }

  simdutf_warn_unused size_t convert_utf8_to_latin1(const char * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf8_to_latin1(buf, len,latin1_output);
  }

  simdutf_warn_unused result convert_utf8_to_latin1_with_errors(const char* buf, size_t len, char* latin1_output) const noexcept  final override {
  return set_best()->convert_utf8_to_latin1_with_errors(buf, len, latin1_output);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_latin1(const char * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_valid_utf8_to_latin1(buf, len,latin1_output);
  }

  simdutf_warn_unused size_t convert_utf8_to_utf16le(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf16le(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_utf8_to_utf16be(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf16be(buf, len, utf16_output);
  }

  simdutf_warn_unused result convert_utf8_to_utf16le_with_errors(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf16le_with_errors(buf, len, utf16_output);
  }

  simdutf_warn_unused result convert_utf8_to_utf16be_with_errors(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf16be_with_errors(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf16le(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_valid_utf8_to_utf16le(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf16be(const char * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_valid_utf8_to_utf16be(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_utf8_to_utf32(const char * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf32(buf, len, utf32_output);
  }

  simdutf_warn_unused result convert_utf8_to_utf32_with_errors(const char * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf8_to_utf32_with_errors(buf, len, utf32_output);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf32(const char * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_valid_utf8_to_utf32(buf, len, utf32_output);
  }

  simdutf_warn_unused size_t convert_utf16le_to_latin1(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf16le_to_latin1(buf, len, latin1_output);
  }

  simdutf_warn_unused size_t convert_utf16be_to_latin1(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf16be_to_latin1(buf, len, latin1_output);
  }

  simdutf_warn_unused result convert_utf16le_to_latin1_with_errors(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf16le_to_latin1_with_errors(buf, len, latin1_output);
  }

  simdutf_warn_unused result convert_utf16be_to_latin1_with_errors(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf16be_to_latin1_with_errors(buf, len, latin1_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_latin1(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_valid_utf16le_to_latin1(buf, len, latin1_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_latin1(const char16_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_valid_utf16be_to_latin1(buf, len, latin1_output);
  }

  simdutf_warn_unused size_t convert_utf16le_to_utf8(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf16le_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_utf16be_to_utf8(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf16be_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused result convert_utf16le_to_utf8_with_errors(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf16le_to_utf8_with_errors(buf, len, utf8_output);
  }

  simdutf_warn_unused result convert_utf16be_to_utf8_with_errors(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf16be_to_utf8_with_errors(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_utf8(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_valid_utf16le_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_utf8(const char16_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_valid_utf16be_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_utf32_to_latin1(const char32_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf32_to_latin1(buf, len,latin1_output);
  }

  simdutf_warn_unused result convert_utf32_to_latin1_with_errors(const char32_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf32_to_latin1_with_errors(buf, len,latin1_output);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_latin1(const char32_t * buf, size_t len, char* latin1_output) const noexcept final override {
    return set_best()->convert_utf32_to_latin1(buf, len,latin1_output);
  }

  simdutf_warn_unused size_t convert_utf32_to_utf8(const char32_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused result convert_utf32_to_utf8_with_errors(const char32_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf8_with_errors(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf8(const char32_t * buf, size_t len, char* utf8_output) const noexcept final override {
    return set_best()->convert_valid_utf32_to_utf8(buf, len, utf8_output);
  }

  simdutf_warn_unused size_t convert_utf32_to_utf16le(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf16le(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_utf32_to_utf16be(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf16be(buf, len, utf16_output);
  }

  simdutf_warn_unused result convert_utf32_to_utf16le_with_errors(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf16le_with_errors(buf, len, utf16_output);
  }

  simdutf_warn_unused result convert_utf32_to_utf16be_with_errors(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_utf32_to_utf16be_with_errors(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf16le(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_valid_utf32_to_utf16le(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf16be(const char32_t * buf, size_t len, char16_t* utf16_output) const noexcept final override {
    return set_best()->convert_valid_utf32_to_utf16be(buf, len, utf16_output);
  }

  simdutf_warn_unused size_t convert_utf16le_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf16le_to_utf32(buf, len, utf32_output);
  }

  simdutf_warn_unused size_t convert_utf16be_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf16be_to_utf32(buf, len, utf32_output);
  }

  simdutf_warn_unused result convert_utf16le_to_utf32_with_errors(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf16le_to_utf32_with_errors(buf, len, utf32_output);
  }

  simdutf_warn_unused result convert_utf16be_to_utf32_with_errors(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_utf16be_to_utf32_with_errors(buf, len, utf32_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_valid_utf16le_to_utf32(buf, len, utf32_output);
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_output) const noexcept final override {
    return set_best()->convert_valid_utf16be_to_utf32(buf, len, utf32_output);
  }

  void change_endianness_utf16(const char16_t * buf, size_t len, char16_t * output) const noexcept final override {
    set_best()->change_endianness_utf16(buf, len, output);
  }

  simdutf_warn_unused size_t count_utf16le(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->count_utf16le(buf, len);
  }

  simdutf_warn_unused size_t count_utf16be(const char16_t * buf, size_t len) const noexcept final override {
    return set_best()->count_utf16be(buf, len);
  }

  simdutf_warn_unused size_t count_utf8(const char * buf, size_t len) const noexcept final override {
    return set_best()->count_utf8(buf, len);
  }

  simdutf_warn_unused size_t latin1_length_from_utf8(const char * buf, size_t len) const noexcept override {
    return set_best()->latin1_length_from_utf8(buf, len);
  }

  simdutf_warn_unused size_t latin1_length_from_utf16(size_t len) const noexcept override {
    return set_best()->latin1_length_from_utf16(len);
  }

  simdutf_warn_unused size_t latin1_length_from_utf32(size_t len) const noexcept override {
    return set_best()->latin1_length_from_utf32(len);
  }

  simdutf_warn_unused size_t utf8_length_from_latin1(const char * buf, size_t len) const noexcept override {
    return set_best()->utf8_length_from_latin1(buf, len);
  }

  simdutf_warn_unused size_t utf8_length_from_utf16le(const char16_t * buf, size_t len) const noexcept override {
    return set_best()->utf8_length_from_utf16le(buf, len);
  }

  simdutf_warn_unused size_t utf8_length_from_utf16be(const char16_t * buf, size_t len) const noexcept override {
    return set_best()->utf8_length_from_utf16be(buf, len);
  }

  simdutf_warn_unused size_t utf16_length_from_latin1(size_t len) const noexcept override {
    return set_best()->utf16_length_from_latin1(len);
  }

  simdutf_warn_unused size_t utf32_length_from_latin1(size_t len) const noexcept override {
    return set_best()->utf32_length_from_latin1(len);
  }

  simdutf_warn_unused size_t utf32_length_from_utf16le(const char16_t * buf, size_t len) const noexcept override {
    return set_best()->utf32_length_from_utf16le(buf, len);
  }

  simdutf_warn_unused size_t utf32_length_from_utf16be(const char16_t * buf, size_t len) const noexcept override {
    return set_best()->utf32_length_from_utf16be(buf, len);
  }

  simdutf_warn_unused size_t utf16_length_from_utf8(const char * buf, size_t len) const noexcept override {
    return set_best()->utf16_length_from_utf8(buf, len);
  }

  simdutf_warn_unused size_t utf8_length_from_utf32(const char32_t * buf, size_t len) const noexcept override {
    return set_best()->utf8_length_from_utf32(buf, len);
  }

  simdutf_warn_unused size_t utf16_length_from_utf32(const char32_t * buf, size_t len) const noexcept override {
    return set_best()->utf16_length_from_utf32(buf, len);
  }

  simdutf_warn_unused size_t utf32_length_from_utf8(const char * buf, size_t len) const noexcept override {
    return set_best()->utf32_length_from_utf8(buf, len);
  }

  simdutf_warn_unused size_t maximal_binary_length_from_base64(const char * input, size_t length) const noexcept override {
    return set_best()->maximal_binary_length_from_base64(input, length);
  }

  simdutf_warn_unused result base64_to_binary(const char * input, size_t length, char* output, base64_options options) const noexcept override {
    return set_best()->base64_to_binary(input, length, output, options);
  }

  simdutf_warn_unused size_t maximal_binary_length_from_base64(const char16_t * input, size_t length) const noexcept override {
    return set_best()->maximal_binary_length_from_base64(input, length);
  }

  simdutf_warn_unused result base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options) const noexcept override {
    return set_best()->base64_to_binary(input, length, output, options);
  }

  simdutf_warn_unused size_t base64_length_from_binary(size_t length, base64_options options) const noexcept override {
    return set_best()->base64_length_from_binary(length, options);
  }

  size_t binary_to_base64(const char * input, size_t length, char* output, base64_options options) const noexcept override {
    return set_best()->binary_to_base64(input, length, output, options);
  }

  simdutf_really_inline detect_best_supported_implementation_on_first_use() noexcept : implementation("best_supported_detector", "Detects the best supported implementation and sets it", 0) {}

private:
  const implementation *set_best() const noexcept;
};

static_assert(std::is_trivially_destructible<detect_best_supported_implementation_on_first_use>::value, "detect_best_supported_implementation_on_first_use should be trivially destructible");

static const std::initializer_list<const implementation *>& get_available_implementation_pointers() {
  static const std::initializer_list<const implementation *> available_implementation_pointers {
#if SIMDUTF_IMPLEMENTATION_ICELAKE
    get_icelake_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_HASWELL
    get_haswell_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_WESTMERE
    get_westmere_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_ARM64
    get_arm64_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_PPC64
    get_ppc64_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_RVV
    get_rvv_singleton(),
#endif
#if SIMDUTF_IMPLEMENTATION_FALLBACK
    get_fallback_singleton(),
#endif
  }; // available_implementation_pointers
  return available_implementation_pointers;
}

// So we can return UNSUPPORTED_ARCHITECTURE from the parser when there is no support
class unsupported_implementation final : public implementation {
public:
  simdutf_warn_unused int detect_encodings(const char *, size_t) const noexcept override {
    return encoding_type::unspecified;
  }

  simdutf_warn_unused bool validate_utf8(const char *, size_t) const noexcept final override {
    return false; // Just refuse to validate. Given that we have a fallback implementation
    // it seems unlikely that unsupported_implementation will ever be used. If it is used,
    // then it will flag all strings as invalid. The alternative is to return an error_code
    // from which the user has to figure out whether the string is valid UTF-8... which seems
    // like a lot of work just to handle the very unlikely case that we have an unsupported
    // implementation. And, when it does happen (that we have an unsupported implementation),
    // what are the chances that the programmer has a fallback? Given that *we* provide the
    // fallback, it implies that the programmer would need a fallback for our fallback.
  }

  simdutf_warn_unused result validate_utf8_with_errors(const char *, size_t) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused bool validate_ascii(const char *, size_t) const noexcept final override {
    return false;
  }

  simdutf_warn_unused result validate_ascii_with_errors(const char *, size_t) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused bool validate_utf16le(const char16_t*, size_t) const noexcept final override {
    return false;
  }

  simdutf_warn_unused bool validate_utf16be(const char16_t*, size_t) const noexcept final override {
    return false;
  }

  simdutf_warn_unused result validate_utf16le_with_errors(const char16_t*, size_t) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result validate_utf16be_with_errors(const char16_t*, size_t) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused bool validate_utf32(const char32_t*, size_t) const noexcept final override {
    return false;
  }

  simdutf_warn_unused result validate_utf32_with_errors(const char32_t*, size_t) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_latin1_to_utf8(const char*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_latin1_to_utf16le(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_latin1_to_utf16be(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_latin1_to_utf32(const char*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf8_to_latin1(const char*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf8_to_latin1_with_errors(const char*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_latin1(const char*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf8_to_utf16le(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf8_to_utf16be(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf8_to_utf16le_with_errors(const char*, size_t, char16_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result convert_utf8_to_utf16be_with_errors(const char*, size_t, char16_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf16le(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf16be(const char*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf8_to_utf32(const char*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf8_to_utf32_with_errors(const char*, size_t, char32_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf8_to_utf32(const char*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16le_to_latin1(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16be_to_latin1(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf16le_to_latin1_with_errors(const char16_t*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result convert_utf16be_to_latin1_with_errors(const char16_t*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_latin1(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_latin1(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16le_to_utf8(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16be_to_utf8(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf16le_to_utf8_with_errors(const char16_t*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result convert_utf16be_to_utf8_with_errors(const char16_t*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_utf8(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_utf8(const char16_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf32_to_latin1(const char32_t *, size_t, char* ) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf32_to_latin1_with_errors(const char32_t *, size_t, char* ) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_latin1(const char32_t *, size_t, char* ) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf32_to_utf8(const char32_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf32_to_utf8_with_errors(const char32_t*, size_t, char*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf8(const char32_t*, size_t, char*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf32_to_utf16le(const char32_t*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf32_to_utf16be(const char32_t*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf32_to_utf16le_with_errors(const char32_t*, size_t, char16_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result convert_utf32_to_utf16be_with_errors(const char32_t*, size_t, char16_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf16le(const char32_t*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_valid_utf32_to_utf16be(const char32_t*, size_t, char16_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16le_to_utf32(const char16_t*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_utf16be_to_utf32(const char16_t*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused result convert_utf16le_to_utf32_with_errors(const char16_t*, size_t, char32_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused result convert_utf16be_to_utf32_with_errors(const char16_t*, size_t, char32_t*) const noexcept final override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t convert_valid_utf16le_to_utf32(const char16_t*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t convert_valid_utf16be_to_utf32(const char16_t*, size_t, char32_t*) const noexcept final override {
    return 0;
  }

  void change_endianness_utf16(const char16_t *, size_t, char16_t *) const noexcept final override {

  }

  simdutf_warn_unused size_t count_utf16le(const char16_t *, size_t) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t count_utf16be(const char16_t *, size_t) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t count_utf8(const char *, size_t) const noexcept final override {
    return 0;
  }

  simdutf_warn_unused size_t latin1_length_from_utf8(const char *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t latin1_length_from_utf16(size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t latin1_length_from_utf32(size_t) const noexcept override {
    return 0;
  }
  simdutf_warn_unused size_t utf8_length_from_latin1(const char *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf8_length_from_utf16le(const char16_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf8_length_from_utf16be(const char16_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf32_length_from_utf16le(const char16_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf32_length_from_utf16be(const char16_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf32_length_from_latin1(size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf16_length_from_utf8(const char *, size_t) const noexcept override {
    return 0;
  }
  simdutf_warn_unused size_t utf16_length_from_latin1(size_t) const noexcept override {
    return 0;
  }
  simdutf_warn_unused size_t utf8_length_from_utf32(const char32_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf16_length_from_utf32(const char32_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t utf32_length_from_utf8(const char *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused size_t maximal_binary_length_from_base64(const char *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused result base64_to_binary(const char *, size_t, char*, base64_options) const noexcept override {
    return result(error_code::OTHER, 0);
  }

  simdutf_warn_unused size_t maximal_binary_length_from_base64(const char16_t *, size_t) const noexcept override {
    return 0;
  }

  simdutf_warn_unused result base64_to_binary(const char16_t *, size_t, char*, base64_options) const noexcept override {
    return result(error_code::OTHER, 0);
  }


  simdutf_warn_unused size_t base64_length_from_binary(size_t, base64_options) const noexcept override {
    return 0;
  }

  size_t binary_to_base64(const char *, size_t, char*, base64_options) const noexcept override {
    return 0;
  }

  unsupported_implementation() : implementation("unsupported", "Unsupported CPU (no detected SIMD instructions)", 0) {}
};

const unsupported_implementation* get_unsupported_singleton() {
    static const unsupported_implementation unsupported_singleton{};
    return &unsupported_singleton;
}
static_assert(std::is_trivially_destructible<unsupported_implementation>::value, "unsupported_singleton should be trivially destructible");

size_t available_implementation_list::size() const noexcept {
  return internal::get_available_implementation_pointers().size();
}
const implementation * const *available_implementation_list::begin() const noexcept {
  return internal::get_available_implementation_pointers().begin();
}
const implementation * const *available_implementation_list::end() const noexcept {
  return internal::get_available_implementation_pointers().end();
}
const implementation *available_implementation_list::detect_best_supported() const noexcept {
  // They are prelisted in priority order, so we just go down the list
  uint32_t supported_instruction_sets = internal::detect_supported_architectures();
  for (const implementation *impl : internal::get_available_implementation_pointers()) {
    uint32_t required_instruction_sets = impl->required_instruction_sets();
    if ((supported_instruction_sets & required_instruction_sets) == required_instruction_sets) { return impl; }
  }
  return get_unsupported_singleton(); // this should never happen?
}

const implementation *detect_best_supported_implementation_on_first_use::set_best() const noexcept {
  SIMDUTF_PUSH_DISABLE_WARNINGS
  SIMDUTF_DISABLE_DEPRECATED_WARNING // Disable CRT_SECURE warning on MSVC: manually verified this is safe
  char *force_implementation_name = getenv("SIMDUTF_FORCE_IMPLEMENTATION");
  SIMDUTF_POP_DISABLE_WARNINGS

  if (force_implementation_name) {
    auto force_implementation = get_available_implementations()[force_implementation_name];
    if (force_implementation) {
      return get_active_implementation() = force_implementation;
    } else {
      // Note: abort() and stderr usage within the library is forbidden.
      return get_active_implementation() = get_unsupported_singleton();
    }
  }
  return get_active_implementation() = get_available_implementations().detect_best_supported();
}

} // namespace internal



/**
 * The list of available implementations compiled into simdutf.
 */
SIMDUTF_DLLIMPORTEXPORT const internal::available_implementation_list& get_available_implementations() {
  static const internal::available_implementation_list available_implementations{};
  return available_implementations;
}

/**
 * The active implementation.
 */
SIMDUTF_DLLIMPORTEXPORT internal::atomic_ptr<const implementation>& get_active_implementation() {
#if SIMDUTF_SINGLE_IMPLEMENTATION
    // skip runtime detection
    static internal::atomic_ptr<const implementation> active_implementation{internal::get_single_implementation()};
    return active_implementation;
#else
    static const internal::detect_best_supported_implementation_on_first_use detect_best_supported_implementation_on_first_use_singleton;
    static internal::atomic_ptr<const implementation> active_implementation{&detect_best_supported_implementation_on_first_use_singleton};
    return active_implementation;
#endif
}


#if SIMDUTF_SINGLE_IMPLEMENTATION
const implementation * get_default_implementation() {
  return internal::get_single_implementation();
}
#else
internal::atomic_ptr<const implementation>& get_default_implementation() {
  return get_active_implementation();
}
#endif
#define SIMDUTF_GET_CURRENT_IMPLEMENTION

simdutf_warn_unused bool validate_utf8(const char *buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf8(buf, len);
}
simdutf_warn_unused result validate_utf8_with_errors(const char *buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf8_with_errors(buf, len);
}
simdutf_warn_unused bool validate_ascii(const char *buf, size_t len) noexcept {
  return get_default_implementation()->validate_ascii(buf, len);
}
simdutf_warn_unused result validate_ascii_with_errors(const char *buf, size_t len) noexcept {
  return get_default_implementation()->validate_ascii_with_errors(buf, len);
}
simdutf_warn_unused size_t convert_utf8_to_utf16(const char * input, size_t length, char16_t* utf16_output) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf8_to_utf16be(input, length, utf16_output);
  #else
  return convert_utf8_to_utf16le(input, length, utf16_output);
  #endif
}
simdutf_warn_unused size_t convert_latin1_to_utf8(const char * buf, size_t len, char* utf8_output) noexcept {
  return get_default_implementation()->convert_latin1_to_utf8(buf, len,utf8_output);
}
simdutf_warn_unused size_t convert_latin1_to_utf16le(const char * buf, size_t len, char16_t* utf16_output) noexcept {
  return get_default_implementation()->convert_latin1_to_utf16le(buf, len, utf16_output);
}
simdutf_warn_unused size_t convert_latin1_to_utf16be(const char * buf, size_t len, char16_t* utf16_output) noexcept{
  return get_default_implementation()->convert_latin1_to_utf16be(buf, len, utf16_output);
}
simdutf_warn_unused size_t convert_latin1_to_utf32(const char * buf, size_t len, char32_t * latin1_output) noexcept {
  return get_default_implementation()->convert_latin1_to_utf32(buf, len,latin1_output);
}
simdutf_warn_unused size_t convert_utf8_to_latin1(const char * buf, size_t len, char* latin1_output) noexcept {
  return get_default_implementation()->convert_utf8_to_latin1(buf, len,latin1_output);
}
simdutf_warn_unused result convert_utf8_to_latin1_with_errors(const char* buf, size_t len, char* latin1_output) noexcept {
  return get_default_implementation()->convert_utf8_to_latin1_with_errors(buf, len, latin1_output);
}
simdutf_warn_unused size_t convert_valid_utf8_to_latin1(const char * buf, size_t len, char* latin1_output) noexcept {
  return get_default_implementation()->convert_valid_utf8_to_latin1(buf, len,latin1_output);
}
simdutf_warn_unused size_t convert_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf16le(input, length, utf16_output);
}
simdutf_warn_unused size_t convert_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf16be(input, length, utf16_output);
}
simdutf_warn_unused result convert_utf8_to_utf16_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf8_to_utf16be_with_errors(input, length, utf16_output);
  #else
  return convert_utf8_to_utf16le_with_errors(input, length, utf16_output);
  #endif
}
simdutf_warn_unused result convert_utf8_to_utf16le_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf16le_with_errors(input, length, utf16_output);
}
simdutf_warn_unused result convert_utf8_to_utf16be_with_errors(const char * input, size_t length, char16_t* utf16_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf16be_with_errors(input, length, utf16_output);
}
simdutf_warn_unused size_t convert_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf32(input, length, utf32_output);
}
simdutf_warn_unused result convert_utf8_to_utf32_with_errors(const char * input, size_t length, char32_t* utf32_output) noexcept {
  return get_default_implementation()->convert_utf8_to_utf32_with_errors(input, length, utf32_output);
}
simdutf_warn_unused bool validate_utf16(const char16_t * buf, size_t len) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return validate_utf16be(buf, len);
  #else
  return validate_utf16le(buf, len);
  #endif
}
simdutf_warn_unused bool validate_utf16le(const char16_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf16le(buf, len);
}
simdutf_warn_unused bool validate_utf16be(const char16_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf16be(buf, len);
}
simdutf_warn_unused result validate_utf16_with_errors(const char16_t * buf, size_t len) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return validate_utf16be_with_errors(buf, len);
  #else
  return validate_utf16le_with_errors(buf, len);
  #endif
}
simdutf_warn_unused result validate_utf16le_with_errors(const char16_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf16le_with_errors(buf, len);
}
simdutf_warn_unused result validate_utf16be_with_errors(const char16_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf16be_with_errors(buf, len);
}
simdutf_warn_unused bool validate_utf32(const char32_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf32(buf, len);
}
simdutf_warn_unused result validate_utf32_with_errors(const char32_t * buf, size_t len) noexcept {
  return get_default_implementation()->validate_utf32_with_errors(buf, len);
}
simdutf_warn_unused size_t convert_valid_utf8_to_utf16(const char * input, size_t length, char16_t* utf16_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_valid_utf8_to_utf16be(input, length, utf16_buffer);
  #else
  return convert_valid_utf8_to_utf16le(input, length, utf16_buffer);
  #endif
}
simdutf_warn_unused size_t convert_valid_utf8_to_utf16le(const char * input, size_t length, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf8_to_utf16le(input, length, utf16_buffer);
}
simdutf_warn_unused size_t convert_valid_utf8_to_utf16be(const char * input, size_t length, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf8_to_utf16be(input, length, utf16_buffer);
}
simdutf_warn_unused size_t convert_valid_utf8_to_utf32(const char * input, size_t length, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf8_to_utf32(input, length, utf32_buffer);
}
simdutf_warn_unused size_t convert_utf16_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_utf8(buf, len, utf8_buffer);
  #else
  return convert_utf16le_to_utf8(buf, len, utf8_buffer);
  #endif
}
simdutf_warn_unused size_t convert_utf16_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_latin1(buf, len, latin1_buffer);
  #else
  return convert_utf16le_to_latin1(buf, len, latin1_buffer);
  #endif
}
simdutf_warn_unused size_t convert_latin1_to_utf16(const char * buf, size_t len, char16_t* utf16_output) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_latin1_to_utf16be(buf, len, utf16_output);
  #else
  return convert_latin1_to_utf16le(buf, len, utf16_output);
  #endif
}
simdutf_warn_unused size_t convert_utf16be_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_latin1(buf, len, latin1_buffer);
}
simdutf_warn_unused size_t convert_utf16le_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_latin1(buf, len, latin1_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16be_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16be_to_latin1(buf, len, latin1_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16le_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16le_to_latin1(buf, len, latin1_buffer);
}
simdutf_warn_unused result convert_utf16le_to_latin1_with_errors(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_latin1_with_errors(buf, len, latin1_buffer);
}
simdutf_warn_unused result convert_utf16be_to_latin1_with_errors(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_latin1_with_errors(buf, len, latin1_buffer);
}
simdutf_warn_unused size_t convert_utf16le_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_utf16be_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused result convert_utf16_to_utf8_with_errors(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_utf8_with_errors(buf, len, utf8_buffer);
  #else
  return convert_utf16le_to_utf8_with_errors(buf, len, utf8_buffer);
  #endif
}
simdutf_warn_unused result convert_utf16_to_latin1_with_errors(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_latin1_with_errors(buf, len, latin1_buffer);
  #else
  return convert_utf16le_to_latin1_with_errors(buf, len, latin1_buffer);
  #endif
}
simdutf_warn_unused result convert_utf16le_to_utf8_with_errors(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_utf8_with_errors(buf, len, utf8_buffer);
}
simdutf_warn_unused result convert_utf16be_to_utf8_with_errors(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_utf8_with_errors(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_valid_utf16be_to_utf8(buf, len, utf8_buffer);
  #else
  return convert_valid_utf16le_to_utf8(buf, len, utf8_buffer);
  #endif
}
simdutf_warn_unused size_t convert_valid_utf16_to_latin1(const char16_t * buf, size_t len, char* latin1_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_valid_utf16be_to_latin1(buf, len, latin1_buffer);
  #else
  return convert_valid_utf16le_to_latin1(buf, len, latin1_buffer);
  #endif
}
simdutf_warn_unused size_t convert_valid_utf16le_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16le_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16be_to_utf8(const char16_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16be_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_utf32_to_utf8(const char32_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused result convert_utf32_to_utf8_with_errors(const char32_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf8_with_errors(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_valid_utf32_to_utf8(const char32_t * buf, size_t len, char* utf8_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf32_to_utf8(buf, len, utf8_buffer);
}
simdutf_warn_unused size_t convert_utf32_to_utf16(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf32_to_utf16be(buf, len, utf16_buffer);
  #else
  return convert_utf32_to_utf16le(buf, len, utf16_buffer);
  #endif
}
simdutf_warn_unused size_t convert_utf32_to_latin1(const char32_t * input, size_t length, char* latin1_output) noexcept {
  return get_default_implementation()->convert_utf32_to_latin1(input, length, latin1_output);
}
simdutf_warn_unused size_t convert_utf32_to_utf16le(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf16le(buf, len, utf16_buffer);
}
simdutf_warn_unused size_t convert_utf32_to_utf16be(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf16be(buf, len, utf16_buffer);
}
simdutf_warn_unused result convert_utf32_to_utf16_with_errors(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf32_to_utf16be_with_errors(buf, len, utf16_buffer);
  #else
  return convert_utf32_to_utf16le_with_errors(buf, len, utf16_buffer);
  #endif
}
simdutf_warn_unused result convert_utf32_to_utf16le_with_errors(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf16le_with_errors(buf, len, utf16_buffer);
}
simdutf_warn_unused result convert_utf32_to_utf16be_with_errors(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_utf32_to_utf16be_with_errors(buf, len, utf16_buffer);
}
simdutf_warn_unused size_t convert_valid_utf32_to_utf16(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_valid_utf32_to_utf16be(buf, len, utf16_buffer);
  #else
  return convert_valid_utf32_to_utf16le(buf, len, utf16_buffer);
  #endif
}
simdutf_warn_unused size_t convert_valid_utf32_to_utf16le(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf32_to_utf16le(buf, len, utf16_buffer);
}
simdutf_warn_unused size_t convert_valid_utf32_to_utf16be(const char32_t * buf, size_t len, char16_t* utf16_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf32_to_utf16be(buf, len, utf16_buffer);
}
simdutf_warn_unused size_t convert_utf16_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_utf32(buf, len, utf32_buffer);
  #else
  return convert_utf16le_to_utf32(buf, len, utf32_buffer);
  #endif
}
simdutf_warn_unused size_t convert_utf16le_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_utf32(buf, len, utf32_buffer);
}
simdutf_warn_unused size_t convert_utf16be_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_utf32(buf, len, utf32_buffer);
}
simdutf_warn_unused result convert_utf16_to_utf32_with_errors(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_utf16be_to_utf32_with_errors(buf, len, utf32_buffer);
  #else
  return convert_utf16le_to_utf32_with_errors(buf, len, utf32_buffer);
  #endif
}
simdutf_warn_unused result convert_utf16le_to_utf32_with_errors(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_utf16le_to_utf32_with_errors(buf, len, utf32_buffer);
}
simdutf_warn_unused result convert_utf16be_to_utf32_with_errors(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_utf16be_to_utf32_with_errors(buf, len, utf32_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return convert_valid_utf16be_to_utf32(buf, len, utf32_buffer);
  #else
  return convert_valid_utf16le_to_utf32(buf, len, utf32_buffer);
  #endif
}
simdutf_warn_unused size_t convert_valid_utf16le_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16le_to_utf32(buf, len, utf32_buffer);
}
simdutf_warn_unused size_t convert_valid_utf16be_to_utf32(const char16_t * buf, size_t len, char32_t* utf32_buffer) noexcept {
  return get_default_implementation()->convert_valid_utf16be_to_utf32(buf, len, utf32_buffer);
}
void change_endianness_utf16(const char16_t * input, size_t length, char16_t * output) noexcept {
  get_default_implementation()->change_endianness_utf16(input, length, output);
}
simdutf_warn_unused size_t count_utf16(const char16_t * input, size_t length) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return count_utf16be(input, length);
  #else
  return count_utf16le(input, length);
  #endif
}
simdutf_warn_unused size_t count_utf16le(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->count_utf16le(input, length);
}
simdutf_warn_unused size_t count_utf16be(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->count_utf16be(input, length);
}
simdutf_warn_unused size_t count_utf8(const char * input, size_t length) noexcept {
  return get_default_implementation()->count_utf8(input, length);
}
simdutf_warn_unused size_t latin1_length_from_utf8(const char * buf, size_t len) noexcept {
  return get_default_implementation()->latin1_length_from_utf8(buf, len);
}
simdutf_warn_unused size_t latin1_length_from_utf16(size_t len) noexcept {
  return get_default_implementation()->latin1_length_from_utf16(len);
}
simdutf_warn_unused size_t latin1_length_from_utf32(size_t len) noexcept {
  return get_default_implementation()->latin1_length_from_utf32(len);
}
simdutf_warn_unused size_t utf8_length_from_latin1(const char * buf, size_t len) noexcept {
  return get_default_implementation()->utf8_length_from_latin1(buf, len);
}
simdutf_warn_unused size_t utf8_length_from_utf16(const char16_t * input, size_t length) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return utf8_length_from_utf16be(input, length);
  #else
  return utf8_length_from_utf16le(input, length);
  #endif
}
simdutf_warn_unused size_t utf8_length_from_utf16le(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->utf8_length_from_utf16le(input, length);
}
simdutf_warn_unused size_t utf8_length_from_utf16be(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->utf8_length_from_utf16be(input, length);
}
simdutf_warn_unused size_t utf32_length_from_utf16(const char16_t * input, size_t length) noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return utf32_length_from_utf16be(input, length);
  #else
  return utf32_length_from_utf16le(input, length);
  #endif
}
simdutf_warn_unused size_t utf32_length_from_utf16le(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->utf32_length_from_utf16le(input, length);
}
simdutf_warn_unused size_t utf32_length_from_utf16be(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->utf32_length_from_utf16be(input, length);
}
simdutf_warn_unused size_t utf16_length_from_utf8(const char * input, size_t length) noexcept {
  return get_default_implementation()->utf16_length_from_utf8(input, length);
}
simdutf_warn_unused size_t utf16_length_from_latin1(size_t length) noexcept {
  return get_default_implementation()->utf16_length_from_latin1(length);
}
simdutf_warn_unused size_t utf8_length_from_utf32(const char32_t * input, size_t length) noexcept {
  return get_default_implementation()->utf8_length_from_utf32(input, length);
}
simdutf_warn_unused size_t utf16_length_from_utf32(const char32_t * input, size_t length) noexcept {
  return get_default_implementation()->utf16_length_from_utf32(input, length);
}
simdutf_warn_unused size_t utf32_length_from_utf8(const char * input, size_t length) noexcept {
  return get_default_implementation()->utf32_length_from_utf8(input, length);
}

simdutf_warn_unused size_t maximal_binary_length_from_base64(const char * input, size_t length) noexcept {
  return get_default_implementation()->maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result base64_to_binary(const char * input, size_t length, char* output, base64_options options) noexcept {
  return get_default_implementation()->base64_to_binary(input, length, output, options);
}

simdutf_warn_unused size_t maximal_binary_length_from_base64(const char16_t * input, size_t length) noexcept {
  return get_default_implementation()->maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options) noexcept {
  return get_default_implementation()->base64_to_binary(input, length, output, options);
}

template <typename chartype>
simdutf_warn_unused result base64_to_binary_safe_impl(const chartype * input, size_t length, char* output, size_t& outlen, base64_options options) noexcept {
  static_assert(std::is_same<chartype, char>::value || std::is_same<chartype, char16_t>::value, "Only char and char16_t are supported.");
  // The implementation could be nicer, but we expect that most times, the user
  // will provide us with a buffer that is large enough.
  size_t max_length = maximal_binary_length_from_base64(input, length);
  if(outlen >= max_length) {
    // fast path
    result r = base64_to_binary(input, length, output, options);
    if(r.error != error_code::INVALID_BASE64_CHARACTER) { outlen = r.count; r.count = length; }
    return r;
  }
  // The output buffer is maybe too small. We will decode a truncated version of the input.
  size_t outlen3 = outlen / 3 * 3; // round down to multiple of 3
  size_t safe_input = base64_length_from_binary(outlen3, options);
  result r = base64_to_binary(input, safe_input, output, options);
  if(r.error == error_code::INVALID_BASE64_CHARACTER) { return r; }
  size_t offset = (r.error == error_code::BASE64_INPUT_REMAINDER) ? 1 :
    ((r.count % 3) == 0 ? 0 : (r.count % 3) + 1);
  size_t output_index = r.count - (r.count % 3);
  size_t input_index = safe_input;
  // offset is a value that is no larger than 3. We backtrack
  // by up to offset characters + an undetermined number of
  // white space characters. It is expected that the next loop
  // runs at most 3 times + the number of white space characters
  // in between them, so we are not worried about performance.
  while(offset > 0 && input_index > 0) {
    chartype c = input[--input_index];
    if(scalar::base64::is_ascii_white_space(c)){
      // skipping
    } else {
      offset--;
    }
  }
  size_t remaining_out = outlen - output_index;
  const chartype * tail_input = input + input_index;
  size_t tail_length = length - input_index;
  while(tail_length > 0 && scalar::base64::is_ascii_white_space(tail_input[tail_length - 1])) {
    tail_length--;
  }
  size_t padding_characts = 0;
  if(tail_length > 0 && tail_input[tail_length - 1] == '=') {
    tail_length--;
    padding_characts++;
    while(tail_length > 0 && scalar::base64::is_ascii_white_space(tail_input[tail_length - 1])) {
      tail_length--;
    }
    if(tail_length > 0 && tail_input[tail_length - 1] == '=') {
      tail_length--;
      padding_characts++;
    }
  }
  r = scalar::base64::base64_tail_decode_safe(output + output_index, remaining_out, tail_input, tail_length, options);
  outlen = output_index + remaining_out;
  if(r.error == error_code::SUCCESS && padding_characts > 0) {
    // additional checks
    if((outlen % 3 == 0) || ((outlen % 3) + 1 + padding_characts != 4)) {
      r.error = error_code::INVALID_BASE64_CHARACTER;
    }
  }
  r.count += input_index;
  return r;
}



simdutf_warn_unused size_t convert_latin1_to_utf8_safe(const char * buf, size_t len, char* utf8_output, size_t utf8_len) noexcept {
  const auto start{utf8_output};

  while (true) {
    // convert_latin1_to_utf8 will never write more than input length * 2
    auto read_len = std::min(len, utf8_len >> 1);
    if (read_len <= 16) {
      break;
    }

    const auto write_len = simdutf::convert_latin1_to_utf8(buf, read_len, utf8_output);

    utf8_output += write_len;
    utf8_len -= write_len;
    buf += read_len;
    len -= read_len;
  }

  utf8_output += scalar::latin1_to_utf8::convert_safe(buf, len, utf8_output, utf8_len);

  return utf8_output - start;
}


simdutf_warn_unused result base64_to_binary_safe(const char * input, size_t length, char* output, size_t& outlen, base64_options options) noexcept {
  return base64_to_binary_safe_impl<char>(input, length, output, outlen, options);
}
simdutf_warn_unused result base64_to_binary_safe(const char16_t * input, size_t length, char* output, size_t& outlen, base64_options options) noexcept {
  return base64_to_binary_safe_impl<char16_t>(input, length, output, outlen, options);
}

simdutf_warn_unused size_t base64_length_from_binary(size_t length, base64_options options) noexcept {
  return get_default_implementation()->base64_length_from_binary(length, options);
}

size_t binary_to_base64(const char * input, size_t length, char* output, base64_options options) noexcept {
  return get_default_implementation()->binary_to_base64(input, length, output, options);
}

simdutf_warn_unused simdutf::encoding_type autodetect_encoding(const char * buf, size_t length) noexcept {
  return get_default_implementation()->autodetect_encoding(buf, length);
}
simdutf_warn_unused int detect_encodings(const char * buf, size_t length) noexcept {
  return get_default_implementation()->detect_encodings(buf, length);
}
const implementation * builtin_implementation() {
  static const implementation * builtin_impl = get_available_implementations()[SIMDUTF_STRINGIFY(SIMDUTF_BUILTIN_IMPLEMENTATION)];
  return builtin_impl;
}

simdutf_warn_unused size_t trim_partial_utf8(const char *input, size_t length) {
  return scalar::utf8::trim_partial_utf8(input, length);
}

simdutf_warn_unused size_t trim_partial_utf16be(const char16_t* input, size_t length) {
  return scalar::utf16::trim_partial_utf16<BIG>(input, length);
}

simdutf_warn_unused size_t trim_partial_utf16le(const char16_t* input, size_t length) {
  return scalar::utf16::trim_partial_utf16<LITTLE>(input, length);
}

simdutf_warn_unused size_t trim_partial_utf16(const char16_t* input, size_t length) {
  #if SIMDUTF_IS_BIG_ENDIAN
  return trim_partial_utf16be(input, length);
  #else
  return trim_partial_utf16le(input, length);
  #endif
}

} // namespace simdutf

