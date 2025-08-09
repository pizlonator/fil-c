//  _   _                             __    _____
// | \ | |                           / _|  / ____|_     _
// |  \| | __ _ _ __ ___   ___  ___ | |_  | |   _| |_ _| |_
// | . ` |/ _` | '_ ` _ \ / _ \/ _ \|  _| | |  |_   _|_   _|
// | |\  | (_| | | | | | |  __/ (_) | |   | |____|_|   |_|
// |_| \_|\__,_|_| |_| |_|\___|\___/|_|    \_____|
// https://github.com/Neargye/nameof
// version 0.10.4
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2016 - 2024 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef NEARGYE_NAMEOF_HPP
#define NEARGYE_NAMEOF_HPP

#define NAMEOF_VERSION_MAJOR 0
#define NAMEOF_VERSION_MINOR 10
#define NAMEOF_VERSION_PATCH 4

#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#if !defined(NAMEOF_USING_ALIAS_STRING)
#  include <string>
#endif
#if !defined(NAMEOF_USING_ALIAS_STRING_VIEW)
#  include <string_view>
#endif

#if __has_include(<cxxabi.h>)
#  include <cxxabi.h>
#  include <cstdlib>
#endif

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunknown-warning-option"
#  pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overflow" // Missing terminating nul 'enum_name_v'.
#elif defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 26495) // Variable 'cstring<N>::chars_' is uninitialized.
#  pragma warning(disable : 28020) // Arithmetic overflow: Using operator '-' on a 4 byte value and then casting the result to a 8 byte value.
#  pragma warning(disable : 26451) // The expression '0<=_Param_(1)&&_Param_(1)<=1-1' is not true at this call.
#  pragma warning(disable : 4514) // Unreferenced inline function has been removed.
#endif

// Checks nameof_type compiler compatibility.
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 7 || defined(_MSC_VER) && _MSC_VER >= 1910
#  undef  NAMEOF_TYPE_SUPPORTED
#  define NAMEOF_TYPE_SUPPORTED 1
#endif

// Checks nameof_type_rtti compiler compatibility.
#if defined(__clang__)
#  if __has_feature(cxx_rtti)
#    undef  NAMEOF_TYPE_RTTI_SUPPORTED
#    define NAMEOF_TYPE_RTTI_SUPPORTED 1
#  endif
#elif defined(__GNUC__)
#  if defined(__GXX_RTTI)
#    undef  NAMEOF_TYPE_RTTI_SUPPORTED
#    define NAMEOF_TYPE_RTTI_SUPPORTED 1
#  endif
#elif defined(_MSC_VER)
#  if defined(_CPPRTTI)
#    undef  NAMEOF_TYPE_RTTI_SUPPORTED
#    define NAMEOF_TYPE_RTTI_SUPPORTED 1
#  endif
#endif

// Checks nameof_member compiler compatibility.
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 7 || defined(_MSC_VER) && defined(_MSVC_LANG) && _MSVC_LANG >= 202002L
#  undef  NAMEOF_MEMBER_SUPPORTED
#  define NAMEOF_MEMBER_SUPPORTED 1
#endif

// Checks nameof_pointer compiler compatibility.
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 7 || defined(_MSC_VER) && defined(_MSVC_LANG) && _MSVC_LANG >= 202002L
#  undef  NAMEOF_POINTER_SUPPORTED
#  define NAMEOF_POINTER_SUPPORTED 1
#endif

// Checks nameof_enum compiler compatibility.
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 9 || defined(_MSC_VER) && _MSC_VER >= 1910
#  undef  NAMEOF_ENUM_SUPPORTED
#  define NAMEOF_ENUM_SUPPORTED 1
#endif

// Checks nameof_enum compiler aliases compatibility.
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 9 || defined(_MSC_VER) && _MSC_VER >= 1920
#  undef  NAMEOF_ENUM_SUPPORTED_ALIASES
#  define NAMEOF_ENUM_SUPPORTED_ALIASES 1
#endif

// Enum value must be greater or equals than NAMEOF_ENUM_RANGE_MIN. By default NAMEOF_ENUM_RANGE_MIN = -128.
// If need another min range for all enum types by default, redefine the macro NAMEOF_ENUM_RANGE_MIN.
#if !defined(NAMEOF_ENUM_RANGE_MIN)
#  define NAMEOF_ENUM_RANGE_MIN -128
#endif

// Enum value must be less or equals than NAMEOF_ENUM_RANGE_MAX. By default NAMEOF_ENUM_RANGE_MAX = 128.
// If need another max range for all enum types by default, redefine the macro NAMEOF_ENUM_RANGE_MAX.
#if !defined(NAMEOF_ENUM_RANGE_MAX)
#  define NAMEOF_ENUM_RANGE_MAX 128
#endif

namespace nameof {

// If need another string_view type, define the macro NAMEOF_USING_ALIAS_STRING_VIEW.
#if defined(NAMEOF_USING_ALIAS_STRING_VIEW)
NAMEOF_USING_ALIAS_STRING_VIEW
#else
using std::string_view;
#endif

// If need another string type, define the macro NAMEOF_USING_ALIAS_STRING.
#if defined(NAMEOF_USING_ALIAS_STRING)
NAMEOF_USING_ALIAS_STRING
#else
using std::string;
#endif

namespace customize {

// Enum value must be in range [NAMEOF_ENUM_RANGE_MIN, NAMEOF_ENUM_RANGE_MAX]. By default NAMEOF_ENUM_RANGE_MIN = -128, NAMEOF_ENUM_RANGE_MAX = 128.
// If you need another range for all enum types by default, redefine the macro NAMEOF_ENUM_RANGE_MIN and NAMEOF_ENUM_RANGE_MAX.
// If you need another range for specific enum type, add specialization enum_range for necessary enum type.
template <typename E>
struct enum_range {
  static_assert(std::is_enum_v<E>, "nameof::customize::enum_range requires enum type.");
  inline static constexpr int min = NAMEOF_ENUM_RANGE_MIN;
  inline static constexpr int max = NAMEOF_ENUM_RANGE_MAX;
  static_assert(max > min, "nameof::customize::enum_range requires max > min.");
};

static_assert(NAMEOF_ENUM_RANGE_MIN <= 0, "NAMEOF_ENUM_RANGE_MIN must be less or equals than 0.");
static_assert(NAMEOF_ENUM_RANGE_MIN > (std::numeric_limits<std::int16_t>::min)(), "NAMEOF_ENUM_RANGE_MIN must be greater than INT16_MIN.");

static_assert(NAMEOF_ENUM_RANGE_MAX > 0, "NAMEOF_ENUM_RANGE_MAX must be greater than 0.");
static_assert(NAMEOF_ENUM_RANGE_MAX < (std::numeric_limits<std::int16_t>::max)(), "NAMEOF_ENUM_RANGE_MAX must be less than INT16_MAX.");

static_assert(NAMEOF_ENUM_RANGE_MAX > NAMEOF_ENUM_RANGE_MIN, "NAMEOF_ENUM_RANGE_MAX must be greater than NAMEOF_ENUM_RANGE_MIN.");

// If you need custom names for enum, add specialization enum_name for necessary enum type.
template <typename E>
constexpr string_view enum_name(E) noexcept {
  static_assert(std::is_enum_v<E>, "nameof::customize::enum_name requires enum type.");
  return {};
}

// If you need custom name for type, add specialization type_name for necessary type.
template <typename T>
constexpr string_view type_name() noexcept {
  return {};
}

// If you need custom name for member, add specialization member_name for necessary type.
template <auto V>
constexpr string_view member_name() noexcept {
  return {};
}

// If you need custom name for a pointer, add specialization pointer_name for necessary type.
template <auto V>
constexpr string_view pointer_name() noexcept {
  return {};
}

} // namespace nameof::customize

template <std::uint16_t N>
class [[nodiscard]] cstring {
 public:
  using value_type      = const char;
  using size_type       = std::uint16_t;
  using difference_type = std::ptrdiff_t;
  using pointer         = const char*;
  using const_pointer   = const char*;
  using reference       = const char&;
  using const_reference = const char&;

  using iterator       = const char*;
  using const_iterator = const char*;

  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr explicit cstring(string_view str) noexcept : cstring{str, std::make_integer_sequence<std::uint16_t, N>{}} {
    assert(str.size() > 0 && str.size() == N);
  }

  constexpr cstring() = delete;

  constexpr cstring(const cstring&) = default;

  constexpr cstring(cstring&&) = default;

  ~cstring() = default;

  cstring& operator=(const cstring&) = default;

  cstring& operator=(cstring&&) = default;

  [[nodiscard]] constexpr const_pointer data() const noexcept { return chars_; }

  [[nodiscard]] constexpr size_type size() const noexcept { return N; }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }

  [[nodiscard]] constexpr const_iterator end() const noexcept { return data() + size(); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return end(); }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return begin(); }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  [[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept { return assert(i < size()), chars_[i]; }

  [[nodiscard]] constexpr const_reference front() const noexcept { return chars_[0]; }

  [[nodiscard]] constexpr const_reference back() const noexcept { return chars_[N]; }

  [[nodiscard]] constexpr size_type length() const noexcept { return size(); }

  [[nodiscard]] constexpr bool empty() const noexcept { return false; }

  [[nodiscard]] constexpr int compare(string_view str) const noexcept { return string_view{data(), size()}.compare(str); }

  [[nodiscard]] constexpr const char* c_str() const noexcept { return data(); }

  [[nodiscard]] string str() const { return {begin(), end()}; }

  [[nodiscard]] constexpr operator string_view() const noexcept { return {data(), size()}; }

  [[nodiscard]] constexpr explicit operator const_pointer() const noexcept { return data(); }

  [[nodiscard]] explicit operator string() const { return {begin(), end()}; }

 private:
  template <std::uint16_t... I>
  constexpr cstring(string_view str, std::integer_sequence<std::uint16_t, I...>) noexcept : chars_{str[I]..., '\0'} {}

  char chars_[static_cast<std::size_t>(N) + 1];
};

template <>
class [[nodiscard]] cstring<0> {
 public:
  using value_type      = const char;
  using size_type       = std::uint16_t;
  using difference_type = std::ptrdiff_t;
  using pointer         = const char*;
  using const_pointer   = const char*;
  using reference       = const char&;
  using const_reference = const char&;

  using iterator       = const char*;
  using const_iterator = const char*;

  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr explicit cstring(string_view) noexcept {}

  constexpr cstring() = default;

  constexpr cstring(const cstring&) = default;

  constexpr cstring(cstring&&) = default;

  ~cstring() = default;

  cstring& operator=(const cstring&) = default;

  cstring& operator=(cstring&&) = default;

  [[nodiscard]] constexpr const_pointer data() const noexcept { return nullptr; }

  [[nodiscard]] constexpr size_type size() const noexcept { return 0; }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return nullptr; }

  [[nodiscard]] constexpr const_iterator end() const noexcept { return nullptr; }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return nullptr; }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return nullptr; }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return {}; }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return {}; }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return {}; }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return {}; }

  [[nodiscard]] constexpr size_type length() const noexcept { return 0; }

  [[nodiscard]] constexpr bool empty() const noexcept { return true; }

  [[nodiscard]] constexpr int compare(string_view str) const noexcept { return string_view{}.compare(str); }

  [[nodiscard]] constexpr const char* c_str() const noexcept { return nullptr; }

  [[nodiscard]] string str() const { return {}; }

  [[nodiscard]] constexpr operator string_view() const noexcept { return {}; }

  [[nodiscard]] constexpr explicit operator const_pointer() const noexcept { return nullptr; }

  [[nodiscard]] explicit operator string() const { return {}; }
};

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator==(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) == 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator==(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) == 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator!=(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) != 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator!=(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) != 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator>(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) > 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator>(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) > 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator>=(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) >= 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator>=(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) >= 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator<(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator<(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator<=(const cstring<N>& lhs, string_view rhs) noexcept {
  return lhs.compare(rhs) <= 0;
}

template <std::uint16_t N>
[[nodiscard]] constexpr bool operator<=(string_view lhs, const cstring<N>& rhs) noexcept {
  return lhs.compare(rhs) <= 0;
}

template <typename Char, typename Traits, std::uint16_t N>
std::basic_ostream<Char, Traits>& operator<<(std::basic_ostream<Char, Traits>& os, const cstring<N>& srt) {
  for (const auto c : srt) {
    os.put(c);
  }
  return os;
}

namespace detail {

constexpr string_view pretty_name(string_view name, bool remove_suffix = true) noexcept {
  if (name.size() >= 1 && (name[0] == '"' || name[0] == '\'')) {
    return {}; // Narrow multibyte string literal.
  } else if (name.size() >= 2 && name[0] == 'R' && (name[1] == '"' || name[1] == '\'')) {
    return {}; // Raw string literal.
  } else if (name.size() >= 2 && name[0] == 'L' && (name[1] == '"' || name[1] == '\'')) {
    return {}; // Wide string literal.
  } else if (name.size() >= 2 && name[0] == 'U' && (name[1] == '"' || name[1] == '\'')) {
    return {}; // UTF-32 encoded string literal.
  } else if (name.size() >= 2 && name[0] == 'u' && (name[1] == '"' || name[1] == '\'')) {
    return {}; // UTF-16 encoded string literal.
  } else if (name.size() >= 3 && name[0] == 'u' && name[1] == '8' && (name[2] == '"' || name[2] == '\'')) {
    return {}; // UTF-8 encoded string literal.
  } else if (name.size() >= 1 && (name[0] >= '0' && name[0] <= '9')) {
    return {}; // Invalid name.
  }

  for (std::size_t i = name.size(), h = 0, s = 0; i > 0; --i) {
    if (name[i - 1] == ')') {
      ++h;
      ++s;
      continue;
    } else if (name[i - 1] == '(') {
      --h;
      ++s;
      continue;
    }

    if (h == 0) {
      name.remove_suffix(s);
      break;
    } else {
      ++s;
      continue;
    }
  }

  std::size_t s = 0;
  for (std::size_t i = name.size(), h = 0; i > 0; --i) {
    if (name[i - 1] == '>') {
      ++h;
      ++s;
      continue;
    } else if (name[i - 1] == '<') {
      --h;
      ++s;
      continue;
    }

    if (h == 0) {
      break;
    } else {
      ++s;
      continue;
    }
  }

  for (std::size_t i = name.size() - s; i > 0; --i) {
    if (!((name[i - 1] >= '0' && name[i - 1] <= '9') ||
          (name[i - 1] >= 'a' && name[i - 1] <= 'z') ||
          (name[i - 1] >= 'A' && name[i - 1] <= 'Z') ||
          (name[i - 1] == '_'))) {
      name.remove_prefix(i);
      break;
    }
  }
  if (remove_suffix) {
    name.remove_suffix(s);
  }

  if (name.size() > 0 && ((name[0] >= 'a' && name[0] <= 'z') ||
                          (name[0] >= 'A' && name[0] <= 'Z') ||
                          (name[0] == '_'))) {
    return name;
  }

  return {}; // Invalid name.
}

#if defined(__cpp_lib_array_constexpr) && __cpp_lib_array_constexpr >= 201603L
#  define NAMEOF_ARRAY_CONSTEXPR 1
#else
template <typename T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&a)[N], std::index_sequence<I...>) noexcept {
  return {{a[I]...}};
}
#endif

template <typename L, typename R>
constexpr bool cmp_less(L lhs, R rhs) noexcept {
  static_assert(std::is_integral_v<L> && std::is_integral_v<R>, "nameof::detail::cmp_less requires integral type.");

  if constexpr (std::is_signed_v<L> == std::is_signed_v<R>) {
    // If same signedness (both signed or both unsigned).
    return lhs < rhs;
  } else if constexpr (std::is_same_v<L, bool>) { // bool special case
      return static_cast<R>(lhs) < rhs;
  } else if constexpr (std::is_same_v<R, bool>) { // bool special case
      return lhs < static_cast<L>(rhs);
  } else if constexpr (std::is_signed_v<R>) {
    // If 'right' is negative, then result is 'false', otherwise cast & compare.
    return rhs > 0 && lhs < static_cast<std::make_unsigned_t<R>>(rhs);
  } else {
    // If 'left' is negative, then result is 'true', otherwise cast & compare.
    return lhs < 0 || static_cast<std::make_unsigned_t<L>>(lhs) < rhs;
  }
}

template <typename I>
constexpr I log2(I value) noexcept {
  static_assert(std::is_integral_v<I>, "nameof::detail::log2 requires integral type.");

  if constexpr (std::is_same_v<I, bool>) { // bool special case
    return assert(false), value;
  } else {
    auto ret = I{0};
    for (; value > I{1}; value >>= I{1}, ++ret) {}

    return ret;
  }
}

template <typename T>
struct nameof_enum_supported
#if defined(NAMEOF_ENUM_SUPPORTED) && NAMEOF_ENUM_SUPPORTED || defined(NAMEOF_ENUM_NO_CHECK_SUPPORT)
    : std::true_type {};
#else
    : std::false_type {};
#endif

template <typename T, typename R>
using enable_if_enum_t = std::enable_if_t<std::is_enum_v<std::decay_t<T>>, R>;

template <typename T>
inline constexpr bool is_enum_v = std::is_enum_v<T> && std::is_same_v<T, std::decay_t<T>>;

template <typename E, E V>
constexpr auto n() noexcept {
  static_assert(is_enum_v<E>, "nameof::detail::n requires enum type.");

  if constexpr (nameof_enum_supported<E>::value) {
#if defined(__clang__) || defined(__GNUC__)
    constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2});
#elif defined(_MSC_VER)
    constexpr auto name = pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17});
#else
    constexpr auto name = string_view{};
#endif
    return name;
  } else {
    return string_view{};
  }
}

template <typename E, E V>
constexpr auto enum_name() noexcept {
  [[maybe_unused]] constexpr auto custom_name = customize::enum_name<E>(V);

  if constexpr (custom_name.empty()) {
    constexpr auto name = n<E, V>();
    return cstring<name.size()>{name};
  } else {
    return cstring<custom_name.size()>{custom_name};
  }
}

template <typename E, E V>
inline constexpr auto enum_name_v = enum_name<E, V>();

template <typename E, auto V>
constexpr bool is_valid() noexcept {
#if defined(__clang__) && __clang_major__ >= 16
  // https://reviews.llvm.org/D130058, https://reviews.llvm.org/D131307
  constexpr E v = __builtin_bit_cast(E, V);
#else
  constexpr E v = static_cast<E>(V);
#endif
  [[maybe_unused]] constexpr auto custom_name = customize::enum_name<E>(v);
  if constexpr (custom_name.empty()) {
    return n<E, v>().size() != 0;
  } else {
    return custom_name.size() != 0;
  }
}

template <typename E, int O, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr U ualue(std::size_t i) noexcept {
  if constexpr (std::is_same_v<U, bool>) { // bool special case
    static_assert(O == 0, "nameof::detail::ualue requires valid offset.");

    return static_cast<U>(i);
  } else if constexpr (IsFlags) {
    return static_cast<U>(U{1} << static_cast<U>(static_cast<int>(i) + O));
  } else {
    return static_cast<U>(static_cast<int>(i) + O);
  }
}

template <typename E, int O, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr E value(std::size_t i) noexcept {
  return static_cast<E>(ualue<E, O, IsFlags>(i));
}

template <typename E, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr int reflected_min() noexcept {
  if constexpr (IsFlags) {
    return 0;
  } else {
    constexpr auto lhs = customize::enum_range<E>::min;
    constexpr auto rhs = (std::numeric_limits<U>::min)();

    if constexpr (cmp_less(rhs, lhs)) {
      return lhs;
    } else {
      return rhs;
    }
  }
}

template <typename E, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr int reflected_max() noexcept {
  if constexpr (IsFlags) {
    return std::numeric_limits<U>::digits - 1;
  } else {
    constexpr auto lhs = customize::enum_range<E>::max;
    constexpr auto rhs = (std::numeric_limits<U>::max)();

    if constexpr (cmp_less(lhs, rhs)) {
      return lhs;
    } else {
      return rhs;
    }
  }
}

#define NAMEOF_FOR_EACH_256(T)                                                                                                                                                                     \
  T(  0)T(  1)T(  2)T(  3)T(  4)T(  5)T(  6)T(  7)T(  8)T(  9)T( 10)T( 11)T( 12)T( 13)T( 14)T( 15)T( 16)T( 17)T( 18)T( 19)T( 20)T( 21)T( 22)T( 23)T( 24)T( 25)T( 26)T( 27)T( 28)T( 29)T( 30)T( 31) \
  T( 32)T( 33)T( 34)T( 35)T( 36)T( 37)T( 38)T( 39)T( 40)T( 41)T( 42)T( 43)T( 44)T( 45)T( 46)T( 47)T( 48)T( 49)T( 50)T( 51)T( 52)T( 53)T( 54)T( 55)T( 56)T( 57)T( 58)T( 59)T( 60)T( 61)T( 62)T( 63) \
  T( 64)T( 65)T( 66)T( 67)T( 68)T( 69)T( 70)T( 71)T( 72)T( 73)T( 74)T( 75)T( 76)T( 77)T( 78)T( 79)T( 80)T( 81)T( 82)T( 83)T( 84)T( 85)T( 86)T( 87)T( 88)T( 89)T( 90)T( 91)T( 92)T( 93)T( 94)T( 95) \
  T( 96)T( 97)T( 98)T( 99)T(100)T(101)T(102)T(103)T(104)T(105)T(106)T(107)T(108)T(109)T(110)T(111)T(112)T(113)T(114)T(115)T(116)T(117)T(118)T(119)T(120)T(121)T(122)T(123)T(124)T(125)T(126)T(127) \
  T(128)T(129)T(130)T(131)T(132)T(133)T(134)T(135)T(136)T(137)T(138)T(139)T(140)T(141)T(142)T(143)T(144)T(145)T(146)T(147)T(148)T(149)T(150)T(151)T(152)T(153)T(154)T(155)T(156)T(157)T(158)T(159) \
  T(160)T(161)T(162)T(163)T(164)T(165)T(166)T(167)T(168)T(169)T(170)T(171)T(172)T(173)T(174)T(175)T(176)T(177)T(178)T(179)T(180)T(181)T(182)T(183)T(184)T(185)T(186)T(187)T(188)T(189)T(190)T(191) \
  T(192)T(193)T(194)T(195)T(196)T(197)T(198)T(199)T(200)T(201)T(202)T(203)T(204)T(205)T(206)T(207)T(208)T(209)T(210)T(211)T(212)T(213)T(214)T(215)T(216)T(217)T(218)T(219)T(220)T(221)T(222)T(223) \
  T(224)T(225)T(226)T(227)T(228)T(229)T(230)T(231)T(232)T(233)T(234)T(235)T(236)T(237)T(238)T(239)T(240)T(241)T(242)T(243)T(244)T(245)T(246)T(247)T(248)T(249)T(250)T(251)T(252)T(253)T(254)T(255)

template <typename E, bool IsFlags, std::size_t Size, int Min, std::size_t I>
constexpr void valid_count(bool* valid, std::size_t& count) noexcept {
#define NAMEOF_ENUM_V(O)                                          \
  if constexpr ((I + O) < Size) {                                 \
    if constexpr (is_valid<E, ualue<E, Min, IsFlags>(I + O)>()) { \
      valid[I + O] = true;                                        \
      ++count;                                                    \
    }                                                             \
  }

  NAMEOF_FOR_EACH_256(NAMEOF_ENUM_V)

  if constexpr ((I + 256) < Size) {
    valid_count<E, IsFlags, Size, Min, I + 256>(valid, count);
  }
#undef NAMEOF_ENUM_V
}

template <std::size_t N>
struct valid_count_t {
  std::size_t count = 0;
  bool valid[N] = {};
};

template <typename E, bool IsFlags, std::size_t Size, int Min>
constexpr auto valid_count() noexcept {
  valid_count_t<Size> vc;
  valid_count<E, IsFlags, Size, Min, 0>(vc.valid, vc.count);
  return vc;
}

template <typename E, bool IsFlags, std::size_t Size, int Min>
constexpr auto values() noexcept {
  constexpr auto vc = valid_count<E, IsFlags, Size, Min>();

  if constexpr (vc.count > 0) {
#if defined(NAMEOF_ARRAY_CONSTEXPR)
    std::array<E, vc.count> values = {};
#else
    E values[vc.count] = {};
#endif
    for (std::size_t i = 0, v = 0; v < vc.count; ++i) {
      if (vc.valid[i]) {
        values[v++] = value<E, Min, IsFlags>(i);
      }
    }
#if defined(NAMEOF_ARRAY_CONSTEXPR)
    return values;
#else
    return to_array(values, std::make_index_sequence<vc.count>{});
#endif
  } else {
    return std::array<E, 0>{};
  }
}

template <typename E, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr auto values() noexcept {
  constexpr auto min = reflected_min<E, IsFlags>();
  constexpr auto max = reflected_max<E, IsFlags>();
  constexpr auto range_size = max - min + 1;
  static_assert(range_size > 0, "nameof::enum_range requires valid size.");
  static_assert(range_size < (std::numeric_limits<std::uint16_t>::max)(), "nameof::enum_range requires valid size.");

  return values<E, IsFlags, range_size, min>();
}

template <typename E, bool IsFlags = false>
inline constexpr auto values_v = values<E, IsFlags>();

template <typename E, bool IsFlags = false>
inline constexpr auto count_v = values_v<E, IsFlags>.size();

template <typename E, bool IsFlags = false, typename U = std::underlying_type_t<E>>
inline constexpr auto min_v = (count_v<E, IsFlags> > 0) ? static_cast<U>(values_v<E, IsFlags>.front()) : U{0};

template <typename E, bool IsFlags = false, typename U = std::underlying_type_t<E>>
inline constexpr auto max_v = (count_v<E, IsFlags> > 0) ? static_cast<U>(values_v<E, IsFlags>.back()) : U{0};

template <typename E, bool IsFlags, std::size_t... I>
constexpr auto names(std::index_sequence<I...>) noexcept {
  constexpr auto names = std::array<string_view, sizeof...(I)>{{enum_name_v<E, values_v<E, IsFlags>[I]>...}};
  return names;
}

template <typename E, bool IsFlags = false>
inline constexpr auto names_v = names<E, IsFlags>(std::make_index_sequence<count_v<E, IsFlags>>{});

template <typename E, bool IsFlags, typename U = std::underlying_type_t<E>>
constexpr bool is_sparse() noexcept {
  if constexpr (count_v<E, IsFlags> == 0) {
    return false;
  } else if constexpr (std::is_same_v<U, bool>) { // bool special case
    return false;
  } else {
    constexpr auto max = IsFlags ? log2(max_v<E, IsFlags>) : max_v<E, IsFlags>;
    constexpr auto min = IsFlags ? log2(min_v<E, IsFlags>) : min_v<E, IsFlags>;
    constexpr auto range_size = max - min + 1;

    return range_size != count_v<E, IsFlags>;
  }
}

template <typename E, bool IsFlags = false>
inline constexpr bool is_sparse_v = is_sparse<E, IsFlags>();

template <typename E, bool IsFlags = false, typename U = std::underlying_type_t<E>>
constexpr E enum_value(std::size_t i) noexcept {
  if constexpr (is_sparse_v<E, IsFlags>) {
    return values_v<E, IsFlags>[i];
  } else {
    constexpr auto min = IsFlags ? log2(min_v<E, IsFlags>) : min_v<E, IsFlags>;

    return value<E, min, IsFlags>(i);
  }
}

template <typename... T>
struct nameof_type_supported
#if defined(NAMEOF_TYPE_SUPPORTED) && NAMEOF_TYPE_SUPPORTED || defined(NAMEOF_TYPE_NO_CHECK_SUPPORT)
    : std::true_type {};
#else
    : std::false_type {};
#endif

template <typename... T>
struct nameof_type_rtti_supported
#if defined(NAMEOF_TYPE_RTTI_SUPPORTED) && NAMEOF_TYPE_RTTI_SUPPORTED || defined(NAMEOF_TYPE_NO_CHECK_SUPPORT)
    : std::true_type {};
#else
    : std::false_type {};
#endif

template <typename... T>
struct nameof_member_supported
#if defined(NAMEOF_MEMBER_SUPPORTED) && NAMEOF_MEMBER_SUPPORTED || defined(NAMEOF_TYPE_NO_CHECK_SUPPORT)
    : std::true_type {};
#else
    : std::false_type {};
#endif

template <typename... T>
struct nameof_pointer_supported
#if defined(NAMEOF_POINTER_SUPPORTED) && NAMEOF_POINTER_SUPPORTED || defined(NAMEOF_TYPE_NO_CHECK_SUPPORT)
    : std::true_type {};
#else
    : std::false_type {};
#endif

#if defined(_MSC_VER) && !defined(__clang__)
template <typename T>
struct identity {
  using type = T;
};
#else
template <typename T>
using identity = T;
#endif

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T, typename R>
using enable_if_has_short_name_t = std::enable_if_t<!std::is_array_v<T> && !std::is_pointer_v<T>, R>;

template <typename... T>
constexpr auto n() noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
  [[maybe_unused]] constexpr auto custom_name = customize::type_name<typename T::type...>();
#else
  [[maybe_unused]] constexpr auto custom_name = customize::type_name<T...>();
#endif

  if constexpr (custom_name.empty() && nameof_type_supported<T...>::value) {
#if defined(__clang__)
    constexpr string_view name{__PRETTY_FUNCTION__ + 31, sizeof(__PRETTY_FUNCTION__) - 34};
#elif defined(__GNUC__)
    constexpr string_view name{__PRETTY_FUNCTION__ + 46, sizeof(__PRETTY_FUNCTION__) - 49};
#elif defined(_MSC_VER)
    constexpr string_view name{__FUNCSIG__ + 63, sizeof(__FUNCSIG__) - 81 - (__FUNCSIG__[sizeof(__FUNCSIG__) - 19] == ' ' ? 1 : 0)};
#else
    constexpr auto name = string_view{};
#endif
    return cstring<name.size()>{name};
  } else {
    return cstring<custom_name.size()>{custom_name};
  }
}

template <typename... T>
inline constexpr auto type_name_v = n<T...>();

#if __has_include(<cxxabi.h>)
template <typename T>
string nameof_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  const auto dmg = abi::__cxa_demangle(tn, nullptr, nullptr, nullptr);
  const auto name = string{dmg};
  free(dmg);
  assert(!name.empty() && "Type does not have a name.");

  return name;
}

template <typename T>
string nameof_full_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  const auto dmg = abi::__cxa_demangle(tn, nullptr, nullptr, nullptr);
  auto name = string{dmg};
  free(dmg);
  assert(!name.empty() && "Type does not have a name.");
  if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
    name = string{"const "}.append(name);
  }
  if constexpr (std::is_volatile_v<std::remove_reference_t<T>>) {
    name = string{"volatile "}.append(name);
  }
  if constexpr (std::is_lvalue_reference_v<T>) {
    name.append(1, '&');
  }
  if constexpr (std::is_rvalue_reference_v<T>) {
    name.append("&&");
  }

  return name;
}

template <typename T, enable_if_has_short_name_t<T, int> = 0>
string nameof_short_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  const auto dmg = abi::__cxa_demangle(tn, nullptr, nullptr, nullptr);
  const auto pname = pretty_name(dmg);
  const auto name = string{pname.data(), pname.size()};
  free(dmg);
  assert(!name.empty() && "Type does not have a short name.");

  return name;
}
#else
template <typename T>
string nameof_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  const auto name = string_view{tn};
  assert(!name.empty() && "Type does not have a name.");
  return {name.data(), name.size()};
}

template <typename T>
string nameof_full_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  auto name = string{tn};
  assert(!name.empty() && "Type does not have a name.");
  if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
    name = string{"const "}.append(name);
  }
  if constexpr (std::is_volatile_v<std::remove_reference_t<T>>) {
    name = string{"volatile "}.append(name);
  }
  if constexpr (std::is_lvalue_reference_v<T>) {
    name.append(1, '&');
  }
  if constexpr (std::is_rvalue_reference_v<T>) {
    name.append("&&");
  }
  return name;
}

template <typename T, enable_if_has_short_name_t<T, int> = 0>
string nameof_short_type_rtti(const char* tn) {
  static_assert(nameof_type_rtti_supported<T>::value, "nameof::nameof_type_rtti unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  const auto name = pretty_name(tn);
  assert(!name.empty() && "Type does not have a short name.");
  return {name.data(), name.size()};
}
#endif

template <auto V, auto U = V>
constexpr auto n() noexcept {
  [[maybe_unused]] constexpr auto custom_name = customize::member_name<V>();

  if constexpr (custom_name.empty() && nameof_member_supported<decltype(V)>::value) {
#if defined(__clang__) || defined(__GNUC__)
    constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2});
#elif defined(_MSC_VER) && defined(_MSVC_LANG) && _MSVC_LANG >= 202002L
    constexpr auto name = pretty_name({__FUNCSIG__,
                                       sizeof(__FUNCSIG__) - 18 + std::is_member_function_pointer_v<decltype(U)>});
#else
    constexpr auto name = string_view{};
#endif
    return cstring<name.size()>{name};
  } else {
    return cstring<custom_name.size()>{custom_name};
  }
}

#if defined(__clang__) || defined(__GNUC__)
template <auto V>
inline constexpr auto member_name_v = n<V>();
#elif defined(_MSC_VER) && defined(_MSVC_LANG) && _MSVC_LANG >= 202002L
template <typename From, typename Type>
From get_base_type(Type From::*);

template <typename T>
extern T nonexist_object;

template<class T>
struct Store {
    T v;
};

template<class T>
Store(T) -> Store<T>;

template <auto V>
consteval auto get_member_name() noexcept {
  if constexpr (std::is_member_function_pointer_v<decltype(V)>) {
    return n<V>();
  } else {
    constexpr bool is_defined = sizeof(decltype(get_base_type(V))) != 0;
    static_assert(is_defined, "nameof::nameof_member member name can use only if the struct is already fully defined. Please use NAMEOF macro, or separate definition and declaration.");
    if constexpr (is_defined) {
      return n<V, Store{&(nonexist_object<decltype(get_base_type(V))>.*V)}>();
    } else {
      return "";
    }
  }
}

template <auto V>
inline constexpr auto member_name_v = get_member_name<V>();
#else
template <auto V>
inline constexpr auto member_name_v = cstring<0>{};
#endif

template <auto U, auto V>
struct is_same : std::false_type {};

template <auto U>
struct is_same<U, U> : std::true_type {};

template <auto P>
constexpr bool is_nullptr_v = is_same<P, static_cast<std::remove_reference_t<decltype(P)>>(nullptr)>::value;

template <auto V>
constexpr auto p() noexcept {
  [[maybe_unused]] constexpr auto custom_name = customize::pointer_name<V>().empty() && is_nullptr_v<V> ? "nullptr" : customize::pointer_name<V>();

  if constexpr (custom_name.empty() && nameof_pointer_supported<decltype(V)>::value) {
#if defined(__clang__)
    constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2});
#elif defined(__GNUC__)
    constexpr bool has_parenthesis = __PRETTY_FUNCTION__[sizeof(__PRETTY_FUNCTION__) - 3] == ')';
    constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2 - has_parenthesis});
#elif defined(_MSC_VER) && defined(_MSVC_LANG) && _MSVC_LANG >= 202002L
    constexpr auto name = pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17});
#else
    constexpr auto name = string_view{};
#endif
    return cstring<name.size()>{name};
  } else {
    return cstring<custom_name.size()>{custom_name};
  }
}

template <auto V>
inline constexpr auto pointer_name_v = p<V>();

} // namespace nameof::detail

// Checks is nameof_type supported compiler.
inline constexpr bool is_nameof_type_supported = detail::nameof_type_supported<void>::value;

// Checks is nameof_type_rtti supported compiler.
inline constexpr bool is_nameof_type_rtti_supported = detail::nameof_type_rtti_supported<void>::value;

// Checks is nameof_member supported compiler.
inline constexpr bool is_nameof_member_supported = detail::nameof_member_supported<void>::value;

// Checks is nameof_pointer supported compiler.
inline constexpr bool is_nameof_pointer_supported = detail::nameof_pointer_supported<void>::value;

// Checks is nameof_enum supported compiler.
inline constexpr bool is_nameof_enum_supported = detail::nameof_enum_supported<void>::value;

// Obtains name of enum variable.
template <typename E>
[[nodiscard]] constexpr auto nameof_enum(E value) noexcept -> detail::enable_if_enum_t<E, string_view> {
  using D = std::decay_t<E>;
  using U = std::underlying_type_t<D>;
  static_assert(detail::nameof_enum_supported<D>::value, "nameof::nameof_enum unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  static_assert(detail::count_v<D> > 0, "nameof::nameof_enum requires enum implementation and valid max and min.");

  if constexpr (detail::is_sparse_v<D>) {
    for (std::size_t i = 0; i < detail::count_v<D>; ++i) {
      if (detail::enum_value<D>(i) == value) {
        return detail::names_v<D>[i];
      }
    }
  } else {
    const auto v = static_cast<U>(value);
    if (v >= detail::min_v<D> && v <= detail::max_v<D>) {
      return detail::names_v<D>[static_cast<std::size_t>(v - detail::min_v<D>)];
    }
  }
  return {}; // Value out of range.
}

// Obtains name of enum variable or default value if enum variable out of range.
template <typename E>
[[nodiscard]] auto nameof_enum_or(E value, string_view default_value) -> detail::enable_if_enum_t<E, string> {
  using D = std::decay_t<E>;
  static_assert(detail::nameof_enum_supported<D>::value, "nameof::nameof_enum_or unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  static_assert(detail::count_v<D> > 0, "nameof::nameof_enum_or requires enum implementation and valid max and min.");

  if (auto v = nameof_enum<D>(value); !v.empty()) {
    return string{v.data(), v.size()};
  }
  return string{default_value.data(), default_value.size()};
}

// Obtains name of enum-flags variable.
template <typename E>
[[nodiscard]] auto nameof_enum_flag(E value, char sep = '|') -> detail::enable_if_enum_t<E, string> {
  using D = std::decay_t<E>;
  using U = std::underlying_type_t<D>;
  static_assert(detail::nameof_enum_supported<D>::value, "nameof::nameof_enum_flag unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  static_assert(detail::count_v<D, true> > 0, "nameof::nameof_enum_flag requires enum-flags implementation.");

  string name;
  auto check_value = U{0};
  for (std::size_t i = 0; i < detail::count_v<D, true>; ++i) {
    if (const auto v = static_cast<U>(detail::enum_value<D, true>(i)); (static_cast<U>(value) & v) != 0) {
      if (const auto n = detail::names_v<D, true>[i]; !n.empty()) {
        check_value |= v;
        if (!name.empty()) {
          name.append(1, sep);
        }
        name.append(n.data(), n.size());
      } else {
        return {}; // Value out of range.
      }
    }
  }

  if (check_value != 0 && check_value == static_cast<U>(value)) {
    return name;
  }
  return {}; // Invalid value or out of range.
}

// Obtains name of static storage enum variable.
// This version is much lighter on the compile times and is not restricted to the enum_range limitation.
template <auto V>
[[nodiscard]] constexpr auto nameof_enum() noexcept -> detail::enable_if_enum_t<decltype(V), string_view> {
  using D = std::decay_t<decltype(V)>;
  static_assert(detail::nameof_enum_supported<D>::value, "nameof::nameof_enum unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  constexpr string_view name = detail::enum_name_v<D, V>;
  static_assert(!name.empty(), "Enum value does not have a name.");
  return name;
}

// Obtains name of type, reference and cv-qualifiers are ignored.
template <typename T>
[[nodiscard]] constexpr string_view nameof_type() noexcept {
  static_assert(detail::nameof_type_supported<T>::value, "nameof::nameof_type unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  using U = detail::identity<detail::remove_cvref_t<T>>;
  constexpr string_view name = detail::type_name_v<U>;
  static_assert(!name.empty(), "Type does not have a name.");
  return name;
}

// Obtains full name of type, with reference and cv-qualifiers.
template <typename T>
[[nodiscard]] constexpr string_view nameof_full_type() noexcept {
  static_assert(detail::nameof_type_supported<T>::value, "nameof::nameof_type unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  using U = detail::identity<T>;
  constexpr string_view name = detail::type_name_v<U>;
  static_assert(!name.empty(), "Type does not have a full name.");
  return name;
}

// Obtains short name of type.
template <typename T>
[[nodiscard]] constexpr auto nameof_short_type() noexcept -> detail::enable_if_has_short_name_t<T, string_view> {
  static_assert(detail::nameof_type_supported<T>::value, "nameof::nameof_type unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  using U = detail::identity<detail::remove_cvref_t<T>>;
  constexpr string_view name = detail::pretty_name(detail::type_name_v<U>);
  static_assert(!name.empty(), "Type does not have a short name.");
  return name;
}

// Obtains name of member.
template <auto V>
[[nodiscard]] constexpr auto nameof_member() noexcept -> std::enable_if_t<std::is_member_pointer_v<decltype(V)>, string_view> {
  static_assert(detail::nameof_member_supported<decltype(V)>::value, "nameof::nameof_member unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  constexpr string_view name = detail::member_name_v<V>;
  static_assert(!name.empty(), "Member does not have a name.");
  return name;
}

// Obtains name of a function, a global or class static variable.
template <auto V>
[[nodiscard]] constexpr auto nameof_pointer() noexcept -> std::enable_if_t<std::is_pointer_v<decltype(V)>, string_view> {
  static_assert(detail::nameof_pointer_supported<decltype(V)>::value, "nameof::nameof_pointer unsupported compiler (https://github.com/Neargye/nameof#compiler-compatibility).");
  constexpr string_view name = detail::pointer_name_v<V>;
  static_assert(!name.empty(), "Pointer does not have a name.");
  return name;
}

} // namespace nameof

// Obtains name of variable, function, macro.
#define NAMEOF(...) []() constexpr noexcept {                         \
  ::std::void_t<decltype(__VA_ARGS__)>();                             \
  constexpr auto _name = ::nameof::detail::pretty_name(#__VA_ARGS__); \
  static_assert(!_name.empty(), "Expression does not have a name.");  \
  constexpr auto _size = _name.size();                                \
  constexpr auto _nameof = ::nameof::cstring<_size>{_name};           \
  return _nameof; }()

// Obtains full name of variable, function, macro.
#define NAMEOF_FULL(...) []() constexpr noexcept {                           \
  ::std::void_t<decltype(__VA_ARGS__)>();                                    \
  constexpr auto _name = ::nameof::detail::pretty_name(#__VA_ARGS__, false); \
  static_assert(!_name.empty(), "Expression does not have a name.");         \
  constexpr auto _size = _name.size();                                       \
  constexpr auto _nameof_full = ::nameof::cstring<_size>{_name};             \
  return _nameof_full; }()

// Obtains raw name of variable, function, macro.
#define NAMEOF_RAW(...) []() constexpr noexcept {                    \
  ::std::void_t<decltype(__VA_ARGS__)>();                            \
  constexpr auto _name = ::nameof::string_view{#__VA_ARGS__};        \
  static_assert(!_name.empty(), "Expression does not have a name."); \
  constexpr auto _size = _name.size();                               \
  constexpr auto _nameof_raw = ::nameof::cstring<_size>{_name};      \
  return _nameof_raw; }()

// Obtains name of enum variable.
#define NAMEOF_ENUM(...) ::nameof::nameof_enum<::std::decay_t<decltype(__VA_ARGS__)>>(__VA_ARGS__)

// Obtains name of enum variable or default value if enum variable out of range.
#define NAMEOF_ENUM_OR(...) ::nameof::nameof_enum_or(__VA_ARGS__)

// Obtains name of static storage enum variable.
// This version is much lighter on the compile times and is not restricted to the enum_range limitation.
#define NAMEOF_ENUM_CONST(...) ::nameof::nameof_enum<__VA_ARGS__>()

// Obtains name of enum-flags variable.
#define NAMEOF_ENUM_FLAG(...) ::nameof::nameof_enum_flag<::std::decay_t<decltype(__VA_ARGS__)>>(__VA_ARGS__)

// Obtains type name, reference and cv-qualifiers are ignored.
#define NAMEOF_TYPE(...) ::nameof::nameof_type<__VA_ARGS__>()

// Obtains full type name, with reference and cv-qualifiers.
#define NAMEOF_FULL_TYPE(...) ::nameof::nameof_full_type<__VA_ARGS__>()

// Obtains short type name.
#define NAMEOF_SHORT_TYPE(...) ::nameof::nameof_short_type<__VA_ARGS__>()

// Obtains type name of expression, reference and cv-qualifiers are ignored.
#define NAMEOF_TYPE_EXPR(...) ::nameof::nameof_type<decltype(__VA_ARGS__)>()

// Obtains full type name of expression, with reference and cv-qualifiers.
#define NAMEOF_FULL_TYPE_EXPR(...) ::nameof::nameof_full_type<decltype(__VA_ARGS__)>()

// Obtains short type name of expression.
#define NAMEOF_SHORT_TYPE_EXPR(...) ::nameof::nameof_short_type<decltype(__VA_ARGS__)>()

// Obtains type name, with reference and cv-qualifiers, using RTTI.
#define NAMEOF_TYPE_RTTI(...) ::nameof::detail::nameof_type_rtti<::std::void_t<decltype(__VA_ARGS__)>>(typeid(__VA_ARGS__).name())

// Obtains full type name, using RTTI.
#define NAMEOF_FULL_TYPE_RTTI(...) ::nameof::detail::nameof_full_type_rtti<decltype(__VA_ARGS__)>(typeid(__VA_ARGS__).name())

// Obtains short type name, using RTTI.
#define NAMEOF_SHORT_TYPE_RTTI(...) ::nameof::detail::nameof_short_type_rtti<decltype(__VA_ARGS__)>(typeid(__VA_ARGS__).name())

// Obtains name of member.
#define NAMEOF_MEMBER(...) ::nameof::nameof_member<__VA_ARGS__>()

// Obtains name of a function, a global or class static variable.
#define NAMEOF_POINTER(...) ::nameof::nameof_pointer<__VA_ARGS__>()

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif

#endif // NEARGYE_NAMEOF_HPP
