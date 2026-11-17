/*
 * Copyright © 2018 Christian Persch
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

#include "config.h"

#include <algorithm>

#include <glib.h>

#include "tabstops.hh"
#include "vtedefines.hh"

using namespace vte::terminal;

using position_t = unsigned int;
static inline constexpr position_t const npos = -1;

static void
tabstops_set(Tabstops& t,
             std::initializer_list<Tabstops::position_t> l)
{
        for (auto i : l)
                t.set(i);
}

static void
assert_tabstops_default(Tabstops const& t,
                        Tabstops::position_t start = 0,
                        Tabstops::position_t end = Tabstops::position_t(-1),
                        Tabstops::position_t tab_width = VTE_TAB_WIDTH)
{
        if (end == t.npos)
                end = t.size();

        for (auto i = start; i < end; i++) {
                if (i % tab_width)
                        g_assert_false(t.get(i));
                else
                        g_assert_true(t.get(i));
        }
}

static void
assert_tabstops_clear(Tabstops const& t,
                      Tabstops::position_t start = 0,
                      Tabstops::position_t end = Tabstops::position_t(-1))
{
        if (end == t.npos)
                end = t.size();

        for (auto i = start; i < end; i++)
                g_assert_false(t.get(i));
}

static void
assert_tabstops(Tabstops const& t,
                std::initializer_list<Tabstops::position_t> l,
                Tabstops::position_t start = 0,
                Tabstops::position_t end = Tabstops::position_t(-1))
{
        if (end == t.npos)
                end = t.size();

        auto it = l.begin();
        for (auto i = start; i < end; i++) {
                if (it != l.end() && i == *it) {
                        g_assert_true(t.get(i));
                        ++it;
                } else
                        g_assert_false(t.get(i));

        }
        g_assert_true(it == l.end());
}

static void
assert_tabstops_previous(Tabstops const& t,
                         std::initializer_list<std::pair<Tabstops::position_t, Tabstops::position_t>> l,
                         int count = 1,
                         position_t endpos = npos)
{
        for (auto p : l) {
                g_assert_cmpuint(t.get_previous(p.first, count, endpos), ==, p.second);
        }
}

static void
assert_tabstops_next(Tabstops const& t,
                     std::initializer_list<std::pair<Tabstops::position_t, Tabstops::position_t>> l,
                     int count = 1,
                     position_t endpos = npos)
{
        for (auto p : l) {
                g_assert_cmpuint(t.get_next(p.first, count, endpos), ==, p.second);
        }
}

static void
test_tabstops_default(void)
{
        Tabstops t{};
        g_assert_cmpuint(t.size(), ==, VTE_COLUMNS);

        assert_tabstops_default(t);
}

static void
test_tabstops_get_set(void)
{
        Tabstops t{256, false};

        tabstops_set(t, {42, 200});
        assert_tabstops(t, {42, 200});
}

static void
test_tabstops_clear(void)
{
        Tabstops t{128, true};
        t.clear();
        assert_tabstops_clear(t);
}

static void
test_tabstops_reset(void)
{
        unsigned int const tab_width = 7;

        Tabstops t{80, true, tab_width};
        assert_tabstops_default(t, 0, t.npos, tab_width);

        t.resize(80);
        t.resize(160, false, tab_width);
        assert_tabstops_default(t, 0, 80, tab_width);
        assert_tabstops_clear(t, 80, t.npos);

        t.resize(80);
        t.clear();
        t.resize(160, true, tab_width);
        assert_tabstops_clear(t, 0, 80);
        assert_tabstops_default(t, 80, t.npos, tab_width);

        t.resize(256);
        t.reset(tab_width);
        assert_tabstops_default(t, 0, t.npos, tab_width);
        t.resize(1024, true, tab_width);
        assert_tabstops_default(t, 0, t.npos, tab_width);
        t.resize(4096, true, tab_width);
        assert_tabstops_default(t, 0, t.npos, tab_width);
}

static void
test_tabstops_resize(void)
{
        Tabstops t;
        t.resize(80);

        t.reset();
        assert_tabstops_default(t);
        t.resize(161, false);
        assert_tabstops_default(t, 0, 80);
        assert_tabstops_clear(t, 80, t.npos);
}

static void
test_tabstops_previous(void)
{
        Tabstops t{512, false};
        tabstops_set(t, {0, 31, 32, 63, 64, 255, 256});
        assert_tabstops_previous(t, {{511, 256}, {256, 255}, {255, 64}, {64, 63}, {63, 32}, {32, 31}, {31, 0}});
        assert_tabstops_previous(t, {{511, 255}, {257, 255}, {254, 63}, {64, 32}, {33, 31}, {32, 0}, {31, t.npos}, {0, t.npos}}, 2);

        t.clear();
        tabstops_set(t, {127, 256});
        assert_tabstops_previous(t, {{511, 256}, {256, 127}, {127, t.npos}});
        assert_tabstops_previous(t, {{384, 256}, {192, 127}, {92, t.npos}});

        assert_tabstops_previous(t, {{384, 256}, {256, 192}, {192, 192}, {191, 192}}, 1, 192);

        unsigned int const tab_width = 3;
        t.reset(tab_width);

        for (unsigned int p = 1 ; p < t.size(); ++p) {
                g_assert_cmpuint(t.get_previous(p), ==, (p - 1) / tab_width * tab_width);
        }
        g_assert_cmpuint(t.get_previous(0), ==, t.npos);
}

static void
test_tabstops_next(void)
{
        Tabstops t{512, false};
        tabstops_set(t, {0, 31, 32, 63, 64, 255, 256});
        assert_tabstops_next(t, {{0, 31}, {31, 32}, {32, 63}, {63, 64}, {64, 255}, {255, 256}, {256, t.npos}});
        assert_tabstops_next(t, {{0, 32}, {2, 32}, {31, 63}, {48, 64}, {128, 256}, {255, t.npos}}, 2);

        t.clear();
        tabstops_set(t, {127, 256});
        assert_tabstops_next(t, {{0, 127}, {127, 256}, {256, t.npos}});
        assert_tabstops_next(t, {{1, 127}, {192, 256}, {384, t.npos}});

        assert_tabstops_next(t, {{64, 127}, {127, 192}, {192, 192}, {193, 192}}, 1, 192);

        unsigned int const tab_width = 3;
        t.reset(tab_width);

        for (unsigned int p = 0; p < t.size() - tab_width; ++p) {
                g_assert_cmpuint(t.get_next(p), ==, (p / tab_width + 1) * tab_width);
        }
        g_assert_cmpuint(t.get_next(t.size() - 1), ==, t.npos);
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/tabstops/default", test_tabstops_default);
        g_test_add_func("/vte/tabstops/get-set", test_tabstops_get_set);
        g_test_add_func("/vte/tabstops/clear", test_tabstops_clear);
        g_test_add_func("/vte/tabstops/reset", test_tabstops_reset);
        g_test_add_func("/vte/tabstops/resize", test_tabstops_resize);
        g_test_add_func("/vte/tabstops/previous", test_tabstops_previous);
        g_test_add_func("/vte/tabstops/next", test_tabstops_next);

        return g_test_run();
}
