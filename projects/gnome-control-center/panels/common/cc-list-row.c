/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
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
#define G_LOG_DOMAIN "cc-list-row"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-common-resources.h"
#include "cc-list-row.h"

struct _CcListRow
{
  AdwActionRow  parent_instance;

  GtkLabel     *secondary_label;

  GtkImage     *arrow;
  gboolean      show_arrow;
};

G_DEFINE_TYPE (CcListRow, cc_list_row, ADW_TYPE_ACTION_ROW)


enum {
  PROP_0,
  PROP_SECONDARY_LABEL,
  PROP_SHOW_ARROW,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_list_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcListRow *self = (CcListRow *)object;

  switch (prop_id)
    {
    case PROP_SECONDARY_LABEL:
      g_value_set_string (value, gtk_label_get_label (self->secondary_label));
      break;

    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, self->show_arrow);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  CcListRow *self = (CcListRow *)object;

  switch (prop_id)
    {
    case PROP_SECONDARY_LABEL:
      gtk_label_set_label (self->secondary_label, g_value_get_string (value));
      break;

    case PROP_SHOW_ARROW:
      cc_list_row_set_show_arrow (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_sensitivity_changed_cb (CcListRow *self)
{
  if (!gtk_widget_get_sensitive (GTK_WIDGET (self)))
    gtk_widget_add_css_class (GTK_WIDGET (self->arrow), "dim-label");

  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->arrow), "dim-label");
}

static void
cc_list_row_class_init (CcListRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_list_row_get_property;
  object_class->set_property = cc_list_row_set_property;

  properties[PROP_SECONDARY_LABEL] =
    g_param_spec_string ("secondary-label",
                         "Secondary Label",
                         "Set Secondary Label",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_ARROW] =
    g_param_spec_boolean ("show-arrow",
                          "Show Arrow",
                          "Whether to show an arrow at the end of the row",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-list-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcListRow, secondary_label);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, arrow);

  gtk_widget_class_bind_template_callback (widget_class, cc_list_row_sensitivity_changed_cb);
}

static void
add_secondary_label_to_a11y_description (CcListRow *self)
{
  // We're using the widget tree traversal because gtk_accessible_update_relation
  // does not allow appending to the existing relation.
  // FIXME: Use the new append to relation API when available.
  GtkWidget *suffix_box;
  GtkWidget *subtitle_label;

  // In current Libadwaita, the subtitle label is in the previous box of the
  // suffixes box (e. g. our secondary_label's parent)
  // as the last child.
  suffix_box = gtk_widget_get_parent (GTK_WIDGET (self->secondary_label));
  subtitle_label = gtk_widget_get_last_child (gtk_widget_get_prev_sibling (suffix_box));
  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                                  subtitle_label,
                                  self->secondary_label,
                                  NULL,
                                  -1);
}

static void
cc_list_row_init (CcListRow *self)
{
  g_resources_register (cc_common_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  add_secondary_label_to_a11y_description (self);
}

void
cc_list_row_set_show_arrow (CcListRow *self,
                            gboolean   show_arrow)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  if (self->show_arrow == show_arrow)
    return;

  self->show_arrow = show_arrow;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ARROW]);
}

void
cc_list_row_set_secondary_label (CcListRow   *self,
                                 const gchar *label)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  if (!label)
    label = "";

  if (g_str_equal (label, gtk_label_get_label (self->secondary_label)))
    return;

  gtk_label_set_text (self->secondary_label, label);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECONDARY_LABEL]);
}

void
cc_list_row_set_secondary_markup (CcListRow   *self,
                                  const gchar *markup)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  if (!markup)
    markup = "";

  if (g_str_equal (markup, gtk_label_get_label (self->secondary_label)))
    return;

  gtk_label_set_markup (self->secondary_label, markup);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECONDARY_LABEL]);
}
