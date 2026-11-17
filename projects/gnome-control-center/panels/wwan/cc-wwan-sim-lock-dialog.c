/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-network-dialog.c
 *
 * Copyright 2019,2022 Purism SPC
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
#define G_LOG_DOMAIN "cc-wwan-sim-lock-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wwan-sim-lock-dialog.h"
#include "cc-wwan-resources.h"

/**
 * @short_description: Dialog to manage SIM Locks like PIN
 */

#define PIN_MINIMUM_LENGTH 4
#define PIN_MAXIMUM_LENGTH 8

struct _CcWwanSimLockDialog
{
  GtkDialog     parent_instance;

  CcWwanDevice *device;

  GtkButton    *apply_button;
  GtkStack     *button_stack;
  GtkGrid      *lock_change_grid;
  AdwSwitchRow *lock_row;
  GtkEntry     *new_pin_entry;
  GtkButton    *next_button;
  GtkEntry     *pin_confirm_entry;
  GtkEntry     *pin_entry;
  GtkStack     *pin_settings_stack;
};

G_DEFINE_TYPE (CcWwanSimLockDialog, cc_wwan_sim_lock_dialog, GTK_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_wwan_sim_lock_changed_cb (CcWwanSimLockDialog *self)
{
  gboolean row_enabled, lock_enabled;

  lock_enabled = cc_wwan_device_get_sim_lock (self->device);
  row_enabled = adw_switch_row_get_active (self->lock_row);

  gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), lock_enabled != row_enabled);
  gtk_widget_set_visible (GTK_WIDGET (self->lock_change_grid), row_enabled && lock_enabled);
}

static void
cc_wwan_pin_next_clicked_cb (CcWwanSimLockDialog *self)
{
  gtk_stack_set_visible_child_name (self->pin_settings_stack, "pin-entry");
  gtk_editable_set_text (GTK_EDITABLE (self->pin_entry), "");

  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), FALSE);
  gtk_stack_set_visible_child (self->button_stack,
                               GTK_WIDGET (self->apply_button));
}

static void
cc_wwan_pin_apply_clicked_cb (CcWwanSimLockDialog *self)
{
  const gchar *pin, *new_pin;
  gboolean row_enabled, lock_enabled;

  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  lock_enabled = cc_wwan_device_get_sim_lock (self->device);
  row_enabled = adw_switch_row_get_active (self->lock_row);
  pin = gtk_editable_get_text (GTK_EDITABLE (self->pin_entry));
  new_pin = gtk_editable_get_text (GTK_EDITABLE (self->new_pin_entry));

  if (lock_enabled != row_enabled)
    {
      if (row_enabled)
        cc_wwan_device_enable_pin (self->device, pin, NULL, NULL, NULL);
      else
        cc_wwan_device_disable_pin (self->device, pin, NULL, NULL, NULL);

      return;
    }

  cc_wwan_device_change_pin (self->device, pin, new_pin, NULL, NULL, NULL);
}

static void
cc_wwan_pin_entry_text_inserted_cb (CcWwanSimLockDialog *self,
                                    gchar               *new_text,
                                    gint                 new_text_length,
                                    gpointer             position,
                                    GtkEditable         *editable)
{
  size_t digit_end;
  size_t len;

  if (!new_text || !*new_text)
    return;

  if (new_text_length == 1 && g_ascii_isdigit (*new_text))
    return;

  if (new_text_length == -1)
    len = strlen (new_text);
  else
    len = new_text_length;

  if (len == 1 && g_ascii_isdigit (*new_text))
    return;

  digit_end = strspn (new_text, "1234567890");

  /* The maximum length possible for PIN is 8 */
  if (len <= 8 &&  digit_end == len)
    return;

  g_signal_stop_emission_by_name (editable, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (editable));
}

static void
cc_wwan_pin_entry_changed_cb (CcWwanSimLockDialog *self)
{
  const gchar *new_pin, *confirm_pin;

  new_pin = gtk_editable_get_text (GTK_EDITABLE (self->new_pin_entry));
  confirm_pin = gtk_editable_get_text (GTK_EDITABLE (self->pin_confirm_entry));
  gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), FALSE);

  /* A PIN should have a minimum length of 4 */
  if (!new_pin || !confirm_pin || strlen (new_pin) < 4)
    return;

  if (g_str_equal (new_pin, confirm_pin))
    gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), TRUE);
}


static void
cc_wwan_pin_entered_cb (CcWwanSimLockDialog *self)
{
  const gchar *pin;
  gsize len;
  gboolean enable_apply;

  pin = gtk_editable_get_text (GTK_EDITABLE (self->pin_entry));

  if (!pin || !*pin)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), FALSE);
      return;
    }

  len = strlen (pin);
  enable_apply = len >= PIN_MINIMUM_LENGTH && len <= PIN_MAXIMUM_LENGTH;

  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), enable_apply);
}

static void
cc_wwan_sim_lock_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcWwanSimLockDialog *self = (CcWwanSimLockDialog *)object;

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
cc_wwan_sim_lock_dialog_show (GtkWidget *widget)
{
  CcWwanSimLockDialog *self = (CcWwanSimLockDialog *)widget;
  gboolean lock_enabled;

  gtk_editable_set_text (GTK_EDITABLE (self->pin_entry), "");
  gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), FALSE);

  lock_enabled = cc_wwan_device_get_sim_lock (self->device);
  g_object_set (self->lock_row, "active", lock_enabled, NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->lock_change_grid), lock_enabled);

  gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), FALSE);
  gtk_stack_set_visible_child (self->button_stack,
                               GTK_WIDGET (self->next_button));
  gtk_button_set_label (self->apply_button, _("_Set"));

  gtk_stack_set_visible_child_name (self->pin_settings_stack, "pin-settings");

  gtk_editable_set_text (GTK_EDITABLE (self->pin_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->new_pin_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->pin_confirm_entry), "");

  GTK_WIDGET_CLASS (cc_wwan_sim_lock_dialog_parent_class)->show (widget);
}

static void
cc_wwan_sim_lock_dialog_dispose (GObject *object)
{
  CcWwanSimLockDialog *self = (CcWwanSimLockDialog *)object;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_sim_lock_dialog_parent_class)->dispose (object);
}

static void
cc_wwan_sim_lock_dialog_class_init (CcWwanSimLockDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_sim_lock_dialog_set_property;
  object_class->dispose = cc_wwan_sim_lock_dialog_dispose;

  widget_class->show = cc_wwan_sim_lock_dialog_show;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-sim-lock-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, apply_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, button_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, lock_change_grid);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, lock_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, new_pin_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, next_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, pin_confirm_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, pin_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanSimLockDialog, pin_settings_stack);

  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_sim_lock_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_pin_next_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_pin_apply_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_pin_entry_text_inserted_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_pin_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_pin_entered_cb);
}

static void
cc_wwan_sim_lock_dialog_init (CcWwanSimLockDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWindow *
cc_wwan_sim_lock_dialog_new (GtkWindow    *parent_window,
                             CcWwanDevice *device)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  return GTK_WINDOW (g_object_new (CC_TYPE_WWAN_SIM_LOCK_DIALOG,
                                   "transient-for", parent_window,
                                   "use-header-bar", 1,
                                   "device", device,
                                   NULL));
}
