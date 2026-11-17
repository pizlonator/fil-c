/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-data.c
 *
 * Copyright 2019 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-wwan-data"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <string.h>
#include <glib/gi18n.h>
#include <nma-mobile-providers.h>

#include "cc-wwan-data.h"

/**
 * @short_description: Device Internet Data Object
 * @include: "cc-wwan-device-data.h"
 *
 * #CcWwanData represents the data object of the given
 * #CcWwanDevice.  Please note that while #CcWWanDevice
 * is bound to the hardware device, #CcWwanData may also
 * depend on the inserted SIM (if supported). So the state
 * of #CcWwanData changes when SIM is changed.
 */

/*
 * Priority for connections. The larger the number, the lower the priority
 * https://developer.gnome.org/NetworkManager/stable/nm-settings.html:
 *
 *   A lower value is better (higher priority). Zero selects a globally
 *   configured default value. If the latter is missing or zero too, it
 *   defaults to 50 for VPNs and 100 for other connections.
 *
 * Since WiFi and other network connections will likely get the default
 * setting of 100, set WWAN DNS priorities higher than the default, with
 * room to allow multiple modems to set priority above/below each other.
 */
#define CC_WWAN_DNS_PRIORITY_LOW  (120)
#define CC_WWAN_DNS_PRIORITY_HIGH (115)

/* These are to be set as route metric */
#define CC_WWAN_ROUTE_PRIORITY_LOW  (1050)
#define CC_WWAN_ROUTE_PRIORITY_HIGH (1040)

struct _CcWwanData
{
  GObject      parent_instance;

  MMObject    *mm_object;
  MMModem     *modem;
  MMSim       *sim;
  gchar       *sim_id;

  gchar       *operator_code; /* MCCMNC */
  GError      *error;

  NMClient           *nm_client;
  NMDevice           *nm_device;
  NMAMobileProvidersDatabase *apn_db;
  NMAMobileProvider  *apn_provider;
  CcWwanDataApn      *default_apn;
  CcWwanDataApn      *old_default_apn;
  GListStore         *apn_list;
  NMActiveConnection *active_connection;

  gint     priority;
  gboolean data_enabled; /* autoconnect enabled */
  gboolean home_only;    /* Data roaming */
  gboolean apn_list_updated;    /* APN list updated from mobile-provider-info */
};

G_DEFINE_TYPE (CcWwanData, cc_wwan_data, G_TYPE_OBJECT)

/*
 * Default Access Point Settings Logic:
 * For a provided SIM, all the APNs available from NetworkManager
 * that matches the given SIM identifier (ICCID, available via
 * mm_sim_get_identifier() or similar gdbus API) is loaded for
 * the Device (In NetworkManager, it is saved as ‘sim-id’, if
 * present).  At a time, only one connection will be bound to
 * a device.  If there are more than one match, the item with
 * the highest ‘route-metric’ is taken.  If more matches are
 * still available, the first item is chosen.
 *
 * Populating All available APNs:
 * All Possible APNs for the given sim are populated the following
 * way (A list of all the following avoiding duplicates)
 * 1. The above mentioned “Default Access Point Settings Logic”
 * 2. Get All saved Network Manager connections with the
 *    provided MCCMNC of the given SIM
 * 3. Get All possible APNs for the MCCMNC from mobile-provider-info
 *
 * Testing if data is enabled:
 * Check if any of the items from step 1 have ‘autoconnect’ set
 *
 * Checking/Setting current SIM for data (in case of multiple SIM):
 * Since other networks (like wifi, ethernet) should have higher
 * priorities we use a negative number for priority.
 * 1. All APNs by default have priority CC_WWAN_APN_PRIORITY_LOW
 * 2. APN of selected SIM for active data have priority of
 *    CC_WWAN_APN_PRIORITY_HIGH
 *
 * XXX: Since users may create custom APNs via nmtui or like tools
 * we may have to check if there are some inconsistencies with APNs
 * available in NetworkManager, and ask user if they have to reset
 * the APNs that have invalid settings (basically, we care only APNs
 * that are set to have ‘autoconnect’ enabled, and all we need is to
 * disable autoconnect). We won’t interfere CDMA/EVDO networks.
 */
struct _CcWwanDataApn {
  GObject parent_instance;

  /* Set if the APN is from the mobile-provider-info database */
  NMAMobileAccessMethod *access_method;

  /* Set if the APN is saved in NetworkManager */
  NMConnection *nm_connection;
  NMRemoteConnection *remote_connection;

  gboolean modified;
};

G_DEFINE_TYPE (CcWwanDataApn, cc_wwan_data_apn, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ERROR,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
wwan_data_apn_reset (CcWwanDataApn *apn)
{
  if (!apn)
    return;

  g_clear_object (&apn->nm_connection);
  g_clear_object (&apn->remote_connection);
}

static NMConnection *
wwan_data_get_nm_connection (CcWwanDataApn *apn)
{
  NMConnection *connection;
  NMSetting *setting;
  g_autofree gchar *uuid = NULL;

  if (apn->nm_connection)
    return apn->nm_connection;

  if (apn->remote_connection)
    return NM_CONNECTION (apn->remote_connection);

  connection = nm_simple_connection_new ();
  apn->nm_connection = connection;

  setting = nm_setting_connection_new ();
  uuid = nm_utils_uuid_generate ();
  g_object_set (setting,
                NM_SETTING_CONNECTION_UUID, uuid,
                NM_SETTING_CONNECTION_TYPE, NM_SETTING_GSM_SETTING_NAME,
                NULL);
  nm_connection_add_setting (connection, setting);

  setting = nm_setting_serial_new ();
  nm_connection_add_setting (connection, setting);

  setting = nm_setting_ip4_config_new ();
  g_object_set (setting, NM_SETTING_IP_CONFIG_METHOD, "auto", NULL);
  nm_connection_add_setting (connection, setting);

  nm_connection_add_setting (connection, nm_setting_gsm_new ());
  nm_connection_add_setting (connection, nm_setting_ppp_new ());

  return apn->nm_connection;
}

static gboolean
wwan_data_apn_are_same (CcWwanDataApn         *apn,
                        NMAMobileAccessMethod *access_method)
{
  NMConnection *connection;
  NMSetting *setting;

  if (!apn->remote_connection)
    return FALSE;

  connection = NM_CONNECTION (apn->remote_connection);
  setting = NM_SETTING (nm_connection_get_setting_gsm (connection));

  if (g_strcmp0 (nma_mobile_access_method_get_3gpp_apn (access_method),
                 nm_setting_gsm_get_apn (NM_SETTING_GSM (setting))) != 0)
    return FALSE;

  if (g_strcmp0 (nma_mobile_access_method_get_username (access_method),
                 nm_setting_gsm_get_username (NM_SETTING_GSM (setting))) != 0)
    return FALSE;

  if (g_strcmp0 (nma_mobile_access_method_get_password (access_method),
                 cc_wwan_data_apn_get_password (apn)) != 0)
    return FALSE;

  return TRUE;
}

static CcWwanDataApn *
wwan_data_find_matching_apn (CcWwanData            *self,
                             NMAMobileAccessMethod *access_method)
{
  CcWwanDataApn *apn;
  guint i, n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->apn_list));

  for (i = 0; i < n_items; i++)
    {
      apn = g_list_model_get_item (G_LIST_MODEL (self->apn_list), i);

      if (apn->access_method == access_method)
        return apn;

      if (wwan_data_apn_are_same (apn, access_method))
        return apn;

      g_object_unref (apn);
    }

  return NULL;
}

static gboolean
wwan_data_nma_method_is_mms (NMAMobileAccessMethod *method)
{
  const char *str;

  str = nma_mobile_access_method_get_3gpp_apn (method);
  if (str && strcasestr (str, "mms"))
    return TRUE;

  str = nma_mobile_access_method_get_name (method);
  if (str && strcasestr (str, "mms"))
    return TRUE;

  return FALSE;
}

static void
wwan_data_update_apn_list_db (CcWwanData *self)
{
  GSList *apn_methods = NULL, *l;
  g_autoptr(GError) error = NULL;
  guint i = 0;

  if (!self->sim || !self->operator_code || self->apn_list_updated)
    return;

  if (!self->apn_list)
    return;

  if (!self->apn_db)
    self->apn_db = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      return;
    }

  if (!self->apn_provider)
    self->apn_provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (self->apn_db,
                                                                            self->operator_code);

  if (self->apn_provider)
    apn_methods = nma_mobile_provider_get_methods (self->apn_provider);

  self->apn_list_updated = TRUE;

  for (l = apn_methods; l; l = l->next, i++)
    {
      g_autoptr(CcWwanDataApn) apn = NULL;

      /* We don’t list MMS APNs */
      if (wwan_data_nma_method_is_mms (l->data))
        continue;

      apn = wwan_data_find_matching_apn (self, l->data);

      /* Prepend the item in order */
      if (!apn)
        {
          apn = cc_wwan_data_apn_new ();
          apn->access_method = l->data;
          g_list_store_insert (self->apn_list, i, apn);
        }

      apn->access_method = l->data;
    }
}

static void
wwan_data_update_apn_list (CcWwanData *self)
{
  const GPtrArray *nm_connections;
  guint i;

  if (self->apn_list || !self->sim || !self->nm_device ||
      nm_device_get_state (self->nm_device) <= NM_DEVICE_STATE_UNAVAILABLE)
    return;

  if (!self->apn_list)
    self->apn_list = g_list_store_new (CC_TYPE_WWAN_DATA_APN);

  if (self->nm_device)
    {
      nm_connections = nm_device_get_available_connections (self->nm_device);

      for (i = 0; i < nm_connections->len; i++)
        {
          g_autoptr(CcWwanDataApn) apn = NULL;

          apn = cc_wwan_data_apn_new ();
          apn->remote_connection = g_object_ref (nm_connections->pdata[i]);
          g_list_store_append (self->apn_list, apn);

          /* Load the default APN */
          if (!self->default_apn && self->sim_id)
            {
              NMSettingConnection *connection_setting;
              NMSettingIPConfig *ip_setting;
              NMSettingGsm *setting;
              NMConnection *connection;
              const gchar *sim_id;

              connection = NM_CONNECTION (apn->remote_connection);
              setting = nm_connection_get_setting_gsm (connection);
              connection_setting = nm_connection_get_setting_connection (connection);
              sim_id = nm_setting_gsm_get_sim_id (setting);

              if (sim_id && *sim_id && g_str_equal (sim_id, self->sim_id))
                {
                  self->default_apn = apn;
                  self->home_only = nm_setting_gsm_get_home_only (setting);
                  self->data_enabled = nm_setting_connection_get_autoconnect (connection_setting);

                  /* If any of the APN has a high priority, the device have high priority */
                  ip_setting = nm_connection_get_setting_ip4_config (connection);
                  if (nm_setting_ip_config_get_route_metric (ip_setting) == CC_WWAN_ROUTE_PRIORITY_HIGH)
                    self->priority = CC_WWAN_APN_PRIORITY_HIGH;
                }
            }
        }
    }
}

static void
wwan_device_state_changed_cb (CcWwanData *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);
}

static void
wwan_device_3gpp_operator_code_changd_cb (CcWwanData *self)
{
  MMModem3gpp *modem_3gpp;

  modem_3gpp = mm_object_peek_modem_3gpp (self->mm_object);

  if (!self->operator_code)
    {
      self->operator_code = mm_modem_3gpp_dup_operator_code (modem_3gpp);

      if (self->operator_code)
        {
          wwan_data_update_apn_list (self);
          wwan_data_update_apn_list_db (self);
        }
    }
}

static void
cc_wwan_data_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CcWwanData *self = (CcWwanData *)object;

  switch (prop_id)
    {
    case PROP_ERROR:
      g_value_set_boolean (value, self->error != NULL);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, cc_wwan_data_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wwan_data_dispose (GObject *object)
{
  CcWwanData *self = (CcWwanData *)object;

  g_clear_pointer (&self->sim_id, g_free);
  g_clear_pointer (&self->operator_code, g_free);
  g_clear_error (&self->error);
  g_clear_object (&self->apn_list);
  g_clear_object (&self->modem);
  g_clear_object (&self->mm_object);
  g_clear_object (&self->nm_client);
  g_clear_object (&self->active_connection);
  g_clear_object (&self->apn_db);

  G_OBJECT_CLASS (cc_wwan_data_parent_class)->dispose (object);
}

static void
cc_wwan_data_class_init (CcWwanDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_wwan_data_get_property;
  object_class->dispose = cc_wwan_data_dispose;

  properties[PROP_ERROR] =
    g_param_spec_boolean ("error",
                          "Error",
                          "Set if some Error occurs",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "Get if the data is enabled",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_wwan_data_init (CcWwanData *self)
{
  self->home_only = TRUE;
  self->priority = CC_WWAN_APN_PRIORITY_LOW;
}

/**
 * cc_wwan_data_new:
 * @mm_object: An #MMObject
 * @nm_client: An #NMClient
 *
 * Create a new device data representing the given
 * @mm_object. If @mm_object isn’t a 3G/CDMA/LTE
 * modem, %NULL will be returned
 *
 * Returns: A #CcWwanData or %NULL.
 */
CcWwanData *
cc_wwan_data_new (MMObject *mm_object,
                  NMClient *nm_client)
{
  CcWwanData *self;
  NMDevice *nm_device = NULL;
  g_autoptr(MMModem) modem = NULL;
  NMDeviceModemCapabilities capabilities = 0;

  g_return_val_if_fail (MM_IS_OBJECT (mm_object), NULL);
  g_return_val_if_fail (NM_CLIENT (nm_client), NULL);

  modem = mm_object_get_modem (mm_object);

  if (modem)
    nm_device = nm_client_get_device_by_iface (nm_client,
                                               mm_modem_get_primary_port (modem));

  if (NM_IS_DEVICE_MODEM (nm_device))
    capabilities = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));

  if (!(capabilities & (NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS
                        | NM_DEVICE_MODEM_CAPABILITY_LTE)))
    return NULL;

  self = g_object_new (CC_TYPE_WWAN_DATA, NULL);

  self->nm_client = g_object_ref (nm_client);
  self->mm_object = g_object_ref (mm_object);
  self->modem = g_steal_pointer (&modem);
  self->sim = mm_modem_get_sim_sync (self->modem, NULL, NULL);
  self->sim_id = mm_sim_dup_identifier (self->sim);
  self->operator_code = mm_sim_dup_operator_identifier (self->sim);
  self->nm_device = g_object_ref (nm_device);
  self->active_connection = nm_device_get_active_connection (nm_device);

  if (!self->operator_code)
    {
      MMModem3gpp *modem_3gpp;

      modem_3gpp = mm_object_peek_modem_3gpp (mm_object);
      if (modem_3gpp)
        {
          g_signal_connect_object (modem_3gpp, "notify::operator-code",
                                   G_CALLBACK (wwan_device_3gpp_operator_code_changd_cb),
                                   self, G_CONNECT_SWAPPED);
          wwan_device_3gpp_operator_code_changd_cb (self);
        }
    }

  if (self->active_connection)
    g_object_ref (self->active_connection);

  g_signal_connect_object (self->nm_device, "notify::state",
                           G_CALLBACK (wwan_device_state_changed_cb),
                           self, G_CONNECT_SWAPPED);

  wwan_data_update_apn_list (self);
  wwan_data_update_apn_list_db (self);

  return self;
}

GError *
cc_wwan_data_get_error (CcWwanData *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), NULL);

  return self->error;
}

const gchar *
cc_wwan_data_get_simple_html_error (CcWwanData *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), NULL);

  if (!self->error)
    return NULL;

  if (g_error_matches (self->error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return _("Operation Cancelled");

  if (g_error_matches (self->error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
    return _("<b>Error:</b> Access denied changing settings");

  if (self->error->domain == MM_MOBILE_EQUIPMENT_ERROR)
    return _("<b>Error:</b> Mobile Equipment Error");

  return NULL;
}

GListModel *
cc_wwan_data_get_apn_list (CcWwanData *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), NULL);

  if (!self->apn_list)
    wwan_data_update_apn_list (self);

  return G_LIST_MODEL (self->apn_list);
}

static gboolean
wwan_data_apn_is_new (CcWwanDataApn *apn)
{
  return apn->remote_connection == NULL;
}

static void
wwan_data_update_apn (CcWwanData    *self,
                      CcWwanDataApn *apn,
                      NMConnection  *connection)
{
  NMSetting *setting;
  const gchar *name, *username, *password, *apn_name;
  gint dns_priority, route_metric;

  setting = NM_SETTING (nm_connection_get_setting_connection (connection));

  g_object_set (setting,
                NM_SETTING_CONNECTION_AUTOCONNECT, self->data_enabled,
                NULL);

  setting = NM_SETTING (nm_connection_get_setting_gsm (connection));

  g_object_set (setting,
                NM_SETTING_GSM_HOME_ONLY, self->home_only,
                NULL);

  setting = NM_SETTING (nm_connection_get_setting_ip4_config (connection));
  if (self->priority == CC_WWAN_APN_PRIORITY_HIGH &&
      self->default_apn == apn)
    {
      dns_priority = CC_WWAN_DNS_PRIORITY_HIGH;
      route_metric = CC_WWAN_ROUTE_PRIORITY_HIGH;
    }
  else
    {
      dns_priority = CC_WWAN_DNS_PRIORITY_LOW;
      route_metric = CC_WWAN_ROUTE_PRIORITY_LOW;
    }

  g_object_set (setting,
                NM_SETTING_IP_CONFIG_DNS_PRIORITY, dns_priority,
                NM_SETTING_IP_CONFIG_ROUTE_METRIC, (gint64)route_metric,
                NULL);

  if (apn->access_method && !apn->remote_connection)
    {
      name = nma_mobile_access_method_get_name (apn->access_method);
      username = nma_mobile_access_method_get_username (apn->access_method);
      password = nma_mobile_access_method_get_password (apn->access_method);
      apn_name = nma_mobile_access_method_get_3gpp_apn (apn->access_method);
    }
  else
    {
      return;
    }

  setting = NM_SETTING (nm_connection_get_setting_gsm (connection));
  g_object_set (setting,
                NM_SETTING_GSM_USERNAME, username,
                NM_SETTING_GSM_PASSWORD, password,
                NM_SETTING_GSM_APN, apn_name,
                NULL);

  setting = NM_SETTING (nm_connection_get_setting_connection (connection));

  g_object_set (setting,
                NM_SETTING_CONNECTION_ID, name,
                NULL);
}

static gint
wwan_data_get_apn_index (CcWwanData    *self,
                         CcWwanDataApn *apn)
{
  GListModel *model;
  guint i, n_items;

  model = G_LIST_MODEL (self->apn_list);
  n_items = g_list_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(CcWwanDataApn) cached_apn = NULL;

      cached_apn = g_list_model_get_item (model, i);

      if (apn == cached_apn)
        return i;
    }

  return -1;
}

static void
cc_wwan_data_connection_updated_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  CcWwanData *self;
  CcWwanDataApn *apn;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  self = g_task_get_source_object (G_TASK (task));
  apn = g_task_get_task_data (G_TASK (task));

  nm_remote_connection_commit_changes_finish (apn->remote_connection,
                                              result, &error);
  if (!error)
    {
      guint apn_index;
      apn_index = wwan_data_get_apn_index (self, apn);

      if (apn_index >= 0)
        g_list_model_items_changed (G_LIST_MODEL (self->apn_list),
                                    apn_index, 1, 1);
      else
        g_warning ("APN ‘%s’ not in APN list",
                   cc_wwan_data_apn_get_name (apn));

      apn->modified = FALSE;
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_task_return_error (task, g_steal_pointer (&error));
    }
}

static void
cc_wwan_data_new_connection_added_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  CcWwanData *self;
  CcWwanDataApn *apn;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  self = g_task_get_source_object (G_TASK (task));
  apn = g_task_get_task_data (G_TASK (task));
  apn->remote_connection = nm_client_add_connection_finish (self->nm_client,
                                                            result, &error);
  if (!error)
    {
      apn->modified = FALSE;

      /* If APN has access method, it’s already on the list */
      if (!apn->access_method)
        {
          g_list_store_append (self->apn_list, apn);
          g_object_unref (apn);
        }

      g_task_return_pointer (task, apn, NULL);
    }
  else
    {
      g_task_return_error (task, g_steal_pointer (&error));
    }
}

void
cc_wwan_data_save_apn (CcWwanData          *self,
                       CcWwanDataApn       *apn,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  NMConnection *connection = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DATA (self));
  g_return_if_fail (CC_IS_WWAN_DATA_APN (apn));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, apn, NULL);

  connection = wwan_data_get_nm_connection (apn);

  /* If the item has a remote connection, it should already be saved.
   * We should save it again only if it got modified */
  if (apn->remote_connection && !apn->modified)
    {
      g_task_return_pointer (task, apn, NULL);
      return;
    }

  wwan_data_update_apn (self, apn, connection);
  if (wwan_data_apn_is_new (apn))
    {
      nm_client_add_connection_async (self->nm_client, apn->nm_connection,
                                      TRUE, cancellable,
                                      cc_wwan_data_new_connection_added_cb,
                                      g_steal_pointer (&task));
    }
  else
    {
      nm_remote_connection_commit_changes_async (apn->remote_connection, TRUE,
                                                 cancellable,
                                                 cc_wwan_data_connection_updated_cb,
                                                 g_steal_pointer (&task));
    }
}

CcWwanDataApn *
cc_wwan_data_save_apn_finish (CcWwanData    *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
cc_wwan_data_activated_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  CcWwanData *self;
  NMActiveConnection *connection;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  self = g_task_get_source_object (G_TASK (task));
  connection = nm_client_activate_connection_finish (self->nm_client,
                                                     result, &error);
  if (connection)
    {
      g_set_object (&self->active_connection, connection);
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_task_return_error (task, g_steal_pointer (&error));
    }

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
cc_wwan_data_disconnect_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcWwanData *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  self = g_task_get_source_object (G_TASK (task));
  if (nm_device_disconnect_finish (self->nm_device, result, &error))
    {
      g_clear_object (&self->active_connection);
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_task_return_error (task, g_steal_pointer (&error));
    }

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
cc_wwan_data_settings_saved_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  CcWwanData *self;
  GCancellable *cancellable;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  self = g_task_get_source_object (G_TASK (task));
  cancellable = g_task_get_cancellable (G_TASK (task));

  if (!cc_wwan_data_save_apn_finish (self, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self->default_apn->modified = FALSE;

  if (self->data_enabled)
    {
      nm_client_activate_connection_async (self->nm_client,
                                           NM_CONNECTION (self->default_apn->remote_connection),
                                           self->nm_device,
                                           NULL, cancellable,
                                           cc_wwan_data_activated_cb,
                                           g_steal_pointer (&task));
    }
  else
    {
      nm_device_disconnect_async (self->nm_device,
                                  cancellable,
                                  cc_wwan_data_disconnect_cb,
                                  g_steal_pointer (&task));
    }
}

/**
 * cc_wwan_data_save_settings:
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a #GAsyncReadyCallback, or %NULL
 * @user_data: closure data for @callback
 *
 * Save default settings to disk and apply changes.
 * If the default APN has data enabled, the data is
 * activated after the settings are saved.
 *
 * It’s a programmer error to call this function without
 * a default APN set.
 * Finish with cc_wwan_data_save_settings_finish().
 */
void
cc_wwan_data_save_settings (CcWwanData          *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  NMConnection *connection;
  NMSetting *setting;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DATA (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->default_apn != NULL);

  task = g_task_new (self, cancellable, callback, user_data);

  /* Reset old settings to default value */
  if (self->old_default_apn && self->old_default_apn->remote_connection)
    {
      connection = NM_CONNECTION (self->old_default_apn->remote_connection);

      setting = NM_SETTING (nm_connection_get_setting_gsm (connection));
      g_object_set (G_OBJECT (setting),
                    NM_SETTING_GSM_HOME_ONLY, TRUE,
                    NM_SETTING_GSM_SIM_ID, NULL,
                    NULL);

      setting = NM_SETTING (nm_connection_get_setting_ip4_config (connection));
      g_object_set (setting,
                    NM_SETTING_IP_CONFIG_DNS_PRIORITY, CC_WWAN_DNS_PRIORITY_LOW,
                    NM_SETTING_IP_CONFIG_ROUTE_METRIC, (gint64)CC_WWAN_ROUTE_PRIORITY_LOW,
                    NULL);

      setting = NM_SETTING (nm_connection_get_setting_connection (connection));
      g_object_set (G_OBJECT (setting),
                    NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
                    NULL);

      nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (connection),
                                           TRUE, cancellable, NULL);
      self->old_default_apn->modified = FALSE;
      self->old_default_apn = NULL;
    }

  self->default_apn->modified = TRUE;
  connection = wwan_data_get_nm_connection (self->default_apn);

  setting = NM_SETTING (nm_connection_get_setting_gsm (connection));
  g_object_set (G_OBJECT (setting),
                NM_SETTING_GSM_HOME_ONLY, self->home_only,
                NM_SETTING_GSM_SIM_ID, self->sim_id,
                NULL);

  cc_wwan_data_save_apn (self, self->default_apn, cancellable,
                         cc_wwan_data_settings_saved_cb,
                         g_steal_pointer (&task));
}

gboolean
cc_wwan_data_save_settings_finish (CcWwanData    *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
cc_wwan_data_delete_apn (CcWwanData     *self,
                         CcWwanDataApn  *apn,
                         GCancellable   *cancellable,
                         GError        **error)
{
  NMRemoteConnection *connection = NULL;
  gboolean ret = FALSE;
  gint apn_index;

  g_return_val_if_fail (CC_IS_WWAN_DATA (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  apn_index = wwan_data_get_apn_index (self, apn);
  if (apn_index == -1)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "APN not found for the connection");
      return FALSE;
    }

  connection = g_steal_pointer (&apn->remote_connection);
  wwan_data_apn_reset (apn);

  if (connection)
    ret = nm_remote_connection_delete (connection, cancellable, error);

  if (!ret)
    {
      apn->remote_connection = connection;
      *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Deleting APN from NetworkManager failed");
      return ret;
    }

  g_object_unref (connection);

  /* We remove the item only if it's not in the mobile provider database */
  if (!apn->access_method)
    {
      if (self->default_apn == apn)
        self->default_apn = NULL;

      g_list_store_remove (self->apn_list, apn_index);

      return TRUE;
    }

  *error = g_error_new (G_IO_ERROR, G_IO_ERROR_READ_ONLY,
                        "Deleting APN from NetworkManager failed");
  return FALSE;
}

CcWwanDataApn *
cc_wwan_data_get_default_apn (CcWwanData *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), NULL);

  return self->default_apn;
}

gboolean
cc_wwan_data_set_default_apn (CcWwanData    *self,
                              CcWwanDataApn *apn)
{
  NMConnection *connection;
  NMSetting *setting;

  g_return_val_if_fail (CC_IS_WWAN_DATA (self), FALSE);
  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), FALSE);

  if (self->default_apn == apn)
    return FALSE;

  /*
   * APNs are bound to the SIM, not the modem device.
   * This will let the APN work if the same SIM inserted
   * in a different device, and not enable data if a
   * different SIM is inserted to the modem.
   */
  apn->modified = TRUE;
  self->old_default_apn = self->default_apn;
  self->default_apn = apn;
  connection = wwan_data_get_nm_connection (apn);
  setting = NM_SETTING (nm_connection_get_setting_gsm (connection));

  if (self->sim_id)
    g_object_set (G_OBJECT (setting),
                  NM_SETTING_GSM_SIM_ID, self->sim_id, NULL);

  return TRUE;
}

gboolean
cc_wwan_data_get_enabled (CcWwanData *self)
{
  NMSettingConnection *setting;
  NMConnection *connection;
  NMDeviceState state;

  g_return_val_if_fail (CC_IS_WWAN_DATA (self), FALSE);

  state = nm_device_get_state (self->nm_device);

  if (state == NM_DEVICE_STATE_DISCONNECTED ||
      state == NM_DEVICE_STATE_DEACTIVATING)
    if (nm_device_get_state_reason (self->nm_device) == NM_DEVICE_STATE_REASON_USER_REQUESTED)
      return FALSE;

  if (nm_device_get_active_connection (self->nm_device) != NULL)
    return TRUE;

  if (!self->default_apn || !self->default_apn->remote_connection)
    return FALSE;

  connection = NM_CONNECTION (self->default_apn->remote_connection);
  setting = nm_connection_get_setting_connection (connection);

  return nm_setting_connection_get_autoconnect (setting);
}

/**
 * cc_wwan_data_set_enabled:
 * @self: A #CcWwanData
 * @enable_data: whether to enable data
 *
 * Enable data for the device.  The settings is
 * saved to disk only after a default APN is set.
 *
 * If the data is enabled, the device will automatically
 * turn data on everytime the same SIM is available.
 * The data set is bound to the SIM, not the modem device.
 *
 * Use @cc_wwan_data_save_apn() with the default APN
 * to save the changes and really enable/disable data.
 */
void
cc_wwan_data_set_enabled (CcWwanData *self,
                          gboolean    enable_data)
{
  g_return_if_fail (CC_IS_WWAN_DATA (self));

  self->data_enabled = !!enable_data;

  if (self->default_apn)
    self->default_apn->modified = TRUE;
}

gboolean
cc_wwan_data_get_roaming_enabled (CcWwanData *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA (self), FALSE);

  if (!self->default_apn)
    return FALSE;

  return !self->home_only;
}

/**
 * cc_wwan_data_apn_set_roaming_enabled:
 * @self: A #CcWwanData
 * @enable_roaming: whether to enable roaming or not
 *
 * Enable roaming for the device.  The settings is
 * saved to disk only after a default APN is set.
 *
 * Use @cc_wwan_data_save_apn() with the default APN
 * to save the changes and really enable/disable data.
 */
void
cc_wwan_data_set_roaming_enabled (CcWwanData *self,
                                  gboolean    enable_roaming)
{
  g_return_if_fail (CC_IS_WWAN_DATA (self));

  self->home_only = !enable_roaming;

  if (self->default_apn)
    self->default_apn->modified = TRUE;
}

static void
cc_wwan_data_apn_finalize (GObject *object)
{
  CcWwanDataApn *apn = CC_WWAN_DATA_APN (object);

  wwan_data_apn_reset (apn);
  g_clear_pointer (&apn->access_method,
                   nma_mobile_access_method_unref);

  G_OBJECT_CLASS (cc_wwan_data_parent_class)->finalize (object);
}

static void
cc_wwan_data_apn_class_init (CcWwanDataApnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_wwan_data_apn_finalize;
}

static void
cc_wwan_data_apn_init (CcWwanDataApn *apn)
{
}

CcWwanDataApn *
cc_wwan_data_apn_new (void)
{
  return g_object_new (CC_TYPE_WWAN_DATA_APN, NULL);
}

/**
 * cc_wwan_data_apn_get_name:
 * @apn: A #CcWwanDataApn
 *
 * Get the Name of @apn
 *
 * Returns: (transfer none): The Name of @apn
 */
const gchar *
cc_wwan_data_apn_get_name (CcWwanDataApn *apn)
{
  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), "");

  if (apn->remote_connection)
    return nm_connection_get_id (NM_CONNECTION (apn->remote_connection));

  if (apn->access_method)
    return nma_mobile_access_method_get_name (apn->access_method);

  return "";
}

/**
 * cc_wwan_data_apn_set_name:
 * @apn: A #CcWwanDataApn
 * @name: The name to be given for APN, should not
 * be empty
 *
 * Set the name of @apn to be @name.
 *
 * @apn is only modified, use @cc_wwan_data_save_apn()
 * to save the changes.
 */
void
cc_wwan_data_apn_set_name (CcWwanDataApn *apn,
                           const gchar   *name)
{
  NMConnection *connection;
  NMSettingConnection *setting;

  g_return_if_fail (CC_IS_WWAN_DATA_APN (apn));
  g_return_if_fail (name != NULL);
  g_return_if_fail (*name != '\0');

  if (g_str_equal (cc_wwan_data_apn_get_name (apn), name))
    return;

  apn->modified = TRUE;
  connection = wwan_data_get_nm_connection (apn);
  setting = nm_connection_get_setting_connection (connection);
  g_object_set (G_OBJECT (setting),
                NM_SETTING_CONNECTION_ID, name,
                NULL);
}

/**
 * cc_wwan_data_apn_get_apn:
 * @apn: A #CcWwanDataApn
 *
 * Get the APN of @apn
 *
 * Returns: (transfer none): The APN of @apn
 */
const gchar *
cc_wwan_data_apn_get_apn (CcWwanDataApn *apn)
{
  const gchar *apn_name = "";

  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), "");

  if (apn->remote_connection)
    {
      NMSettingGsm *setting;

      setting = nm_connection_get_setting_gsm (NM_CONNECTION (apn->remote_connection));
      apn_name = nm_setting_gsm_get_apn (setting);
    }
  else if (apn->access_method)
    {
      apn_name = nma_mobile_access_method_get_3gpp_apn (apn->access_method);
    }

  return apn_name ? apn_name : "";
}

/**
 * cc_wwan_data_apn_set_apn:
 * @apn: A #CcWwanDataApn
 * @apn_name: The apn to be used, should not be
 * empty
 *
 * Set the APN of @apn to @apn_name. @apn_name is
 * usually a URL like “example.com” or a simple string
 * like “internet”
 *
 * @apn is only modified, use @cc_wwan_data_save_apn()
 * to save the changes.
 */
void
cc_wwan_data_apn_set_apn (CcWwanDataApn *apn,
                          const gchar   *apn_name)
{
  NMConnection *connection;
  NMSettingGsm *setting;

  g_return_if_fail (CC_IS_WWAN_DATA_APN (apn));
  g_return_if_fail (apn_name != NULL);
  g_return_if_fail (*apn_name != '\0');

  if (g_str_equal (cc_wwan_data_apn_get_apn (apn), apn_name))
    return;

  apn->modified = TRUE;
  connection = wwan_data_get_nm_connection (apn);
  setting = nm_connection_get_setting_gsm (connection);
  g_object_set (G_OBJECT (setting),
                NM_SETTING_GSM_APN, apn_name,
                NULL);
}

/**
 * cc_wwan_data_apn_get_username:
 * @apn: A #CcWwanDataApn
 *
 * Get the Username of @apn
 *
 * Returns: (transfer none): The Username of @apn
 */
const gchar *
cc_wwan_data_apn_get_username (CcWwanDataApn *apn)
{
  const gchar *username = "";

  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), "");

  if (apn->remote_connection)
    {
      NMSettingGsm *setting;

      setting = nm_connection_get_setting_gsm (NM_CONNECTION (apn->remote_connection));
      username = nm_setting_gsm_get_username (setting);
    }
  else if (apn->access_method)
    {
      username = nma_mobile_access_method_get_username (apn->access_method);
    }

  return username ? username : "";
}

/**
 * cc_wwan_data_apn_set_username:
 * @apn: A #CcWwanDataAPN
 * @username: The username to be used
 *
 * Set the Username of @apn to @username.
 *
 * @apn is only modified, use @cc_wwan_data_save_apn()
 * to save the changes.
 */
void
cc_wwan_data_apn_set_username (CcWwanDataApn *apn,
                               const gchar   *username)
{
  NMConnection *connection;
  NMSettingGsm *setting;

  g_return_if_fail (CC_IS_WWAN_DATA_APN (apn));

  if (username && !*username)
    username = NULL;

  if (g_strcmp0 (cc_wwan_data_apn_get_username (apn), username) == 0)
    return;

  apn->modified = TRUE;
  connection = wwan_data_get_nm_connection (apn);
  setting = nm_connection_get_setting_gsm (connection);
  g_object_set (G_OBJECT (setting),
                NM_SETTING_GSM_USERNAME, username,
                NULL);
}

/**
 * cc_wwan_data_apn_get_password:
 * @apn: A #CcWwanDataApn
 *
 * Get the Password of @apn
 *
 * Returns: (transfer none): The Password of @apn
 */
const gchar *
cc_wwan_data_apn_get_password (CcWwanDataApn *apn)
{
  const gchar *password = "";

  g_return_val_if_fail (CC_IS_WWAN_DATA_APN (apn), "");

  if (NM_IS_REMOTE_CONNECTION (apn->remote_connection))
    {
      g_autoptr(GVariant) secrets = NULL;
      g_autoptr(GError) error = NULL;

      secrets = nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (apn->remote_connection),
                                                  "gsm", NULL, &error);

      if (!error)
        nm_connection_update_secrets (NM_CONNECTION (apn->remote_connection),
                                      "gsm", secrets, &error);

      if (error)
        {
          g_warning ("Error: %s", error->message);
          return "";
        }
    }

  if (apn->remote_connection)
    {
      NMSettingGsm *setting;

      setting = nm_connection_get_setting_gsm (NM_CONNECTION (apn->remote_connection));
      password = nm_setting_gsm_get_password (setting);
    }
  else if (apn->access_method)
    {
      password = nma_mobile_access_method_get_password (apn->access_method);
    }

  return password ? password : "";
}

/**
 * cc_wwan_data_apn_set_password:
 * @apn: A #CcWwanDataApn
 * @password: The password to be used
 *
 * Set the Password of @apn to @password.
 *
 * @apn is only modified, use @cc_wwan_data_save_apn()
 * to save the changes.
 */
void
cc_wwan_data_apn_set_password (CcWwanDataApn *apn,
                               const gchar   *password)
{
  NMConnection *connection;
  NMSettingGsm *setting;

  g_return_if_fail (CC_IS_WWAN_DATA_APN (apn));

  if (password && !*password)
    password = NULL;

  if (g_strcmp0 (cc_wwan_data_apn_get_password (apn), password) == 0)
    return;

  apn->modified = TRUE;
  connection = wwan_data_get_nm_connection (apn);
  setting = nm_connection_get_setting_gsm (connection);
  g_object_set (G_OBJECT (setting),
                NM_SETTING_GSM_PASSWORD, password,
                NULL);
}

gint
cc_wwan_data_get_priority (CcWwanData *self)
{
  CcWwanDataApn *apn;
  NMSettingIPConfig *setting;

  g_return_val_if_fail (CC_IS_WWAN_DATA (self),
                        CC_WWAN_APN_PRIORITY_LOW);

  apn = self->default_apn;

  if (!apn || !apn->remote_connection)
    return CC_WWAN_APN_PRIORITY_LOW;

  setting = nm_connection_get_setting_ip4_config (NM_CONNECTION (apn->remote_connection));

  /* Lower the number, higher the priority */
  if (nm_setting_ip_config_get_route_metric (setting) <= CC_WWAN_ROUTE_PRIORITY_HIGH)
    return CC_WWAN_APN_PRIORITY_HIGH;
  else
    return CC_WWAN_APN_PRIORITY_LOW;
}

void
cc_wwan_data_set_priority (CcWwanData *self,
                           int         priority)
{
  g_return_if_fail (CC_IS_WWAN_DATA (self));
  g_return_if_fail (priority == CC_WWAN_APN_PRIORITY_LOW ||
                    priority == CC_WWAN_APN_PRIORITY_HIGH);

  if (self->priority == priority)
    return;

  self->priority = priority;

  if (self->default_apn)
    self->default_apn->modified = TRUE;
}
