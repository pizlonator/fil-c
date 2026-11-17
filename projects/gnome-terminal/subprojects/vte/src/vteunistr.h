/*
 * Copyright (C) 2008 Red Hat, Inc.
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
 *
 * Author(s):
 * 	Behdad Esfahbod
 */

#ifndef vte_vteunistr_h_included
#define vte_vteunistr_h_included

#include <glib.h>

#include "bidiarrays.hh"

G_BEGIN_DECLS

#define VTE_UNISTR_START 0x80000000

/**
 * vteunistr:
 *
 * vteunistr is a gunichar-compatible way to store strings.  A string
 * consisting of a single unichar c is represented as the same value
 * as c itself.  In that sense, gunichars can be readily used as
 * vteunistrs.  Longer strings can be built by appending a unichar
 * to an already existing string.
 *
 * vteunistr is essentially just a gunicode-compatible quark value.
 * It can be used to store strings (of a base followed by combining
 * characters) where the code was designed to only allow one character.
 *
 * Strings are internalized efficiently and never freed.  No memory
 * management of vteunistr values is needed.
 **/
typedef guint32 vteunistr;

/**
 * _vte_unistr_append_unichar:
 * @s: a #vteunistr
 * @c: Unicode character to append to @s
 *
 * Creates a vteunistr value for the string @s followed by the
 * character @c.
 *
 * Returns: the new #vteunistr value
 **/
vteunistr
_vte_unistr_append_unichar (vteunistr s, gunichar c);

/**
 * _vte_unistr_append_unistr:
 * @s: a #vteunistr
 * @t: another #vteunistr to append to @s
 *
 * Creates a vteunistr value for the string @s followed by the
 * string @t.
 *
 * Returns: the new #vteunistr value
 **/
vteunistr
_vte_unistr_append_unistr (vteunistr s, vteunistr t);

gunichar
_vte_unistr_get_base (vteunistr s);

/**
 * _vte_unistr_replace_base:
 * @s: a #vteunistr
 * @c: Unicode character to replace the base character of @s.
 *
 * Creates a vteunistr value where the base character from @s is
 * replaced by @c, while the combining characters from @s are carried over.
 *
 * Returns: the new #vteunistr value
 */
vteunistr
_vte_unistr_replace_base (vteunistr s, gunichar c);

static inline int
_vte_g_string_append_unichar (GString *s, gunichar c)
{
        char outbuf[8];
        guint len = 0;
        int first;
        int i;

        if (c < 0x80) {
                first = 0;
                len = 1;
        }
        else if (c < 0x800) {
                first = 0xc0;
                len = 2;
        }
        else if (c < 0x10000) {
                first = 0xe0;
                len = 3;
        }
        else if (c < 0x200000) {
                first = 0xf0;
                len = 4;
        }
        else {
                g_assert_not_reached ();
        }

        for (i = len - 1; i > 0; --i) {
                outbuf[i] = (c & 0x3f) | 0x80;
                c >>= 6;
        }

        outbuf[0] = c | first;

        // GLib can do an inlined append()
        g_string_append_len (s, outbuf, len);

        return len;
}

/**
 * _vte_unistr_append_to_string:
 * @s: a #vteunistr
 * @gs: a #GString to append @s to
 *
 * Appends @s to @gs.  This is how one converts a #vteunistr to a
 * traditional string.
 **/
void
_vte_unistr_append_to_string (vteunistr s, GString *gs);
#define _vte_unistr_append_to_string(s,gs)                              \
        G_STMT_START {                                                  \
                if G_LIKELY (s < VTE_UNISTR_START)                      \
                        _vte_g_string_append_unichar (gs, (gunichar)s); \
                else                                                    \
                        (_vte_unistr_append_to_string) (s, gs);         \
        } G_STMT_END

/**
 * _vte_unistr_append_to_gunichars:
 * @s: a #vteunistr
 * @a: a #VteBidiChars of #gunichar items to append @s to
 *
 * Appends @s to @a.
 **/
void
_vte_unistr_append_to_gunichars (vteunistr s, VteBidiChars *a);

/**
 * _vte_unistr_strlen:
 * @s: a #vteunistr
 *
 * Counts the number of character in @s.
 *
 * Returns: length of @s in characters.
 **/
int
_vte_unistr_strlen (vteunistr s);
#define _vte_unistr_strlen(s) \
        ((s) < VTE_UNISTR_START ? 1 : (_vte_unistr_strlen)(s))

G_END_DECLS

#endif
