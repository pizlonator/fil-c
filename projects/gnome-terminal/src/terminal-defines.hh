/*
 *  Copyright © 2011 Christian Persch
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

#ifndef TERMINAL_DEFINES_H
#define TERMINAL_DEFINES_H

G_BEGIN_DECLS

enum {
  _EXIT_FAILURE_WRONG_ID = 7,
  _EXIT_FAILURE_NO_UTF8 = 8,
  _EXIT_FAILURE_UNSUPPORTED_LOCALE = 9,
  _EXIT_FAILURE_GTK_INIT = 10
};

#define TERMINAL_APPLICATION_ID                 "org.gnome.Terminal"

#define TERMINAL_OBJECT_PATH_PREFIX             "/org/gnome/Terminal"
#define TERMINAL_OBJECT_INTERFACE_PREFIX        "org.gnome.Terminal"

#define TERMINAL_FACTORY_OBJECT_PATH            TERMINAL_OBJECT_PATH_PREFIX "/Factory0"
#define TERMINAL_FACTORY_INTERFACE_NAME         TERMINAL_OBJECT_INTERFACE_PREFIX ".Factory0"

#define TERMINAL_RECEIVER_OBJECT_PATH_FORMAT    TERMINAL_OBJECT_PATH_PREFIX "/screen/%s"
#define TEMRINAL_RECEIVER_INTERFACE_NAME        TERMINAL_OBJECT_INTERFACE_PREFIX ".Terminal0"

#define TERMINAL_SEARCH_PROVIDER_PATH           TERMINAL_OBJECT_PATH_PREFIX "/SearchProvider"

#define TERMINAL_SETTINGS_BRIDGE_INTERFACE_NAME "org.gnome.Terminal.SettingsBridge0"
#define TERMINAL_SETTINGS_BRIDGE_OBJECT_PATH    TERMINAL_OBJECT_PATH_PREFIX "/SettingsBridge"

#define TERMINAL_PREFERENCES_APPLICATION_ID     TERMINAL_APPLICATION_ID ".Preferences"
#define TERMINAL_PREFERENCES_OBJECT_PATH        TERMINAL_OBJECT_PATH_PREFIX  "/Preferences"

#define TERMINAL_ENV_SERVICE_NAME               "GNOME_TERMINAL_SERVICE"
#define TERMINAL_ENV_SCREEN                     "GNOME_TERMINAL_SCREEN"

#define TERMINAL_PREFERENCES_BINARY_NAME        "gnome-terminal-preferences"

G_END_DECLS

#endif /* !TERMINAL_DEFINES_H */
