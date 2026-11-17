/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2011, 2013 Christian Persch
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

#include "config.h"

#include "terminal-profiles-list.hh"
#include "terminal-schemas.hh"
#include "terminal-libgsystem.hh"

#include <string.h>
#include <uuid.h>

/* Counts occurrences of @str in @strv */
static guint
strv_contains (char **strv,
               const char *str,
               guint *idx)
{
  guint n, i;

  if (strv == nullptr)
    return 0;

  n = 0;
  for (i = 0; strv[i]; i++) {
    if (strcmp (strv[i], str) == 0) {
      n++;
      if (idx)
        *idx = i;
    }
  }

  return n;
}

static gboolean
valid_uuid (const char *str,
            GError **error)
{
  if (terminal_settings_list_valid_uuid (str))
    return TRUE;

  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "\"%s\" is not a valid UUID", str);
  return FALSE;
}

/**
 * terminal_profiles_list_new:
 * @backend: a #GSettingsBackend
 * @schema_source: a #GSettingsSchemaSource
 *
 * Returns: (transfer full): a new #TerminalSettingsList for the profiles list
 */
TerminalSettingsList *
terminal_profiles_list_new(GSettingsBackend* backend,
                           GSettingsSchemaSource* schema_source)
{
  return terminal_settings_list_new (backend,
                                     schema_source,
                                     TERMINAL_PROFILES_PATH_PREFIX,
                                     TERMINAL_PROFILES_LIST_SCHEMA,
                                     TERMINAL_PROFILE_SCHEMA,
                                     TERMINAL_SETTINGS_LIST_FLAG_HAS_DEFAULT);
}

static void
get_profile_names (TerminalSettingsList *list,
                   char ***profilesp,
                   char ***namesp)
{
  char **profiles, **names;
  guint i, n;

  *profilesp = profiles = terminal_settings_list_dupv_children (list);

  n = g_strv_length (profiles);
  *namesp = names = g_new0 (char *, n + 1);
  for (i = 0; i < n; i++) {
    gs_unref_object GSettings *profile;

    profile = terminal_settings_list_ref_child (list, profiles[i]);
    names[i] = g_settings_get_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  }

  names[n] = nullptr;
}

/**
 * terminal_profiles_list_ref_children_sorted:
 * @list:
 *
 * Returns: (transfer full):
 */
GList *
terminal_profiles_list_ref_children_sorted (TerminalSettingsList *list)
{
  return g_list_sort (terminal_settings_list_ref_children (list),
                      terminal_profiles_compare);
}

/**
 * terminal_profiles_list_dup_uuid:
 * @list:
 * @uuid: (allow-none):
 * @error:
 *
 * Returns: (transfer full): the UUID of the profile specified by @uuid, or %nullptr
 */
char *
terminal_profiles_list_dup_uuid (TerminalSettingsList *list,
                                 const char *uuid,
                                 GError **error)
{
  char *rv;

  if (uuid == nullptr) {
    rv = terminal_settings_list_dup_default_child (list);
    if (rv == nullptr)
      goto err;
    return rv;
  } else if (!valid_uuid (uuid, error))
    return nullptr;

  if (terminal_settings_list_has_child (list, uuid))
    return g_strdup (uuid);

 err:
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "No profile with UUID \"%s\" exists", uuid);
  return nullptr;
}

/**
 * terminal_profiles_list_ref_profile_by_uuid_or_name:
 * @list:
 * @uuid:
 * @error:
 *
 * Returns: (transfer full): the profile #GSettings specified by @uuid, or %nullptr
 */
GSettings *
terminal_profiles_list_ref_profile_by_uuid (TerminalSettingsList *list,
                                            const char *uuid,
                                            GError **error)
{
  gs_free char *profile_uuid;
  GSettings *profile;

  profile_uuid = terminal_profiles_list_dup_uuid (list, uuid, error);
  if (profile_uuid == nullptr)
    return nullptr;

  profile = terminal_settings_list_ref_child (list, profile_uuid);
  return profile;
}

/**
 * terminal_profiles_list_get_profile_by_uuid:
 * @list:
 * @uuid: (allow-none):
 * @error:
 *
 * Returns: (transfer full): the UUID of the profile specified by @uuid, or %nullptr
 */
char *
terminal_profiles_list_dup_uuid_or_name (TerminalSettingsList *list,
                                         const char *uuid_or_name,
                                         GError **error)
{
  char **profiles, **profile_names;
  char *rv;
  guint n, i;

  rv = terminal_profiles_list_dup_uuid (list, uuid_or_name, nullptr);
  if (rv != nullptr)
    return rv;

  /* Not found as UUID; try finding a profile with this string as 'visible-name' */
  get_profile_names (list, &profiles, &profile_names);
  n = strv_contains (profile_names, uuid_or_name, &i);

  if (n == 0) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "No profile with UUID or name \"%s\" exists", uuid_or_name);
    rv = nullptr;
  } else if (n != 1) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "No profile with UUID \"%s\" found and name is ambiguous", uuid_or_name);
    rv = nullptr;
  } else {
    rv = g_strdup (profiles[i]);
  }

  g_strfreev (profiles);
  g_strfreev (profile_names);

  return rv;
}

/**
 * terminal_profiles_list_ref_profile_by_uuid_or_name:
 * @list:
 * @uuid:
 * @error:
 *
 * Returns: (transfer full): the profile #GSettings specified by @uuid, or %nullptr
 */
GSettings *
terminal_profiles_list_ref_profile_by_uuid_or_name (TerminalSettingsList *list,
                                                    const char *uuid_or_name,
                                                    GError **error)
{
  gs_free char *uuid;
  GSettings *profile;

  uuid = terminal_profiles_list_dup_uuid_or_name (list, uuid_or_name, error);
  if (uuid == nullptr)
    return nullptr;

  profile = terminal_settings_list_ref_child (list, uuid);
  g_assert (profile != nullptr);
  return profile;
}

int
terminal_profiles_compare (gconstpointer pa,
                           gconstpointer pb)
{
  GSettings *a = (GSettings *) pa;
  GSettings *b = (GSettings *) pb;
  gs_free char *na = nullptr;
  gs_free char *nb = nullptr;
  gs_free char *patha = nullptr;
  gs_free char *pathb = nullptr;
  int result;

  if (pa == pb)
    return 0;
  if (pa == nullptr)
    return 1;
  if (pb == nullptr)
    return -1;

  na = g_settings_get_string (a, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  nb = g_settings_get_string (b, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  result =  g_utf8_collate (na, nb);
  if (result != 0)
    return result;

  g_object_get (a, "path", &patha, nullptr);
  g_object_get (b, "path", &pathb, nullptr);
  return strcmp (patha, pathb);
}
