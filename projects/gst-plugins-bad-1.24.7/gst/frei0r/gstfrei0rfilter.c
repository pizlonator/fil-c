/* GStreamer
 * Copyright (C) 2009,2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstfrei0r.h"
#include "gstfrei0rfilter.h"

GST_DEBUG_CATEGORY_EXTERN (frei0r_debug);
#define GST_CAT_DEFAULT frei0r_debug

typedef struct
{
  f0r_plugin_info_t info;
  GstFrei0rFuncTable ftable;
} GstFrei0rFilterClassData;

static gboolean
gst_frei0r_filter_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (trans);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (trans);
  GstVideoInfo info;
  gboolean destroy_f0r_instance = FALSE;

  gst_video_info_init (&info);
  if (!gst_video_info_from_caps (&info, incaps))
    return FALSE;

  if (self->width != info.width || self->height != info.height)
    destroy_f0r_instance = TRUE;

  self->width = info.width;
  self->height = info.height;

  if (self->f0r_instance && destroy_f0r_instance) {
    klass->ftable->destruct (self->f0r_instance);
    self->f0r_instance = NULL;
  }

  return TRUE;
}

static gboolean
gst_frei0r_filter_stop (GstBaseTransform * trans)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (trans);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (trans);

  if (self->f0r_instance) {
    klass->ftable->destruct (self->f0r_instance);
    self->f0r_instance = NULL;
  }

  self->width = self->height = 0;

  return TRUE;
}

static void
gst_frei0r_filter_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstClockTime timestamp;
  GstFrei0rFilter *self = GST_FREI0R_FILTER (trans);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  timestamp =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (self, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (GST_OBJECT (self), timestamp);
}

static GstFlowReturn
gst_frei0r_filter_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (trans);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (trans);
  gdouble time;
  GstMapInfo inmap, outmap;

  if (G_UNLIKELY (self->width <= 0 || self->height <= 0))
    return GST_FLOW_NOT_NEGOTIATED;

  if (G_UNLIKELY (!self->f0r_instance)) {
    self->f0r_instance =
        gst_frei0r_instance_construct (klass->ftable, klass->properties,
        klass->n_properties, self->property_cache, self->width, self->height);
    if (G_UNLIKELY (!self->f0r_instance))
      return GST_FLOW_ERROR;
  }

  time = ((gdouble) GST_BUFFER_TIMESTAMP (inbuf)) / GST_SECOND;

  GST_OBJECT_LOCK (self);

  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE);

  if (klass->ftable->update2)
    klass->ftable->update2 (self->f0r_instance, time,
        (const guint32 *) inmap.data, NULL, NULL, (guint32 *) outmap.data);
  else
    klass->ftable->update (self->f0r_instance, time,
        (const guint32 *) inmap.data, (guint32 *) outmap.data);

  gst_buffer_unmap (outbuf, &outmap);
  gst_buffer_unmap (inbuf, &inmap);

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}

static void
gst_frei0r_filter_finalize (GObject * object)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (object);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (object);

  if (self->f0r_instance) {
    klass->ftable->destruct (self->f0r_instance);
    self->f0r_instance = NULL;
  }

  if (self->property_cache)
    gst_frei0r_property_cache_free (klass->properties, self->property_cache,
        klass->n_properties);
  self->property_cache = NULL;

  G_OBJECT_CLASS (g_type_class_peek_parent (klass))->finalize (object);
}

static void
gst_frei0r_filter_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (object);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (object);

  GST_OBJECT_LOCK (self);
  if (!gst_frei0r_get_property (self->f0r_instance, klass->ftable,
          klass->properties, klass->n_properties, self->property_cache, prop_id,
          value))
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_frei0r_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFrei0rFilter *self = GST_FREI0R_FILTER (object);
  GstFrei0rFilterClass *klass = GST_FREI0R_FILTER_GET_CLASS (object);

  GST_OBJECT_LOCK (self);
  if (!gst_frei0r_set_property (self->f0r_instance, klass->ftable,
          klass->properties, klass->n_properties, self->property_cache, prop_id,
          value))
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_frei0r_filter_class_init (GstFrei0rFilterClass * klass,
    GstFrei0rFilterClassData * class_data)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gsttrans_class = (GstBaseTransformClass *) klass;
  GstPadTemplate *templ;
  const gchar *desc;
  GstCaps *caps;
  gchar *author;

  klass->ftable = &class_data->ftable;
  klass->info = &class_data->info;

  gobject_class->finalize = gst_frei0r_filter_finalize;
  gobject_class->set_property = gst_frei0r_filter_set_property;
  gobject_class->get_property = gst_frei0r_filter_get_property;

  klass->n_properties = klass->info->num_params;
  klass->properties = g_new0 (GstFrei0rProperty, klass->n_properties);

  gst_frei0r_klass_install_properties (gobject_class, klass->ftable,
      klass->properties, klass->n_properties);

  author =
      g_strdup_printf
      ("Sebastian Dröge <sebastian.droege@collabora.co.uk>, %s",
      class_data->info.author);
  desc = class_data->info.explanation;
  if (desc == NULL || *desc == '\0')
    desc = "No details";
  gst_element_class_set_metadata (gstelement_class, class_data->info.name,
      "Filter/Effect/Video", desc, author);
  g_free (author);

  caps = gst_frei0r_caps_from_color_model (class_data->info.color_model);

  templ =
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_caps_ref (caps));
  gst_element_class_add_pad_template (gstelement_class, templ);

  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (gstelement_class, templ);
  gst_caps_unref (caps);

  gsttrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_frei0r_filter_set_caps);
  gsttrans_class->stop = GST_DEBUG_FUNCPTR (gst_frei0r_filter_stop);
  gsttrans_class->transform = GST_DEBUG_FUNCPTR (gst_frei0r_filter_transform);
  gsttrans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_frei0r_filter_before_transform);
}

static void
gst_frei0r_filter_init (GstFrei0rFilter * self, GstFrei0rFilterClass * klass)
{
  self->property_cache =
      gst_frei0r_property_cache_init (klass->properties, klass->n_properties);
  gst_pad_use_fixed_caps (GST_BASE_TRANSFORM_SINK_PAD (self));
  gst_pad_use_fixed_caps (GST_BASE_TRANSFORM_SRC_PAD (self));
}

GstFrei0rPluginRegisterReturn
gst_frei0r_filter_register (GstPlugin * plugin, const gchar * vendor,
    const f0r_plugin_info_t * info, const GstFrei0rFuncTable * ftable)
{
  GTypeInfo typeinfo = {
    sizeof (GstFrei0rFilterClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_frei0r_filter_class_init,
    NULL,
    NULL,
    sizeof (GstFrei0rFilter),
    0,
    (GInstanceInitFunc) gst_frei0r_filter_init
  };
  GType type;
  gchar *type_name, *tmp;
  GstFrei0rFilterClassData *class_data;
  GstFrei0rPluginRegisterReturn ret = GST_FREI0R_PLUGIN_REGISTER_RETURN_FAILED;

  if (vendor)
    tmp = g_strdup_printf ("frei0r-filter-%s-%s", vendor, info->name);
  else
    tmp = g_strdup_printf ("frei0r-filter-%s", info->name);
  type_name = g_ascii_strdown (tmp, -1);
  g_free (tmp);
  g_strcanon (type_name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-+", '-');

  if (g_type_from_name (type_name)) {
    GST_DEBUG ("Type '%s' already exists", type_name);
    return GST_FREI0R_PLUGIN_REGISTER_RETURN_ALREADY_REGISTERED;
  }

  class_data = g_new0 (GstFrei0rFilterClassData, 1);
  memcpy (&class_data->info, info, sizeof (f0r_plugin_info_t));
  memcpy (&class_data->ftable, ftable, sizeof (GstFrei0rFuncTable));
  typeinfo.class_data = class_data;

  type =
      g_type_register_static (GST_TYPE_VIDEO_FILTER, type_name, &typeinfo, 0);
  if (gst_element_register (plugin, type_name, GST_RANK_NONE, type))
    ret = GST_FREI0R_PLUGIN_REGISTER_RETURN_OK;

  g_free (type_name);
  return ret;
}
