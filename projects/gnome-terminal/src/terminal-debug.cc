/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib.h>

#include "terminal-debug.hh"

TerminalDebugFlags _terminal_debug_flags;

void
_terminal_debug_init(void)
{
#ifdef ENABLE_DEBUG
  const GDebugKey keys[] = {
    { "accels",        TERMINAL_DEBUG_ACCELS        },
    { "clipboard",     TERMINAL_DEBUG_CLIPBOARD     },
    { "encodings",     TERMINAL_DEBUG_ENCODINGS     },
    { "server",        TERMINAL_DEBUG_SERVER        },
    { "geometry",      TERMINAL_DEBUG_GEOMETRY      },
    { "mdi",           TERMINAL_DEBUG_MDI           },
    { "processes",     TERMINAL_DEBUG_PROCESSES     },
    { "profile",       TERMINAL_DEBUG_PROFILE       },
    { "settings-list", TERMINAL_DEBUG_SETTINGS_LIST },
    { "search",        TERMINAL_DEBUG_SEARCH        },
    { "bridge",        TERMINAL_DEBUG_BRIDGE        },
    { "default",       TERMINAL_DEBUG_DEFAULT       },
  };

  _terminal_debug_flags = TerminalDebugFlags(g_parse_debug_string (g_getenv ("GNOME_TERMINAL_DEBUG"),
								   keys, G_N_ELEMENTS (keys)));

#endif /* ENABLE_DEBUG */
}

