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

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glib.h>

#include "modes.hh"

static void
test_modes_ecma(void)
{
        vte::terminal::modes::ECMA modes{};

        g_assert_false(modes.IRM());
        g_assert_true(modes.BDSM());
        modes.set_IRM(true);
        g_assert_true(modes.IRM());
        g_assert_true(modes.BDSM());
        modes.set_BDSM(false);
        g_assert_true(modes.IRM());
        g_assert_false(modes.BDSM());

        vte::terminal::modes::ECMA copy{modes};
        g_assert_cmpuint(copy.get_modes(), ==, modes.get_modes());
        g_assert_cmpint(copy.IRM(), ==, modes.IRM());
        g_assert_cmpint(copy.BDSM(), ==, modes.BDSM());

        modes.reset();
        g_assert_false(modes.IRM());
        g_assert_true(modes.BDSM());
}

static void
test_modes_private(void)
{
        vte::terminal::modes::Private modes{};

        g_assert_true(modes.DEC_AUTOWRAP());
        g_assert_true(modes.XTERM_META_SENDS_ESCAPE());

        g_assert_false(modes.XTERM_FOCUS());
        modes.set_XTERM_FOCUS(true);
        g_assert_true(modes.XTERM_FOCUS());
        modes.push_saved(vte::terminal::modes::Private::eXTERM_FOCUS);
        modes.set_XTERM_FOCUS(false);
        g_assert_false(modes.XTERM_FOCUS());
        bool set = modes.pop_saved(vte::terminal::modes::Private::eXTERM_FOCUS);
        g_assert_true(set);
        modes.set_XTERM_FOCUS(set);
        g_assert_true(modes.XTERM_FOCUS());
        modes.push_saved(vte::terminal::modes::Private::eXTERM_FOCUS);
        modes.clear_saved();
        set = modes.pop_saved(vte::terminal::modes::Private::eXTERM_FOCUS);
        g_assert_false(set);
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/modes/ecma", test_modes_ecma);
        g_test_add_func("/vte/modes/private", test_modes_private);

        return g_test_run();
}
