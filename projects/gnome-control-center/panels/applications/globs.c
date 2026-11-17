/* globs.c
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

#include "globs.h"

/* parse mime/globs and return a string->string hash table */
GHashTable *
parse_globs (void)
{
  GHashTable *globs;
  const gchar * const *dirs;
  gint i;

  globs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i]; i++)
    {
      g_autofree gchar *file = g_build_filename (dirs[i], "mime", "globs", NULL);
      g_autofree gchar *contents = NULL;

      if (g_file_get_contents (file, &contents, NULL, NULL))
        {
          g_auto(GStrv) strv = NULL;
          int i;

          strv = g_strsplit (contents, "\n", 0);
          for (i = 0; strv[i]; i++)
            {
              g_auto(GStrv) parts = NULL;

              if (strv[i][0] == '#' || strv[i][0] == '\0')
                continue;

              parts = g_strsplit (strv[i], ":", 2);
              g_hash_table_insert (globs, g_strdup (parts[0]), g_strdup (parts[1]));
            }
        }
    }

  return globs;
}
