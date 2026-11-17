/*
 * Copyright (C) 2002 Red Hat, Inc.
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

/* The interfaces in this file are subject to change at any time. */

#ifndef ENABLE_DEBUG_H
#define ENABLE_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  TERMINAL_DEBUG_ACCELS        = 1 << 0,
  TERMINAL_DEBUG_CLIPBOARD     = 1 << 1,
  TERMINAL_DEBUG_ENCODINGS     = 1 << 2,
  TERMINAL_DEBUG_SERVER        = 1 << 3,
  TERMINAL_DEBUG_GEOMETRY      = 1 << 4,
  TERMINAL_DEBUG_MDI           = 1 << 5,
  TERMINAL_DEBUG_PROCESSES     = 1 << 6,
  TERMINAL_DEBUG_PROFILE       = 1 << 7,
  TERMINAL_DEBUG_SETTINGS_LIST = 1 << 8,
  TERMINAL_DEBUG_SEARCH        = 1 << 9,
  TERMINAL_DEBUG_BRIDGE        = 1 << 10,
  TERMINAL_DEBUG_DEFAULT       = 1 << 11,
} TerminalDebugFlags;

void _terminal_debug_init(void);

extern TerminalDebugFlags _terminal_debug_flags;
static inline gboolean _terminal_debug_on (TerminalDebugFlags flags) G_GNUC_CONST G_GNUC_UNUSED;

static inline gboolean
_terminal_debug_on (TerminalDebugFlags flags)
{
  return (_terminal_debug_flags & flags) == flags;
}

#ifdef ENABLE_DEBUG
#define _TERMINAL_DEBUG_IF(flags) if (G_UNLIKELY (_terminal_debug_on (flags)))
#else
#define _TERMINAL_DEBUG_IF(flags) if (0)
#endif

#if defined(__GNUC__) && G_HAVE_GNUC_VARARGS
#define _terminal_debug_print(flags, fmt, ...) \
  G_STMT_START { _TERMINAL_DEBUG_IF(flags) g_printerr(fmt, ##__VA_ARGS__); } G_STMT_END
#else
#include <stdarg.h>
#include <glib/gstdio.h>
static void _terminal_debug_print (guint flags, const char *fmt, ...)
{
  if (_terminal_debug_on (flags)) {
    va_list  ap;
    va_start (ap, fmt);
    g_vfprintf (stderr, fmt, ap);
    va_end (ap);
  }
}
#endif

G_END_DECLS

#endif /* !ENABLE_DEBUG_H */
