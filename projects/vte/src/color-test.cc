/*
 * Copyright © 2023 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Some code below was copied from gtk+, there under LGPL2+; used and modified
 * here and distributed under LGPL3+.
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "color-parser.hh"

#include <cassert>
#include <cstdint>

#include <glib.h>

using namespace vte;
using namespace std::literals;

using rgba = color::rgba_base<float>;

enum class color_format {
        CSSLIKE,
        X11LIKE,
};

template<class Color = rgba>
static std::optional<Color>
parse(std::string const& spec,
      color_format fmt = color_format::CSSLIKE)
{
        switch(fmt) {
                using enum color_format;
        case CSSLIKE:
                return color::parse_csslike<Color>(spec);
        case X11LIKE:
                return color::parse_x11like<Color>(spec);
        default:
                __builtin_unreachable();
                return std::nullopt;
        }
}

template<class Color = rgba>
static void
assert_color_parse_nothing(std::string_view const& str,
                           color_format fmt,
                           int line = __builtin_LINE()) noexcept
{
        auto value = parse<Color>(std::string{str}, fmt);
        assert(!value);
}

template<typename Color>
static void
assert_color_parse_value(std::string_view const& str,
                         Color const& expected_value,
                         color_format fmt,
                         int line = __builtin_LINE()) noexcept
{
        auto const value = parse<Color>(std::string{str}, fmt);
        assert(value);
        assert(*value == expected_value);
}

static void
test_color_parse_css(void)
{
        assert_color_parse_value("rgba(100,90,80,0.1)", rgba(100/255., 90/255., 80/255., 0.1), color_format::CSSLIKE);
        assert_color_parse_value("rgba(40%,30%,20%,0.1)", rgba(0.4, 0.3, 0.2, 0.1), color_format::CSSLIKE);

        assert_color_parse_value("rgba(  40 % ,  30 %  ,   20 % ,  0.1    )", rgba(0.4, 0.3, 0.2, 0.1), color_format::CSSLIKE);

        assert_color_parse_value("red", rgba(1.0, 0.0, 0.0, 1.0), color_format::CSSLIKE);

        assert_color_parse_value("#0080ff", rgba(0.0, 0x8080 / 65535., 1.0, 1.0), color_format::CSSLIKE);
        assert_color_parse_value("#0080ff80", rgba(0.0, 0x8080 / 65535., 1.0, 0x8080 / 65535.), color_format::CSSLIKE);

        assert_color_parse_value("rgb(0,0,0)", rgba(0.0, 0.0, 0.0, 1.0), color_format::CSSLIKE);

        assert_color_parse_value("hsl (0, 100%, 50%)", rgba(1.0, 0.0, 0.0, 1.0), color_format::CSSLIKE);
        assert_color_parse_value("hsla (120, 255, 50%, 0.1)", rgba(0.0, 1.0, 0.0, 0.1), color_format::CSSLIKE);
        assert_color_parse_value("hsl(180, 100%, 25%)", rgba(0.0, 0.5, 0.5, 1.0), color_format::CSSLIKE);
}

static void
test_color_parse_x11(void)
{
        assert_color_parse_value("#789", color::from_bits<rgba>(UINT64_C(0x789), 4, false), color_format::X11LIKE);
        assert_color_parse_value("#78899a", color::from_bits<rgba>(UINT64_C(0x78899a), 8, false), color_format::X11LIKE);
        assert_color_parse_value("#7899abbcd", color::from_bits<rgba>(UINT64_C(0x7899abbcd), 12, false), color_format::X11LIKE);
        assert_color_parse_value("#789a9abcbcde", color::from_bits<rgba>(UINT64_C(0x789a9abcbcde), 16, false), color_format::X11LIKE);

        assert_color_parse_value("rgb:7/8/9", color::from_bits<rgba>(UINT64_C(0x789), 4, false), color_format::X11LIKE);
        assert_color_parse_value("rgb:78/89/9a", color::from_bits<rgba>(UINT64_C(0x78899a), 8, false), color_format::X11LIKE);
        assert_color_parse_value("rgb:789/9ab/bcd", color::from_bits<rgba>(UINT64_C(0x7899abbcd), 12, false), color_format::X11LIKE);
        assert_color_parse_value("rgb:789a/9abc/bcde", color::from_bits<rgba>(UINT64_C(0x789a9abcbcde), 16, false), color_format::X11LIKE);
}

static void
assert_color_parse_named(char const* str,
                         uint32_t color,
                         int line = __builtin_LINE()) noexcept
{
        assert_color_parse_value(str,
                                 color::from_bits<rgba>(color, 8, false),
                                 color_format::X11LIKE,
                                 line);
}

static void
test_color_parse_named(void)
{
#include "color-names-tests.hh"
}

static void
test_color_parse_nothing(void)
{
        assert_color_parse_nothing("", color_format::CSSLIKE);
        assert_color_parse_nothing("foo", color_format::CSSLIKE);

        assert_color_parse_nothing("rgba(100,90,80,0.1)", color_format::X11LIKE);
        assert_color_parse_nothing("rgb:00/00/00", color_format::CSSLIKE);

        // http://bugzilla.gnome.org/show_bug.cgi?id=667485
        assert_color_parse_nothing("rgb(,,)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(%,%,%)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(nan,nan,nan)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(inf,inf,inf)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(1p12,0,0)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(5d1%,1,1)", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(0,0,0)foo", color_format::CSSLIKE);
        assert_color_parse_nothing("rgb(0,0,0)  foo", color_format::CSSLIKE);
        assert_color_parse_nothing("#XGB", color_format::CSSLIKE);
        assert_color_parse_nothing("#XGBQ", color_format::CSSLIKE);
        assert_color_parse_nothing("#AAAAXGBQ", color_format::CSSLIKE);

        assert_color_parse_nothing("rgb:00000/000000/000000", color_format::X11LIKE);
        assert_color_parse_nothing("rgbi:0.0/0.0/0.0", color_format::X11LIKE);
}

static void
test_color_to_string (void)
{
        auto test = [](std::string str,
                       bool alpha = false) noexcept -> void
        {
                auto const value = parse<rgba>(str);
                assert(value);

                auto tstr = color::to_string(*value, alpha);
                g_assert_true(g_ascii_strcasecmp(str.c_str(), tstr.c_str()) == 0);
        };

        test("#000000"s);
        test("#00000000"s, true);

        test("#123456"s);
        test("#12345678"s, true);

        test("#ffffff"s);
        test("#ffffffff"s, true);
}

int
main(int argc,
     char *argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/color/parse/css", test_color_parse_css);
        g_test_add_func("/vte/color/parse/x11", test_color_parse_x11);
        g_test_add_func("/vte/color/parse/named", test_color_parse_named);
        g_test_add_func("/vte/color/parse/nothing", test_color_parse_nothing);
        g_test_add_func("/vte/color/to-string", test_color_to_string);

        return g_test_run();
}
