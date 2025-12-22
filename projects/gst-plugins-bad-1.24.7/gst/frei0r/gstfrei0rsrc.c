/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
#include "gstfrei0rsrc.h"

GST_DEBUG_CATEGORY_EXTERN (frei0r_debug);
#define GST_CAT_DEFAULT frei0r_debug

typedef struct
{
  f0r_plugin_info_t info;
  GstFrei0rFuncTable ftable;
} GstFrei0rSrcClassData;

static gboolean
gst_frei0r_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (src);

  gst_video_info_init (&self->info);
  if (!gst_video_info_from_caps (&self->info, caps))
    return FALSE;

  gst_base_src_set_blocksize (src, self->info.size);

  return TRUE;
}

static GstFlowReturn
gst_frei0r_src_fill (GstPushSrc * src, GstBuffer * buf)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (src);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (src);
  GstClockTime timestamp;
  gdouble time;
  GstMapInfo map;

  if (G_UNLIKELY (!self->f0r_instance)) {
    self->f0r_instance =
        gst_frei0r_instance_construct (klass->ftable, klass->properties,
        klass->n_properties, self->property_cache, self->info.width,
        self->info.height);

    if (G_UNLIKELY (!self->f0r_instance))
      return GST_FLOW_ERROR;
  }

  timestamp =
      gst_util_uint64_scale (self->n_frames, GST_SECOND * self->info.fps_d,
      self->info.fps_n);
  GST_BUFFER_PTS (buf) = GST_BUFFER_DTS (buf) = timestamp;
  GST_BUFFER_OFFSET (buf) = self->n_frames;
  self->n_frames++;
  GST_BUFFER_OFFSET_END (buf) = self->n_frames;
  GST_BUFFER_DURATION (buf) =
      gst_util_uint64_scale (self->n_frames, GST_SECOND * self->info.fps_d,
      self->info.fps_n) - GST_BUFFER_TIMESTAMP (buf);

  timestamp =
      gst_segment_to_stream_time (&GST_BASE_SRC_CAST (self)->segment,
      GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (self, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (GST_OBJECT (self), timestamp);

  time = ((gdouble) GST_BUFFER_TIMESTAMP (buf)) / GST_SECOND;

  GST_OBJECT_LOCK (self);

  if (!gst_buffer_map (buf, &map, GST_MAP_WRITE))
    goto map_error;

  if (klass->ftable->update2)
    klass->ftable->update2 (self->f0r_instance, time, NULL, NULL, NULL,
        (guint32 *) map.data);
  else
    klass->ftable->update (self->f0r_instance, time, NULL,
        (guint32 *) map.data);

  gst_buffer_unmap (buf, &map);

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;

map_error:
  GST_OBJECT_UNLOCK (self);
  GST_ELEMENT_ERROR (GST_ELEMENT (src), RESOURCE, WRITE, (NULL),
      ("Could not map buffer for writing"));
  return GST_FLOW_ERROR;
}

static gboolean
gst_frei0r_src_start (GstBaseSrc * basesrc)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (basesrc);

  self->n_frames = 0;

  return TRUE;
}

static gboolean
gst_frei0r_src_stop (GstBaseSrc * basesrc)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (basesrc);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (basesrc);

  if (self->f0r_instance) {
    klass->ftable->destruct (self->f0r_instance);
    self->f0r_instance = NULL;
  }

  gst_video_info_init (&self->info);
  self->n_frames = 0;

  return TRUE;
}

static gboolean
gst_frei0r_src_is_seekable (GstBaseSrc * psrc)
{
  return TRUE;
}

static gboolean
gst_frei0r_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstClockTime time;
  GstFrei0rSrc *self = GST_FREI0R_SRC (bsrc);

  segment->time = segment->start;
  time = segment->position;

  /* now move to the time indicated */
  if (self->info.fps_n) {
    self->n_frames = gst_util_uint64_scale (time,
        self->info.fps_n, self->info.fps_d * GST_SECOND);
  } else {
    self->n_frames = 0;
  }

  return TRUE;
}

static gboolean
gst_frei0r_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  gboolean res;
  GstFrei0rSrc *self = GST_FREI0R_SRC (bsrc);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (self);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (src_fmt == dest_fmt) {
        dest_val = src_val;
        goto done;
      }

      switch (src_fmt) {
        case GST_FORMAT_DEFAULT:
          switch (dest_fmt) {
            case GST_FORMAT_TIME:
              /* frames to time */
              if (self->info.fps_n) {
                dest_val = gst_util_uint64_scale (src_val,
                    self->info.fps_d * GST_SECOND, self->info.fps_n);
              } else {
                dest_val = 0;
              }
              break;
            default:
              goto error;
          }
          break;
        case GST_FORMAT_TIME:
          switch (dest_fmt) {
            case GST_FORMAT_DEFAULT:
              /* time to frames */
              if (self->info.fps_n) {
                dest_val = gst_util_uint64_scale (src_val,
                    self->info.fps_n, self->info.fps_d * GST_SECOND);
              } else {
                dest_val = 0;
              }
              break;
            default:
              goto error;
          }
          break;
        default:
          goto error;
      }
    done:
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      res = TRUE;
      break;
    }
    default:
      res =
          GST_BASE_SRC_CLASS (g_type_class_peek_parent (klass))->query (bsrc,
          query);
  }
  return res;

  /* ERROR */
error:
  {
    GST_DEBUG_OBJECT (self, "query failed");
    return FALSE;
  }
}

static GstCaps *
gst_frei0r_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);

  return caps;
}

static void
gst_frei0r_src_finalize (GObject * object)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (object);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (object);

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
gst_frei0r_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (object);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (object);

  GST_OBJECT_LOCK (self);
  if (!gst_frei0r_get_property (self->f0r_instance, klass->ftable,
          klass->properties, klass->n_properties, self->property_cache, prop_id,
          value))
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_frei0r_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFrei0rSrc *self = GST_FREI0R_SRC (object);
  GstFrei0rSrcClass *klass = GST_FREI0R_SRC_GET_CLASS (object);

  GST_OBJECT_LOCK (self);
  if (!gst_frei0r_set_property (self->f0r_instance, klass->ftable,
          klass->properties, klass->n_properties, self->property_cache, prop_id,
          value))
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_frei0r_src_class_init (GstFrei0rSrcClass * klass,
    GstFrei0rSrcClassData * class_data)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPadTemplate *templ;
  const gchar *desc;
  GstCaps *caps;
  gchar *author;

  klass->ftable = &class_data->ftable;
  klass->info = &class_data->info;

  gobject_class->finalize = gst_frei0r_src_finalize;
  gobject_class->set_property = gst_frei0r_src_set_property;
  gobject_class->get_property = gst_frei0r_src_get_property;

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
      "Src/Video", desc, author);
  g_free (author);

  caps = gst_frei0r_caps_from_color_model (class_data->info.color_model);

  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (gstelement_class, templ);
  gst_caps_unref (caps);

  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_frei0r_src_set_caps);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_frei0r_src_is_seekable);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_frei0r_src_do_seek);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_frei0r_src_query);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_frei0r_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_frei0r_src_stop);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_frei0r_src_fixate);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_frei0r_src_fill);
}

static void
gst_frei0r_src_init (GstFrei0rSrc * self, GstFrei0rSrcClass * klass)
{
  self->property_cache =
      gst_frei0r_property_cache_init (klass->properties, klass->n_properties);
  gst_video_info_init (&self->info);
  gst_base_src_set_format (GST_BASE_SRC_CAST (self), GST_FORMAT_TIME);
}

GstFrei0rPluginRegisterReturn
gst_frei0r_src_register (GstPlugin * plugin, const gchar * vendor,
    const f0r_plugin_info_t * info, const GstFrei0rFuncTable * ftable)
{
  GTypeInfo typeinfo = {
    sizeof (GstFrei0rSrcClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_frei0r_src_class_init,
    NULL,
    NULL,
    sizeof (GstFrei0rSrc),
    0,
    (GInstanceInitFunc) gst_frei0r_src_init
  };
  GType type;
  gchar *type_name, *tmp;
  GstFrei0rSrcClassData *class_data;
  GstFrei0rPluginRegisterReturn ret = GST_FREI0R_PLUGIN_REGISTER_RETURN_FAILED;

  if (vendor)
    tmp = g_strdup_printf ("frei0r-src-%s-%s", vendor, info->name);
  else
    tmp = g_strdup_printf ("frei0r-src-%s", info->name);
  type_name = g_ascii_strdown (tmp, -1);
  g_free (tmp);
  g_strcanon (type_name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-+", '-');

  if (g_type_from_name (type_name)) {
    GST_DEBUG ("Type '%s' already exists", type_name);
    return GST_FREI0R_PLUGIN_REGISTER_RETURN_ALREADY_REGISTERED;
  }

  class_data = g_new0 (GstFrei0rSrcClassData, 1);
  memcpy (&class_data->info, info, sizeof (f0r_plugin_info_t));
  memcpy (&class_data->ftable, ftable, sizeof (GstFrei0rFuncTable));
  typeinfo.class_data = class_data;

  type = g_type_register_static (GST_TYPE_PUSH_SRC, type_name, &typeinfo, 0);
  if (gst_element_register (plugin, type_name, GST_RANK_NONE, type))
    ret = GST_FREI0R_PLUGIN_REGISTER_RETURN_OK;

  g_free (type_name);
  return ret;
}
