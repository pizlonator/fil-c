#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <cstdio>

namespace simdutf { namespace tests { namespace helpers {




  /**
   * It would be nice to use a single helper for both UTF-8 => UTF-16LE
   * and UTF-8 <= UTF-16LE, but the function signatures differ when you
   * represent UTF8 with char8_t and UTF-16LE with char16_t.
   */
  class transcode_test_base {
  protected:
    void encode_utf8(uint32_t codepoint, std::vector<char>& target);
    void encode_utf16(uint32_t codepoint, std::vector<char16_t>& target);
    void encode_utf32(uint32_t codepoint, std::vector<char32_t>& target);
    void encode_latin1(uint32_t codepoint, std::vector<char>& target);
  };

  /**
   * This class can be used to test Latin1 => UTF-8 transcoding.
   */
  class transcode_latin1_to_utf8_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_utf8; // what the procedure under test produced
    std::vector<char> reference_output_utf8; // what we are expecting

    std::vector<char> input_latin1; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_latin1_to_utf8_test_base(GenerateCodepoint generate, size_t input_size); //constructor

    template <typename COLLECTION>
    transcode_latin1_to_utf8_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf8.resize(reference_output_utf8.size() + output_size_margin);
      output_utf8.shrink_to_fit(); // to help detect overruns.
      
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size(), output_utf8.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size());
      if (saved_chars != reference_output_utf8.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));

        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

  /**
   * This class can be used to test Latin1 => UTF-16LE transcoding.
   */
  class transcode_latin1_to_utf16_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> input_latin1; 
    std::vector<char16_t> output_utf16;
    std::vector<char16_t> reference_output_utf16; 

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_latin1_to_utf16_test_base(GenerateCodepoint generate, size_t input_size);

    template <typename COLLECTION>
    transcode_latin1_to_utf16_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf16.resize(reference_output_utf16.size() + output_size_margin);
      output_utf16.shrink_to_fit(); // to help detect overruns.

    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size(), output_utf16.data());
      return validate(saved_chars);
    }

    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size());
      if (saved_chars != reference_output_utf16.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));
        return false;
      }
      return true;
    }


  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

  /**
   * This class can be used to test UTF-16LE => UTF-32 transcoding.
   */
  class transcode_utf16_to_latin1_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_latin1; // what the procedure under test produced
    std::vector<char> reference_output_latin1; // what we are expecting

    std::vector<char16_t> input_utf16; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 16; // extra room for buggy procedures

  public:
    transcode_utf16_to_latin1_test_base(GenerateCodepoint generate, size_t input_size);

    transcode_utf16_to_latin1_test_base(const std::vector<char16_t>& input_utf16);

    template <typename COLLECTION>
    transcode_utf16_to_latin1_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_latin1.resize(reference_output_latin1.size() + output_size_margin);
      output_latin1.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size(), output_latin1.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size());
      if (saved_chars != reference_output_latin1.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };



      /**
   * This class can be used to test Latin1 => UTF-32 transcoding.
   */
  class transcode_latin1_to_utf32_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char32_t> output_utf32; // what the procedure under test produced
    std::vector<char32_t> reference_output_utf32; // what we are expecting

    std::vector<char> input_latin1; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_latin1_to_utf32_test_base(GenerateCodepoint generate, size_t input_size); //constructor

    template <typename COLLECTION>
    transcode_latin1_to_utf32_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf32.resize(reference_output_utf32.size() + output_size_margin);
      output_utf32.shrink_to_fit(); // to help detect overruns.
      
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size(), output_utf32.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_latin1.data(), input_latin1.size());
      if (saved_chars != reference_output_utf32.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));

        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

  /**
   * This class can be used to test UTF-8 => Latin1 transcoding.
   */
  class transcode_utf8_to_latin1_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_latin1; // what the procedure under test produced
    std::vector<char> reference_output_latin1; // what we are expecting

    std::vector<char> input_utf8; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf8_to_latin1_test_base(GenerateCodepoint generate, size_t input_size); //constructor

    template <typename COLLECTION>
    transcode_utf8_to_latin1_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_latin1.resize(reference_output_latin1.size() + output_size_margin);
      output_latin1.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size(), output_latin1.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size());
      if (saved_chars != reference_output_latin1.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

  /**
   * This class can be used to test UTF-8 => UTF-16LE transcoding.
   */
  class transcode_utf8_to_utf16_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> input_utf8; // source-encoded message: what we're going to transcode
    std::vector<char16_t> output_utf16; // what the procedure under test produced
    std::vector<char16_t> reference_output_utf16; // what we are expecting

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf8_to_utf16_test_base(GenerateCodepoint generate, size_t input_size);

    template <typename COLLECTION>
    transcode_utf8_to_utf16_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf16.resize(reference_output_utf16.size() + output_size_margin);
      output_utf16.shrink_to_fit(); // to help detect overruns.

    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size(), output_utf16.data());
      return validate(saved_chars);
    }

    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size());
      if (saved_chars != reference_output_utf16.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));
        return false;
      }
      return true;
    }


  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };


  /**
   * This class can be used to test UTF-8 => UTF-32 transcoding.
   */
  class transcode_utf8_to_utf32_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> input_utf8; // source-encoded message: what we're going to transcode
    std::vector<char32_t> output_utf32; // what the procedure under test produced
    std::vector<char32_t> reference_output_utf32; // what we are expecting

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf8_to_utf32_test_base(GenerateCodepoint generate, size_t input_size);

    template <typename COLLECTION>
    transcode_utf8_to_utf32_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf32.resize(reference_output_utf32.size() + output_size_margin);
      output_utf32.shrink_to_fit(); // to help detect overruns.

    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size(), output_utf32.data());
      return validate(saved_chars);
    }

    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf8.data(), input_utf8.size());
      if (saved_chars != reference_output_utf32.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));
        return false;
      }
      return true;
    }


  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };


  /**
   * This class can be used to test UTF-16LE => UTF-8 transcoding.
   */
  class transcode_utf16_to_utf8_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_utf8; // what the procedure under test produced
    std::vector<char> reference_output_utf8; // what we are expecting

    std::vector<char16_t> input_utf16; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf16_to_utf8_test_base(GenerateCodepoint generate, size_t input_size);

    transcode_utf16_to_utf8_test_base(const std::vector<char16_t>& input_utf16);

    template <typename COLLECTION>
    transcode_utf16_to_utf8_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf8.resize(reference_output_utf8.size() + output_size_margin);
      output_utf8.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size(), output_utf8.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size());
      if (saved_chars != reference_output_utf8.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

   /**
   * This class can be used to test UTF-32 => Latin1 transcoding.
   */
  class transcode_utf32_to_latin1_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_latin1; // what the procedure under test produced
    std::vector<char> reference_output_latin1; // what we are expecting

    std::vector<char32_t> input_utf32; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf32_to_latin1_test_base(GenerateCodepoint generate, size_t input_size); //constructor

    template <typename COLLECTION>
    transcode_utf32_to_latin1_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_latin1.resize(reference_output_latin1.size() + output_size_margin);
      output_latin1.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size(), output_latin1.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size());
      if (saved_chars != reference_output_latin1.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };


  /**
   * This class can be used to test UTF-32 => UTF-8 transcoding.
   */
  class transcode_utf32_to_utf8_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char> output_utf8; // what the procedure under test produced
    std::vector<char> reference_output_utf8; // what we are expecting

    std::vector<char32_t> input_utf32; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf32_to_utf8_test_base(GenerateCodepoint generate, size_t input_size);

    transcode_utf32_to_utf8_test_base(const std::vector<char32_t>& input_utf32);

    template <typename COLLECTION>
    transcode_utf32_to_utf8_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf8.resize(reference_output_utf8.size() + output_size_margin);
      output_utf8.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size(), output_utf8.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size());
      if (saved_chars != reference_output_utf8.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };


  /**
   * This class can be used to test UTF-32 => UTF-16LE transcoding.
   */
  class transcode_utf32_to_utf16_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char16_t> output_utf16; // what the procedure under test produced
    std::vector<char16_t> reference_output_utf16; // what we are expecting

    std::vector<char32_t> input_utf32; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 0; // extra room for buggy procedures

  public:
    transcode_utf32_to_utf16_test_base(GenerateCodepoint generate, size_t input_size);

    template <typename COLLECTION>
    transcode_utf32_to_utf16_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf16.resize(reference_output_utf16.size() + output_size_margin);
      output_utf16.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size(), output_utf16.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf32.data(), input_utf32.size());
      if (saved_chars != reference_output_utf16.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };



  /**
   * This class can be used to test UTF-16LE => UTF-32 transcoding.
   */
  class transcode_utf16_to_utf32_test_base : transcode_test_base {
  public:
    using GenerateCodepoint = std::function<uint32_t()>;

    std::vector<char32_t> output_utf32; // what the procedure under test produced
    std::vector<char32_t> reference_output_utf32; // what we are expecting

    std::vector<char16_t> input_utf16; // source-encoded message: what we're going to transcode

    static constexpr size_t output_size_margin = 16; // extra room for buggy procedures

  public:
    transcode_utf16_to_utf32_test_base(GenerateCodepoint generate, size_t input_size);

    transcode_utf16_to_utf32_test_base(const std::vector<char16_t>& input_utf16);

    template <typename COLLECTION>
    transcode_utf16_to_utf32_test_base(COLLECTION&& collection) {
      for (const uint32_t codepoint: collection) {
        prepare_input(codepoint);
      }
      output_utf32.resize(reference_output_utf32.size() + output_size_margin);
      output_utf32.shrink_to_fit(); // to help detect overruns.
    }

    template <typename PROCEDURE>
    bool operator()(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size(), output_utf32.data());
      return validate(saved_chars);
    }
    template <typename PROCEDURE>
    bool check_size(PROCEDURE procedure) {
      size_t saved_chars = procedure(input_utf16.data(), input_utf16.size());
      if (saved_chars != reference_output_utf32.size()) {
        printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));
        return false;
      }
      return true;
    }
  private:
    void prepare_input(uint32_t codepoint);
    bool validate(size_t procedure_result) const;
    bool is_input_valid() const;
  };

}}} // namespace simdutf::tests::helpers
