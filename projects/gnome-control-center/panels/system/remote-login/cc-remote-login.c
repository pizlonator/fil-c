/*
 * Copyright (C) 2013 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */
#include "cc-remote-login.h"
#include "cc-systemd-service.h"

#include <gio/gio.h>
#include <polkit/polkit.h>

#ifndef SSHD_SERVICE
#define SSHD_SERVICE "sshd.service"
#endif

typedef struct
{
  AdwSwitchRow *widget;
  GtkWidget    *row;
  GCancellable *cancellable;
} CallbackData;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CallbackData, g_free)

void
cc_remote_login_get_enabled (AdwSwitchRow  *widget)
{
  /* disable the switch until the current state is known */
  gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

  adw_switch_row_set_active (widget, cc_is_service_active (SSHD_SERVICE, G_BUS_TYPE_SYSTEM));

  gtk_widget_set_sensitive (GTK_WIDGET (widget), TRUE);
}

static void
enable_ssh_service (GError** error)
{
  cc_enable_service (SSHD_SERVICE, G_BUS_TYPE_SYSTEM, error);
}

static void
disable_ssh_service (GError** error)
{
  cc_disable_service (SSHD_SERVICE, G_BUS_TYPE_SYSTEM, error);
}

static void
on_permission_acquired (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GPermission             *permission = (GPermission*) source_object;
  g_autoptr(CallbackData)  callback_data = user_data;
  g_autoptr(GError)        error = NULL;

  if (!g_permission_acquire_finish (permission, res, &error))
    {
      g_warning ("Cannot acquire '%s' permission: %s",
                 "org.gnome.controlcenter.remote-login-helper",
                 error->message);
    }
  else
    {
      if (g_permission_get_allowed (permission))
        {
          if (adw_switch_row_get_active (callback_data->widget))
            enable_ssh_service (&error);
          else
            disable_ssh_service (&error);

          /* Switch state should match service state */
          return;
        }
      else
        {
          g_warning ("Permission: %s not granted",
                     "org.gnome.controlcenter.remote-login-helper");
        }
    }

  /* If permission could not be acquired, or permission was not granted,
   * switch might be out of sync, update switch state */
  cc_remote_login_get_enabled (callback_data->widget);
}

void
cc_remote_login_set_enabled (GCancellable *cancellable,
                             AdwSwitchRow    *widget)
{
  GPermission       *permission;
  g_autoptr(GError)  error = NULL;

  CallbackData *callback_data;

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "set-from-dbus")) == 1)
    {
      g_object_set_data (G_OBJECT (widget), "set-from-dbus", NULL);
      return;
    }

  callback_data = g_new0 (CallbackData, 1);
  callback_data->widget = widget;
  callback_data->cancellable = cancellable;

  permission = polkit_permission_new_sync ("org.gnome.controlcenter.remote-login-helper",
                                           NULL, NULL, &error);

  if (permission != NULL)
    {
      g_permission_acquire_async (permission, callback_data->cancellable,
                                  on_permission_acquired, callback_data);
    }
  else
    {
      g_warning ("Cannot create '%s' permission: %s",
                "org.gnome.controlcenter.remote-login-helper",
                error->message);
    }
}

