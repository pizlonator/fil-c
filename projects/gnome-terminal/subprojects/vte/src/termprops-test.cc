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

#include "config.h"

#include "termprops.hh"

#include <cassert>
#include <string>

using namespace std::literals;
using namespace vte::terminal;

static void
assert_name_valid(std::string_view const& str,
                  int required_components = 2,
                  int line = __builtin_LINE()) noexcept
{
        assert(validate_termprop_name(str, required_components));
}

static void
assert_name_invalid(std::string_view const& str,
                  int required_components = 2,
                  int line = __builtin_LINE()) noexcept
{
        assert(!validate_termprop_name(str, required_components));
}

static void
test_termprops_names(void)
{
        assert_name_valid("a.b"sv);
        assert_name_valid("a.b.c"sv);
        assert_name_valid("a.b.c.d"sv);
        assert_name_valid("a-a.b"sv);
        assert_name_valid("a0.b"sv);
        assert_name_valid("a-a1.b"sv);
        assert_name_valid("a.b-b"sv);
        assert_name_valid("a.b1-b"sv);
        assert_name_valid("a"sv, 1);
        assert_name_valid("a.b"sv, 2);
        assert_name_valid("a.b.c"sv, 3);
        assert_name_valid("a.b.c.d"sv, 4);
        assert_name_valid("a.b.c.d.e"sv, 5);
        assert_name_invalid("a"sv);
        assert_name_invalid("a"sv);
        assert_name_invalid("a."sv);
        assert_name_invalid(".a"sv);
        assert_name_invalid("-a.b"sv);
        assert_name_invalid("0.b"sv);
        assert_name_invalid("0.b0a"sv);
        assert_name_invalid("-0.b"sv);
        assert_name_invalid("a.0"sv);
        assert_name_invalid("a.b0a"sv);
        assert_name_invalid("a.-b"sv);
        assert_name_invalid("a.-0"sv);
        assert_name_invalid("a"sv, 2);
        assert_name_invalid("a.b"sv, 3);
        assert_name_invalid("a.b.c"sv, 4);
        assert_name_invalid("a.b.c.d"sv, 5);
        assert_name_invalid("a.b.c.d.e"sv, 6);
        assert_name_invalid("a..b");
        assert_name_invalid("a--b");
        assert_name_invalid("A.b"sv);
        assert_name_invalid("a.B"sv);

        assert_name_invalid("a.b.0"sv, 3);
        assert_name_valid("a.b.0"sv, 2);

        assert_name_invalid("a.b.0-1"sv, 2);
}

static void
assert_termprop_parse_nothing(TermpropType type,
                              std::string_view const& str,
                              int line = __builtin_LINE()) noexcept
{
        auto value = parse_termprop_value(type, str);
        assert(!value);
}

static void
assert_registered(char const* name,
                  TermpropType type) noexcept
{
        register_termprop(name, g_quark_from_string(name), type);
        auto const info = get_termprop_info(name);
        assert(info);
        assert(info->type() == type);
}

static void
test_termprops_register(void)
{
        assert_registered("test.valueless", TermpropType::VALUELESS);
        assert_registered("test.bool", TermpropType::BOOL);
        assert_registered("test.uint", TermpropType::UINT);
        assert_registered("test.string", TermpropType::STRING);
        assert_registered("test.data", TermpropType::DATA);
}

template<typename T>
static void
assert_termprop_parse_value(TermpropType type,
                            std::string_view const& str,
                            T const& expected_value = {},
                            int line = __builtin_LINE()) noexcept
{
        auto const value = parse_termprop_value(type, str);
        assert(value);
        assert(!value->valueless_by_exception());
        assert(std::holds_alternative<T>(*value));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        assert(std::get<T>(*value) == expected_value);
#pragma GCC diagnostic pop

        auto tstr = unparse_termprop_value(type, *value);
        assert(tstr);
        auto const tvalue = parse_termprop_value(type, *tstr);
        assert(tvalue);
        assert(!tvalue->valueless_by_exception());
        assert(std::holds_alternative<T>(*tvalue));
        assert(value == tvalue);
        assert(*value == *tvalue);
}

template<std::integral T>
static void
assert_termprop_parse_integral_value(TermpropType type,
                                     std::string_view const& str,
                                     T const& expected_value = {},
                                     int line = __builtin_LINE()) noexcept
{
        using V = std::conditional_t<std::is_unsigned_v<T>, uint64_t, int64_t>;
        assert_termprop_parse_value<V>(type, str, V{expected_value}, line);
}

static void
assert_termprop_parse_uri(std::string_view const& str,
                          int line = __builtin_LINE()) noexcept
{
        auto const value = parse_termprop_value(TermpropType::URI, str);
        assert(value);
        assert(!value->valueless_by_exception());
        assert(std::holds_alternative<vte::terminal::TermpropURIValue>(*value));

        assert(str == std::get<vte::terminal::TermpropURIValue>(*value).second);

        auto ustr = vte::glib::take_string(g_uri_to_string(std::get<vte::terminal::TermpropURIValue>(*value).first.get()));
        assert(ustr);
        assert(str == ustr.get());
}

static void
test_termprops_valueless(void)
{
        assert_termprop_parse_nothing(TermpropType::VALUELESS, ""sv);
        assert_termprop_parse_nothing(TermpropType::VALUELESS, "0"sv);
        assert_termprop_parse_nothing(TermpropType::VALUELESS, "1"sv);
        assert_termprop_parse_nothing(TermpropType::VALUELESS, "a"sv);
}

static void
test_termprops_bool(void)
{
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "0"sv, false);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "1"sv, true);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "false"sv, false);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "true"sv, true);

        // Case variants
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "False"sv, false);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "True"sv, true);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "FALSE"sv, false);
        assert_termprop_parse_value<bool>(TermpropType::BOOL, "TRUE"sv, true);

        // Invalid case variants
        assert_termprop_parse_nothing(TermpropType::BOOL, "tRue"sv);
        assert_termprop_parse_nothing(TermpropType::BOOL, "FaLSe"sv);

        // No other names
        assert_termprop_parse_nothing(TermpropType::BOOL, "yes"sv);
        assert_termprop_parse_nothing(TermpropType::BOOL, "no"sv);
}

static void
test_termprops_int(void)
{
        assert_termprop_parse_integral_value(TermpropType::INT, "0"sv, 0ll);
        assert_termprop_parse_integral_value(TermpropType::INT, "1"sv, 1ll);
        assert_termprop_parse_integral_value(TermpropType::INT, "9223372036854775807"sv, 9223372036854775807ll);
        assert_termprop_parse_integral_value(TermpropType::INT, "-1"sv, -1ll);
        assert_termprop_parse_integral_value(TermpropType::INT, "-9223372036854775808"sv, INT64_MIN);
        assert_termprop_parse_nothing(TermpropType::INT, "9223372036854775808"sv);
        assert_termprop_parse_nothing(TermpropType::INT, "-9223372036854775809"sv);
        assert_termprop_parse_nothing(TermpropType::INT, "0a"sv);
        assert_termprop_parse_nothing(TermpropType::INT, "a0"sv);
        assert_termprop_parse_nothing(TermpropType::INT, "-"sv);
        assert_termprop_parse_nothing(TermpropType::INT, "-a"sv);
}

static void
test_termprops_uint(void)
{
        assert_termprop_parse_integral_value(TermpropType::UINT, "0"sv, 0ull);
        assert_termprop_parse_integral_value(TermpropType::UINT, "1"sv, 1ull);
        assert_termprop_parse_integral_value(TermpropType::UINT, "18446744073709551614"sv, 18446744073709551614ull);
        assert_termprop_parse_integral_value(TermpropType::UINT, "18446744073709551615"sv, 18446744073709551615ull);
        assert_termprop_parse_nothing(TermpropType::UINT, "-1"sv);
        assert_termprop_parse_nothing(TermpropType::UINT, "0a"sv);
        assert_termprop_parse_nothing(TermpropType::UINT, "a0"sv);
        assert_termprop_parse_nothing(TermpropType::UINT, "18446744073709551616"sv);
}

static void
test_termprops_double(void)
{
        assert_termprop_parse_value(TermpropType::DOUBLE, "0"sv, 0.0);
        assert_termprop_parse_value(TermpropType::DOUBLE, "0.1"sv, 0.1);
        assert_termprop_parse_value(TermpropType::DOUBLE, "1.0"sv, 1.0);
        assert_termprop_parse_value(TermpropType::DOUBLE, "2.0E8"sv, 2.0E8);

        // No leading whitespace
        assert_termprop_parse_nothing(TermpropType::DOUBLE, " 1.0"sv);

        // No trailing whitespace
        assert_termprop_parse_nothing(TermpropType::DOUBLE, "1.0 "sv);

        // No hex format
        assert_termprop_parse_nothing(TermpropType::DOUBLE, "0x12345678"sv);

        // No infinities
        assert_termprop_parse_nothing(TermpropType::DOUBLE, "Inf"sv);
        assert_termprop_parse_nothing(TermpropType::DOUBLE, "-Inf"sv);

        // No NaNs
        assert_termprop_parse_nothing(TermpropType::DOUBLE, "NaN"sv);
}

static void
test_termprops_rgb(void)
{
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGB, "#123456"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0x123456, 8, false));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGB, "#abcdef"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdef, 8, false));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGB, "#ABCDEF"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdef, 8, false));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGB, "#000"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0, 8, false));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGB, "#0000"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0, 8, false));
        assert_termprop_parse_nothing(TermpropType::RGB, "0"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "00"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "000"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "0000"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "00000"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "000000"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "#0"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "#00"sv);
        assert_termprop_parse_nothing(TermpropType::RGB, "#00000"sv);
        //assert_termprop_parse_nothing(TermpropType::RGB, "rgb(1,2,3)"sv);
}

static void
test_termprops_rgba(void)
{
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#123456"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0x123456ff, 8, true));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#abcdef"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdefff, 8, true));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#ABCDEF"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdefff, 8, true));

        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#12345678"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0x12345678, 8, true));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#abcdef01"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdef01, 8, true));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#ABCDEF76"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0xabcdef76, 8, true));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#000"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0, 8, false));
        assert_termprop_parse_value<vte::terminal::termprop_rgba>(TermpropType::RGBA, "#0000"sv, vte::color::from_bits<vte::terminal::termprop_rgba>(0, 8, true));
        assert_termprop_parse_nothing(TermpropType::RGBA, "0"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "00"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "0000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "00000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "000000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "0000000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "00000000"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "#0"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "#00"sv);
        assert_termprop_parse_nothing(TermpropType::RGBA, "#00000"sv);
        //assert_termprop_parse_nothing(TermpropType::RGBA, "rgb(1,2,3)"sv);
        //assert_termprop_parse_nothing(TermpropType::RGBA, "rgba(1,2,3,4)"sv);
}

// Note that our OSC parser makes sure no C0 and C1 controls are
// present in the control string, so we do not need to test how the
// termprop parser handles these.

static void
test_termprops_string(void)
{
        assert_termprop_parse_value<std::string>(TermpropType::STRING, ""sv, ""s);
        assert_termprop_parse_value<std::string>(TermpropType::STRING, "abc"sv, "abc"s);

        auto const max_len = TermpropInfo::k_max_string_len;

        auto str = std::string{};
        str.resize(max_len, 'a');
        assert_termprop_parse_value<std::string>(TermpropType::STRING, str, str);

        str.push_back('a');
        assert_termprop_parse_nothing(TermpropType::STRING, str);

        // Test escapes
        assert_termprop_parse_value<std::string>(TermpropType::STRING, "a\\sb\\nc\\\\d"sv, "a;b\nc\\d"s);

        // Test string value containing the termprop assignment characters ! or =
        assert_termprop_parse_value<std::string>(TermpropType::STRING, "a=b"sv, "a=b"s);
        assert_termprop_parse_value<std::string>(TermpropType::STRING, "a!"sv, "a!"s);

        // Missing or invalid escapes
        assert_termprop_parse_nothing(TermpropType::STRING, "a;b");
        assert_termprop_parse_nothing(TermpropType::STRING, "a\\");
        assert_termprop_parse_nothing(TermpropType::STRING, "a\\"sv);
        assert_termprop_parse_nothing(TermpropType::STRING, "a\\a"sv);
}

static void
test_termprops_data(void)
{
        assert_termprop_parse_value<std::string>(TermpropType::DATA, ""sv, ""s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "YQ=="sv, "a"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "YWE="sv, "aa"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "YWFh"sv, "aaa"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "AA=="sv, "\0"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "YQBi"sv, "a\0b"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "YQBi"sv, "a\0b"s);
        assert_termprop_parse_value<std::string>(TermpropType::DATA, "gMH/YWJj"sv, "\x80\xc1\xff""abc"s); // Note: not valid UTF-8 after decoding
        assert_termprop_parse_nothing(TermpropType::DATA, "YQ="sv);
        assert_termprop_parse_nothing(TermpropType::DATA, "YQ"sv);
        assert_termprop_parse_nothing(TermpropType::DATA, "Y"sv);
}

static void
test_termprops_uuid(void)
{
        auto const uuid = VTE_DEFINE_UUID(49ec5248, 2d9a, 493f, 99fa, 9e1cfb95b430);
        assert_termprop_parse_value<vte::uuid>(TermpropType::UUID, "49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv, uuid);
        assert_termprop_parse_value<vte::uuid>(TermpropType::UUID, "{49ec5248-2d9a-493f-99fa-9e1cfb95b430}"sv, uuid);
        assert_termprop_parse_value<vte::uuid>(TermpropType::UUID, "urn:uuid:49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv, uuid);
        assert_termprop_parse_nothing(TermpropType::UUID, "49ec5248-2d9a-493f-99fa-9e1cfb95b43"sv);
        assert_termprop_parse_nothing(TermpropType::UUID, "{49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv);
        assert_termprop_parse_nothing(TermpropType::UUID, "urn:49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv);
        assert_termprop_parse_nothing(TermpropType::UUID, "{urn:uuid:49ec5248-2d9a-493f-99fa-9e1cfb95b430}"sv);
}

static void
test_termprops_uri(void)
{
        assert_termprop_parse_uri("https://www.gnome.org/index.html"sv);
        assert_termprop_parse_uri("file:///uri/bin"sv);
        assert_termprop_parse_nothing(TermpropType::URI, "data:text/plain;base64,QQo=");
        assert_termprop_parse_nothing(TermpropType::URI, "data:text/plain%3BQbase64,Qo=");
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/terminal/termprops/names", test_termprops_names);
        g_test_add_func("/vte/terminal/termprops/register", test_termprops_register);
        g_test_add_func("/vte/terminal/termprops/type/valueless", test_termprops_valueless);
        g_test_add_func("/vte/terminal/termprops/type/bool", test_termprops_bool);
        g_test_add_func("/vte/terminal/termprops/type/int", test_termprops_int);
        g_test_add_func("/vte/terminal/termprops/type/uint", test_termprops_uint);
        g_test_add_func("/vte/terminal/termprops/type/double", test_termprops_double);
        g_test_add_func("/vte/terminal/termprops/type/rgb", test_termprops_rgb);
        g_test_add_func("/vte/terminal/termprops/type/rgba", test_termprops_rgba);
        g_test_add_func("/vte/terminal/termprops/type/string", test_termprops_string);
        g_test_add_func("/vte/terminal/termprops/type/data", test_termprops_data);
        g_test_add_func("/vte/terminal/termprops/type/uuid", test_termprops_uuid);
        g_test_add_func("/vte/terminal/termprops/type/uri", test_termprops_uri);

        return g_test_run();
}
