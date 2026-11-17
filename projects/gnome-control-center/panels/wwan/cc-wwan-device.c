/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-device.c
 *
 * Copyright 2019-2020 Purism SPC
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
#define G_LOG_DOMAIN "cc-wwan-device"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <polkit/polkit.h>
#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
# include <NetworkManager.h>
# include <nma-mobile-providers.h>
#endif

#include "cc-wwan-errors-private.h"
#include "cc-wwan-device.h"

/**
 * @short_description: Device Object
 * @include: "cc-wwan-device.h"
 */

struct _CcWwanDevice
{
  GObject      parent_instance;

  MMObject    *mm_object;
  MMModem     *modem;
  MMSim       *sim;
  MMModem3gpp *modem_3gpp;

  const char  *operator_code; /* MCCMNC */
  GError      *error;

  /* Building with NetworkManager is optional,
   * so #NMclient type can’t be used here.
   */
  GObject      *nm_client; /* An #NMClient */
  CcWwanData   *wwan_data;

  gulong      modem_3gpp_id;
  gulong      modem_3gpp_locks_id;

  /* Enabled locks like PIN, PIN2, PUK, etc. */
  MMModem3gppFacility locks;

  CcWwanState  registration_state;
  gboolean     network_is_manual;
};

G_DEFINE_TYPE (CcWwanDevice, cc_wwan_device, G_TYPE_OBJECT)


enum {
  PROP_0,
  PROP_OPERATOR_NAME,
  PROP_ENABLED_LOCKS,
  PROP_ERROR,
  PROP_HAS_DATA,
  PROP_NETWORK_MODE,
  PROP_REGISTRATION_STATE,
  PROP_SIGNAL,
  PROP_UNLOCK_REQUIRED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_wwan_device_state_changed_cb (CcWwanDevice *self)
{
  MMModem3gppRegistrationState state;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATOR_NAME]);

  state = mm_modem_3gpp_get_registration_state (self->modem_3gpp);

  switch (state)
    {
    case MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_UNKNOWN;
      break;

    case MM_MODEM_3GPP_REGISTRATION_STATE_DENIED:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_DENIED;
      break;

    case MM_MODEM_3GPP_REGISTRATION_STATE_IDLE:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_IDLE;
      break;

    case MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_SEARCHING;
      break;

    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_ROAMING;
      break;

    default:
      self->registration_state = CC_WWAN_REGISTRATION_STATE_REGISTERED;
      break;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REGISTRATION_STATE]);
}

static void
cc_wwan_device_locks_changed_cb (CcWwanDevice *self)
{
  self->locks = mm_modem_3gpp_get_enabled_facility_locks (self->modem_3gpp);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED_LOCKS]);
}

static void
cc_wwan_device_3gpp_changed_cb (CcWwanDevice *self)
{
  gulong handler_id = 0;

  if (self->modem_3gpp_id)
    g_signal_handler_disconnect (self->modem_3gpp, self->modem_3gpp_id);
  self->modem_3gpp_id = 0;

  if (self->modem_3gpp_locks_id)
    g_signal_handler_disconnect (self->modem_3gpp, self->modem_3gpp_locks_id);
  self->modem_3gpp_locks_id = 0;

  g_clear_object (&self->modem_3gpp);
  self->modem_3gpp = mm_object_get_modem_3gpp (self->mm_object);

  if (self->modem_3gpp)
    {
      handler_id = g_signal_connect_object (self->modem_3gpp, "notify::registration-state",
                                            G_CALLBACK (cc_wwan_device_state_changed_cb),
                                            self, G_CONNECT_SWAPPED);
      self->modem_3gpp_id = handler_id;

      handler_id = g_signal_connect_object (self->modem_3gpp, "notify::enabled-facility-locks",
                                            G_CALLBACK (cc_wwan_device_locks_changed_cb),
                                            self, G_CONNECT_SWAPPED);
      self->modem_3gpp_locks_id = handler_id;
      cc_wwan_device_locks_changed_cb (self);
      cc_wwan_device_state_changed_cb (self);
    }
}

static void
cc_wwan_device_signal_quality_changed_cb (CcWwanDevice *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGNAL]);
}

static void
cc_wwan_device_mode_changed_cb (CcWwanDevice *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NETWORK_MODE]);
}

static void
wwan_device_emit_data_changed (CcWwanDevice *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_DATA]);
}

static void
cc_wwan_device_unlock_required_cb (CcWwanDevice *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNLOCK_REQUIRED]);
}

#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
static void
cc_wwan_device_nm_changed_cb (CcWwanDevice *self,
                              GParamSpec   *pspec,
                              NMClient     *client)
{
  gboolean nm_is_running;

  nm_is_running = nm_client_get_nm_running (client);

  if (!nm_is_running && self->wwan_data != NULL)
    {
      g_clear_object (&self->wwan_data);
      wwan_device_emit_data_changed (self);
    }
}

static void
cc_wwan_device_nm_device_added_cb (CcWwanDevice *self,
                                   NMDevice     *nm_device)
{
  if (!NM_IS_DEVICE_MODEM (nm_device))
    return;

  if(!self->sim || !cc_wwan_device_is_nm_device (self, G_OBJECT (nm_device)))
    return;

  self->wwan_data = cc_wwan_data_new (self->mm_object,
                                      NM_CLIENT (self->nm_client));

  if (self->wwan_data)
    {
      g_signal_connect_object (self->wwan_data, "notify::enabled",
                               G_CALLBACK (wwan_device_emit_data_changed),
                               self, G_CONNECT_SWAPPED);
      wwan_device_emit_data_changed (self);
    }
}
#endif

static void
cc_wwan_device_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CcWwanDevice *self = (CcWwanDevice *)object;
  MMModemMode allowed, preferred;

  switch (prop_id)
    {
    case PROP_OPERATOR_NAME:
      g_value_set_string (value, cc_wwan_device_get_operator_name (self));
      break;

    case PROP_ERROR:
      g_value_set_boolean (value, self->error != NULL);
      break;

    case PROP_HAS_DATA:
      g_value_set_boolean (value, self->wwan_data != NULL);
      break;

    case PROP_ENABLED_LOCKS:
      g_value_set_int (value, self->locks);
      break;

    case PROP_NETWORK_MODE:
      if (cc_wwan_device_get_current_mode (self, &allowed, &preferred))
        g_value_take_string (value, cc_wwan_device_get_string_from_mode (self, allowed, preferred));
      break;

    case PROP_REGISTRATION_STATE:
      g_value_set_int (value, self->registration_state);
      break;

    case PROP_UNLOCK_REQUIRED:
      g_value_set_int (value, cc_wwan_device_get_lock (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wwan_device_dispose (GObject *object)
{
  CcWwanDevice *self = (CcWwanDevice *)object;

  g_clear_error (&self->error);
  g_clear_object (&self->modem);
  g_clear_object (&self->mm_object);
  g_clear_object (&self->sim);
  g_clear_object (&self->modem_3gpp);

  g_clear_object (&self->nm_client);
  g_clear_object (&self->wwan_data);

  G_OBJECT_CLASS (cc_wwan_device_parent_class)->dispose (object);
}

static void
cc_wwan_device_class_init (CcWwanDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_wwan_device_get_property;
  object_class->dispose = cc_wwan_device_dispose;

  properties[PROP_OPERATOR_NAME] =
    g_param_spec_string ("operator-name",
                         "Operator Name",
                         "Operator Name the device is connected to",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLED_LOCKS] =
    g_param_spec_int ("enabled-locks",
                      "Enabled Locks",
                      "Locks Enabled in Modem",
                      MM_MODEM_3GPP_FACILITY_NONE,
                      MM_MODEM_3GPP_FACILITY_CORP_PERS,
                      MM_MODEM_3GPP_FACILITY_NONE,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ERROR] =
    g_param_spec_boolean ("error",
                          "Error",
                          "Set if some Error occurs",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_HAS_DATA] =
    g_param_spec_boolean ("has-data",
                          "has-data",
                          "Data for the device",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NETWORK_MODE] =
    g_param_spec_string ("network-mode",
                         "Network Mode",
                         "A String representing preferred network mode",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_REGISTRATION_STATE] =
    g_param_spec_int ("registration-state",
                      "Registration State",
                      "The current network registration state",
                      CC_WWAN_REGISTRATION_STATE_UNKNOWN,
                      CC_WWAN_REGISTRATION_STATE_DENIED,
                      CC_WWAN_REGISTRATION_STATE_UNKNOWN,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_UNLOCK_REQUIRED] =
    g_param_spec_int ("unlock-required",
                      "Unlock Required",
                      "The Modem lock status changed",
                      MM_MODEM_LOCK_UNKNOWN,
                      MM_MODEM_LOCK_PH_NETSUB_PUK,
                      MM_MODEM_LOCK_UNKNOWN,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SIGNAL] =
    g_param_spec_int ("signal",
                      "Signal",
                      "Get Device Signal",
                      0, 100, 0,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_wwan_device_init (CcWwanDevice *self)
{
}

/**
 * cc_wwan_device_new:
 * @mm_object: (transfer full): An #MMObject
 *
 * Create a new device representing the given
 * @mm_object.
 *
 * Returns: A #CcWwanDevice
 */
CcWwanDevice *
cc_wwan_device_new (MMObject *mm_object,
                    GObject  *nm_client)
{
  CcWwanDevice *self;

  g_return_val_if_fail (MM_IS_OBJECT (mm_object), NULL);
#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
  g_return_val_if_fail (NM_IS_CLIENT (nm_client), NULL);
#else
  g_return_val_if_fail (!nm_client, NULL);
#endif

  self = g_object_new (CC_TYPE_WWAN_DEVICE, NULL);

  self->mm_object = g_object_ref (mm_object);
  self->modem = mm_object_get_modem (mm_object);
  self->sim = mm_modem_get_sim_sync (self->modem, NULL, NULL);
  g_set_object (&self->nm_client, nm_client);
  if (self->sim)
    {
      self->operator_code = mm_sim_get_operator_identifier (self->sim);
#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
      self->wwan_data = cc_wwan_data_new (mm_object,
                                          NM_CLIENT (self->nm_client));
#endif
    }

  g_signal_connect_object (self->mm_object, "notify::unlock-required",
                           G_CALLBACK (cc_wwan_device_unlock_required_cb),
                           self, G_CONNECT_SWAPPED);
  if (self->wwan_data)
    g_signal_connect_object (self->wwan_data, "notify::enabled",
                             G_CALLBACK (wwan_device_emit_data_changed),
                             self, G_CONNECT_SWAPPED);

#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
  g_signal_connect_object (self->nm_client, "notify::nm-running" ,
                           G_CALLBACK (cc_wwan_device_nm_changed_cb), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->nm_client, "device-added",
                           G_CALLBACK (cc_wwan_device_nm_device_added_cb),
                           self, G_CONNECT_SWAPPED);
#endif

  g_signal_connect_object (self->mm_object, "notify::modem3gpp",
                           G_CALLBACK (cc_wwan_device_3gpp_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->modem, "notify::signal-quality",
                           G_CALLBACK (cc_wwan_device_signal_quality_changed_cb),
                           self, G_CONNECT_SWAPPED);

  cc_wwan_device_3gpp_changed_cb (self);
  g_signal_connect_object (self->modem, "notify::current-modes",
                           G_CALLBACK (cc_wwan_device_mode_changed_cb),
                           self, G_CONNECT_SWAPPED);

  return self;
}

gboolean
cc_wwan_device_has_sim (CcWwanDevice *self)
{
  MMModemStateFailedReason state_reason;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);

  state_reason = mm_modem_get_state_failed_reason (self->modem);

  if (state_reason == MM_MODEM_STATE_FAILED_REASON_SIM_MISSING)
    return FALSE;

  return TRUE;
}

/**
 * cc_wwan_device_get_lock:
 * @self: a #CcWwanDevice
 *
 * Get the active device lock that is required to
 * be unlocked for accessing device features.
 *
 * Returns: %TRUE if PIN enabled, %FALSE otherwise.
 */
MMModemLock
cc_wwan_device_get_lock (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), MM_MODEM_LOCK_UNKNOWN);

  return mm_modem_get_unlock_required (self->modem);
}


/**
 * cc_wwan_device_get_sim_lock:
 * @self: a #CcWwanDevice
 *
 * Get if SIM lock with PIN is enabled.  SIM PIN
 * enabled doesn’t mean that SIM is locked.
 * See cc_wwan_device_get_lock().
 *
 * Returns: %TRUE if PIN enabled, %FALSE otherwise.
 */
gboolean
cc_wwan_device_get_sim_lock (CcWwanDevice *self)
{
  gboolean sim_lock;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);

  sim_lock = self->locks & MM_MODEM_3GPP_FACILITY_SIM;

  return !!sim_lock;
}

guint
cc_wwan_device_get_unlock_retries (CcWwanDevice *self,
                                   MMModemLock   lock)
{
  MMUnlockRetries *retries;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), 0);

  retries = mm_modem_get_unlock_retries (self->modem);

  return mm_unlock_retries_get (retries, lock);
}

static void
cc_wwan_device_pin_sent_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcWwanDevice *self;
  MMSim *sim = (MMSim *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_sim_send_pin_finish (sim, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_send_pin (CcWwanDevice        *self,
                         const gchar         *pin,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (MM_IS_SIM (self->sim));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (pin && *pin);

  task = g_task_new (self, cancellable, callback, user_data);

  mm_sim_send_pin (self->sim, pin, cancellable,
                   cc_wwan_device_pin_sent_cb,
                   g_steal_pointer (&task));
}

gboolean
cc_wwan_device_send_pin_finish (CcWwanDevice  *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cc_wwan_device_puk_sent_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcWwanDevice *self;
  MMSim *sim = (MMSim *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_sim_send_puk_finish (sim, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_send_puk (CcWwanDevice        *self,
                         const gchar         *puk,
                         const gchar         *pin,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (MM_IS_SIM (self->sim));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (puk && *puk);
  g_return_if_fail (pin && *pin);

  task = g_task_new (self, cancellable, callback, user_data);

  mm_sim_send_puk (self->sim, puk, pin, cancellable,
                   cc_wwan_device_puk_sent_cb,
                   g_steal_pointer (&task));
}

gboolean
cc_wwan_device_send_puk_finish (CcWwanDevice  *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cc_wwan_device_enable_pin_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  CcWwanDevice *self;
  MMSim *sim = (MMSim *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_sim_enable_pin_finish (sim, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_enable_pin (CcWwanDevice        *self,
                           const gchar         *pin,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (pin && *pin);

  task = g_task_new (self, cancellable, callback, user_data);

  mm_sim_enable_pin (self->sim, pin, cancellable,
                     cc_wwan_device_enable_pin_cb,
                     g_steal_pointer (&task));
}

gboolean
cc_wwan_device_enable_pin_finish (CcWwanDevice  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cc_wwan_device_disable_pin_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  CcWwanDevice *self;
  MMSim *sim = (MMSim *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_sim_disable_pin_finish (sim, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_disable_pin (CcWwanDevice        *self,
                            const gchar         *pin,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (pin && *pin);

  task = g_task_new (self, cancellable, callback, user_data);

  mm_sim_disable_pin (self->sim, pin, cancellable,
                      cc_wwan_device_disable_pin_cb,
                      g_steal_pointer (&task));
}

gboolean
cc_wwan_device_disable_pin_finish (CcWwanDevice *self,
                                   GAsyncResult *result,
                                   GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cc_wwan_device_change_pin_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  CcWwanDevice *self;
  MMSim *sim = (MMSim *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_sim_change_pin_finish (sim, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_change_pin (CcWwanDevice        *self,
                           const gchar         *old_pin,
                           const gchar         *new_pin,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (old_pin && *old_pin);
  g_return_if_fail (new_pin && *new_pin);

  task = g_task_new (self, cancellable, callback, user_data);

  mm_sim_change_pin (self->sim, old_pin, new_pin, cancellable,
                     cc_wwan_device_change_pin_cb,
                     g_steal_pointer (&task));
}

gboolean
cc_wwan_device_change_pin_finish (CcWwanDevice  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cc_wwan_device_network_mode_set_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  CcWwanDevice *self;
  MMModem *modem = (MMModem *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_modem_set_current_modes_finish (modem, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_warning ("Error: %s", error->message);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

/**
 * cc_wwan_device_set_network_mode:
 * @self: a #CcWwanDevice
 * @allowed: The allowed #MMModemModes
 * @preferred: The preferred #MMModemMode
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a #GAsyncReadyCallback or %NULL
 * @user_data: (nullable): closure data for @callback
 *
 * Asynchronously set preferred network mode.
 *
 * Call @cc_wwan_device_set_current_mode_finish()
 * in @callback to get the result of operation.
 */
void
cc_wwan_device_set_current_mode (CcWwanDevice        *self,
                                 MMModemMode          allowed,
                                 MMModemMode          preferred,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GPermission *permission;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  permission = polkit_permission_new_sync ("org.freedesktop.ModemManager1.Device.Control",
                                           NULL, cancellable, &error);
  if (permission)
    g_task_set_task_data (task, permission, g_object_unref);

  if (error)
    g_warning ("error: %s", error->message);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
    }
  else if (!g_permission_get_allowed (permission))
    {
      error = g_error_new (G_IO_ERROR,
                           G_IO_ERROR_PERMISSION_DENIED,
                           "Access Denied");
      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      mm_modem_set_current_modes (self->modem, allowed, preferred,
                                  cancellable, cc_wwan_device_network_mode_set_cb,
                                  g_steal_pointer (&task));
    }
}

/**
 * cc_wwan_device_set_current_mode_finish:
 * @self: a #CcWwanDevice
 * @result: a #GAsyncResult
 * @error: a location for #GError or %NULL
 *
 * Get the status whether setting network mode
 * succeeded
 *
 * Returns: %TRUE if network mode was successfully set,
 * %FALSE otherwise.
 */
gboolean
cc_wwan_device_set_current_mode_finish (CcWwanDevice  *self,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
cc_wwan_device_get_current_mode (CcWwanDevice *self,
                                 MMModemMode  *allowed,
                                 MMModemMode  *preferred)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);

  return mm_modem_get_current_modes (self->modem, allowed, preferred);
}

gboolean
cc_wwan_device_is_auto_network (CcWwanDevice *self)
{
  /*
   * XXX: ModemManager Doesn’t have a true API to check
   * if registration is automatic or manual.  So Let’s
   * do some guess work.
   */
  if (self->registration_state == CC_WWAN_REGISTRATION_STATE_DENIED)
    return FALSE;

  return !self->network_is_manual;
}

CcWwanState
cc_wwan_device_get_network_state (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), 0);

  return self->registration_state;
}

gboolean
cc_wwan_device_get_supported_modes (CcWwanDevice *self,
                                    MMModemMode  *allowed,
                                    MMModemMode  *preferred)
{
  g_autofree MMModemModeCombination *modes = NULL;
  guint n_modes, i;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);

  if (!mm_modem_get_supported_modes (self->modem, &modes, &n_modes))
    return FALSE;

  if (allowed)
    *allowed = 0;
  if (preferred)
    *preferred = 0;

  for (i = 0; i < n_modes; i++)
    {
      if (allowed)
        *allowed = *allowed | modes[i].allowed;
      if (preferred)
        *preferred = *preferred | modes[i].preferred;
    }

  return TRUE;
}

gchar *
cc_wwan_device_get_string_from_mode (CcWwanDevice *self,
                                     MMModemMode   allowed,
                                     MMModemMode   preferred)
{
  GString *str;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);
  g_return_val_if_fail (allowed != 0, NULL);

  if (allowed == MM_MODEM_MODE_2G)
    return g_strdup (_("2G Only"));

  if (allowed == MM_MODEM_MODE_3G)
    return g_strdup (_("3G Only"));

  if (allowed == MM_MODEM_MODE_4G)
    return g_strdup (_("4G Only"));

  if (allowed == MM_MODEM_MODE_5G)
    return g_strdup (_("5G Only"));

  str = g_string_sized_new (10);

  if (allowed & MM_MODEM_MODE_2G &&
      allowed & MM_MODEM_MODE_3G &&
      allowed & MM_MODEM_MODE_4G &&
      allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("2G, 3G, 4G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("2G, 3G, 4G (Preferred), 5G"));
      else if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("2G, 3G (Preferred), 4G, 5G"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 3G, 4G, 5G"));
      else
        g_string_append (str, _("2G, 3G, 4G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
      allowed & MM_MODEM_MODE_3G &&
      allowed & MM_MODEM_MODE_4G)
    {
      if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("2G, 3G, 4G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("2G, 3G (Preferred), 4G"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 3G, 4G"));
      else
        g_string_append (str, _("2G, 3G, 4G"));
    }
  else if (allowed & MM_MODEM_MODE_3G &&
      allowed & MM_MODEM_MODE_4G &&
      allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("3G, 4G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("3G, 4G (Preferred), 5G"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("3G (Preferred), 4G, 5G"));
      else
        g_string_append (str, _("3G, 4G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
      allowed & MM_MODEM_MODE_4G &&
      allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("2G, 4G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("2G, 4G (Preferred), 5G"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 4G, 5G"));
      else
        g_string_append (str, _("2G, 4G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
      allowed & MM_MODEM_MODE_3G &&
      allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("2G, 3G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("2G, 3G (Preferred), 5G"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 3G, 5G"));
      else
        g_string_append (str, _("2G, 3G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_3G &&
           allowed & MM_MODEM_MODE_4G)
    {
      if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("3G, 4G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("3G (Preferred), 4G"));
      else
        g_string_append (str, _("3G, 4G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
           allowed & MM_MODEM_MODE_4G)
    {
      if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("2G, 4G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 4G"));
      else
        g_string_append (str, _("2G, 4G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
           allowed & MM_MODEM_MODE_3G)
    {
      if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("2G, 3G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 3G"));
      else
        g_string_append (str, _("2G, 3G"));
    }
  else if (allowed & MM_MODEM_MODE_2G &&
           allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("2G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_2G)
        g_string_append (str, _("2G (Preferred), 5G"));
      else
        g_string_append (str, _("2G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_3G &&
           allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("3G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_3G)
        g_string_append (str, _("3G (Preferred), 5G"));
      else
        g_string_append (str, _("3G, 5G"));
    }
  else if (allowed & MM_MODEM_MODE_4G &&
           allowed & MM_MODEM_MODE_5G)
    {
      if (preferred & MM_MODEM_MODE_5G)
        g_string_append (str, _("4G, 5G (Preferred)"));
      else if (preferred & MM_MODEM_MODE_4G)
        g_string_append (str, _("4G (Preferred), 5G"));
      else
        g_string_append (str, _("4G, 5G"));
    }

  if (!str->len)
    g_string_append (str, C_("Network mode", "Unknown"));

  return g_string_free (str, FALSE);
}

static void
wwan_network_list_free (GList *network_list)
{
  g_list_free_full (network_list, (GDestroyNotify)mm_modem_3gpp_network_free);
}

static void
cc_wwan_device_scan_complete_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  MMModem3gpp *modem_3gpp = (MMModem3gpp *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GList *network_list;

  network_list = mm_modem_3gpp_scan_finish (modem_3gpp, result, &error);

  if (error)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, network_list, (GDestroyNotify)wwan_network_list_free);
}

void
cc_wwan_device_scan_networks (CcWwanDevice        *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  mm_modem_3gpp_scan (self->modem_3gpp, cancellable,
                      cc_wwan_device_scan_complete_cb,
                      g_steal_pointer (&task));
}

GList *
cc_wwan_device_scan_networks_finish (CcWwanDevice  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
cc_wwan_device_register_network_complete_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  CcWwanDevice *self;
  MMModem3gpp *modem_3gpp = (MMModem3gpp *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  if (!mm_modem_3gpp_register_finish (modem_3gpp, result, &error))
    {
      self = g_task_get_source_object (G_TASK (task));

      g_clear_error (&self->error);
      self->error = g_error_copy (error);
      g_warning ("Error: %s", error->message);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);

      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }
}

void
cc_wwan_device_register_network (CcWwanDevice        *self,
                                 const gchar         *network_id,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CC_IS_WWAN_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (network_id && *network_id)
    self->network_is_manual = TRUE;
  else
    self->network_is_manual = FALSE;

  mm_modem_3gpp_register (self->modem_3gpp, network_id, cancellable,
                          cc_wwan_device_register_network_complete_cb,
                          g_steal_pointer (&task));
}

gboolean
cc_wwan_device_register_network_finish (CcWwanDevice *self,
                                        GAsyncResult *result,
                                        GError       **error)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * cc_wwan_device_get_operator_name:
 * @self: a #CcWwanDevice
 *
 * Get the human readable network operator name
 * currently the device is connected to.
 *
 * Returns: (nullable): The operator name or %NULL
 */
const gchar *
cc_wwan_device_get_operator_name (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  if (!self->modem_3gpp)
    return NULL;

  return mm_modem_3gpp_get_operator_name (self->modem_3gpp);
}

gchar *
cc_wwan_device_dup_own_numbers (CcWwanDevice *self)
{
  const char *const *own_numbers;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  own_numbers = mm_modem_get_own_numbers (self->modem);

  if (!own_numbers)
    return NULL;

  return g_strjoinv ("\n", (char **)own_numbers);
}

gchar *
cc_wwan_device_dup_network_type_string (CcWwanDevice *self)
{
  MMModemAccessTechnology type;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  type = mm_modem_get_access_technologies (self->modem);

  return mm_modem_access_technology_build_string_from_mask (type);
}

gchar *
cc_wwan_device_dup_signal_string (CcWwanDevice *self)
{
  MMModemSignal *modem_signal;
  MMSignal *signal;
  GString *str;
  gdouble value;
  gboolean recent;
  guint refresh_rate;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  modem_signal = mm_object_peek_modem_signal (self->mm_object);

  if (modem_signal)
    refresh_rate = mm_modem_signal_get_rate (modem_signal);

  if (!modem_signal || !refresh_rate)
    return g_strdup_printf ("%d%%", mm_modem_get_signal_quality (self->modem, &recent));

  str = g_string_new ("");

  /* Adapted from ModemManager mmcli-modem-signal.c */
  signal = mm_modem_signal_peek_cdma (modem_signal);
  if (signal)
    {
      if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rssi: %.2g dBm ", value);
      if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "ecio: %.2g dBm ", value);
    }

  signal = mm_modem_signal_peek_evdo (modem_signal);
  if (signal)
    {
      if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rssi: %.2g dBm ", value);
      if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "ecio: %.2g dBm ", value);
      if ((value = mm_signal_get_sinr (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "sinr: %.2g dB ", value);
      if ((value = mm_signal_get_io (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "io: %.2g dBm ", value);
    }

  signal = mm_modem_signal_peek_gsm (modem_signal);
  if (signal)
    if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
      g_string_append_printf (str, "rssi: %.2g dBm ", value);

  signal = mm_modem_signal_peek_umts (modem_signal);
  if (signal)
    {
      if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rssi: %.2g dBm ", value);
      if ((value = mm_signal_get_rscp (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rscp: %.2g dBm ", value);
      if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "ecio: %.2g dBm ", value);
    }

  signal = mm_modem_signal_peek_lte (modem_signal);
  if (signal)
    {
      if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rssi: %.2g dBm ", value);
      if ((value = mm_signal_get_rsrq (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rsrq: %.2g dB ", value);
      if ((value = mm_signal_get_rsrp (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "rsrp: %.2g dBm ", value);
      if ((value = mm_signal_get_snr (signal)) != MM_SIGNAL_UNKNOWN)
        g_string_append_printf (str, "snr: %.2g dB ", value);
    }

  return g_string_free (str, FALSE);
}

const gchar *
cc_wwan_device_get_manufacturer (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  return mm_modem_get_manufacturer (self->modem);
}

const gchar *
cc_wwan_device_get_model (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  return mm_modem_get_model (self->modem);
}

const gchar *
cc_wwan_device_get_firmware_version (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  return mm_modem_get_revision (self->modem);
}

const gchar *
cc_wwan_device_get_identifier (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  return mm_modem_get_equipment_identifier (self->modem);
}

const gchar *
cc_wwan_device_get_simple_error (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  if (!self->error)
    return NULL;

  g_dbus_error_strip_remote_error (self->error);

  return cc_wwan_error_get_message (self->error);
}

gboolean
cc_wwan_device_is_nm_device (CcWwanDevice *self,
                             GObject      *nm_device)
{
#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
  g_return_val_if_fail (NM_IS_DEVICE (nm_device), FALSE);

  return g_str_equal (mm_modem_get_primary_port (self->modem),
                      nm_device_get_iface (NM_DEVICE (nm_device)));
#else
  return FALSE;
#endif
}

const gchar *
cc_wwan_device_get_path (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), "");

  return mm_object_get_path (self->mm_object);
}

CcWwanData *
cc_wwan_device_get_data (CcWwanDevice *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (self), NULL);

  return self->wwan_data;
}

gboolean
cc_wwan_device_pin_valid (const gchar *password,
                          MMModemLock  lock)
{
  size_t len;

  g_return_val_if_fail (lock == MM_MODEM_LOCK_SIM_PIN  ||
                        lock == MM_MODEM_LOCK_SIM_PIN2 ||
                        lock == MM_MODEM_LOCK_SIM_PUK  ||
                        lock == MM_MODEM_LOCK_SIM_PUK2, FALSE);
  if (!password)
    return FALSE;

  len = strlen (password);

  if (len < 4 || len > 8)
    return FALSE;

  if (strspn (password, "0123456789") != len)
    return FALSE;

  /*
   * XXX: Can PUK code be something other than 8 digits?
   * 3GPP standard seems mum on this
   */
  if (lock == MM_MODEM_LOCK_SIM_PUK ||
      lock == MM_MODEM_LOCK_SIM_PUK2)
    if (len != 8)
      return FALSE;

  return TRUE;
}
