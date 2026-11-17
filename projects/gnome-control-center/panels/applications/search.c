/* search.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <config.h>

#include "search.h"


#define SHELL_PROVIDER_GROUP "Shell Search Provider"

static void
add_one_provider (GHashTable *search_providers,
                  GFile      *file)
{
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *app_id = NULL;
  g_autofree gchar *path = NULL;
  gboolean default_disabled;

  path = g_file_get_path (file);
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error);

  if (error != NULL)
    {
      g_warning ("Error loading %s: %s - search provider will be ignored",
                 path, error->message);
      return;
    }

  if (!g_key_file_has_group (keyfile, SHELL_PROVIDER_GROUP))
    {
      g_debug ("Shell search provider group missing from '%s', ignoring", path);
      return;
    }

  app_id = g_key_file_get_string (keyfile, SHELL_PROVIDER_GROUP, "DesktopId", &error);

  if (error != NULL)
    {
      g_warning ("Unable to read desktop ID from %s: %s - search provider will be ignored",
                 path, error->message);
      return;
    }

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  default_disabled = g_key_file_get_boolean (keyfile, SHELL_PROVIDER_GROUP, "DefaultDisabled", NULL);

  g_hash_table_insert (search_providers, g_strdup (app_id), GINT_TO_POINTER (default_disabled));
}

static void
parse_search_providers_one_dir (GHashTable  *search_providers,
                                const gchar *system_dir)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) providers_location = NULL;
  g_autofree gchar *providers_path = NULL;

  providers_path = g_build_filename (system_dir, "gnome-shell", "search-providers", NULL);
  providers_location = g_file_new_for_path (providers_path);

  enumerator = g_file_enumerate_children (providers_location,
                                          "standard::type,standard::name,standard::content-type",
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error opening %s: %s - search provider configuration won't be possible",
                   providers_path, error->message);
      return;
    }

  while (TRUE)
    {
      GFile *provider = NULL;

      if (!g_file_enumerator_iterate (enumerator, NULL, &provider, NULL, &error))
        {
          g_warning ("Error while reading %s: %s - search provider configuration won't be possible",
                   providers_path, error->message);
          return;
        }

      if (provider == NULL)
        break;

      add_one_provider (search_providers, provider);
    }
}

/* parse gnome-shell/search-provider files and return a string->boolean hash table */
GHashTable *
parse_search_providers (void)
{
  GHashTable *search_providers;
  const gchar * const *dirs;
  gint i;

  search_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i]; i++)
    parse_search_providers_one_dir (search_providers, dirs[i]);

  return search_providers;
}

