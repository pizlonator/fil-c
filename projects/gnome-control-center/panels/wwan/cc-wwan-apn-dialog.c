/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-apn-dialog.c
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
#define G_LOG_DOMAIN "cc-wwan-apn-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wwan-device.h"
#include "cc-wwan-data.h"
#include "cc-wwan-apn-dialog.h"
#include "cc-wwan-resources.h"

/**
 * @short_description: Dialog to manage Internet Access Points
 */

struct _CcWwanApnDialog
{
  GtkDialog          parent_instance;

  GtkButton         *add_button;
  GtkButton         *back_button;
  GtkButton         *save_button;
  GtkEntry          *apn_entry;
  GtkEntry          *name_entry;
  GtkEntry          *password_entry;
  GtkEntry          *username_entry;
  GtkGrid           *apn_edit_view;
  GtkListBox        *apn_list;
  GtkCheckButton    *apn_radio_button;
  GtkScrolledWindow *apn_list_view;
  GtkStack          *apn_settings_stack;

  CcWwanData        *wwan_data;
  CcWwanDataApn     *apn_to_save;   /* The APN currently being edited */
  CcWwanDevice      *device;

  gboolean           enable_data;
  gboolean           enable_roaming;
};

G_DEFINE_TYPE (CcWwanApnDialog, cc_wwan_apn_dialog, GTK_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

#define CC_TYPE_WWAN_APN_ROW (cc_wwan_apn_row_get_type())
G_DECLARE_FINAL_TYPE (CcWwanApnRow, cc_wwan_apn_row, CC, WWAN_APN_ROW, GtkListBoxRow)

struct _CcWwanApnRow
{
  GtkListBoxRow   parent_instance;
  GtkCheckButton *radio_button;
  CcWwanDataApn  *apn;
};

G_DEFINE_TYPE (CcWwanApnRow, cc_wwan_apn_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_wwan_apn_row_finalize (GObject *object)
{
  CcWwanApnRow *row = (CcWwanApnRow *)object;

  g_clear_object (&row->apn);

  G_OBJECT_CLASS (cc_wwan_apn_row_parent_class)->finalize (object);
}

static void
cc_wwan_apn_row_class_init (CcWwanApnRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_wwan_apn_row_finalize;
}

static void
cc_wwan_apn_row_init (CcWwanApnRow *row)
{
}

static void
cc_wwan_apn_back_clicked_cb (CcWwanApnDialog *self)
{
  GtkWidget *view;

  view = gtk_stack_get_visible_child (self->apn_settings_stack);

  if (view == GTK_WIDGET (self->apn_edit_view))
    {
      gtk_widget_set_visible (GTK_WIDGET (self->save_button), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->add_button), TRUE);
      gtk_stack_set_visible_child (self->apn_settings_stack,
                                   GTK_WIDGET (self->apn_list_view));
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
    }
}

static void
cc_wwan_apn_add_clicked_cb (CcWwanApnDialog *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->apn_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->username_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), "");

  gtk_widget_set_visible (GTK_WIDGET (self->add_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->save_button), TRUE);
  self->apn_to_save = NULL;
  gtk_stack_set_visible_child (self->apn_settings_stack,
                               GTK_WIDGET (self->apn_edit_view));
}

static void
cc_wwan_apn_save_clicked_cb (CcWwanApnDialog *self)
{
  const gchar *name, *apn_name;
  CcWwanDataApn *apn;

  apn = self->apn_to_save;
  self->apn_to_save = NULL;

  name = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
  apn_name = gtk_editable_get_text (GTK_EDITABLE (self->apn_entry));

  if (!apn)
    apn = cc_wwan_data_apn_new ();

  cc_wwan_data_apn_set_name (apn, name);
  cc_wwan_data_apn_set_apn (apn, apn_name);
  cc_wwan_data_apn_set_username (apn, gtk_editable_get_text (GTK_EDITABLE (self->username_entry)));
  cc_wwan_data_apn_set_password (apn, gtk_editable_get_text (GTK_EDITABLE (self->password_entry)));

  cc_wwan_data_save_apn (self->wwan_data, apn, NULL, NULL, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->save_button), FALSE);
  gtk_stack_set_visible_child (self->apn_settings_stack,
                               GTK_WIDGET (self->apn_list_view));
}

static void
cc_wwan_apn_entry_changed_cb (CcWwanApnDialog *self)
{
  GtkWidget *widget;
  const gchar *str;
  gboolean valid_name, valid_apn;

  widget = GTK_WIDGET (self->name_entry);
  str = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
  valid_name = str && *str;

  if (valid_name)
    gtk_widget_remove_css_class (widget, "error");
  else
    gtk_widget_add_css_class (widget, "error");

  widget = GTK_WIDGET (self->apn_entry);
  str = gtk_editable_get_text (GTK_EDITABLE (self->apn_entry));
  valid_apn = str && *str;

  if (valid_apn)
    gtk_widget_remove_css_class (widget, "error");
  else
    gtk_widget_add_css_class (widget, "error");

  gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), valid_name && valid_apn);
}

static void
cc_wwan_apn_activated_cb (CcWwanApnDialog *self,
                          CcWwanApnRow    *row)
{
  gtk_check_button_set_active (GTK_CHECK_BUTTON (row->radio_button), TRUE);
}

static void
cc_wwan_apn_changed_cb (CcWwanApnDialog *self,
                        GtkWidget       *widget)
{
  CcWwanApnRow *row;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (widget)))
    return;

  widget = gtk_widget_get_ancestor (widget, CC_TYPE_WWAN_APN_ROW);
  row = CC_WWAN_APN_ROW (widget);

  if (cc_wwan_data_set_default_apn (self->wwan_data, row->apn))
    cc_wwan_data_save_settings (self->wwan_data, NULL, NULL, NULL);
}

static void
cc_wwan_apn_edit_clicked_cb (CcWwanApnDialog *self,
                             GtkButton       *button)
{
  CcWwanDataApn *apn;
  CcWwanApnRow *row;
  GtkWidget *widget;

  widget = gtk_widget_get_ancestor (GTK_WIDGET (button), CC_TYPE_WWAN_APN_ROW);
  row = CC_WWAN_APN_ROW (widget);
  apn = row->apn;
  self->apn_to_save = apn;

  gtk_widget_set_visible (GTK_WIDGET (self->save_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->add_button), FALSE);

  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), cc_wwan_data_apn_get_name (apn));
  gtk_editable_set_text (GTK_EDITABLE (self->apn_entry), cc_wwan_data_apn_get_apn (apn));
  gtk_editable_set_text (GTK_EDITABLE (self->username_entry), cc_wwan_data_apn_get_username (apn));
  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), cc_wwan_data_apn_get_password (apn));

  gtk_stack_set_visible_child (self->apn_settings_stack,
                               GTK_WIDGET (self->apn_edit_view));
}

static GtkWidget *
cc_wwan_apn_dialog_row_new (CcWwanDataApn   *apn,
                            CcWwanApnDialog *self)
{
  CcWwanApnRow *row;
  GtkWidget *grid, *name_label, *apn_label, *radio, *edit_button;

  row = g_object_new (CC_TYPE_WWAN_APN_ROW, NULL);

  grid = g_object_new (GTK_TYPE_GRID,
                       "margin-top", 6,
                       "margin-bottom", 6,
                       "margin-start", 6,
                       "margin-end", 6,
                       NULL);

  radio = gtk_check_button_new ();
  row->radio_button = GTK_CHECK_BUTTON (radio);
  gtk_check_button_set_group (row->radio_button, self->apn_radio_button);
  gtk_widget_set_margin_end (radio, 12);
  gtk_grid_attach (GTK_GRID (grid), radio, 0, 0, 1, 2);
  row->apn = g_object_ref (apn);

  if (cc_wwan_data_get_default_apn (self->wwan_data) == apn)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
  g_signal_connect_object (radio, "toggled",
                           G_CALLBACK (cc_wwan_apn_changed_cb),
                           self, G_CONNECT_SWAPPED);

  name_label = gtk_label_new (cc_wwan_data_apn_get_name (apn));
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_halign (name_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (name_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), name_label, 1, 0, 1, 1);

  apn_label = gtk_label_new (cc_wwan_data_apn_get_apn (apn));
  gtk_label_set_ellipsize (GTK_LABEL (apn_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_halign (apn_label, GTK_ALIGN_START);
  gtk_widget_add_css_class (apn_label, "dim-label");
  gtk_grid_attach (GTK_GRID (grid), apn_label, 1, 1, 1, 1);

  edit_button = gtk_button_new_from_icon_name ("emblem-system-symbolic");
  gtk_accessible_update_property (GTK_ACCESSIBLE (edit_button),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Edit"),
                                -1);
  g_signal_connect_object (edit_button, "clicked",
                           G_CALLBACK (cc_wwan_apn_edit_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  gtk_grid_attach (GTK_GRID (grid), edit_button, 2, 0, 1, 2);

  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), grid);

  return GTK_WIDGET (row);
}

static void
cc_wwan_apn_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcWwanApnDialog *self = (CcWwanApnDialog *)object;

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
cc_wwan_apn_dialog_constructed (GObject *object)
{
  CcWwanApnDialog *self = (CcWwanApnDialog *)object;

  G_OBJECT_CLASS (cc_wwan_apn_dialog_parent_class)->constructed (object);

  self->wwan_data = cc_wwan_device_get_data (self->device);

  gtk_list_box_bind_model (self->apn_list,
                           cc_wwan_data_get_apn_list (self->wwan_data),
                           (GtkListBoxCreateWidgetFunc)cc_wwan_apn_dialog_row_new,
                           self, NULL);
}

static void
cc_wwan_apn_dialog_dispose (GObject *object)
{
  CcWwanApnDialog *self = (CcWwanApnDialog *)object;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_apn_dialog_parent_class)->dispose (object);
}


static void
cc_wwan_apn_dialog_show (GtkWidget *widget)
{
  CcWwanApnDialog *self = (CcWwanApnDialog *)widget;

  gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->add_button), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->save_button), FALSE);
  gtk_stack_set_visible_child (self->apn_settings_stack,
                               GTK_WIDGET (self->apn_list_view));

  GTK_WIDGET_CLASS (cc_wwan_apn_dialog_parent_class)->show (widget);
}

static void
cc_wwan_apn_dialog_class_init (CcWwanApnDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_apn_dialog_set_property;
  object_class->constructed  = cc_wwan_apn_dialog_constructed;
  object_class->dispose = cc_wwan_apn_dialog_dispose;

  widget_class->show = cc_wwan_apn_dialog_show;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-apn-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, add_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_edit_view);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_list);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_list_view);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_radio_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, apn_settings_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, save_button);
  gtk_widget_class_bind_template_child (widget_class, CcWwanApnDialog, username_entry);

  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_apn_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_apn_add_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_apn_save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_apn_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_apn_activated_cb);
}

static void
cc_wwan_apn_dialog_init (CcWwanApnDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWindow *
cc_wwan_apn_dialog_new (GtkWindow    *parent_window,
                        CcWwanDevice *device)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  return GTK_WINDOW (g_object_new (CC_TYPE_WWAN_APN_DIALOG,
                                   "transient-for", parent_window,
                                   "use-header-bar", 1,
                                   "device", device,
                                   NULL));
}
