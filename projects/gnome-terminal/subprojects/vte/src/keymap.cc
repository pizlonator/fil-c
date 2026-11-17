/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#include <cassert>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "caps.hh"
#include "debug.h"
#include "keymap.h"

#if VTE_DEBUG
static void
_vte_keysym_print(guint keyval,
                  guint modifiers)
{
	g_printerr("Mapping ");
	if (modifiers & GDK_CONTROL_MASK) {
		g_printerr("Control+");
	}
	if (modifiers & VTE_ALT_MASK) {
		g_printerr("Alt+");
	}
	if (modifiers & VTE_NUMLOCK_MASK) {
		g_printerr("NumLock+");
	}
	if (modifiers & GDK_SHIFT_MASK) {
		g_printerr("Shift+");
	}
	g_printerr("%s" , gdk_keyval_name(keyval));
}
#else
static void
_vte_keysym_print(guint keyval,
                  guint modifiers)
{
}
#endif

enum _vte_cursor_mode {
	cursor_default =	1u << 0,
	cursor_app =		1u << 1
};

enum _vte_keypad_mode {
	keypad_default =	1u << 0,
	keypad_app =		1u << 1
};

#define cursor_all	(cursor_default | cursor_app)
#define keypad_all	(keypad_default | keypad_app)

struct _vte_keymap_entry {
	guint cursor_mode;
	guint keypad_mode;
	guint mod_mask;
	const char normal[8];
        int8_t normal_length;
};

#define X_NULL ""

enum _vte_modifier_encoding_method {
	MODIFIER_ENCODING_NONE,
	MODIFIER_ENCODING_SHORT,
	MODIFIER_ENCODING_LONG,
};

static const struct _vte_keymap_entry _vte_keymap_GDK_space[] = {
	/* Control+Alt+space = ESC+NUL */
        {cursor_all, keypad_all, GDK_CONTROL_MASK | VTE_ALT_MASK, _VTE_CAP_ESC "\0", 2},
	/* Alt+space = ESC+" " */
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC " ", 2},
	/* Control+space = NUL */
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\0", 1},
	/* Regular space. */
        {cursor_all, keypad_all, 0, " ", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Tab[] = {
	/* Shift+Tab = Back-Tab */
        {cursor_all, keypad_all, GDK_SHIFT_MASK, _VTE_CAP_CSI "Z", -1},
	/* Alt+Tab = Esc+Tab */
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC "\t", -1},
	/* Regular tab. */
        {cursor_all, keypad_all, 0, "\t", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Return[] = {
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC "\r", 2},
        {cursor_all, keypad_all, 0, "\r", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Escape[] = {
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC _VTE_CAP_ESC, 2},
        {cursor_all, keypad_all, 0, _VTE_CAP_ESC, 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Insert[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "2~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_ISO_Left_Tab[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "Z", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_slash[] = {
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC "/", 2},
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, "/", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_question[] = {
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_ESC "?", 2},
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\177", 1},
        {cursor_all, keypad_all, 0, "?", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Various numeric keys enter control characters. */
static const struct _vte_keymap_entry _vte_keymap_GDK_2[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\0", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_3[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\033", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_4[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\034", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_5[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\035", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_6[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\036", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_7[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_8[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\177", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_Minus[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Keys (potentially) affected by the cursor key mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_Home[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "H", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "H", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_End[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "F", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "F", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Page_Up[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "5~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Page_Down[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "6~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Up[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "A", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "A", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Down[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "B", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "B", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Right[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "C", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "C", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Left[] = {
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "D", -1},
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "D", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Keys (potentially) affected by the keypad key mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Space[] = {
        {cursor_all, keypad_default, 0, " ", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 " ", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Tab[] = {
        {cursor_all, keypad_default, 0, "\t", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "I", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Enter[] = {
        {cursor_all, keypad_app, VTE_NUMLOCK_MASK, "\r", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "M", -1},
        {cursor_all, keypad_all, 0, "\r", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F1[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "P", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F2[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "Q", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F3[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "R", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F4[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "S", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Multiply[] = {
        {cursor_all, keypad_default, 0, "*", 1},
        {cursor_all, keypad_app, VTE_NUMLOCK_MASK, "*", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "j", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Add[] = {
        {cursor_all, keypad_default, 0, "+", 1},
        {cursor_all, keypad_app, VTE_NUMLOCK_MASK, "+", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "k", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Separator[] = {
        {cursor_all, keypad_default, 0, ",", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "l", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Subtract[] = {
        {cursor_all, keypad_default, 0, "-", 1},
        {cursor_all, keypad_app, VTE_NUMLOCK_MASK, "-", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "m", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Decimal_Delete[] = {
        {cursor_all, keypad_default, 0, ".", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "3~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Divide[] = {
        {cursor_all, keypad_default, 0, "/", 1},
        {cursor_all, keypad_app, VTE_NUMLOCK_MASK, "/", 1},
        {cursor_all, keypad_app, 0, _VTE_CAP_SS3 "o", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* GDK already separates keypad "0" from keypad "Insert", so the only time
 * we'll see this key is when NumLock is on. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_0[] = {
        {cursor_all, keypad_all, 0, "0", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_1[] = {
        {cursor_all, keypad_all, 0, "1", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_2[] = {
        {cursor_all, keypad_all, 0, "2", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_3[] = {
        {cursor_all, keypad_all, 0, "3", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_4[] = {
        {cursor_all, keypad_all, 0, "4", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_5[] = {
        {cursor_all, keypad_all, 0, "5", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_6[] = {
        {cursor_all, keypad_all, 0, "6", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_7[] = {
        {cursor_all, keypad_all, 0, "7", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_8[] = {
        {cursor_all, keypad_all, 0, "8", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_9[] = {
        {cursor_all, keypad_all, 0, "9", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* These are the same keys as above, but without numlock. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Insert[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "2~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_End[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "F", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "F", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Down[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "B", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "B", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Down[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "6~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Left[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "D", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "D", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Begin[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "E", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "E", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Right[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "C", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "C", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Home[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "H", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "H", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Up[] = {
        {cursor_app, keypad_all, 0, _VTE_CAP_SS3 "A", -1},
        {cursor_default, keypad_all, 0, _VTE_CAP_CSI "A", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Up[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "5~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F1[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, _VTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, GDK_SHIFT_MASK, _VTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "P", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F2[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, _VTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, GDK_SHIFT_MASK, _VTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "Q", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F3[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, _VTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, GDK_SHIFT_MASK, _VTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "R", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F4[] = {
        {cursor_all, keypad_all, GDK_CONTROL_MASK, _VTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, GDK_SHIFT_MASK, _VTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, VTE_ALT_MASK, _VTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, 0, _VTE_CAP_SS3 "S", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F5[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "15~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F6[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "17~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F7[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "18~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F8[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "19~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F9[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "20~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F10[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "21~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F11[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "23~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F12[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "24~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F13[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "25~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F14[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "26~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F15[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "28~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F16[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "29~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F17[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "31~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F18[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "32~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F19[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "33~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F20[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "34~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F21[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "42~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F22[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "43~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F23[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "44~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F24[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "45~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F25[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "46~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F26[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "47~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F27[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "48~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F28[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "49~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F29[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "50~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F30[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "51~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F31[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "52~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F32[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "53~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F33[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "54~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F34[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "55~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F35[] = {
        {cursor_all, keypad_all, 0, _VTE_CAP_CSI "56~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _vte_keymap_group {
	guint keyval;
	const struct _vte_keymap_entry *entries;
} _vte_keymap[] = {
	{GDK_KEY_space,			_vte_keymap_GDK_space},
	{GDK_KEY_Return,		_vte_keymap_GDK_Return},
	{GDK_KEY_Escape,		_vte_keymap_GDK_Escape},
	{GDK_KEY_Tab,			_vte_keymap_GDK_Tab},
	{GDK_KEY_ISO_Left_Tab,		_vte_keymap_GDK_ISO_Left_Tab},
	{GDK_KEY_Home,			_vte_keymap_GDK_Home},
	{GDK_KEY_End,			_vte_keymap_GDK_End},
	{GDK_KEY_Insert,		_vte_keymap_GDK_Insert},
	{GDK_KEY_slash,			_vte_keymap_GDK_slash},
	{GDK_KEY_question,		_vte_keymap_GDK_question},
	/* GDK_Delete is all handled in code, due to funkiness. */
	{GDK_KEY_Page_Up,		_vte_keymap_GDK_Page_Up},
	{GDK_KEY_Page_Down,		_vte_keymap_GDK_Page_Down},

	{GDK_KEY_2,			_vte_keymap_GDK_2},
	{GDK_KEY_3,			_vte_keymap_GDK_3},
	{GDK_KEY_4,			_vte_keymap_GDK_4},
	{GDK_KEY_5,			_vte_keymap_GDK_5},
	{GDK_KEY_6,			_vte_keymap_GDK_6},
	{GDK_KEY_7,			_vte_keymap_GDK_7},
	{GDK_KEY_8,			_vte_keymap_GDK_8},
	{GDK_KEY_minus,			_vte_keymap_GDK_Minus},

	{GDK_KEY_Up,			_vte_keymap_GDK_Up},
	{GDK_KEY_Down,			_vte_keymap_GDK_Down},
	{GDK_KEY_Right,			_vte_keymap_GDK_Right},
	{GDK_KEY_Left,			_vte_keymap_GDK_Left},

	{GDK_KEY_KP_Space,		_vte_keymap_GDK_KP_Space},
	{GDK_KEY_KP_Tab,		_vte_keymap_GDK_KP_Tab},
	{GDK_KEY_KP_Enter,		_vte_keymap_GDK_KP_Enter},
	{GDK_KEY_KP_F1,			_vte_keymap_GDK_KP_F1},
	{GDK_KEY_KP_F2,			_vte_keymap_GDK_KP_F2},
	{GDK_KEY_KP_F3,			_vte_keymap_GDK_KP_F3},
	{GDK_KEY_KP_F4,			_vte_keymap_GDK_KP_F4},
	{GDK_KEY_KP_Multiply,		_vte_keymap_GDK_KP_Multiply},
	{GDK_KEY_KP_Add,		_vte_keymap_GDK_KP_Add},
	{GDK_KEY_KP_Separator,		_vte_keymap_GDK_KP_Separator},
	{GDK_KEY_KP_Subtract,		_vte_keymap_GDK_KP_Subtract},
	{GDK_KEY_KP_Decimal,		_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KEY_KP_Delete,		_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KEY_KP_Divide,		_vte_keymap_GDK_KP_Divide},
	{GDK_KEY_KP_0,			_vte_keymap_GDK_KP_0},
	{GDK_KEY_KP_Insert,		_vte_keymap_GDK_KP_Insert},
	{GDK_KEY_KP_1,			_vte_keymap_GDK_KP_1},
	{GDK_KEY_KP_End,		_vte_keymap_GDK_KP_End},
	{GDK_KEY_KP_2,			_vte_keymap_GDK_KP_2},
	{GDK_KEY_KP_Down,		_vte_keymap_GDK_KP_Down},
	{GDK_KEY_KP_3,			_vte_keymap_GDK_KP_3},
	{GDK_KEY_KP_Page_Down,		_vte_keymap_GDK_KP_Page_Down},
	{GDK_KEY_KP_4,			_vte_keymap_GDK_KP_4},
	{GDK_KEY_KP_Left,		_vte_keymap_GDK_KP_Left},
	{GDK_KEY_KP_5,			_vte_keymap_GDK_KP_5},
	{GDK_KEY_KP_Begin,		_vte_keymap_GDK_KP_Begin},
	{GDK_KEY_KP_6,			_vte_keymap_GDK_KP_6},
	{GDK_KEY_KP_Right,		_vte_keymap_GDK_KP_Right},
	{GDK_KEY_KP_7,			_vte_keymap_GDK_KP_7},
	{GDK_KEY_KP_Home,		_vte_keymap_GDK_KP_Home},
	{GDK_KEY_KP_8,			_vte_keymap_GDK_KP_8},
	{GDK_KEY_KP_Up,			_vte_keymap_GDK_KP_Up},
	{GDK_KEY_KP_9,			_vte_keymap_GDK_KP_9},
	{GDK_KEY_KP_Page_Up,		_vte_keymap_GDK_KP_Page_Up},

	{GDK_KEY_F1,			_vte_keymap_GDK_F1},
	{GDK_KEY_F2,			_vte_keymap_GDK_F2},
	{GDK_KEY_F3,			_vte_keymap_GDK_F3},
	{GDK_KEY_F4,			_vte_keymap_GDK_F4},
	{GDK_KEY_F5,			_vte_keymap_GDK_F5},
	{GDK_KEY_F6,			_vte_keymap_GDK_F6},
	{GDK_KEY_F7,			_vte_keymap_GDK_F7},
	{GDK_KEY_F8,			_vte_keymap_GDK_F8},
	{GDK_KEY_F9,			_vte_keymap_GDK_F9},
	{GDK_KEY_F10,			_vte_keymap_GDK_F10},
	{GDK_KEY_F11,			_vte_keymap_GDK_F11},
	{GDK_KEY_F12,			_vte_keymap_GDK_F12},
	{GDK_KEY_F13,			_vte_keymap_GDK_F13},
	{GDK_KEY_F14,			_vte_keymap_GDK_F14},
	{GDK_KEY_F15,			_vte_keymap_GDK_F15},
	{GDK_KEY_F16,			_vte_keymap_GDK_F16},
	{GDK_KEY_F17,			_vte_keymap_GDK_F17},
	{GDK_KEY_F18,			_vte_keymap_GDK_F18},
	{GDK_KEY_F19,			_vte_keymap_GDK_F19},
	{GDK_KEY_F20,			_vte_keymap_GDK_F20},
	{GDK_KEY_F21,			_vte_keymap_GDK_F21},
	{GDK_KEY_F22,			_vte_keymap_GDK_F22},
	{GDK_KEY_F23,			_vte_keymap_GDK_F23},
	{GDK_KEY_F24,			_vte_keymap_GDK_F24},
	{GDK_KEY_F25,			_vte_keymap_GDK_F25},
	{GDK_KEY_F26,			_vte_keymap_GDK_F26},
	{GDK_KEY_F27,			_vte_keymap_GDK_F27},
	{GDK_KEY_F28,			_vte_keymap_GDK_F28},
	{GDK_KEY_F29,			_vte_keymap_GDK_F29},
	{GDK_KEY_F30,			_vte_keymap_GDK_F30},
	{GDK_KEY_F31,			_vte_keymap_GDK_F31},
	{GDK_KEY_F32,			_vte_keymap_GDK_F32},
	{GDK_KEY_F33,			_vte_keymap_GDK_F33},
	{GDK_KEY_F34,			_vte_keymap_GDK_F34},
	{GDK_KEY_F35,			_vte_keymap_GDK_F35},
};

/* Map the specified keyval/modifier setup, dependent on the mode, to
 * a literal string. */
void
_vte_keymap_map(guint keyval,
		guint modifiers,
		gboolean app_cursor_keys,
		gboolean app_keypad_keys,
		char **normal,
		gsize *normal_length)
{
	gsize i;
	const struct _vte_keymap_entry *entries;
	enum _vte_cursor_mode cursor_mode;
	enum _vte_keypad_mode keypad_mode;

	g_return_if_fail(normal != NULL);
	g_return_if_fail(normal_length != NULL);

	_VTE_DEBUG_IF(VTE_DEBUG_KEYBOARD) 
		_vte_keysym_print(keyval, modifiers);

	/* Start from scratch. */
	*normal = NULL;
	*normal_length = 0;

	/* Search for the list for this key. */
	entries = NULL;
	for (i = 0; i < G_N_ELEMENTS(_vte_keymap); i++) {
		if (_vte_keymap[i].keyval == keyval) {
			/* Found it! */
			entries = _vte_keymap[i].entries;
			break;
		}
	}
	if (entries == NULL) {
		_vte_debug_print(VTE_DEBUG_KEYBOARD,
				" (ignoring, no map for key).\n");
		return;
	}

	/* Build mode masks. */
	cursor_mode = app_cursor_keys ? cursor_app : cursor_default;
	keypad_mode = app_keypad_keys ? keypad_app : keypad_default;
	modifiers &= GDK_SHIFT_MASK | GDK_CONTROL_MASK | VTE_ALT_MASK | VTE_NUMLOCK_MASK;

	/* Search for the conditions. */
	for (i = 0; entries[i].normal_length; i++)
	if ((entries[i].cursor_mode & cursor_mode) &&
	    (entries[i].keypad_mode & keypad_mode))
	if ((modifiers & entries[i].mod_mask) == entries[i].mod_mask) {
                if (entries[i].normal_length != -1) {
                        *normal_length = entries[i].normal_length;
                        *normal = (char*)g_memdup(entries[i].normal,
                                                  entries[i].normal_length);
                } else {
                        *normal_length = strlen(entries[i].normal);
                        *normal = g_strdup(entries[i].normal);
                }
                _vte_keymap_key_add_key_modifiers(keyval,
                                                  modifiers,
                                                  cursor_mode & cursor_app,
                                                  normal,
                                                  normal_length);
                _vte_debug_print(VTE_DEBUG_KEYBOARD,
                                 " to '%s'.\n",
                                 _vte_debug_sequence_to_string(*normal, *normal_length));
                return;
	}

	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			" (ignoring, no match for modifier state).\n");
}

gboolean
_vte_keymap_key_is_modifier(guint keyval)
{
	gboolean modifier = FALSE;
	/* Determine if this is just a modifier key. */
	switch (keyval) {
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Caps_Lock:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
	case GDK_KEY_Eisu_Shift:
	case GDK_KEY_Hyper_L:
	case GDK_KEY_Hyper_R:
	case GDK_KEY_ISO_First_Group:
	case GDK_KEY_ISO_First_Group_Lock:
	case GDK_KEY_ISO_Group_Latch:
	case GDK_KEY_ISO_Group_Lock:
	case GDK_KEY_ISO_Group_Shift:
	case GDK_KEY_ISO_Last_Group:
	case GDK_KEY_ISO_Last_Group_Lock:
	case GDK_KEY_ISO_Level2_Latch:
	case GDK_KEY_ISO_Level3_Latch:
	case GDK_KEY_ISO_Level3_Lock:
	case GDK_KEY_ISO_Level3_Shift:
	case GDK_KEY_ISO_Level5_Latch:
	case GDK_KEY_ISO_Level5_Lock:
	case GDK_KEY_ISO_Level5_Shift:
	case GDK_KEY_ISO_Lock:
	case GDK_KEY_ISO_Next_Group:
	case GDK_KEY_ISO_Next_Group_Lock:
	case GDK_KEY_ISO_Prev_Group:
	case GDK_KEY_ISO_Prev_Group_Lock:
	case GDK_KEY_Kana_Lock:
	case GDK_KEY_Kana_Shift:
	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
        case GDK_KEY_ModeLock:
	case GDK_KEY_Num_Lock:
	case GDK_KEY_Scroll_Lock:
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_Lock:
	case GDK_KEY_Shift_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
		modifier = TRUE;
		break;
	default:
		modifier = FALSE;
		break;
	}
	return modifier;
}

static enum _vte_modifier_encoding_method
_vte_keymap_key_get_modifier_encoding_method(guint keyval)
{
	enum _vte_modifier_encoding_method method = MODIFIER_ENCODING_NONE;
	/* Determine if this key gets modifiers. */
	switch (keyval) {
	case GDK_KEY_Up:
	case GDK_KEY_Down:
	case GDK_KEY_Left:
	case GDK_KEY_Right:
	case GDK_KEY_Insert:
	case GDK_KEY_Delete:
	case GDK_KEY_Home:
	case GDK_KEY_End:
	case GDK_KEY_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Up:
	case GDK_KEY_KP_Down:
	case GDK_KEY_KP_Left:
	case GDK_KEY_KP_Right:
	case GDK_KEY_KP_Insert:
	case GDK_KEY_KP_Delete:
	case GDK_KEY_KP_Home:
	case GDK_KEY_KP_End:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_KP_Page_Down:
	case GDK_KEY_KP_Begin:
	case GDK_KEY_F1:
	case GDK_KEY_F2:
	case GDK_KEY_F3:
	case GDK_KEY_F4:
	case GDK_KEY_F5:
	case GDK_KEY_F6:
	case GDK_KEY_F7:
	case GDK_KEY_F8:
	case GDK_KEY_F9:
	case GDK_KEY_F10:
	case GDK_KEY_F11:
	case GDK_KEY_F12:
	case GDK_KEY_F13:
	case GDK_KEY_F14:
	case GDK_KEY_F15:
	case GDK_KEY_F16:
	case GDK_KEY_F17:
	case GDK_KEY_F18:
	case GDK_KEY_F19:
	case GDK_KEY_F20:
	case GDK_KEY_F21:
	case GDK_KEY_F22:
	case GDK_KEY_F23:
	case GDK_KEY_F24:
	case GDK_KEY_F25:
	case GDK_KEY_F26:
	case GDK_KEY_F27:
	case GDK_KEY_F28:
	case GDK_KEY_F29:
	case GDK_KEY_F30:
	case GDK_KEY_F31:
	case GDK_KEY_F32:
	case GDK_KEY_F33:
	case GDK_KEY_F34:
	case GDK_KEY_F35:
		method = MODIFIER_ENCODING_LONG;
		break;
	case GDK_KEY_KP_Divide:
	case GDK_KEY_KP_Multiply:
	case GDK_KEY_KP_Subtract:
	case GDK_KEY_KP_Add:
	case GDK_KEY_KP_Enter:
		method = MODIFIER_ENCODING_SHORT;
		break;
	default:
		method = MODIFIER_ENCODING_NONE;
		break;
	}
	return method;
}

/* Prior and Next are ommitted for the SS3 to CSI switch below */
static gboolean
is_cursor_key(guint keyval)
{
	switch (keyval) {
	case GDK_KEY_Home:
	case GDK_KEY_Left:
	case GDK_KEY_Up:
	case GDK_KEY_Right:
	case GDK_KEY_Down:
	case GDK_KEY_End:
        case GDK_KEY_Begin:

	case GDK_KEY_KP_Home:
	case GDK_KEY_KP_Left:
	case GDK_KEY_KP_Up:
	case GDK_KEY_KP_Right:
	case GDK_KEY_KP_Down:
	case GDK_KEY_KP_End:
	case GDK_KEY_KP_Begin:
		return TRUE;
	default:
		return FALSE;
	}
}


void
_vte_keymap_key_add_key_modifiers(guint keyval,
				  guint modifiers,
				  gboolean cursor_app_mode,
				  char **normal,
				  gsize *normal_length)
{
	int modifier, offset;
	char *nnormal;
	enum _vte_modifier_encoding_method modifier_encoding_method;
	guint significant_modifiers;

	significant_modifiers = GDK_SHIFT_MASK |
				GDK_CONTROL_MASK |
				VTE_ALT_MASK;

	modifier_encoding_method = _vte_keymap_key_get_modifier_encoding_method(keyval);
	if (modifier_encoding_method == MODIFIER_ENCODING_NONE) {
		return;
	}

	switch (modifiers & significant_modifiers) {
	case 0:
		modifier = 0;
		break;
	case GDK_SHIFT_MASK:
		modifier = 2;
		break;
	case VTE_ALT_MASK:
		modifier = 3;
		break;
	case GDK_SHIFT_MASK | VTE_ALT_MASK:
		modifier = 4;
		break;
	case GDK_CONTROL_MASK:
		modifier = 5;
		break;
	case GDK_SHIFT_MASK | GDK_CONTROL_MASK:
		modifier = 6;
		break;
	case VTE_ALT_MASK | GDK_CONTROL_MASK:
		modifier = 7;
		break;
	case GDK_SHIFT_MASK | VTE_ALT_MASK | GDK_CONTROL_MASK:
		modifier = 8;
		break;
	default:
		modifier = 8;
		break;
	}

	if (modifier == 0) {
		return;
	}

	nnormal = g_new0(char, *normal_length + 4);
	memcpy(nnormal, *normal, *normal_length);
	if (strlen(nnormal) > 1) {
		/* SS3 should have no modifiers so make it CSI instead. See
		 * http://cvsweb.xfree86.org/cvsweb/xc/programs/xterm/input.c.diff?r1=3.57&r2=3.58
		 */
		if (cursor_app_mode &&
			g_str_has_prefix(nnormal, _VTE_CAP_SS3)
			&& is_cursor_key(keyval)) {
			nnormal[1] = '[';
		}

		/* Get the offset of the last character. */
		offset = strlen(nnormal) - 1;
		if (g_ascii_isdigit(nnormal[offset - 1])) {
			/* Stuff a semicolon and the modifier in right before
			 * that last character. */
			nnormal[offset + 2] = nnormal[offset];
			nnormal[offset + 1] = modifier + '0';
			nnormal[offset + 0] = ';';
			*normal_length += 2;
		} else if (modifier_encoding_method == MODIFIER_ENCODING_LONG) {
			/* Stuff a "1", a semicolon and the modifier in right
			 * before that last character, matching Xterm most of the time. */
			nnormal[offset + 3] = nnormal[offset];
			nnormal[offset + 2] = modifier + '0';
			nnormal[offset + 1] = ';';
			nnormal[offset + 0] = '1';
			*normal_length += 3;
		} else {
			/* Stuff the modifier in right before that last
			 * character, matching what people expect,
			 * and what Xterm does with numpad math operators */
			nnormal[offset + 1] = nnormal[offset];
			nnormal[offset + 0] = modifier + '0';
			*normal_length += 1;
		}
		g_free(*normal);
		*normal = nnormal;
	} else {
		g_free(nnormal);
	}
}
