/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008, 2010 Christian Persch
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

#ifndef TERMINAL_ENUMS_H
#define TERMINAL_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  TERMINAL_NEW_TERMINAL_MODE_WINDOW,
  TERMINAL_NEW_TERMINAL_MODE_TAB
} TerminalNewTerminalMode;

typedef enum {
  TERMINAL_NEW_TAB_POSITION_LAST,
  TERMINAL_NEW_TAB_POSITION_NEXT
} TerminalNewTabPosition;

typedef enum
{
  TERMINAL_EXIT_CLOSE,
  TERMINAL_EXIT_RESTART,
  TERMINAL_EXIT_HOLD
} TerminalExitAction;

typedef enum {
  TERMINAL_SETTINGS_LIST_FLAG_NONE = 0,
  TERMINAL_SETTINGS_LIST_FLAG_HAS_DEFAULT = 1 << 0,
  TERMINAL_SETTINGS_LIST_FLAG_ALLOW_EMPTY = 1 << 1
} TerminalSettingsListFlags;

typedef enum {
  TERMINAL_CJK_WIDTH_NARROW = 1,
  TERMINAL_CJK_WIDTH_WIDE   = 2
} TerminalCJKWidth;

typedef enum {
  TERMINAL_THEME_VARIANT_SYSTEM = 0,
  TERMINAL_THEME_VARIANT_LIGHT  = 1,
  TERMINAL_THEME_VARIANT_DARK   = 2
} TerminalThemeVariant;

typedef enum {
  TERMINAL_PRESERVE_WORKING_DIRECTORY_NEVER  = 0,
  TERMINAL_PRESERVE_WORKING_DIRECTORY_SAFE   = 1,
  TERMINAL_PRESERVE_WORKING_DIRECTORY_ALWAYS = 2,
} TerminalPreserveWorkingDirectory;

G_END_DECLS

#endif /* TERMINAL_ENUMS_H */
