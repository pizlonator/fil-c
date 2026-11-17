/* Copyright © 2018 Red Hat, Inc
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

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include "shell/cc-application.h"
#include "cc-bolt-device-dialog.h"
#include "cc-bolt-device-entry.h"

#include "bolt-client.h"
#include "bolt-names.h"
#include "bolt-str.h"

#include "cc-bolt-page.h"

struct _CcBoltPage
{
  AdwNavigationPage   parent;

  BoltClient         *client;

  /* headerbar menu */
  GtkBox             *headerbar_box;
  GtkLockButton      *lock_button;

  /* main ui */
  GtkStack           *container;

  /* empty state */
  AdwStatusPage      *notb_page;

  /* notifications */
  GtkLabel           *notification_label;
  GtkRevealer        *notification_revealer;

  /* authmode */
  GtkSwitch          *authmode_switch;
  GtkSpinner         *authmode_spinner;
  GtkStack           *direct_access_row;

  /* device list */
  GHashTable         *devices;

  GtkStack           *devices_stack;
  GtkBox             *devices_box;
  GtkBox             *pending_box;

  GtkListBox         *devices_list;
  GtkListBox         *pending_list;

  /* device details dialog */
  CcBoltDeviceDialog *device_dialog;

  /* polkit integration */
  GPermission        *permission;

  GCancellable       *cancellable;
};

/* initialization */
static void           bolt_client_ready (GObject      *source,
                                         GAsyncResult *res,
                                         gpointer      user_data);

/* panel functions */
static void                cc_bolt_page_set_no_thunderbolt (CcBoltPage *self,
                                                             const char *custom_msg);

static void                cc_bolt_page_name_owner_changed (CcBoltPage *self);

static CcBoltDeviceEntry * cc_bolt_page_add_device (CcBoltPage *self,
                                                     BoltDevice *dev);

static void                cc_bolt_page_del_device_entry (CcBoltPage        *self,
                                                           CcBoltDeviceEntry *entry);

static void                cc_bolt_page_authmode_sync (CcBoltPage *self);

static void                cc_panel_list_box_migrate (CcBoltPage        *self,
                                                      GtkListBox        *from,
                                                      GtkListBox        *to,
                                                      CcBoltDeviceEntry *entry);

/* bolt client signals */
static void     on_bolt_name_owner_changed_cb (GObject    *object,
                                               GParamSpec *pspec,
                                               gpointer    user_data);

static void     on_bolt_device_added_cb (BoltClient *cli,
                                         const char *path,
                                         CcBoltPage *self);

static void     on_bolt_device_removed_cb (BoltClient *cli,
                                           const char *opath,
                                           CcBoltPage *self);

static void     on_bolt_notify_authmode_cb (GObject    *gobject,
                                            GParamSpec *pspec,
                                            gpointer    user_data);

/* panel signals */
static gboolean on_authmode_state_set_cb (CcBoltPage *self,
                                          gboolean    state,
                                          GtkSwitch  *toggle);

static void     on_device_entry_row_activated_cb (CcBoltPage    *self,
                                                  GtkListBoxRow *row);

static void     on_device_entry_status_changed_cb (CcBoltDeviceEntry *entry,
                                                   BoltStatus         new_status,
                                                   CcBoltPage        *self);

static void     on_notification_button_clicked_cb (CcBoltPage *self);


/* polkit */
static void      on_permission_ready (GObject      *source_object,
                                      GAsyncResult *res,
                                      gpointer      user_data);

static void      on_permission_notify_cb (GPermission *permission,
                                          GParamSpec  *pspec,
                                          CcBoltPage  *self);

G_DEFINE_TYPE (CcBoltPage, cc_bolt_page, ADW_TYPE_NAVIGATION_PAGE)

static void
update_visibility (BoltClient  *client,
                   const char  *path,
                   gpointer     user_data)
{
  g_autoptr(GPtrArray) devices = NULL;
  gboolean visible = FALSE;
  CcBoltPage *self;

  self = CC_BOLT_PAGE (user_data);

  if (client)
    {
      devices = bolt_client_list_devices (client, self->cancellable, NULL);
      if (devices)
        visible = devices->len > 0;
    }

  gtk_widget_set_visible (GTK_WIDGET (self), visible);
}

static void
on_visibility_client_ready (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  BoltClient *client;
  CcBoltPage *self;

  self = CC_BOLT_PAGE (user_data);

  client = bolt_client_new_finish (res, NULL);
  if (client == NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      return;
    }

  g_signal_connect_object (client,
                           "device-added",
                           G_CALLBACK (update_visibility),
                           self,
                           0);
  g_signal_connect_object (client,
                           "device-removed",
                           G_CALLBACK (update_visibility),
                           self,
                           0);
  update_visibility (client, NULL, self);
}

static void
bolt_client_ready (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(CcBoltPage) self = NULL;
  BoltClient *client;

  self = CC_BOLT_PAGE (user_data);
  client = bolt_client_new_finish (res, &err);

  if (client == NULL)
    {
      const char *text;

      /* operation got cancelled because the panel got destroyed */
      if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
          g_error_matches (err, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        return;

      g_warning ("Could not create client: %s", err->message);
      text = _("The Thunderbolt subsystem (boltd) is not installed or "
               "not set up properly.");

      adw_status_page_set_description (self->notb_page, text);
      gtk_stack_set_visible_child_name (self->container, "no-thunderbolt");

      return;
    }

  g_signal_connect_object (client,
                           "notify::g-name-owner",
                           G_CALLBACK (on_bolt_name_owner_changed_cb),
                           self,
                           0);

  g_signal_connect_object (client,
                           "device-added",
                           G_CALLBACK (on_bolt_device_added_cb),
                           self,
                           0);

  g_signal_connect_object (client,
                           "device-removed",
                           G_CALLBACK (on_bolt_device_removed_cb),
                           self,
                           0);

  g_signal_connect_object (client,
                           "notify::auth-mode",
                           G_CALLBACK (on_bolt_notify_authmode_cb),
                           self,
                           0);

  /* Treat security-level changes, which should rarely happen, as
   * if the name owner changed, i.e. as if boltd got restarted */
  g_signal_connect_object (client,
                           "notify::security-level",
                           G_CALLBACK (on_bolt_name_owner_changed_cb),
                           self,
                           0);

  self->client = client;

  cc_bolt_device_dialog_set_client (self->device_dialog, client);

  cc_bolt_page_authmode_sync (self);

  g_object_bind_property (self->authmode_switch,
                          "active",
                          self->devices_box,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->authmode_switch,
                          "active",
                          self->pending_box,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  gtk_stack_set_visible_child_name (self->devices_stack, "no-devices");
  cc_bolt_page_name_owner_changed (self);
}

static gboolean
devices_table_transfer_entry (GHashTable   *from,
                              GHashTable   *to,
                              gconstpointer key)
{
  gpointer k, v;
  gboolean found;

  found = g_hash_table_lookup_extended (from, key, &k, &v);

  if (found)
    {
      g_hash_table_steal (from, key);
      g_hash_table_insert (to, k, v);
    }

  return found;
}

static void
devices_table_clear_entries (GHashTable *table,
                             CcBoltPage *self)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      CcBoltDeviceEntry *entry = value;

      cc_bolt_page_del_device_entry (self, entry);
      g_hash_table_iter_remove (&iter);
    }
}

static void
devices_table_synchronize (CcBoltPage *self)
{
  g_autoptr(GHashTable) old = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  g_autoptr(GError) err = NULL;
  guint i;

  devices = bolt_client_list_devices (self->client, self->cancellable, &err);

  if (!devices)
    {
      g_warning ("Could not list devices: %s", err->message);
      devices = g_ptr_array_new_with_free_func (g_object_unref);
    }

  old = self->devices;
  self->devices = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; i < devices->len; i++)
    {
      BoltDevice *dev = g_ptr_array_index (devices, i);
      const char *path;
      gboolean found;

      path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (dev));
      found = devices_table_transfer_entry (old, self->devices, path);

      if (found)
        continue;

      cc_bolt_page_add_device (self, dev);
    }

  devices_table_clear_entries (old, self);
  gtk_stack_set_visible_child_name (self->container, "devices-listing");
}

static gboolean
list_box_sync_visible (GtkListBox *listbox)
{
  GtkWidget *child;
  gboolean show;

  child = gtk_widget_get_first_child (GTK_WIDGET (listbox));
  show = child != NULL;

  gtk_widget_set_visible (GTK_WIDGET (listbox), show);

  return show;
}

static GtkWidget *
cc_bolt_page_box_for_listbox (CcBoltPage *self,
                               GtkListBox *lstbox)
{
  if ((gpointer) lstbox == self->devices_list)
    return GTK_WIDGET (self->devices_box);
  else if ((gpointer) lstbox == self->pending_list)
    return GTK_WIDGET (self->pending_box);

  g_return_val_if_reached (NULL);
}

static CcBoltDeviceEntry *
cc_bolt_page_add_device (CcBoltPage *self,
                          BoltDevice *dev)
{
  CcBoltDeviceEntry *entry;
  BoltDeviceType type;
  BoltStatus status;
  const char *path;

  type = bolt_device_get_device_type (dev);

  if (type != BOLT_DEVICE_PERIPHERAL)
    return FALSE;

  entry = cc_bolt_device_entry_new (dev, FALSE);
  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (dev));

  /* add to the list box */
  status = bolt_device_get_status (dev);

  if (bolt_status_is_pending (status))
    {
      gtk_list_box_append (self->pending_list, GTK_WIDGET (entry));
      gtk_widget_set_visible (GTK_WIDGET (self->pending_list), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->pending_box), TRUE);
    }
  else
    {
      gtk_list_box_append (self->devices_list, GTK_WIDGET (entry));
      gtk_widget_set_visible (GTK_WIDGET (self->devices_list), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->devices_box), TRUE);
    }

  g_signal_connect_object (entry,
                           "status-changed",
                           G_CALLBACK (on_device_entry_status_changed_cb),
                           self,
                           0);

  gtk_stack_set_visible_child_name (self->devices_stack, "have-devices");
  g_hash_table_insert (self->devices, (gpointer) path, entry);

  return entry;
}

static void
cc_bolt_page_del_device_entry (CcBoltPage        *self,
                                CcBoltDeviceEntry *entry)
{
  BoltDevice *dev;
  GtkWidget *box;
  GtkWidget *p;
  gboolean show;

  dev = cc_bolt_device_entry_get_device (entry);
  if (cc_bolt_device_dialog_device_equal (self->device_dialog, dev))
    {
      gtk_window_close (GTK_WINDOW (self->device_dialog));
      cc_bolt_device_dialog_set_device (self->device_dialog, NULL, NULL);
    }

  p = gtk_widget_get_parent (GTK_WIDGET (entry));
  gtk_list_box_remove (GTK_LIST_BOX (p), GTK_WIDGET (entry));

  box = cc_bolt_page_box_for_listbox (self, GTK_LIST_BOX (p));
  show = list_box_sync_visible (GTK_LIST_BOX (p));
  gtk_widget_set_visible (box, show);

  if (!gtk_widget_is_visible (GTK_WIDGET (self->pending_list)) &&
      !gtk_widget_is_visible (GTK_WIDGET (self->devices_list)))
    {
      gtk_stack_set_visible_child_name (self->devices_stack, "no-devices");
    }
}

static void
cc_bolt_page_authmode_sync (CcBoltPage *self)
{
  BoltClient *client = self->client;
  BoltAuthMode mode;
  gboolean enabled;

  mode = bolt_client_get_authmode (client);
  enabled = (mode & BOLT_AUTH_ENABLED) != 0;

  g_signal_handlers_block_by_func (self->authmode_switch, on_authmode_state_set_cb, self);

  gtk_switch_set_active (self->authmode_switch, enabled);

  g_signal_handlers_unblock_by_func (self->authmode_switch, on_authmode_state_set_cb, self);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->direct_access_row),
                                 enabled ?
                                 _("Allow direct access to devices such as docks and external GPUs.") :
                                 _("Only USB and Display Port devices can attach."));
}

static void
cc_panel_list_box_migrate (CcBoltPage        *self,
                           GtkListBox        *from,
                           GtkListBox        *to,
                           CcBoltDeviceEntry *entry)
{
  GtkWidget *from_box;
  GtkWidget *to_box;
  gboolean show;
  GtkWidget *target;

  target = GTK_WIDGET (entry);

  gtk_list_box_remove (from, target);
  gtk_list_box_append (to, target);
  gtk_widget_set_visible (GTK_WIDGET (to), TRUE);

  from_box = cc_bolt_page_box_for_listbox (self, from);
  to_box = cc_bolt_page_box_for_listbox (self, to);

  show = list_box_sync_visible (from);
  gtk_widget_set_visible (from_box, show);
  gtk_widget_set_visible (to_box, TRUE);
}

/* bolt client signals */
static void
cc_bolt_page_set_no_thunderbolt (CcBoltPage *self,
                                  const char *msg)
{
  if (!msg)
    {
      msg = _("Thunderbolt could not be detected.\n"
              "Either the system lacks Thunderbolt support, "
              "it has been disabled in the BIOS or is set to "
              "an unsupported security level in the BIOS.");
    }

  adw_status_page_set_description (self->notb_page, msg);
  gtk_stack_set_visible_child_name (self->container, "no-thunderbolt");
}

static void
cc_bolt_page_name_owner_changed (CcBoltPage *self)
{
  g_autofree char *name_owner = NULL;
  BoltClient *client = self->client;
  BoltSecurity sl;
  gboolean notb = TRUE;
  const char *text = NULL;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (self->client));

  if (name_owner == NULL)
    {
      cc_bolt_page_set_no_thunderbolt (self, NULL);
      devices_table_clear_entries (self->devices, self);
      gtk_widget_set_visible (GTK_WIDGET (self->headerbar_box), FALSE);
      return;
    }

  gtk_stack_set_visible_child_name (self->container, "loading");

  sl = bolt_client_get_security (client);

  switch (sl)
    {
    case BOLT_SECURITY_NONE:
    case BOLT_SECURITY_SECURE:
    case BOLT_SECURITY_USER:
      /* we fetch the device list and show them here */
      notb = FALSE;
      break;

    case BOLT_SECURITY_DPONLY:
    case BOLT_SECURITY_USBONLY:
      text = _("Thunderbolt support has been disabled in the BIOS.");
      break;

    case BOLT_SECURITY_UNKNOWN:
      text = _("Thunderbolt security level could not be determined.");;
      break;
    }

  if (notb)
    {
      /* security level is unknown or un-handled */
      cc_bolt_page_set_no_thunderbolt (self, text);
      return;
    }

  if (self->permission)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->headerbar_box), TRUE);
    }
  else
    {
      polkit_permission_new ("org.freedesktop.bolt.manage",
                             NULL,
                             self->cancellable,
                             on_permission_ready,
                             g_object_ref (self));
    }

  devices_table_synchronize (self);
}

/* bolt client signals */
static void
on_bolt_name_owner_changed_cb (GObject    *object,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  cc_bolt_page_name_owner_changed (CC_BOLT_PAGE (user_data));
}

static void
on_bolt_device_added_cb (BoltClient *cli,
                         const char *path,
                         CcBoltPage *self)
{
  g_autoptr(GError) err = NULL;
  GDBusConnection *bus;
  BoltDevice *dev;
  gboolean found;

  found = g_hash_table_contains (self->devices, path);

  if (found)
    return;

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (self->client));
  dev = bolt_device_new_for_object_path (bus, path, self->cancellable, &err);

  if (!dev)
    {
      g_warning ("Could not create proxy for %s", path);
      return;
    }

  cc_bolt_page_add_device (self, dev);
}

static void
on_bolt_device_removed_cb (BoltClient *cli,
                           const char *path,
                           CcBoltPage *self)
{
  CcBoltDeviceEntry *entry;

  entry = g_hash_table_lookup (self->devices, path);

  if (!entry)
    return;

  cc_bolt_page_del_device_entry (self, entry);
  g_hash_table_remove (self->devices, path);
}

static void
on_bolt_notify_authmode_cb (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  cc_bolt_page_authmode_sync (CC_BOLT_PAGE (user_data));
}

/* panel signals */

static void
on_authmode_ready (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  BoltClient *client = BOLT_CLIENT (source_object);
  CcBoltPage *self;
  gboolean ok;

  ok = bolt_client_set_authmode_finish (client, res, &error);
  if (ok)
    {
      BoltAuthMode mode;
      gboolean enabled;

      self = CC_BOLT_PAGE (user_data);
      mode = bolt_client_get_authmode (client);
      enabled = (mode & BOLT_AUTH_ENABLED) != 0;
      gtk_switch_set_state (self->authmode_switch, enabled);
    }
  else
    {
      g_autofree char *text = NULL;

      g_warning ("Could not set authmode: %s", error->message);

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      self = CC_BOLT_PAGE (user_data);
      text = g_strdup_printf (_("Error switching direct mode: %s"), error->message);
      gtk_label_set_markup (self->notification_label, text);
      gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

      /* make sure we are reflecting the correct state */
      cc_bolt_page_authmode_sync (self);
    }

  gtk_spinner_stop (self->authmode_spinner);
  gtk_widget_set_sensitive (GTK_WIDGET (self->authmode_switch), TRUE);
}

static gboolean
on_authmode_state_set_cb (CcBoltPage *self,
                          gboolean    enable,
                          GtkSwitch  *toggle)
{
  BoltClient *client = self->client;
  BoltAuthMode mode;

  gtk_widget_set_sensitive (GTK_WIDGET (self->authmode_switch), FALSE);
  gtk_spinner_start (self->authmode_spinner);

  mode = bolt_client_get_authmode (client);

  if (enable)
    mode = mode | BOLT_AUTH_ENABLED;
  else
    mode = mode & ~BOLT_AUTH_ENABLED;

  bolt_client_set_authmode_async (client, mode, NULL, on_authmode_ready, self);

  return TRUE;
}

static void
on_device_entry_row_activated_cb (CcBoltPage    *self,
                                  GtkListBoxRow *row)
{
  g_autoptr(GPtrArray) parents = NULL;
  CcBoltDeviceEntry *entry;
  BoltDevice *device;
  BoltDevice *iter;
  const char *parent;

  if (!CC_IS_BOLT_DEVICE_ENTRY (row))
    return;

  entry = CC_BOLT_DEVICE_ENTRY (row);
  device = cc_bolt_device_entry_get_device (entry);

  /* walk up the chain and collect all parents */
  parents = g_ptr_array_new_with_free_func (g_object_unref);
  iter = device;

  parent = bolt_device_get_parent (iter);
  while (parent != NULL)
    {
      g_autofree char *path = NULL;
      CcBoltDeviceEntry *child;
      BoltDevice *dev;

      path = bolt_gen_object_path (BOLT_DBUS_PATH_DEVICES, parent);

      /* NB: the host device is not a peripheral and thus not
       * in the hash table; therefore when get a NULL back, we
       * should have reached the end of the chain */
      child = g_hash_table_lookup (self->devices, path);
      if (!child)
	break;

      dev = cc_bolt_device_entry_get_device (child);
      g_ptr_array_add (parents, g_object_ref (dev));
      iter = dev;

      parent = bolt_device_get_parent (iter);
    }

  cc_bolt_device_dialog_set_device (self->device_dialog, device, parents);

  gtk_window_set_default_size (GTK_WINDOW (self->device_dialog), 1, 1);
  gtk_window_present (GTK_WINDOW (self->device_dialog));
}

static void
on_device_entry_status_changed_cb (CcBoltDeviceEntry *entry,
                                   BoltStatus         new_status,
                                   CcBoltPage        *self)
{
  GtkListBox *from = NULL;
  GtkListBox *to = NULL;
  GtkWidget *p;
  gboolean is_pending;
  gboolean parent_pending;

  /* if we are doing some active work, then lets not change
   * the list the entry is in; otherwise we might just hop
   * from one box to the other and back again.
   */
  if (new_status == BOLT_STATUS_CONNECTING || new_status == BOLT_STATUS_AUTHORIZING)
    return;

  is_pending = bolt_status_is_pending (new_status);

  p = gtk_widget_get_parent (GTK_WIDGET (entry));
  parent_pending = (gpointer) p == self->pending_list;

  /*  */
  if (is_pending && !parent_pending)
    {
      from = self->devices_list;
      to = self->pending_list;
    }
  else if (!is_pending && parent_pending)
    {
      from = self->pending_list;
      to = self->devices_list;
    }

  if (from && to)
    cc_panel_list_box_migrate (self, from, to, entry);
}


static void
on_notification_button_clicked_cb (CcBoltPage *self)
{
  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
}

/* polkit */

static void
on_permission_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  g_autoptr(CcBoltPage) self = user_data;
  g_autoptr(GError) err = NULL;
  GPermission *permission;
  gboolean is_allowed;
  const char *name;

  permission = polkit_permission_new_finish (res, &err);
  self->permission = permission;

  if (!self->permission)
    {
      g_warning ("Could not get polkit permissions: %s", err->message);
      return;
    }

  g_signal_connect_object (permission,
                           "notify",
                           G_CALLBACK (on_permission_notify_cb),
                           self,
                           G_CONNECT_AFTER);

  is_allowed = g_permission_get_allowed (permission);
  gtk_widget_set_sensitive (GTK_WIDGET (self->authmode_switch), is_allowed);
  gtk_lock_button_set_permission (self->lock_button, permission);

  name = gtk_stack_get_visible_child_name (self->container);

  gtk_widget_set_visible (GTK_WIDGET (self->headerbar_box),
                          bolt_streq (name, "devices-listing"));
}

static void
on_permission_notify_cb (GPermission *permission,
                         GParamSpec  *pspec,
                         CcBoltPage  *self)
{
  gboolean is_allowed = g_permission_get_allowed (permission);

  gtk_widget_set_sensitive (GTK_WIDGET (self->authmode_switch), is_allowed);
}

static gint
device_entries_sort_by_recency_cb (GtkListBoxRow *a_row,
                                   GtkListBoxRow *b_row,
                                   gpointer       user_data)
{
  CcBoltDeviceEntry *a_entry = CC_BOLT_DEVICE_ENTRY (a_row);
  CcBoltDeviceEntry *b_entry = CC_BOLT_DEVICE_ENTRY (b_row);
  BoltDevice *a = cc_bolt_device_entry_get_device (a_entry);
  BoltDevice *b = cc_bolt_device_entry_get_device (b_entry);
  BoltStatus status;
  gint64 a_ts, b_ts;
  gint64 score;

  a_ts = (gint64) bolt_device_get_timestamp (a);
  b_ts = (gint64) bolt_device_get_timestamp (b);

  score = b_ts - a_ts;

  if (score != 0)
    return score;

  status = bolt_device_get_status (a);

  if (bolt_status_is_connected (status))
    {
      const char *a_path;
      const char *b_path;

      a_path = bolt_device_get_syspath (a);
      b_path = bolt_device_get_syspath (b);

      return g_strcmp0 (a_path, b_path);
    }
  else
    {
      const char *a_name;
      const char *b_name;

      a_name = bolt_device_get_name (a);
      b_name = bolt_device_get_name (b);

      return g_strcmp0 (a_name, b_name);
    }

  return 0;
}

static gint
device_entries_sort_by_syspath_cb (GtkListBoxRow *a_row,
                                   GtkListBoxRow *b_row,
                                   gpointer       user_data)
{
  CcBoltDeviceEntry *a_entry = CC_BOLT_DEVICE_ENTRY (a_row);
  CcBoltDeviceEntry *b_entry = CC_BOLT_DEVICE_ENTRY (b_row);
  BoltDevice *a = cc_bolt_device_entry_get_device (a_entry);
  BoltDevice *b = cc_bolt_device_entry_get_device (b_entry);

  const char *a_path;
  const char *b_path;

  a_path = bolt_device_get_syspath (a);
  b_path = bolt_device_get_syspath (b);

  return g_strcmp0 (a_path, b_path);
}

/* GObject overrides */

static void
cc_bolt_page_finalize (GObject *object)
{
  CcBoltPage *self = CC_BOLT_PAGE (object);

  g_clear_object (&self->client);
  g_clear_pointer (&self->devices, g_hash_table_unref);
  g_clear_object (&self->permission);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (cc_bolt_page_parent_class)->finalize (object);
}

static void
cc_bolt_page_dispose (GObject *object)
{
  CcBoltPage *self = CC_BOLT_PAGE (object);

  /* Must be destroyed in dispose, not finalize. */
  cc_bolt_device_dialog_set_device (self->device_dialog, NULL, NULL);
  g_clear_pointer ((GtkWindow **) &self->device_dialog, gtk_window_destroy);

  G_OBJECT_CLASS (cc_bolt_page_parent_class)->dispose (object);
}

static void
cc_bolt_page_constructed (GObject *object)
{
  CcBoltPage *self = CC_BOLT_PAGE (object);
  GtkWindow *parent;

  G_OBJECT_CLASS (cc_bolt_page_parent_class)->constructed (object);

  parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_window_set_transient_for (GTK_WINDOW (self->device_dialog), parent);
}

static void
cc_bolt_page_class_init (CcBoltPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_bolt_page_constructed;
  object_class->dispose = cc_bolt_page_dispose;
  object_class->finalize = cc_bolt_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-bolt-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, authmode_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, authmode_switch);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, container);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, devices_list);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, devices_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, devices_stack);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, direct_access_row);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, headerbar_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, lock_button);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, notb_page);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, pending_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPage, pending_list);

  gtk_widget_class_bind_template_callback (widget_class, on_notification_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_authmode_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_entry_row_activated_cb);
}

static void
cc_bolt_page_init (CcBoltPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Startup invisible as the visibility will be set as a result of BoltClient
   * succeeding its connection to boltd. */
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  self->cancellable = g_cancellable_new ();
  bolt_client_new_async (self->cancellable, on_visibility_client_ready, self);

  gtk_stack_set_visible_child_name (self->container, "loading");

  gtk_list_box_set_sort_func (self->devices_list,
                              device_entries_sort_by_recency_cb,
                              self,
                              NULL);

  gtk_list_box_set_sort_func (self->pending_list,
                              device_entries_sort_by_syspath_cb,
                              self,
                              NULL);

  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

  self->device_dialog = cc_bolt_device_dialog_new ();

  bolt_client_new_async (self->cancellable, bolt_client_ready, g_object_ref (self));
}

CcBoltPage *
cc_bolt_page_new (void)
{
  return g_object_new (CC_TYPE_BOLT_PAGE, NULL);
}
