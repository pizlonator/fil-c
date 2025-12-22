/*
 * GStreamer
 * Copyright (C) 2017 Collabora Inc.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gstfakesinkutils.h"
#include <gst/base/gstbasesink.h>

/* TODO complete the types */
void
gst_util_proxy_class_properties (GObjectClass * object_class,
    GObjectClass * target_class, guint property_id_offset)
{
  GParamSpec **properties;
  guint n_properties, i;

  properties = g_object_class_list_properties (G_OBJECT_CLASS (target_class),
      &n_properties);

  for (i = 0; i < n_properties; i++) {
    guint property_id = i + property_id_offset;

    if (properties[i]->owner_type == G_TYPE_OBJECT
        || properties[i]->owner_type == GST_TYPE_OBJECT)
      continue;

    if (G_IS_PARAM_SPEC_BOOLEAN (properties[i])) {
      GParamSpecBoolean *prop = G_PARAM_SPEC_BOOLEAN (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_boolean (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->default_value, properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_INT (properties[i])) {
      GParamSpecInt *prop = G_PARAM_SPEC_INT (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_int (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->minimum, prop->maximum, prop->default_value,
              properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_UINT (properties[i])) {
      GParamSpecUInt *prop = G_PARAM_SPEC_UINT (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_uint (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->minimum, prop->maximum, prop->default_value,
              properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_INT64 (properties[i])) {
      GParamSpecInt64 *prop = G_PARAM_SPEC_INT64 (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_int64 (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->minimum, prop->maximum, prop->default_value,
              properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_UINT64 (properties[i])) {
      GParamSpecUInt64 *prop = G_PARAM_SPEC_UINT64 (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_uint64 (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->minimum, prop->maximum, prop->default_value,
              properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_ENUM (properties[i])) {
      GParamSpecEnum *prop = G_PARAM_SPEC_ENUM (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_enum (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              properties[i]->value_type, prop->default_value,
              properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_STRING (properties[i])) {
      GParamSpecString *prop = G_PARAM_SPEC_STRING (properties[i]);
      g_object_class_install_property (object_class, property_id,
          g_param_spec_string (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              prop->default_value, properties[i]->flags));
    } else if (G_IS_PARAM_SPEC_BOXED (properties[i])) {
      g_object_class_install_property (object_class, property_id,
          g_param_spec_boxed (g_param_spec_get_name (properties[i]),
              g_param_spec_get_nick (properties[i]),
              g_param_spec_get_blurb (properties[i]),
              properties[i]->value_type, properties[i]->flags));
    }
  }

  g_free (properties);
}
