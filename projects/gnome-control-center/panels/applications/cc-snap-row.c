/* cc-snap-row.c
 *
 * Copyright 2019 Canonical Ltd.
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
#include <glib/gi18n.h>

#include "cc-snap-row.h"
#include "cc-applications-resources.h"
#include "cc-snapd-client.h"

#define CHANGE_POLL_TIME 100

struct _CcSnapRow
{
  AdwActionRow parent;

  GtkSwitch     *slot_toggle;
  GtkComboBox   *slots_combo;
  GtkListStore  *slots_combo_model;

  GCancellable  *cancellable;

  CcSnapdClient *client;
  JsonObject    *plug;
  JsonObject    *connected_slot;
  JsonArray     *slots;
  JsonObject    *target_slot;

  gchar         *change_id;
  guint          change_timeout;
};

G_DEFINE_TYPE (CcSnapRow, cc_snap_row, ADW_TYPE_ACTION_ROW)

static void
update_state (CcSnapRow *self)
{
  gboolean have_single_option;
  GtkTreeIter iter;

  have_single_option = json_array_get_length (self->slots) == 1;
  gtk_widget_set_visible (GTK_WIDGET (self->slot_toggle), have_single_option);
  gtk_widget_set_visible (GTK_WIDGET (self->slots_combo), !have_single_option);

  gtk_switch_set_active (self->slot_toggle, self->connected_slot != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->slots_combo_model), &iter))
    {
      do
        {
          JsonObject *slot;

          gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
          if (slot == self->connected_slot)
            gtk_combo_box_set_active_iter (self->slots_combo, &iter);
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->slots_combo_model), &iter));
    }
}

static void
disable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), FALSE);
}

static void
enable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), TRUE);
}

static void
change_complete (CcSnapRow *self)
{
  g_clear_object (&self->client);
  g_clear_pointer (&self->target_slot, json_object_unref);
  g_clear_pointer (&self->change_id, g_free);
  g_clear_handle_id (&self->change_timeout, g_source_remove);

  update_state (self);
  enable_controls (self);
}

static gboolean
poll_change_cb (gpointer user_data)
{
  CcSnapRow *self = user_data;
  g_autoptr(JsonObject) change = NULL;
  g_autoptr(GError) error = NULL;

  change = cc_snapd_client_get_change_sync (self->client, self->change_id, self->cancellable, &error);
  if (change == NULL)
    {
      g_warning ("Failed to monitor change %s: %s", self->change_id, error->message);
      change_complete (self);
      return G_SOURCE_REMOVE;
    }

  if (json_object_get_boolean_member (change, "ready"))
    {
      const gchar *status = json_object_get_string_member (change, "status");

      if (g_strcmp0 (status, "Done") == 0)
        {
          g_clear_pointer (&self->connected_slot, json_object_unref);
          self->connected_slot = self->target_slot ? json_object_ref (self->target_slot) : NULL;
        }
      else
        {
          g_warning ("Change completed with status %s", status);
        }

      change_complete (self);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
monitor_change (CcSnapRow *self, const gchar *change_id)
{
  g_free (self->change_id);
  self->change_id = g_strdup (change_id);
  g_clear_handle_id (&self->change_timeout, g_source_remove);
  self->change_timeout = g_timeout_add (CHANGE_POLL_TIME, poll_change_cb, self);
}

static CcSnapdClient *
get_client(CcSnapRow *self)
{
  if (self->client == NULL)
    self->client = cc_snapd_client_new ();
  return self->client;
}

static void
connect_plug (CcSnapRow *self, JsonObject *slot)
{
  g_autofree gchar *change_id = NULL;
  g_autoptr(GError) error = NULL;

  /* already connected */
  if (self->connected_slot != NULL &&
      g_strcmp0 (json_object_get_string_member (self->connected_slot, "snap"),
                 json_object_get_string_member (slot, "snap")) == 0 &&
      g_strcmp0 (json_object_get_string_member (self->connected_slot, "slot"),
                 json_object_get_string_member (slot, "slot")) == 0)
    return;

  disable_controls (self);

  change_id = cc_snapd_client_connect_interface_sync (get_client (self),
                                                      json_object_get_string_member (self->plug, "snap"),
                                                      json_object_get_string_member (self->plug, "plug"),
                                                      json_object_get_string_member (slot, "snap"),
                                                      json_object_get_string_member (slot, "slot"),
                                                      self->cancellable,
                                                      &error);
  if (change_id == NULL)
    {
      g_warning ("Failed to connect plug: %s", error->message);
      change_complete (self);
      return;
    }

  g_clear_pointer (&self->target_slot, json_object_unref);
  self->target_slot = json_object_ref (slot);
  monitor_change (self, change_id);
}

static void
disconnect_plug (CcSnapRow *self)
{
  g_autofree gchar *change_id = NULL;
  g_autoptr(GError) error = NULL;

  /* already disconnected */
  if (self->connected_slot == NULL)
    return;

  disable_controls (self);

  change_id = cc_snapd_client_disconnect_interface_sync (get_client (self),
                                                         json_object_get_string_member (self->plug, "snap"),
                                                         json_object_get_string_member (self->plug, "plug"),
                                                         "", "",
                                                         self->cancellable, &error);
  if (change_id == NULL)
    {
      g_warning ("Failed to disconnect plug: %s", error->message);
      change_complete (self);
      return;
    }

  g_clear_pointer (&self->target_slot, json_object_unref);
  monitor_change (self, change_id);
}

static void
switch_changed_cb (CcSnapRow *self)
{
  if (gtk_switch_get_active (self->slot_toggle))
    {
      if (json_array_get_length (self->slots) == 1)
        connect_plug (self, json_array_get_object_element (self->slots, 0));
    }
  else
    {
      disconnect_plug (self);
    }
}

static void
combo_changed_cb (CcSnapRow *self)
{
  GtkTreeIter iter;
  JsonObject *slot = NULL;

  if (!gtk_combo_box_get_active_iter (self->slots_combo, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
  if (slot != NULL)
    connect_plug (self, slot);
  else
    disconnect_plug (self);
}

static const gchar *
make_interface_label (const gchar *interface_name)
{
  if (strcmp (interface_name, "account-control") == 0)
    return _("Add user accounts and change passwords");
  else if (strcmp (interface_name, "alsa") == 0)
    return _("Play and record sound");
  else if (strcmp (interface_name, "audio-playback") == 0)
    return _("Play audio");
  else if (strcmp (interface_name, "audio-record") == 0)
    return _("Record audio");
  else if (strcmp (interface_name, "avahi-observe") == 0)
    return _("Detect network devices using mDNS/DNS-SD (Bonjour/zeroconf)");
  else if (strcmp (interface_name, "bluetooth-control") == 0)
    return _("Access bluetooth hardware directly");
  else if (strcmp (interface_name, "bluez") == 0)
    return _("Use bluetooth devices");
  else if (strcmp (interface_name, "camera") == 0)
    return _("Use your camera");
  else if (strcmp (interface_name, "cups-control") == 0)
    return _("Print documents");
  else if (strcmp (interface_name, "joystick") == 0)
    return _("Use any connected joystick");
  else if (strcmp (interface_name, "docker") == 0)
    return _("Allow connecting to the Docker service");
  else if (strcmp (interface_name, "firewall-control") == 0)
    return _("Configure network firewall");
  else if (strcmp (interface_name, "fuse-support") == 0)
    return _("Setup and use privileged FUSE filesystems");
  else if (strcmp (interface_name, "fwupd") == 0)
    return _("Update firmware on this device");
  else if (strcmp (interface_name, "hardware-observe") == 0)
    return _("Access hardware information");
  else if (strcmp (interface_name, "hardware-random-control") == 0)
    return _("Provide entropy to hardware random number generator");
  else if (strcmp (interface_name, "hardware-random-observe") == 0)
    return _("Use hardware-generated random numbers");
  else if (strcmp (interface_name, "home") == 0)
    return _("Access files in your home folder");
  else if (strcmp (interface_name, "libvirt") == 0)
    return _("Access libvirt service");
  else if (strcmp (interface_name, "locale-control") == 0)
    return _("Change system language and region settings");
  else if (strcmp (interface_name, "location-control") == 0)
    return _("Change location settings and providers");
  else if (strcmp (interface_name, "location-observe") == 0)
    return _("Access your location");
  else if (strcmp (interface_name, "log-observe") == 0)
    return _("Read system and application logs");
  else if (strcmp (interface_name, "lxd") == 0)
    return _("Access LXD service");
  else if (strcmp (interface_name, "modem-manager") == 0)
    return _("Use and configure modems");
  else if (strcmp (interface_name, "mount-observe") == 0)
    return _("Read system mount information and disk quotas");
  else if (strcmp (interface_name, "mpris") == 0)
    return _("Control music and video players");
  else if (strcmp (interface_name, "network-control") == 0)
    return _("Change low-level network settings");
  else if (strcmp (interface_name, "network-manager") == 0)
    return _("Access the NetworkManager service to read and change network settings");
  else if (strcmp (interface_name, "network-observe") == 0)
    return _("Read access to network settings");
  else if (strcmp (interface_name, "network-setup-control") == 0)
    return _("Change network settings");
  else if (strcmp (interface_name, "network-setup-observe") == 0)
    return _("Read network settings");
  else if (strcmp (interface_name, "ofono") == 0)
    return _("Access the ofono service to read and change network settings for mobile telephony");
  else if (strcmp (interface_name, "openvswitch") == 0)
    return _("Control Open vSwitch hardware");
  else if (strcmp (interface_name, "optical-drive") == 0)
    return _("Read from CD/DVD");
  else if (strcmp (interface_name, "password-manager-service") == 0)
    return _("Read, add, change, or remove saved passwords");
  else if (strcmp (interface_name, "ppp") == 0)
    return _("Access pppd and ppp devices for configuring Point-to-Point Protocol connections");
  else if (strcmp (interface_name, "process-control") == 0)
    return _("Pause or end any process on the system");
  else if (strcmp (interface_name, "pulseaudio") == 0)
    return _("Play and record sound");
  else if (strcmp (interface_name, "raw-usb") == 0)
    return _("Access USB hardware directly");
  else if (strcmp (interface_name, "removable-media") == 0)
    return _("Read/write files on removable storage devices");
  else if (strcmp (interface_name, "screen-inhibit-control") == 0)
    return _("Prevent screen sleep/lock");
  else if (strcmp (interface_name, "serial-port") == 0)
    return _("Access serial port hardware");
  else if (strcmp (interface_name, "shutdown") == 0)
    return _("Restart or power off the device");
  else if (strcmp (interface_name, "snapd-control") == 0)
    return _("Install, remove and configure software");
  else if (strcmp (interface_name, "storage-framework-service") == 0)
    return _("Access Storage Framework service");
  else if (strcmp (interface_name, "system-observe") == 0)
    return _("Read process and system information");
  else if (strcmp (interface_name, "system-trace") == 0)
    return _("Monitor and control any running program");
  else if (strcmp (interface_name, "time-control") == 0)
    return _("Change the date and time");
  else if (strcmp (interface_name, "timeserver-control") == 0)
    return _("Change time server settings");
  else if (strcmp (interface_name, "timezone-control") == 0)
    return _("Change the time zone");
  else if (strcmp (interface_name, "udisks2") == 0)
    return _("Access the UDisks2 service for configuring disks and removable media");
  else if (strcmp (interface_name, "upower-observe") == 0)
    return _("Access energy usage data");
  else if (strcmp (interface_name, "u2f-devices") == 0)
    return _("Read/write access to U2F devices exposed");
  else
    return interface_name;
}

static void
cc_snap_row_finalize (GObject *object)
{
  CcSnapRow *self = CC_SNAP_ROW (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->client);
  g_clear_pointer (&self->plug, json_object_unref);
  g_clear_pointer (&self->slots, json_array_unref);
  g_clear_pointer (&self->target_slot, json_object_unref);
  g_clear_pointer (&self->change_id, g_free);
  g_clear_handle_id (&self->change_timeout, g_source_remove);
  self->change_timeout = 0;

  G_OBJECT_CLASS (cc_snap_row_parent_class)->finalize (object);
}

static void
cc_snap_row_class_init (CcSnapRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_snap_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-snap-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slot_toggle);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo_model);

  gtk_widget_class_bind_template_callback (widget_class, combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_changed_cb);
}

static void
cc_snap_row_init (CcSnapRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcSnapRow *
cc_snap_row_new (GCancellable *cancellable, JsonObject *plug, JsonArray *slots)
{
  CcSnapRow *self;
  const gchar *label = NULL;
  GtkTreeIter iter;

  self = CC_SNAP_ROW (g_object_new (CC_TYPE_SNAP_ROW, NULL));

  self->cancellable = g_object_ref (cancellable);
  self->plug = json_object_ref (plug);
  self->slots = json_array_ref (slots);

  if (json_object_has_member (plug, "connections"))
    {
      JsonArray *connected_slots = json_object_get_array_member (plug, "connections");
      JsonObject *connected_slot_ref = json_array_get_object_element (connected_slots, 0);

      for (guint i = 0; i < json_array_get_length (slots); i++)
        {
          JsonObject *slot = json_array_get_object_element (slots, i);

          if (g_strcmp0 (json_object_get_string_member (slot, "snap"),
                         json_object_get_string_member (connected_slot_ref, "snap")) == 0 &&
              g_strcmp0 (json_object_get_string_member (slot, "slot"),
                         json_object_get_string_member (connected_slot_ref, "slot")) == 0)
            self->connected_slot = slot;
        }
    }

  label = make_interface_label (json_object_get_string_member (plug, "interface"));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), label);

  /* Add option into combo box */
  gtk_list_store_append (self->slots_combo_model, &iter);
  gtk_list_store_set (self->slots_combo_model, &iter, 1, "--", -1);
  for (guint i = 0; i < json_array_get_length (slots); i++)
    {
      JsonObject *slot = json_array_get_object_element (slots, i);
      g_autofree gchar *label = NULL;

      label = g_strdup_printf ("%s:%s", json_object_get_string_member (slot, "snap"),
                               json_object_get_string_member (slot, "slot"));
      gtk_list_store_append (self->slots_combo_model, &iter);
      gtk_list_store_set (self->slots_combo_model, &iter, 0, slot, 1, label, -1);
    }

  update_state (self);

  return self;
}
