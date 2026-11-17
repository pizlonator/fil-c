/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2015 Christian Persch
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

/**
 * SECTION: vte-terminal
 * @short_description: A terminal widget implementation
 *
 * A VteTerminal is a terminal emulator implemented as a GTK3 widget.
 */

#pragma once

enum {
        SIGNAL_BELL,
        SIGNAL_CHAR_SIZE_CHANGED,
        SIGNAL_CHILD_EXITED,
        SIGNAL_COMMIT,
        SIGNAL_CONTENTS_CHANGED,
        SIGNAL_COPY_CLIPBOARD,
        SIGNAL_CURRENT_DIRECTORY_URI_CHANGED,
        SIGNAL_CURRENT_FILE_URI_CHANGED,
        SIGNAL_CURSOR_MOVED,
        SIGNAL_DECREASE_FONT_SIZE,
        SIGNAL_DEICONIFY_WINDOW,
        SIGNAL_ENCODING_CHANGED,
        SIGNAL_EOF,
        SIGNAL_HYPERLINK_HOVER_URI_CHANGED,
        SIGNAL_ICON_TITLE_CHANGED,
        SIGNAL_ICONIFY_WINDOW,
        SIGNAL_INCREASE_FONT_SIZE,
        SIGNAL_LOWER_WINDOW,
        SIGNAL_MAXIMIZE_WINDOW,
        SIGNAL_MOVE_WINDOW,
        SIGNAL_PASTE_CLIPBOARD,
        SIGNAL_RAISE_WINDOW,
        SIGNAL_REFRESH_WINDOW,
        SIGNAL_RESIZE_WINDOW,
        SIGNAL_RESTORE_WINDOW,
        SIGNAL_SELECTION_CHANGED,
        SIGNAL_SETUP_CONTEXT_MENU,
        SIGNAL_TERMPROPS_CHANGED,
        SIGNAL_TERMPROP_CHANGED,
        SIGNAL_WINDOW_TITLE_CHANGED,
        LAST_SIGNAL
};
extern guint signals[LAST_SIGNAL];

enum {
        PROP_0,
        PROP_ALLOW_BOLD,
        PROP_ALLOW_HYPERLINK,
        PROP_AUDIBLE_BELL,
        PROP_BACKSPACE_BINDING,
        PROP_BOLD_IS_BRIGHT,
        PROP_CELL_HEIGHT_SCALE,
        PROP_CELL_WIDTH_SCALE,
        PROP_CJK_AMBIGUOUS_WIDTH,
        PROP_CONTEXT_MENU_MODEL,
        PROP_CONTEXT_MENU,
        PROP_CURSOR_BLINK_MODE,
        PROP_CURSOR_SHAPE,
        PROP_CURRENT_DIRECTORY_URI,
        PROP_CURRENT_FILE_URI,
        PROP_DELETE_BINDING,
        PROP_ENABLE_A11Y,
        PROP_ENABLE_BIDI,
        PROP_ENABLE_FALLBACK_SCROLLING,
        PROP_ENABLE_LEGACY_OSC777,
        PROP_ENABLE_SHAPING,
        PROP_ENABLE_SIXEL,
        PROP_ENCODING,
        PROP_FONT_DESC,
        PROP_FONT_OPTIONS,
        PROP_FONT_SCALE,
        PROP_HYPERLINK_HOVER_URI,
        PROP_ICON_TITLE,
        PROP_INPUT_ENABLED,
        PROP_MOUSE_POINTER_AUTOHIDE,
        PROP_PTY,
        PROP_REWRAP_ON_RESIZE,
        PROP_SCROLLBACK_LINES,
        PROP_SCROLL_ON_INSERT,
        PROP_SCROLL_ON_KEYSTROKE,
        PROP_SCROLL_ON_OUTPUT,
        PROP_SCROLL_UNIT_IS_PIXELS,
        PROP_TEXT_BLINK_MODE,
        PROP_WINDOW_TITLE,
        PROP_WORD_CHAR_EXCEPTIONS,
        PROP_XALIGN,
        PROP_YALIGN,
        PROP_XFILL,
        PROP_YFILL,
        LAST_PROP,

        /* override properties */
        PROP_HADJUSTMENT,
        PROP_VADJUSTMENT,
        PROP_HSCROLL_POLICY,
        PROP_VSCROLL_POLICY
};
extern GParamSpec *pspecs[LAST_PROP];
