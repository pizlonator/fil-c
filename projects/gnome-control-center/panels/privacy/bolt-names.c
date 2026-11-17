/*
 * Copyright © 2019 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#include "config.h"

#include "bolt-names.h"

/* Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_" */
#define DBUS_OPATH_VALID_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"

char *
bolt_gen_object_path (const char *base,
                      const char *oid)
{
  g_autofree char *id = NULL;

  if (oid)
    {
      id = g_strdup (oid);
      g_strcanon (id, DBUS_OPATH_VALID_CHARS, '_');
    }

  if (base && id)
    return g_build_path ("/", "/", base, id, NULL);
  else if (base)
    return g_build_path ("/", "/", base, NULL);
  else if (id)
    return g_build_path ("/", "/", id, NULL);

  return g_strdup ("/");
}
