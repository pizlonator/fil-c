/*
 * GStreamer
 * Copyright (C) 2004, 2008 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 * Copyright (C) 2014 Thibault Saunier <tsaunier@gnome.org>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-d3d11compositor
 * @title: d3d11compositor
 *
 * A Direct3D11 based video compositing element.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d11compositor name=c ! d3d11videosink \
 *     videotestsrc ! video/x-raw,width=320,height=240 ! c. \
 *     videotestsrc pattern=ball ! video/x-raw,width=100,height=100 ! c.
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11compositor.h"
#include "gstd3d11pluginutils.h"
#include <string.h>
#include <wrl.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_compositor_debug);
#define GST_CAT_DEFAULT gst_d3d11_compositor_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

typedef enum
{
  GST_D3D11_COMPOSITOR_BACKGROUND_CHECKER,
  GST_D3D11_COMPOSITOR_BACKGROUND_BLACK,
  GST_D3D11_COMPOSITOR_BACKGROUND_WHITE,
  GST_D3D11_COMPOSITOR_BACKGROUND_TRANSPARENT,
} GstD3D11CompositorBackground;

#define GST_TYPE_D3D11_COMPOSITOR_BACKGROUND (gst_d3d11_compositor_background_get_type())
static GType
gst_d3d11_compositor_background_get_type (void)
{
  static GType compositor_background_type = 0;

  static const GEnumValue compositor_background[] = {
    {GST_D3D11_COMPOSITOR_BACKGROUND_CHECKER, "Checker pattern", "checker"},
    {GST_D3D11_COMPOSITOR_BACKGROUND_BLACK, "Black", "black"},
    {GST_D3D11_COMPOSITOR_BACKGROUND_WHITE, "White", "white"},
    {GST_D3D11_COMPOSITOR_BACKGROUND_TRANSPARENT,
        "Transparent Background to enable further compositing", "transparent"},
    {0, nullptr, nullptr},
  };

  if (!compositor_background_type) {
    compositor_background_type =
        g_enum_register_static ("GstD3D11CompositorBackground",
        compositor_background);
  }
  return compositor_background_type;
}

typedef enum
{
  GST_D3D11_COMPOSITOR_OPERATOR_SOURCE,
  GST_D3D11_COMPOSITOR_OPERATOR_OVER,
} GstD3D11CompositorOperator;

/**
 * GstD3D11CompositorOperator:
 *
 * Since: 1.22
 */
#define GST_TYPE_D3D11_COMPOSITOR_OPERATOR (gst_d3d11_compositor_operator_get_type())
static GType
gst_d3d11_compositor_operator_get_type (void)
{
  static GType compositor_operator_type = 0;

  static const GEnumValue compositor_operator[] = {
    {GST_D3D11_COMPOSITOR_OPERATOR_SOURCE, "Source", "source"},
    {GST_D3D11_COMPOSITOR_OPERATOR_OVER, "Over", "over"},
    {0, nullptr, nullptr},
  };

  if (!compositor_operator_type) {
    compositor_operator_type =
        g_enum_register_static ("GstD3D11CompositorOperator",
        compositor_operator);
  }
  return compositor_operator_type;
}

typedef enum
{
  GST_D3D11_COMPOSITOR_SIZING_POLICY_NONE,
  GST_D3D11_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO,
} GstD3D11CompositorSizingPolicy;

#define GST_TYPE_D3D11_COMPOSITOR_SIZING_POLICY (gst_d3d11_compositor_sizing_policy_get_type())
static GType
gst_d3d11_compositor_sizing_policy_get_type (void)
{
  static GType sizing_policy_type = 0;

  static const GEnumValue sizing_polices[] = {
    {GST_D3D11_COMPOSITOR_SIZING_POLICY_NONE,
        "None: Image is scaled to fill configured destination rectangle without "
          "padding or keeping the aspect ratio", "none"},
    {GST_D3D11_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO,
          "Keep Aspect Ratio: Image is scaled to fit destination rectangle "
          "specified by GstD3D11CompositorPad:{xpos, ypos, width, height} "
          "with preserved aspect ratio. Resulting image will be centered in "
          "the destination rectangle with padding if necessary",
        "keep-aspect-ratio"},
    {0, nullptr, nullptr},
  };

  if (!sizing_policy_type) {
    sizing_policy_type =
        g_enum_register_static ("GstD3D11CompositorSizingPolicy",
        sizing_polices);
  }
  return sizing_policy_type;
}

/* *INDENT-OFF* */
static D3D11_RENDER_TARGET_BLEND_DESC blend_templ[] = {
  /* SOURCE */
  {
    TRUE,
    D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
    D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
    D3D11_COLOR_WRITE_ENABLE_ALL
  },
  /* OVER */
  {
    TRUE,
    D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
    D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
    D3D11_COLOR_WRITE_ENABLE_ALL,
  },
};
/* *INDENT-ON* */

struct _GstD3D11CompositorPad
{
  GstVideoAggregatorConvertPad parent;

  GstD3D11Converter *convert;

  gboolean position_updated;
  gboolean alpha_updated;
  gboolean blend_desc_updated;
  gboolean config_updated;
  ID3D11BlendState *blend;

  D3D11_RENDER_TARGET_BLEND_DESC desc;

  /* properties */
  gint xpos;
  gint ypos;
  gint width;
  gint height;
  gdouble alpha;
  GstD3D11CompositorOperator op;
  GstD3D11CompositorSizingPolicy sizing_policy;
  GstVideoGammaMode gamma_mode;
  GstVideoPrimariesMode primaries_mode;
};

typedef struct
{
  ID3D11PixelShader *ps;
  ID3D11VertexShader *vs;
  ID3D11InputLayout *layout;
  ID3D11RasterizerState *rs;
  ID3D11Buffer *vertex_buffer;
  ID3D11Buffer *index_buffer;
  D3D11_VIEWPORT viewport;
} GstD3D11CompositorQuad;

typedef struct
{
  /* [rtv][colors] */
  FLOAT color[4][4];
} GstD3D11CompositorClearColor;

struct _GstD3D11Compositor
{
  GstVideoAggregator parent;

  GstD3D11Device *device;

  GstBuffer *fallback_buf;
  GstCaps *negotiated_caps;

  GstD3D11CompositorQuad *checker_background;
  /* black/white/transparent */
  GstD3D11CompositorClearColor clear_color[3];

  gboolean downstream_supports_d3d11;

  /* properties */
  gint adapter;
  GstD3D11CompositorBackground background;
};

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
  PROP_PAD_GAMMA_MODE,
  PROP_PAD_PRIMARIES_MODE,
};

#define DEFAULT_PAD_XPOS   0
#define DEFAULT_PAD_YPOS   0
#define DEFAULT_PAD_WIDTH  0
#define DEFAULT_PAD_HEIGHT 0
#define DEFAULT_PAD_ALPHA  1.0
#define DEFAULT_PAD_OPERATOR GST_D3D11_COMPOSITOR_OPERATOR_OVER
#define DEFAULT_PAD_SIZING_POLICY GST_D3D11_COMPOSITOR_SIZING_POLICY_NONE
#define DEFAULT_PAD_GAMMA_MODE GST_VIDEO_GAMMA_MODE_NONE
#define DEFAULT_PAD_PRIMARIES_MODE GST_VIDEO_PRIMARIES_MODE_NONE

static void gst_d3d11_compositor_pad_dispose (GObject * object);
static void gst_d3d11_compositor_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_compositor_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean
gst_d3d11_compositor_pad_prepare_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame);
static void gst_d3d11_compositor_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame);

#define gst_d3d11_compositor_pad_parent_class parent_pad_class
G_DEFINE_TYPE (GstD3D11CompositorPad, gst_d3d11_compositor_pad,
    GST_TYPE_VIDEO_AGGREGATOR_PAD);

static void
gst_d3d11_compositor_pad_class_init (GstD3D11CompositorPadClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVideoAggregatorPadClass *vagg_pad_class =
      GST_VIDEO_AGGREGATOR_PAD_CLASS (klass);
  GParamFlags param_flags = (GParamFlags)
      (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS);

  object_class->dispose = gst_d3d11_compositor_pad_dispose;
  object_class->set_property = gst_d3d11_compositor_pad_set_property;
  object_class->get_property = gst_d3d11_compositor_pad_get_property;

  g_object_class_install_property (object_class, PROP_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_WIDTH, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_HEIGHT, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_PAD_ALPHA, param_flags));

  /**
   * GstD3D11CompositorPad:operator:
   *
   * Blending operator to use for blending this pad over the previous ones
   *
   * Since: 1.22
   */
  g_object_class_install_property (object_class, PROP_PAD_OPERATOR,
      g_param_spec_enum ("operator", "Operator",
          "Blending operator to use for blending this pad over the previous ones",
          GST_TYPE_D3D11_COMPOSITOR_OPERATOR, DEFAULT_PAD_OPERATOR,
          param_flags));
  g_object_class_install_property (object_class, PROP_PAD_SIZING_POLICY,
      g_param_spec_enum ("sizing-policy", "Sizing policy",
          "Sizing policy to use for image scaling",
          GST_TYPE_D3D11_COMPOSITOR_SIZING_POLICY, DEFAULT_PAD_SIZING_POLICY,
          param_flags));

  /**
   * GstD3D11CompositorPad:gamma-mode:
   *
   * Gamma conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (object_class, PROP_PAD_GAMMA_MODE,
      g_param_spec_enum ("gamma-mode", "Gamma mode",
          "Gamma conversion mode", GST_TYPE_VIDEO_GAMMA_MODE,
          DEFAULT_PAD_GAMMA_MODE, param_flags));

  /**
   * GstD3D11CompositorPad:primaries-mode:
   *
   * Primaries conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (object_class, PROP_PAD_PRIMARIES_MODE,
      g_param_spec_enum ("primaries-mode", "Primaries Mode",
          "Primaries conversion mode", GST_TYPE_VIDEO_PRIMARIES_MODE,
          DEFAULT_PAD_PRIMARIES_MODE, param_flags));

  vagg_pad_class->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_pad_prepare_frame);
  vagg_pad_class->clean_frame =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_pad_clean_frame);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_COMPOSITOR_OPERATOR,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_COMPOSITOR_SIZING_POLICY,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d11_compositor_pad_init (GstD3D11CompositorPad * pad)
{
  pad->xpos = DEFAULT_PAD_XPOS;
  pad->ypos = DEFAULT_PAD_YPOS;
  pad->width = DEFAULT_PAD_WIDTH;
  pad->height = DEFAULT_PAD_HEIGHT;
  pad->alpha = DEFAULT_PAD_ALPHA;
  pad->op = DEFAULT_PAD_OPERATOR;
  pad->sizing_policy = DEFAULT_PAD_SIZING_POLICY;
  pad->desc = blend_templ[DEFAULT_PAD_OPERATOR];
  pad->gamma_mode = DEFAULT_PAD_GAMMA_MODE;
  pad->primaries_mode = DEFAULT_PAD_PRIMARIES_MODE;
}

static void
gst_d3d11_compositor_pad_dispose (GObject * object)
{
  GstD3D11CompositorPad *self = GST_D3D11_COMPOSITOR_PAD (object);

  gst_clear_object (&self->convert);
  GST_D3D11_CLEAR_COM (self->blend);

  G_OBJECT_CLASS (parent_pad_class)->dispose (object);
}

static void
gst_d3d11_compositor_pad_update_position (GstD3D11CompositorPad * self,
    gint * old, const GValue * value)
{
  gint tmp = g_value_get_int (value);

  if (*old != tmp) {
    *old = tmp;
    self->position_updated = TRUE;
  }
}

static void
gst_d3d11_compositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11CompositorPad *pad = GST_D3D11_COMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      gst_d3d11_compositor_pad_update_position (pad, &pad->xpos, value);
      break;
    case PROP_PAD_YPOS:
      gst_d3d11_compositor_pad_update_position (pad, &pad->ypos, value);
      break;
    case PROP_PAD_WIDTH:
      gst_d3d11_compositor_pad_update_position (pad, &pad->width, value);
      break;
    case PROP_PAD_HEIGHT:
      gst_d3d11_compositor_pad_update_position (pad, &pad->height, value);
      break;
    case PROP_PAD_ALPHA:{
      gdouble alpha = g_value_get_double (value);
      if (pad->alpha != alpha) {
        pad->alpha_updated = TRUE;
        pad->alpha = alpha;
      }
      break;
    }
    case PROP_PAD_OPERATOR:{
      GstD3D11CompositorOperator op =
          (GstD3D11CompositorOperator) g_value_get_enum (value);
      if (op != pad->op) {
        pad->op = op;
        pad->desc = blend_templ[op];
        pad->blend_desc_updated = TRUE;
      }
      break;
    }
    case PROP_PAD_SIZING_POLICY:{
      GstD3D11CompositorSizingPolicy policy =
          (GstD3D11CompositorSizingPolicy) g_value_get_enum (value);
      if (pad->sizing_policy != policy) {
        pad->sizing_policy = policy;
        pad->position_updated = TRUE;
      }
      break;
    }
    case PROP_PAD_GAMMA_MODE:{
      GstVideoGammaMode mode = (GstVideoGammaMode) g_value_get_enum (value);
      if (pad->gamma_mode != mode) {
        pad->gamma_mode = mode;
        pad->config_updated = TRUE;
      }
      break;
    }
    case PROP_PAD_PRIMARIES_MODE:{
      GstVideoPrimariesMode mode =
          (GstVideoPrimariesMode) g_value_get_enum (value);
      if (pad->primaries_mode != mode) {
        pad->primaries_mode = mode;
        pad->config_updated = TRUE;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_compositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11CompositorPad *pad = GST_D3D11_COMPOSITOR_PAD (object);

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
    case PROP_PAD_GAMMA_MODE:
      g_value_set_enum (value, pad->gamma_mode);
      break;
    case PROP_PAD_PRIMARIES_MODE:
      g_value_set_enum (value, pad->primaries_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_compositor_pad_get_output_size (GstD3D11CompositorPad * comp_pad,
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

  pad_width =
      comp_pad->width <=
      0 ? GST_VIDEO_INFO_WIDTH (&vagg_pad->info) : comp_pad->width;
  pad_height =
      comp_pad->height <=
      0 ? GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) : comp_pad->height;

  if (pad_width == 0 || pad_height == 0)
    return;

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
          GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
          GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d)) {
    GST_WARNING_OBJECT (comp_pad, "Cannot calculate display aspect ratio");
    return;
  }

  GST_TRACE_OBJECT (comp_pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)",
      pad_width, pad_height, dar_n, dar_d,
      GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d);

  switch (comp_pad->sizing_policy) {
    case GST_D3D11_COMPOSITOR_SIZING_POLICY_NONE:
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
    case GST_D3D11_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO:{
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

static gboolean
gst_d3d11_compositor_pad_check_frame_obscured (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  GstD3D11CompositorPad *cpad = GST_D3D11_COMPOSITOR_PAD (pad);
  gint width, height;
  GstVideoInfo *info = &vagg->info;
  /* The rectangle representing this frame, clamped to the video's boundaries.
   * Due to the clamping, this is different from the frame width/height above. */
  GstVideoRectangle frame_rect;
  gint x_offset, y_offset;

  /* There's three types of width/height here:
   * 1. GST_VIDEO_FRAME_WIDTH/HEIGHT:
   *     The frame width/height (same as pad->info.height/width;
   *     see gst_video_frame_map())
   * 2. cpad->width/height:
   *     The optional pad property for scaling the frame (if zero, the video is
   *     left unscaled)
   */

  if (cpad->alpha == 0)
    return TRUE;

  gst_d3d11_compositor_pad_get_output_size (cpad, GST_VIDEO_INFO_PAR_N (info),
      GST_VIDEO_INFO_PAR_D (info), &width, &height, &x_offset, &y_offset);

  frame_rect = clamp_rectangle (cpad->xpos + x_offset, cpad->ypos + y_offset,
      width, height, GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

  if (frame_rect.w == 0 || frame_rect.h == 0) {
    GST_DEBUG_OBJECT (pad, "Resulting frame is zero-width or zero-height "
        "(w: %i, h: %i), skipping", frame_rect.w, frame_rect.h);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_d3d11_compositor_pad_prepare_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  /* Skip this frame */
  if (gst_d3d11_compositor_pad_check_frame_obscured (pad, vagg))
    return TRUE;

  /* don't map/upload now, it will happen in converter object.
   * Just mark this frame is preparted instead */
  prepared_frame->buffer = buffer;

  return TRUE;
}

static void
gst_d3d11_compositor_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  memset (prepared_frame, 0, sizeof (GstVideoFrame));
}

static gboolean
gst_d3d11_compositor_pad_setup_converter (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  GstD3D11CompositorPad *cpad = GST_D3D11_COMPOSITOR_PAD (pad);
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (vagg);
  gint width, height;
  GstVideoInfo *info = &vagg->info;
  GstVideoRectangle frame_rect;
  gboolean is_first = FALSE;
  gboolean output_has_alpha_comp = FALSE;
  gint x_offset, y_offset;
#ifndef GST_DISABLE_GST_DEBUG
  guint zorder = 0;
#endif
  static const D3D11_RENDER_TARGET_BLEND_DESC blend_over_no_alpha = {
    TRUE,
    D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR, D3D11_BLEND_OP_ADD,
    D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR, D3D11_BLEND_OP_ADD,
    D3D11_COLOR_WRITE_ENABLE_ALL,
  };

  if (GST_VIDEO_INFO_HAS_ALPHA (info) ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_BGRx ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_RGBx) {
    output_has_alpha_comp = TRUE;
  }

  if (cpad->config_updated) {
    gst_clear_object (&cpad->convert);
    cpad->config_updated = FALSE;
  }

  if (!cpad->convert) {
    GstStructure *config = gst_structure_new ("converter-config",
        /* XXX: Always use shader, to workaround buggy blending behavior of
         * vendor implemented converter. Need investigation */
        GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
        GST_D3D11_CONVERTER_BACKEND_SHADER,
        GST_D3D11_CONVERTER_OPT_GAMMA_MODE,
        GST_TYPE_VIDEO_GAMMA_MODE, cpad->gamma_mode,
        GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE,
        GST_TYPE_VIDEO_PRIMARIES_MODE, cpad->primaries_mode, nullptr);

    cpad->convert = gst_d3d11_converter_new (self->device, &pad->info, info,
        config);
    if (!cpad->convert) {
      GST_ERROR_OBJECT (pad, "Couldn't create converter");
      return FALSE;
    }

    is_first = TRUE;
  }

  if (cpad->alpha_updated || is_first) {
    if (output_has_alpha_comp) {
      g_object_set (cpad->convert, "alpha", cpad->alpha, nullptr);
    } else {
      gfloat blend_factor = cpad->alpha;

      g_object_set (cpad->convert,
          "blend-factor-red", blend_factor,
          "blend-factor-green", blend_factor,
          "blend-factor-blue", blend_factor,
          "blend-factor-alpha", blend_factor, nullptr);
    }

    cpad->alpha_updated = FALSE;
  }

  if (!cpad->blend || cpad->blend_desc_updated || is_first) {
    HRESULT hr;
    D3D11_BLEND_DESC desc = { 0, };
    ID3D11BlendState *blend = nullptr;
    ID3D11Device *device_handle =
        gst_d3d11_device_get_device_handle (self->device);
    gfloat blend_factor = 1.0f;

    GST_D3D11_CLEAR_COM (cpad->blend);

    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0] = cpad->desc;
    if (!output_has_alpha_comp &&
        cpad->op == GST_D3D11_COMPOSITOR_OPERATOR_OVER) {
      desc.RenderTarget[0] = blend_over_no_alpha;
      blend_factor = cpad->alpha;
    }

    hr = device_handle->CreateBlendState (&desc, &blend);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (pad, "Couldn't create blend staten, hr: 0x%x",
          (guint) hr);
      return FALSE;
    }

    cpad->blend = blend;
    g_object_set (cpad->convert, "blend-state", blend,
        "blend-factor-red", blend_factor,
        "blend-factor-green", blend_factor,
        "blend-factor-blue", blend_factor,
        "blend-factor-alpha", blend_factor, nullptr);

    cpad->blend_desc_updated = FALSE;
  }

  if (!is_first && !cpad->position_updated)
    return TRUE;

  gst_d3d11_compositor_pad_get_output_size (cpad, GST_VIDEO_INFO_PAR_N (info),
      GST_VIDEO_INFO_PAR_D (info), &width, &height, &x_offset, &y_offset);

  frame_rect = clamp_rectangle (cpad->xpos + x_offset, cpad->ypos + y_offset,
      width, height, GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

#ifndef GST_DISABLE_GST_DEBUG
  g_object_get (pad, "zorder", &zorder, nullptr);

  GST_LOG_OBJECT (pad, "Update position, pad-xpos %d, pad-ypos %d, "
      "pad-zorder %d, pad-width %d, pad-height %d, in-resolution %dx%d, "
      "out-resoution %dx%d, dst-{x,y,width,height} %d-%d-%d-%d",
      cpad->xpos, cpad->ypos, zorder, cpad->width, cpad->height,
      GST_VIDEO_INFO_WIDTH (&pad->info), GST_VIDEO_INFO_HEIGHT (&pad->info),
      GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info),
      frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h);
#endif

  cpad->position_updated = FALSE;

  g_object_set (cpad->convert, "dest-x", frame_rect.x,
      "dest-y", frame_rect.y, "dest-width", frame_rect.w,
      "dest-height", frame_rect.h, nullptr);

  return TRUE;
}

static GstStaticCaps sink_pad_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SINK_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE (GST_D3D11_SINK_FORMATS));

/* formats we can output without conversion.
 * Excludes 10/12 bits planar YUV (needs bitshift) and
 * AYUV/AYUV64 (d3d11 runtime does not understand the ayuv order) */
#define COMPOSITOR_SRC_FORMATS \
    "{ RGBA64_LE, RGB10A2_LE, BGRA, RGBA, BGRx, RGBx, VUYA, NV12, NV21, " \
    "P010_10LE, P012_LE, P016_LE, I420, YV12, Y42B, Y444, Y444_16LE, " \
    "GRAY8, GRAY16_LE }"

static GstStaticCaps src_pad_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, COMPOSITOR_SRC_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE (COMPOSITOR_SRC_FORMATS));

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_BACKGROUND,
  PROP_IGNORE_INACTIVE_PADS,
};

#define DEFAULT_ADAPTER -1
#define DEFAULT_BACKGROUND GST_D3D11_COMPOSITOR_BACKGROUND_CHECKER

static void gst_d3d11_compositor_child_proxy_init (gpointer g_iface,
    gpointer iface_data);
static void gst_d3d11_compositor_dispose (GObject * object);
static void gst_d3d11_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstPad *gst_d3d11_compositor_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_d3d11_compositor_release_pad (GstElement * element,
    GstPad * pad);
static void gst_d3d11_compositor_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_d3d11_compositor_start (GstAggregator * agg);
static gboolean gst_d3d11_compositor_stop (GstAggregator * agg);
static gboolean gst_d3d11_compositor_sink_query (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * query);
static gboolean gst_d3d11_compositor_src_query (GstAggregator * agg,
    GstQuery * query);
static GstCaps *gst_d3d11_compositor_fixate_src_caps (GstAggregator * agg,
    GstCaps * caps);
static gboolean gst_d3d11_compositor_negotiated_src_caps (GstAggregator * agg,
    GstCaps * caps);
static gboolean
gst_d3d11_compositor_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query);
static gboolean gst_d3d11_compositor_decide_allocation (GstAggregator * agg,
    GstQuery * query);
static GstFlowReturn
gst_d3d11_compositor_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuf);
static GstFlowReturn
gst_d3d11_compositor_create_output_buffer (GstVideoAggregator * vagg,
    GstBuffer ** outbuffer);
static void gst_d3d11_compositor_quad_free (GstD3D11CompositorQuad * quad);

#define gst_d3d11_compositor_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstD3D11Compositor, gst_d3d11_compositor,
    GST_TYPE_VIDEO_AGGREGATOR, G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_d3d11_compositor_child_proxy_init));

static void
gst_d3d11_compositor_class_init (GstD3D11CompositorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *agg_class = GST_AGGREGATOR_CLASS (klass);
  GstVideoAggregatorClass *vagg_class = GST_VIDEO_AGGREGATOR_CLASS (klass);
  GstCaps *caps;

  object_class->dispose = gst_d3d11_compositor_dispose;
  object_class->set_property = gst_d3d11_compositor_set_property;
  object_class->get_property = gst_d3d11_compositor_get_property;

  g_object_class_install_property (object_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "Adapter index for creating device (-1 for default)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_BACKGROUND,
      g_param_spec_enum ("background", "Background", "Background type",
          GST_TYPE_D3D11_COMPOSITOR_BACKGROUND,
          DEFAULT_BACKGROUND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Compositor:ignore-inactive-pads:
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
   * Since: 1.24
   */
  g_object_class_install_property (object_class,
      PROP_IGNORE_INACTIVE_PADS, g_param_spec_boolean ("ignore-inactive-pads",
          "Ignore inactive pads",
          "Avoid timing out waiting for inactive pads", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_request_new_pad);
  element_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_release_pad);
  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_set_context);

  agg_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_compositor_start);
  agg_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_compositor_stop);
  agg_class->sink_query = GST_DEBUG_FUNCPTR (gst_d3d11_compositor_sink_query);
  agg_class->src_query = GST_DEBUG_FUNCPTR (gst_d3d11_compositor_src_query);
  agg_class->fixate_src_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_fixate_src_caps);
  agg_class->negotiated_src_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_negotiated_src_caps);
  agg_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_propose_allocation);
  agg_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_decide_allocation);

  vagg_class->aggregate_frames =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_aggregate_frames);
  vagg_class->create_output_buffer =
      GST_DEBUG_FUNCPTR (gst_d3d11_compositor_create_output_buffer);

  caps = gst_d3d11_get_updated_template_caps (&sink_pad_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new_with_gtype ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
          caps, GST_TYPE_D3D11_COMPOSITOR_PAD));
  gst_caps_unref (caps);

  caps = gst_d3d11_get_updated_template_caps (&src_pad_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new_with_gtype ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          caps, GST_TYPE_AGGREGATOR_PAD));
  gst_caps_unref (caps);

  gst_element_class_set_static_metadata (element_class, "Direct3D11 Compositor",
      "Filter/Editor/Video/Compositor", "A Direct3D11 compositor",
      "Seungha Yang <seungha@centricular.com>");

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_COMPOSITOR_BACKGROUND,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_COMPOSITOR_PAD,
      (GstPluginAPIFlags) 0);

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_compositor_debug,
      "d3d11compositor", 0, "d3d11compositor element");
}

static void
gst_d3d11_compositor_init (GstD3D11Compositor * self)
{
  self->adapter = DEFAULT_ADAPTER;
  self->background = DEFAULT_BACKGROUND;
}

static void
gst_d3d11_compositor_dispose (GObject * object)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (object);

  gst_clear_object (&self->device);
  gst_clear_buffer (&self->fallback_buf);
  g_clear_pointer (&self->checker_background, gst_d3d11_compositor_quad_free);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      self->adapter = g_value_get_int (value);
      break;
    case PROP_BACKGROUND:
      self->background =
          (GstD3D11CompositorBackground) g_value_get_enum (value);
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

static void
gst_d3d11_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, self->adapter);
      break;
    case PROP_BACKGROUND:
      g_value_set_enum (value, self->background);
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

static GObject *
gst_d3d11_compositor_child_proxy_get_child_by_index (GstChildProxy * proxy,
    guint index)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (proxy);
  GObject *obj = nullptr;

  GST_OBJECT_LOCK (self);
  obj = (GObject *) g_list_nth_data (GST_ELEMENT_CAST (self)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (self);

  return obj;
}

static guint
gst_d3d11_compositor_child_proxy_get_children_count (GstChildProxy * proxy)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (proxy);
  guint count = 0;

  GST_OBJECT_LOCK (self);
  count = GST_ELEMENT_CAST (self)->numsinkpads;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_d3d11_compositor_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = (GstChildProxyInterface *) g_iface;

  iface->get_child_by_index =
      gst_d3d11_compositor_child_proxy_get_child_by_index;
  iface->get_children_count =
      gst_d3d11_compositor_child_proxy_get_children_count;
}

static GstPad *
gst_d3d11_compositor_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad;

  pad = GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, name, caps);

  if (!pad) {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return nullptr;
  }

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_DEBUG_OBJECT (element, "Created new pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  return pad;
}

static gboolean
gst_d3d11_compositor_pad_clear_resource (GstD3D11Compositor * self,
    GstD3D11CompositorPad * cpad, gpointer user_data)
{
  gst_clear_object (&cpad->convert);
  GST_D3D11_CLEAR_COM (cpad->blend);

  return TRUE;
}

static void
gst_d3d11_compositor_release_pad (GstElement * element, GstPad * pad)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (element);

  GST_DEBUG_OBJECT (self, "Releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}

static void
gst_d3d11_compositor_set_context (GstElement * element, GstContext * context)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (element);

  gst_d3d11_handle_set_context (element, context, self->adapter, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d11_compositor_start (GstAggregator * agg)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);

  if (!gst_d3d11_ensure_element_data (GST_ELEMENT_CAST (self),
          self->adapter, &self->device)) {
    GST_ERROR_OBJECT (self, "Failed to get D3D11 device");
    return FALSE;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->start (agg);
}

static gboolean
gst_d3d11_compositor_stop (GstAggregator * agg)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);

  g_clear_pointer (&self->checker_background, gst_d3d11_compositor_quad_free);
  gst_clear_object (&self->device);
  gst_clear_caps (&self->negotiated_caps);

  return GST_AGGREGATOR_CLASS (parent_class)->stop (agg);
}

static GstCaps *
gst_d3d11_compositor_sink_getcaps (GstPad * pad, GstCaps * filter)
{
  GstCaps *sinkcaps;
  GstCaps *template_caps;
  GstCaps *filtered_caps;
  GstCaps *returned_caps;

  template_caps = gst_pad_get_pad_template_caps (pad);

  sinkcaps = gst_pad_get_current_caps (pad);
  if (sinkcaps == nullptr) {
    sinkcaps = gst_caps_ref (template_caps);
  } else {
    sinkcaps = gst_caps_merge (sinkcaps, gst_caps_ref (template_caps));
  }

  if (filter) {
    filtered_caps = gst_caps_intersect (sinkcaps, filter);
    gst_caps_unref (sinkcaps);
  } else {
    filtered_caps = sinkcaps;   /* pass ownership */
  }

  returned_caps = gst_caps_intersect (filtered_caps, template_caps);

  gst_caps_unref (template_caps);
  gst_caps_unref (filtered_caps);

  GST_DEBUG_OBJECT (pad, "returning %" GST_PTR_FORMAT, returned_caps);

  return returned_caps;
}

static gboolean
gst_d3d11_compositor_sink_acceptcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret;
  GstCaps *template_caps;

  GST_DEBUG_OBJECT (pad, "try accept caps of %" GST_PTR_FORMAT, caps);

  template_caps = gst_pad_get_pad_template_caps (pad);
  template_caps = gst_caps_make_writable (template_caps);

  ret = gst_caps_can_intersect (caps, template_caps);
  GST_DEBUG_OBJECT (pad, "%saccepted caps %" GST_PTR_FORMAT,
      (ret ? "" : "not "), caps);
  gst_caps_unref (template_caps);

  return ret;
}

static gboolean
gst_d3d11_compositor_sink_query (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * query)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT (agg), query,
              self->device)) {
        return TRUE;
      }
      break;
    case GST_QUERY_CAPS:{
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_d3d11_compositor_sink_getcaps (GST_PAD (pad), filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      return TRUE;
    }
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *caps;
      gboolean ret;

      gst_query_parse_accept_caps (query, &caps);
      ret = gst_d3d11_compositor_sink_acceptcaps (GST_PAD (pad), caps);
      gst_query_set_accept_caps_result (query, ret);
      return TRUE;
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, pad, query);
}

static gboolean
gst_d3d11_compositor_src_query (GstAggregator * agg, GstQuery * query)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT (agg), query,
              self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
}

static GstCaps *
gst_d3d11_compositor_fixate_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GList *l;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  gint par_n, par_d;
  gdouble best_fps = 0.;
  GstCaps *ret = nullptr;
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
    GstVideoAggregatorPad *vaggpad = GST_VIDEO_AGGREGATOR_PAD (l->data);
    GstD3D11CompositorPad *cpad = GST_D3D11_COMPOSITOR_PAD (vaggpad);
    gint this_width, this_height;
    gint width, height;
    gint fps_n, fps_d;
    gdouble cur_fps;
    gint x_offset;
    gint y_offset;

    fps_n = GST_VIDEO_INFO_FPS_N (&vaggpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&vaggpad->info);
    gst_d3d11_compositor_pad_get_output_size (cpad,
        par_n, par_d, &width, &height, &x_offset, &y_offset);

    if (width == 0 || height == 0)
      continue;

    /* {x,y}_offset represent padding size of each top and left area.
     * To calculate total resolution, count bottom and right padding area
     * as well here */
    this_width = width + MAX (cpad->xpos + 2 * x_offset, 0);
    this_height = height + MAX (cpad->ypos + 2 * y_offset, 0);

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

  GST_LOG_OBJECT (agg, "Fixated caps %" GST_PTR_FORMAT, ret);

  return ret;
}

static void
convert_info_gray_to_yuv (const GstVideoInfo * gray, GstVideoInfo * yuv)
{
  GstVideoInfo tmp;

  if (GST_VIDEO_INFO_IS_YUV (gray)) {
    *yuv = *gray;
    return;
  }

  if (gray->finfo->depth[0] == 8) {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444, gray->width, gray->height);
  } else {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444_16LE, gray->width, gray->height);
  }

  tmp.colorimetry.range = gray->colorimetry.range;
  if (tmp.colorimetry.range == GST_VIDEO_COLOR_RANGE_UNKNOWN)
    tmp.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

  tmp.colorimetry.primaries = gray->colorimetry.primaries;
  if (tmp.colorimetry.primaries == GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
    tmp.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;

  tmp.colorimetry.transfer = gray->colorimetry.transfer;
  if (tmp.colorimetry.transfer == GST_VIDEO_TRANSFER_UNKNOWN)
    tmp.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;

  tmp.colorimetry.matrix = gray->colorimetry.matrix;
  if (tmp.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN)
    tmp.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;

  *yuv = tmp;
}

static void
gst_d3d11_compositor_calculate_background_color (GstD3D11Compositor * self,
    const GstVideoInfo * info)
{
  GstD3D11ColorMatrix clear_color_matrix;
  gdouble rgb[3];
  gdouble converted[3];
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);

  if (GST_VIDEO_INFO_IS_RGB (info)) {
    GstVideoInfo rgb_info = *info;
    rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

    gst_d3d11_color_range_adjust_matrix_unorm (&rgb_info, info,
        &clear_color_matrix);
  } else {
    GstVideoInfo rgb_info;
    GstVideoInfo yuv_info;

    gst_video_info_set_format (&rgb_info, GST_VIDEO_FORMAT_RGBA64_LE,
        info->width, info->height);
    convert_info_gray_to_yuv (info, &yuv_info);

    if (yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
      GST_WARNING_OBJECT (self, "Invalid matrix is detected");
      yuv_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }

    gst_d3d11_rgb_to_yuv_matrix_unorm (&rgb_info,
        &yuv_info, &clear_color_matrix);
  }

  /* Calculate black and white color values */
  for (guint i = 0; i < 2; i++) {
    GstD3D11CompositorClearColor *clear_color = &self->clear_color[i];
    rgb[0] = rgb[1] = rgb[2] = (gdouble) i;

    for (guint j = 0; j < 3; j++) {
      converted[j] = 0;
      for (guint k = 0; k < 3; k++) {
        converted[j] += clear_color_matrix.matrix[j][k] * rgb[k];
      }
      converted[j] += clear_color_matrix.offset[j];
      converted[j] = CLAMP (converted[j],
          clear_color_matrix.min[j], clear_color_matrix.max[j]);
    }

    GST_DEBUG_OBJECT (self, "Calculated background color RGB: %f, %f, %f",
        converted[0], converted[1], converted[2]);

    if (GST_VIDEO_INFO_IS_RGB (info) || GST_VIDEO_INFO_IS_GRAY (info)) {
      for (guint j = 0; j < 3; j++)
        clear_color->color[0][j] = converted[j];
      clear_color->color[0][3] = 1.0;
    } else {
      switch (format) {
        case GST_VIDEO_FORMAT_VUYA:
          clear_color->color[0][0] = converted[2];
          clear_color->color[0][1] = converted[1];
          clear_color->color[0][2] = converted[0];
          clear_color->color[0][3] = 1.0;
          break;
        case GST_VIDEO_FORMAT_NV12:
        case GST_VIDEO_FORMAT_NV21:
        case GST_VIDEO_FORMAT_P010_10LE:
        case GST_VIDEO_FORMAT_P012_LE:
        case GST_VIDEO_FORMAT_P016_LE:
          clear_color->color[0][0] = converted[0];
          clear_color->color[0][1] = 0;
          clear_color->color[0][2] = 0;
          clear_color->color[0][3] = 1.0;
          if (format == GST_VIDEO_FORMAT_NV21) {
            clear_color->color[1][0] = converted[2];
            clear_color->color[1][1] = converted[1];
          } else {
            clear_color->color[1][0] = converted[1];
            clear_color->color[1][1] = converted[2];
          }
          clear_color->color[1][2] = 0;
          clear_color->color[1][3] = 1.0;
          break;
        case GST_VIDEO_FORMAT_I420:
        case GST_VIDEO_FORMAT_YV12:
        case GST_VIDEO_FORMAT_I420_10LE:
        case GST_VIDEO_FORMAT_I420_12LE:
        case GST_VIDEO_FORMAT_Y42B:
        case GST_VIDEO_FORMAT_I422_10LE:
        case GST_VIDEO_FORMAT_I422_12LE:
        case GST_VIDEO_FORMAT_Y444:
        case GST_VIDEO_FORMAT_Y444_10LE:
        case GST_VIDEO_FORMAT_Y444_12LE:
        case GST_VIDEO_FORMAT_Y444_16LE:
          clear_color->color[0][0] = converted[0];
          clear_color->color[0][1] = 0;
          clear_color->color[0][2] = 0;
          clear_color->color[0][3] = 1.0;
          if (format == GST_VIDEO_FORMAT_YV12) {
            clear_color->color[1][0] = converted[2];
            clear_color->color[2][0] = converted[1];
          } else {
            clear_color->color[1][0] = converted[1];
            clear_color->color[2][0] = converted[2];
          }
          clear_color->color[1][1] = 0;
          clear_color->color[1][2] = 0;
          clear_color->color[1][3] = 1.0;
          clear_color->color[2][1] = 0;
          clear_color->color[2][2] = 0;
          clear_color->color[2][3] = 1.0;
          break;
        default:
          g_assert_not_reached ();
          break;
      }
    }
  }

  self->clear_color[2] = self->clear_color[0];
  self->clear_color[2].color[0][3] = 0.0;
}

static gboolean
gst_d3d11_compositor_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);
  GstCapsFeatures *features;
  GstVideoInfo info;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Failed to convert caps to info");
    return FALSE;
  }

  if (self->negotiated_caps && gst_caps_is_equal (self->negotiated_caps, caps)) {
    GST_DEBUG_OBJECT (self, "Negotiated caps is not changed");
    goto done;
  }

  features = gst_caps_get_features (caps, 0);
  if (features
      && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    GST_DEBUG_OBJECT (self, "Negotiated with D3D11 memory caps");
    self->downstream_supports_d3d11 = TRUE;
  } else {
    GST_DEBUG_OBJECT (self, "Negotiated with system memory caps");
    self->downstream_supports_d3d11 = FALSE;
  }

  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
      (GstElementForeachPadFunc) gst_d3d11_compositor_pad_clear_resource,
      nullptr);

  gst_clear_buffer (&self->fallback_buf);
  g_clear_pointer (&self->checker_background, gst_d3d11_compositor_quad_free);

  gst_d3d11_compositor_calculate_background_color (self, &info);

  if (!self->downstream_supports_d3d11) {
    GstD3D11AllocationParams *d3d11_params;
    GstBufferPool *pool;
    GstFlowReturn flow_ret;

    d3d11_params = gst_d3d11_allocation_params_new (self->device,
        &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);

    pool = gst_d3d11_buffer_pool_new_with_options (self->device,
        caps, d3d11_params, 0, 0);
    gst_d3d11_allocation_params_free (d3d11_params);

    if (!pool) {
      GST_ERROR_OBJECT (self, "Failed to create pool");
      return FALSE;
    }

    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to set active");
      gst_object_unref (pool);
      return FALSE;
    }

    flow_ret = gst_buffer_pool_acquire_buffer (pool, &self->fallback_buf,
        nullptr);
    if (flow_ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Failed to acquire buffer");
      gst_object_unref (pool);
      return FALSE;
    }

    gst_buffer_pool_set_active (pool, FALSE);
    gst_object_unref (pool);
  }

  gst_caps_replace (&self->negotiated_caps, caps);

done:
  return GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps);
}

static gboolean
gst_d3d11_compositor_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (caps == nullptr)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstCapsFeatures *features;
    GstStructure *config;
    gboolean is_d3d11 = FALSE;

    features = gst_caps_get_features (caps, 0);
    if (features
        && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
      GST_DEBUG_OBJECT (pad, "upstream support d3d11 memory");
      pool = gst_d3d11_buffer_pool_new (self->device);
      is_d3d11 = TRUE;
    } else {
      pool = gst_video_buffer_pool_new ();
    }

    if (!pool) {
      GST_ERROR_OBJECT (self, "Failed to create buffer pool");
      return FALSE;
    }

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    size = GST_VIDEO_INFO_SIZE (&info);
    if (is_d3d11) {
      GstD3D11AllocationParams *d3d11_params;

      d3d11_params =
          gst_d3d11_allocation_params_new (self->device,
          &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_SHADER_RESOURCE,
          0);

      gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
      gst_d3d11_allocation_params_free (d3d11_params);
    } else {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    gst_buffer_pool_config_set_params (config, caps, (guint) size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (pool, "Couldn't set config");
      gst_object_unref (pool);

      return FALSE;
    }

    /* d3d11 buffer pool will update buffer size based on allocated texture,
     * get size from config again */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config,
        nullptr, &size, nullptr, nullptr);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, nullptr);

  return TRUE;
}

static gboolean
gst_d3d11_compositor_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);
  GstCaps *caps;
  GstBufferPool *pool = nullptr;
  guint n, size, min, max;
  GstVideoInfo info;
  GstStructure *config;
  gboolean use_d3d11_pool;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_DEBUG_OBJECT (self, "No output caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Invalid caps");
    return FALSE;
  }

  use_d3d11_pool = self->downstream_supports_d3d11;

  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  /* create our own pool */
  if (pool && use_d3d11_pool) {
    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      GST_DEBUG_OBJECT (self,
          "Downstream pool is not d3d11, will create new one");
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != self->device) {
        GST_DEBUG_OBJECT (self, "Different device, will create new one");
        gst_clear_object (&pool);
      }
    }
  }

  size = (guint) info.size;

  if (!pool) {
    if (use_d3d11_pool)
      pool = gst_d3d11_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();

    min = 0;
    max = 0;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (use_d3d11_pool) {
    GstD3D11AllocationParams *d3d11_params;

    d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
    if (!d3d11_params) {
      d3d11_params = gst_d3d11_allocation_params_new (self->device,
          &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT,
          D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);
    } else {
      gst_d3d11_allocation_params_set_bind_flags (d3d11_params,
          D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    }

    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  }

  gst_buffer_pool_set_config (pool, config);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}

typedef struct
{
  struct
  {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct
  {
    FLOAT u;
    FLOAT v;
  } texture;
} VertexData;

static GstD3D11CompositorQuad *
gst_d3d11_compositor_create_checker_quad (GstD3D11Compositor * self,
    const GstVideoInfo * info)
{
  GstD3D11CompositorQuad *quad = nullptr;
  VertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
  ID3D11Device *device_handle;
  D3D11_BUFFER_DESC buffer_desc;
  D3D11_SUBRESOURCE_DATA subresource;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11RasterizerState > rs;
  HRESULT hr;

  device_handle = gst_d3d11_device_get_device_handle (self->device);

  if (GST_VIDEO_INFO_IS_RGB (info)) {
    hr = gst_d3d11_get_pixel_shader_checker_rgb (self->device, &ps);
  } else if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_VUYA) {
    hr = gst_d3d11_get_pixel_shader_checker_vuya (self->device, &ps);
  } else {
    hr = gst_d3d11_get_pixel_shader_checker_luma (self->device, &ps);
  }

  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't setup pixel shader");
    return nullptr;
  }

  hr = gst_d3d11_get_vertex_shader_pos (self->device, &vs, &layout);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't setup vertex shader");
    return nullptr;
  }

  hr = gst_d3d11_device_get_rasterizer (self->device, &rs);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't setup rasterizer state");
    return nullptr;
  }

  /* bottom left */
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.u = 0.0f;
  vertex_data[0].texture.v = 1.0f;

  /* top left */
  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.u = 0.0f;
  vertex_data[1].texture.v = 0.0f;

  /* top right */
  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.u = 1.0f;
  vertex_data[2].texture.v = 0.0f;

  /* bottom right */
  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.u = 1.0f;
  vertex_data[3].texture.v = 1.0f;

  memset (&subresource, 0, sizeof (D3D11_SUBRESOURCE_DATA));
  memset (&buffer_desc, 0, sizeof (D3D11_BUFFER_DESC));

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (VertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = vertex_data;
  subresource.SysMemPitch = sizeof (VertexData) * 4;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &vertex_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "Couldn't create vertex buffer, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = indices;
  subresource.SysMemPitch = sizeof (WORD) * 6;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &index_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "Couldn't create index buffer, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  quad = g_new0 (GstD3D11CompositorQuad, 1);
  quad->ps = ps.Detach ();
  quad->vs = vs.Detach ();
  quad->rs = rs.Detach ();
  quad->layout = layout.Detach ();

  quad->vertex_buffer = vertex_buffer.Detach ();
  quad->index_buffer = index_buffer.Detach ();

  quad->viewport.TopLeftX = 0;
  quad->viewport.TopLeftY = 0;
  quad->viewport.Width = GST_VIDEO_INFO_WIDTH (info);
  quad->viewport.Height = GST_VIDEO_INFO_HEIGHT (info);
  quad->viewport.MinDepth = 0.0f;
  quad->viewport.MaxDepth = 1.0f;

  return quad;
}

static void
gst_d3d11_compositor_quad_free (GstD3D11CompositorQuad * quad)
{
  if (!quad)
    return;

  GST_D3D11_CLEAR_COM (quad->ps);
  GST_D3D11_CLEAR_COM (quad->vs);
  GST_D3D11_CLEAR_COM (quad->layout);
  GST_D3D11_CLEAR_COM (quad->rs);
  GST_D3D11_CLEAR_COM (quad->vertex_buffer);
  GST_D3D11_CLEAR_COM (quad->index_buffer);

  g_free (quad);
}

static gboolean
gst_d3d11_compositor_draw_background_checker (GstD3D11Compositor * self,
    ID3D11RenderTargetView * rtv)
{
  ID3D11DeviceContext *context =
      gst_d3d11_device_get_device_context_handle (self->device);
  UINT offsets = 0;
  UINT strides = sizeof (VertexData);
  GstD3D11CompositorQuad *quad;

  if (!self->checker_background) {
    GstVideoInfo *info = &GST_VIDEO_AGGREGATOR_CAST (self)->info;

    self->checker_background =
        gst_d3d11_compositor_create_checker_quad (self, info);
    if (!self->checker_background)
      return FALSE;
  }

  quad = self->checker_background;
  context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->IASetInputLayout (quad->layout);
  context->IASetVertexBuffers (0, 1, &quad->vertex_buffer, &strides, &offsets);
  context->IASetIndexBuffer (quad->index_buffer, DXGI_FORMAT_R16_UINT, 0);
  context->VSSetShader (quad->vs, nullptr, 0);
  context->PSSetShader (quad->ps, nullptr, 0);
  context->RSSetViewports (1, &quad->viewport);
  context->RSSetState (quad->rs);
  context->OMSetRenderTargets (1, &rtv, nullptr);
  context->OMSetBlendState (nullptr, nullptr, 0xffffffff);
  context->DrawIndexed (6, 0, 0);
  context->OMSetRenderTargets (0, nullptr, nullptr);

  return TRUE;
}

/* Must be called with d3d11 device lock */
static gboolean
gst_d3d11_compositor_draw_background (GstD3D11Compositor * self,
    ID3D11RenderTargetView * rtv[GST_VIDEO_MAX_PLANES], guint num_rtv)
{
  ID3D11DeviceContext *context =
      gst_d3d11_device_get_device_context_handle (self->device);
  GstD3D11CompositorClearColor *color = &self->clear_color[0];

  if (self->background == GST_D3D11_COMPOSITOR_BACKGROUND_CHECKER) {
    if (!gst_d3d11_compositor_draw_background_checker (self, rtv[0]))
      return FALSE;

    /* clear U and V components if needed */
    for (guint i = 1; i < num_rtv; i++)
      context->ClearRenderTargetView (rtv[i], color->color[i]);

    return TRUE;
  }

  switch (self->background) {
    case GST_D3D11_COMPOSITOR_BACKGROUND_BLACK:
      color = &self->clear_color[0];
      break;
    case GST_D3D11_COMPOSITOR_BACKGROUND_WHITE:
      color = &self->clear_color[1];
      break;
    case GST_D3D11_COMPOSITOR_BACKGROUND_TRANSPARENT:
      color = &self->clear_color[2];
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  for (guint i = 0; i < num_rtv; i++)
    context->ClearRenderTargetView (rtv[i], color->color[i]);

  return TRUE;
}

static GstFlowReturn
gst_d3d11_compositor_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuf)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (vagg);
  GList *iter;
  GstBuffer *target_buf = outbuf;
  GstFlowReturn ret = GST_FLOW_OK;
  ID3D11RenderTargetView *rtv[GST_VIDEO_MAX_PLANES] = { nullptr, };
  GstVideoFrame target_frame;
  guint num_rtv = GST_VIDEO_INFO_N_PLANES (&vagg->info);
  GstD3D11DeviceLockGuard lk (self->device);

  if (!self->downstream_supports_d3d11)
    target_buf = self->fallback_buf;

  if (!gst_video_frame_map (&target_frame, &vagg->info, target_buf,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map render target frame");
    return GST_FLOW_ERROR;
  }

  if (!gst_d3d11_buffer_get_render_target_view (target_buf, rtv)) {
    GST_ERROR_OBJECT (self, "RTV is unavailable");
    gst_video_frame_unmap (&target_frame);
    return GST_FLOW_ERROR;
  }

  if (!gst_d3d11_compositor_draw_background (self, rtv, num_rtv)) {
    GST_ERROR_OBJECT (self, "Couldn't draw background");
    gst_video_frame_unmap (&target_frame);
    return GST_FLOW_ERROR;
  }

  gst_video_frame_unmap (&target_frame);

  GST_OBJECT_LOCK (self);
  for (iter = GST_ELEMENT (vagg)->sinkpads; iter; iter = g_list_next (iter)) {
    GstVideoAggregatorPad *pad = GST_VIDEO_AGGREGATOR_PAD (iter->data);
    GstD3D11CompositorPad *cpad = GST_D3D11_COMPOSITOR_PAD (pad);
    GstVideoFrame *prepared_frame =
        gst_video_aggregator_pad_get_prepared_frame (pad);
    gint x, y, w, h;
    GstVideoCropMeta *crop_meta;

    if (!prepared_frame)
      continue;

    if (!gst_d3d11_compositor_pad_setup_converter (pad, vagg)) {
      GST_ERROR_OBJECT (self, "Couldn't setup converter");
      ret = GST_FLOW_ERROR;
      break;
    }

    crop_meta = gst_buffer_get_video_crop_meta (prepared_frame->buffer);
    if (crop_meta) {
      x = crop_meta->x;
      y = crop_meta->y;
      w = crop_meta->width;
      h = crop_meta->height;
    } else {
      x = y = 0;
      w = pad->info.width;
      h = pad->info.height;
    }

    g_object_set (cpad->convert, "src-x", x, "src-y", y, "src-width", w,
        "src-height", h, nullptr);

    if (!gst_d3d11_converter_convert_buffer_unlocked (cpad->convert,
            prepared_frame->buffer, target_buf)) {
      GST_ERROR_OBJECT (self, "Couldn't convert frame");
      ret = GST_FLOW_ERROR;
      break;
    }
  }
  GST_OBJECT_UNLOCK (self);

  if (ret != GST_FLOW_OK)
    return ret;

  if (!self->downstream_supports_d3d11) {
    if (!gst_d3d11_buffer_copy_into (outbuf, self->fallback_buf, &vagg->info)) {
      GST_ERROR_OBJECT (self, "Couldn't copy input buffer to fallback buffer");
      return GST_FLOW_ERROR;
    }
  }

  return GST_FLOW_OK;
}

typedef struct
{
  /* without holding ref */
  GstD3D11Device *other_device;
  gboolean have_same_device;
} DeviceCheckData;

static gboolean
gst_d3d11_compositor_check_device_update (GstElement * agg,
    GstVideoAggregatorPad * vpad, DeviceCheckData * data)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (agg);
  GstBuffer *buf;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  gboolean update_device = FALSE;

  buf = gst_video_aggregator_pad_get_current_buffer (vpad);
  if (!buf)
    return TRUE;

  /* Ignore gap buffer */
  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP) ||
      gst_buffer_get_size (buf) == 0) {
    return TRUE;
  }

  mem = gst_buffer_peek_memory (buf, 0);
  if (!gst_is_d3d11_memory (mem))
    return TRUE;

  dmem = GST_D3D11_MEMORY_CAST (mem);

  /* We can use existing device */
  if (dmem->device == self->device) {
    data->have_same_device = TRUE;
    return FALSE;
  }

  if (self->adapter < 0) {
    update_device = TRUE;
  } else {
    guint adapter = 0;

    g_object_get (dmem->device, "adapter", &adapter, nullptr);
    /* The same GPU as what user wanted, update */
    if (adapter == (guint) self->adapter)
      update_device = TRUE;
  }

  if (!update_device)
    return TRUE;

  data->other_device = dmem->device;

  /* Keep iterate since there might be one buffer which holds the same device
   * as ours */
  return TRUE;
}

static GstFlowReturn
gst_d3d11_compositor_create_output_buffer (GstVideoAggregator * vagg,
    GstBuffer ** outbuffer)
{
  GstD3D11Compositor *self = GST_D3D11_COMPOSITOR (vagg);
  DeviceCheckData data;

  /* Check whether there is at least one sinkpad which holds d3d11 buffer
   * with compatible device, and if not, update our device */
  data.other_device = nullptr;
  data.have_same_device = FALSE;

  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg),
      (GstElementForeachPadFunc) gst_d3d11_compositor_check_device_update,
      &data);

  if (data.have_same_device || !data.other_device) {
    return
        GST_VIDEO_AGGREGATOR_CLASS (parent_class)->create_output_buffer (vagg,
        outbuffer);
  }

  /* Clear all device dependent resources */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg),
      (GstElementForeachPadFunc) gst_d3d11_compositor_pad_clear_resource,
      nullptr);

  gst_clear_buffer (&self->fallback_buf);
  g_clear_pointer (&self->checker_background, gst_d3d11_compositor_quad_free);

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, data.other_device);
  gst_object_unref (self->device);
  self->device = (GstD3D11Device *) gst_object_ref (data.other_device);

  /* We cannot call gst_aggregator_negotiate() here, since GstVideoAggregator
   * is holding GST_VIDEO_AGGREGATOR_LOCK() already.
   * Mark reconfigure and do reconfigure later */
  gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg));

  return GST_AGGREGATOR_FLOW_NEED_DATA;
}
