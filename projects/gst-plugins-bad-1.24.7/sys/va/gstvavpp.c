/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vapostproc
 * @title: vapostproc
 * @short_description: A VA-API base video postprocessing filter
 *
 * vapostproc applies different video filters to VA surfaces. These
 * filters vary depending on the installed and chosen
 * [VA-API](https://01.org/linuxmedia/vaapi) driver, but usually
 * resizing and color conversion are available.
 *
 * The generated surfaces can be mapped onto main memory as video
 * frames.
 *
 * Use gst-inspect-1.0 to introspect the available capabilities of the
 * driver's post-processor entry point.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! "video/x-raw,format=(string)NV12" ! vapostproc ! autovideosink
 * ```
 *
 * Cropping is supported via buffers' crop meta. It's only done if the
 * postproccessor is not in passthrough mode or if downstream doesn't
 * support the crop meta API.
 *
 * ### Cropping example
 * ```
 * gst-launch-1.0 videotestsrc ! "video/x-raw,format=(string)NV12" ! videocrop bottom=50 left=100 ! vapostproc ! autovideosink
 * ```
 *
 * If the VA driver support color balance filter, with controls such
 * as hue, brightness, contrast, etc., those controls are exposed both
 * as element properties and through the #GstColorBalance interface.
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvavpp.h"

#include <gst/va/gstva.h>
#include <gst/video/video.h>
#include <va/va_drmcommon.h>
#include <gst/va/gstvavideoformat.h>

#include "gstvabasetransform.h"
#include "gstvacaps.h"
#include "gstvadisplay_priv.h"
#include "gstvafilter.h"
#include "gstvapluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_vpp_debug);
#define GST_CAT_DEFAULT gst_va_vpp_debug

#define GST_VA_VPP(obj)           ((GstVaVpp *) obj)
#define GST_VA_VPP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaVppClass))
#define GST_VA_VPP_CLASS(klass)    ((GstVaVppClass *) klass)

#define SWAP_INT(a, b) G_STMT_START { \
  gint __tmp = a; \
  a = b; \
  b = __tmp; \
} G_STMT_END

typedef struct _GstVaVpp GstVaVpp;
typedef struct _GstVaVppClass GstVaVppClass;

struct _GstVaVppClass
{
  /* GstVideoFilter overlaps functionality */
  GstVaBaseTransformClass parent_class;
};

struct _GstVaVpp
{
  GstVaBaseTransform parent;

  gboolean rebuild_filters;
  guint op_flags;

  /* filters */
  float denoise;
  float sharpen;
  float skintone;
  float brightness;
  float contrast;
  float hue;
  float saturation;
  gboolean auto_contrast;
  gboolean auto_brightness;
  gboolean auto_saturation;
  GstVideoOrientationMethod direction;
  GstVideoOrientationMethod prev_direction;
  GstVideoOrientationMethod tag_direction;
  gboolean add_borders;
  gint borders_h;
  gint borders_w;
  guint32 scale_method;

  gboolean hdr_mapping;
  gboolean has_hdr_meta;
  VAHdrMetaDataHDR10 hdr_meta;

  gboolean pseudo_passthrough;

  GList *channels;
};

static GstElementClass *parent_class = NULL;

struct CData
{
  gchar *render_device_path;
  gchar *description;
};

enum
{
  PROP_DISABLE_PASSTHROUGH = GST_VA_FILTER_PROP_LAST + 1,
  PROP_ADD_BORDERS,
  PROP_SCALE_METHOD,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES - GST_VA_FILTER_PROP_LAST];

#define PROPERTIES(idx) properties[idx - GST_VA_FILTER_PROP_LAST]

/* convertions that disable passthrough */
enum
{
  VPP_CONVERT_SIZE = 1 << 0,
  VPP_CONVERT_FORMAT = 1 << 1,
  VPP_CONVERT_FILTERS = 1 << 2,
  VPP_CONVERT_DIRECTION = 1 << 3,
  VPP_CONVERT_FEATURE = 1 << 4,
  VPP_CONVERT_CROP = 1 << 5,
  VPP_CONVERT_DUMMY = 1 << 6,
};

/* *INDENT-OFF* */
static const gchar *caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12, I420, YV12, YUY2, RGBA, BGRA, P010_10LE, ARGB, ABGR }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ VUYA, GRAY8, NV12, NV21, YUY2, UYVY, YV12, "
        "I420, P010_10LE, RGBA, BGRA, ARGB, ABGR  }");
/* *INDENT-ON* */

#define META_TAG_COLORSPACE meta_tag_colorspace_quark
static GQuark meta_tag_colorspace_quark;
#define META_TAG_SIZE meta_tag_size_quark
static GQuark meta_tag_size_quark;
#define META_TAG_ORIENTATION meta_tag_orientation_quark
static GQuark meta_tag_orientation_quark;
#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

static void gst_va_vpp_colorbalance_init (gpointer iface, gpointer data);
static void gst_va_vpp_rebuild_filters (GstVaVpp * self);

static void
gst_va_vpp_dispose (GObject * object)
{
  GstVaVpp *self = GST_VA_VPP (object);

  if (self->channels)
    g_list_free_full (g_steal_pointer (&self->channels), g_object_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_vpp_update_passthrough (GstVaVpp * self, gboolean reconf)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (self);
  gboolean old, new;

  old = gst_base_transform_is_passthrough (trans);

  GST_OBJECT_LOCK (self);
  new = (self->op_flags == 0);
  GST_OBJECT_UNLOCK (self);

  if (old != new) {
    GST_INFO_OBJECT (self, "%s passthrough", new ? "enabling" : "disabling");
    if (reconf)
      gst_base_transform_reconfigure_src (trans);
    gst_base_transform_set_passthrough (trans, new);
  }
}

static void
_update_properties_unlocked (GstVaVpp * self)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);

  if (!btrans->filter)
    return;

  if ((self->direction != GST_VIDEO_ORIENTATION_AUTO
          && self->direction != self->prev_direction)
      || (self->direction == GST_VIDEO_ORIENTATION_AUTO
          && self->tag_direction != self->prev_direction)) {

    GstVideoOrientationMethod direction =
        (self->direction == GST_VIDEO_ORIENTATION_AUTO) ?
        self->tag_direction : self->direction;

    if (!gst_va_filter_set_orientation (btrans->filter, direction)) {
      if (self->direction == GST_VIDEO_ORIENTATION_AUTO)
        self->tag_direction = self->prev_direction;
      else
        self->direction = self->prev_direction;

      self->op_flags &= ~VPP_CONVERT_DIRECTION;

      /* FIXME: unlocked bus warning message */
      GST_WARNING_OBJECT (self,
          "Driver cannot set resquested orientation. Setting it back.");
    } else {
      self->prev_direction = direction;

      self->op_flags |= VPP_CONVERT_DIRECTION;

      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (self));
    }
  } else {
    self->op_flags &= ~VPP_CONVERT_DIRECTION;
  }

  if (!gst_va_filter_set_scale_method (btrans->filter, self->scale_method))
    GST_WARNING_OBJECT (self, "could not set the filter scale method.");
}

static void
gst_va_vpp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaVpp *self = GST_VA_VPP (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case GST_VA_FILTER_PROP_DENOISE:
      self->denoise = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_SHARPEN:
      self->sharpen = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_SKINTONE:
      if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
        self->skintone = (float) g_value_get_boolean (value);
      else
        self->skintone = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_VIDEO_DIR:{
      GstVideoOrientationMethod direction = g_value_get_enum (value);
      self->prev_direction = (direction == GST_VIDEO_ORIENTATION_AUTO) ?
          self->tag_direction : self->direction;
      self->direction = direction;
      break;
    }
    case GST_VA_FILTER_PROP_HUE:
      self->hue = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_SATURATION:
      self->saturation = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_BRIGHTNESS:
      self->brightness = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_CONTRAST:
      self->contrast = g_value_get_float (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_AUTO_SATURATION:
      self->auto_saturation = g_value_get_boolean (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_AUTO_BRIGHTNESS:
      self->auto_brightness = g_value_get_boolean (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_AUTO_CONTRAST:
      self->auto_contrast = g_value_get_boolean (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case GST_VA_FILTER_PROP_HDR:
      self->hdr_mapping = g_value_get_boolean (value);
      g_atomic_int_set (&self->rebuild_filters, TRUE);
      break;
    case PROP_DISABLE_PASSTHROUGH:{
      gboolean disable_passthrough = g_value_get_boolean (value);
      if (disable_passthrough)
        self->op_flags |= VPP_CONVERT_DUMMY;
      else
        self->op_flags &= ~VPP_CONVERT_DUMMY;
      break;
    }
    case PROP_ADD_BORDERS:
      self->add_borders = g_value_get_boolean (value);
      break;
    case PROP_SCALE_METHOD:
      self->scale_method = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  _update_properties_unlocked (self);
  GST_OBJECT_UNLOCK (object);

  gst_va_vpp_update_passthrough (self, FALSE);
}

static void
gst_va_vpp_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVaVpp *self = GST_VA_VPP (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case GST_VA_FILTER_PROP_DENOISE:
      g_value_set_float (value, self->denoise);
      break;
    case GST_VA_FILTER_PROP_SHARPEN:
      g_value_set_float (value, self->sharpen);
      break;
    case GST_VA_FILTER_PROP_SKINTONE:
      if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
        g_value_set_boolean (value, self->skintone > 0);
      else
        g_value_set_float (value, self->skintone);
      break;
    case GST_VA_FILTER_PROP_VIDEO_DIR:
      g_value_set_enum (value, self->direction);
      break;
    case GST_VA_FILTER_PROP_HUE:
      g_value_set_float (value, self->hue);
      break;
    case GST_VA_FILTER_PROP_SATURATION:
      g_value_set_float (value, self->saturation);
      break;
    case GST_VA_FILTER_PROP_BRIGHTNESS:
      g_value_set_float (value, self->brightness);
      break;
    case GST_VA_FILTER_PROP_CONTRAST:
      g_value_set_float (value, self->contrast);
      break;
    case GST_VA_FILTER_PROP_AUTO_SATURATION:
      g_value_set_boolean (value, self->auto_saturation);
      break;
    case GST_VA_FILTER_PROP_AUTO_BRIGHTNESS:
      g_value_set_boolean (value, self->auto_brightness);
      break;
    case GST_VA_FILTER_PROP_AUTO_CONTRAST:
      g_value_set_boolean (value, self->auto_contrast);
      break;
    case GST_VA_FILTER_PROP_HDR:
      g_value_set_boolean (value, self->hdr_mapping);
      break;
    case PROP_DISABLE_PASSTHROUGH:
      g_value_set_boolean (value, (self->op_flags & VPP_CONVERT_DUMMY));
      break;
    case PROP_ADD_BORDERS:
      g_value_set_boolean (value, self->add_borders);
      break;
    case PROP_SCALE_METHOD:
      g_value_set_enum (value, self->scale_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static gboolean
gst_va_vpp_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  /* if we are not passthrough, we can handle crop meta */
  if (decide_query)
    gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
      decide_query, query);
}

static void
gst_va_vpp_update_properties (GstVaBaseTransform * btrans)
{
  GstVaVpp *self = GST_VA_VPP (btrans);

  gst_va_vpp_rebuild_filters (self);
  _update_properties_unlocked (self);
}

static void
_set_hdr_metadata (GstVaVpp * self, GstCaps * caps)
{
  GstVideoMasteringDisplayInfo mdinfo;
  GstVideoContentLightLevel llevel;

  self->has_hdr_meta = FALSE;

  if (gst_video_mastering_display_info_from_caps (&mdinfo, caps)) {
    self->hdr_meta.display_primaries_x[0] = mdinfo.display_primaries[1].x;
    self->hdr_meta.display_primaries_x[1] = mdinfo.display_primaries[2].x;
    self->hdr_meta.display_primaries_x[2] = mdinfo.display_primaries[0].x;

    self->hdr_meta.display_primaries_y[0] = mdinfo.display_primaries[1].y;
    self->hdr_meta.display_primaries_y[1] = mdinfo.display_primaries[2].y;
    self->hdr_meta.display_primaries_y[2] = mdinfo.display_primaries[0].y;

    self->hdr_meta.white_point_x = mdinfo.white_point.x;
    self->hdr_meta.white_point_y = mdinfo.white_point.y;

    self->hdr_meta.max_display_mastering_luminance =
        mdinfo.max_display_mastering_luminance;
    self->hdr_meta.min_display_mastering_luminance =
        mdinfo.min_display_mastering_luminance;

    self->has_hdr_meta = TRUE;
  }


  if (gst_video_content_light_level_from_caps (&llevel, caps)) {
    self->hdr_meta.max_content_light_level = llevel.max_content_light_level;
    self->hdr_meta.max_pic_average_light_level =
        llevel.max_frame_average_light_level;

    self->has_hdr_meta = TRUE;
  };

  /* rebuild filters only if hdr mapping is enabled */
  g_atomic_int_set (&self->rebuild_filters, self->hdr_mapping);
}

static gboolean
gst_va_vpp_set_info (GstVaBaseTransform * btrans, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstVaVpp *self = GST_VA_VPP (btrans);
  GstCapsFeatures *infeat, *outfeat;
  gint from_dar_n, from_dar_d, to_dar_n, to_dar_d;

  if (GST_VIDEO_INFO_INTERLACE_MODE (in_info) !=
      GST_VIDEO_INFO_INTERLACE_MODE (out_info)) {
    GST_ERROR_OBJECT (self, "input and output formats do not match");
    return FALSE;
  }

  /* calculate possible borders if display-aspect-ratio change */
  {
    if (!gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (in_info),
            GST_VIDEO_INFO_HEIGHT (in_info), GST_VIDEO_INFO_PAR_N (in_info),
            GST_VIDEO_INFO_PAR_D (in_info), &from_dar_n, &from_dar_d)) {
      from_dar_n = from_dar_d = -1;
    }

    if (!gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (out_info),
            GST_VIDEO_INFO_HEIGHT (out_info), GST_VIDEO_INFO_PAR_N (out_info),
            GST_VIDEO_INFO_PAR_D (out_info), &to_dar_n, &to_dar_d)) {
      to_dar_n = to_dar_d = -1;
    }

    /* if video-orientation changes consider it for borders */
    switch (gst_va_filter_get_orientation (btrans->filter)) {
      case GST_VIDEO_ORIENTATION_90R:
      case GST_VIDEO_ORIENTATION_90L:
      case GST_VIDEO_ORIENTATION_UL_LR:
      case GST_VIDEO_ORIENTATION_UR_LL:
        SWAP_INT (from_dar_n, from_dar_d);
        break;
      default:
        break;
    }

    self->borders_h = self->borders_w = 0;
    if (to_dar_n != from_dar_n || to_dar_d != from_dar_d) {
      if (self->add_borders) {
        gint n, d, to_h, to_w;

        if (from_dar_n != -1 && from_dar_d != -1
            && gst_util_fraction_multiply (from_dar_n, from_dar_d,
                out_info->par_d, out_info->par_n, &n, &d)) {
          to_h = gst_util_uint64_scale_int (out_info->width, d, n);
          if (to_h <= out_info->height) {
            self->borders_h = out_info->height - to_h;
            self->borders_w = 0;
          } else {
            to_w = gst_util_uint64_scale_int (out_info->height, n, d);
            g_assert (to_w <= out_info->width);
            self->borders_h = 0;
            self->borders_w = out_info->width - to_w;
          }
        } else {
          GST_WARNING_OBJECT (self, "Can't calculate borders");
        }
      } else {
        GST_WARNING_OBJECT (self, "Can't keep DAR!");
      }
    }
  }

  if (gst_video_info_is_equal (in_info, out_info)) {
    self->op_flags &= ~VPP_CONVERT_FORMAT & ~VPP_CONVERT_SIZE;
  } else {
    if ((GST_VIDEO_INFO_FORMAT (in_info) != GST_VIDEO_INFO_FORMAT (out_info))
        || !gst_video_colorimetry_is_equivalent (&GST_VIDEO_INFO_COLORIMETRY
            (in_info), GST_VIDEO_INFO_COMP_DEPTH (in_info, 0),
            &GST_VIDEO_INFO_COLORIMETRY (out_info),
            GST_VIDEO_INFO_COMP_DEPTH (out_info, 0)))
      self->op_flags |= VPP_CONVERT_FORMAT;
    else
      self->op_flags &= ~VPP_CONVERT_FORMAT;

    if (GST_VIDEO_INFO_WIDTH (in_info) != GST_VIDEO_INFO_WIDTH (out_info)
        || GST_VIDEO_INFO_HEIGHT (in_info) != GST_VIDEO_INFO_HEIGHT (out_info)
        || self->borders_h > 0 || self->borders_w > 0)
      self->op_flags |= VPP_CONVERT_SIZE;
    else
      self->op_flags &= ~VPP_CONVERT_SIZE;
  }

  infeat = gst_caps_get_features (incaps, 0);
  outfeat = gst_caps_get_features (outcaps, 0);
  if (!gst_caps_features_is_equal (infeat, outfeat))
    self->op_flags |= VPP_CONVERT_FEATURE;
  else
    self->op_flags &= ~VPP_CONVERT_FEATURE;

  if (gst_va_filter_set_video_info (btrans->filter, in_info, out_info)) {
    _set_hdr_metadata (self, incaps);
    gst_va_vpp_update_passthrough (self, FALSE);
    return TRUE;
  }

  return FALSE;
}

static inline gboolean
_get_filter_value (GstVaVpp * self, VAProcFilterType type, gfloat * value)
{
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (self);
  switch (type) {
    case VAProcFilterNoiseReduction:
      *value = self->denoise;
      break;
    case VAProcFilterSharpening:
      *value = self->sharpen;
      break;
    case VAProcFilterSkinToneEnhancement:
      *value = self->skintone;
      break;
    default:
      ret = FALSE;
      break;
  }
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static inline gboolean
_add_filter_buffer (GstVaVpp * self, VAProcFilterType type,
    const VAProcFilterCap * cap)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  VAProcFilterParameterBuffer param;
  gfloat value = 0;

  if (!_get_filter_value (self, type, &value))
    return FALSE;
  if (value == cap->range.default_value)
    return FALSE;

  /* *INDENT-OFF* */
  param = (VAProcFilterParameterBuffer) {
    .type = type,
    .value = value,
  };
  /* *INDENT-ON* */

  return gst_va_filter_add_filter_buffer (btrans->filter, &param,
      sizeof (param), 1);
}

static inline gboolean
_get_filter_cb_value (GstVaVpp * self, VAProcColorBalanceType type,
    gfloat * value)
{
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (self);
  switch (type) {
    case VAProcColorBalanceHue:
      *value = self->hue;
      break;
    case VAProcColorBalanceSaturation:
      *value = self->saturation;
      break;
    case VAProcColorBalanceBrightness:
      *value = self->brightness;
      break;
    case VAProcColorBalanceContrast:
      *value = self->contrast;
      break;
    case VAProcColorBalanceAutoSaturation:
      *value = self->auto_saturation;
      break;
    case VAProcColorBalanceAutoBrightness:
      *value = self->auto_brightness;
      break;
    case VAProcColorBalanceAutoContrast:
      *value = self->auto_contrast;
      break;
    default:
      ret = FALSE;
      break;
  }
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static inline gboolean
_add_filter_cb_buffer (GstVaVpp * self,
    const VAProcFilterCapColorBalance * caps, guint num_caps)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  VAProcFilterParameterBufferColorBalance param[VAProcColorBalanceCount] =
      { 0, };
  gfloat value;
  guint i, c = 0;

  value = 0;
  for (i = 0; i < num_caps && i < VAProcColorBalanceCount; i++) {
    if (!_get_filter_cb_value (self, caps[i].type, &value))
      continue;
    if (value == caps[i].range.default_value)
      continue;

    /* *INDENT-OFF* */
    param[c++] = (VAProcFilterParameterBufferColorBalance) {
      .type = VAProcFilterColorBalance,
      .attrib = caps[i].type,
      .value = value,
    };
    /* *INDENT-ON* */
  }

  if (c > 0) {
    return gst_va_filter_add_filter_buffer (btrans->filter, param,
        sizeof (*param), c);
  }
  return FALSE;
}

static inline gboolean
_add_filter_hdr_buffer (GstVaVpp * self,
    const VAProcFilterCapHighDynamicRange * caps)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  /* *INDENT-OFF* */
  VAProcFilterParameterBufferHDRToneMapping params = {
    .type = VAProcFilterHighDynamicRangeToneMapping,
    .data = {
      .metadata_type = VAProcHighDynamicRangeMetadataHDR10,
      .metadata = &self->hdr_meta,
      .metadata_size = sizeof (self->hdr_meta),
    },
  };
  /* *INDENT-ON* */

  /* if not has hdr meta, it may try later again */
  if (!(self->has_hdr_meta && self->hdr_mapping))
    return FALSE;

  if (!(caps && caps->metadata_type == VAProcHighDynamicRangeMetadataHDR10
          && (caps->caps_flag & VA_TONE_MAPPING_HDR_TO_SDR)))
    goto bail;

  if (self->op_flags & VPP_CONVERT_FORMAT) {
    GST_WARNING_OBJECT (self, "Cannot apply HDR with color conversion");
    goto bail;
  }

  return gst_va_filter_add_filter_buffer (btrans->filter, &params,
      sizeof (params), 1);

bail:
  self->hdr_mapping = FALSE;
  g_object_notify (G_OBJECT (self), "hdr-tone-mapping");
  return FALSE;
}

static void
_build_filters (GstVaVpp * self)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  static const VAProcFilterType filter_types[] = { VAProcFilterNoiseReduction,
    VAProcFilterSharpening, VAProcFilterSkinToneEnhancement,
    VAProcFilterColorBalance, VAProcFilterHighDynamicRangeToneMapping,
  };
  guint i, num_caps;
  gboolean apply = FALSE;

  for (i = 0; i < G_N_ELEMENTS (filter_types); i++) {
    const gpointer caps = gst_va_filter_get_filter_caps (btrans->filter,
        filter_types[i], &num_caps);
    if (!caps)
      continue;

    switch (filter_types[i]) {
      case VAProcFilterNoiseReduction:
        apply |= _add_filter_buffer (self, filter_types[i], caps);
        break;
      case VAProcFilterSharpening:
        apply |= _add_filter_buffer (self, filter_types[i], caps);
        break;
      case VAProcFilterSkinToneEnhancement:
        apply |= _add_filter_buffer (self, filter_types[i], caps);
        break;
      case VAProcFilterColorBalance:
        apply |= _add_filter_cb_buffer (self, caps, num_caps);
        break;
      case VAProcFilterHighDynamicRangeToneMapping:
        apply |= _add_filter_hdr_buffer (self, caps);
      default:
        break;
    }
  }

  GST_OBJECT_LOCK (self);
  if (apply)
    self->op_flags |= VPP_CONVERT_FILTERS;
  else
    self->op_flags &= ~VPP_CONVERT_FILTERS;
  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_vpp_rebuild_filters (GstVaVpp * self)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);

  if (!g_atomic_int_get (&self->rebuild_filters))
    return;

  gst_va_filter_drop_filter_buffers (btrans->filter);
  _build_filters (self);
  g_atomic_int_set (&self->rebuild_filters, FALSE);
}

static void
gst_va_vpp_before_transform (GstBaseTransform * trans, GstBuffer * inbuf)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  GstClockTime ts, stream_time;
  gboolean is_passthrough;

  ts = GST_BUFFER_TIMESTAMP (inbuf);
  stream_time =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, ts);

  GST_TRACE_OBJECT (self, "sync to %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (self), stream_time);

  gst_va_vpp_rebuild_filters (self);
  gst_va_vpp_update_passthrough (self, TRUE);

  /* cropping is only enabled if vapostproc is not in passthrough */
  is_passthrough = gst_base_transform_is_passthrough (trans);
  GST_OBJECT_LOCK (self);
  if (!is_passthrough && gst_buffer_get_video_crop_meta (inbuf)) {
    self->op_flags |= VPP_CONVERT_CROP;
  } else {
    self->op_flags &= ~VPP_CONVERT_CROP;
  }
  gst_va_filter_enable_cropping (btrans->filter,
      (self->op_flags & VPP_CONVERT_CROP) == VPP_CONVERT_CROP);
  GST_OBJECT_UNLOCK (self);
}

static GstFlowReturn
gst_va_vpp_prepare_output_buffer (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer ** outbuf)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);

  if (((self->op_flags & VPP_CONVERT_FEATURE) == self->op_flags)
      && gst_caps_is_vamemory (btrans->in_caps)
      && gst_caps_is_raw (btrans->out_caps)) {
    self->pseudo_passthrough = TRUE;
    *outbuf = inbuf;
    return GST_FLOW_OK;
  }

  self->pseudo_passthrough = FALSE;
  return GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      inbuf, outbuf);
}

static GstFlowReturn
gst_va_vpp_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  GstBuffer *buf = NULL;
  GstFlowReturn res = GST_FLOW_OK;
  GstVaSample src, dst;

  if (G_UNLIKELY (!btrans->negotiated))
    goto unknown_format;

  res = gst_va_base_transform_import_buffer (btrans, inbuf, &buf);
  if (res != GST_FLOW_OK)
    return res;

  if (self->pseudo_passthrough && (inbuf == buf))
    goto bail;

  /* *INDENT-OFF* */
  src = (GstVaSample) {
    .buffer = buf,
    .flags = gst_va_buffer_get_surface_flags (buf, &btrans->in_info),
  };

  dst = (GstVaSample) {
    .buffer = outbuf,
    .borders_h = self->borders_h,
    .borders_w = self->borders_w,
    .flags = gst_va_buffer_get_surface_flags (outbuf, &btrans->out_info),
  };
  /* *INDENT-ON* */

  if (!gst_va_filter_process (btrans->filter, &src, &dst)) {
    gst_buffer_set_flags (outbuf, GST_BUFFER_FLAG_CORRUPTED);
    res = GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

bail:
  gst_buffer_unref (buf);

  return res;

  /* ERRORS */
unknown_format:
  {
    GST_ELEMENT_ERROR (self, CORE, NOT_IMPLEMENTED, (NULL), ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static gboolean
gst_va_vpp_transform_meta (GstBaseTransform * trans, GstBuffer * inbuf,
    GstMeta * meta, GstBuffer * outbuf)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags)
    return TRUE;

  /* don't copy colorspace/size/orientation specific metadata */
  if ((self->op_flags & VPP_CONVERT_FORMAT) == VPP_CONVERT_FORMAT
      && gst_meta_api_type_has_tag (info->api, META_TAG_COLORSPACE))
    return FALSE;
  else if ((self->op_flags & (VPP_CONVERT_SIZE | VPP_CONVERT_CROP)) != 0
      && gst_meta_api_type_has_tag (info->api, META_TAG_SIZE))
    return FALSE;
  else if ((self->op_flags & VPP_CONVERT_DIRECTION) == VPP_CONVERT_DIRECTION
      && gst_meta_api_type_has_tag (info->api, META_TAG_ORIENTATION))
    return FALSE;
  else if (gst_meta_api_type_has_tag (info->api, META_TAG_VIDEO))
    return TRUE;

  return FALSE;
}

/* In structures with supported caps features it's:
 * + Rangified resolution size.
 * + Rangified "pixel-aspect-ratio" if present.
 * + Removed "format", "colorimetry", "chroma-site"
 *
 * Structures with unsupported caps features are copied as-is.
 */
static GstCaps *
gst_va_vpp_caps_remove_fields (GstCaps * caps)
{
  GstCaps *ret;
  GstStructure *structure;
  GstCapsFeatures *features;
  gint i, j, n, m;

  ret = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    structure = gst_caps_get_structure (caps, i);
    features = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (ret, structure, features))
      continue;

    structure = gst_structure_copy (structure);

    m = gst_caps_features_get_size (features);
    for (j = 0; j < m; j++) {
      const gchar *feature = gst_caps_features_get_nth (features, j);

      if (g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY) == 0
          || g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_DMABUF) == 0
          || g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_VA) == 0) {

        /* rangify frame size */
        gst_structure_set (structure, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

        /* if pixel aspect ratio, make a range of it */
        if (gst_structure_has_field (structure, "pixel-aspect-ratio")) {
          gst_structure_set (structure, "pixel-aspect-ratio",
              GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
        }

        /* remove format-related fields */
        gst_structure_remove_fields (structure, "format", "drm-format",
            "colorimetry", "chroma-site", NULL);

        break;
      }
    }

    gst_caps_append_structure_full (ret, structure,
        gst_caps_features_copy (features));
  }

  return ret;
}

/* Returns all structures in @caps without @feature_name but now with
 * @feature_name */
static GstCaps *
gst_va_vpp_complete_caps_features (const GstCaps * caps,
    const gchar * feature_name)
{
  guint i, j, m, n;
  GstCaps *tmp;

  tmp = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstCapsFeatures *features, *orig_features;
    GstStructure *s = gst_caps_get_structure (caps, i);
    gboolean contained = FALSE;

    orig_features = gst_caps_get_features (caps, i);
    features = gst_caps_features_new (feature_name, NULL);

    m = gst_caps_features_get_size (orig_features);
    for (j = 0; j < m; j++) {
      const gchar *feature = gst_caps_features_get_nth (orig_features, j);

      /* if we already have the features */
      if (gst_caps_features_contains (features, feature)) {
        contained = TRUE;
        break;
      }
    }

    if (!contained && !gst_caps_is_subset_structure_full (tmp, s, features))
      gst_caps_append_structure_full (tmp, gst_structure_copy (s), features);
    else
      gst_caps_features_free (features);
  }

  return tmp;
}

static GstCaps *
gst_va_vpp_transform_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  GstCaps *ret, *tmp, *filter_caps;

  GST_DEBUG_OBJECT (self,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  filter_caps = gst_va_base_transform_get_filter_caps (btrans);
  if (filter_caps && !gst_caps_can_intersect (caps, filter_caps)) {
    ret = gst_caps_ref (caps);
    goto bail;
  }

  ret = gst_va_vpp_caps_remove_fields (caps);

  tmp = gst_va_vpp_complete_caps_features (ret, GST_CAPS_FEATURE_MEMORY_VA);
  if (!gst_caps_is_subset (tmp, ret)) {
    gst_caps_append (ret, tmp);
  } else {
    gst_caps_unref (tmp);
  }

  tmp = gst_va_vpp_complete_caps_features (ret, GST_CAPS_FEATURE_MEMORY_DMABUF);
  if (!gst_caps_is_subset (tmp, ret)) {
    gst_caps_append (ret, tmp);
  } else {
    gst_caps_unref (tmp);
  }

  tmp = gst_va_vpp_complete_caps_features (ret,
      GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
  if (!gst_caps_is_subset (tmp, ret)) {
    gst_caps_append (ret, tmp);
  } else {
    gst_caps_unref (tmp);
  }

bail:
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

/*
 * This is an incomplete matrix of in formats and a score for the preferred output
 * format.
 *
 *         out: RGB24   RGB16  ARGB  AYUV  YUV444  YUV422 YUV420 YUV411 YUV410  PAL  GRAY
 *  in
 * RGB24          0      2       1     2     2       3      4      5      6      7    8
 * RGB16          1      0       1     2     2       3      4      5      6      7    8
 * ARGB           2      3       0     1     4       5      6      7      8      9    10
 * AYUV           3      4       1     0     2       5      6      7      8      9    10
 * YUV444         2      4       3     1     0       5      6      7      8      9    10
 * YUV422         3      5       4     2     1       0      6      7      8      9    10
 * YUV420         4      6       5     3     2       1      0      7      8      9    10
 * YUV411         4      6       5     3     2       1      7      0      8      9    10
 * YUV410         6      8       7     5     4       3      2      1      0      9    10
 * PAL            1      3       2     6     4       6      7      8      9      0    10
 * GRAY           1      4       3     2     1       5      6      7      8      9    0
 *
 * PAL or GRAY are never preferred, if we can we would convert to PAL instead
 * of GRAY, though
 * less subsampling is preferred and if any, preferably horizontal
 * We would like to keep the alpha, even if we would need to to colorspace conversion
 * or lose depth.
 */
#define SCORE_FORMAT_CHANGE       1
#define SCORE_DEPTH_CHANGE        1
#define SCORE_ALPHA_CHANGE        1
#define SCORE_CHROMA_W_CHANGE     1
#define SCORE_CHROMA_H_CHANGE     1
#define SCORE_PALETTE_CHANGE      1

#define SCORE_COLORSPACE_LOSS     2     /* RGB <-> YUV */
#define SCORE_DEPTH_LOSS          4     /* change bit depth */
#define SCORE_ALPHA_LOSS          8     /* lose the alpha channel */
#define SCORE_CHROMA_W_LOSS      16     /* vertical subsample */
#define SCORE_CHROMA_H_LOSS      32     /* horizontal subsample */
#define SCORE_PALETTE_LOSS       64     /* convert to palette format */
#define SCORE_COLOR_LOSS        128     /* convert to GRAY */

#define COLORSPACE_MASK (GST_VIDEO_FORMAT_FLAG_YUV | \
                         GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_GRAY)
#define ALPHA_MASK      (GST_VIDEO_FORMAT_FLAG_ALPHA)
#define PALETTE_MASK    (GST_VIDEO_FORMAT_FLAG_PALETTE)

/* calculate how much loss a conversion would be */
static gboolean
score_value (GstVaVpp * self, const GstVideoFormatInfo * in_info,
    GstVideoFormat format, gint * min_loss,
    const GstVideoFormatInfo ** out_info)
{
  const GstVideoFormatInfo *t_info;
  GstVideoFormatFlags in_flags, t_flags;
  gint loss;

  t_info = gst_video_format_get_info (format);
  if (!t_info || t_info->format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  /* accept input format immediately without loss */
  if (in_info == t_info) {
    *min_loss = 0;
    *out_info = t_info;
    return TRUE;
  }

  loss = SCORE_FORMAT_CHANGE;

  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  t_flags = GST_VIDEO_FORMAT_INFO_FLAGS (t_info);
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  if ((t_flags & PALETTE_MASK) != (in_flags & PALETTE_MASK)) {
    loss += SCORE_PALETTE_CHANGE;
    if (t_flags & PALETTE_MASK)
      loss += SCORE_PALETTE_LOSS;
  }

  if ((t_flags & COLORSPACE_MASK) != (in_flags & COLORSPACE_MASK)) {
    loss += SCORE_COLORSPACE_LOSS;
    if (t_flags & GST_VIDEO_FORMAT_FLAG_GRAY)
      loss += SCORE_COLOR_LOSS;
  }

  if ((t_flags & ALPHA_MASK) != (in_flags & ALPHA_MASK)) {
    loss += SCORE_ALPHA_CHANGE;
    if (in_flags & ALPHA_MASK)
      loss += SCORE_ALPHA_LOSS;
  }

  if ((in_info->h_sub[1]) != (t_info->h_sub[1])) {
    loss += SCORE_CHROMA_H_CHANGE;
    if ((in_info->h_sub[1]) < (t_info->h_sub[1]))
      loss += SCORE_CHROMA_H_LOSS;
  }
  if ((in_info->w_sub[1]) != (t_info->w_sub[1])) {
    loss += SCORE_CHROMA_W_CHANGE;
    if ((in_info->w_sub[1]) < (t_info->w_sub[1]))
      loss += SCORE_CHROMA_W_LOSS;
  }

  if ((in_info->bits) != (t_info->bits)) {
    loss += SCORE_DEPTH_CHANGE;
    if ((in_info->bits) > (t_info->bits))
      loss += SCORE_DEPTH_LOSS;
  }

  GST_DEBUG_OBJECT (self, "score %s -> %s = %d",
      GST_VIDEO_FORMAT_INFO_NAME (in_info),
      GST_VIDEO_FORMAT_INFO_NAME (t_info), loss);

  if (loss < *min_loss) {
    GST_DEBUG_OBJECT (self, "found new best %d", loss);
    *out_info = t_info;
    *min_loss = loss;
    return TRUE;
  }

  return FALSE;
}

static GstCaps *
gst_va_vpp_fixate_format (GstVaVpp * self, GstCaps * caps, GstCaps * result)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  GstStructure *ins;
  const gchar *in_format;
  const GstVideoFormatInfo *in_info, *out_info = NULL;
  GstCapsFeatures *features;
  GstVideoFormat fmt;
  gint min_loss = G_MAXINT;
  guint i, best_i, capslen;
  guint64 best_modifier;

  ins = gst_caps_get_structure (caps, 0);

  if (gst_video_is_dma_drm_caps (caps)) {
    guint32 fourcc;
    GstVideoFormat video_format;

    in_format = gst_structure_get_string (ins, "drm-format");
    if (!in_format)
      return NULL;

    fourcc = gst_video_dma_drm_fourcc_from_string (in_format, NULL);
    video_format = gst_va_video_format_from_drm_fourcc (fourcc);
    if (video_format == GST_VIDEO_FORMAT_UNKNOWN)
      return NULL;

    in_info = gst_video_format_get_info (video_format);
  } else {
    in_format = gst_structure_get_string (ins, "format");
    if (!in_format)
      return NULL;

    in_info =
        gst_video_format_get_info (gst_video_format_from_string (in_format));
  }

  if (!in_info)
    return NULL;

  GST_DEBUG_OBJECT (self, "source format %s", in_format);

  best_i = 0;
  best_modifier = DRM_FORMAT_MOD_INVALID;
  capslen = gst_caps_get_size (result);
  GST_DEBUG_OBJECT (self, "iterate %d structures", capslen);
  for (i = 0; i < capslen; i++) {
    gboolean is_dma;
    GstStructure *tests;
    const GValue *format;
    guint64 modifier;
    guint32 fourcc;

    features = gst_caps_get_features (result, i);
    tests = gst_caps_get_structure (result, i);

    if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      format = gst_structure_get_value (tests, "drm-format");
      is_dma = TRUE;
    } else {
      format = gst_structure_get_value (tests, "format");
      is_dma = FALSE;
    }
    /* should not happen */
    if (format == NULL)
      continue;

    if (GST_VALUE_HOLDS_LIST (format)) {
      gint j, len;

      len = gst_value_list_get_size (format);
      GST_DEBUG_OBJECT (self, "have %d formats", len);
      for (j = 0; j < len; j++) {
        const GValue *val;

        modifier = DRM_FORMAT_MOD_INVALID;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
          if (is_dma) {
            fourcc = gst_video_dma_drm_fourcc_from_string
                (g_value_get_string (val), &modifier);
            fmt = gst_va_video_format_from_drm_fourcc (fourcc);
          } else {
            fmt = gst_video_format_from_string (g_value_get_string (val));
          }
          if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
            continue;

          if (!gst_va_filter_has_video_format (btrans->filter, fmt, features))
            continue;

          if (score_value (self, in_info, fmt, &min_loss, &out_info)) {
            best_i = i;
            best_modifier = modifier;
          }

          if (min_loss == 0)
            break;
        }
      }
    } else if (G_VALUE_HOLDS_STRING (format)) {
      modifier = DRM_FORMAT_MOD_INVALID;

      if (is_dma) {
        fourcc = gst_video_dma_drm_fourcc_from_string
            (g_value_get_string (format), &modifier);
        fmt = gst_va_video_format_from_drm_fourcc (fourcc);
      } else {
        fmt = gst_video_format_from_string (g_value_get_string (format));
      }
      if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      if (!gst_va_filter_has_video_format (btrans->filter, fmt, features))
        continue;

      if (score_value (self, in_info, fmt, &min_loss, &out_info)) {
        best_i = i;
        best_modifier = modifier;
      }
    }

    if (min_loss == 0)
      break;
  }

  if (out_info) {
    GstCaps *ret;
    GstStructure *out;

    features = gst_caps_features_copy (gst_caps_get_features (result, best_i));
    out = gst_structure_copy (gst_caps_get_structure (result, best_i));

    if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      gchar *drm_fmt_name;

      g_assert (best_modifier != DRM_FORMAT_MOD_INVALID);

      drm_fmt_name = gst_video_dma_drm_fourcc_to_string
          (gst_va_drm_fourcc_from_video_format (out_info->format),
          best_modifier);
      gst_structure_set (out, "drm-format", G_TYPE_STRING, drm_fmt_name, NULL);
      g_free (drm_fmt_name);
    } else {
      gst_structure_set (out, "format", G_TYPE_STRING,
          GST_VIDEO_FORMAT_INFO_NAME (out_info), NULL);
    }
    ret = gst_caps_new_full (out, NULL);
    gst_caps_set_features_simple (ret, features);
    return ret;
  }

  return NULL;
}

static void
gst_va_vpp_fixate_size (GstVaVpp * self, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;
  GValue fpar = { 0, };
  GValue tpar = { 0, };

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* If we're fixating from the sinkpad we always set the PAR and
   * assume that missing PAR on the sinkpad means 1/1 and
   * missing PAR on the srcpad means undefined
   */
  if (direction == GST_PAD_SINK) {
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION_RANGE);
      gst_value_set_fraction_range_full (&tpar, 1, G_MAXINT, G_MAXINT, 1);
      to_par = &tpar;
    }
  } else {
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;

      gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
          NULL);
    }
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
  }

  /* we have both PAR but they might not be fixated */
  {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
    gint w = 0, h = 0;
    gint from_dar_n, from_dar_d;
    gint num, den;

    /* from_par should be fixed */
    g_return_if_fail (gst_value_is_fixed (from_par));

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    gst_structure_get_int (outs, "width", &w);
    gst_structure_get_int (outs, "height", &h);

    /* if video-orientation changes */
    switch (gst_va_filter_get_orientation (btrans->filter)) {
      case GST_VIDEO_ORIENTATION_90R:
      case GST_VIDEO_ORIENTATION_90L:
      case GST_VIDEO_ORIENTATION_UL_LR:
      case GST_VIDEO_ORIENTATION_UR_LL:
        SWAP_INT (from_w, from_h);
        SWAP_INT (from_par_n, from_par_d);
        break;
      default:
        break;
    }

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (w && h) {
      guint n, d;

      GST_DEBUG_OBJECT (self, "dimensions already set to %dx%d, not fixating",
          w, h);
      if (!gst_value_is_fixed (to_par)) {
        if (gst_video_calculate_display_ratio (&n, &d, from_w, from_h,
                from_par_n, from_par_d, w, h)) {
          GST_DEBUG_OBJECT (self, "fixating to_par to %dx%d", n, d);
          if (gst_structure_has_field (outs, "pixel-aspect-ratio"))
            gst_structure_fixate_field_nearest_fraction (outs,
                "pixel-aspect-ratio", n, d);
          else if (n != d)
            gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                n, d, NULL);
        }
      }
      goto done;
    }

    /* Calculate input DAR */
    if (!gst_util_fraction_multiply (from_w, from_h, from_par_n, from_par_d,
            &from_dar_n, &from_dar_d)) {
      GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      goto done;
    }

    GST_DEBUG_OBJECT (self, "Input DAR is %d/%d", from_dar_n, from_dar_d);

    /* If either width or height are fixed there's not much we
     * can do either except choosing a height or width and PAR
     * that matches the DAR as good as possible
     */
    if (h) {
      GstStructure *tmp;
      gint set_w, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (self, "height is fixed (%d)", h);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the width that is nearest to the
       * width with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (self, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den)) {
          GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
              ("Error calculating the output scaled size - integer overflow"));
          goto done;
        }

        w = (guint) gst_util_uint64_scale_int_round (h, num, den);
        gst_structure_fixate_field_nearest_int (outs, "width", w);

        goto done;
      }

      /* The PAR is not fixed and it's quite likely that we can set
       * an arbitrary PAR. */

      /* Check if we can keep the input width */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* Might have failed but try to keep the DAR nonetheless by
       * adjusting the PAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "width", G_TYPE_INT, set_w,
              "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
              NULL);
        goto done;
      }

      /* Otherwise scale the width to the new PAR and check if the
       * adjusted with is accepted. If all that fails we can't keep
       * the DAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int_round (h, num, den);
      gst_structure_fixate_field_nearest_int (outs, "width", w);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);

      goto done;
    } else if (w) {
      GstStructure *tmp;
      gint set_h, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (self, "width is fixed (%d)", w);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the height that is nearest to the
       * height with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (self, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den)) {
          GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
              ("Error calculating the output scaled size - integer overflow"));
          goto done;
        }

        h = (guint) gst_util_uint64_scale_int_round (w, den, num);
        gst_structure_fixate_field_nearest_int (outs, "height", h);

        goto done;
      }

      /* The PAR is not fixed and it's quite likely that we can set
       * an arbitrary PAR. */

      /* Check if we can keep the input height */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* Might have failed but try to keep the DAR nonetheless by
       * adjusting the PAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }
      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "height", G_TYPE_INT, set_h,
              "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
              NULL);
        goto done;
      }

      /* Otherwise scale the height to the new PAR and check if the
       * adjusted with is accepted. If all that fails we can't keep
       * the DAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scale sized - integer overflow"));
        goto done;
      }

      h = (guint) gst_util_uint64_scale_int_round (w, den, num);
      gst_structure_fixate_field_nearest_int (outs, "height", h);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);

      goto done;
    } else if (gst_value_is_fixed (to_par)) {
      GstStructure *tmp;
      gint set_h, set_w, f_h, f_w;

      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      /* Calculate scale factor for the PAR change */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_n,
              to_par_d, &num, &den)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      /* Try to keep the input height (because of interlacing) */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &set_w);
      gst_structure_free (tmp);

      /* We kept the DAR and the height is nearest to the original height */
      if (set_w == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      f_h = set_h;
      f_w = set_w;

      /* If the former failed, try to keep the input width at least */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_free (tmp);

      /* We kept the DAR and the width is nearest to the original width */
      if (set_h == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      /* If all this failed, keep the dimensions with the DAR that was closest
       * to the correct DAR. This changes the DAR but there's not much else to
       * do here.
       */
      if (set_w * ABS (set_h - h) < ABS (f_w - w) * f_h) {
        f_h = set_h;
        f_w = set_w;
      }
      gst_structure_set (outs, "width", G_TYPE_INT, f_w, "height", G_TYPE_INT,
          f_h, NULL);
      goto done;
    } else {
      GstStructure *tmp;
      gint set_h, set_w, set_par_n, set_par_d, tmp2;

      /* width, height and PAR are not fixed but passthrough is not possible */

      /* First try to keep the height and width as good as possible
       * and scale PAR */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);

        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* Otherwise try to scale width to keep the DAR with the set
       * PAR and height */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, tmp2, "height",
            G_TYPE_INT, set_h, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* ... or try the same with the height */
      h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, tmp2, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* If all fails we can't keep the DAR and take the nearest values
       * for everything from the first try */
      gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
          G_TYPE_INT, set_h, NULL);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);
    }
  }

done:
  if (from_par == &fpar)
    g_value_unset (&fpar);
  if (to_par == &tpar)
    g_value_unset (&tpar);
}

static gboolean
subsampling_unchanged (GstVideoInfo * in_info, GstVideoInfo * out_info)
{
  gint i;
  const GstVideoFormatInfo *in_format, *out_format;

  if (GST_VIDEO_INFO_N_COMPONENTS (in_info) !=
      GST_VIDEO_INFO_N_COMPONENTS (out_info))
    return FALSE;

  in_format = in_info->finfo;
  out_format = out_info->finfo;

  for (i = 0; i < GST_VIDEO_INFO_N_COMPONENTS (in_info); i++) {
    if (GST_VIDEO_FORMAT_INFO_W_SUB (in_format,
            i) != GST_VIDEO_FORMAT_INFO_W_SUB (out_format, i))
      return FALSE;
    if (GST_VIDEO_FORMAT_INFO_H_SUB (in_format,
            i) != GST_VIDEO_FORMAT_INFO_H_SUB (out_format, i))
      return FALSE;
  }

  return TRUE;
}

static void
transfer_colorimetry_from_input (GstVaVpp * self, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstStructure *out_caps_s = gst_caps_get_structure (out_caps, 0);
  GstStructure *in_caps_s = gst_caps_get_structure (in_caps, 0);
  gboolean have_colorimetry =
      gst_structure_has_field (out_caps_s, "colorimetry");
  gboolean have_chroma_site =
      gst_structure_has_field (out_caps_s, "chroma-site");

  /* If the output already has colorimetry and chroma-site, stop,
   * otherwise try and transfer what we can from the input caps */
  if (have_colorimetry && have_chroma_site)
    return;

  {
    GstVideoInfo in_info, out_info;
    const GValue *in_colorimetry =
        gst_structure_get_value (in_caps_s, "colorimetry");

    if (!gst_va_video_info_from_caps (&in_info, NULL, in_caps)) {
      GST_WARNING_OBJECT (self,
          "Failed to convert sink pad caps to video info");
      return;
    }

    if (!gst_va_video_info_from_caps (&out_info, NULL, out_caps)) {
      GST_WARNING_OBJECT (self, "Failed to convert src pad caps to video info");
      return;
    }

    if (!have_colorimetry && in_colorimetry != NULL) {
      if ((GST_VIDEO_INFO_IS_YUV (&out_info)
              && GST_VIDEO_INFO_IS_YUV (&in_info))
          || (GST_VIDEO_INFO_IS_RGB (&out_info)
              && GST_VIDEO_INFO_IS_RGB (&in_info))
          || (GST_VIDEO_INFO_IS_GRAY (&out_info)
              && GST_VIDEO_INFO_IS_GRAY (&in_info))) {
        /* Can transfer the colorimetry intact from the input if it has it */
        gst_structure_set_value (out_caps_s, "colorimetry", in_colorimetry);
      } else {
        gchar *colorimetry_str;

        /* Changing between YUV/RGB - forward primaries and transfer function, but use
         * default range and matrix.
         * the primaries is used for conversion between RGB and XYZ (CIE 1931 coordinate).
         * the transfer function could be another reference (e.g., HDR)
         */
        out_info.colorimetry.primaries = in_info.colorimetry.primaries;
        out_info.colorimetry.transfer = in_info.colorimetry.transfer;

        colorimetry_str =
            gst_video_colorimetry_to_string (&out_info.colorimetry);
        gst_caps_set_simple (out_caps, "colorimetry", G_TYPE_STRING,
            colorimetry_str, NULL);
        g_free (colorimetry_str);
      }
    }

    /* Only YUV output needs chroma-site. If the input was also YUV and had the same chroma
     * subsampling, transfer the siting. If the sub-sampling is changing, then the planes get
     * scaled anyway so there's no real reason to prefer the input siting. */
    if (!have_chroma_site && GST_VIDEO_INFO_IS_YUV (&out_info)) {
      if (GST_VIDEO_INFO_IS_YUV (&in_info)) {
        const GValue *in_chroma_site =
            gst_structure_get_value (in_caps_s, "chroma-site");
        if (in_chroma_site != NULL
            && subsampling_unchanged (&in_info, &out_info))
          gst_structure_set_value (out_caps_s, "chroma-site", in_chroma_site);
      }
    }
  }
}

static void
copy_misc_fields_from_input (GstCaps * in_caps, GstCaps * out_caps)
{
  const gchar *fields[] = { "interlace-mode", "field-order", "multiview-mode",
    "multiview-flags", "framerate"
  };
  GstStructure *out_caps_s = gst_caps_get_structure (out_caps, 0);
  GstStructure *in_caps_s = gst_caps_get_structure (in_caps, 0);
  int i;

  for (i = 0; i < G_N_ELEMENTS (fields); i++) {
    const GValue *in_field = gst_structure_get_value (in_caps_s, fields[i]);
    const GValue *out_field = gst_structure_get_value (out_caps_s, fields[i]);

    if (out_field && gst_value_is_fixed (out_field))
      continue;

    if (in_field)
      gst_structure_set_value (out_caps_s, fields[i], in_field);
  }
}

static void
update_hdr_fields (GstVaVpp * self, GstCaps * result)
{
  GstStructure *s = gst_caps_get_structure (result, 0);
  GstVideoInfo out_info;
  gboolean have_colorimetry;

  gst_structure_remove_fields (s, "mastering-display-info",
      "content-light-level", "hdr-format", NULL);

  have_colorimetry = gst_structure_has_field (s, "colorimetry");
  if (!have_colorimetry) {
    if (gst_va_video_info_from_caps (&out_info, NULL, result)) {
      gchar *colorimetry_str =
          gst_video_colorimetry_to_string (&out_info.colorimetry);
      gst_caps_set_simple (result, "colorimetry", G_TYPE_STRING,
          colorimetry_str, NULL);
      g_free (colorimetry_str);
    } else {
      GST_WARNING_OBJECT (self, "Failed to convert src pad caps to video info");
    }
  }
}

static GstCaps *
gst_va_vpp_fixate_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstCaps *result;

  GST_DEBUG_OBJECT (self,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  /* will iterate in all structures to find one with "best color" */
  result = gst_va_vpp_fixate_format (self, caps, othercaps);
  if (!result)
    return othercaps;

  gst_clear_caps (&othercaps);

  gst_va_vpp_fixate_size (self, direction, caps, result);

  /* some fields might be lost while feature caps conversion */
  copy_misc_fields_from_input (caps, result);

  /* fixate remaining fields */
  result = gst_caps_fixate (result);

  if (direction == GST_PAD_SINK) {
    if (self->hdr_mapping)
      update_hdr_fields (self, result);

    /* Try and preserve input colorimetry / chroma information */
    transfer_colorimetry_from_input (self, caps, result);

    if (gst_caps_is_subset (caps, result))
      gst_caps_replace (&result, caps);
  }

  GST_DEBUG_OBJECT (self, "fixated othercaps to %" GST_PTR_FORMAT, result);

  return result;
}

static void
_get_scale_factor (GstVaVpp * self, gdouble * w_factor, gdouble * h_factor)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  gdouble w = GST_VIDEO_INFO_WIDTH (&btrans->out_info);
  gdouble h = GST_VIDEO_INFO_HEIGHT (&btrans->out_info);

  switch (gst_va_filter_get_orientation (btrans->filter)) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UR_LL:
    case GST_VIDEO_ORIENTATION_UL_LR:{
      gdouble tmp = h;
      h = w;
      w = tmp;
      break;
    }
    default:
      break;
  }

  *w_factor = GST_VIDEO_INFO_WIDTH (&btrans->in_info);
  *w_factor /= w;

  *h_factor = GST_VIDEO_INFO_HEIGHT (&btrans->in_info);
  *h_factor /= h;
}

static gboolean
gst_va_vpp_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  const GstVideoInfo *in_info = &btrans->in_info, *out_info = &btrans->out_info;
  gdouble new_x = 0, new_y = 0, x = 0, y = 0, w_factor = 1, h_factor = 1;
  gboolean ret;

  GST_TRACE_OBJECT (self, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
      if (GST_VIDEO_INFO_WIDTH (in_info) != GST_VIDEO_INFO_WIDTH (out_info)
          || GST_VIDEO_INFO_HEIGHT (in_info) != GST_VIDEO_INFO_HEIGHT (out_info)
          || gst_va_filter_get_orientation (btrans->filter) !=
          GST_VIDEO_ORIENTATION_IDENTITY) {

        if (!gst_navigation_event_get_coordinates (event, &x, &y))
          break;

        event = gst_event_make_writable (event);

        /* video-direction compensation */
        switch (gst_va_filter_get_orientation (btrans->filter)) {
          case GST_VIDEO_ORIENTATION_90R:
            new_x = y;
            new_y = GST_VIDEO_INFO_WIDTH (out_info) - 1 - x;
            break;
          case GST_VIDEO_ORIENTATION_90L:
            new_x = GST_VIDEO_INFO_HEIGHT (out_info) - 1 - y;
            new_y = x;
            break;
          case GST_VIDEO_ORIENTATION_UL_LR:
            new_x = y;
            new_y = x;
            break;
          case GST_VIDEO_ORIENTATION_UR_LL:
            new_x = GST_VIDEO_INFO_HEIGHT (out_info) - 1 - y;
            new_y = GST_VIDEO_INFO_WIDTH (out_info) - 1 - x;
            break;
          case GST_VIDEO_ORIENTATION_180:
            /* FIXME: is this correct? */
            new_x = GST_VIDEO_INFO_WIDTH (out_info) - 1 - x;
            new_y = GST_VIDEO_INFO_HEIGHT (out_info) - 1 - y;
            break;
          case GST_VIDEO_ORIENTATION_HORIZ:
            new_x = GST_VIDEO_INFO_WIDTH (out_info) - 1 - x;
            new_y = y;
            break;
          case GST_VIDEO_ORIENTATION_VERT:
            new_x = x;
            new_y = GST_VIDEO_INFO_HEIGHT (out_info) - 1 - y;
            break;
          default:
            new_x = x;
            new_y = y;
            break;
        }

        /* scale compensation */
        _get_scale_factor (self, &w_factor, &h_factor);
        new_x *= w_factor;
        new_y *= h_factor;

        /* crop compensation is done by videocrop */

        GST_TRACE_OBJECT (self, "from %fx%f to %fx%f", x, y, new_x, new_y);
        gst_navigation_event_set_coordinates (event, new_x, new_y);
      }
      break;
    default:
      break;
  }

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->src_event (trans, event);

  return ret;
}

static gboolean
gst_va_vpp_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVaVpp *self = GST_VA_VPP (trans);
  GstTagList *taglist;
  GstVideoOrientationMethod method;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      gst_event_parse_tag (event, &taglist);

      if (self->direction != GST_VIDEO_ORIENTATION_AUTO)
        break;

      if (!gst_video_orientation_from_tag (taglist, &method))
        break;

      GST_OBJECT_LOCK (self);
      self->tag_direction = method;
      _update_properties_unlocked (self);
      GST_OBJECT_UNLOCK (self);

      gst_va_vpp_update_passthrough (self, FALSE);

      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static void
_install_static_properties (GObjectClass * klass)
{
  /**
   * GstVaPostProc:disable-passthrough:
   *
   * If set to %TRUE the filter will not enable passthrough mode, thus
   * each frame will be processed. It's useful for cropping, for
   * example.
   *
   * Since: 1.20
   */
  PROPERTIES (PROP_DISABLE_PASSTHROUGH) =
      g_param_spec_boolean ("disable-passthrough", "Disable Passthrough",
      "Forces passing buffers through the postprocessor", FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY);
  g_object_class_install_property (klass, PROP_DISABLE_PASSTHROUGH,
      PROPERTIES (PROP_DISABLE_PASSTHROUGH));

  /**
   * GstVaPostProc:add-borders:
   *
   * If set to %TRUE the filter will add black borders if necessary to
   * keep the display aspect ratio.
   *
   * Since: 1.20
   */
  PROPERTIES (PROP_ADD_BORDERS) = g_param_spec_boolean ("add-borders",
      "Add Borders",
      "Add black borders if necessary to keep the display aspect ratio", FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING);
  g_object_class_install_property (klass, PROP_ADD_BORDERS,
      PROPERTIES (PROP_ADD_BORDERS));

  /**
   * GstVaPostProc:scale-method
   *
   * Sets the scale method algorithm to use when resizing.
   *
   * Since: 1.22
   */
  PROPERTIES (PROP_SCALE_METHOD) = g_param_spec_enum ("scale-method",
      "Scale Method", "Scale method to use", GST_TYPE_VA_SCALE_METHOD,
      VA_FILTER_SCALING_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
      | GST_PARAM_MUTABLE_PLAYING);
  g_object_class_install_property (klass, PROP_SCALE_METHOD,
      PROPERTIES (PROP_SCALE_METHOD));
}

static void
gst_va_vpp_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *doc_caps, *caps = NULL;
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstVaBaseTransformClass *btrans_class = GST_VA_BASE_TRANSFORM_CLASS (g_class);
  GstVaDisplay *display;
  GstVaFilter *filter;
  struct CData *cdata = class_data;
  gchar *long_name;
  GString *klass;

  parent_class = g_type_class_peek_parent (g_class);

  btrans_class->render_device_path = g_strdup (cdata->render_device_path);

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API Video Postprocessor in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API Video Postprocessor");
  }

  klass = g_string_new ("Converter/Filter/Colorspace/Scaler/Video/Hardware");

  display = gst_va_display_platform_new (btrans_class->render_device_path);
  filter = gst_va_filter_new (display);

  if (gst_va_filter_open (filter)) {
    caps = gst_va_filter_get_caps (filter);

    /* adds any to enable passthrough */
    {
      GstCaps *any_caps = gst_caps_new_empty_simple ("video/x-raw");
      gst_caps_set_features_simple (any_caps, gst_caps_features_new_any ());
      caps = gst_caps_merge (caps, any_caps);
    }

    /* add converter klass */
    {
      int i;
      VAProcFilterType types[] = { VAProcFilterColorBalance,
        VAProcFilterSkinToneEnhancement, VAProcFilterSharpening,
        VAProcFilterNoiseReduction
      };

      for (i = 0; i < G_N_ELEMENTS (types); i++) {
        if (gst_va_filter_has_filter (filter, types[i])) {
          g_string_prepend (klass, "Effect/");
          break;
        }
      }
    }
  } else {
    caps = gst_caps_from_string (caps_str);
  }

  gst_element_class_set_metadata (element_class, long_name, klass->str,
      "VA-API based video postprocessor",
      "Víctor Jáquez <vjaquez@igalia.com>");

  g_string_free (klass, TRUE);

  doc_caps = gst_caps_from_string (caps_str);

  sink_pad_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      caps);
  gst_element_class_add_pad_template (element_class, sink_pad_templ);
  gst_pad_template_set_documentation_caps (sink_pad_templ,
      gst_caps_ref (doc_caps));

  src_pad_templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      caps);
  gst_element_class_add_pad_template (element_class, src_pad_templ);
  gst_pad_template_set_documentation_caps (src_pad_templ,
      gst_caps_ref (doc_caps));
  gst_caps_unref (doc_caps);

  gst_caps_unref (caps);

  object_class->dispose = gst_va_vpp_dispose;
  object_class->set_property = gst_va_vpp_set_property;
  object_class->get_property = gst_va_vpp_get_property;

  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_va_vpp_propose_allocation);
  trans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_va_vpp_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_va_vpp_fixate_caps);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_va_vpp_before_transform);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_va_vpp_transform);
  trans_class->transform_meta = GST_DEBUG_FUNCPTR (gst_va_vpp_transform_meta);
  trans_class->src_event = GST_DEBUG_FUNCPTR (gst_va_vpp_src_event);
  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_va_vpp_sink_event);
  trans_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_va_vpp_prepare_output_buffer);

  trans_class->transform_ip_on_passthrough = FALSE;

  btrans_class->set_info = GST_DEBUG_FUNCPTR (gst_va_vpp_set_info);
  btrans_class->update_properties =
      GST_DEBUG_FUNCPTR (gst_va_vpp_update_properties);

  gst_va_filter_install_properties (filter, object_class);

  _install_static_properties (object_class);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  g_free (cdata);
  gst_object_unref (filter);
  gst_object_unref (display);
}

static inline void
_create_colorbalance_channel (GstVaVpp * self, const gchar * label)
{
  GstColorBalanceChannel *channel;

  channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
  channel->label = g_strdup_printf ("VA-%s", label);
  channel->min_value = -1000;
  channel->max_value = 1000;

  self->channels = g_list_append (self->channels, channel);
}

static void
gst_va_vpp_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaVpp *self = GST_VA_VPP (instance);
  GParamSpec *pspec;

  self->direction = GST_VIDEO_ORIENTATION_IDENTITY;
  self->prev_direction = self->direction;
  self->tag_direction = GST_VIDEO_ORIENTATION_AUTO;

  pspec = g_object_class_find_property (g_class, "denoise");
  if (pspec)
    self->denoise = g_value_get_float (g_param_spec_get_default_value (pspec));

  pspec = g_object_class_find_property (g_class, "sharpen");
  if (pspec)
    self->sharpen = g_value_get_float (g_param_spec_get_default_value (pspec));

  pspec = g_object_class_find_property (g_class, "skin-tone");
  if (pspec) {
    const GValue *value = g_param_spec_get_default_value (pspec);
    if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
      self->skintone = g_value_get_boolean (value);
    else
      self->skintone = g_value_get_float (value);
  }

  /* color balance */
  pspec = g_object_class_find_property (g_class, "brightness");
  if (pspec) {
    self->brightness =
        g_value_get_float (g_param_spec_get_default_value (pspec));
    _create_colorbalance_channel (self, "BRIGHTNESS");
  }
  pspec = g_object_class_find_property (g_class, "contrast");
  if (pspec) {
    self->contrast = g_value_get_float (g_param_spec_get_default_value (pspec));
    _create_colorbalance_channel (self, "CONTRAST");
  }
  pspec = g_object_class_find_property (g_class, "hue");
  if (pspec) {
    self->hue = g_value_get_float (g_param_spec_get_default_value (pspec));
    _create_colorbalance_channel (self, "HUE");
  }
  pspec = g_object_class_find_property (g_class, "saturation");
  if (pspec) {
    self->saturation =
        g_value_get_float (g_param_spec_get_default_value (pspec));
    _create_colorbalance_channel (self, "SATURATION");
  }

  /* HDR tone mapping */
  pspec = g_object_class_find_property (g_class, "hdr-tone-mapping");
  if (pspec) {
    self->hdr_mapping =
        g_value_get_boolean (g_param_spec_get_default_value (pspec));
  }

  /* enable QoS */
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (instance), TRUE);
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_vpp_debug, "vapostproc", 0,
      "VA Video Postprocessor");

#define D(type) \
  G_PASTE (META_TAG_, type) = \
    g_quark_from_static_string (G_PASTE (G_PASTE (GST_META_TAG_VIDEO_, type), _STR))
  D (COLORSPACE);
  D (SIZE);
  D (ORIENTATION);
#undef D
  META_TAG_VIDEO = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);

  return NULL;
}

gboolean
gst_va_vpp_register (GstPlugin * plugin, GstVaDevice * device,
    gboolean has_colorbalance, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaVppClass),
    .class_init = gst_va_vpp_class_init,
    .instance_size = sizeof (GstVaVpp),
    .instance_init = gst_va_vpp_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaPostProc", "GstVa%sPostProc",
      &type_name, "vapostproc", "va%spostproc", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_VA_BASE_TRANSFORM, type_name,
      &type_info, 0);

  if (has_colorbalance) {
    const GInterfaceInfo info = { gst_va_vpp_colorbalance_init, NULL, NULL };
    g_type_add_interface_static (type, GST_TYPE_COLOR_BALANCE, &info);
  }

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}

/* Color Balance interface */
static const GList *
gst_va_vpp_colorbalance_list_channels (GstColorBalance * balance)
{
  GstVaVpp *self = GST_VA_VPP (balance);

  return self->channels;
}

/* This assumes --as happens with intel drivers-- that max values are
 * bigger than the simmetrical values of min values */
static float
make_max_simmetrical (GParamSpecFloat * fpspec)
{
  gfloat max;

  if (fpspec->default_value == 0)
    max = -fpspec->minimum;
  else
    max = fpspec->default_value + ABS (fpspec->minimum - fpspec->default_value);

  return MIN (max, fpspec->maximum);
}

static gboolean
_set_cb_val (GstVaVpp * self, const gchar * name,
    GstColorBalanceChannel * channel, gint value, gfloat * cb)
{
  GObjectClass *klass = G_OBJECT_CLASS (GST_VA_VPP_GET_CLASS (self));
  GParamSpec *pspec;
  GParamSpecFloat *fpspec;
  gfloat new_value, max;
  gboolean changed;

  pspec = g_object_class_find_property (klass, name);
  if (!pspec)
    return FALSE;

  fpspec = G_PARAM_SPEC_FLOAT (pspec);
  max = make_max_simmetrical (fpspec);

  new_value = (value - channel->min_value) * (max - fpspec->minimum)
      / (channel->max_value - channel->min_value) + fpspec->minimum;

  GST_OBJECT_LOCK (self);
  changed = new_value != *cb;
  *cb = new_value;
  value = (*cb + fpspec->minimum) * (channel->max_value - channel->min_value)
      / (max - fpspec->minimum) + channel->min_value;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "%s: %d / %f", channel->label, value, new_value);
    gst_color_balance_value_changed (GST_COLOR_BALANCE (self), channel, value);
    g_atomic_int_set (&self->rebuild_filters, TRUE);
  }

  return TRUE;
}

static void
gst_va_vpp_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstVaVpp *self = GST_VA_VPP (balance);

  if (g_str_has_suffix (channel->label, "HUE"))
    _set_cb_val (self, "hue", channel, value, &self->hue);
  else if (g_str_has_suffix (channel->label, "BRIGHTNESS"))
    _set_cb_val (self, "brightness", channel, value, &self->brightness);
  else if (g_str_has_suffix (channel->label, "CONTRAST"))
    _set_cb_val (self, "contrast", channel, value, &self->contrast);
  else if (g_str_has_suffix (channel->label, "SATURATION"))
    _set_cb_val (self, "saturation", channel, value, &self->saturation);
}

static gboolean
_get_cb_val (GstVaVpp * self, const gchar * name,
    GstColorBalanceChannel * channel, gfloat * cb, gint * val)
{
  GObjectClass *klass = G_OBJECT_CLASS (GST_VA_VPP_GET_CLASS (self));
  GParamSpec *pspec;
  GParamSpecFloat *fpspec;
  gfloat max;

  pspec = g_object_class_find_property (klass, name);
  if (!pspec)
    return FALSE;

  fpspec = G_PARAM_SPEC_FLOAT (pspec);
  max = make_max_simmetrical (fpspec);

  GST_OBJECT_LOCK (self);
  *val = (*cb + fpspec->minimum) * (channel->max_value - channel->min_value)
      / (max - fpspec->minimum) + channel->min_value;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gint
gst_va_vpp_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstVaVpp *self = GST_VA_VPP (balance);
  gint value = 0;

  if (g_str_has_suffix (channel->label, "HUE"))
    _get_cb_val (self, "hue", channel, &self->hue, &value);
  else if (g_str_has_suffix (channel->label, "BRIGHTNESS"))
    _get_cb_val (self, "brightness", channel, &self->brightness, &value);
  else if (g_str_has_suffix (channel->label, "CONTRAST"))
    _get_cb_val (self, "contrast", channel, &self->contrast, &value);
  else if (g_str_has_suffix (channel->label, "SATURATION"))
    _get_cb_val (self, "saturation", channel, &self->saturation, &value);

  return value;
}

static GstColorBalanceType
gst_va_vpp_colorbalance_get_balance_type (GstColorBalance * balance)
{
  return GST_COLOR_BALANCE_HARDWARE;
}

static void
gst_va_vpp_colorbalance_init (gpointer iface, gpointer data)
{
  GstColorBalanceInterface *cbiface = iface;

  cbiface->list_channels = gst_va_vpp_colorbalance_list_channels;
  cbiface->set_value = gst_va_vpp_colorbalance_set_value;
  cbiface->get_value = gst_va_vpp_colorbalance_get_value;
  cbiface->get_balance_type = gst_va_vpp_colorbalance_get_balance_type;
}
