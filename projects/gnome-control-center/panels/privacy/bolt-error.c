/*
 * Copyright © 2017 Red Hat, Inc
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

#include "bolt-error.h"

#include "bolt-names.h"

#include <gio/gio.h>

/**
 * SECTION:bolt-error
 * @Title: Error codes
 *
 */

static const GDBusErrorEntry bolt_error_entries[] = {
  {BOLT_ERROR_FAILED,     BOLT_DBUS_NAME ".Error.Failed"},
  {BOLT_ERROR_UDEV,       BOLT_DBUS_NAME ".Error.UDev"},
};


GQuark
bolt_error_quark (void)
{
  static volatile gsize quark_volatile = 0;

  g_dbus_error_register_error_domain ("bolt-error-quark",
                                      &quark_volatile,
                                      bolt_error_entries,
                                      G_N_ELEMENTS (bolt_error_entries));
  return (GQuark) quark_volatile;
}

gboolean
bolt_err_notfound (const GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
         g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) ||
         g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
         g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
}

gboolean
bolt_err_exists (const GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS) ||
         g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_EXIST);
}

gboolean
bolt_err_inval (const GError *error)
{
  return g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
}

gboolean
bolt_err_cancelled (const GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

gboolean
bolt_error_propagate_stripped (GError **dest,
                               GError **source)
{
  GError *src;

  g_return_val_if_fail (source != NULL, FALSE);

  src = *source;

  if (src == NULL)
    return TRUE;

  if (g_dbus_error_is_remote_error (src))
    g_dbus_error_strip_remote_error (src);

  g_propagate_error (dest, g_steal_pointer (source));
  return FALSE;
}
