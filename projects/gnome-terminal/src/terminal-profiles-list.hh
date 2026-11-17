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

#ifndef TERMINAL_PROFILES_LIST_H
#define TERMINAL_PROFILES_LIST_H

#include <glib.h>
#include <gio/gio.h>

#include "terminal-settings-list.hh"

G_BEGIN_DECLS

TerminalSettingsList *terminal_profiles_list_new(GSettingsBackend* backend,
                                                 GSettingsSchemaSource* schema_source);

GList *terminal_profiles_list_ref_children_sorted (TerminalSettingsList *list);

char *terminal_profiles_list_dup_uuid (TerminalSettingsList *list,
                                       const char *uuid,
                                       GError **error);

GSettings *terminal_profiles_list_ref_profile_by_uuid (TerminalSettingsList *list,
                                                       const char *uuid,
                                                       GError **error);

char *terminal_profiles_list_dup_uuid_or_name (TerminalSettingsList *list,
                                               const char *uuid_or_name,
                                               GError **error);

GSettings *terminal_profiles_list_ref_profile_by_uuid_or_name (TerminalSettingsList *list,
                                                               const char *uuid_or_name,
                                                               GError **error);

int terminal_profiles_compare (gconstpointer pa,
                               gconstpointer pb);

G_END_DECLS

#endif /* TERMINAL_PROFILES_LIST_H */
