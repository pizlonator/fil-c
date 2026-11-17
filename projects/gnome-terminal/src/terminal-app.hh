/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008 Christian Persch
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

#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"
#include "terminal-profiles-list.hh"

G_BEGIN_DECLS

#define GNOME_TERMINAL_ICON_NAME "org.gnome.Terminal"

#define TERMINAL_RESOURCES_PATH_PREFIX "/org/gnome/terminal"

#define MONOSPACE_FONT_KEY_NAME                 "monospace-font-name"

/* TerminalApp */

#define TERMINAL_TYPE_APP              (terminal_app_get_type ())
#define TERMINAL_APP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_APP, TerminalApp))
#define TERMINAL_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_APP, TerminalAppClass))
#define TERMINAL_IS_APP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_APP))
#define TERMINAL_IS_APP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_APP))
#define TERMINAL_APP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_APP, TerminalAppClass))

typedef struct _TerminalAppClass TerminalAppClass;
typedef struct _TerminalApp TerminalApp;

GType terminal_app_get_type (void);

GApplication *terminal_app_new (const char *app_id,
                                GApplicationFlags flags,
                                GSettingsBackend* backend);

#define terminal_app_get (TerminalApp *) g_application_get_default

GDBusObjectManagerServer *terminal_app_get_object_manager (TerminalApp *app);

GdkAtom *terminal_app_get_clipboard_targets (TerminalApp *app,
                                             GtkClipboard *clipboard,
                                             int *n_targets);

void terminal_app_edit_preferences (TerminalApp *app,
                                    GSettings   *profile,
                                    const char  *widget_name,
                                    unsigned timestamp);

char *terminal_app_new_profile (TerminalApp *app,
                                GSettings   *default_base_profile,
                                const char  *name);

void terminal_app_remove_profile (TerminalApp *app,
                                  GSettings *profile);

char *terminal_app_dup_screen_object_path (TerminalApp *app,
                                           TerminalScreen *screen);

TerminalScreen *terminal_app_get_screen_by_uuid (TerminalApp *app,
                                                 const char  *uuid);

TerminalScreen *terminal_app_get_screen_by_object_path (TerminalApp *app,
                                                        const char *object_path);

void terminal_app_register_screen (TerminalApp *app,
                                   TerminalScreen *screen);

void terminal_app_unregister_screen (TerminalApp *app,
                                     TerminalScreen *screen);

TerminalSettingsList *terminal_app_get_profiles_list (TerminalApp *app);

/* Menus */

GMenuModel *terminal_app_get_menubar (TerminalApp *app);

GMenuModel *terminal_app_get_headermenu (TerminalApp *app);

GMenuModel *terminal_app_get_profilemenu (TerminalApp *app);

GMenuModel *terminal_app_get_profile_section (TerminalApp *app);

gboolean terminal_app_get_menu_unified (TerminalApp *app);

gboolean terminal_app_get_use_headerbar (TerminalApp *app);

gboolean terminal_app_get_dialog_use_headerbar (TerminalApp *app);

/* GSettings */

typedef enum {
  TERMINAL_PROXY_HTTP  = 0,
  TERMINAL_PROXY_HTTPS = 1,
  TERMINAL_PROXY_FTP   = 2,
  TERMINAL_PROXY_SOCKS = 3,
} TerminalProxyProtocol;

GSettingsBackend* terminal_app_get_settings_backend(TerminalApp* app);

GSettingsSchemaSource* terminal_app_get_schema_source(TerminalApp* app);

GSettings *terminal_app_get_global_settings (TerminalApp *app);

GSettings *terminal_app_get_desktop_interface_settings (TerminalApp *app);

GSettings *terminal_app_get_proxy_settings (TerminalApp *app);

GSettings *terminal_app_get_proxy_settings_for_protocol(TerminalApp *app,
                                                        TerminalProxyProtocol protocol);

GSettings *terminal_app_get_gtk_debug_settings (TerminalApp *app);

PangoFontDescription *terminal_app_get_system_font (TerminalApp *app);

gboolean terminal_app_is_default_terminal(TerminalApp* app);

gboolean terminal_app_get_ask_default_terminal(TerminalApp* app);

void terminal_app_unset_ask_default_terminal(TerminalApp* app);

void terminal_app_make_default_terminal(TerminalApp* app);

G_END_DECLS

#endif /* !TERMINAL_APP_H */
