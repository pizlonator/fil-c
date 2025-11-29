/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "nine_quirk.h"

#include "util/u_debug.h"

static const struct debug_named_value nine_quirk_table[] = {
    { "fakecaps", QUIRK_FAKE_CAPS,
      "Fake caps to emulate D3D specs regardless of hardware caps." },
    { "lenientshader", QUIRK_LENIENT_SHADER,
      "Be lenient when translating shaders." },
    { "all", ~0U,
      "Enable all quirks." },
    DEBUG_NAMED_VALUE_END
};

bool
_nine_get_quirk( unsigned quirk )
{
    static bool first = true;
    static unsigned long flags = 0;

    if (first) {
        first = false;
        flags = debug_get_flags_option("NINE_QUIRKS", nine_quirk_table, 0);
    }

    return !!(flags & quirk);
}
