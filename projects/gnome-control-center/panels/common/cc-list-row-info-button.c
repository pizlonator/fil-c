/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
 *
 * Copyright 2023 Red Hat Inc
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
 *   Felipe Borges <felipeborges@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-list-row-info-button"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-common-resources.h"
#include "cc-list-row-info-button.h"

struct _CcListRowInfoButton
{
  GtkWidget      parent_instance;

  GtkWidget     *button;
  GtkLabel      *label;
};

G_DEFINE_TYPE (CcListRowInfoButton, cc_list_row_info_button, GTK_TYPE_WIDGET)


enum {
  PROP_0,
  PROP_TEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_list_row_info_button_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcListRowInfoButton *self = (CcListRowInfoButton *)object;

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_label (self->label));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_info_button_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcListRowInfoButton *self = (CcListRowInfoButton *)object;

  switch (prop_id)
    {
    case PROP_TEXT:
      gtk_label_set_label (self->label, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_info_button_dispose (GObject *object)
{
  CcListRowInfoButton *self = CC_LIST_ROW_INFO_BUTTON (object);

  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (cc_list_row_info_button_parent_class)->dispose (object);
}

static void
cc_list_row_info_button_class_init (CcListRowInfoButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_list_row_info_button_dispose;
  object_class->get_property = cc_list_row_info_button_get_property;
  object_class->set_property = cc_list_row_info_button_set_property;

  properties[PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "Set text for the popover label",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-list-row-info-button.ui");

  gtk_widget_class_bind_template_child (widget_class, CcListRowInfoButton, button);
  gtk_widget_class_bind_template_child (widget_class, CcListRowInfoButton, label);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
cc_list_row_info_button_init (CcListRowInfoButton *self)
{
  g_resources_register (cc_common_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_list_row_info_button_set_text (CcListRowInfoButton *self,
                                  const gchar         *text)
{
  g_return_if_fail (CC_IS_LIST_ROW_INFO_BUTTON (self));

  if (!text)
    text = "";

  if (g_str_equal (text, gtk_label_get_label (self->label)))
    return;

  gtk_label_set_text (self->label, text);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}
