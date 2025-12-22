/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2007 Wim Taymans <wim.taymans@collabora.co.uk>
 * Copyright (C) 2007 Edward Hervey <edward.hervey@collabora.co.uk>
 * Copyright (C) 2007 Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * SECTION:element-chromahold
 * @title: chromahold
 *
 * The chromahold element will remove all color information for
 * all colors except a single one and converts them to grayscale.
 *
 * Sample pipeline:
 * |[
 * gst-launch-1.0 videotestsrc pattern=smpte75 ! \
 *   chromahold target-r=0 target-g=0 target-b=255 ! \
 *   videoconvert ! autovideosink     \
 * ]| This pipeline only keeps the red color.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstchromahold.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_chroma_hold_debug);
#define GST_CAT_DEFAULT gst_chroma_hold_debug

#define DEFAULT_TARGET_R 255
#define DEFAULT_TARGET_G 0
#define DEFAULT_TARGET_B 0
#define DEFAULT_TOLERANCE 30

enum
{
  PROP_0,
  PROP_TARGET_R,
  PROP_TARGET_G,
  PROP_TARGET_B,
  PROP_TOLERANCE
};

static GstStaticPadTemplate gst_chroma_hold_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ ARGB, BGRA, ABGR, RGBA, xRGB, BGRx, xBGR, RGBx}"))
    );

static GstStaticPadTemplate gst_chroma_hold_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ ARGB, BGRA, ABGR, RGBA, xRGB, BGRx, xBGR, RGBx}"))
    );

#define GST_CHROMA_HOLD_LOCK(self) G_STMT_START { \
  GST_LOG_OBJECT (self, "Locking chromahold from thread %p", g_thread_self ()); \
  g_mutex_lock (&self->lock); \
  GST_LOG_OBJECT (self, "Locked chromahold from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_CHROMA_HOLD_UNLOCK(self) G_STMT_START { \
  GST_LOG_OBJECT (self, "Unlocking chromahold from thread %p", \
      g_thread_self ()); \
  g_mutex_unlock (&self->lock); \
} G_STMT_END

static gboolean gst_chroma_hold_start (GstBaseTransform * trans);
static gboolean gst_chroma_hold_set_info (GstVideoFilter * vfilter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_chroma_hold_transform_frame_ip (GstVideoFilter *
    vfilter, GstVideoFrame * frame);
static void gst_chroma_hold_before_transform (GstBaseTransform * btrans,
    GstBuffer * buf);

static void gst_chroma_hold_init_params (GstChromaHold * self);
static gboolean gst_chroma_hold_set_process_function (GstChromaHold * self);

static void gst_chroma_hold_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_chroma_hold_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_chroma_hold_finalize (GObject * object);

#define gst_chroma_hold_parent_class parent_class
G_DEFINE_TYPE (GstChromaHold, gst_chroma_hold, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (chromahold, "chromahold",
    GST_RANK_NONE, gst_chroma_hold_get_type ());

static void
gst_chroma_hold_class_init (GstChromaHoldClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gobject_class->set_property = gst_chroma_hold_set_property;
  gobject_class->get_property = gst_chroma_hold_get_property;
  gobject_class->finalize = gst_chroma_hold_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_R,
      g_param_spec_uint ("target-r", "Target Red", "The Red target", 0, 255,
          DEFAULT_TARGET_R,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_G,
      g_param_spec_uint ("target-g", "Target Green", "The Green target", 0, 255,
          DEFAULT_TARGET_G,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_B,
      g_param_spec_uint ("target-b", "Target Blue", "The Blue target", 0, 255,
          DEFAULT_TARGET_B,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TOLERANCE,
      g_param_spec_uint ("tolerance", "Tolerance",
          "Tolerance for the target color", 0, 180, DEFAULT_TOLERANCE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  btrans_class->start = GST_DEBUG_FUNCPTR (gst_chroma_hold_start);
  btrans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_chroma_hold_before_transform);

  vfilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_chroma_hold_transform_frame_ip);
  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_chroma_hold_set_info);

  gst_element_class_set_static_metadata (gstelement_class, "Chroma hold filter",
      "Filter/Effect/Video",
      "Removes all color information except for one color",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chroma_hold_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_chroma_hold_src_template);

  GST_DEBUG_CATEGORY_INIT (gst_chroma_hold_debug, "chromahold", 0,
      "chromahold - Removes all color information except for one color");
}

static void
gst_chroma_hold_init (GstChromaHold * self)
{
  self->target_r = DEFAULT_TARGET_R;
  self->target_g = DEFAULT_TARGET_G;
  self->target_b = DEFAULT_TARGET_B;
  self->tolerance = DEFAULT_TOLERANCE;

  g_mutex_init (&self->lock);
}

static void
gst_chroma_hold_finalize (GObject * object)
{
  GstChromaHold *self = GST_CHROMA_HOLD (object);

  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_chroma_hold_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstChromaHold *self = GST_CHROMA_HOLD (object);

  GST_CHROMA_HOLD_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET_R:
      self->target_r = g_value_get_uint (value);
      gst_chroma_hold_init_params (self);
      break;
    case PROP_TARGET_G:
      self->target_g = g_value_get_uint (value);
      gst_chroma_hold_init_params (self);
      break;
    case PROP_TARGET_B:
      self->target_b = g_value_get_uint (value);
      gst_chroma_hold_init_params (self);
      break;
    case PROP_TOLERANCE:
      self->tolerance = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_CHROMA_HOLD_UNLOCK (self);
}

static void
gst_chroma_hold_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstChromaHold *self = GST_CHROMA_HOLD (object);

  switch (prop_id) {
    case PROP_TARGET_R:
      g_value_set_uint (value, self->target_r);
      break;
    case PROP_TARGET_G:
      g_value_set_uint (value, self->target_g);
      break;
    case PROP_TARGET_B:
      g_value_set_uint (value, self->target_b);
      break;
    case PROP_TOLERANCE:
      g_value_set_uint (value, self->tolerance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_chroma_hold_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstChromaHold *self = GST_CHROMA_HOLD (vfilter);

  GST_CHROMA_HOLD_LOCK (self);

  GST_DEBUG_OBJECT (self,
      "Setting caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);

  self->format = GST_VIDEO_INFO_FORMAT (in_info);
  self->width = GST_VIDEO_INFO_WIDTH (in_info);
  self->height = GST_VIDEO_INFO_HEIGHT (in_info);

  if (!gst_chroma_hold_set_process_function (self)) {
    GST_WARNING_OBJECT (self, "No processing function for this caps");
    GST_CHROMA_HOLD_UNLOCK (self);
    return FALSE;
  }

  GST_CHROMA_HOLD_UNLOCK (self);

  return TRUE;
}

static inline gint
rgb_to_hue (gint r, gint g, gint b)
{
  gint m, M, C, C2, h;

  m = MIN (MIN (r, g), b);
  M = MAX (MAX (r, g), b);
  C = M - m;
  C2 = C >> 1;

  if (C == 0) {
    return G_MAXUINT;
  } else if (M == r) {
    h = ((256 * 60 * (g - b) + C2) / C);
  } else if (M == g) {
    h = ((256 * 60 * (b - r) + C2) / C) + 120 * 256;
  } else {
    /* if (M == b) */
    h = ((256 * 60 * (r - g) + C2) / C) + 240 * 256;
  }
  h >>= 8;

  if (h >= 360)
    h -= 360;
  else if (h < 0)
    h += 360;

  return h;
}

static inline gint
hue_dist (gint h1, gint h2)
{
  gint d1, d2;

  d1 = h1 - h2;
  d2 = h2 - h1;

  if (d1 < 0)
    d1 += 360;
  if (d2 < 0)
    d2 += 360;

  return MIN (d1, d2);
}

static void
gst_chroma_hold_process_xrgb (GstVideoFrame * frame, gint width,
    gint height, GstChromaHold * self)
{
  gint i, j;
  gint r, g, b;
  gint grey;
  gint h1, h2;
  gint tolerance = self->tolerance;
  gint p[4];
  gint diff;
  gint row_wrap;
  guint8 *dest;

  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  p[0] = GST_VIDEO_FRAME_COMP_POFFSET (frame, 3);
  p[1] = GST_VIDEO_FRAME_COMP_POFFSET (frame, 0);
  p[2] = GST_VIDEO_FRAME_COMP_POFFSET (frame, 1);
  p[3] = GST_VIDEO_FRAME_COMP_POFFSET (frame, 2);
  row_wrap = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) - 4 * width;

  h1 = self->hue;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      r = dest[p[1]];
      g = dest[p[2]];
      b = dest[p[3]];

      h2 = rgb_to_hue (r, g, b);
      diff = hue_dist (h1, h2);
      if (h1 == G_MAXUINT || diff > tolerance) {
        grey = (13938 * r + 46869 * g + 4730 * b) >> 16;
        grey = CLAMP (grey, 0, 255);
        dest[p[1]] = grey;
        dest[p[2]] = grey;
        dest[p[3]] = grey;
      }

      dest += 4;
    }
    dest += row_wrap;
  }
}

/* Protected with the chroma hold lock */
static void
gst_chroma_hold_init_params (GstChromaHold * self)
{
  self->hue = rgb_to_hue (self->target_r, self->target_g, self->target_b);
}

/* Protected with the chroma hold lock */
static gboolean
gst_chroma_hold_set_process_function (GstChromaHold * self)
{
  self->process = NULL;

  switch (self->format) {
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
      self->process = gst_chroma_hold_process_xrgb;
      break;
    default:
      break;
  }
  return self->process != NULL;
}

static gboolean
gst_chroma_hold_start (GstBaseTransform * btrans)
{
  GstChromaHold *self = GST_CHROMA_HOLD (btrans);

  GST_CHROMA_HOLD_LOCK (self);
  gst_chroma_hold_init_params (self);
  GST_CHROMA_HOLD_UNLOCK (self);

  return TRUE;
}

static void
gst_chroma_hold_before_transform (GstBaseTransform * btrans, GstBuffer * buf)
{
  GstChromaHold *self = GST_CHROMA_HOLD (btrans);
  GstClockTime timestamp;

  timestamp = gst_segment_to_stream_time (&btrans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  GST_LOG ("Got stream time of %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));
  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (GST_OBJECT (self), timestamp);
}

static GstFlowReturn
gst_chroma_hold_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame)
{
  GstChromaHold *self = GST_CHROMA_HOLD (vfilter);

  GST_CHROMA_HOLD_LOCK (self);

  if (G_UNLIKELY (!self->process)) {
    GST_ERROR_OBJECT (self, "Not negotiated yet");
    GST_CHROMA_HOLD_UNLOCK (self);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  self->process (frame, self->width, self->height, self);

  GST_CHROMA_HOLD_UNLOCK (self);

  return GST_FLOW_OK;
}
