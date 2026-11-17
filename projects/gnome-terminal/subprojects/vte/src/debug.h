/*
 * Copyright (C) 2002 Red Hat, Inc.
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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_debug_h_included
#define vte_debug_h_included

#include <config.h>
#include <stdint.h>
#include <glib.h>

#ifndef VTE_COMPILATION
#define _vte_debug_flags _vte_external_debug_flags
#define _vte_debug_init  _vte_external_debug_init
#define _vte_debug_on    _vte_external_debug_on
#if !defined(__GNUC__) || !G_HAVE_GNUC_VARARGS
#define _vte_debug_print _vte_external_debug_print
#endif
#endif

G_BEGIN_DECLS

typedef enum : unsigned {
        VTE_DEBUG_MISC          = 1u << 0,
        VTE_DEBUG_PARSER        = 1u << 1,
        VTE_DEBUG_IO            = 1u << 2,
        VTE_DEBUG_UPDATES       = 1u << 3,
        VTE_DEBUG_EVENTS        = 1u << 4,
        VTE_DEBUG_SIGNALS       = 1u << 5,
        VTE_DEBUG_SELECTION     = 1u << 6,
        VTE_DEBUG_SUBSTITUTION  = 1u << 7,
        VTE_DEBUG_RING          = 1u << 8,
        VTE_DEBUG_PTY           = 1u << 9,
        VTE_DEBUG_CURSOR        = 1u << 10,
        VTE_DEBUG_KEYBOARD      = 1u << 11,
        VTE_DEBUG_LIFECYCLE     = 1u << 12,
        VTE_DEBUG_WORK          = 1u << 13,
        VTE_DEBUG_CELLS         = 1u << 14,
        VTE_DEBUG_TIMEOUT       = 1u << 15,
        VTE_DEBUG_DRAW          = 1u << 16,
        VTE_DEBUG_ALLY          = 1u << 17,
        VTE_DEBUG_ADJ           = 1u << 18,
        VTE_DEBUG_PANGOCAIRO    = 1u << 19,
        VTE_DEBUG_WIDGET_SIZE   = 1u << 20,
        VTE_DEBUG_STYLE         = 1u << 21,
        VTE_DEBUG_RESIZE        = 1u << 22,
        VTE_DEBUG_REGEX         = 1u << 23,
        VTE_DEBUG_HYPERLINK     = 1u << 24,
        VTE_DEBUG_MODES         = 1u << 25,
        VTE_DEBUG_EMULATION     = 1u << 26,
        VTE_DEBUG_RINGVIEW      = 1u << 27,
        VTE_DEBUG_BIDI          = 1u << 28,
        VTE_DEBUG_CONVERSION    = 1u << 29,
        VTE_DEBUG_EXCEPTIONS    = 1u << 30,
} VteDebugFlags;

void _vte_debug_init(void);
const char *_vte_debug_sequence_to_string(const char *str,
                                          gssize length);

void _vte_debug_hexdump(char const* str,
                        uint8_t const* buf,
                        size_t len);

extern guint _vte_debug_flags;
static inline gboolean _vte_debug_on(guint flags) G_GNUC_CONST G_GNUC_UNUSED;

static inline gboolean
_vte_debug_on(guint flags)
{
	return (_vte_debug_flags & flags) != 0;
}

#if VTE_DEBUG
#define _VTE_DEBUG_IF(flags) if (G_UNLIKELY (_vte_debug_on (flags)))
#else
#define _VTE_DEBUG_IF(flags) if (0)
#endif

#if VTE_DEBUG
#if defined(__GNUC__) && G_HAVE_GNUC_VARARGS
#define _vte_debug_print(flags, fmt, ...) \
	G_STMT_START { _VTE_DEBUG_IF(flags) g_printerr(fmt, ##__VA_ARGS__); } G_STMT_END
#else
#include <stdarg.h>
static void _vte_debug_print(guint flags, const char *fmt, ...)
{
	_VTE_DEBUG_IF(flags) {
		va_list  ap;
		va_start (ap, fmt);
		g_vfprintf (stderr, fmt, ap);
		va_end (ap);
	}
}
#endif
#else
#define _vte_debug_print(args...) do { } while(0)
#endif /* VTE_DEBUG */

static inline char const*
_vte_debug_tf(bool v) noexcept
{
        return v ? "true" : "false";
}

#ifdef G_DISABLE_ASSERT
# define vte_assert_cmpint(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_cmpuint(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_cmphex(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_true(v) G_STMT_START {} G_STMT_END
# define vte_assert_false(v) G_STMT_START {} G_STMT_END
#else
# define vte_assert_cmpint(a,op,b) g_assert_cmpint(a,op,b)
# define vte_assert_cmpuint(a,op,b) g_assert_cmpuint(a,op,b)
# define vte_assert_cmphex(a,op,b) g_assert_cmphex(a,op,b)
# define vte_assert_true(v) g_assert_true(v)
# define vte_assert_false(v) g_assert_false(v)
#endif

G_END_DECLS

#endif
