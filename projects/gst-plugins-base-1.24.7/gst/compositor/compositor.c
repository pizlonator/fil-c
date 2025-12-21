/* Video compositor plugin
 * Copyright (C) 2004, 2008 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 * Copyright (C) 2014 Thibault Saunier <tsaunier@gnome.org>
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
 * SECTION:element-compositor
 * @title: compositor
 *
 * Compositor can accept AYUV, VUYA, ARGB and BGRA video streams. For each of the requested
 * sink pads it will compare the incoming geometry and framerate to define the
 * output parameters. Indeed output video frames will have the geometry of the
 * biggest incoming video stream and the framerate of the fastest incoming one.
 *
 * Compositor will do colorspace conversion.
 *
 * Individual parameters for each input stream can be configured on the
 * #GstCompositorPad:
 *
 * * "xpos": The x-coordinate position of the top-left corner of the picture (#gint)
 * * "ypos": The y-coordinate position of the top-left corner of the picture (#gint)
 * * "width": The width of the picture; the input will be scaled if necessary (#gint)
 * * "height": The height of the picture; the input will be scaled if necessary (#gint)
 * * "alpha": The transparency of the picture; between 0.0 and 1.0. The blending
 *   is a simple copy when fully-transparent (0.0) and fully-opaque (1.0). (#gdouble)
 * * "zorder": The z-order position of the picture in the composition (#guint)
 *
 * ## Sample pipelines
 * |[
 * gst-launch-1.0 \
 *   videotestsrc pattern=1 ! \
 *   video/x-raw,format=AYUV,framerate=\(fraction\)10/1,width=100,height=100 ! \
 *   videobox border-alpha=0 top=-70 bottom=-70 right=-220 ! \
 *   compositor name=comp sink_0::alpha=0.7 sink_1::alpha=0.5 ! \
 *   videoconvert ! xvimagesink \
 *   videotestsrc ! \
 *   video/x-raw,format=AYUV,framerate=\(fraction\)5/1,width=320,height=240 ! comp.
 * ]| A pipeline to demonstrate compositor used together with videobox.
 * This should show a 320x240 pixels video test source with some transparency
 * showing the background checker pattern. Another video test source with just
 * the snow pattern of 100x100 pixels is overlaid on top of the first one on
 * the left vertically centered with a small transparency showing the first
 * video test source behind and the checker pattern under it. Note that the
 * framerate of the output video is 10 frames per second.
 * |[
 * gst-launch-1.0 videotestsrc pattern=1 ! \
 *   video/x-raw, framerate=\(fraction\)10/1, width=100, height=100 ! \
 *   compositor name=comp ! videoconvert ! ximagesink \
 *   videotestsrc !  \
 *   video/x-raw, framerate=\(fraction\)5/1, width=320, height=240 ! comp.
 * ]| A pipeline to demonstrate bgra comping. (This does not demonstrate alpha blending).
 * |[
 * gst-launch-1.0 videotestsrc pattern=1 ! \
 *   video/x-raw,format =I420, framerate=\(fraction\)10/1, width=100, height=100 ! \
 *   compositor name=comp ! videoconvert ! ximagesink \
 *   videotestsrc ! \
 *   video/x-raw,format=I420, framerate=\(fraction\)5/1, width=320, height=240 ! comp.
 * ]| A pipeline to test I420
 * |[
 * gst-launch-1.0 compositor name=comp sink_1::alpha=0.5 sink_1::xpos=50 sink_1::ypos=50 ! \
 *   videoconvert ! ximagesink \
 *   videotestsrc pattern=snow timestamp-offset=3000000000 ! \
 *   "video/x-raw,format=AYUV,width=640,height=480,framerate=(fraction)30/1" ! \
 *   timeoverlay ! queue2 ! comp. \
 *   videotestsrc pattern=smpte ! \
 *   "video/x-raw,format=AYUV,width=800,height=600,framerate=(fraction)10/1" ! \
 *   timeoverlay ! queue2 ! comp.
 * ]| A pipeline to demonstrate synchronized compositing (the second stream starts after 3 seconds)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "compositor.h"

#ifdef DISABLE_ORC
#define orc_memset memset
#else
#include <orc/orcfunctions.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_compositor_debug);
#define GST_CAT_DEFAULT gst_compositor_debug

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static void gst_compositor_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define GST_TYPE_COMPOSITOR_OPERATOR (gst_compositor_operator_get_type())
static GType
gst_compositor_operator_get_type (void)
{
  static GType compositor_operator_type = 0;

  static const GEnumValue compositor_operator[] = {
    {COMPOSITOR_OPERATOR_SOURCE, "Source", "source"},
    {COMPOSITOR_OPERATOR_OVER, "Over", "over"},
    {COMPOSITOR_OPERATOR_ADD, "Add", "add"},
    {0, NULL, NULL},
  };

  if (!compositor_operator_type) {
    compositor_operator_type =
        g_enum_register_static ("GstCompositorOperator", compositor_operator);
  }
  return compositor_operator_type;
}

#define GST_TYPE_COMPOSITOR_BACKGROUND (gst_compositor_background_get_type())
static GType
gst_compositor_background_get_type (void)
{
  static GType compositor_background_type = 0;

  static const GEnumValue compositor_background[] = {
    {COMPOSITOR_BACKGROUND_CHECKER, "Checker pattern", "checker"},
    {COMPOSITOR_BACKGROUND_BLACK, "Black", "black"},
    {COMPOSITOR_BACKGROUND_WHITE, "White", "white"},
    {COMPOSITOR_BACKGROUND_TRANSPARENT,
        "Transparent Background to enable further compositing", "transparent"},
    {0, NULL, NULL},
  };

  if (!compositor_background_type) {
    compositor_background_type =
        g_enum_register_static ("GstCompositorBackground",
        compositor_background);
  }
  return compositor_background_type;
}

#define GST_TYPE_COMPOSITOR_SIZING_POLICY (gst_compositor_sizing_policy_get_type())
static GType
gst_compositor_sizing_policy_get_type (void)
{
  static GType sizing_policy_type = 0;

  static const GEnumValue sizing_polices[] = {
    {COMPOSITOR_SIZING_POLICY_NONE,
        "None: Image is scaled to fill configured destination rectangle without "
          "padding or keeping the aspect ratio", "none"},
    {COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO,
          "Keep Aspect Ratio: Image is scaled to fit destination rectangle "
          "specified by GstCompositorPad:{xpos, ypos, width, height} "
          "with preserved aspect ratio. Resulting image will be centered in "
          "the destination rectangle with padding if necessary",
        "keep-aspect-ratio"},
    {0, NULL, NULL},
  };

  if (!sizing_policy_type) {
    sizing_policy_type =
        g_enum_register_static ("GstCompositorSizingPolicy", sizing_polices);
  }
  return sizing_policy_type;
}

#define DEFAULT_PAD_XPOS   0
#define DEFAULT_PAD_YPOS   0
#define DEFAULT_PAD_WIDTH  -1
#define DEFAULT_PAD_HEIGHT -1
#define DEFAULT_PAD_ALPHA  1.0
#define DEFAULT_PAD_OPERATOR COMPOSITOR_OPERATOR_OVER
#define DEFAULT_PAD_SIZING_POLICY COMPOSITOR_SIZING_POLICY_NONE

enum
{
  PROP_PAD_0,
  PROP_PAD_XPOS,
  PROP_PAD_YPOS,
  PROP_PAD_WIDTH,
  PROP_PAD_HEIGHT,
  PROP_PAD_ALPHA,
  PROP_PAD_OPERATOR,
  PROP_PAD_SIZING_POLICY,
};

G_DEFINE_TYPE (GstCompositorPad, gst_compositor_pad,
    GST_TYPE_VIDEO_AGGREGATOR_PARALLEL_CONVERT_PAD);

static void
gst_compositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCompositorPad *pad = GST_COMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      g_value_set_int (value, pad->xpos);
      break;
    case PROP_PAD_YPOS:
      g_value_set_int (value, pad->ypos);
      break;
    case PROP_PAD_WIDTH:
      g_value_set_int (value, pad->width);
      break;
    case PROP_PAD_HEIGHT:
      g_value_set_int (value, pad->height);
      break;
    case PROP_PAD_ALPHA:
      g_value_set_double (value, pad->alpha);
      break;
    case PROP_PAD_OPERATOR:
      g_value_set_enum (value, pad->op);
      break;
    case PROP_PAD_SIZING_POLICY:
      g_value_set_enum (value, pad->sizing_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_compositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCompositorPad *pad = GST_COMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      pad->xpos = g_value_get_int (value);
      break;
    case PROP_PAD_YPOS:
      pad->ypos = g_value_get_int (value);
      break;
    case PROP_PAD_WIDTH:
      pad->width = g_value_get_int (value);
      gst_video_aggregator_convert_pad_update_conversion_info
          (GST_VIDEO_AGGREGATOR_CONVERT_PAD (pad));
      break;
    case PROP_PAD_HEIGHT:
      pad->height = g_value_get_int (value);
      gst_video_aggregator_convert_pad_update_conversion_info
          (GST_VIDEO_AGGREGATOR_CONVERT_PAD (pad));
      break;
    case PROP_PAD_ALPHA:
      pad->alpha = g_value_get_double (value);
      break;
    case PROP_PAD_OPERATOR:
      pad->op = g_value_get_enum (value);
      gst_video_aggregator_pad_set_needs_alpha (GST_VIDEO_AGGREGATOR_PAD (pad),
          pad->op == COMPOSITOR_OPERATOR_ADD);
      break;
    case PROP_PAD_SIZING_POLICY:
      pad->sizing_policy = g_value_get_enum (value);
      gst_video_aggregator_convert_pad_update_conversion_info
          (GST_VIDEO_AGGREGATOR_CONVERT_PAD (pad));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
_mixer_pad_get_output_size (GstCompositor * comp, GstCompositorPad * comp_pad,
    gint out_par_n, gint out_par_d, gint * width, gint * height,
    gint * x_offset, gint * y_offset)
{
  GstVideoAggregatorPad *vagg_pad = GST_VIDEO_AGGREGATOR_PAD (comp_pad);
  gint pad_width, pad_height;
  guint dar_n, dar_d;

  *x_offset = 0;
  *y_offset = 0;
  *width = 0;
  *height = 0;

  /* FIXME: Anything better we can do here? */
  if (!vagg_pad->info.finfo
      || vagg_pad->info.finfo->format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_DEBUG_OBJECT (comp_pad, "Have no caps yet");
    return;
  }

  if (comp->zero_size_is_unscaled) {
    pad_width =
        comp_pad->width <=
        0 ? GST_VIDEO_INFO_WIDTH (&vagg_pad->info) : comp_pad->width;
    pad_height =
        comp_pad->height <=
        0 ? GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) : comp_pad->height;
  } else {
    pad_width =
        comp_pad->width <
        0 ? GST_VIDEO_INFO_WIDTH (&vagg_pad->info) : comp_pad->width;
    pad_height =
        comp_pad->height <
        0 ? GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) : comp_pad->height;
  }

  if (pad_width == 0 || pad_height == 0)
    return;

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
          GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
          GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d)) {
    GST_WARNING_OBJECT (comp_pad, "Cannot calculate display aspect ratio");
    return;
  }

  GST_LOG_OBJECT (comp_pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)", pad_width,
      pad_height, dar_n, dar_d, GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d);

  switch (comp_pad->sizing_policy) {
    case COMPOSITOR_SIZING_POLICY_NONE:
      /* Pick either height or width, whichever is an integer multiple of the
       * display aspect ratio. However, prefer preserving the height to account
       * for interlaced video. */
      if (pad_height % dar_n == 0) {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      } else if (pad_width % dar_d == 0) {
        pad_height = gst_util_uint64_scale_int (pad_width, dar_d, dar_n);
      } else {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      }
      break;
    case COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO:
    {
      gint from_dar_n, from_dar_d, to_dar_n, to_dar_d, num, den;

      /* Calculate DAR again with actual video size */
      if (!gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (&vagg_pad->info),
              GST_VIDEO_INFO_HEIGHT (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_D (&vagg_pad->info), &from_dar_n,
              &from_dar_d)) {
        from_dar_n = from_dar_d = -1;
      }

      if (!gst_util_fraction_multiply (pad_width, pad_height,
              out_par_n, out_par_d, &to_dar_n, &to_dar_d)) {
        to_dar_n = to_dar_d = -1;
      }

      if (from_dar_n != to_dar_n || from_dar_d != to_dar_d) {
        /* Calculate new output resolution */
        if (from_dar_n != -1 && from_dar_d != -1
            && gst_util_fraction_multiply (from_dar_n, from_dar_d,
                out_par_d, out_par_n, &num, &den)) {
          GstVideoRectangle src_rect, dst_rect, rst_rect;

          src_rect.h = gst_util_uint64_scale_int (pad_width, den, num);
          if (src_rect.h == 0) {
            pad_width = 0;
            pad_height = 0;
            break;
          }

          src_rect.x = src_rect.y = 0;
          src_rect.w = pad_width;

          dst_rect.x = dst_rect.y = 0;
          dst_rect.w = pad_width;
          dst_rect.h = pad_height;

          /* Scale rect to be centered in destination rect */
          gst_video_center_rect (&src_rect, &dst_rect, &rst_rect, TRUE);

          GST_LOG_OBJECT (comp_pad,
              "Re-calculated size %dx%d -> %dx%d (x-offset %d, y-offset %d)",
              pad_width, pad_height, rst_rect.w, rst_rect.h, rst_rect.x,
              rst_rect.h);

          *x_offset = rst_rect.x;
          *y_offset = rst_rect.y;
          pad_width = rst_rect.w;
          pad_height = rst_rect.h;
        } else {
          GST_WARNING_OBJECT (comp_pad, "Failed to calculate output size");

          *x_offset = 0;
          *y_offset = 0;
          pad_width = 0;
          pad_height = 0;
        }
      }
      break;
    }
  }

  *width = pad_width;
  *height = pad_height;
}

static gboolean
is_point_contained (const GstVideoRectangle rect, const gint px, const gint py)
{
  if ((px >= rect.x) && (px <= rect.x + rect.w) &&
      (py >= rect.y) && (py <= rect.y + rect.h))
    return TRUE;
  return FALSE;
}

/* Test whether rectangle2 contains rectangle 1 (geometrically) */
static gboolean
is_rectangle_contained (const GstVideoRectangle rect1,
    const GstVideoRectangle rect2)
{
  if ((rect2.x <= rect1.x) && (rect2.y <= rect1.y) &&
      ((rect2.x + rect2.w) >= (rect1.x + rect1.w)) &&
      ((rect2.y + rect2.h) >= (rect1.y + rect1.h)))
    return TRUE;
  return FALSE;
}

static GstVideoRectangle
clamp_rectangle (gint x, gint y, gint w, gint h, gint outer_width,
    gint outer_height)
{
  gint x2 = x + w;
  gint y2 = y + h;
  GstVideoRectangle clamped;

  /* Clamp the x/y coordinates of this frame to the output boundaries to cover
   * the case where (say, with negative xpos/ypos or w/h greater than the output
   * size) the non-obscured portion of the frame could be outside the bounds of
   * the video itself and hence not visible at all */
  clamped.x = CLAMP (x, 0, outer_width);
  clamped.y = CLAMP (y, 0, outer_height);
  clamped.w = CLAMP (x2, 0, outer_width) - clamped.x;
  clamped.h = CLAMP (y2, 0, outer_height) - clamped.y;

  return clamped;
}

/* Call this with the lock taken */
static gboolean
_pad_obscures_rectangle (GstVideoAggregator * vagg, GstVideoAggregatorPad * pad,
    const GstVideoRectangle rect)
{
  GstVideoRectangle pad_rect;
  GstCompositorPad *cpad = GST_COMPOSITOR_PAD (pad);
  GstStructure *converter_config = NULL;
  gboolean fill_border = TRUE;
  guint32 border_argb = 0xff000000;
  gint x_offset, y_offset;

  /* No buffer to obscure the rectangle with */
  if (!gst_video_aggregator_pad_has_current_buffer (pad))
    return FALSE;

  /* Can't obscure if we introduce alpha or if the format has an alpha
   * component as we'd have to inspect every pixel to know if the frame is
   * opaque, so assume it doesn't obscure
   */
  if (cpad->alpha != 1.0 || GST_VIDEO_INFO_HAS_ALPHA (&pad->info))
    return FALSE;

  /* If a converter-config is set and it is either configured to not fill any
   * borders, or configured to use a non-opaque color, then we have to handle
   * the pad as potentially containing transparency */
  g_object_get (pad, "converter-config", &converter_config, NULL);
  if (converter_config) {
    gst_structure_get (converter_config, GST_VIDEO_CONVERTER_OPT_BORDER_ARGB,
        G_TYPE_UINT, &border_argb, NULL);
    gst_structure_get (converter_config, GST_VIDEO_CONVERTER_OPT_FILL_BORDER,
        G_TYPE_BOOLEAN, &fill_border, NULL);
  }
  gst_clear_structure (&converter_config);
  if (!fill_border || (border_argb & 0xff000000) != 0xff000000)
    return FALSE;

  pad_rect.x = cpad->xpos;
  pad_rect.y = cpad->ypos;
  /* Handle pixel and display aspect ratios to find the actual size */
  _mixer_pad_get_output_size (GST_COMPOSITOR (vagg), cpad,
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info),
      &(pad_rect.w), &(pad_rect.h), &x_offset, &y_offset);
  pad_rect.x += x_offset;
  pad_rect.y += y_offset;

  if (!is_rectangle_contained (rect, pad_rect))
    return FALSE;

  GST_DEBUG_OBJECT (pad, "Pad %s %ix%i@(%i,%i) obscures rect %ix%i@(%i,%i)",
      GST_PAD_NAME (pad), pad_rect.w, pad_rect.h, pad_rect.x, pad_rect.y,
      rect.w, rect.h, rect.x, rect.y);

  return TRUE;
}

static void
gst_compositor_pad_prepare_frame_start (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  GstCompositorPad *cpad = GST_COMPOSITOR_PAD (pad);
  gint width, height;
  gboolean frame_obscured = FALSE;
  GList *l;
  /* The rectangle representing this frame, clamped to the video's boundaries.
   * Due to the clamping, this is different from the frame width/height above. */
  GstVideoRectangle frame_rect;

  /* There's three types of width/height here:
   * 1. GST_VIDEO_FRAME_WIDTH/HEIGHT:
   *     The frame width/height (same as pad->info.height/width;
   *     see gst_video_frame_map())
   * 2. cpad->width/height:
   *     The optional pad property for scaling the frame (if zero, the video is
   *     left unscaled)
   * 3. conversion_info.width/height:
   *     Equal to cpad->width/height if it's set, otherwise it's the pad
   *     width/height. See ->set_info()
   * */

  _mixer_pad_get_output_size (GST_COMPOSITOR (vagg), cpad,
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info),
      &width, &height, &cpad->x_offset, &cpad->y_offset);

  if (cpad->alpha == 0.0) {
    GST_DEBUG_OBJECT (pad, "Pad has alpha 0.0, not converting frame");
    return;
  }

  if (gst_aggregator_pad_is_inactive (GST_AGGREGATOR_PAD (pad)))
    return;

  frame_rect = clamp_rectangle (cpad->xpos + cpad->x_offset,
      cpad->ypos + cpad->y_offset, width, height,
      GST_VIDEO_INFO_WIDTH (&vagg->info), GST_VIDEO_INFO_HEIGHT (&vagg->info));

  if (frame_rect.w == 0 || frame_rect.h == 0) {
    GST_DEBUG_OBJECT (pad, "Resulting frame is zero-width or zero-height "
        "(w: %i, h: %i), skipping", frame_rect.w, frame_rect.h);
    return;
  }

  GST_OBJECT_LOCK (vagg);
  /* Check if this frame is obscured by a higher-zorder frame
   * TODO: Also skip a frame if it's obscured by a combination of
   * higher-zorder frames */
  l = g_list_find (GST_ELEMENT (vagg)->sinkpads, pad);
  /* The pad might've just been removed */
  if (l)
    l = l->next;
  for (; l; l = l->next) {
    GstBuffer *pad_buffer;

    pad_buffer =
        gst_video_aggregator_pad_get_current_buffer (GST_VIDEO_AGGREGATOR_PAD
        (l->data));

    if (pad_buffer == NULL)
      continue;

    if (gst_buffer_get_size (pad_buffer) == 0 &&
        GST_BUFFER_FLAG_IS_SET (pad_buffer, GST_BUFFER_FLAG_GAP)) {
      continue;
    }

    if (_pad_obscures_rectangle (vagg, l->data, frame_rect)) {
      frame_obscured = TRUE;
      break;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (frame_obscured)
    return;

  GST_VIDEO_AGGREGATOR_PAD_CLASS
      (gst_compositor_pad_parent_class)->prepare_frame_start (pad, vagg, buffer,
      prepared_frame);
}

static void
gst_compositor_pad_create_conversion_info (GstVideoAggregatorConvertPad * pad,
    GstVideoAggregator * vagg, GstVideoInfo * conversion_info)
{
  GstCompositor *self = GST_COMPOSITOR (vagg);
  GstCompositorPad *cpad = GST_COMPOSITOR_PAD (pad);
  gint width, height;
  gint x_offset, y_offset;

  GST_VIDEO_AGGREGATOR_CONVERT_PAD_CLASS
      (gst_compositor_pad_parent_class)->create_conversion_info (pad, vagg,
      conversion_info);
  if (!conversion_info->finfo)
    return;

  /* Need intermediate conversion? */
  if (self->intermediate_frame) {
    GstVideoInfo intermediate_info;
    gst_video_info_set_interlaced_format (&intermediate_info,
        GST_VIDEO_INFO_FORMAT (&self->intermediate_info),
        conversion_info->interlace_mode,
        GST_VIDEO_INFO_WIDTH (conversion_info),
        GST_VIDEO_INFO_HEIGHT (conversion_info));
    intermediate_info.colorimetry = conversion_info->colorimetry;
    intermediate_info.par_n = conversion_info->par_n;
    intermediate_info.par_d = conversion_info->par_d;
    intermediate_info.fps_n = conversion_info->fps_n;
    intermediate_info.fps_d = conversion_info->fps_d;
    intermediate_info.flags = conversion_info->flags;
    *conversion_info = intermediate_info;
  }

  _mixer_pad_get_output_size (self, cpad,
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info),
      &width, &height, &x_offset, &y_offset);

  /* The only thing that can change here is the width
   * and height, otherwise set_info would've been called */
  if (GST_VIDEO_INFO_WIDTH (conversion_info) != width ||
      GST_VIDEO_INFO_HEIGHT (conversion_info) != height) {
    GstVideoInfo tmp_info;

    /* Initialize with the wanted video format and our original width and
     * height as we don't want to rescale. Then copy over the wanted
     * colorimetry, and chroma-site and our current pixel-aspect-ratio
     * and other relevant fields.
     */
    gst_video_info_set_interlaced_format (&tmp_info,
        GST_VIDEO_INFO_FORMAT (conversion_info),
        conversion_info->interlace_mode, width, height);
    tmp_info.chroma_site = conversion_info->chroma_site;
    tmp_info.colorimetry = conversion_info->colorimetry;
    tmp_info.par_n = conversion_info->par_n;
    tmp_info.par_d = conversion_info->par_d;
    tmp_info.fps_n = conversion_info->fps_n;
    tmp_info.fps_d = conversion_info->fps_d;
    tmp_info.flags = conversion_info->flags;

    *conversion_info = tmp_info;
  }
}

static void
gst_compositor_pad_class_init (GstCompositorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorPadClass *vaggpadclass =
      (GstVideoAggregatorPadClass *) klass;
  GstVideoAggregatorConvertPadClass *vaggcpadclass =
      (GstVideoAggregatorConvertPadClass *) klass;

  gobject_class->set_property = gst_compositor_pad_set_property;
  gobject_class->get_property = gst_compositor_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_PAD_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_OPERATOR,
      g_param_spec_enum ("operator", "Operator",
          "Blending operator to use for blending this pad over the previous ones",
          GST_TYPE_COMPOSITOR_OPERATOR, DEFAULT_PAD_OPERATOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCompositorPad:sizing-policy:
   *
   * Specifies sizing policy to use. Depending on selected sizing policy,
   * scaled image might not fully cover the configured target rectangle area
   * (e.g., "keep-aspect-ratio"). In that case, any uncovered area will be
   * filled with background unless the uncovered area is drawn by other image.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_PAD_SIZING_POLICY,
      g_param_spec_enum ("sizing-policy", "Sizing policy",
          "Sizing policy to use for image scaling",
          GST_TYPE_COMPOSITOR_SIZING_POLICY, DEFAULT_PAD_SIZING_POLICY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  vaggpadclass->prepare_frame_start =
      GST_DEBUG_FUNCPTR (gst_compositor_pad_prepare_frame_start);

  vaggcpadclass->create_conversion_info =
      GST_DEBUG_FUNCPTR (gst_compositor_pad_create_conversion_info);

  gst_type_mark_as_plugin_api (GST_TYPE_COMPOSITOR_SIZING_POLICY, 0);
}

static void
gst_compositor_pad_init (GstCompositorPad * compo_pad)
{
  compo_pad->xpos = DEFAULT_PAD_XPOS;
  compo_pad->ypos = DEFAULT_PAD_YPOS;
  compo_pad->alpha = DEFAULT_PAD_ALPHA;
  compo_pad->op = DEFAULT_PAD_OPERATOR;
  compo_pad->width = DEFAULT_PAD_WIDTH;
  compo_pad->height = DEFAULT_PAD_HEIGHT;
  compo_pad->sizing_policy = DEFAULT_PAD_SIZING_POLICY;
}


/* GstCompositor */
#define DEFAULT_BACKGROUND COMPOSITOR_BACKGROUND_CHECKER
#define DEFAULT_ZERO_SIZE_IS_UNSCALED TRUE
#define DEFAULT_MAX_THREADS 0

enum
{
  PROP_0,
  PROP_BACKGROUND,
  PROP_ZERO_SIZE_IS_UNSCALED,
  PROP_MAX_THREADS,
  PROP_IGNORE_INACTIVE_PADS,
};

static void
gst_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstCompositor *self = GST_COMPOSITOR (object);

  switch (prop_id) {
    case PROP_BACKGROUND:
      g_value_set_enum (value, self->background);
      break;
    case PROP_ZERO_SIZE_IS_UNSCALED:
      g_value_set_boolean (value, self->zero_size_is_unscaled);
      break;
    case PROP_MAX_THREADS:
      g_value_set_uint (value, self->max_threads);
      break;
    case PROP_IGNORE_INACTIVE_PADS:
      g_value_set_boolean (value,
          gst_aggregator_get_ignore_inactive_pads (GST_AGGREGATOR (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstCompositor *self = GST_COMPOSITOR (object);

  switch (prop_id) {
    case PROP_BACKGROUND:
      self->background = g_value_get_enum (value);
      break;
    case PROP_ZERO_SIZE_IS_UNSCALED:
      self->zero_size_is_unscaled = g_value_get_boolean (value);
      break;
    case PROP_MAX_THREADS:
      self->max_threads = g_value_get_uint (value);
      break;
    case PROP_IGNORE_INACTIVE_PADS:
      gst_aggregator_set_ignore_inactive_pads (GST_AGGREGATOR (object),
          g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define gst_compositor_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstCompositor, gst_compositor,
    GST_TYPE_VIDEO_AGGREGATOR, G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_compositor_child_proxy_init));
GST_ELEMENT_REGISTER_DEFINE (compositor, "compositor", GST_RANK_PRIMARY + 1,
    GST_TYPE_COMPOSITOR);

static gboolean
set_functions (GstCompositor * self, const GstVideoInfo * info)
{
  gint offset[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  gint scale[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  gint i;

  gst_clear_buffer (&self->intermediate_frame);
  g_clear_pointer (&self->intermediate_convert, gst_video_converter_free);

  self->blend = NULL;
  self->overlay = NULL;
  self->fill_checker = NULL;
  self->fill_color = NULL;

  self->intermediate_info = *info;

  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_AYUV:
      self->blend = gst_compositor_blend_ayuv;
      self->overlay = gst_compositor_overlay_ayuv;
      self->fill_checker = gst_compositor_fill_checker_ayuv;
      self->fill_color = gst_compositor_fill_color_ayuv;
      break;
    case GST_VIDEO_FORMAT_VUYA:
      self->blend = gst_compositor_blend_vuya;
      self->overlay = gst_compositor_overlay_vuya;
      self->fill_checker = gst_compositor_fill_checker_vuya;
      self->fill_color = gst_compositor_fill_color_vuya;
      break;
    case GST_VIDEO_FORMAT_ARGB:
      self->blend = gst_compositor_blend_argb;
      self->overlay = gst_compositor_overlay_argb;
      self->fill_checker = gst_compositor_fill_checker_argb;
      self->fill_color = gst_compositor_fill_color_argb;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      self->blend = gst_compositor_blend_bgra;
      self->overlay = gst_compositor_overlay_bgra;
      self->fill_checker = gst_compositor_fill_checker_bgra;
      self->fill_color = gst_compositor_fill_color_bgra;
      break;
    case GST_VIDEO_FORMAT_ABGR:
      self->blend = gst_compositor_blend_abgr;
      self->overlay = gst_compositor_overlay_abgr;
      self->fill_checker = gst_compositor_fill_checker_abgr;
      self->fill_color = gst_compositor_fill_color_abgr;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      self->blend = gst_compositor_blend_rgba;
      self->overlay = gst_compositor_overlay_rgba;
      self->fill_checker = gst_compositor_fill_checker_rgba;
      self->fill_color = gst_compositor_fill_color_rgba;
      break;
    case GST_VIDEO_FORMAT_Y444_16LE:
      self->blend = gst_compositor_blend_y444_16le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_16le;
      self->fill_color = gst_compositor_fill_color_y444_16le;
      break;
    case GST_VIDEO_FORMAT_Y444_16BE:
      self->blend = gst_compositor_blend_y444_16be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_16be;
      self->fill_color = gst_compositor_fill_color_y444_16be;
      break;
    case GST_VIDEO_FORMAT_Y444_12LE:
      self->blend = gst_compositor_blend_y444_12le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_12le;
      self->fill_color = gst_compositor_fill_color_y444_12le;
      break;
    case GST_VIDEO_FORMAT_Y444_12BE:
      self->blend = gst_compositor_blend_y444_12be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_12be;
      self->fill_color = gst_compositor_fill_color_y444_12be;
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
      self->blend = gst_compositor_blend_y444_10le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_10le;
      self->fill_color = gst_compositor_fill_color_y444_10le;
      break;
    case GST_VIDEO_FORMAT_Y444_10BE:
      self->blend = gst_compositor_blend_y444_10be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444_10be;
      self->fill_color = gst_compositor_fill_color_y444_10be;
      break;
    case GST_VIDEO_FORMAT_Y444:
      self->blend = gst_compositor_blend_y444;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y444;
      self->fill_color = gst_compositor_fill_color_y444;
      break;
    case GST_VIDEO_FORMAT_Y42B:
      self->blend = gst_compositor_blend_y42b;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y42b;
      self->fill_color = gst_compositor_fill_color_y42b;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      self->blend = gst_compositor_blend_yuy2;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_yuy2;
      self->fill_color = gst_compositor_fill_color_yuy2;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      self->blend = gst_compositor_blend_uyvy;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_uyvy;
      self->fill_color = gst_compositor_fill_color_uyvy;
      break;
    case GST_VIDEO_FORMAT_YVYU:
      self->blend = gst_compositor_blend_yvyu;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_yvyu;
      self->fill_color = gst_compositor_fill_color_yvyu;
      break;
    case GST_VIDEO_FORMAT_I422_12LE:
      self->blend = gst_compositor_blend_i422_12le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i422_12le;
      self->fill_color = gst_compositor_fill_color_i422_12le;
      break;
    case GST_VIDEO_FORMAT_I422_12BE:
      self->blend = gst_compositor_blend_i422_12be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i422_12be;
      self->fill_color = gst_compositor_fill_color_i422_12be;
      break;
    case GST_VIDEO_FORMAT_I422_10LE:
      self->blend = gst_compositor_blend_i422_10le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i422_10le;
      self->fill_color = gst_compositor_fill_color_i422_10le;
      break;
    case GST_VIDEO_FORMAT_I422_10BE:
      self->blend = gst_compositor_blend_i422_10be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i422_10be;
      self->fill_color = gst_compositor_fill_color_i422_10be;
      break;
    case GST_VIDEO_FORMAT_I420_12LE:
      self->blend = gst_compositor_blend_i420_12le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i420_12le;
      self->fill_color = gst_compositor_fill_color_i420_12le;
      break;
    case GST_VIDEO_FORMAT_I420_12BE:
      self->blend = gst_compositor_blend_i420_12be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i420_12be;
      self->fill_color = gst_compositor_fill_color_i420_12be;
      break;
    case GST_VIDEO_FORMAT_I420_10LE:
      self->blend = gst_compositor_blend_i420_10le;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i420_10le;
      self->fill_color = gst_compositor_fill_color_i420_10le;
      break;
    case GST_VIDEO_FORMAT_I420_10BE:
      self->blend = gst_compositor_blend_i420_10be;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i420_10be;
      self->fill_color = gst_compositor_fill_color_i420_10be;
      break;
    case GST_VIDEO_FORMAT_I420:
      self->blend = gst_compositor_blend_i420;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_i420;
      self->fill_color = gst_compositor_fill_color_i420;
      break;
    case GST_VIDEO_FORMAT_YV12:
      self->blend = gst_compositor_blend_yv12;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_yv12;
      self->fill_color = gst_compositor_fill_color_yv12;
      break;
    case GST_VIDEO_FORMAT_NV12:
      self->blend = gst_compositor_blend_nv12;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_nv12;
      self->fill_color = gst_compositor_fill_color_nv12;
      break;
    case GST_VIDEO_FORMAT_NV21:
      self->blend = gst_compositor_blend_nv21;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_nv21;
      self->fill_color = gst_compositor_fill_color_nv21;
      break;
    case GST_VIDEO_FORMAT_Y41B:
      self->blend = gst_compositor_blend_y41b;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_y41b;
      self->fill_color = gst_compositor_fill_color_y41b;
      break;
    case GST_VIDEO_FORMAT_RGB:
      self->blend = gst_compositor_blend_rgb;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_rgb;
      self->fill_color = gst_compositor_fill_color_rgb;
      break;
    case GST_VIDEO_FORMAT_BGR:
      self->blend = gst_compositor_blend_bgr;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_bgr;
      self->fill_color = gst_compositor_fill_color_bgr;
      break;
    case GST_VIDEO_FORMAT_xRGB:
      self->blend = gst_compositor_blend_xrgb;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_xrgb;
      self->fill_color = gst_compositor_fill_color_xrgb;
      break;
    case GST_VIDEO_FORMAT_xBGR:
      self->blend = gst_compositor_blend_xbgr;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_xbgr;
      self->fill_color = gst_compositor_fill_color_xbgr;
      break;
    case GST_VIDEO_FORMAT_RGBx:
      self->blend = gst_compositor_blend_rgbx;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_rgbx;
      self->fill_color = gst_compositor_fill_color_rgbx;
      break;
    case GST_VIDEO_FORMAT_BGRx:
      self->blend = gst_compositor_blend_bgrx;
      self->overlay = self->blend;
      self->fill_checker = gst_compositor_fill_checker_bgrx;
      self->fill_color = gst_compositor_fill_color_bgrx;
      break;
    case GST_VIDEO_FORMAT_ARGB64:
      self->blend = gst_compositor_blend_argb64;
      self->overlay = gst_compositor_overlay_argb64;
      self->fill_checker = gst_compositor_fill_checker_argb64;
      self->fill_color = gst_compositor_fill_color_argb64;
      break;
    case GST_VIDEO_FORMAT_AYUV64:
      self->blend = gst_compositor_blend_ayuv64;
      self->overlay = gst_compositor_overlay_ayuv64;
      self->fill_checker = gst_compositor_fill_checker_ayuv64;
      self->fill_color = gst_compositor_fill_color_ayuv64;
      break;
    default:
    {
      GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
      GstVideoInfo *intermediate_info = &self->intermediate_info;
      if (GST_VIDEO_INFO_IS_YUV (info)) {
        if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) == 8)
          format = GST_VIDEO_FORMAT_AYUV;
        else
          format = GST_VIDEO_FORMAT_AYUV64;
      } else {
        if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) == 8)
          format = GST_VIDEO_FORMAT_ARGB;
        else
          format = GST_VIDEO_FORMAT_ARGB64;
      }

      switch (format) {
        case GST_VIDEO_FORMAT_AYUV:
          self->blend = gst_compositor_blend_ayuv;
          self->overlay = gst_compositor_overlay_ayuv;
          self->fill_checker = gst_compositor_fill_checker_ayuv;
          self->fill_color = gst_compositor_fill_color_ayuv;
          break;
        case GST_VIDEO_FORMAT_AYUV64:
          self->blend = gst_compositor_blend_ayuv64;
          self->overlay = gst_compositor_overlay_ayuv64;
          self->fill_checker = gst_compositor_fill_checker_ayuv64;
          self->fill_color = gst_compositor_fill_color_ayuv64;
          break;
        case GST_VIDEO_FORMAT_ARGB:
          self->blend = gst_compositor_blend_argb;
          self->overlay = gst_compositor_overlay_argb;
          self->fill_checker = gst_compositor_fill_checker_argb;
          self->fill_color = gst_compositor_fill_color_argb;
          break;
        case GST_VIDEO_FORMAT_ARGB64:
          self->blend = gst_compositor_blend_argb64;
          self->overlay = gst_compositor_overlay_argb64;
          self->fill_checker = gst_compositor_fill_checker_argb64;
          self->fill_color = gst_compositor_fill_color_argb64;
          break;
        default:
          GST_ERROR_OBJECT (self, "Unhandled format %s -> %s",
              gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)),
              gst_video_format_to_string (format));
          return FALSE;
      }

      GST_DEBUG_OBJECT (self,
          "Configured intermediate format %s for output format %s",
          gst_video_format_to_string (format),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));

      /* needs intermediate conversion */
      gst_video_info_set_interlaced_format (intermediate_info,
          format, info->interlace_mode, info->width, info->height);
      intermediate_info->par_n = info->par_n;
      intermediate_info->par_d = info->par_d;
      intermediate_info->fps_n = info->fps_n;
      intermediate_info->fps_d = info->fps_d;
      intermediate_info->flags = info->flags;

      /* preserve colorimetry if required */
      if (!GST_VIDEO_INFO_IS_GRAY (info))
        intermediate_info->colorimetry = info->colorimetry;

      self->intermediate_frame =
          gst_buffer_new_and_alloc (self->intermediate_info.size);
      break;
    }
  }

  /* calculate black and white colors */
  gst_video_color_range_offsets (self->intermediate_info.colorimetry.range,
      self->intermediate_info.finfo, offset, scale);
  if (GST_VIDEO_INFO_IS_YUV (info)) {
    /* black color [0.0, 0.0, 0.0] */
    self->black_color[0] = offset[0];

    /* white color [1.0, 0.0, 0.0] */
    self->white_color[0] = scale[0] + offset[0];

    for (i = 1; i < 3; i++)
      self->black_color[i] = self->white_color[i] = offset[i];
  } else {
    for (i = 0; i < 3; i++) {
      self->black_color[i] = offset[i];
      self->white_color[i] = scale[i] + offset[i];
    }
  }

  GST_DEBUG_OBJECT (self,
      "Calculated background color block: [%d %d %d], white: [%d %d %d]",
      self->black_color[0], self->black_color[1], self->black_color[2],
      self->white_color[0], self->white_color[1], self->white_color[2]);

  return TRUE;
}

static GstCaps *
_fixate_caps (GstAggregator * agg, GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GList *l;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  gint par_n, par_d;
  gdouble best_fps = 0.;
  GstCaps *ret = NULL;
  GstStructure *s;

  ret = gst_caps_make_writable (caps);

  /* we need this to calculate how large to make the output frame */
  s = gst_caps_get_structure (ret, 0);
  if (gst_structure_has_field (s, "pixel-aspect-ratio")) {
    gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio", 1, 1);
    gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);
  } else {
    par_n = par_d = 1;
  }

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *vaggpad = l->data;
    GstCompositorPad *compositor_pad = GST_COMPOSITOR_PAD (vaggpad);
    gint this_width, this_height;
    gint width, height;
    gint fps_n, fps_d;
    gdouble cur_fps;
    gint x_offset;
    gint y_offset;

    if (gst_aggregator_pad_is_inactive (GST_AGGREGATOR_PAD (vaggpad)))
      continue;

    fps_n = GST_VIDEO_INFO_FPS_N (&vaggpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&vaggpad->info);
    _mixer_pad_get_output_size (GST_COMPOSITOR (vagg), compositor_pad, par_n,
        par_d, &width, &height, &x_offset, &y_offset);

    if (width == 0 || height == 0)
      continue;

    /* {x,y}_offset represent padding size of each top and left area.
     * To calculate total resolution, count bottom and right padding area
     * as well here */
    this_width = width + MAX (compositor_pad->xpos + 2 * x_offset, 0);
    this_height = height + MAX (compositor_pad->ypos + 2 * y_offset, 0);

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }

  gst_structure_fixate_field_nearest_int (s, "width", best_width);
  gst_structure_fixate_field_nearest_int (s, "height", best_height);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", best_fps_n,
      best_fps_d);
  ret = gst_caps_fixate (ret);

  return ret;
}

static void
gst_parallelized_task_thread_func (gpointer data)
{
  GstParallelizedTaskRunner *runner = data;
  gint idx;

  g_mutex_lock (&runner->lock);
  idx = runner->n_todo--;
  g_assert (runner->n_todo >= -1);
  g_mutex_unlock (&runner->lock);

  g_assert (runner->func != NULL);

  runner->func (runner->task_data[idx]);
}

static void
gst_parallelized_task_runner_join (GstParallelizedTaskRunner * self)
{
  gboolean joined = FALSE;

  while (!joined) {
    g_mutex_lock (&self->lock);
    if (!(joined = gst_queue_array_is_empty (self->tasks))) {
      gpointer task = gst_queue_array_pop_head (self->tasks);
      g_mutex_unlock (&self->lock);
      gst_task_pool_join (self->pool, task);
    } else {
      g_mutex_unlock (&self->lock);
    }
  }
}

static void
gst_parallelized_task_runner_free (GstParallelizedTaskRunner * self)
{
  gst_parallelized_task_runner_join (self);

  gst_queue_array_free (self->tasks);
  if (self->own_pool)
    gst_task_pool_cleanup (self->pool);
  gst_object_unref (self->pool);
  g_mutex_clear (&self->lock);
  g_free (self);
}

static GstParallelizedTaskRunner *
gst_parallelized_task_runner_new (guint n_threads, GstTaskPool * pool,
    gboolean async_tasks)
{
  GstParallelizedTaskRunner *self;

  if (n_threads == 0)
    n_threads = g_get_num_processors ();

  self = g_new0 (GstParallelizedTaskRunner, 1);

  if (pool) {
    self->pool = g_object_ref (pool);
    self->own_pool = FALSE;

    /* No reason to split up the work between more threads than the
     * pool can spawn */
    if (GST_IS_SHARED_TASK_POOL (pool))
      n_threads =
          MIN (n_threads,
          gst_shared_task_pool_get_max_threads (GST_SHARED_TASK_POOL (pool)));
  } else {
    self->pool = gst_shared_task_pool_new ();
    self->own_pool = TRUE;
    gst_shared_task_pool_set_max_threads (GST_SHARED_TASK_POOL (self->pool),
        n_threads);
    gst_task_pool_prepare (self->pool, NULL);
  }

  self->tasks = gst_queue_array_new (n_threads);

  self->n_threads = n_threads;

  self->n_todo = -1;
  g_mutex_init (&self->lock);

  /* Set when scheduling a job */
  self->func = NULL;
  self->task_data = NULL;
  self->async_tasks = async_tasks;

  return self;
}

static void
gst_parallelized_task_runner_finish (GstParallelizedTaskRunner * self)
{
  g_return_if_fail (self->func != NULL);

  gst_parallelized_task_runner_join (self);

  self->func = NULL;
  self->task_data = NULL;
}

static void
gst_parallelized_task_runner_run (GstParallelizedTaskRunner * self,
    GstParallelizedTaskFunc func, gpointer * task_data)
{
  guint n_threads = self->n_threads;

  self->func = func;
  self->task_data = task_data;

  if (n_threads > 1 || self->async_tasks) {
    guint i = 0;
    g_mutex_lock (&self->lock);
    self->n_todo = self->n_threads - 1;
    if (!self->async_tasks) {
      /* if not async, perform one of the functions in the current thread */
      self->n_todo--;
      i = 1;
    }
    for (; i < n_threads; i++) {
      gpointer task =
          gst_task_pool_push (self->pool, gst_parallelized_task_thread_func,
          self, NULL);

      /* The return value of push() is nullable but NULL is only returned
       * with the shared task pool when gst_task_pool_prepare() has not been
       * called and would thus be a programming error that we should hard-fail
       * on.
       */
      g_assert (task != NULL);
      gst_queue_array_push_tail (self->tasks, task);
    }
    g_mutex_unlock (&self->lock);
  }

  if (!self->async_tasks) {
    self->func (self->task_data[self->n_threads - 1]);

    gst_parallelized_task_runner_finish (self);
  }
}

static gboolean
_negotiated_caps (GstAggregator * agg, GstCaps * caps)
{
  GstCompositor *compositor = GST_COMPOSITOR (agg);
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstVideoInfo v_info;
  guint n_threads;
  GList *iter;
  guint n_sdr = 0;
  guint n_hlg = 0;
  guint n_pq = 0;

  GST_DEBUG_OBJECT (agg, "Negotiated caps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&v_info, caps))
    return FALSE;

  if (!set_functions (compositor, &v_info)) {
    GST_ERROR_OBJECT (agg, "Failed to setup vfuncs");
    return FALSE;
  }

  GST_OBJECT_LOCK (vagg);
  for (iter = GST_ELEMENT (vagg)->sinkpads; iter; iter = g_list_next (iter)) {
    GstVideoAggregatorPad *pad = (GstVideoAggregatorPad *) iter->data;

    if (!pad->info.finfo)
      continue;

    switch (pad->info.colorimetry.transfer) {
      case GST_VIDEO_TRANSFER_SMPTE2084:
        n_pq++;
        break;
      case GST_VIDEO_TRANSFER_ARIB_STD_B67:
        n_hlg++;
        break;
      default:
        n_sdr++;
        break;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  /* TODO: we don't have support for HDR tone-mapping, so mixing HDR/SDR might
   * result in somewhat visually bad image. Needs enhancement to the
   * video-convert or somewhere */
  if (n_sdr > 0) {
    if (n_hlg > 0 || n_pq > 0) {
      GST_ELEMENT_WARNING (compositor, STREAM, NOT_IMPLEMENTED,
          ("Mixing SDR and HDR contents would result in color loss"), (NULL));
    }
  } else if (n_hlg > 0 && n_pq > 0) {
    GST_ELEMENT_WARNING (compositor, STREAM, NOT_IMPLEMENTED,
        ("Mixing HDR10 and HLG contents would result in color loss"), (NULL));
  }

  if (compositor->max_threads == 0)
    n_threads = g_get_num_processors ();
  else
    n_threads = compositor->max_threads;

  /* Magic number of 200 lines */
  if (GST_VIDEO_INFO_HEIGHT (&v_info) / n_threads < 200)
    n_threads = (GST_VIDEO_INFO_HEIGHT (&v_info) + 199) / 200;
  if (n_threads < 1)
    n_threads = 1;

  /* XXX: implement better thread count change */
  if (compositor->blend_runner
      && compositor->blend_runner->n_threads != n_threads) {
    gst_parallelized_task_runner_free (compositor->blend_runner);
    compositor->blend_runner = NULL;
  }
  if (!compositor->blend_runner) {
    GstTaskPool *pool = gst_video_aggregator_get_execution_task_pool (vagg);
    compositor->blend_runner =
        gst_parallelized_task_runner_new (n_threads, pool, FALSE);
    gst_clear_object (&pool);
  }

  if (compositor->intermediate_frame) {
    GstStructure *config = NULL;
    GstTaskPool *pool = gst_video_aggregator_get_execution_task_pool (vagg);

    if (pool && n_threads > 1) {
      config = gst_structure_new_empty ("GstVideoConverterConfig");
      gst_structure_set (config, GST_VIDEO_CONVERTER_OPT_THREADS,
          G_TYPE_UINT, n_threads, NULL);
    }

    compositor->intermediate_convert =
        gst_video_converter_new_with_pool (&compositor->intermediate_info,
        &v_info, config, pool);
    gst_clear_object (&pool);
  }

  return GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps);
}

static gboolean
gst_composior_stop (GstAggregator * agg)
{
  GstCompositor *self = GST_COMPOSITOR (agg);

  gst_clear_buffer (&self->intermediate_frame);
  g_clear_pointer (&self->intermediate_convert, gst_video_converter_free);

  return GST_AGGREGATOR_CLASS (parent_class)->stop (agg);
}

static gboolean
_should_draw_background (GstVideoAggregator * vagg)
{
  GstVideoRectangle bg_rect;
  gboolean draw = TRUE;
  GList *l;

  bg_rect.x = bg_rect.y = 0;

  GST_OBJECT_LOCK (vagg);
  bg_rect.w = GST_VIDEO_INFO_WIDTH (&vagg->info);
  bg_rect.h = GST_VIDEO_INFO_HEIGHT (&vagg->info);
  /* Check if the background is completely obscured by a pad
   * TODO: Also skip if it's obscured by a combination of pads */
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    if (gst_aggregator_pad_is_inactive (GST_AGGREGATOR_PAD (l->data))
        ||
        gst_video_aggregator_pad_get_prepared_frame (GST_VIDEO_AGGREGATOR_PAD
            (l->data)) == NULL)
      continue;

    if (_pad_obscures_rectangle (vagg, l->data, bg_rect)) {
      draw = FALSE;
      break;
    }
  }
  GST_OBJECT_UNLOCK (vagg);
  return draw;
}

static gboolean
frames_can_copy (const GstVideoFrame * frame1, const GstVideoFrame * frame2)
{
  if (GST_VIDEO_FRAME_FORMAT (frame1) != GST_VIDEO_FRAME_FORMAT (frame2))
    return FALSE;
  if (GST_VIDEO_FRAME_HEIGHT (frame1) != GST_VIDEO_FRAME_HEIGHT (frame2))
    return FALSE;
  if (GST_VIDEO_FRAME_WIDTH (frame1) != GST_VIDEO_FRAME_WIDTH (frame2))
    return FALSE;
  return TRUE;
}

struct CompositePadInfo
{
  GstVideoFrame *prepared_frame;
  GstCompositorPad *pad;
  GstCompositorBlendMode blend_mode;
};

struct CompositeTask
{
  GstCompositor *compositor;
  GstVideoFrame *out_frame;
  guint dst_line_start;
  guint dst_line_end;
  gboolean draw_background;
  guint n_pads;
  struct CompositePadInfo *pads_info;
};

static void
_draw_background (GstCompositor * comp, GstVideoFrame * outframe,
    guint y_start, guint y_end, BlendFunction * composite)
{
  *composite = comp->blend;

  switch (comp->background) {
    case COMPOSITOR_BACKGROUND_CHECKER:
      comp->fill_checker (outframe, y_start, y_end);
      break;
    case COMPOSITOR_BACKGROUND_BLACK:
      comp->fill_color (outframe, y_start, y_end,
          comp->black_color[GST_VIDEO_COMP_Y],
          comp->black_color[GST_VIDEO_COMP_U],
          comp->black_color[GST_VIDEO_COMP_V]);
      break;
    case COMPOSITOR_BACKGROUND_WHITE:
      comp->fill_color (outframe, y_start, y_end,
          comp->white_color[GST_VIDEO_COMP_Y],
          comp->white_color[GST_VIDEO_COMP_U],
          comp->white_color[GST_VIDEO_COMP_V]);
      break;
    case COMPOSITOR_BACKGROUND_TRANSPARENT:
    {
      guint i, plane, num_planes, height;

      num_planes = GST_VIDEO_FRAME_N_PLANES (outframe);
      for (plane = 0; plane < num_planes; ++plane) {
        const GstVideoFormatInfo *info;
        gint comp[GST_VIDEO_MAX_COMPONENTS];
        guint8 *pdata;
        gsize rowsize, plane_stride;
        gint yoffset;

        info = outframe->info.finfo;
        pdata = GST_VIDEO_FRAME_PLANE_DATA (outframe, plane);
        plane_stride = GST_VIDEO_FRAME_PLANE_STRIDE (outframe, plane);

        gst_video_format_info_component (info, plane, comp);
        rowsize = GST_VIDEO_FRAME_COMP_WIDTH (outframe, comp[0])
            * GST_VIDEO_FRAME_COMP_PSTRIDE (outframe, comp[0]);
        height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, comp[0],
            (y_end - y_start));

        yoffset = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, comp[0], y_start);

        pdata += yoffset * plane_stride;
        for (i = 0; i < height; ++i) {
          memset (pdata, 0, rowsize);
          pdata += plane_stride;
        }
      }
      /* use overlay to keep background transparent */
      *composite = comp->overlay;
      break;
    }
  }
}

static void
blend_pads (struct CompositeTask *comp)
{
  BlendFunction composite;
  guint i;

  composite = comp->compositor->blend;

  if (comp->draw_background) {
    _draw_background (comp->compositor, comp->out_frame, comp->dst_line_start,
        comp->dst_line_end, &composite);
  }

  for (i = 0; i < comp->n_pads; i++) {
    composite (comp->pads_info[i].prepared_frame,
        comp->pads_info[i].pad->xpos + comp->pads_info[i].pad->x_offset,
        comp->pads_info[i].pad->ypos + comp->pads_info[i].pad->y_offset,
        comp->pads_info[i].pad->alpha, comp->out_frame, comp->dst_line_start,
        comp->dst_line_end, comp->pads_info[i].blend_mode);
  }
}

static GstFlowReturn
gst_compositor_aggregate_frames (GstVideoAggregator * vagg, GstBuffer * outbuf)
{
  GstCompositor *compositor = GST_COMPOSITOR (vagg);
  GList *l;
  GstVideoFrame out_frame, intermediate_frame, *outframe;
  gboolean draw_background;
  guint drawn_a_pad = FALSE;
  struct CompositePadInfo *pads_info;
  guint i, n_pads = 0;

  if (!gst_video_frame_map (&out_frame, &vagg->info, outbuf, GST_MAP_WRITE)) {
    GST_WARNING_OBJECT (vagg, "Could not map output buffer");
    return GST_FLOW_ERROR;
  }

  outframe = &out_frame;

  if (compositor->intermediate_frame) {
    if (!gst_video_frame_map (&intermediate_frame,
            &compositor->intermediate_info, compositor->intermediate_frame,
            GST_MAP_READWRITE)) {
      GST_WARNING_OBJECT (vagg, "Could not map intermediate buffer");
      gst_video_frame_unmap (&out_frame);
      return GST_FLOW_ERROR;
    }

    outframe = &intermediate_frame;
  }

  /* If one of the frames to be composited completely obscures the background,
   * don't bother drawing the background at all. We can also always use the
   * 'blend' BlendFunction in that case because it only changes if we have to
   * overlay on top of a transparent background. */
  draw_background = _should_draw_background (vagg);

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *pad = l->data;
    GstVideoFrame *prepared_frame =
        gst_video_aggregator_pad_get_prepared_frame (pad);

    if (prepared_frame)
      n_pads++;
  }

  /* If no prepared frame, we should draw background unconditionally in order
   * to clear output buffer */
  if (n_pads == 0)
    draw_background = TRUE;

  pads_info = g_newa (struct CompositePadInfo, n_pads);
  n_pads = 0;

  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *pad = l->data;
    GstCompositorPad *compo_pad = GST_COMPOSITOR_PAD (pad);
    GstVideoFrame *prepared_frame =
        gst_video_aggregator_pad_get_prepared_frame (pad);
    GstCompositorBlendMode blend_mode = COMPOSITOR_BLEND_MODE_OVER;

    switch (compo_pad->op) {
      case COMPOSITOR_OPERATOR_SOURCE:
        blend_mode = COMPOSITOR_BLEND_MODE_SOURCE;
        break;
      case COMPOSITOR_OPERATOR_OVER:
        blend_mode = COMPOSITOR_BLEND_MODE_OVER;
        break;
      case COMPOSITOR_OPERATOR_ADD:
        blend_mode = COMPOSITOR_BLEND_MODE_ADD;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    if (prepared_frame != NULL) {
      /* If this is the first pad we're drawing, and we didn't draw the
       * background, and @prepared_frame has the same format, height, and width
       * as @outframe, then we can just copy it as-is. Subsequent pads (if any)
       * will be composited on top of it. */
      if (!drawn_a_pad && !draw_background &&
          frames_can_copy (prepared_frame, outframe)) {
        gst_video_frame_copy (outframe, prepared_frame);
      } else {
        pads_info[n_pads].pad = compo_pad;
        pads_info[n_pads].prepared_frame = prepared_frame;
        pads_info[n_pads].blend_mode = blend_mode;
        n_pads++;
      }
      drawn_a_pad = TRUE;
    }
  }

  {
    guint n_threads, lines_per_thread;
    guint out_height;
    struct CompositeTask *tasks;
    struct CompositeTask **tasks_p;

    n_threads = compositor->blend_runner->n_threads;

    tasks = g_newa (struct CompositeTask, n_threads);
    tasks_p = g_newa (struct CompositeTask *, n_threads);

    out_height = GST_VIDEO_FRAME_HEIGHT (outframe);
    lines_per_thread = (out_height + n_threads - 1) / n_threads;

    for (i = 0; i < n_threads; i++) {
      tasks[i].compositor = compositor;
      tasks[i].n_pads = n_pads;
      tasks[i].pads_info = pads_info;
      tasks[i].out_frame = outframe;
      tasks[i].draw_background = draw_background;
      /* This is a dumb split of the work by number of output lines.
       * If there is a section of the output that reads from a lot of source
       * pads, then that thread will consume more time. Maybe tracking and
       * splitting on the source fill rate would produce better results. */
      tasks[i].dst_line_start = i * lines_per_thread;
      tasks[i].dst_line_end = MIN ((i + 1) * lines_per_thread, out_height);

      tasks_p[i] = &tasks[i];
    }

    gst_parallelized_task_runner_run (compositor->blend_runner,
        (GstParallelizedTaskFunc) blend_pads, (gpointer *) tasks_p);
  }

  GST_OBJECT_UNLOCK (vagg);

  if (compositor->intermediate_frame) {
    gst_video_converter_frame (compositor->intermediate_convert,
        &intermediate_frame, &out_frame);

    gst_video_frame_unmap (&intermediate_frame);
  }

  gst_video_frame_unmap (&out_frame);

  return GST_FLOW_OK;
}

static GstPad *
gst_compositor_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL)
    goto could_not_create;

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return newpad;

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return NULL;
  }
}

static void
gst_compositor_release_pad (GstElement * element, GstPad * pad)
{
  GstCompositor *compositor;

  compositor = GST_COMPOSITOR (element);

  GST_DEBUG_OBJECT (compositor, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (compositor), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}

static gboolean
src_pad_mouse_event (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR_CAST (element);
  GstCompositor *comp = GST_COMPOSITOR (element);
  GstCompositorPad *cpad = GST_COMPOSITOR_PAD (pad);
  GstStructure *st =
      gst_structure_copy (gst_event_get_structure (GST_EVENT_CAST (user_data)));
  gdouble event_x, event_y;
  gint offset_x, offset_y;
  GstVideoRectangle rect;

  gst_structure_get (st, "pointer_x", G_TYPE_DOUBLE, &event_x,
      "pointer_y", G_TYPE_DOUBLE, &event_y, NULL);

  /* Find output rectangle of this pad */
  _mixer_pad_get_output_size (comp, cpad,
      GST_VIDEO_INFO_PAR_N (&vagg->info),
      GST_VIDEO_INFO_PAR_D (&vagg->info),
      &(rect.w), &(rect.h), &offset_x, &offset_y);
  rect.x = cpad->xpos + offset_x;
  rect.y = cpad->ypos + offset_y;

  /* Translate coordinates and send event if it lies in this rectangle */
  if (is_point_contained (rect, event_x, event_y)) {
    GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD_CAST (cpad);
    gdouble w, h, x, y;

    w = (gdouble) GST_VIDEO_INFO_WIDTH (&vpad->info);
    h = (gdouble) GST_VIDEO_INFO_HEIGHT (&vpad->info);
    x = (event_x - (gdouble) rect.x) * (w / (gdouble) rect.w);
    y = (event_y - (gdouble) rect.y) * (h / (gdouble) rect.h);

    gst_structure_set (st, "pointer_x", G_TYPE_DOUBLE, x,
        "pointer_y", G_TYPE_DOUBLE, y, NULL);
    gst_pad_push_event (pad, gst_event_new_navigation (st));
  } else {
    gst_structure_free (st);
  }

  return TRUE;
}

static gboolean
_src_event (GstAggregator * agg, GstEvent * event)
{
  GstNavigationEventType event_type;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
    {
      event_type = gst_navigation_event_get_type (event);
      switch (event_type) {
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
        case GST_NAVIGATION_EVENT_MOUSE_MOVE:
        case GST_NAVIGATION_EVENT_MOUSE_SCROLL:
          gst_element_foreach_sink_pad (GST_ELEMENT_CAST (agg),
              src_pad_mouse_event, event);
          gst_event_unref (event);
          return TRUE;

        default:
          break;
      }
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_event (agg, event);
}

static gboolean
_sink_query (GstAggregator * agg, GstAggregatorPad * bpad, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:{
      GstCaps *caps;
      GstVideoInfo info;
      GstBufferPool *pool;
      guint size;
      GstStructure *structure;

      gst_query_parse_allocation (query, &caps, NULL);

      if (caps == NULL)
        return FALSE;

      if (!gst_video_info_from_caps (&info, caps))
        return FALSE;

      size = GST_VIDEO_INFO_SIZE (&info);

      pool = gst_video_buffer_pool_new ();

      structure = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (structure, caps, size, 0, 0);

      if (!gst_buffer_pool_set_config (pool, structure)) {
        gst_object_unref (pool);
        return FALSE;
      }

      gst_query_add_allocation_pool (query, pool, size, 0, 0);
      gst_object_unref (pool);
      gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

      return TRUE;
    }
    default:
      return GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, bpad, query);
  }
}

static void
gst_compositor_finalize (GObject * object)
{
  GstCompositor *compositor = GST_COMPOSITOR (object);

  if (compositor->blend_runner)
    gst_parallelized_task_runner_free (compositor->blend_runner);
  compositor->blend_runner = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GObject boilerplate */
static void
gst_compositor_class_init (GstCompositorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoAggregatorClass *videoaggregator_class =
      (GstVideoAggregatorClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;

  gobject_class->get_property = gst_compositor_get_property;
  gobject_class->set_property = gst_compositor_set_property;
  gobject_class->finalize = gst_compositor_finalize;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_compositor_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_compositor_release_pad);
  agg_class->sink_query = _sink_query;
  agg_class->src_event = _src_event;
  agg_class->fixate_src_caps = _fixate_caps;
  agg_class->negotiated_src_caps = _negotiated_caps;
  agg_class->stop = GST_DEBUG_FUNCPTR (gst_composior_stop);
  videoaggregator_class->aggregate_frames = gst_compositor_aggregate_frames;

  g_object_class_install_property (gobject_class, PROP_BACKGROUND,
      g_param_spec_enum ("background", "Background", "Background type",
          GST_TYPE_COMPOSITOR_BACKGROUND,
          DEFAULT_BACKGROUND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * compositor:zero-size-is-unscaled:
   *
   * Whether a pad with height or width 0 should be left unscaled
   * in that dimension, or simply not composited in. Setting it to
   * %FALSE might be useful when animating those properties.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_ZERO_SIZE_IS_UNSCALED,
      g_param_spec_boolean ("zero-size-is-unscaled", "Zero size is unscaled",
          "If TRUE, then input video is unscaled in that dimension "
          "if width or height is 0 (for backwards compatibility)",
          DEFAULT_ZERO_SIZE_IS_UNSCALED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * compositor:max-threads:
   *
   * Maximum number of blending/rendering worker threads to spawn (0 = auto)
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_MAX_THREADS,
      g_param_spec_uint ("max-threads", "Max Threads",
          "Maximum number of blending/rendering worker threads to spawn "
          "(0 = auto)", 0, G_MAXINT, DEFAULT_MAX_THREADS,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_factory, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_factory, GST_TYPE_COMPOSITOR_PAD);

  gst_element_class_set_static_metadata (gstelement_class, "Compositor",
      "Filter/Editor/Video/Compositor",
      "Composite multiple video streams", "Wim Taymans <wim@fluendo.com>, "
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  /**
   * compositor:ignore-inactive-pads:
   *
   * Don't wait for inactive pads when live. An inactive pad
   * is a pad that hasn't yet received a buffer, but that has
   * been waited on at least once.
   *
   * The purpose of this property is to avoid aggregating on
   * timeout when new pads are requested in advance of receiving
   * data flow, for example the user may decide to connect it later,
   * but wants to configure it already.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_IGNORE_INACTIVE_PADS, g_param_spec_boolean ("ignore-inactive-pads",
          "Ignore inactive pads",
          "Avoid timing out waiting for inactive pads", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_COMPOSITOR_PAD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_COMPOSITOR_OPERATOR, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_COMPOSITOR_BACKGROUND, 0);
}

static void
gst_compositor_init (GstCompositor * self)
{
  /* initialize variables */
  self->background = DEFAULT_BACKGROUND;
  self->zero_size_is_unscaled = DEFAULT_ZERO_SIZE_IS_UNSCALED;
  self->max_threads = DEFAULT_MAX_THREADS;
}

/* GstChildProxy implementation */
static GObject *
gst_compositor_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstCompositor *compositor = GST_COMPOSITOR (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (compositor);
  obj = g_list_nth_data (GST_ELEMENT_CAST (compositor)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (compositor);

  return obj;
}

static guint
gst_compositor_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstCompositor *compositor = GST_COMPOSITOR (child_proxy);

  GST_OBJECT_LOCK (compositor);
  count = GST_ELEMENT_CAST (compositor)->numsinkpads;
  GST_OBJECT_UNLOCK (compositor);
  GST_INFO_OBJECT (compositor, "Children Count: %d", count);

  return count;
}

static void
gst_compositor_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_compositor_child_proxy_get_child_by_index;
  iface->get_children_count = gst_compositor_child_proxy_get_children_count;
}

/* Element registration */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_compositor_debug, "compositor", 0, "compositor");

  gst_compositor_init_blend ();

  return GST_ELEMENT_REGISTER (compositor, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    compositor,
    "Compositor", plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
