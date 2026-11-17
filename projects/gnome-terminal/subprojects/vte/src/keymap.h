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

#ifndef vte_keymap_h_included
#define vte_keymap_h_included

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#if VTE_GTK == 3
#define VTE_ALT_MASK		GDK_MOD1_MASK
#define VTE_NUMLOCK_MASK	GDK_MOD2_MASK
#elif VTE_GTK == 4
#define VTE_ALT_MASK		GDK_ALT_MASK
#define VTE_NUMLOCK_MASK	0 /* FIXME */
#endif

/* Map the specified keyval/modifier setup, dependent on the mode, to either
 * a literal string or a capability name. */
void _vte_keymap_map(guint keyval,
		     guint modifiers,
		     gboolean app_cursor_keys,
		     gboolean app_keypad_keys,
		     char **normal,
		     gsize *normal_length);

/* Return TRUE if a keyval is just a modifier key. */
gboolean _vte_keymap_key_is_modifier(guint keyval);

/* Add modifiers to the sequence if they're needed. */
void _vte_keymap_key_add_key_modifiers(guint keyval,
				       guint modifiers,
				       gboolean app_cursor_keys,
				       char **normal,
				       gsize *normal_length);

G_END_DECLS

#endif
