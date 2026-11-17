/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-device-page.c
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
#define G_LOG_DOMAIN "cc-wwan-device-page"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>
#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>

#include "cc-list-row.h"
#include "cc-wwan-data.h"
#include "cc-wwan-mode-dialog.h"
#include "cc-wwan-network-dialog.h"
#include "cc-wwan-details-dialog.h"
#include "cc-wwan-sim-lock-dialog.h"
#include "cc-wwan-apn-dialog.h"
#include "cc-wwan-device-page.h"
#include "cc-wwan-resources.h"

#include "shell/cc-application.h"
#include "shell/cc-object-storage.h"

/**
 * @short_description: Device settings page
 * @include: "cc-wwan-device-page.h"
 *
 * The Device page allows users to configure device
 * settings.  Please note that there is no one-to-one
 * maping for a device settings page and a physical
 * device.  Say, if a device have two SIM card slots,
 * there should be two device pages, one for each SIM.
 */

struct _CcWwanDevicePage
{
  GtkBox         parent_instance;

  GtkListBox    *advanced_settings_list;
  CcListRow     *apn_settings_row;
  AdwSwitchRow  *data_enable_row;
  AdwSwitchRow  *data_roaming_row;
  GtkListBox    *data_settings_list;
  CcListRow     *details_row;
  GtkStack      *main_stack;
  CcListRow     *network_mode_row;
  CcListRow     *network_name_row;
  GtkListBox    *network_settings_list;
  CcListRow     *sim_lock_row;
  GtkButton     *unlock_button;

  AdwToastOverlay *toast_overlay;

  CcWwanDevice  *device;
  CcWwanData    *wwan_data;
  GDBusProxy    *wwan_proxy;

  GtkWindow     *apn_dialog;
  GtkWindow     *details_dialog;
  GtkWindow     *network_mode_dialog;
  GtkWindow     *network_dialog;
  GtkWindow     *sim_lock_dialog;

  gint                 sim_index;
  /* Set if a change is triggered in a signal’s callback,
   * to avoid re-triggering of callback.  This is used
   * instead of blocking handlers where the signal may be
   * emitted async and the block/unblock may not work right
   */
  gboolean is_self_change;
  gboolean is_retry;
};

G_DEFINE_TYPE (CcWwanDevicePage, cc_wwan_device_page, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
wwan_device_page_handle_data_row (CcWwanDevicePage *self,
                                  AdwSwitchRow     *data_row)
{
  gboolean active;

  /* The user dismissed the dialog for selecting default APN */
  if (cc_wwan_data_get_default_apn (self->wwan_data) == NULL)
    {
      self->is_self_change = TRUE;
      gtk_widget_activate (GTK_WIDGET (data_row));

      return;
    }

  active = adw_switch_row_get_active (data_row);

  if (data_row == self->data_enable_row)
    cc_wwan_data_set_enabled (self->wwan_data, active);
  else
    cc_wwan_data_set_roaming_enabled (self->wwan_data, active);

  cc_wwan_data_save_settings (self->wwan_data, NULL, NULL, NULL);
}

static gboolean
wwan_apn_dialog_closed_cb (CcWwanDevicePage *self)
{
  AdwSwitchRow *data_row;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return FALSE;

  data_row = g_object_get_data (G_OBJECT (self->apn_dialog), "row");
  g_object_set_data (G_OBJECT (self->apn_dialog), "row", NULL);

  if (data_row)
    wwan_device_page_handle_data_row (self, data_row);

  return FALSE;
}

static void
wwan_data_show_apn_dialog (CcWwanDevicePage *self)
{
  GtkWindow *top_level;

  top_level = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

  if (!self->apn_dialog)
    {
      self->apn_dialog = cc_wwan_apn_dialog_new (top_level, self->device);
      g_signal_connect_object (self->apn_dialog, "unmap",
                               G_CALLBACK (wwan_apn_dialog_closed_cb),
                               self, G_CONNECT_SWAPPED);
    }

  gtk_widget_set_visible (GTK_WIDGET (self->apn_dialog), TRUE);
}

static GcrPrompt *
cc_wwan_device_page_new_prompt (CcWwanDevicePage *self,
                                MMModemLock       lock)
{
  GcrPrompt *prompt;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *description = NULL;
  g_autofree gchar *warning = NULL;
  const gchar *message = NULL;
  guint num;

  prompt = GCR_PROMPT (gcr_system_prompt_open (-1, NULL, &error));

  if (error)
    {
      g_warning ("Error opening Prompt: %s", error->message);
      return NULL;
    }

  gcr_prompt_set_title (prompt, _("Unlock SIM card"));
  gcr_prompt_set_continue_label (prompt, _("Unlock"));
  gcr_prompt_set_cancel_label (prompt, _("Cancel"));

  if (lock == MM_MODEM_LOCK_SIM_PIN)
    {
      description = g_strdup_printf (_("Please provide PIN code for SIM %d"), self->sim_index);
      message = _("Enter PIN to unlock your SIM card");
    }
  else if (lock == MM_MODEM_LOCK_SIM_PUK)
    {
      description = g_strdup_printf (_("Please provide PUK code for SIM %d"), self->sim_index);
      message = _("Enter PUK to unlock your SIM card");
    }
  else
    {
      g_warn_if_reached ();
      g_object_unref (prompt);

      return NULL;
    }

  gcr_prompt_set_description (prompt, description);
  gcr_prompt_set_message (prompt, message);

  num = cc_wwan_device_get_unlock_retries (self->device, lock);

  if (num != MM_UNLOCK_RETRIES_UNKNOWN)
    {
      if (self->is_retry)
        warning = g_strdup_printf (ngettext ("Wrong password entered. You have %1$u try left",
                                             "Wrong password entered. You have %1$u tries left", num), num);
      else
        warning = g_strdup_printf (ngettext ("You have %u try left",
                                             "You have %u tries left", num), num);
    }
  else if (self->is_retry)
    {
      warning = g_strdup (_("Wrong password entered."));
    }

  gcr_prompt_set_warning (prompt, warning);

  return prompt;
}

static void
wwan_update_unlock_button (CcWwanDevicePage *self)
{
  gtk_button_set_label (self->unlock_button, _("Unlock"));
  gtk_widget_set_sensitive (GTK_WIDGET (self->unlock_button), TRUE);
}

static void
cc_wwan_device_page_unlocked_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  CcWwanDevicePage *self = user_data;
  wwan_update_unlock_button (self);
}

static void
wwan_device_unlock_clicked_cb (CcWwanDevicePage *self)
{
  g_autoptr(GError) error = NULL;
  GcrPrompt *prompt;
  const gchar *password, *warning;
  const gchar *pin = "";
  const gchar *puk = "";
  MMModemLock lock;

  lock = cc_wwan_device_get_lock (self->device);
  password = "";

  if (lock != MM_MODEM_LOCK_SIM_PIN &&
      lock != MM_MODEM_LOCK_SIM_PUK)
    g_return_if_reached ();

  if (lock == MM_MODEM_LOCK_SIM_PUK)
    {
      prompt = cc_wwan_device_page_new_prompt (self, lock);

      warning = _("PUK code should be an 8 digit number");
      while (password && !cc_wwan_device_pin_valid (password, lock))
        {
          password = gcr_prompt_password (prompt, NULL, &error);
          gcr_prompt_set_warning (prompt, warning);
        }

      puk = g_strdup (password);
      password = "";
      gcr_prompt_close (prompt);
      g_object_unref (prompt);

      if (error)
        g_warning ("Error: %s", error->message);

      /* Error or User cancelled PUK */
      if (!puk)
        return;
    }

  prompt = cc_wwan_device_page_new_prompt (self, MM_MODEM_LOCK_SIM_PIN);
  if (lock == MM_MODEM_LOCK_SIM_PUK)
    {
      gcr_prompt_set_password_new (prompt, TRUE);
      gcr_prompt_set_message (prompt, _("Enter New PIN"));
      gcr_prompt_set_warning (prompt, "");
    }

  warning = _("PIN code should be a 4-8 digit number");
  while (password && !cc_wwan_device_pin_valid (password, MM_MODEM_LOCK_SIM_PIN))
    {
      password = gcr_prompt_password (prompt, NULL, &error);
      gcr_prompt_set_warning (prompt, warning);
    }

  pin = g_strdup (password);
  gcr_prompt_close (prompt);
  g_object_unref (prompt);

  if (error)
    g_warning ("Error: %s", error->message);

  /* Error or User cancelled PIN */
  if (!pin)
    return;

  gtk_button_set_label (self->unlock_button, _("Unlocking…"));
  gtk_widget_set_sensitive (GTK_WIDGET (self->unlock_button), FALSE);

  if (lock == MM_MODEM_LOCK_SIM_PIN)
    cc_wwan_device_send_pin (self->device, pin,
                             NULL, /* cancellable */
                             cc_wwan_device_page_unlocked_cb,
                             self);
  else if (lock == MM_MODEM_LOCK_SIM_PUK)
    {
      cc_wwan_device_send_puk (self->device, puk, pin,
                               NULL, /* Cancellable */
                               cc_wwan_device_page_unlocked_cb,
                               self);
    }
  else
    {
      g_warn_if_reached ();
    }
}

static void
wwan_data_settings_changed_cb (CcWwanDevicePage *self,
                               GParamSpec       *pspec,
                               AdwSwitchRow     *data_row)
{
  if (self->is_self_change)
    {
      self->is_self_change = FALSE;
      return;
    }

  if (cc_wwan_data_get_default_apn (self->wwan_data) == NULL)
    {
      wwan_data_show_apn_dialog (self);
      g_object_set_data (G_OBJECT (self->apn_dialog), "row", data_row);
    }
  else
    {
      wwan_device_page_handle_data_row (self, data_row);
    }
}

static void
wwan_network_settings_activated_cb (CcWwanDevicePage *self,
                                    CcListRow        *row)
{
  GtkWidget *dialog;
  GtkWindow *top_level;

  top_level = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

  if (row == self->network_mode_row)
    {
      if (!self->network_mode_dialog)
        self->network_mode_dialog = cc_wwan_mode_dialog_new (top_level, self->device);

      dialog = GTK_WIDGET (self->network_mode_dialog);
    }
  else if (row == self->network_name_row)
    {
      if (!self->network_dialog)
        self->network_dialog = cc_wwan_network_dialog_new (top_level, self->device);

      dialog = GTK_WIDGET (self->network_dialog);
    }
  else
    {
      return;
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
wwan_advanced_settings_activated_cb (CcWwanDevicePage *self,
                                     CcListRow        *row)
{
  GtkWindow *top_level;

  top_level = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

  if (row == self->sim_lock_row)
    {
      if (!self->sim_lock_dialog)
        self->sim_lock_dialog = cc_wwan_sim_lock_dialog_new (top_level, self->device);
      gtk_widget_set_visible (GTK_WIDGET (self->sim_lock_dialog), TRUE);
    }
  else if (row == self->details_row)
    {
      if (!self->details_dialog)
        self->details_dialog = cc_wwan_details_dialog_new (top_level, self->device);
      gtk_widget_set_visible (GTK_WIDGET (self->details_dialog), TRUE);
    }
  else if (row == self->apn_settings_row)
    {
      wwan_data_show_apn_dialog (self);
    }
  else
    {
      g_return_if_reached ();
    }
}

static void
cc_wwan_device_page_update_data (CcWwanDevicePage *self)
{
  gboolean has_data;

  if (self->wwan_data == cc_wwan_device_get_data (self->device))
    return;

  self->wwan_data = cc_wwan_device_get_data (self->device);
  has_data = self->wwan_data != NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (self->data_settings_list), has_data);
  gtk_widget_set_sensitive (GTK_WIDGET (self->apn_settings_row), has_data);

  if (!has_data)
    return;

  g_signal_handlers_block_by_func (self->data_roaming_row,
                                   wwan_data_settings_changed_cb, self);
  g_signal_handlers_block_by_func (self->data_enable_row,
                                   wwan_data_settings_changed_cb, self);

  g_object_set (self->data_roaming_row, "active",
                cc_wwan_data_get_roaming_enabled (self->wwan_data), NULL);

  g_object_set (self->data_enable_row, "active",
                cc_wwan_data_get_enabled (self->wwan_data), NULL);

  g_signal_handlers_unblock_by_func (self->data_roaming_row,
                                     wwan_data_settings_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->data_enable_row,
                                     wwan_data_settings_changed_cb, self);
}

static void
cc_wwan_device_page_update (CcWwanDevicePage *self)
{
  GtkStack *main_stack;
  MMModemLock lock;

  main_stack = self->main_stack;
  if (!cc_wwan_device_has_sim (self->device))
    gtk_stack_set_visible_child_name (main_stack, "no-sim-view");
  else if ((lock = cc_wwan_device_get_lock (self->device)) == MM_MODEM_LOCK_SIM_PIN ||
           lock == MM_MODEM_LOCK_SIM_PUK)
    gtk_stack_set_visible_child_name (main_stack, "sim-lock-view");
  else
    gtk_stack_set_visible_child_name (main_stack, "settings-view");
}

static void
cc_wwan_locks_changed_cb (CcWwanDevicePage *self)
{
  const gchar *label;

  if (cc_wwan_device_get_sim_lock (self->device))
    label = _("Enabled");
  else
    label = _("Disabled");

  cc_list_row_set_secondary_label (self->sim_lock_row, label);
  cc_wwan_device_page_update (self);
}

static void
cc_wwan_device_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcWwanDevicePage *self = (CcWwanDevicePage *)object;

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wwan_device_page_constructed (GObject *object)
{
  CcWwanDevicePage *self = (CcWwanDevicePage *)object;

  G_OBJECT_CLASS (cc_wwan_device_page_parent_class)->constructed (object);

  cc_wwan_device_page_update_data (self);

  g_object_bind_property (self->device, "operator-name",
                          self->network_name_row, "secondary-label",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->device, "network-mode",
                          self->network_mode_row, "secondary-label",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_signal_connect_object (self->device, "notify::enabled-locks",
                           (GCallback)cc_wwan_locks_changed_cb,
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->device, "notify::has-data",
                           (GCallback)cc_wwan_device_page_update_data,
                           self, G_CONNECT_SWAPPED);

  cc_wwan_device_page_update (self);
  cc_wwan_locks_changed_cb (self);
}

static void
cc_wwan_device_page_dispose (GObject *object)
{
  CcWwanDevicePage *self = (CcWwanDevicePage *)object;

  g_clear_pointer (&self->apn_dialog, gtk_window_destroy);
  g_clear_pointer (&self->details_dialog, gtk_window_destroy);
  g_clear_pointer (&self->network_mode_dialog, gtk_window_destroy);
  g_clear_pointer (&self->network_dialog, gtk_window_destroy);
  g_clear_pointer (&self->sim_lock_dialog, gtk_window_destroy);

  g_clear_object (&self->wwan_proxy);
  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_device_page_parent_class)->dispose (object);
}

static void
cc_wwan_device_page_class_init (CcWwanDevicePageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_device_page_set_property;
  object_class->constructed  = cc_wwan_device_page_constructed;
  object_class->dispose = cc_wwan_device_page_dispose;

  g_type_ensure (CC_TYPE_WWAN_DEVICE);

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-device-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, advanced_settings_list);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, apn_settings_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, data_enable_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, data_roaming_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, data_settings_list);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, details_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, network_mode_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, network_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, network_settings_list);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, sim_lock_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDevicePage, unlock_button);

  gtk_widget_class_bind_template_callback (widget_class, wwan_device_unlock_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, wwan_data_settings_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, wwan_network_settings_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, wwan_advanced_settings_activated_cb);
}

static void
cc_wwan_device_page_init (CcWwanDevicePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_wwan_error_changed_cb (CcWwanDevicePage *self)
{
  AdwToast *toast;
  const gchar *message;

  message = cc_wwan_device_get_simple_error (self->device);

  if (!message)
    return;

  toast = adw_toast_new (message);
  adw_toast_overlay_add_toast (self->toast_overlay, toast);
}

CcWwanDevicePage *
cc_wwan_device_page_new (CcWwanDevice *device,
                         GtkWidget    *toast_overlay)
{
  CcWwanDevicePage *self;

  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  self = g_object_new (CC_TYPE_WWAN_DEVICE_PAGE,
                       "device", device,
                       NULL);

  self->toast_overlay = ADW_TOAST_OVERLAY (toast_overlay);

  g_signal_connect_object (self->device, "notify::error",
                           G_CALLBACK (cc_wwan_error_changed_cb),
                           self, G_CONNECT_SWAPPED);

  return self;
}

CcWwanDevice *
cc_wwan_device_page_get_device (CcWwanDevicePage *self)
{
  g_return_val_if_fail (CC_IS_WWAN_DEVICE_PAGE (self), NULL);

  return self->device;
}

void
cc_wwan_device_page_set_sim_index (CcWwanDevicePage *self,
                                   gint              sim_index)
{
  g_return_if_fail (CC_IS_WWAN_DEVICE_PAGE (self));
  g_return_if_fail (sim_index >= 1);

  self->sim_index = sim_index;
}
