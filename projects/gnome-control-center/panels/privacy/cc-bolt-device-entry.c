/* Copyright (C) 2018 Red Hat, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Christian J. Kellner <ckellner@redhat.com>
 *
 */

#include <config.h>

#include "bolt-str.h"

#include "cc-bolt-device-entry.h"

#include <glib/gi18n.h>

#define RESOURCE_UI "/org/gnome/control-center/privacy/cc-bolt-device-entry.ui"

struct _CcBoltDeviceEntry
{
  AdwActionRow parent;

  BoltDevice   *device;

  /* main ui */
  GtkLabel *status_warning;
  gboolean  show_warnings;
};

static const char *   device_status_to_brief_for_ui (BoltDevice *dev);

G_DEFINE_TYPE (CcBoltDeviceEntry, cc_bolt_device_entry, ADW_TYPE_ACTION_ROW);

enum
{
  SIGNAL_STATUS_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
entry_set_name (CcBoltDeviceEntry *entry)
{
  g_autofree char *name = NULL;
  BoltDevice *dev = entry->device;

  g_return_if_fail (dev != NULL);

  name = bolt_device_get_display_name (dev);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (entry), name);
}

static void
entry_update_status (CcBoltDeviceEntry *entry)
{
  const char *brief;
  BoltStatus status;
  gboolean warn;

  status = bolt_device_get_status (entry->device);
  brief = device_status_to_brief_for_ui (entry->device);

  adw_action_row_set_subtitle (ADW_ACTION_ROW (entry), brief);

  g_signal_emit (entry,
                 signals[SIGNAL_STATUS_CHANGED],
                 0,
                 status);

  warn = entry->show_warnings && bolt_status_is_pending (status);
  gtk_widget_set_visible (GTK_WIDGET (entry->status_warning), warn);
}

static void
on_device_notify_cb (GObject    *gobject,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  CcBoltDeviceEntry *entry = CC_BOLT_DEVICE_ENTRY (user_data);
  const char *what;

  what = g_param_spec_get_name (pspec);

  if (bolt_streq (what, "status"))
    entry_update_status (entry);
  else if (bolt_streq (what, "label") ||
           bolt_streq (what, "name") ||
           bolt_streq (what, "vendor"))
    entry_set_name (entry);
}

/* device helpers */

static const char *
device_status_to_brief_for_ui (BoltDevice *dev)
{
  BoltStatus status;
  BoltAuthFlags aflags;
  gboolean nopcie;

  status = bolt_device_get_status (dev);
  aflags = bolt_device_get_authflags(dev);
  nopcie = bolt_flag_isset (aflags, BOLT_AUTH_NOPCIE);

  switch (status)
    {
    case BOLT_STATUS_DISCONNECTED:
      return C_("Thunderbolt Device Status", "Disconnected");

    case BOLT_STATUS_CONNECTING:
      return C_("Thunderbolt Device Status", "Connecting");

    case BOLT_STATUS_CONNECTED:
    case BOLT_STATUS_AUTHORIZED_DPONLY:
      return C_("Thunderbolt Device Status", "Connected");

    case BOLT_STATUS_AUTH_ERROR:
      return C_("Thunderbolt Device Status", "Error");

    case BOLT_STATUS_AUTHORIZING:
      return C_("Thunderbolt Device Status", "Authorizing");

    case BOLT_STATUS_AUTHORIZED:
    case BOLT_STATUS_AUTHORIZED_NEWKEY:
    case BOLT_STATUS_AUTHORIZED_SECURE:
      if (nopcie)
        return C_("Thunderbolt Device Status", "Connected");
      else
        return C_("Thunderbolt Device Status", "Authorized");

    case BOLT_STATUS_UNKNOWN:
      break; /* use function default */
    }

  return C_("Thunderbolt Device Status", "Unknown");
}

static void
cc_bolt_device_entry_finalize (GObject *object)
{
  CcBoltDeviceEntry *entry = CC_BOLT_DEVICE_ENTRY (object);

  g_clear_object (&entry->device);

  G_OBJECT_CLASS (cc_bolt_device_entry_parent_class)->finalize (object);
}

static void
cc_bolt_device_entry_class_init (CcBoltDeviceEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_bolt_device_entry_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_UI);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceEntry, status_warning);

  signals[SIGNAL_STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, BOLT_TYPE_STATUS);
}

static void
cc_bolt_device_entry_init (CcBoltDeviceEntry *entry)
{
  gtk_widget_init_template (GTK_WIDGET (entry));
}

/* public function */

CcBoltDeviceEntry *
cc_bolt_device_entry_new (BoltDevice *device,
			  gboolean    show_warnings)
{
  CcBoltDeviceEntry *entry;

  entry = g_object_new (CC_TYPE_BOLT_DEVICE_ENTRY, NULL);
  entry->device = g_object_ref (device);
  entry->show_warnings = show_warnings;

  entry_set_name (entry);
  entry_update_status (entry);

  g_signal_connect_object (entry->device,
                           "notify",
                           G_CALLBACK (on_device_notify_cb),
                           entry,
                           0);

  return entry;
}

BoltDevice *
cc_bolt_device_entry_get_device (CcBoltDeviceEntry *entry)
{
  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (CC_IS_BOLT_DEVICE_ENTRY (entry), NULL);

  return entry->device;
}
