// this fuzzes the convert_ functions
// by Paul Dreik 2024

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <span>
#include <vector>

#include "helpers/common.h"
#include "helpers/nameof.hpp"

#include "simdutf.h"

// these knobs tweak how the fuzzer works
constexpr bool allow_implementations_to_differ = false;
constexpr bool use_canary_in_output = true;
constexpr bool use_separate_allocation = true;

enum class UtfEncodings { UTF16BE, UTF16LE, UTF8, UTF32, LATIN1 };

template <UtfEncodings encoding> struct ValidationFunctionTrait {};

template <> struct ValidationFunctionTrait<UtfEncodings::UTF16BE> {
  static inline auto Validation = &simdutf::implementation::validate_utf16be;
  static inline auto ValidationWithErrors =
      &simdutf::implementation::validate_utf16be_with_errors;
  static inline std::string ValidationWithErrorsName{
      NAMEOF(&simdutf::implementation::validate_utf16be_with_errors)};
  static inline std::string ValidationName{
      NAMEOF(&simdutf::implementation::validate_utf16be)};
  using RawType = char16_t;
};
template <> struct ValidationFunctionTrait<UtfEncodings::UTF16LE> {
  static inline auto Validation = &simdutf::implementation::validate_utf16le;
  static inline auto ValidationWithErrors =
      &simdutf::implementation::validate_utf16le_with_errors;
  static inline std::string ValidationWithErrorsName{
      NAMEOF(&simdutf::implementation::validate_utf16le_with_errors)};
  static inline std::string ValidationName{
      NAMEOF(&simdutf::implementation::validate_utf16le)};
  using RawType = char16_t;
};
template <> struct ValidationFunctionTrait<UtfEncodings::UTF32> {
  static inline auto Validation = &simdutf::implementation::validate_utf32;
  static inline auto ValidationWithErrors =
      &simdutf::implementation::validate_utf32_with_errors;
  static inline std::string ValidationWithErrorsName{
      NAMEOF(&simdutf::implementation::validate_utf32_with_errors)};
  static inline std::string ValidationName{
      NAMEOF(&simdutf::implementation::validate_utf32)};
  using RawType = char32_t;
};
template <> struct ValidationFunctionTrait<UtfEncodings::UTF8> {
  static inline auto Validation = &simdutf::implementation::validate_utf8;
  static inline auto ValidationWithErrors =
      &simdutf::implementation::validate_utf8_with_errors;
  static inline std::string ValidationWithErrorsName{
      NAMEOF(&simdutf::implementation::validate_utf8_with_errors)};
  static inline std::string ValidationName{
      NAMEOF(&simdutf::implementation::validate_utf8)};
  using RawType = char;
};
template <> struct ValidationFunctionTrait<UtfEncodings::LATIN1> {
  // note - there are no validation functions for latin1, all input is valid.
  using RawType = char;
};

constexpr std::string_view nameoftype(char) { return "char"; }
constexpr std::string_view nameoftype(char16_t) { return "char16_t"; }
constexpr std::string_view nameoftype(char32_t) { return "char32_t"; }

/// given the name of a conversion function, return the enum describing the
/// *from* type. must be a macro because of string view not being sufficiently
/// constexpr.
#define ENCODING_FROM_CONVERSION_NAME(x)                                       \
  []() {                                                                       \
    using sv = std::string_view;                                               \
    using enum UtfEncodings;                                                   \
    if constexpr (sv{NAMEOF(x)}.find("utf16be_to") != sv::npos) {              \
      return UTF16BE;                                                          \
    } else if constexpr (sv{NAMEOF(x)}.find("utf16le_to") != sv::npos) {       \
      return UTF16LE;                                                          \
    } else if constexpr (sv{NAMEOF(x)}.find("utf32_to") != sv::npos) {         \
      return UTF32;                                                            \
    } else if constexpr (sv{NAMEOF(x)}.find("utf8_to") != sv::npos) {          \
      return UTF8;                                                             \
    } else if constexpr (sv{NAMEOF(x)}.find("latin1_to") != sv::npos) {        \
      return LATIN1;                                                           \
    } else {                                                                   \
      throw "oops";                                                            \
    }                                                                          \
  }()

/// given the name of a conversion function, return the enum describing the
/// *to* type. must be a macro because of string view not being sufficiently
/// constexpr.
#define ENCODING_TO_CONVERSION_NAME(x)                                         \
  []() {                                                                       \
    using sv = std::string_view;                                               \
    using enum UtfEncodings;                                                   \
    if constexpr (sv{NAMEOF(x)}.find("to_utf16be") != sv::npos) {              \
      return UTF16BE;                                                          \
    } else if constexpr (sv{NAMEOF(x)}.find("to_utf16le") != sv::npos) {       \
      return UTF16LE;                                                          \
    } else if constexpr (sv{NAMEOF(x)}.find("to_utf32") != sv::npos) {         \
      return UTF32;                                                            \
    } else if constexpr (sv{NAMEOF(x)}.find("to_utf8") != sv::npos) {          \
      return UTF8;                                                             \
    } else if constexpr (sv{NAMEOF(x)}.find("to_latin1") != sv::npos) {        \
      return LATIN1;                                                           \
    } else {                                                                   \
      throw "oops";                                                            \
    }                                                                          \
  }()

template <typename R> struct result {
  R retval{};
  std::string outputhash;
  auto operator<=>(const result<R>&) const = default;
};

template <typename R>
std::ostream& operator<<(std::ostream& os, const result<R>& r) {
  os << "[retval=" << r.retval << ", output hash=" << r.outputhash << "]";
  return os;
}

template <UtfEncodings From, UtfEncodings To,
          member_function_pointer LengthFunction,
          member_function_pointer ConversionFunction>
struct Conversion {
  LengthFunction lengthcalc;
  ConversionFunction conversion;
  std::string lengthcalcname;
  std::string name;

  using FromType = ValidationFunctionTrait<From>::RawType;
  using ToType = ValidationFunctionTrait<To>::RawType;

  using FromSpan = std::span<const FromType>;

  using ConversionResult =
      std::invoke_result<ConversionFunction, const simdutf::implementation*,
                         const FromType*, std::size_t, ToType*>::type;

  struct validation_result {
    bool valid{};
    bool implementations_agree{};
  };

  struct length_result {
    std::vector<std::size_t> length{};
    bool implementations_agree{};
  };

  struct conversion_result {
    std::size_t written{};
    bool implementations_agree{};
  };

  void fuzz(std::span<const char> chardata) const {
    // assume the input is aligned to FromType
    const FromSpan from{reinterpret_cast<const FromType*>(chardata.data()),
                        chardata.size() / sizeof(FromType)};

    static const bool do_print_testcase =
        std::getenv("PRINT_FUZZ_CASE") != nullptr;

    if (do_print_testcase) {
      dump_testcase(from, std::cerr);
      std::exit(EXIT_SUCCESS);
    }

    do {
      // step 0 - is the input valid?
      const auto [inputisvalid, valid_input_agree] = verify_valid_input(from);
      if (!valid_input_agree && !allow_implementations_to_differ)
        break;

      // step 1 - count the input (only makes sense for some of the encodings)
      if constexpr (From == UtfEncodings::UTF16BE ||
                    From == UtfEncodings::UTF16LE ||
                    From == UtfEncodings::UTF8) {
        if (!count_the_input(from) && !allow_implementations_to_differ)
          break;
      }

      // step 2 - what is the required size of the output?
      const auto [output_length, length_agree] =
          calculate_length(from, inputisvalid);
      if (!length_agree && !allow_implementations_to_differ)
        break;

      if (!inputisvalid && name.find("valid") != std::string::npos) {
        // don't run the conversion step, it requires valid input
        return;
      }

      // step 3 - run the conversion
      const auto [written, outputs_agree] =
          do_conversion(from, output_length, inputisvalid);
      if (!outputs_agree && !allow_implementations_to_differ)
        break;

      // coming this far means no problems were found
      return;
    } while (0);
    // if we come here, something failed
    std::cerr << "something failed, rerun with PRINT_FUZZ_CASE set to print a "
                 "reproducer to stderr\n";
    std::abort();
  }

  template <typename Dummy = void>
    requires(From != UtfEncodings::LATIN1)
  validation_result verify_valid_input(FromSpan src) const {
    validation_result ret{};

    auto input_validation = ValidationFunctionTrait<From>::ValidationWithErrors;
    const auto implementations = get_supported_implementations();
    std::vector<simdutf::result> results;
    results.reserve(implementations.size());

    for (auto impl : implementations) {
      results.push_back(
          std::invoke(input_validation, impl, src.data(), src.size()));

      // make sure the validation variant that returns a bool agrees
      const bool validation1 = results.back().error == simdutf::SUCCESS;
      const bool validation2 =
          std::invoke(ValidationFunctionTrait<From>::Validation, impl,
                      src.data(), src.size());
      if (validation1 != validation2) {
        std::cerr << "begin errormessage for verify_valid_input()\n";
        std::cerr << ValidationFunctionTrait<From>::ValidationWithErrorsName
                  << " gives " << validation1 << " while "
                  << ValidationFunctionTrait<From>::ValidationName << " gave "
                  << validation2 << " for implementation " << impl->name()
                  << '\n';
        std::cerr << "end errormessage\n";
        std::abort();
      }
    }

    auto neq = [](const auto& a, const auto& b) { return a != b; };
    if (std::ranges::adjacent_find(results, neq) != results.end()) {
      std::cerr << "begin errormessage for verify_valid_input()\n";
      std::cerr << "in fuzz case for "
                << ValidationFunctionTrait<From>::ValidationWithErrorsName
                << " invoked with " << src.size() << " elements:\n";
      for (std::size_t i = 0; i < results.size(); ++i) {
        std::cerr << "got return " << std::dec << results[i]
                  << " from implementation " << implementations[i]->name()
                  << '\n';
      }
      std::cerr << "end errormessage\n";
      ret.implementations_agree = false;
    } else {
      ret.implementations_agree = true;
    }
    ret.valid = std::ranges::all_of(results, [](const simdutf::result& r) {
      return r.error == simdutf::SUCCESS;
    });
    return ret;
  }

  template <typename Dummy = void>
    requires(From == UtfEncodings::LATIN1)
  validation_result verify_valid_input(FromSpan) const {
    // all latin1 input is valid. there is no simdutf validation function for
    // it.
    return validation_result{.valid = true, .implementations_agree = true};
  }

  bool count_the_input(FromSpan src) const {
    const auto implementations = get_supported_implementations();
    std::vector<std::size_t> results;
    results.reserve(implementations.size());

    for (auto impl : implementations) {
      std::size_t ret;
      if constexpr (From == UtfEncodings::UTF16BE) {
        ret = impl->count_utf16be(src.data(), src.size());
      } else if constexpr (From == UtfEncodings::UTF16LE) {
        ret = impl->count_utf16le(src.data(), src.size());
      } else if constexpr (From == UtfEncodings::UTF8) {
        ret = impl->count_utf8(src.data(), src.size());
      }
      results.push_back(ret);
    }
    auto neq = [](const auto& a, const auto& b) { return a != b; };
    if (std::ranges::adjacent_find(results, neq) != results.end()) {
      std::cerr << "begin errormessage for count_the_input()\n";
      std::cerr << "in fuzz case for "
                << ValidationFunctionTrait<From>::ValidationWithErrorsName
                << " invoked with " << src.size() << " elements:\n";
      for (std::size_t i = 0; i < results.size(); ++i) {
        std::cerr << "got return " << std::dec << results[i]
                  << " from implementation " << implementations[i]->name()
                  << '\n';
      }
      std::cerr << "end errormessage\n";
      return false;
    }

    return true;
  }

  // this quirk is needed because length calculations do not have consistent
  // signatures since some of them do not look at the input data, just the
  // length of it.
  template <typename Dummy = void>
    requires std::is_invocable_v<LengthFunction, const simdutf::implementation*,
                                 const FromType*, std::size_t>
  std::size_t invoke_lengthcalc(const simdutf::implementation* impl,
                                FromSpan src) const {
    return std::invoke(lengthcalc, impl, src.data(), src.size());
  }
  template <typename Dummy = void>
    requires std::is_invocable_v<LengthFunction, const simdutf::implementation*,
                                 // const FromType *,
                                 std::size_t>
  std::size_t invoke_lengthcalc(const simdutf::implementation* impl,
                                FromSpan src) const {
    return std::invoke(lengthcalc, impl, /*src.data(),*/ src.size());
  }

  length_result calculate_length(FromSpan src, const bool inputisvalid) const {
    length_result ret{};

    const auto implementations = get_supported_implementations();
    std::vector<std::size_t> results;
    results.reserve(implementations.size());

    for (auto impl : implementations) {
      const auto len = invoke_lengthcalc(impl, src);
      results.push_back(len);
      ret.length.push_back(len);
    }

    auto neq = [](const auto& a, const auto& b) { return a != b; };
    if (std::ranges::adjacent_find(results, neq) != results.end()) {
      std::cerr << "begin errormessage for calculate_length\n";
      std::cerr << "in fuzz case invoking " << lengthcalcname << " with "
                << src.size() << " elements with valid input=" << inputisvalid
                << ":\n";
      for (std::size_t i = 0; i < results.size(); ++i) {
        std::cerr << "got return " << std::dec << results[i]
                  << " from implementation " << implementations[i]->name()
                  << '\n';
      }
      std::cerr << "end errormessage\n";
      if (inputisvalid) {
        ret.implementations_agree = false;
      } else {
        std::cerr
            << "impementations are allowed to disagree on invalid input\n";
        ret.implementations_agree = true;
      }
    } else {
      ret.implementations_agree = true;
    }
    return ret;
  }

  conversion_result do_conversion(FromSpan src,
                                  const std::vector<std::size_t>& outlength,
                                  const bool inputisvalid) const {
    conversion_result ret{};

    const auto implementations = get_supported_implementations();

    std::vector<result<ConversionResult>> results;
    results.reserve(implementations.size());

    // put the output in a separate allocation to make access violations easier
    // to catch
    std::vector<std::vector<ToType>> outputbuffers;
    outputbuffers.reserve(implementations.size());
    for (std::size_t i = 0; i < implementations.size(); ++i) {
      auto impl = implementations[i];
      const ToType canary1{42};
      auto& outputbuffer = outputbuffers.emplace_back(outlength.at(i), canary1);
      const auto implret1 = std::invoke(conversion, impl, src.data(),
                                        src.size(), outputbuffer.data());
      // was the conversion successful?
      const auto success = [](const ConversionResult& r) -> bool {
        if constexpr (std::is_same_v<ConversionResult, std::size_t>) {
          return r != 0;
        } else {
          return r.error == simdutf::error_code::SUCCESS;
        }
      }(implret1);
      const auto hash1 = FNV1A_hash::as_str(outputbuffer);
      if constexpr (use_canary_in_output) {
        // optionally convert again, this time with the buffer filled with
        // a different value. if the output differs, it means some of the buffer
        // was not written to by the conversion function.
        const ToType canary2{25};
        const auto outputbuffer_first_run = outputbuffer;
        std::ranges::fill(outputbuffer, canary2);
        const auto implret2 = std::invoke(conversion, impl, src.data(),
                                          src.size(), outputbuffer.data());

        if (implret1 != implret2) {
          std::cerr << "different return value the second time!\n";
          std::abort();
        }
        if (inputisvalid && success) {
          // only care about the output if the input is valid
          const auto hash2 = FNV1A_hash::as_str(outputbuffer);
          if (hash1 != hash2) {
            std::cerr << "different output the second time!\n";
            std::cerr << "implementation " << impl->name() << " " << name
                      << '\n';
            std::cerr << "input is valid=" << inputisvalid << '\n';
            std::cerr << "output length=" << outputbuffer.size() << '\n';
            std::cerr << "conversion was a success? " << success << '\n';
            for (std::size_t j = 0; j < outputbuffer.size(); ++j) {
              std::cerr << "output[" << j << "]\t" << +outputbuffer_first_run[j]
                        << '\t' << +outputbuffer[j] << '\n';
            }
            std::abort();
          }
        }
      }
      results.emplace_back(implret1, success ? hash1 : "");
    }

    // do not require implementations to give the same output if
    // the input is not valid.
    if (!inputisvalid) {
      for (auto& e : results) {
        e.outputhash.clear();
      }
    }

    auto neq = [](const auto& a, const auto& b) { return a != b; };
    if (std::ranges::adjacent_find(results, neq) != results.end()) {
      std::cerr << "begin errormessage for do_conversion\n";
      std::cerr << "in fuzz case for " << name << " invoked with " << src.size()
                << " elements:\n";
      std::cerr << "input data is valid ? " << inputisvalid << '\n';
      for (std::size_t i = 0; i < results.size(); ++i) {
        std::cerr << "got return " << std::dec << results[i]
                  << " from implementation " << implementations[i]->name()
                  << " using outlen=" << outlength.at(i) << '\n';
      }
      for (std::size_t i = 0; i < results.size(); ++i) {
        std::cerr << "implementation " << implementations[i]->name()
                  << " out: ";
        for (const auto e : outputbuffers.at(i)) {
          std::cerr << +e << ", ";
        }
        std::cerr << '\n';
      }
      std::cerr << "end errormessage\n";
      ret.implementations_agree = false;
    } else {
      ret.implementations_agree = true;
    }
    return ret;
  }

  void dump_testcase(FromSpan typedspan, std::ostream& os) const {
    const auto testhash = FNV1A_hash::as_str(name, typedspan);

    os << "// begin testcase\n";
    os << "TEST(issue_" << name << "_" << testhash << ") {\n";
    os << " alignas(" << sizeof(FromType) << ") const unsigned char data[]={";
    const auto first = reinterpret_cast<const unsigned char*>(typedspan.data());
    const auto last = first + typedspan.size_bytes();
    for (auto it = first; it != last; ++it) {
      os << "0x" << std::hex << std::setfill('0') << std::setw(2) << (+*it)
         << (it + 1 == last ? "};\n" : ", ");
    }
    os << " constexpr std::size_t data_len_bytes=sizeof(data);\n";
    os << " constexpr std::size_t data_len=data_len_bytes/sizeof("
       << nameoftype(FromType{}) << ");\n";
    if constexpr (From != UtfEncodings::LATIN1) {
      os << "const auto validation1=implementation."
         << ValidationFunctionTrait<From>::ValidationWithErrorsName
         << "((const " << nameoftype(FromType{}) << "*) data,\n data_len);\n";
      os << "   ASSERT_EQUAL(validation1.count, 1234);\n";
      os << "   ASSERT_EQUAL(validation1.error, "
            "simdutf::error_code::SUCCESS);\n";
      os << '\n';
      os << "const bool validation2=implementation."
         << ValidationFunctionTrait<From>::ValidationName << "((const "
         << nameoftype(FromType{}) << "*) data,\n data_len);\n";
      os << "   "
            "ASSERT_EQUAL(validation1.error==simdutf::error_code::SUCCESS,"
            "validation2);\n";
      os << '\n';
      os << " if(validation1.error!= simdutf::error_code::SUCCESS) {return;}\n";
    }

    if (std::is_invocable_v<LengthFunction, const simdutf::implementation*,
                            const FromType*, std::size_t>) {
      os << "const auto outlen=implementation." << lengthcalcname << "((const "
         << nameoftype(FromType{}) << "*) data,\n data_len);\n";
    } else if (std::is_invocable_v<LengthFunction,
                                   const simdutf::implementation*,
                                   std::size_t>) {
      os << "const auto outlen=implementation." << lengthcalcname
         << "(data_len);\n";
    } else {
      // programming error
      std::abort();
    }
    os << "ASSERT_EQUAL(outlen, 1234);\n";
    os << "std::vector<" << nameoftype(ToType{}) << "> output(outlen);\n";
    os << "const auto r = implementation." << name << "((const "
       << nameoftype(FromType{}) << "*) data\n, data_len\n, output.data());\n";

    if constexpr (std::is_same_v<ConversionResult, simdutf::result>) {
      os << " ASSERT_EQUAL(r.error,simdutf::error_code::SUCCESS);\n";
      os << " ASSERT_EQUAL(r.count,1234);\n";
    } else {
      os << "   ASSERT_EQUAL(r, 1234);\n";
    }

    // dump the output data
    os << "const std::vector<" << nameoftype(ToType{}) << "> expected_out{};\n";
    os << " ASSERT_TRUE(output.size()==expected_out.size());\n";
    os << " for(std::size_t i=0; i<output.size(); ++i) { "
          "ASSERT_EQUAL(+output.at(i),+expected_out.at(i));};\n";

    os << "}\n";
    os << "// end testcase\n";
  }
};

const auto populate_functions() {
  using I = simdutf::implementation;
  using FuzzSignature = void (*)(std::span<const char>);

#define ADD(lenfunc, conversionfunc)                                           \
  FuzzSignature {                                                              \
    +[](std::span<const char> chardata) {                                      \
      const auto c =                                                           \
          Conversion<ENCODING_FROM_CONVERSION_NAME(&I::conversionfunc),        \
                     ENCODING_TO_CONVERSION_NAME(&I::conversionfunc),          \
                     decltype(&I::lenfunc), decltype(&I::conversionfunc)>{     \
              &I::lenfunc, &I::conversionfunc,                                 \
              std::string{NAMEOF(&I::lenfunc)},                                \
              std::string{NAMEOF(&I::conversionfunc)}};                        \
      c.fuzz(chardata);                                                        \
    }                                                                          \
  }

  return std::array{
      // all these cases require valid input for invoking the convert function

      // see #493
      // IGNORE(latin1_length_from_utf16, convert_valid_utf16be_to_latin1),
      ADD(utf32_length_from_utf16be, convert_valid_utf16be_to_utf32),
      ADD(utf8_length_from_utf16be, convert_valid_utf16be_to_utf8),

      //  see #493
      // IGNORE(latin1_length_from_utf16, convert_valid_utf16le_to_latin1),
      ADD(utf32_length_from_utf16le, convert_valid_utf16le_to_utf32),
      ADD(utf8_length_from_utf16le, convert_valid_utf16le_to_utf8),

      // see #493
      // IGNORE(latin1_length_from_utf32, convert_valid_utf32_to_latin1),
      ADD(utf16_length_from_utf32, convert_valid_utf32_to_utf16be),
      ADD(utf16_length_from_utf32, convert_valid_utf32_to_utf16le),
      ADD(utf8_length_from_utf32, convert_valid_utf32_to_utf8),

      // see #493
      // IGNORE(latin1_length_from_utf8, convert_valid_utf8_to_latin1),
      ADD(utf16_length_from_utf8, convert_valid_utf8_to_utf16be),
      ADD(utf16_length_from_utf8, convert_valid_utf8_to_utf16le),
      ADD(utf32_length_from_utf8, convert_valid_utf8_to_utf32),

      // all these cases operate on arbitrary data
      ADD(latin1_length_from_utf16, convert_utf16be_to_latin1),
      ADD(utf32_length_from_utf16be, convert_utf16be_to_utf32),
      ADD(utf8_length_from_utf16be, convert_utf16be_to_utf8),

      ADD(latin1_length_from_utf16, convert_utf16le_to_latin1),
      ADD(utf32_length_from_utf16le, convert_utf16le_to_utf32),
      ADD(utf8_length_from_utf16le, convert_utf16le_to_utf8),

      ADD(latin1_length_from_utf32, convert_utf32_to_latin1),
      ADD(utf16_length_from_utf32, convert_utf32_to_utf16be),
      ADD(utf16_length_from_utf32, convert_utf32_to_utf16le),
      ADD(utf8_length_from_utf32, convert_utf32_to_utf8),

      ADD(latin1_length_from_utf8, convert_utf8_to_latin1),
      ADD(utf16_length_from_utf8, convert_utf8_to_utf16be),
      ADD(utf16_length_from_utf8, convert_utf8_to_utf16le),
      ADD(utf32_length_from_utf8, convert_utf8_to_utf32),

      // all these cases operate on arbitrary data and use the _with_errors
      // variant
      ADD(latin1_length_from_utf16, convert_utf16be_to_latin1_with_errors),
      ADD(utf32_length_from_utf16be, convert_utf16be_to_utf32_with_errors),
      ADD(utf8_length_from_utf16be, convert_utf16be_to_utf8_with_errors),

      ADD(latin1_length_from_utf16, convert_utf16le_to_latin1_with_errors),
      ADD(utf32_length_from_utf16le, convert_utf16le_to_utf32_with_errors),
      ADD(utf8_length_from_utf16le, convert_utf16le_to_utf8_with_errors),

      ADD(latin1_length_from_utf32, convert_utf32_to_latin1_with_errors),
      ADD(utf16_length_from_utf32, convert_utf32_to_utf16be_with_errors),
      ADD(utf16_length_from_utf32, convert_utf32_to_utf16le_with_errors),
      ADD(utf8_length_from_utf32, convert_utf32_to_utf8_with_errors),

      ADD(latin1_length_from_utf8, convert_utf8_to_latin1_with_errors),
      ADD(utf16_length_from_utf8, convert_utf8_to_utf16be_with_errors),
      ADD(utf16_length_from_utf8, convert_utf8_to_utf16le_with_errors),
      ADD(utf32_length_from_utf8, convert_utf8_to_utf32_with_errors),

      // these are a bit special since all input is valid
      ADD(utf32_length_from_latin1, convert_latin1_to_utf32),
      ADD(utf16_length_from_latin1, convert_latin1_to_utf16be),
      ADD(utf16_length_from_latin1, convert_latin1_to_utf16le),
      ADD(utf8_length_from_latin1, convert_latin1_to_utf8)};

#undef ADD
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const auto fptrs = populate_functions();
  constexpr std::size_t Ncases = fptrs.size();

  // pick one of the function pointers, based on the fuzz data
  // the first byte is which action to take. step forward
  // several bytes so the input is aligned.
  if (size < 4) {
    return 0;
  }

  constexpr auto actionmask = std::bit_ceil(Ncases) - 1;
  const auto action = data[0] & actionmask;
  data += 4;
  size -= 4;

  if (action >= Ncases) {
    return 0;
  }

  if constexpr (use_separate_allocation) {
    // this is better at excercising null input and catch buffer underflows
    const std::vector<char> separate{data, data + size};
    fptrs[action](std::span(separate));
  } else {
    std::span<const char> chardata{(const char*)data, size};
    fptrs[action](chardata);
  }

  return 0;
}
