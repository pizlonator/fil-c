#pragma once
#include "benchmark_base.h"
#include "cmdline.h"
#include "simdutf.h"
#include <cstddef>

/**
 * ICU is a standard library that is often already present on the system.
 */
#if defined __has_include
#if !defined(ICU_AVAILABLE) && __has_include (<unicode/unistr.h>)
#define ICU_AVAILABLE 1
// U_ICU_VERSION is relevant here.
#endif //__has_include (<unicode/unistr.h>)

#if !defined(ICONV_AVAILABLE) && __has_include (<iconv.h>)
#define ICONV_AVAILABLE 1
#endif //__has_include (<iconv.h>)

#endif // defined __has_include

#if ICU_AVAILABLE
#include <unicode/uconfig.h>
#include <unicode/platform.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>
#endif

#if ICONV_AVAILABLE
#include <iconv.h>
#endif
/**
 * inoue2008 is:
 * Hiroshi Inoue and Hideaki Komatsu and Toshio Nakatani,
 * Accelerating UTF-8 Decoding Using SIMD Instructions (in Japanese),
 * Information Processing Society of Japan Transactions on Programming 1 (2), 2008.
 */
#include "benchmarks/competition/inoue2008/inoue_utf8_to_utf16.h"

/**
 * Nemanja Trifunovic, UTF8-CPP: UTF-8 with C++ in a Portable Way
 * https://github.com/nemtrif/utfcpp/releases/tag/v3.2.2
 */
#include "benchmarks/competition/utfcpp/source/utf8.h"

namespace simdutf::benchmarks {

    class Benchmark : public BenchmarkBase {
    public:
        using BenchmarkBase::run;

        Benchmark(std::vector<input::Testcase>&& testcases);

        static Benchmark create(const CommandLine& cmdline);
        virtual const std::set<std::string>& all_procedures() const override;
        virtual std::set<simdutf::encoding_type> expected_encodings(const std::string& procedure) override;

    protected:
        virtual void run(const std::string& procedure_name, size_t iterations) override;

    private:
        std::set<std::string> known_procedures;
        std::map<std::string,std::set<simdutf::encoding_type>> expected_input_encoding;

    private:
        void run_validate_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_validate_utf8_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_validate_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_validate_utf16_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_validate_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_validate_utf32_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_count_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_count_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_utf8_length_from_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_latin1_to_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_latin1_to_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_latin1_to_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_latin1_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf16_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf32_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf16_with_dynamic_allocation(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf8_to_utf32_with_dynamic_allocation(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf8_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf8_to_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf8_to_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_latin1_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf8_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf32_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf8_with_dynamic_allocation(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf16_to_utf32_with_dynamic_allocation(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf16_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf16_to_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_latin1_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_utf8_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf32_to_latin1(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf32_to_utf8(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_utf32_to_utf16_with_errors(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf32_to_utf16(const simdutf::implementation& implementation, size_t iterations);
        void run_convert_valid_utf16_to_utf32(const simdutf::implementation& implementation, size_t iterations);
        void run_detect_encodings(const simdutf::implementation& implementation, size_t iterations);
        void run_utf8_length_from_latin1_node(size_t iterations);
#if ICU_AVAILABLE
        void run_convert_latin1_to_utf8_icu(size_t iterations);
        void run_convert_latin1_to_utf16_icu(size_t iterations);
        void run_convert_latin1_to_utf32_icu(size_t iterations);
        void run_convert_utf8_to_latin1_icu(size_t iterations);
        void run_convert_utf8_to_utf16_icu(size_t iterations);
        void run_convert_utf16_to_utf8_icu(size_t iterations);
        void run_convert_utf16_to_latin1_icu(size_t iterations);
        void run_convert_utf32_to_latin1_icu(size_t iterations);
#endif
#if ICONV_AVAILABLE
        void run_convert_latin1_to_utf8_iconv(size_t iterations);
        void run_convert_latin1_to_utf16_iconv(size_t iterations);
        void run_convert_latin1_to_utf32_iconv(size_t iterations);
        void run_convert_utf8_to_latin1_iconv(size_t iterations);
        void run_convert_utf8_to_utf16_iconv(size_t iterations);
        void run_convert_utf16_to_utf8_iconv(size_t iterations);
        void run_convert_utf16_to_latin1_iconv(size_t iterations);
        void run_convert_utf32_to_latin1_iconv(size_t iterations);
#endif
#ifdef INOUE2008
        /**
         * Hiroshi Inoue and Hideaki Komatsu and Toshio Nakatani,
         * Accelerating UTF-8 Decoding Using SIMD Instructions (in Japanese),
         * Information Processing Society of Japan Transactions on Programming 1 (2), 2008.
         */
        void run_convert_valid_utf8_to_utf16_inoue2008(size_t iterations);
#endif
#ifdef __x86_64__
       /**
        * utf8lut: Vectorized UTF-8 converter.
        * by stgatilov (2019)
        *  https://dirtyhandscoding.github.io/posts/utf8lut-vectorized-utf-8-converter-introduction.html
        */
       void run_convert_utf16_to_utf8_utf8lut(size_t iterations);
       void run_convert_valid_utf16_to_utf8_utf8lut(size_t iterations);

       void run_convert_utf8_to_utf16_utf8lut(size_t iterations);
       void run_convert_utf8_to_utf32_utf8lut(size_t iterations);
       void run_convert_valid_utf8_to_utf16_utf8lut(size_t iterations);
       void run_convert_valid_utf8_to_utf32_utf8lut(size_t iterations);

       void run_convert_utf32_to_utf8_utf8lut(size_t iterations);
       void run_convert_valid_utf32_to_utf8_utf8lut(size_t iterations);

       /**
        * Bob Steagall, CppCon2018
        * https://github.com/BobSteagall/CppCon2018/
        *
        * Fast Conversion From UTF-8 with C++, DFAs, and SSE Intrinsics
        * https://www.youtube.com/watch?v=5FQ87-Ecb-A
        */
        void run_convert_utf8_to_utf16_cppcon2018(size_t iterations);
        void run_convert_utf8_to_utf32_cppcon2018(size_t iterations);
        /**
         * benchmarks/competition/u8u16 contains an open source version of u8u16, referenced in
         * Cameron, Robert D, A case study in SIMD text processing with parallel bit streams: UTF-8 to UTF-16 transcoding,
         * Proceedings of the 13th ACM SIGPLAN Symposium on Principles and practice of parallel programming, 91--98.
         */
        void run_convert_utf8_to_utf16_u8u16(size_t iterations);
        /**
         * Olivier Goffart, UTF-8 processing using SIMD (SSE4), 2012.
         * https://woboq.com/blog/utf-8-processing-using-simd.html
         */
        void run_convert_utf8_to_utf16_utf8sse4(size_t iterations);
#endif
        void run_convert_utf8_to_utf16_hoehrmann(size_t iterations);
        void run_convert_utf8_to_utf32_hoehrmann(size_t iterations);
        /**
         * LLVM relies on code from the Unicode Consortium
         * https://en.wikipedia.org/wiki/Unicode_Consortium
         */
        void run_convert_utf8_to_utf16_llvm(size_t iterations);
        void run_convert_utf8_to_utf32_llvm(size_t iterations);
        void run_convert_utf16_to_utf8_llvm(size_t iterations);
        void run_convert_utf32_to_utf8_llvm(size_t iterations);
        void run_convert_utf32_to_utf16_llvm(size_t iterations);
        void run_convert_utf16_to_utf32_llvm(size_t iterations);
        /**
        * Nemanja Trifunovic, UTF8-CPP: UTF-8 with C++ in a Portable Way
        * https://github.com/nemtrif/utfcpp/releases/tag/v3.2.2
        */
        void run_convert_utf8_to_utf16_utfcpp(size_t iterations);
        void run_convert_utf16_to_utf8_utfcpp(size_t iterations);
        void run_convert_utf8_to_utf32_utfcpp(size_t iterations);
        void run_convert_utf32_to_utf8_utfcpp(size_t iterations);
    };

} // namespace simdutf::benchmarks
