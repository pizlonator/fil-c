/*
 * Copyright © 2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-wifi-connection-list.h"
#include "cc-wifi-connection-row.h"

struct _CcWifiConnectionList
{
  AdwBin         parent_instance;

  GtkListBox    *listbox;

  guint          freeze_count;
  gboolean       updating;

  gboolean       checkable;
  gboolean       forgettable;
  gboolean       hide_unavailable;
  gboolean       show_aps;

  NMClient      *client;
  NMDeviceWifi  *device;

  NMConnection  *last_active;

  GPtrArray     *connections;
  GPtrArray     *connections_row;

  /* AP SSID cache stores the APs SSID used for assigning it to a row.
   * This is necessary to efficiently remove it when its SSID changes.
   *
   * Note that we only group APs that cannot be assigned to a connection
   * by the SSID. In principle this is wrong, because other attributes may
   * be different rendering them separate networks.
   * In practice this will almost never happen, and if it does, we just
   * show and select the strongest AP.
   */
  GHashTable    *ap_ssid_cache;
  GHashTable    *ssid_to_row;
};

static void on_device_ap_added_cb   (CcWifiConnectionList *self,
                                     NMAccessPoint        *ap,
                                     NMDeviceWifi         *device);
static void on_device_ap_removed_cb (CcWifiConnectionList *self,
                                     NMAccessPoint        *ap,
                                     NMDeviceWifi         *device);
static void on_row_configured_cb    (CcWifiConnectionList *self,
                                     CcWifiConnectionRow  *row);
static void on_row_forget_cb        (CcWifiConnectionList *self,
                                     CcWifiConnectionRow  *row);
static void on_row_show_qr_code_cb (CcWifiConnectionList *self,
                                    CcWifiConnectionRow  *row);

G_DEFINE_TYPE (CcWifiConnectionList, cc_wifi_connection_list, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_CHECKABLE,
  PROP_HIDE_UNAVAILABLE,
  PROP_SHOW_APS,
  PROP_CLIENT,
  PROP_DEVICE,
  PROP_FORGETTABLE,
  PROP_LAST
};

static GParamSpec *props [PROP_LAST];

static GBytes*
new_hashable_ssid (GBytes *ssid)
{
  GBytes *res;
  const guint8 *data;
  gsize size;

  /* This is what nm_utils_same_ssid does, but returning it so that we can
   * use the result in other ways (i.e. hash table lookups). */
  data = g_bytes_get_data ((GBytes*) ssid, &size);
  if (data[size-1] == '\0')
    size -= 1;
  res = g_bytes_new (data, size);

  return res;
}

static gboolean
connection_ignored (NMConnection *connection)
{
  NMSettingWireless *sw;

  /* Ignore AP and adhoc modes (i.e. accept infrastructure or empty) */
  sw = nm_connection_get_setting_wireless (connection);
  if (!sw)
    return TRUE;
  if (g_strcmp0 (nm_setting_wireless_get_mode (sw), "adhoc") == 0 ||
      g_strcmp0 (nm_setting_wireless_get_mode (sw), "ap") == 0)
    {
      return TRUE;
    }

  return FALSE;
}

static CcWifiConnectionRow*
cc_wifi_connection_list_row_add (CcWifiConnectionList *self,
                                 NMConnection         *connection,
                                 NMAccessPoint        *ap,
                                 gboolean              known_connection)
{
  CcWifiConnectionRow *res;
  g_autoptr(GPtrArray) aps = NULL;

  if (ap)
    {
      aps = g_ptr_array_new ();
      g_ptr_array_add (aps, ap);
    }

  res = cc_wifi_connection_row_new (self->device,
                                    connection,
                                    aps,
                                    self->checkable,
                                    known_connection,
                                    self->forgettable);
  gtk_list_box_append (self->listbox, GTK_WIDGET (res));

  g_signal_connect_object (res, "configure", G_CALLBACK (on_row_configured_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (res, "forget", G_CALLBACK (on_row_forget_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (res, "show-qr-code", G_CALLBACK (on_row_show_qr_code_cb), self, G_CONNECT_SWAPPED);

  g_signal_emit_by_name (self, "add-row", res);

  return res;
}

static void
clear_widget (CcWifiConnectionList *self)
{
  const GPtrArray *aps;
  GHashTableIter iter;
  CcWifiConnectionRow *row;
  gint i;

  /* Clear everything; disconnect all AP signals first */
  aps = nm_device_wifi_get_access_points (self->device);
  for (i = 0; i < aps->len; i++)
    g_signal_handlers_disconnect_by_data (g_ptr_array_index (aps, i), self);

  /* Remove all AP only rows */
  g_hash_table_iter_init (&iter, self->ssid_to_row);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &row))
    {
      g_hash_table_iter_remove (&iter);
      g_signal_emit_by_name (self, "remove-row", row);
      gtk_list_box_remove (self->listbox, GTK_WIDGET (row));
    }

  /* Remove all connection rows */
  for (i = 0; i < self->connections_row->len; i++)
    {
      if (!g_ptr_array_index (self->connections_row, i))
        continue;

      row = g_ptr_array_index (self->connections_row, i);
      g_ptr_array_index (self->connections_row, i) = NULL;
      g_signal_emit_by_name (self, "remove-row", row);
      gtk_list_box_remove (self->listbox, GTK_WIDGET (row));
    }

  /* Reset the internal state */
  g_ptr_array_set_size (self->connections, 0);
  g_ptr_array_set_size (self->connections_row, 0);
  g_hash_table_remove_all (self->ssid_to_row);
  g_hash_table_remove_all (self->ap_ssid_cache);
}

static void
update_connections (CcWifiConnectionList *self)
{
  const GPtrArray *aps;
  const GPtrArray *acs_client;
  g_autoptr(GPtrArray) acs = NULL;
  NMActiveConnection *ac;
  NMConnection *ac_con = NULL;
  gint i;

  /* We don't want full UI rebuilds during some UI interactions, so allow freezing the list. */
  if (self->freeze_count > 0)
    return;

  /* Prevent recursion (maybe move this into an idle handler instead?) */
  if (self->updating)
    return;
  self->updating = TRUE;

  clear_widget (self);

  /* Copy the new connections; also create a row if we show unavailable
   * connections */
  acs_client = nm_client_get_connections (self->client);

  acs = g_ptr_array_new_full (acs_client->len + 1, NULL);
  for (i = 0; i < acs_client->len; i++)
    g_ptr_array_add (acs, g_ptr_array_index (acs_client, i));

  ac = nm_device_get_active_connection (NM_DEVICE (self->device));
  if (ac)
    ac_con = NM_CONNECTION (nm_active_connection_get_connection (ac));

  if (ac_con && !g_ptr_array_find (acs, ac_con, NULL))
    {
      g_debug ("Adding remote connection for active connection");
      g_ptr_array_add (acs, g_object_ref (ac_con));
    }

  for (i = 0; i < acs->len; i++)
    {
      NMConnection *con;

      con = g_ptr_array_index (acs, i);
      if (connection_ignored (con))
        continue;

      g_ptr_array_add (self->connections, g_object_ref (con));
      if (self->hide_unavailable && con != ac_con)
        g_ptr_array_add (self->connections_row, NULL);
      else
        g_ptr_array_add (self->connections_row,
                         cc_wifi_connection_list_row_add (self, con,
                         NULL, TRUE));
    }

  /* Coldplug all known APs again */
  aps = nm_device_wifi_get_access_points (self->device);
  for (i = 0; i < aps->len; i++)
    on_device_ap_added_cb (self, g_ptr_array_index (aps, i), self->device);

  self->updating = FALSE;
}

static void
on_row_configured_cb (CcWifiConnectionList *self, CcWifiConnectionRow *row)
{
  g_signal_emit_by_name (self, "configure", row);
}

static void
on_row_forget_cb (CcWifiConnectionList *self, CcWifiConnectionRow *row)
{
  g_signal_emit_by_name (self, "forget", row);
}

static void
on_row_show_qr_code_cb (CcWifiConnectionList *self, CcWifiConnectionRow *row)
{
  g_signal_emit_by_name (self, "show_qr_code", row);
}

static void
on_access_point_property_changed (CcWifiConnectionList *self,
                                  GParamSpec           *pspec,
                                  NMAccessPoint        *ap)
{
  CcWifiConnectionRow *row;
  GBytes *ssid;
  gboolean has_connection = FALSE;
  gint i;

  /* If the SSID changed then the AP needs to be added/removed from rows.
   * Do this by simulating an AP addition/removal.  */
  if (g_str_equal (pspec->name, NM_ACCESS_POINT_SSID))
    {
      g_debug ("Simulating add/remove for SSID change");
      on_device_ap_removed_cb (self, ap, self->device);
      on_device_ap_added_cb (self, ap, self->device);
      return;
    }

  /* Otherwise, find all rows that contain the AP and update it. Do this by
   * first searching all rows with connections, and then looking it up in the
   * SSID rows if not found. */
  for (i = 0; i < self->connections_row->len; i++)
    {
      row = g_ptr_array_index (self->connections_row, i);
      if (row && cc_wifi_connection_row_has_access_point (row, ap))
        {
          cc_wifi_connection_row_update (row);
          has_connection = TRUE;
        }
    }

  if (!self->show_aps || has_connection)
    return;

  ssid = g_hash_table_lookup (self->ap_ssid_cache, ap);
  if (!ssid)
    return;

  row = g_hash_table_lookup (self->ssid_to_row, ssid);
  if (!row)
    g_assert_not_reached ();
  else
    cc_wifi_connection_row_update (row);
}

static void
on_device_ap_added_cb (CcWifiConnectionList *self,
                       NMAccessPoint        *ap,
                       NMDeviceWifi         *device)
{
  g_autoptr(GPtrArray) connections = NULL;
  NM80211ApSecurityFlags rsn_flags;
  CcWifiConnectionRow *row;
  GBytes *ap_ssid;
  g_autoptr(GBytes) ssid = NULL;
  guint i, j;

  g_signal_connect_object (ap, "notify",
                           G_CALLBACK (on_access_point_property_changed),
                           self, G_CONNECT_SWAPPED);

  connections = nm_access_point_filter_connections (ap, self->connections);

  /* If this is the active AP, then add the active connection to the list. This
   * is a workaround because nm_access_pointer_filter_connections() will not
   * include it otherwise.
   * So it seems like the dummy AP entry that NM creates internally is not actually
   * compatible with the connection that is being activated.
   */
  if (ap == nm_device_wifi_get_active_access_point (device))
    {
      NMActiveConnection *ac;
      NMConnection *ac_con;

      ac = nm_device_get_active_connection (NM_DEVICE (self->device));

      if (ac)
        {
          guint idx;

          ac_con = NM_CONNECTION (nm_active_connection_get_connection (ac));

          if (!g_ptr_array_find (connections, ac_con, NULL) &&
              g_ptr_array_find (self->connections, ac_con, &idx))
            {
              g_debug ("Adding active connection to list of valid connections for AP");
              g_ptr_array_add (connections, g_object_ref (ac_con));
            }
        }
    }

  /* Add the AP to all connection related rows, creating the row if neccessary. */
  for (i = 0; i < connections->len; i++)
    {
      gboolean found = g_ptr_array_find (self->connections, g_ptr_array_index (connections, i), &j);

      g_assert (found);

      row = g_ptr_array_index (self->connections_row, j);
      if (!row)
        row = cc_wifi_connection_list_row_add (self, g_ptr_array_index (connections, i), NULL, TRUE);
      cc_wifi_connection_row_add_access_point (row, ap);
      g_ptr_array_index (self->connections_row, j) = row;
    }

  if (connections->len > 0)
    return;

  if (!self->show_aps)
    return;

  /* The AP is not compatible to any known connection, generate an entry for the
   * SSID or add to existing one. However, not for hidden APs that don't have an SSID
   * or a hidden OWE transition network.
   */
  ap_ssid = nm_access_point_get_ssid (ap);
  if (ap_ssid == NULL)
    return;

  /* Skip OWE-TM network with OWE RSN */
  rsn_flags = nm_access_point_get_rsn_flags (ap);
  if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE && rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM)
    return;

  ssid = new_hashable_ssid (ap_ssid);

  g_hash_table_insert (self->ap_ssid_cache, ap, g_bytes_ref (ssid));

  row = g_hash_table_lookup (self->ssid_to_row, ssid);
  if (!row)
    {
      row = cc_wifi_connection_list_row_add (self, NULL, ap, FALSE);

      g_hash_table_insert (self->ssid_to_row, g_bytes_ref (ssid), row);
    }
  else
    {
      cc_wifi_connection_row_add_access_point (row, ap);
    }
}

static void
on_device_ap_removed_cb (CcWifiConnectionList *self,
                         NMAccessPoint        *ap,
                         NMDeviceWifi         *device)
{
  CcWifiConnectionRow *row;
  g_autoptr(GBytes) ssid = NULL;
  gboolean found = FALSE;
  gint i;

  g_signal_handlers_disconnect_by_data (ap, self);

  /* Find any connection related row with the AP and remove the AP from it. Remove the
   * row if it was the last AP and we are hiding unavailable connections. */
  for (i = 0; i < self->connections_row->len; i++)
    {
      row = g_ptr_array_index (self->connections_row, i);
      if (row && cc_wifi_connection_row_remove_access_point (row, ap))
        {
          found = TRUE;

          if (self->hide_unavailable)
            {
              g_ptr_array_index (self->connections_row, i) = NULL;
              g_signal_emit_by_name (self, "remove-row", row);
              gtk_list_box_remove (self->listbox, GTK_WIDGET (row));
            }
        }
    }

  if (found || !self->show_aps)
    return;

  /* If the AP was inserted into a row without a connection, then we will get an
   * SSID for it here. */
  g_hash_table_steal_extended (self->ap_ssid_cache, ap, NULL, (gpointer*) &ssid);
  if (!ssid)
    return;

  /* And we can update the row (possibly removing it) */
  row = g_hash_table_lookup (self->ssid_to_row, ssid);
  g_assert (row != NULL);

  if (cc_wifi_connection_row_remove_access_point (row, ap))
    {
      g_hash_table_remove (self->ssid_to_row, ssid);
      g_signal_emit_by_name (self, "remove-row", row);
      gtk_list_box_remove (self->listbox, GTK_WIDGET (row));
    }
}

static void
on_client_connection_added_cb (CcWifiConnectionList *self,
                               NMConnection         *connection,
                               NMClient             *client)
{
  if (!nm_device_connection_compatible (NM_DEVICE (self->device), connection, NULL))
    return;

  if (connection_ignored (connection))
    return;

  /* The approach we take to handle connection changes is to do a full rebuild.
   * It happens seldom enough to make this feasible.
   */
  update_connections (self);
}

static void
on_client_connection_removed_cb (CcWifiConnectionList *self,
                                 NMConnection         *connection,
                                 NMClient             *client)
{
  if (!g_ptr_array_find (self->connections, connection, NULL))
    return;

  /* The approach we take to handle connection changes is to do a full rebuild.
   * It happens seldom enough to make this feasible.
   */
  update_connections (self);
}

static void
on_device_state_changed_cb (CcWifiConnectionList *self,
                            GParamSpec           *pspec,
                            NMDeviceWifi         *device)
{
  NMActiveConnection *ac;
  NMConnection *connection = NULL;
  guint idx;

  ac = nm_device_get_active_connection (NM_DEVICE (self->device));
  if (ac)
    connection = NM_CONNECTION (nm_active_connection_get_connection (ac));

  /* Just update the corresponding row if the AC is still the same. */
  if (self->last_active == connection &&
      g_ptr_array_find (self->connections, connection, &idx) &&
      g_ptr_array_index (self->connections_row, idx))
    {
      cc_wifi_connection_row_update (g_ptr_array_index (self->connections_row, idx));
      return;
    }

  /* Give up and do a full update. */
  update_connections (self);
  self->last_active = connection;
}

static void
on_device_active_ap_changed_cb (CcWifiConnectionList *self,
                                GParamSpec           *pspec,
                                NMDeviceWifi         *device)
{
  NMAccessPoint *ap;
  /* We need to make sure the active AP is grouped with the active connection.
   * Do so by simply removing and adding it.
   *
   * This is necessary because the AP is added before this property
   * is updated. */
  ap = nm_device_wifi_get_active_access_point (self->device);
  if (ap)
    {
      g_debug ("Simulating add/remove for active AP change");
      on_device_ap_removed_cb (self, ap, self->device);
      on_device_ap_added_cb (self, ap, self->device);
    }
}

static void
cc_wifi_connection_list_dispose (GObject *object)
{
  CcWifiConnectionList *self = (CcWifiConnectionList *)object;

  /* Prevent any further updates; clear_widget must not indirectly recurse
   * through updates_connections */
  self->updating = TRUE;

  /* Drop all external references */
  clear_widget (self);

  G_OBJECT_CLASS (cc_wifi_connection_list_parent_class)->dispose (object);
}

static void
cc_wifi_connection_list_finalize (GObject *object)
{
  CcWifiConnectionList *self = (CcWifiConnectionList *)object;

  g_clear_object (&self->client);
  g_clear_object (&self->device);

  g_clear_pointer (&self->connections, g_ptr_array_unref);
  g_clear_pointer (&self->connections_row, g_ptr_array_unref);
  g_clear_pointer (&self->ssid_to_row, g_hash_table_unref);
  g_clear_pointer (&self->ap_ssid_cache, g_hash_table_unref);

  G_OBJECT_CLASS (cc_wifi_connection_list_parent_class)->finalize (object);
}

static void
cc_wifi_connection_list_constructed (GObject *object)
{
  CcWifiConnectionList *self = (CcWifiConnectionList *)object;

  G_OBJECT_CLASS (cc_wifi_connection_list_parent_class)->constructed (object);

  g_assert (self->client);
  g_assert (self->device);

  g_signal_connect_object (self->client, "connection-added",
                           G_CALLBACK (on_client_connection_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, "connection-removed",
                           G_CALLBACK (on_client_connection_removed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->device, "access-point-added",
                           G_CALLBACK (on_device_ap_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->device, "access-point-removed",
                           G_CALLBACK (on_device_ap_removed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->device, "notify::state",
                           G_CALLBACK (on_device_state_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->device, "notify::active-connection",
                           G_CALLBACK (on_device_state_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->device, "notify::active-access-point",
                           G_CALLBACK (on_device_active_ap_changed_cb),
                           self, G_CONNECT_SWAPPED);
  on_device_state_changed_cb (self, NULL, self->device);

  /* Simulate a change notification on the available connections.
   * This uses the implementation detail that the list is rebuild
   * completely in this case. */
  update_connections (self);
}

static void
cc_wifi_connection_list_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcWifiConnectionList *self = CC_WIFI_CONNECTION_LIST (object);

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      g_value_set_boolean (value, self->checkable);
      break;

    case PROP_HIDE_UNAVAILABLE:
      g_value_set_boolean (value, self->hide_unavailable);
      break;

    case PROP_SHOW_APS:
      g_value_set_boolean (value, self->show_aps);
      break;

    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_FORGETTABLE:
      g_value_set_boolean (value, self->forgettable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_list_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcWifiConnectionList *self = CC_WIFI_CONNECTION_LIST (object);

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      self->checkable = g_value_get_boolean (value);
      break;

    case PROP_HIDE_UNAVAILABLE:
      self->hide_unavailable = g_value_get_boolean (value);
      break;

    case PROP_SHOW_APS:
      self->show_aps = g_value_get_boolean (value);
      break;

    case PROP_CLIENT:
      self->client = g_value_dup_object (value);
      break;

    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    case PROP_FORGETTABLE:
      self->forgettable = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_list_class_init (CcWifiConnectionListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = cc_wifi_connection_list_constructed;
  object_class->dispose = cc_wifi_connection_list_dispose;
  object_class->finalize = cc_wifi_connection_list_finalize;
  object_class->get_property = cc_wifi_connection_list_get_property;
  object_class->set_property = cc_wifi_connection_list_set_property;

  props[PROP_CHECKABLE] =
    g_param_spec_boolean ("checkable", "checkable",
                          "Passed to the created rows to show/hide the checkbox for deletion",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_HIDE_UNAVAILABLE] =
    g_param_spec_boolean ("hide-unavailable", "HideUnavailable",
                          "Whether to show or hide unavailable connections",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_SHOW_APS] =
    g_param_spec_boolean ("show-aps", "ShowAPs",
                          "Whether to show available SSIDs/APs without a connection",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CLIENT] =
    g_param_spec_object ("client", "NMClient",
                         "The NM Client",
                         NM_TYPE_CLIENT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_DEVICE] =
    g_param_spec_object ("device", "WiFi Device",
                         "The WiFi Device for this connection list",
                         NM_TYPE_DEVICE_WIFI,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  props[PROP_FORGETTABLE] =
      g_param_spec_boolean ("forgettable", "forgettable",
                           "Passed to the created rows to show/hide the checkbox for deletion",
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     PROP_LAST,
                                     props);

  g_signal_new ("configure",
                CC_TYPE_WIFI_CONNECTION_LIST,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, CC_TYPE_WIFI_CONNECTION_ROW);
  g_signal_new ("show_qr_code",
                CC_TYPE_WIFI_CONNECTION_LIST,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, CC_TYPE_WIFI_CONNECTION_ROW);
  g_signal_new ("forget",
               CC_TYPE_WIFI_CONNECTION_LIST,
               G_SIGNAL_RUN_LAST,
               0, NULL, NULL, NULL,
               G_TYPE_NONE, 1, CC_TYPE_WIFI_CONNECTION_ROW);
  g_signal_new ("add-row",
                CC_TYPE_WIFI_CONNECTION_LIST,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, CC_TYPE_WIFI_CONNECTION_ROW);
  g_signal_new ("remove-row",
                CC_TYPE_WIFI_CONNECTION_LIST,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, CC_TYPE_WIFI_CONNECTION_ROW);
}

static void
cc_wifi_connection_list_init (CcWifiConnectionList *self)
{
  self->listbox = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);
  gtk_widget_set_valign (GTK_WIDGET (self->listbox), GTK_ALIGN_START);
  gtk_widget_add_css_class (GTK_WIDGET (self->listbox), "boxed-list");
  adw_bin_set_child (ADW_BIN (self), GTK_WIDGET (self->listbox));

  self->hide_unavailable = TRUE;
  self->show_aps = TRUE;

  self->connections = g_ptr_array_new_with_free_func (g_object_unref);
  self->connections_row = g_ptr_array_new ();
  self->ssid_to_row = g_hash_table_new_full (g_bytes_hash, g_bytes_equal,
                                             (GDestroyNotify) g_bytes_unref, NULL);
  self->ap_ssid_cache = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, (GDestroyNotify) g_bytes_unref);
}

CcWifiConnectionList *
cc_wifi_connection_list_new (NMClient     *client,
                             NMDeviceWifi *device,
                             gboolean      hide_unavailable,
                             gboolean      show_aps,
                             gboolean      checkable,
                             gboolean      forgettable)
{
  return g_object_new (CC_TYPE_WIFI_CONNECTION_LIST,
                       "client", client,
                       "device", device,
                       "hide-unavailable", hide_unavailable,
                       "show-aps", show_aps,
                       "checkable", checkable,
                       "forgettable", forgettable,
                       NULL);
}

void
cc_wifi_connection_list_freeze (CcWifiConnectionList *self)
{
  g_return_if_fail (CC_WIFI_CONNECTION_LIST (self));

  if (self->freeze_count == 0)
    g_debug ("wifi connection list has been frozen");

  self->freeze_count += 1;
}

void
cc_wifi_connection_list_thaw (CcWifiConnectionList *self)
{
  g_return_if_fail (CC_WIFI_CONNECTION_LIST (self));

  g_return_if_fail (self->freeze_count > 0);

  self->freeze_count -= 1;

  if (self->freeze_count == 0)
    {
      g_debug ("wifi connection list has been thawed");
      update_connections (self);
    }
}

GtkListBox *
cc_wifi_connection_list_get_list_box (CcWifiConnectionList *self)
{
  g_return_val_if_fail (CC_IS_WIFI_CONNECTION_LIST (self), NULL);

  return self->listbox;
}

gboolean
cc_wifi_connection_list_is_empty (CcWifiConnectionList *self)
{
  g_return_val_if_fail (CC_IS_WIFI_CONNECTION_LIST (self), TRUE);

  return self->connections->len == 0;
}
