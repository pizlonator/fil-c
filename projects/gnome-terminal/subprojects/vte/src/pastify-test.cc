/*
 * Copyright © 2021 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib.h>

#include "pastify.hh"

using namespace std::literals;

static bool
assert_streq(std::string_view const& str1,
             std::string_view const& str2)
{
        auto s1 = std::string{str1};
        auto s2 = std::string{str2};

        g_assert_cmpstr(s1.c_str(), ==, s2.c_str());
        return true;
}

static void
test_pastify(std::string_view const& str,
             std::string_view const& expected,
             bool insert_brackets = false,
             bool c1 = false)
{
        auto rv = vte::terminal::pastify_string(str, insert_brackets, c1);
        assert_streq(rv, expected);

        /* Check idempotence */
        if (!insert_brackets) {
                auto rv2 = vte::terminal::pastify_string(rv, false, false);
                assert_streq(rv, rv2);
        }
}

static void
test_pastify_brackets_c0(void)
{
        test_pastify("0"sv, "\e[200~0\e[201~"sv, true, false);
}

static void
test_pastify_brackets_c1(void)
{
        test_pastify("0"sv, "\xc2\x9b" "200~0\xc2\x9b" "201~"sv, true, true);
}

static void
test_pastify_control(std::string const& ctrl,
                     std::string const& pict)
{
        test_pastify(ctrl, pict);
        test_pastify(ctrl + ctrl, pict + pict);
        test_pastify("abc"s + ctrl, "abc"s + pict);
        test_pastify("abc"s + ctrl + ctrl, "abc"s + pict + pict);
        test_pastify(ctrl + "abc"s, pict + "abc"s);
        test_pastify(ctrl + ctrl + "abc"s, pict + pict + "abc"s);
        test_pastify("abc"s + ctrl + "abc"s, "abc"s + pict + "abc"s);
        test_pastify("abc"s + ctrl + ctrl + "abc"s, "abc"s + pict + pict + "abc"s);
}

static void
test_pastify_control_c0(void const* ptr)
{
        auto const c = *reinterpret_cast<unsigned char const*>(ptr);
        auto ctrl = ""s;
        ctrl.push_back(c);

        auto pict = std::string{};
        vte::terminal::append_control_picture(pict, c);

        test_pastify_control(ctrl, pict);
}

static void
test_pastify_control_c1(void const* ptr)
{
        auto const c = *reinterpret_cast<unsigned char const*>(ptr);

        auto ctrl = ""s;
        ctrl.push_back(0xc2);
        ctrl.push_back(c);

        auto pict = std::string{};
        vte::terminal::append_control_picture(pict, c);

        test_pastify_control(ctrl, pict);
}

struct TestString {
public:
        char const* m_str;
        char const* m_expected;
        int m_line;

        TestString() = default;
        consteval TestString(char const* str,
                             char const* expected,
                             int line = __builtin_LINE()) noexcept :
                m_str(str),
                m_expected(expected),
                m_line(line)
        {
        }
};

consteval auto
identity_test(char const *str,
              int line = __builtin_LINE()) noexcept
{
        return TestString(str, str, line);
}

static void
test_pastify_string(void const* ptr)
{
        auto str = reinterpret_cast<TestString const*>(ptr);
        test_pastify(str->m_str, str->m_expected);
}

static constinit TestString const test_strings[] = {
        /* Controls */
        identity_test("\x09"), /* HT passes through */
        identity_test("\x0d"), /* CR passes through */

        /* Non-C1 but starting with a 0xC2 byte */
        identity_test("abc\xc2\xa0xyz"),

        /* CR/LF conversion */
        TestString("\x0a", "\x0d"),
        TestString("\x0a\x0d", "\x0d\x0d"),
        TestString("\x0d\x0a", "\x0d"),
        TestString("\x0d\x0a\x0d", "\x0d\x0d"),
        TestString("\x0d\x0a\x0d\x0a", "\x0d\x0d"),
};

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/pastify/brackets/c0", test_pastify_brackets_c0);
        g_test_add_func("/vte/pastify/brackets/c1", test_pastify_brackets_c1);

        /* C0 controls */
        for (auto c = 0; c < 0x20; ++c) {
                /* NUL, HT, CR, LF */
                if (c == 0 || c == 0x09 || c == 0x0a || c == 0x0d)
                        continue;

                char path[64];
                g_snprintf(path, sizeof(path), "/vte/pastify/controls/c0/%02x", c);

                auto ptr = g_new(unsigned char, 1);
                *ptr = c;

                g_test_add_data_func_full(path, ptr, test_pastify_control_c0, g_free);
        }

        /* DEL too */
        {
                auto const path = "/vte/pastify/controls/c0/7f";
                auto ptr = g_new(unsigned char, 1);
                *ptr = 0x7f;

                g_test_add_data_func_full(path, ptr, test_pastify_control_c0, g_free);
        }

        /* C1 controls */
        for (auto c = 0x80; c < 0xa0; ++c) {
                char path[64];
                g_snprintf(path, sizeof(path), "/vte/pastify/controls/c1/%02x", c);

                auto ptr = g_new(unsigned char, 1);
                *ptr = c;

                g_test_add_data_func_full(path, ptr, test_pastify_control_c1, g_free);
        }

        /* Extra test strings */
        for (auto i = 0u; i < G_N_ELEMENTS (test_strings); ++i) {
                auto const* str = &test_strings[i];

                char path[64];
                g_snprintf(path, sizeof(path), "/vte/pastify/string/%d", str->m_line);
                g_test_add_data_func(path, str, test_pastify_string);
        }

        return g_test_run();
}
