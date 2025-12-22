/* GStreamer
 *
 *  Copyright 2007-2012 Collabora Ltd
 *   @author: Olivier Crete <olivier.crete@collabora.com>
 *  Copyright 2007-2008 Nokia
 *  Copyright 2023-2024 Igalia S.L.
 *   @author: Thibault Saunier <tsaunier@igalia.com>
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
/**
 * SECTION:element-autoconvert
 * @title: autoconvert
 *
 * The #autoconvert element has one sink and one source pad. It will look for
 * other elements that also have one sink and one source pad.
 * It will then pick an element that matches the caps on both sides.
 * If the caps change, it may change the selected element if the current one
 * no longer matches the caps.
 *
 * The list of element it will look into can be specified in the
 * #GstAutoConvert:factories property, otherwise it will look at all available
 * elements.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstautoconvert.h"
#include <gst/pbutils/pbutils.h>

GST_DEBUG_CATEGORY (autoconvert_debug);
#define GST_CAT_DEFAULT (autoconvert_debug)
struct _GstAutoConvert
{
  GstBaseAutoConvert parent;
};

G_DEFINE_TYPE (GstAutoConvert, gst_auto_convert, GST_TYPE_BASE_AUTO_CONVERT);
GST_ELEMENT_REGISTER_DEFINE (autoconvert, "autoconvert",
    GST_RANK_NONE, gst_auto_convert_get_type ());

enum
{
  PROP_0,
  PROP_FACTORIES,
  PROP_FACTORY_NAMES,
};

static void
gst_auto_convert_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstBaseAutoConvert *baseautoconvert = GST_BASE_AUTO_CONVERT (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    case PROP_FACTORIES:
    {
      GList *factories;

      factories = g_value_get_pointer (value);
      GST_OBJECT_LOCK (object);
      if (!baseautoconvert->factories)
        baseautoconvert->factories =
            g_list_copy_deep (factories, (GCopyFunc) gst_object_ref, NULL);
      else
        GST_WARNING_OBJECT (object, "Can not reset factories after they"
            " have been set or auto-discovered");
      GST_OBJECT_UNLOCK (object);
      break;
    }
    case PROP_FACTORY_NAMES:
    {
      GST_OBJECT_LOCK (object);
      if (!baseautoconvert->factories) {
        gint i;

        for (i = 0; i < gst_value_array_get_size (value); i++) {
          const GValue *v = gst_value_array_get_value (value, i);
          GstElementFactory *factory = (GstElementFactory *)
              gst_registry_find_feature (gst_registry_get (),
              g_value_get_string (v), GST_TYPE_ELEMENT_FACTORY);

          if (!factory) {
            gst_element_post_message (GST_ELEMENT_CAST (baseautoconvert),
                gst_missing_element_message_new (GST_ELEMENT_CAST
                    (baseautoconvert), g_value_get_string (v)));
            continue;
          }

          baseautoconvert->factories =
              g_list_append (baseautoconvert->factories, factory);
        }
      } else {
        GST_WARNING_OBJECT (object, "Can not reset factories after they"
            " have been set or auto-discovered");
      }
      GST_OBJECT_UNLOCK (object);
      break;
    }
  }
}

static void
gst_auto_convert_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstBaseAutoConvert *baseautoconvert = GST_BASE_AUTO_CONVERT (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    case PROP_FACTORIES:
      GST_OBJECT_LOCK (object);
      g_value_set_pointer (value, baseautoconvert->factories);
      GST_OBJECT_UNLOCK (object);
      break;
    case PROP_FACTORY_NAMES:
    {
      GList *tmp;

      GST_OBJECT_LOCK (object);
      for (tmp = baseautoconvert->factories; tmp; tmp = tmp->next) {
        GValue factory = G_VALUE_INIT;

        g_value_init (&factory, G_TYPE_STRING);
        g_value_take_string (&factory, gst_object_get_name (tmp->data));
        gst_value_array_append_and_take_value (value, &factory);
      }
      GST_OBJECT_UNLOCK (object);
      break;
    }
  }
}

static void
gst_auto_convert_class_init (GstAutoConvertClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (autoconvert_debug, "autoconvert", 0,
      "Auto convert element");

  gobject_class->set_property = gst_auto_convert_set_property;
  gobject_class->get_property = gst_auto_convert_get_property;

  gst_element_class_set_static_metadata (element_class,
      "Select converter based on caps", "Generic/Bin",
      "Selects the right transform element based on the caps",
      "Olivier Crete <olivier.crete@collabora.com>");

  g_object_class_install_property (gobject_class, PROP_FACTORIES,
      g_param_spec_pointer ("factories",
          "GList of GstElementFactory",
          "GList of GstElementFactory objects to pick from (the element takes"
          " ownership of the list (NULL means it will go through all possible"
          " elements), can only be set once",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * autoconvert:factory-names:
   *
   * A #GstValueArray of factory names to use
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_FACTORY_NAMES,
      gst_param_spec_array ("factory-names", "Factory names"
          "Names of the Factories to use",
          "Names of the GstElementFactory to be used to automatically plug"
          " elements.",
          g_param_spec_string ("factory-name", "Factory name",
              "An element factory name", NULL,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_BASE_AUTO_CONVERT_CLASS (klass)->registers_filters = FALSE;
}

static void
gst_auto_convert_init (GstAutoConvert * autoconvert)
{
}
