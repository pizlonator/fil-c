/*
 * Copyright © 2024 Christian Persch
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

#define MINIFONT_TEST
#include "minifont-coverage.inc"
#include "minifont-coverage-tests.inc"

static void
test_minifont_coverage(void)
{
        for (auto i = 0u; i < G_N_ELEMENTS(minifont_coverage); ++i) {
                g_assert_true(unistr_is_local_graphic(minifont_coverage[i]));
        }

        //        for (uint32_t c = 0; c < 0x110000; ++c) {
        //        }
}

int
main(int argc, char *argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/minifont/coverage", test_minifont_coverage);

        return g_test_run();
}
