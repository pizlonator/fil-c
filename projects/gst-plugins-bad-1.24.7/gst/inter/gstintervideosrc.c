/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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
 * SECTION:element-intervideosrc
 * @title: gstintervideosrc
 *
 * The intervideosrc element is a video source element.  It is used
 * in connection with a intervideosink element in a different pipeline,
 * similar to interaudiosink and interaudiosrc.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v intervideosrc ! queue ! xvimagesink
 * ]|
 *
 * The intersubsrc element cannot be used effectively with gst-launch-1.0,
 * as it requires a second pipeline in the application to send subtitles.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/video.h>
#include "gstintervideosrc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_inter_video_src_debug_category);
#define GST_CAT_DEFAULT gst_inter_video_src_debug_category

/* prototypes */
static void gst_inter_video_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_video_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_video_src_finalize (GObject * object);

static GstCaps *gst_inter_video_src_get_caps (GstBaseSrc * src,
    GstCaps * filter);
static gboolean gst_inter_video_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static GstCaps *gst_inter_video_src_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_inter_video_src_start (GstBaseSrc * src);
static gboolean gst_inter_video_src_stop (GstBaseSrc * src);
static void
gst_inter_video_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static GstFlowReturn
gst_inter_video_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_TIMEOUT
};

#define DEFAULT_CHANNEL ("default")
#define DEFAULT_TIMEOUT (GST_SECOND)

/* pad templates */
static GstStaticPadTemplate gst_inter_video_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );


/* class initialization */
#define parent_class gst_inter_video_src_parent_class
G_DEFINE_TYPE (GstInterVideoSrc, gst_inter_video_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE (intervideosrc, "intervideosrc",
    GST_RANK_NONE, GST_TYPE_INTER_VIDEO_SRC);

static void
gst_inter_video_src_class_init (GstInterVideoSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_video_src_debug_category, "intervideosrc",
      0, "debug category for intervideosrc element");

  gst_element_class_add_static_pad_template (element_class,
      &gst_inter_video_src_src_template);

  gst_element_class_set_static_metadata (element_class,
      "Internal video source",
      "Source/Video",
      "Virtual video source for internal process communication",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_inter_video_src_set_property;
  gobject_class->get_property = gst_inter_video_src_get_property;
  gobject_class->finalize = gst_inter_video_src_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_inter_video_src_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_inter_video_src_set_caps);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_inter_video_src_fixate);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_inter_video_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_inter_video_src_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_inter_video_src_get_times);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_inter_video_src_create);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          DEFAULT_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Timeout after which to start outputting black frames",
          0, G_MAXUINT64, DEFAULT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_video_src_init (GstInterVideoSrc * intervideosrc)
{
  gst_base_src_set_format (GST_BASE_SRC (intervideosrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (intervideosrc), TRUE);

  intervideosrc->channel = g_strdup (DEFAULT_CHANNEL);
  intervideosrc->timeout = DEFAULT_TIMEOUT;
}

void
gst_inter_video_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (intervideosrc->channel);
      intervideosrc->channel = g_value_dup_string (value);
      break;
    case PROP_TIMEOUT:
      intervideosrc->timeout = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_video_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, intervideosrc->channel);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, intervideosrc->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_video_src_finalize (GObject * object)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (object);

  /* clean up object here */
  g_free (intervideosrc->channel);

  G_OBJECT_CLASS (gst_inter_video_src_parent_class)->finalize (object);
}

static GstCaps *
gst_inter_video_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (src);
  GstCaps *caps;

  GST_DEBUG_OBJECT (intervideosrc, "get_caps");

  if (!intervideosrc->surface)
    return GST_BASE_SRC_CLASS (parent_class)->get_caps (src, filter);

  g_mutex_lock (&intervideosrc->surface->mutex);
  if (intervideosrc->surface->video_info.finfo) {
    caps = gst_video_info_to_caps (&intervideosrc->surface->video_info);
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION_RANGE, 1,
        G_MAXINT, G_MAXINT, 1, NULL);

    if (filter) {
      GstCaps *tmp;

      tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = tmp;
    }
  } else {
    caps = NULL;
  }
  g_mutex_unlock (&intervideosrc->surface->mutex);

  if (caps)
    return caps;
  else
    return GST_BASE_SRC_CLASS (parent_class)->get_caps (src, filter);
}

static gboolean
gst_inter_video_src_set_caps (GstBaseSrc * base, GstCaps * caps)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (base);
  GstVideoConverter *converter;
  GstVideoFrame src_frame, dest_frame;
  GstBuffer *src, *dest;
  GstVideoInfo black_info;

  GST_DEBUG_OBJECT (intervideosrc, "set_caps");

  if (!gst_video_info_from_caps (&intervideosrc->info, caps)) {
    GST_ERROR_OBJECT (intervideosrc, "Failed to parse caps %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }

  /* Create a black frame */
  gst_buffer_replace (&intervideosrc->black_frame, NULL);
  gst_video_info_set_format (&black_info, GST_VIDEO_FORMAT_ARGB,
      intervideosrc->info.width, intervideosrc->info.height);
  black_info.fps_n = intervideosrc->info.fps_n;
  black_info.fps_d = intervideosrc->info.fps_d;
  src = gst_buffer_new_and_alloc (black_info.size);
  dest = gst_buffer_new_and_alloc (intervideosrc->info.size);
  gst_buffer_memset (src, 0, 0, black_info.size);
  gst_video_frame_map (&src_frame, &black_info, src, GST_MAP_READ);
  gst_video_frame_map (&dest_frame, &intervideosrc->info, dest, GST_MAP_WRITE);
  converter = gst_video_converter_new (&black_info, &intervideosrc->info, NULL);
  gst_video_converter_frame (converter, &src_frame, &dest_frame);
  gst_video_converter_free (converter);
  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dest_frame);
  gst_buffer_unref (src);
  intervideosrc->black_frame = dest;

  return TRUE;
}

static gboolean
gst_inter_video_src_start (GstBaseSrc * src)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (intervideosrc, "start");

  intervideosrc->surface = gst_inter_surface_get (intervideosrc->channel);
  intervideosrc->timestamp_offset = 0;
  intervideosrc->n_frames = 0;

  return TRUE;
}

static gboolean
gst_inter_video_src_stop (GstBaseSrc * src)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (intervideosrc, "stop");

  gst_inter_surface_unref (intervideosrc->surface);
  intervideosrc->surface = NULL;
  gst_buffer_replace (&intervideosrc->black_frame, NULL);

  return TRUE;
}

static void
gst_inter_video_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GST_DEBUG_OBJECT (src, "get_times");

  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (src)) {
    GstClockTime timestamp = GST_BUFFER_PTS (buffer);

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
gst_inter_video_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstInterVideoSrc *intervideosrc = GST_INTER_VIDEO_SRC (src);
  GstCaps *caps;
  GstBuffer *buffer;
  guint64 frames;
  gboolean is_gap = FALSE;

  GST_DEBUG_OBJECT (intervideosrc, "create");

  caps = NULL;
  buffer = NULL;

  frames = gst_util_uint64_scale_ceil (intervideosrc->timeout,
      GST_VIDEO_INFO_FPS_N (&intervideosrc->info),
      GST_VIDEO_INFO_FPS_D (&intervideosrc->info) * GST_SECOND);

  g_mutex_lock (&intervideosrc->surface->mutex);
  if (intervideosrc->surface->video_info.finfo) {
    GstVideoInfo tmp_info = intervideosrc->surface->video_info;

    /* We negotiate the framerate ourselves */
    tmp_info.fps_n = intervideosrc->info.fps_n;
    tmp_info.fps_d = intervideosrc->info.fps_d;
    if (intervideosrc->info.flags & GST_VIDEO_FLAG_VARIABLE_FPS)
      tmp_info.flags |= GST_VIDEO_FLAG_VARIABLE_FPS;
    else
      tmp_info.flags &= ~GST_VIDEO_FLAG_VARIABLE_FPS;

    if (!gst_video_info_is_equal (&tmp_info, &intervideosrc->info)) {
      caps = gst_video_info_to_caps (&tmp_info);
      intervideosrc->timestamp_offset +=
          gst_util_uint64_scale (GST_SECOND * intervideosrc->n_frames,
          GST_VIDEO_INFO_FPS_D (&intervideosrc->info),
          GST_VIDEO_INFO_FPS_N (&intervideosrc->info));
      intervideosrc->n_frames = 0;
    }
  }

  if (intervideosrc->surface->video_buffer) {
    /* We have a buffer to push */
    buffer = gst_buffer_ref (intervideosrc->surface->video_buffer);

    /* Can only be true if timeout > 0 */
    if (intervideosrc->surface->video_buffer_count == frames) {
      gst_buffer_unref (intervideosrc->surface->video_buffer);
      intervideosrc->surface->video_buffer = NULL;
    }
  }

  if (intervideosrc->surface->video_buffer_count != 0 &&
      intervideosrc->surface->video_buffer_count != (frames + 1)) {
    /* This is a repeat of the stored buffer or of a black frame */
    is_gap = TRUE;
  }

  intervideosrc->surface->video_buffer_count++;
  g_mutex_unlock (&intervideosrc->surface->mutex);

  if (caps) {
    gboolean ret;
    GstStructure *s;
    GstCaps *downstream_caps;
    GstCaps *tmp, *negotiated_caps;
    gint fps_n = 0, fps_d = 1;

    /* Negotiate a framerate with downstream */
    downstream_caps = gst_pad_get_allowed_caps (GST_BASE_SRC_PAD (src));

    /* Remove all framerates */
    tmp = gst_caps_copy (caps);
    s = gst_caps_get_structure (tmp, 0);
    gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d);
    if (fps_n == 0)
      gst_structure_get_fraction (s, "max-framerate", &fps_n, &fps_d);
    gst_structure_remove_field (s, "framerate");
    gst_structure_remove_field (s, "max-framerate");

    negotiated_caps =
        gst_caps_intersect_full (downstream_caps, tmp,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    gst_caps_unref (downstream_caps);

    if (gst_caps_is_empty (negotiated_caps)) {
      GST_ERROR_OBJECT (src, "Failed to negotiate caps %" GST_PTR_FORMAT, caps);
      if (buffer)
        gst_buffer_unref (buffer);
      gst_caps_unref (caps);
      return GST_FLOW_NOT_NEGOTIATED;
    }
    gst_caps_unref (caps);
    caps = NULL;

    /* Prefer what the source produces, otherwise 30 fps */
    if (fps_n == 0) {
      fps_n = 30;
      fps_d = 1;
    }

    negotiated_caps = gst_caps_truncate (negotiated_caps);
    s = gst_caps_get_structure (negotiated_caps, 0);
    if (!gst_structure_has_field (s, "framerate"))
      gst_structure_set (s, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
    else
      gst_structure_fixate_field_nearest_fraction (s, "framerate", fps_n,
          fps_d);

    ret = gst_base_src_set_caps (src, negotiated_caps);
    if (!ret) {
      GST_ERROR_OBJECT (src, "Failed to set caps %" GST_PTR_FORMAT,
          negotiated_caps);
      if (buffer)
        gst_buffer_unref (buffer);
      gst_caps_unref (negotiated_caps);
      return GST_FLOW_NOT_NEGOTIATED;
    }
    gst_caps_unref (negotiated_caps);
  }

  if (buffer == NULL) {
    GST_DEBUG_OBJECT (intervideosrc, "Creating black frame");
    buffer = gst_buffer_copy (intervideosrc->black_frame);
  }

  buffer = gst_buffer_make_writable (buffer);

  if (is_gap)
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_GAP);

  GST_BUFFER_PTS (buffer) = intervideosrc->timestamp_offset +
      gst_util_uint64_scale (GST_SECOND * intervideosrc->n_frames,
      GST_VIDEO_INFO_FPS_D (&intervideosrc->info),
      GST_VIDEO_INFO_FPS_N (&intervideosrc->info));
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_DEBUG_OBJECT (intervideosrc, "create ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)));
  GST_BUFFER_DURATION (buffer) = intervideosrc->timestamp_offset +
      gst_util_uint64_scale (GST_SECOND * (intervideosrc->n_frames + 1),
      GST_VIDEO_INFO_FPS_D (&intervideosrc->info),
      GST_VIDEO_INFO_FPS_N (&intervideosrc->info)) - GST_BUFFER_PTS (buffer);
  GST_BUFFER_OFFSET (buffer) = intervideosrc->n_frames;
  GST_BUFFER_OFFSET_END (buffer) = -1;
  GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  if (intervideosrc->n_frames == 0) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  }
  intervideosrc->n_frames++;

  *buf = buffer;

  return GST_FLOW_OK;
}

static GstCaps *
gst_inter_video_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstStructure *structure;

  GST_DEBUG_OBJECT (src, "fixate");

  caps = gst_caps_make_writable (caps);
  caps = gst_caps_truncate (caps);
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_string (structure, "format", "I420");
  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);
  if (gst_structure_has_field (structure, "pixel-aspect-ratio"))
    gst_structure_fixate_field_nearest_fraction (structure,
        "pixel-aspect-ratio", 1, 1);
  if (gst_structure_has_field (structure, "color-matrix"))
    gst_structure_fixate_field_string (structure, "color-matrix", "sdtv");
  if (gst_structure_has_field (structure, "chroma-site"))
    gst_structure_fixate_field_string (structure, "chroma-site", "mpeg2");

  if (gst_structure_has_field (structure, "interlace-mode"))
    gst_structure_fixate_field_string (structure, "interlace-mode",
        "progressive");

  return caps;
}
