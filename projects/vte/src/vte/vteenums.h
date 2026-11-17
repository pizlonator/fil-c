/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * VteCursorBlinkMode:
 * @VTE_CURSOR_BLINK_SYSTEM: Follow GTK+ settings for cursor blinking.
 * @VTE_CURSOR_BLINK_ON: Cursor blinks.
 * @VTE_CURSOR_BLINK_OFF: Cursor does not blink.
 *
 * An enumerated type which can be used to indicate the cursor blink mode
 * for the terminal.
 */
typedef enum {
        VTE_CURSOR_BLINK_SYSTEM,
        VTE_CURSOR_BLINK_ON,
        VTE_CURSOR_BLINK_OFF
} VteCursorBlinkMode;

/**
 * VteCursorShape:
 * @VTE_CURSOR_SHAPE_BLOCK: Draw a block cursor.  This is the default.
 * @VTE_CURSOR_SHAPE_IBEAM: Draw a vertical bar on the left side of character.
 * This is similar to the default cursor for other GTK+ widgets.
 * @VTE_CURSOR_SHAPE_UNDERLINE: Draw a horizontal bar below the character.
 *
 * An enumerated type which can be used to indicate what should the terminal
 * draw at the cursor position.
 */
typedef enum {
        VTE_CURSOR_SHAPE_BLOCK,
        VTE_CURSOR_SHAPE_IBEAM,
        VTE_CURSOR_SHAPE_UNDERLINE
} VteCursorShape;

/**
 * VteTextBlinkMode:
 * @VTE_TEXT_BLINK_NEVER: Do not blink the text.
 * @VTE_TEXT_BLINK_FOCUSED: Allow blinking text only if the terminal is focused.
 * @VTE_TEXT_BLINK_UNFOCUSED: Allow blinking text only if the terminal is unfocused.
 * @VTE_TEXT_BLINK_ALWAYS: Allow blinking text. This is the default.
 *
 * An enumerated type which can be used to indicate whether the terminal allows
 * the text contents to be blinked.
 *
 * Since: 0.52
 */
typedef enum {
        VTE_TEXT_BLINK_NEVER     = 0,
        VTE_TEXT_BLINK_FOCUSED   = 1,
        VTE_TEXT_BLINK_UNFOCUSED = 2,
        VTE_TEXT_BLINK_ALWAYS    = 3
} VteTextBlinkMode;

/**
 * VteEraseBinding:
 * @VTE_ERASE_AUTO: For backspace, attempt to determine the right value from the terminal's IO settings.  For delete, use the control sequence.
 * @VTE_ERASE_ASCII_BACKSPACE: Send an ASCII backspace character (0x08).
 * @VTE_ERASE_ASCII_DELETE: Send an ASCII delete character (0x7F).
 * @VTE_ERASE_DELETE_SEQUENCE: Send the "@@7" control sequence.
 * @VTE_ERASE_TTY: Send terminal's "erase" setting.
 *
 * An enumerated type which can be used to indicate which string the terminal
 * should send to an application when the user presses the Delete or Backspace
 * keys.
 */
typedef enum {
	VTE_ERASE_AUTO,
	VTE_ERASE_ASCII_BACKSPACE,
	VTE_ERASE_ASCII_DELETE,
	VTE_ERASE_DELETE_SEQUENCE,
	VTE_ERASE_TTY
} VteEraseBinding;

/**
 * VtePtyError:
 * @VTE_PTY_ERROR_PTY_HELPER_FAILED: Obsolete. Deprecated: 0.42
 * @VTE_PTY_ERROR_PTY98_FAILED: failure when using PTY98 to allocate the PTY
 */
typedef enum {
  VTE_PTY_ERROR_PTY_HELPER_FAILED = 0,
  VTE_PTY_ERROR_PTY98_FAILED
} VtePtyError;

/**
 * VtePtyFlags:
 * @VTE_PTY_NO_LASTLOG: Unused. Deprecated: 0.38
 * @VTE_PTY_NO_UTMP: Unused. Deprecated: 0.38
 * @VTE_PTY_NO_WTMP: Unused. Deprecated: 0.38
 * @VTE_PTY_NO_HELPER: Unused. Deprecated: 0.38
 * @VTE_PTY_NO_FALLBACK: Unused. Deprecated: 0.38
 * @VTE_PTY_NO_SESSION: Do not start a new session for the child in
 *   vte_pty_child_setup(). See man:setsid(2) for more information. Since: 0.58
 * @VTE_PTY_NO_CTTY: Do not set the PTY as the controlling TTY for the child
 *   in vte_pty_child_setup(). See man:tty_ioctl(4) for more information. Since: 0.58
 * @VTE_PTY_DEFAULT: the default flags
 */
typedef enum {
  VTE_PTY_NO_LASTLOG  = 1 << 0,
  VTE_PTY_NO_UTMP     = 1 << 1,
  VTE_PTY_NO_WTMP     = 1 << 2,
  VTE_PTY_NO_HELPER   = 1 << 3,
  VTE_PTY_NO_FALLBACK = 1 << 4,
  VTE_PTY_NO_SESSION  = 1 << 5,
  VTE_PTY_NO_CTTY     = 1 << 6,
  VTE_PTY_DEFAULT     = 0
} VtePtyFlags;

/**
 * VteWriteFlags:
 * @VTE_WRITE_DEFAULT: Write contents as UTF-8 text.  This is the default.
 *
 * A flag type to determine how terminal contents should be written
 * to an output stream.
 */
typedef enum {
  VTE_WRITE_DEFAULT = 0
} VteWriteFlags;

/**
 * VteRegexError:
 * @VTE_REGEX_ERROR_INCOMPATIBLE: The PCRE2 library was built without
 *   Unicode support which is required for VTE
 * @VTE_REGEX_ERROR_NOT_SUPPORTED: Regexes are not supported because VTE was
 *   built without PCRE2 support
 *
 * An enum type for regex errors. In addition to the values listed above,
 * any PCRE2 error values may occur.
 *
 * Since: 0.46
 */
typedef enum {
        /* Negative values are PCRE2 errors */

        /* VTE specific values */
        VTE_REGEX_ERROR_INCOMPATIBLE  = G_MAXINT-1,
        VTE_REGEX_ERROR_NOT_SUPPORTED = G_MAXINT
} VteRegexError;

/**
 * VteFormat:
 * @VTE_FORMAT_TEXT: Export as plain text
 * @VTE_FORMAT_HTML: Export as HTML formatted text
 *
 * An enumeration type that can be used to specify the format the selection
 * should be copied to the clipboard in.
 *
 * Since: 0.50
 */
typedef enum {
        VTE_FORMAT_TEXT = 1,
        VTE_FORMAT_HTML = 2
} VteFormat;

/**
 * VteFeatureFlags:
 * @VTE_FEATURE_FLAG_BIDI: whether VTE was built with bidirectional text support
 * @VTE_FEATURE_FLAG_ICU: whether VTE was built with ICU support
 * @VTE_FEATURE_FLAG_SYSTEMD: whether VTE was built with systemd support
 * @VTE_FEATURE_FLAG_SIXEL: whether VTE was built with SIXEL support
 * @VTE_FEATURE_FLAGS_MASK: mask of all feature flags
 *
 * An enumeration type for features.
 *
 * Since: 0.62
 */
typedef enum /*< skip >*/ {
        VTE_FEATURE_FLAG_BIDI    = 1ULL << 0,
        VTE_FEATURE_FLAG_ICU     = 1ULL << 1,
        VTE_FEATURE_FLAG_SYSTEMD = 1ULL << 2,
        VTE_FEATURE_FLAG_SIXEL   = 1ULL << 3,

        VTE_FEATURE_FLAGS_MASK   = 0xFFFFFFFFFFFFFFFFULL, /* force enum to 64 bit */
} VteFeatureFlags;


/**
 * VteAlign:
 * @VTE_ALIGN_START: align to left/top
 * @VTE_ALIGN_CENTER: align to centre
 * @VTE_ALIGN_END: align to right/bottom
 *
 * An enumeration type that can be used to specify how the terminal
 * uses extra allocated space.
 *
 * Since: 0.76
 */
typedef enum {
        VTE_ALIGN_START       = 0U,
        VTE_ALIGN_CENTER      = 1U,
        VTE_ALIGN_END         = 2U,
} VteAlign;

/*
 * VteUuidFormat:
 * @VTE_UUID_FORMAT_SIMPLE: simple format
 * @VTE_UUID_FORMAT_BRACED: braced format
 * @VTE_UUID_FORMAT_URN: urn format
 * @VTE_UUID_FORMAT_ANY: any format of the above
 *
 * An enumeration that specifies the format of an UUID.
 *
 * Since: 0.78
 */
typedef enum /*< flags >*/ {
        VTE_UUID_FORMAT_SIMPLE = 1u << 0,
        VTE_UUID_FORMAT_BRACED = 1u << 1,
        VTE_UUID_FORMAT_URN = 1u << 2,
        VTE_UUID_FORMAT_ANY = 0x7u,
} VteUuidFormat;

/**
 * VtePropertyFlags:
 * @VTE_PROPERTY_FLAG_NONE: no flags, default
 * @VTE_PROPERTY_FLAG_EPHEMERAL: denotes an ephemeral termprop
 *
 * A flags type.
 *
 * Since: 0.78
 */
typedef enum /*< flags >*/ {
        VTE_PROPERTY_FLAG_NONE = 0u,
        VTE_PROPERTY_FLAG_EPHEMERAL = 1u << 0,
} VtePropertyFlags;

/**
 * VtePropertyType:
 * @VTE_PROPERTY_VALUELESS: no value, use for signalling
 * @VTE_PROPERTY_BOOL: a bool
 * @VTE_PROPERTY_INT: a signed 64-bit integer
 * @VTE_PROPERTY_UINT: an unsigned 64-bit integer
 * @VTE_PROPERTY_DOUBLE: a finite double-precision floating point number
 * @VTE_PROPERTY_RGB: a color
 * @VTE_PROPERTY_RGBA: a color with alpha
 * @VTE_PROPERTY_STRING: a string
 * @VTE_PROPERTY_DATA: binary data
 * @VTE_PROPERTY_UUID: a UUID
 * @VTE_PROPERTY_URI: a URI
 *
 * An enumeration type describing types of properties.
 *
 * Since: 0.78
 */
typedef enum {
        VTE_PROPERTY_INVALID = -1, /*< skip >*/
        VTE_PROPERTY_VALUELESS = 0,
        VTE_PROPERTY_BOOL,
        VTE_PROPERTY_INT,
        VTE_PROPERTY_UINT,
        VTE_PROPERTY_DOUBLE,
        VTE_PROPERTY_RGB,
        VTE_PROPERTY_RGBA,
        VTE_PROPERTY_STRING,
        VTE_PROPERTY_DATA,
        VTE_PROPERTY_UUID,
        VTE_PROPERTY_URI,
} VtePropertyType;

/**
 * VtePropertyId:
 * @VTE_PROPERTY_ID_CURRENT_DIRECTORY_URI: the ID of the %VTE_TERMPROP_CURRENT_DIRECTORY_URI termprop
 * @VTE_PROPERTY_ID_CURRENT_FILE_URI: the ID of the %VTE_TERMPROP_CURRENT_FILE_URI termprop
 * @VTE_PROPERTY_ID_XTERM_TITLE: the ID of the %VTE_TERMPROP_XTERM_TITLE termprop
 * @VTE_PROPERTY_ID_CONTAINER_NAME: the ID of the %VTE_TERMPROP_CONTAINER_NAME termprop
 * @VTE_PROPERTY_ID_CONTAINER_RUNTIME: the ID of the %VTE_TERMPROP_CONTAINER_RUNTIME termprop
 * @VTE_PROPERTY_ID_CONTAINER_UID: the ID of the %VTE_TERMPROP_CONTAINER_UID termprop
 * @VTE_PROPERTY_ID_SHELL_PRECMD: the ID of the %VTE_TERMPROP_SHELL_PRECMD termprop
 * @VTE_PROPERTY_ID_SHELL_PREEXEC: the ID of the %VTE_TERMPROP_SHELL_PREEXEC termprop
 * @VTE_PROPERTY_ID_SHELL_POSTEXEC: the ID of the %VTE_TERMPROP_SHELL_POSTEXEC termprop
 *
 * An enum containing the IDs of the always-installed termprops.
 *
 * Since: 0.78
 */
typedef enum {
        VTE_PROPERTY_ID_CURRENT_DIRECTORY_URI = 0,
        VTE_PROPERTY_ID_CURRENT_FILE_URI,
        VTE_PROPERTY_ID_XTERM_TITLE,
        VTE_PROPERTY_ID_CONTAINER_NAME,
        VTE_PROPERTY_ID_CONTAINER_RUNTIME,
        VTE_PROPERTY_ID_CONTAINER_UID,
        VTE_PROPERTY_ID_SHELL_PRECMD,
        VTE_PROPERTY_ID_SHELL_PREEXEC,
        VTE_PROPERTY_ID_SHELL_POSTEXEC,
	_VTE_PROPERTY_ID_MAX = 0x7ffffff, /*< skip >*/
} VtePropertyId;

G_END_DECLS
