// Copyright © 2021, 2022, 2023 Christian Persch
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <glib.h>

#include "fwd.hh"
#include "uuid.hh"
#include "color.hh"
#include "color-parser.hh"
#include "glib-glue.hh"

#include <cmath> // for std::isfinite

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <version>

namespace vte::terminal {

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

// Termprops. Make sure the enum values are the same as in the
// public VtePropertyType enum.
enum class TermpropType {
        VALUELESS,
        BOOL,
        INT,
        UINT,
        DOUBLE,
        RGB,
        RGBA,
        STRING,
        DATA,
        UUID,
        URI,
        INVALID = -1,
};

enum class TermpropFlags {
        // public
        NONE = 0u,
        EPHEMERAL = 1u << 0,

        // private
        NO_OSC = 1u << 1, // not settable via the termprop OSC
};

class TermpropInfo {
private:
        int m_id{-1};
        unsigned m_quark{0};
        TermpropType m_type{TermpropType::INVALID};
        TermpropFlags m_flags{TermpropFlags::NONE};

public:

        static inline constexpr auto const k_max_string_len = 1024u;
        static inline constexpr auto const k_max_data_len = 2048u;

        TermpropInfo(int id,
                     unsigned quark,
                     TermpropType type,
                     TermpropFlags flags = TermpropFlags::NONE) noexcept
                : m_id{id},
                  m_quark{quark},
                  m_type{type},
                  m_flags{flags}
        {
        }

        TermpropInfo() noexcept = delete;
        TermpropInfo(TermpropInfo const&) = default;
        TermpropInfo(TermpropInfo&&) = default;
        ~TermpropInfo() = default;

        TermpropInfo& operator=(TermpropInfo const&) = default;
        TermpropInfo& operator=(TermpropInfo&&) = default;

        constexpr auto id() const noexcept { return m_id; }
        constexpr auto quark() const noexcept { return m_quark; }
        constexpr auto type() const noexcept { return m_type; }
        constexpr auto flags() const noexcept { return m_flags; }

        auto name() const noexcept { return g_quark_to_string(quark()); }

}; // class TermpropInfo

inline std::vector<TermpropInfo> s_registered_termprops{};

// This requires GCC 11. Remove the #else once we depend on that.
#if defined(__cpp_lib_generic_unordered_lookup) &&__cpp_lib_generic_unordered_lookup >= 201811

template<class CharT,
         class Traits = std::char_traits<CharT>>
class BasicStringHash {
public:

        using string_view_type = std::basic_string_view<CharT, Traits>;
        using string_type = std::basic_string<CharT, Traits>;
        using hash_type = std::hash<string_view_type>;
        using is_transparent = void;

        constexpr auto operator()(CharT const* str) const noexcept { return hash_type{}(str); }
        constexpr auto operator()(string_view_type const& str) const noexcept { return hash_type{}(str); }
        constexpr auto operator()(string_type const& str) const noexcept { return hash_type{}(str); }

}; // class BasicStringHash

using StringHash = BasicStringHash<char>;

inline std::unordered_map<std::string,
                          int,
                          StringHash,
                          std::equal_to<>> s_registered_termprops_by_name{};

#else

inline std::unordered_map<std::string,
                          int,
                          std::hash<std::string>> s_registered_termprops_by_name{};

#endif // __cpp_lib_generic_unordered_lookup >= 201811

inline auto
register_termprop(std::string_view const& name,
                  unsigned quark,
                  TermpropType type,
                  TermpropFlags flags = TermpropFlags::NONE) -> auto
{
        auto const id = int(s_registered_termprops.size());
        s_registered_termprops.emplace_back(id, quark, type, flags);
        assert(s_registered_termprops[id].id() == id);

        s_registered_termprops_by_name.try_emplace(std::string{name}, id);
        return id;
}

inline auto
register_termprop_alias(std::string_view const& name,
                        TermpropInfo const& info) -> auto
{
        s_registered_termprops_by_name.try_emplace(std::string{name}, info.id());
        return info.id();
}

inline auto
n_registered_termprops() -> auto
{
        return s_registered_termprops.size();
}

inline TermpropInfo const*
get_termprop_info(int id)
{
        return std::addressof(s_registered_termprops.at(id));
}

inline TermpropInfo const*
get_termprop_info(std::string_view const& str) noexcept
{
#if defined(__cpp_lib_generic_unordered_lookup) &&__cpp_lib_generic_unordered_lookup >= 201811
        auto const prop = s_registered_termprops_by_name.find(str);
#else
        auto const prop = s_registered_termprops_by_name.find(std::string{str});
#endif
        return prop == std::end(s_registered_termprops_by_name) ? nullptr : get_termprop_info(prop->second);
}

inline int
get_termprop_id(std::string_view const& str) noexcept
{
#if defined(__cpp_lib_generic_unordered_lookup) &&__cpp_lib_generic_unordered_lookup >= 201811
        auto const prop = s_registered_termprops_by_name.find(str);
#else
        auto const prop = s_registered_termprops_by_name.find(std::string{str});
#endif
        return prop == std::end(s_registered_termprops_by_name) ? -1 : prop->second;
}

// validate_termprop_name:
// @str:
//
// Validates that @str is a valid termprop name. Valid name consists of at least two
// non-empty components delimited by dots ('.'), each component starting with a letter,
// and have least one letter after any dash ('-'), and does not contain consecutive
// dashes.
//
// Returns: %true iff the name is valid
//
inline bool
validate_termprop_name(std::string_view const& str,
                       int n_components_required = 2) noexcept
{
        auto allow_dot = false, allow_letter = true, allow_digit = false;
        auto n_dots = 0;
        auto len = 0;
        for (auto const c : str) {
                ++len;
                switch (c) {
                case '0' ... '9':
                        if (!allow_digit)
                                return false;
                        allow_letter = false;
                        allow_dot = true;
                        break;
                case 'a' ... 'z':
                        if (!allow_letter)
                                return false;

                        allow_dot = allow_digit = true;
                        break;
                case '.':
                        ++n_dots;
                        [[fallthrough]];
                case '-':
                        if (!allow_dot)
                                return false;
                        allow_dot = false;
                        allow_digit = (c == '.' && n_dots >= n_components_required);
                        allow_letter = true;
                        len = 0;
                        break;
                default:
                        return false;
                }
        }

        return (n_dots + 1) >= n_components_required && len > 0;
}

#ifdef VTE_GTK
#if VTE_GTK == 3
using termprop_rgba = vte::color::rgba_base<double>;
#elif VTE_GTK == 4
using termprop_rgba = vte::color::rgba_base<float>;
#else
#error
#endif
#else // Termprop test
using termprop_rgba = vte::color::rgba_base<double>;
#endif

using TermpropURIValue = std::pair<vte::Freeable<GUri>, std::string>;

using TermpropValue = std::variant<std::monostate,
                                   bool,
                                   int64_t,
                                   uint64_t,
                                   double,
                                   termprop_rgba,
                                   vte::uuid,
				   std::string,
                                   TermpropURIValue>;

namespace impl {

inline std::optional<TermpropValue>
parse_termprop_base64(std::string_view const& str) noexcept
{
        auto const size = str.size();
        auto buf = std::string{};
        buf.resize((size / 4) * 3 + 3);

        auto state = 0;
        auto save = 0u;
        auto len = g_base64_decode_step(str.data(),
                                        size,
                                        reinterpret_cast<unsigned char*>(buf.data()),
                                        &state,
                                        &save);

        if (state != 0 || len > TermpropInfo::k_max_data_len)
                return std::nullopt;

        buf.resize(len);
        return buf;
}

inline std::optional<std::string>
unparse_termprop_base64(std::string_view const& str)
{
        return std::make_optional<std::string>
                (vte::glib::take_string
                 (g_base64_encode(reinterpret_cast<unsigned char const*>(str.data()),
                                  str.size())).get());
}

inline std::optional<TermpropValue>
parse_termprop_bool(std::string_view const& str) noexcept
{
        if (str == "1"sv ||
            str == "true"sv ||
            str == "True"sv ||
            str == "TRUE"sv)
                return true;
        else if (str == "0"sv ||
                 str == "false"sv ||
                 str == "False"sv ||
                 str == "FALSE"sv)
                return false;
        else
                return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_bool(bool v)
{
        return std::make_optional(v ? "1"s : "0"s);
}

inline std::optional<TermpropValue>
parse_termprop_color(std::string_view const& str_view,
                     bool with_alpha) noexcept
{
        if (auto value = vte::color::parse_any<termprop_rgba>(std::string{str_view})) {
                auto color = *value;
                if (!with_alpha)
                        color = {color.red(), color.green(), color.blue(), termprop_rgba::component_type(1.0)};

                return color;
        }

        return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_color(termprop_rgba const& v,
                       bool alpha)
{
        return std::make_optional(vte::color::to_string(v, alpha, vte::color::color_output_format::HEX));
}

template<std::integral T>
inline std::optional<TermpropValue>
parse_termprop_integral(std::string_view const& str) noexcept
{
        auto v = T{};
        if (auto [ptr, err] = std::from_chars(std::begin(str),
                                              std::end(str),
                                              v);
            err == std::errc() && ptr == std::end(str)) {
                if constexpr (std::is_unsigned_v<T>) {
                        return uint64_t(v);
                } else {
                        return int64_t(v);
                }
        }

        return std::nullopt;
}

template<std::integral T>
inline std::optional<std::string>
unparse_termprop_integral(T v)
{
        char buf[64];
        if (auto [ptr, err] = std::to_chars(buf,
                                            buf + sizeof(buf),
                                            v);
            err == std::errc()) {
                return std::make_optional<std::string>(buf, ptr);
        }

        return std::nullopt;
}

template<std::floating_point T>
inline std::optional<TermpropValue>
parse_termprop_floating(std::string_view const& str) noexcept
{
        auto v = T{};
        if (auto [ptr, err] = std::from_chars(std::begin(str),
                                              std::end(str),
                                              v,
                                              std::chars_format::general);
            err == std::errc() &&
            ptr == std::end(str) &&
            std::isfinite(v)) {
                return double(v);
        }

        return std::nullopt;
}

template<std::floating_point T>
inline std::optional<std::string>
unparse_termprop_floating(T v)
{
        char buf[64];
        if (auto [ptr, err] = std::to_chars(buf,
                                            buf + sizeof(buf),
                                            v,
                                            std::chars_format::scientific);
            err == std::errc()) {
                return std::make_optional<std::string>(buf, ptr);
        }

        return std::nullopt;
}

// On return, @pos will point to the first character not parsed yet.
inline std::optional<char8_t>
parse_string_escape(std::string_view const& str,
                    std::string_view::size_type& pos) noexcept
{
        if (pos == str.size())
                return std::nullopt;

        switch (char8_t(str[pos++])) {
        case 'n':  return u8'\n'; // U+000A LINE FEED (LF)
        case '\\': return u8'\\'; // U+005C REVERSE SOLIDUS
        case 's':  return u8';'; // U+003B SEMICOLON
        default:   return std::nullopt; // unsupported escape
        }
}

inline std::optional<TermpropValue>
parse_termprop_string(std::string_view str)
{
        static constinit auto const needle = std::string_view{"\\;"};

        auto unescaped = std::string{};
        unescaped.reserve(str.size());

        auto ok = true;
        while (str.size() != 0) {
                auto run = str.find_first_of(needle, 0);

                unescaped.append(str, 0, run);
                if (run == str.npos)
                        break;

                auto const c = str[run];
                if (c == '\\') [[likely]] {
                        ++run;
                        auto ec = parse_string_escape(str, run);
                        ok = bool(ec);
                        if (!ok)
                                break;

                        // c is a 7-bit character actually, so can just append
                        unescaped.push_back(*ec);
                } else if (c == ';') {
                        // unescaped semicolon
                        ok = false;
                        break;
                } else {
                        // Can't happen
                        __builtin_unreachable();
                        ++run;
                }

                str = str.substr(run);
        }

        if (ok &&
            g_utf8_strlen(unescaped.c_str(), unescaped.size()) <= TermpropInfo::k_max_string_len)
                return unescaped;

        return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_string(std::string_view str)
{
        static constinit auto const needle = ";\n\\"sv;

        auto ostr = std::string{};
        while (str.size() != 0) {
                auto run = str.find_first_of(needle, 0);

                ostr.append(str, 0, run);
                if (run == str.npos)
                        break;

                ostr.push_back('\\');
                switch (str[run]) {
                case '\n': ostr.push_back('n');  break;
                case '\\': ostr.push_back('\\'); break;
                case ';':  ostr.push_back('s');  break;
                default: __builtin_unreachable(); break;
                }

                ++run;
                str = str.substr(run);
        }

        return std::make_optional(std::move(ostr));
}

inline std::optional<TermpropValue>
parse_termprop_uuid(std::string_view str)
{
        try {
                return vte::uuid{str};
        } catch (...) {
                return std::nullopt;
        }
}

inline std::optional<std::string>
unparse_termprop_uuid(vte::uuid const& u)
{
        return std::make_optional(u.str());
}

inline std::optional<TermpropValue>
parse_termprop_uri(std::string_view str)
{
        if (auto uri = vte::take_freeable(g_uri_parse(std::string{str}.c_str(),
                                                      GUriFlags(G_URI_FLAGS_NONE),
                                                      nullptr));
            uri &&
            g_uri_get_scheme(uri.get()) &&
            !g_str_equal(g_uri_get_scheme(uri.get()), "data")) {
                return std::make_optional<TermpropValue>(std::in_place_type<TermpropURIValue>, std::move(uri), str);
        }

        return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_uri(TermpropURIValue const& v)
{
        return std::make_optional<std::string>(v.second);
}

} // namespace impl

inline std::optional<TermpropValue>
parse_termprop_value(TermpropType type,
                     std::string_view const& value) noexcept
{
        switch (type) {
                using enum TermpropType;
        case TermpropType::VALUELESS:
                return std::nullopt;

        case TermpropType::BOOL:
                return impl::parse_termprop_bool(value);

        case TermpropType::INT:
                return impl::parse_termprop_integral<int64_t>(value);

        case TermpropType::UINT:
                return impl::parse_termprop_integral<uint64_t>(value);

        case TermpropType::DOUBLE:
                return impl::parse_termprop_floating<double>(value);

        case TermpropType::RGB:
        case TermpropType::RGBA:
                return impl::parse_termprop_color(value, type == RGBA);

        case TermpropType::STRING:
                return impl::parse_termprop_string(value);

        case TermpropType::DATA:
                return impl::parse_termprop_base64(value);

        case TermpropType::UUID:
                return impl::parse_termprop_uuid(value);

        case TermpropType::URI:
                return impl::parse_termprop_uri(value);

        default:
                __builtin_unreachable();
                return std::nullopt;
        }
}

inline std::optional<std::string>
unparse_termprop_value(TermpropType type,
                       TermpropValue const& value)
{
        switch (type) {
                using enum vte::terminal::TermpropType;
        case VALUELESS:
                break;

        case BOOL:
                if (std::holds_alternative<bool>(value)) {
                        return impl::unparse_termprop_bool(std::get<bool>(value));
                }
                break;

        case INT:
                if (std::holds_alternative<int64_t>(value)) {
                        return impl::unparse_termprop_integral(std::get<int64_t>(value));
                }
                break;

        case UINT:
                if (std::holds_alternative<uint64_t>(value)) {
                        return impl::unparse_termprop_integral(std::get<uint64_t>(value));
                }
                break;

        case DOUBLE:
                if (std::holds_alternative<double>(value)) {
                        return impl::unparse_termprop_floating(std::get<double>(value));
                }
                break;

        case RGB:
        case RGBA:
                if (std::holds_alternative<vte::terminal::termprop_rgba>(value)) {
                        return impl::unparse_termprop_color(std::get<vte::terminal::termprop_rgba>(value),
                                                            type == RGBA);
                }
                break;

        case STRING:
                if (std::holds_alternative<std::string>(value)) {
                        return impl::unparse_termprop_string(std::get<std::string>(value));
                }
                break;

        case DATA:
                if (std::holds_alternative<std::string>(value)) {
                        return impl::unparse_termprop_base64(std::get<std::string>(value));
                }
                break;

        case UUID:
                if (std::holds_alternative<vte::uuid>(value)) {
                        return impl::unparse_termprop_uuid(std::get<vte::uuid>(value));
                }
                break;

        case URI:
                if (std::holds_alternative<TermpropURIValue>(value)) {
                        return impl::unparse_termprop_uri(std::get<TermpropURIValue>(value));
                }
                break;

        default:
                __builtin_unreachable();
                break;
        }

        return std::nullopt;
}

} // namespace vte::terminal
