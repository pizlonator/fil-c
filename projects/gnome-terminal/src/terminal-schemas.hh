/*
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

#ifndef TERMINAL_SCHEMAS_H
#define TERMINAL_SCHEMAS_H

#include <glib.h>

G_BEGIN_DECLS

#define TERMINAL_SCHEMA_VERSION         (3u)

#define TERMINAL_KEYBINDINGS_SCHEMA     "org.gnome.Terminal.Legacy.Keybindings"
#define TERMINAL_PROFILE_SCHEMA         "org.gnome.Terminal.Legacy.Profile"
#define TERMINAL_SETTING_SCHEMA         "org.gnome.Terminal.Legacy.Settings"
#define TERMINAL_SETTINGS_LIST_SCHEMA   "org.gnome.Terminal.SettingsList"
#define TERMINAL_PROFILES_LIST_SCHEMA   "org.gnome.Terminal.ProfilesList"

#define TERMINAL_KEYBINDINGS_SCHEMA_PATH "/org/gnome/terminal/legacy/keybindings/"

#define TERMINAL_PROFILE_AUDIBLE_BELL_KEY               "audible-bell"
#define TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY             "bold-is-bright"
#define TERMINAL_PROFILE_BACKGROUND_COLOR_KEY           "background-color"
#define TERMINAL_PROFILE_BACKSPACE_BINDING_KEY          "backspace-binding"
#define TERMINAL_PROFILE_BOLD_COLOR_KEY                 "bold-color"
#define TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY      "bold-color-same-as-fg"
#define TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY          "cell-height-scale"
#define TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY           "cell-width-scale"
#define TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY          "cursor-colors-set"
#define TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY    "cursor-background-color"
#define TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY    "cursor-foreground-color"
#define TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY   "cjk-utf8-ambiguous-width"
#define TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY          "cursor-blink-mode"
#define TERMINAL_PROFILE_CURSOR_SHAPE_KEY               "cursor-shape"
#define TERMINAL_PROFILE_CUSTOM_COMMAND_KEY             "custom-command"
#define TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY       "default-size-columns"
#define TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY          "default-size-rows"
#define TERMINAL_PROFILE_DELETE_BINDING_KEY             "delete-binding"
#define TERMINAL_PROFILE_ENABLE_BIDI_KEY                "enable-bidi"
#define TERMINAL_PROFILE_ENABLE_SHAPING_KEY             "enable-shaping"
#define TERMINAL_PROFILE_ENABLE_SIXEL_KEY               "enable-sixel"
#define TERMINAL_PROFILE_ENCODING_KEY                   "encoding"
#define TERMINAL_PROFILE_EXIT_ACTION_KEY                "exit-action"
#define TERMINAL_PROFILE_FONT_KEY                       "font"
#define TERMINAL_PROFILE_FOREGROUND_COLOR_KEY           "foreground-color"
#define TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY       "highlight-colors-set"
#define TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY "highlight-background-color"
#define TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY "highlight-foreground-color"
#define TERMINAL_PROFILE_LOGIN_SHELL_KEY                "login-shell"
#define TERMINAL_PROFILE_NAME_KEY                       "name"
#define TERMINAL_PROFILE_PALETTE_KEY                    "palette"
#define TERMINAL_PROFILE_PRESERVE_WORKING_DIRECTORY_KEY "preserve-working-directory"
#define TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY           "rewrap-on-resize"
#define TERMINAL_PROFILE_SCROLLBACK_LINES_KEY           "scrollback-lines"
#define TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY       "scrollback-unlimited"
#define TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY           "scrollbar-policy"
#define TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY           "scroll-on-insert"
#define TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY        "scroll-on-keystroke"
#define TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY           "scroll-on-output"
#define TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY            "text-blink-mode"
#define TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY         "use-custom-command"
#define TERMINAL_PROFILE_USE_SKEY_KEY                   "use-skey"
#define TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY            "use-system-font"
#define TERMINAL_PROFILE_USE_THEME_COLORS_KEY           "use-theme-colors"
#define TERMINAL_PROFILE_VISIBLE_NAME_KEY               "visible-name"
#define TERMINAL_PROFILE_WORD_CHAR_EXCEPTIONS_KEY       "word-char-exceptions"

#define TERMINAL_SETTING_CONFIRM_CLOSE_KEY              "confirm-close"
#define TERMINAL_SETTING_CONTEXT_INFO_KEY               "context-info"
#define TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY       "default-show-menubar"
#define TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY      "menu-accelerator-enabled"
#define TERMINAL_SETTING_ENABLE_MNEMONICS_KEY           "mnemonics-enabled"
#define TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY           "shortcuts-enabled"
#define TERMINAL_SETTING_HEADERBAR_KEY                  "headerbar"
#define TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY          "new-terminal-mode"
#define TERMINAL_SETTING_NEW_TAB_POSITION_KEY           "new-tab-position"
#define TERMINAL_SETTING_SCHEMA_VERSION                 "schema-version"
#define TERMINAL_SETTING_SHELL_INTEGRATION_KEY          "shell-integration-enabled"
#define TERMINAL_SETTING_TAB_POLICY_KEY                 "tab-policy"
#define TERMINAL_SETTING_TAB_POSITION_KEY               "tab-position"
#define TERMINAL_SETTING_THEME_VARIANT_KEY              "theme-variant"
#define TERMINAL_SETTING_UNIFIED_MENU_KEY               "unified-menu"
#define TERMINAL_SETTING_ALWAYS_CHECK_DEFAULT_KEY       "always-check-default-terminal"

#define TERMINAL_SETTINGS_LIST_LIST_KEY                 "list"
#define TERMINAL_SETTINGS_LIST_DEFAULT_KEY              "default"

#define TERMINAL_PROFILES_PATH_PREFIX   "/org/gnome/terminal/legacy/profiles:/"

G_END_DECLS

#endif /* TERMINAL_SCHEMAS_H */
