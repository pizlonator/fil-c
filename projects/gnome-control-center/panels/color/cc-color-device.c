/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-color-common.h"
#include "cc-color-device.h"

struct _CcColorDevice
{
  GtkListBoxRow parent_instance;

  CdDevice    *device;
  gboolean     expanded;
  gchar       *sortable;
  GtkWidget   *widget_description;
  GtkWidget   *widget_button;
  GtkWidget   *widget_switch;
  GtkWidget   *widget_arrow;
  GtkWidget   *widget_nocalib;
};

G_DEFINE_TYPE (CcColorDevice, cc_color_device, GTK_TYPE_LIST_BOX_ROW)

enum
{
  SIGNAL_EXPANDED_CHANGED,
  SIGNAL_LAST
};

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

static void
cc_color_device_refresh (CcColorDevice *color_device)
{
  g_autofree gchar *title = NULL;
  g_autoptr(GPtrArray) profiles = NULL;
  g_autofree gchar *name1 = NULL;
  g_autofree gchar *name2 = NULL;

  /* add switch and expander if there are profiles, otherwise use a label */
  profiles = cd_device_get_profiles (color_device->device);
  if (profiles == NULL)
    return;

  title = cc_color_device_get_title (color_device->device);
  gtk_label_set_label (GTK_LABEL (color_device->widget_description), title);
  gtk_widget_set_visible (color_device->widget_description, TRUE);

  gtk_widget_set_visible (color_device->widget_switch, profiles->len > 0);
  gtk_widget_set_visible (color_device->widget_button, profiles->len > 0);
  gtk_image_set_from_icon_name (GTK_IMAGE (color_device->widget_arrow),
                                color_device->expanded ? "pan-down-symbolic" : "pan-end-symbolic");
  gtk_widget_set_visible (color_device->widget_nocalib, profiles->len == 0);
  gtk_widget_set_sensitive (color_device->widget_button, cd_device_get_enabled (color_device->device));
  gtk_switch_set_active (GTK_SWITCH (color_device->widget_switch),
                         cd_device_get_enabled (color_device->device));

  name1 = g_strdup_printf (_("Enable color management for %s"), title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (color_device->widget_switch),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, name1,
                                  -1);

  name2 = g_strdup_printf (_("Show color profiles for %s"), title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (color_device->widget_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, name2,
                                  -1);
}

CdDevice *
cc_color_device_get_device (CcColorDevice *color_device)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (color_device), NULL);
  return color_device->device;
}

const gchar *
cc_color_device_get_sortable (CcColorDevice *color_device)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (color_device), NULL);
  return color_device->sortable;
}

static void
cc_color_device_get_property (GObject *object, guint param_id,
                              GValue *value, GParamSpec *pspec)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);
  switch (param_id)
    {
      case PROP_DEVICE:
        g_value_set_object (value, color_device->device);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_device_set_property (GObject *object, guint param_id,
                              const GValue *value, GParamSpec *pspec)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);

  switch (param_id)
    {
      case PROP_DEVICE:
        color_device->device = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_device_finalize (GObject *object)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);

  g_free (color_device->sortable);
  g_object_unref (color_device->device);

  G_OBJECT_CLASS (cc_color_device_parent_class)->finalize (object);
}

void
cc_color_device_set_expanded (CcColorDevice *color_device,
                              gboolean expanded)
{
  /* same as before */
  if (color_device->expanded == expanded)
    return;

  /* refresh */
  color_device->expanded = expanded;
  g_signal_emit (color_device,
                 signals[SIGNAL_EXPANDED_CHANGED], 0,
                 color_device->expanded);
  cc_color_device_refresh (color_device);
}

static void
cc_color_device_notify_enable_device_cb (CcColorDevice *color_device)
{
  gboolean enable;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  enable = gtk_switch_get_active (GTK_SWITCH (color_device->widget_switch));
  g_debug ("Set %s to %i", cd_device_get_id (color_device->device), enable);
  ret = cd_device_set_enabled_sync (color_device->device,
                                    enable, NULL, &error);
  if (!ret)
    g_warning ("failed to %s to the device: %s",
               enable ? "enable" : "disable", error->message);

  /* if expanded, close */
  cc_color_device_set_expanded (color_device, FALSE);
}

static void
cc_color_device_changed_cb (CcColorDevice *color_device)
{
  cc_color_device_refresh (color_device);
}

static void
cc_color_device_constructed (GObject *object)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);
  g_autofree gchar *sortable_tmp = NULL;

  /* watch the device for changes */
  g_signal_connect_object (color_device->device, "changed",
                           G_CALLBACK (cc_color_device_changed_cb), color_device, G_CONNECT_SWAPPED);

  /* calculate sortable -- FIXME: we have to hack this as EggListBox
   * does not let us specify a GtkSortType:
   * https://bugzilla.gnome.org/show_bug.cgi?id=691341 */
  sortable_tmp = cc_color_device_get_sortable_base (color_device->device);
  color_device->sortable = g_strdup_printf ("%sXX", sortable_tmp);

  cc_color_device_refresh (color_device);

  /* watch to see if the user flicked the switch */
  g_signal_connect_object (color_device->widget_switch, "notify::active",
                           G_CALLBACK (cc_color_device_notify_enable_device_cb),
                           color_device, G_CONNECT_SWAPPED);
}

static void
cc_color_device_class_init (CcColorDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = cc_color_device_get_property;
  object_class->set_property = cc_color_device_set_property;
  object_class->constructed = cc_color_device_constructed;
  object_class->finalize = cc_color_device_finalize;

  g_object_class_install_property (object_class, PROP_DEVICE,
                                   g_param_spec_object ("device", NULL,
                                                        NULL,
                                                        CD_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  signals [SIGNAL_EXPANDED_CHANGED] =
    g_signal_new ("expanded-changed",
            G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
cc_color_device_clicked_expander_cb (CcColorDevice *color_device)
{
  color_device->expanded = !color_device->expanded;
  cc_color_device_refresh (color_device);
  g_signal_emit (color_device, signals[SIGNAL_EXPANDED_CHANGED], 0,
                 color_device->expanded);
}

static void
cc_color_device_init (CcColorDevice *color_device)
{
  GtkWidget *box;

  /* description */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 9);
  color_device->widget_description = gtk_label_new ("");
  gtk_widget_set_margin_start (color_device->widget_description, 20);
  gtk_widget_set_margin_top (color_device->widget_description, 12);
  gtk_widget_set_margin_bottom (color_device->widget_description, 12);
  gtk_widget_set_halign (color_device->widget_description, GTK_ALIGN_START);
  gtk_widget_set_hexpand (color_device->widget_description, TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (color_device->widget_description), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (color_device->widget_description), 0);
  gtk_box_append (GTK_BOX (box), color_device->widget_description);

  /* switch */
  color_device->widget_switch = gtk_switch_new ();
  gtk_widget_set_valign (color_device->widget_switch, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), color_device->widget_switch);

  /* arrow button */
  color_device->widget_arrow = gtk_image_new_from_icon_name ("pan-end-symbolic");
  color_device->widget_button = gtk_button_new ();
  g_signal_connect_object (color_device->widget_button, "clicked",
                           G_CALLBACK (cc_color_device_clicked_expander_cb),
                           color_device, G_CONNECT_SWAPPED);
  gtk_widget_set_valign (color_device->widget_button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (color_device->widget_button, "flat");
  gtk_button_set_child (GTK_BUTTON (color_device->widget_button), color_device->widget_arrow);
  gtk_widget_set_margin_top (color_device->widget_button, 9);
  gtk_widget_set_margin_bottom (color_device->widget_button, 9);
  gtk_widget_set_margin_end (color_device->widget_button, 12);
  gtk_box_append (GTK_BOX (box), color_device->widget_button);

  /* not calibrated */
  color_device->widget_nocalib = gtk_label_new (_("Not calibrated"));
  gtk_widget_add_css_class (color_device->widget_nocalib, "dim-label");
  gtk_widget_set_margin_end (color_device->widget_nocalib, 18);
  gtk_box_append (GTK_BOX (box), color_device->widget_nocalib);

  /* refresh */
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (color_device), box);
}

GtkWidget *
cc_color_device_new (CdDevice *device)
{
  return g_object_new (CC_TYPE_COLOR_DEVICE,
                       "device", device,
                       NULL);
}

