/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2023 Cyber Phantom <inam123451@gmail.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cc-info-entry.h"

struct _CcInfoEntry
{
  GtkBox parent;

  GtkLabel *prop;
  GtkLabel *value;
};

G_DEFINE_TYPE (CcInfoEntry, cc_info_entry, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_LABEL,
  PROP_VALUE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_info_entry_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CcInfoEntry *self = (CcInfoEntry *)object;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_label_get_label (self->prop));
      break;

    case PROP_VALUE:
      g_value_set_string (value, gtk_label_get_label (self->value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_info_entry_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcInfoEntry *self = (CcInfoEntry *)object;

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (self->prop, g_value_get_string (value));
      break;

    case PROP_VALUE:
      gtk_label_set_label (self->value, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_info_entry_class_init (CcInfoEntryClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_info_entry_get_property;
  object_class->set_property = cc_info_entry_set_property;

  properties[PROP_LABEL] =
    g_param_spec_string ("label",
                         "Label",
                         "Set the Label of the Info Entry",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VALUE] =
    g_param_spec_string ("value",
                         "Value",
                         "Set the Value of the Info Entry",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/about/cc-info-entry.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInfoEntry, prop);
  gtk_widget_class_bind_template_child (widget_class, CcInfoEntry, value); 
}

static void
cc_info_entry_init (CcInfoEntry *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_info_entry_set_value (CcInfoEntry *self,
                         const gchar *value)
{
  g_return_if_fail (CC_IS_INFO_ENTRY (self));

  if (!value)
    value = "";

  if (g_str_equal (gtk_label_get_label (self->value), value))
    return;

  gtk_label_set_label (self->value, value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

GtkWidget *
cc_info_entry_new (const gchar *label,
                   const gchar *value)
{
  if (label == NULL)
    label = "";
  
  if (value == NULL)
    value = "";

  return g_object_new (CC_TYPE_INFO_ENTRY,
                       "label", label,
                       "value", value,
                       NULL);
}
