/*
 * Copyright © 2013 Christian Persch
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

#ifndef TERMINAL_SETTINGS_LIST_H
#define TERMINAL_SETTINGS_LIST_H

#include <gio/gio.h>

#include "terminal-enums.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_SETTINGS_LIST            (terminal_settings_list_get_type ())
#define TERMINAL_SETTINGS_LIST(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_SETTINGS_LIST, TerminalSettingsList))
#define TERMINAL_SETTINGS_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_SETTINGS_LIST, TerminalSettingsListClass))
#define TERMINAL_IS_SETTINGS_LIST(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_SETTINGS_LIST))
#define TERMINAL_IS_SETTINGS_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_SETTINGS_LIST))
#define TERMINAL_SETTINGS_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_SETTINGS_LIST, TerminalSettingsListClass))

typedef struct _TerminalSettingsList      TerminalSettingsList;
typedef struct _TerminalSettingsListClass TerminalSettingsListClass;

GType terminal_settings_list_get_type (void);

TerminalSettingsList *terminal_settings_list_new (GSettingsBackend* backend,
                                                  GSettingsSchemaSource* schema_source,
                                                  const char *path,
                                                  const char *schema_id,
                                                  const char *child_schema_id,
                                                  TerminalSettingsListFlags flags);

char **terminal_settings_list_dupv_children (TerminalSettingsList *list);

GList *terminal_settings_list_ref_children (TerminalSettingsList *list);

gboolean terminal_settings_list_has_child (TerminalSettingsList *list,
                                           const char *uuid);

GSettings *terminal_settings_list_ref_child (TerminalSettingsList *list,
                                             const char *uuid);

char *terminal_settings_list_add_child (TerminalSettingsList *list,
                                        const char *name);

char *terminal_settings_list_clone_child (TerminalSettingsList *list,
                                          const char *uuid,
                                          const char *name);

void terminal_settings_list_remove_child (TerminalSettingsList *list,
                                          const char *uuid);

char *terminal_settings_list_dup_uuid_from_child (TerminalSettingsList *list,
                                                  GSettings *child);

GSettings *terminal_settings_list_ref_default_child (TerminalSettingsList *list);

char *terminal_settings_list_dup_default_child (TerminalSettingsList *list);

void terminal_settings_list_set_default_child (TerminalSettingsList *list,
                                               const char *uuid);

typedef void (* TerminalSettingsListForeachFunc) (TerminalSettingsList *list,
                                                  const char *uuid,
                                                  GSettings *child,
                                                  gpointer user_data);

void terminal_settings_list_foreach_child (TerminalSettingsList *list,
                                           TerminalSettingsListForeachFunc callback,
                                           gpointer user_data);

guint terminal_settings_list_get_n_children (TerminalSettingsList *list);

gboolean terminal_settings_list_valid_uuid (const char *str);

G_END_DECLS

#endif /* TERMINAL_SETTINGS_LIST_H */
