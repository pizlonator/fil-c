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
#define G_LOG_DOMAIN "cc-wwan-details-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wwan-details-dialog.h"
#include "cc-wwan-resources.h"

/**
 * @short_description: Dialog to Show Device Details
 */

struct _CcWwanDetailsDialog
{
  GtkDialog     parent_instance;

  GtkLabel     *device_identifier;
  GtkLabel     *device_model;
  GtkLabel     *firmware_version;
  GtkLabel     *identifier_label;
  GtkLabel     *manufacturer;
  GtkLabel     *network_status;
  GtkLabel     *network_type;
  GtkLabel     *operator_name;
  GtkLabel     *own_numbers;
  GtkLabel     *signal_strength;

  CcWwanDevice *device;
};

G_DEFINE_TYPE (CcWwanDetailsDialog, cc_wwan_details_dialog, GTK_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_wwan_details_update_network_status (CcWwanDetailsDialog *self)
{
  CcWwanState state;

  g_assert (CC_IS_WWAN_DETAILS_DIALOG (self));

  state = cc_wwan_device_get_network_state (self->device);

  switch (state)
    {
    case CC_WWAN_REGISTRATION_STATE_IDLE:
      gtk_label_set_label (self->network_status, _("Not Registered"));
      break;

    case CC_WWAN_REGISTRATION_STATE_REGISTERED:
      gtk_label_set_label (self->network_status, _("Registered"));
      break;

    case CC_WWAN_REGISTRATION_STATE_ROAMING:
      gtk_label_set_label (self->network_status, _("Roaming"));
      break;

    case CC_WWAN_REGISTRATION_STATE_SEARCHING:
      gtk_label_set_label (self->network_status, _("Searching"));
      break;

    case CC_WWAN_REGISTRATION_STATE_DENIED:
      gtk_label_set_label (self->network_status, _("Denied"));
      break;

    default:
      gtk_label_set_label (self->network_status, _("Unknown"));
      break;
    }
}

static void
cc_wwan_details_signal_changed_cb (CcWwanDetailsDialog *self)
{
  g_autofree gchar *network_type_string = NULL;
  g_autofree gchar *signal_string = NULL;
  const gchar *operator_name;

  g_assert (CC_IS_WWAN_DETAILS_DIALOG (self));

  operator_name = cc_wwan_device_get_operator_name (self->device);
  if (operator_name)
    gtk_label_set_label (self->operator_name, operator_name);

  network_type_string = cc_wwan_device_dup_network_type_string (self->device);
  if (network_type_string)
    gtk_label_set_label (self->network_type, network_type_string);

  signal_string = cc_wwan_device_dup_signal_string (self->device);
  if (signal_string)
    gtk_label_set_label (self->signal_strength, signal_string);

  cc_wwan_details_update_network_status (self);
}

static void
cc_wwan_details_update_hardware_details (CcWwanDetailsDialog *self)
{
  const gchar *str;

  g_assert (CC_IS_WWAN_DETAILS_DIALOG (self));

  str = cc_wwan_device_get_manufacturer (self->device);
  if (str)
    gtk_label_set_label (self->manufacturer, str);

  str = cc_wwan_device_get_model (self->device);
  if (str)
    gtk_label_set_label (self->device_model, str);

  str = cc_wwan_device_get_firmware_version (self->device);
  if (str)
    gtk_label_set_label (self->firmware_version, str);

  str = cc_wwan_device_get_identifier (self->device);
  if (str)
    gtk_label_set_label (self->device_identifier, str);
}

static void
cc_wwan_details_dialog_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcWwanDetailsDialog *self = CC_WWAN_DETAILS_DIALOG (object);

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
cc_wwan_details_dialog_constructed (GObject *object)
{
  CcWwanDetailsDialog *self = CC_WWAN_DETAILS_DIALOG (object);
  g_autofree char *numbers = NULL;

  G_OBJECT_CLASS (cc_wwan_details_dialog_parent_class)->constructed (object);

  g_signal_connect_object (self->device, "notify::signal",
                           G_CALLBACK (cc_wwan_details_signal_changed_cb),
                           self, G_CONNECT_SWAPPED);

  numbers = cc_wwan_device_dup_own_numbers (self->device);
  gtk_widget_set_visible (GTK_WIDGET (self->own_numbers), !!numbers);

  if (numbers)
    gtk_label_set_text (self->own_numbers, numbers);

  cc_wwan_details_signal_changed_cb (self);
  cc_wwan_details_update_hardware_details (self);
}

static void
cc_wwan_details_dialog_dispose (GObject *object)
{
  CcWwanDetailsDialog *self = CC_WWAN_DETAILS_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_details_dialog_parent_class)->dispose (object);
}

static void
cc_wwan_details_dialog_class_init (CcWwanDetailsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_details_dialog_set_property;
  object_class->constructed  = cc_wwan_details_dialog_constructed;
  object_class->dispose = cc_wwan_details_dialog_dispose;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-details-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, device_identifier);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, device_model);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, firmware_version);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, identifier_label);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, manufacturer);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, network_status);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, network_type);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, operator_name);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, own_numbers);
  gtk_widget_class_bind_template_child (widget_class, CcWwanDetailsDialog, signal_strength);
}

static void
cc_wwan_details_dialog_init (CcWwanDetailsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWindow *
cc_wwan_details_dialog_new (GtkWindow    *parent_window,
                            CcWwanDevice *device)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  return GTK_WINDOW (g_object_new (CC_TYPE_WWAN_DETAILS_DIALOG,
                                   "transient-for", parent_window,
                                   "use-header-bar", 1,
                                   "device", device,
                                   NULL));
}
