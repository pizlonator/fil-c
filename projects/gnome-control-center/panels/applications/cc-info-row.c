/* cc-info-row.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
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

#include "cc-info-row.h"
#include "cc-applications-resources.h"

struct _CcInfoRow
{
  AdwActionRow parent;

  GtkWidget    *info;
  GtkWidget    *expander;

  gboolean      expanded;
  gboolean      link;
};

G_DEFINE_TYPE (CcInfoRow, cc_info_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_INFO,
  PROP_HAS_EXPANDER,
  PROP_IS_LINK,
  PROP_EXPANDED
};

static void
cc_info_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcInfoRow *row = CC_INFO_ROW (object);

  switch (prop_id)
    {
    case PROP_INFO:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->info)));
      break;
    case PROP_HAS_EXPANDER:
      g_value_set_boolean (value, gtk_widget_get_visible (row->expander));
      break;
    case PROP_IS_LINK:
      g_value_set_boolean (value, row->link);
      break;
    case PROP_EXPANDED:
      g_value_set_boolean (value, cc_info_row_get_expanded (row));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_expander (CcInfoRow *row)
{
  if (row->link)
    gtk_image_set_from_icon_name (GTK_IMAGE (row->expander), "go-next-symbolic");
  else if (row->expanded)
    gtk_image_set_from_icon_name (GTK_IMAGE (row->expander), "pan-down-symbolic");
  else
    gtk_image_set_from_icon_name (GTK_IMAGE (row->expander), "pan-end-symbolic");
}

static void
cc_info_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcInfoRow *row = CC_INFO_ROW (object);

  switch (prop_id)
    {
    case PROP_INFO:
      gtk_label_set_label (GTK_LABEL (row->info), g_value_get_string (value));
      break;

    case PROP_HAS_EXPANDER:
      gtk_widget_set_visible (row->expander, g_value_get_boolean (value));
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), g_value_get_boolean (value));
      break;

    case PROP_IS_LINK:
      row->link = g_value_get_boolean (value);
      update_expander (row);
      break;

    case PROP_EXPANDED:
      cc_info_row_set_expanded (row, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_info_row_class_init (CcInfoRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_info_row_get_property;
  object_class->set_property = cc_info_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-info-row.ui");

  g_object_class_install_property (object_class,
                                   PROP_INFO,
                                   g_param_spec_string ("info", "info", "info",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HAS_EXPANDER,
                                   g_param_spec_boolean ("has-expander", "has-expander", "has-expander",
                                                         FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_EXPANDED,
                                   g_param_spec_boolean ("expanded", "expanded", "expanded",
                                                         FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_IS_LINK,
                                   g_param_spec_boolean ("is-link", "is-link", "is-link",
                                                         FALSE, G_PARAM_READWRITE));

  gtk_widget_class_bind_template_child (widget_class, CcInfoRow, info);
  gtk_widget_class_bind_template_child (widget_class, CcInfoRow, expander);
}

static void
cc_info_row_init (CcInfoRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcInfoRow *
cc_info_row_new (void)
{
  return CC_INFO_ROW (g_object_new (CC_TYPE_INFO_ROW, NULL));
}

gboolean
cc_info_row_get_expanded (CcInfoRow *row)
{
  return row->expanded;
}

void
cc_info_row_set_expanded (CcInfoRow *row,
                          gboolean   expanded)
{
  if (row->expanded == expanded)
    return;

  row->expanded = expanded;
  update_expander (row);

  g_object_notify (G_OBJECT (row), "expanded");
}

