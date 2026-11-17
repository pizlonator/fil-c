/*
 * Copyright © 2023 Egmont Koblinger
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

#include <cstdint>

#include <glib.h>

#include "debug.h"
#include "unicode-width.hh"

static void
test_widths(void)
{
        gunichar c;

        // ASCII
        for (c = 0x20; c < 0x7F; c++) {
                g_assert_cmpint(_vte_unichar_width(c, 1), ==, 1);
                g_assert_cmpint(_vte_unichar_width(c, 2), ==, 1);
        }

        // Latin and more. Some are ambiguous width.
        for (c = 0xA0; c < 0x0300; c++) {
                g_assert_cmpint(_vte_unichar_width(c, 1), ==, 1);
        }
        g_assert_cmpint(_vte_unichar_width(0xA0, 2), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0xA1, 2), ==, 2);
        g_assert_cmpint(_vte_unichar_width(0xA2, 2), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0xA3, 2), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0xA4, 2), ==, 2);
        g_assert_cmpint(_vte_unichar_width(0xA5, 2), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0xA6, 2), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0xA7, 2), ==, 2);

        // Combining
        for (c = 0x0300; c < 0x0370; c++) {
                g_assert_cmpint(_vte_unichar_width(c, 1), ==, 0);
        }

        // Cyrillic, some historic symbols are combining
        g_assert_cmpint(_vte_unichar_width(0x0480, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x0481, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x0482, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x0483, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0484, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0485, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0486, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0487, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0488, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x0489, 1), ==, 0);
        g_assert_cmpint(_vte_unichar_width(0x048A, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x048B, 1), ==, 1);

        // Hangul
        for (c = 0x1100; c < 0x115F; c++) {
                g_assert_cmpint(_vte_unichar_width(c, 1), ==, 2);
        }

        // Miscellaneous Technical, mixture of single and double
        g_assert_cmpint(_vte_unichar_width(0x2318, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x2319, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x231A, 1), ==, 2);
        g_assert_cmpint(_vte_unichar_width(0x231B, 1), ==, 2);
        g_assert_cmpint(_vte_unichar_width(0x231C, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x231D, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x231E, 1), ==, 1);
        g_assert_cmpint(_vte_unichar_width(0x231F, 1), ==, 1);
}

int
main(int argc, char *argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/unicode-width/widths", test_widths);

        return g_test_run();
}
