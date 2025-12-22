/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-intersubsrc
 * @title: gstintersubsrc
 *
 * The intersubsrc element is a subtitle source element.  It is used
 * in connection with a intersubsink element in a different pipeline,
 * similar to interaudiosink and interaudiosrc.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v intersubsrc ! srtenc ! filesink location=subtitles.srt
 * ]|
 *
 * The intersubsrc element cannot be used effectively with gst-launch-1.0,
 * as it requires a second pipeline in the application to send subtitles.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstintersubsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_inter_sub_src_debug_category);
#define GST_CAT_DEFAULT gst_inter_sub_src_debug_category

/* prototypes */
static void gst_inter_sub_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_sub_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_sub_src_finalize (GObject * object);

static gboolean gst_inter_sub_src_start (GstBaseSrc * src);
static gboolean gst_inter_sub_src_stop (GstBaseSrc * src);
static void
gst_inter_sub_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static GstFlowReturn
gst_inter_sub_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_CHANNEL
};

#define DEFAULT_CHANNEL ("default")

/* pad templates */
static GstStaticPadTemplate gst_inter_sub_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );

/* class initialization */
#define parent_class gst_inter_sub_src_parent_class
G_DEFINE_TYPE (GstInterSubSrc, gst_inter_sub_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE (intersubsrc, "intersubsrc", GST_RANK_NONE,
    GST_TYPE_INTER_SUB_SRC);

static void
gst_inter_sub_src_class_init (GstInterSubSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_sub_src_debug_category, "intersubsrc", 0,
      "debug category for intersubsrc element");

  gst_element_class_add_static_pad_template (element_class,
      &gst_inter_sub_src_src_template);

  gst_element_class_set_static_metadata (element_class,
      "Internal subtitle source",
      "Source/Subtitle",
      "Virtual subtitle source for internal process communication",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_inter_sub_src_set_property;
  gobject_class->get_property = gst_inter_sub_src_get_property;
  gobject_class->finalize = gst_inter_sub_src_finalize;
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_inter_sub_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_inter_sub_src_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_inter_sub_src_get_times);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_inter_sub_src_create);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          DEFAULT_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_sub_src_init (GstInterSubSrc * intersubsrc)
{
  gst_base_src_set_format (GST_BASE_SRC (intersubsrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (intersubsrc), TRUE);

  intersubsrc->channel = g_strdup (DEFAULT_CHANNEL);
}

void
gst_inter_sub_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (intersubsrc->channel);
      intersubsrc->channel = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_sub_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, intersubsrc->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_inter_sub_src_finalize (GObject * object)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (object);

  g_free (intersubsrc->channel);
  intersubsrc->channel = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_inter_sub_src_start (GstBaseSrc * src)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (src);

  GST_DEBUG_OBJECT (intersubsrc, "start");

  intersubsrc->surface = gst_inter_surface_get (intersubsrc->channel);

  return TRUE;
}

static gboolean
gst_inter_sub_src_stop (GstBaseSrc * src)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (src);

  GST_DEBUG_OBJECT (intersubsrc, "stop");

  gst_inter_surface_unref (intersubsrc->surface);
  intersubsrc->surface = NULL;

  return TRUE;
}

static void
gst_inter_sub_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GST_DEBUG_OBJECT (src, "get_times");

  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (src)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static GstFlowReturn
gst_inter_sub_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstInterSubSrc *intersubsrc = GST_INTER_SUB_SRC (src);
  GstBuffer *buffer;

  GST_DEBUG_OBJECT (intersubsrc, "create");

  buffer = NULL;

  g_mutex_lock (&intersubsrc->surface->mutex);
  if (intersubsrc->surface->sub_buffer) {
    buffer = gst_buffer_ref (intersubsrc->surface->sub_buffer);
    //intersubsrc->surface->sub_buffer_count++;
    //if (intersubsrc->surface->sub_buffer_count >= 30) {
    gst_buffer_unref (intersubsrc->surface->sub_buffer);
    intersubsrc->surface->sub_buffer = NULL;
    //}
  }
  g_mutex_unlock (&intersubsrc->surface->mutex);

  if (buffer == NULL) {
    GstMapInfo map;

    buffer = gst_buffer_new_and_alloc (1);

    gst_buffer_map (buffer, &map, GST_MAP_WRITE);
    map.data[0] = 0;
    gst_buffer_unmap (buffer, &map);
  }

  buffer = gst_buffer_make_writable (buffer);

  /* FIXME: does this make sense? Rate is always 0 */
#if 0
  GST_BUFFER_TIMESTAMP (buffer) =
      gst_util_uint64_scale_int (GST_SECOND, intersubsrc->n_frames,
      intersubsrc->rate);
  GST_DEBUG_OBJECT (intersubsrc, "create ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  GST_BUFFER_DURATION (buffer) =
      gst_util_uint64_scale_int (GST_SECOND, (intersubsrc->n_frames + 1),
      intersubsrc->rate) - GST_BUFFER_TIMESTAMP (buffer);
#endif
  GST_BUFFER_OFFSET (buffer) = intersubsrc->n_frames;
  GST_BUFFER_OFFSET_END (buffer) = -1;
  GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  if (intersubsrc->n_frames == 0) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  }
  intersubsrc->n_frames++;

  *buf = buffer;

  return GST_FLOW_OK;
}
