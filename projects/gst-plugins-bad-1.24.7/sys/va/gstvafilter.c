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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvafilter.h"

#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>
#include <gst/video/video.h>
#include <va/va_drmcommon.h>

#include "gstvacaps.h"
#include "gstvadisplay_priv.h"
#include <string.h>

struct _GstVaFilter
{
  GstObject parent;

  GstVaDisplay *display;
  VAConfigID config;
  VAContextID context;

  /* hardware constraints */
  VAProcPipelineCaps pipeline_caps;

  guint32 mem_types;
  gint min_width;
  gint max_width;
  gint min_height;
  gint max_height;

  GArray *surface_formats;
  GArray *image_formats;

  GArray *available_filters;

  /* stream information */
  guint mirror;
  guint rotation;
  GstVideoOrientationMethod orientation;

  guint32 scale_method;

  gboolean crop_enabled;

  VARectangle input_region;
  VARectangle output_region;

  VAProcColorStandardType input_color_standard;
  VAProcColorProperties input_color_properties;
  VAProcColorStandardType output_color_standard;
  VAProcColorProperties output_color_properties;

  GArray *filters;
};

GST_DEBUG_CATEGORY_STATIC (gst_va_filter_debug);
#define GST_CAT_DEFAULT gst_va_filter_debug

#define gst_va_filter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVaFilter, gst_va_filter, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_va_filter_debug, "vafilter", 0, "VA Filter"));

enum
{
  PROP_DISPLAY = 1,
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES];

static void
gst_va_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaFilter *self = GST_VA_FILTER (object);

  switch (prop_id) {
    case PROP_DISPLAY:{
      g_assert (!self->display);
      self->display = g_value_dup_object (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_filter_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVaFilter *self = GST_VA_FILTER (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_filter_dispose (GObject * object)
{
  GstVaFilter *self = GST_VA_FILTER (object);

  gst_va_filter_close (self);

  g_clear_pointer (&self->available_filters, g_array_unref);
  g_clear_pointer (&self->image_formats, g_array_unref);
  g_clear_pointer (&self->surface_formats, g_array_unref);
  gst_clear_object (&self->display);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_filter_class_init (GstVaFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_va_filter_set_property;
  gobject_class->get_property = gst_va_filter_get_property;
  gobject_class->dispose = gst_va_filter_dispose;

  g_properties[PROP_DISPLAY] =
      g_param_spec_object ("display", "GstVaDisplay", "GstVADisplay object",
      GST_TYPE_VA_DISPLAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, g_properties);
}

static void
gst_va_filter_init (GstVaFilter * self)
{
  self->config = VA_INVALID_ID;
  self->context = VA_INVALID_ID;

  self->min_height = 1;
  self->max_height = G_MAXINT;
  self->min_width = 1;
  self->max_width = G_MAXINT;
}

GstVaFilter *
gst_va_filter_new (GstVaDisplay * display)
{
  GstVaFilter *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), NULL);

  self = g_object_new (GST_TYPE_VA_FILTER, "display", display, NULL);
  gst_object_ref_sink (self);

  return self;
}

gboolean
gst_va_filter_is_open (GstVaFilter * self)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  GST_OBJECT_LOCK (self);
  ret = (self->config != VA_INVALID_ID && self->context != VA_INVALID_ID);
  GST_OBJECT_UNLOCK (self);
  return ret;
}

static gboolean
gst_va_filter_ensure_config_attributes (GstVaFilter * self,
    guint32 * rt_formats_ptr)
{
  VAConfigAttrib attribs[] = {
    {.type = VAConfigAttribMaxPictureWidth,},
    {.type = VAConfigAttribMaxPictureHeight,},
    {.type = VAConfigAttribRTFormat,},
  };
  VADisplay dpy;
  VAStatus status;
  guint i, value, rt_formats = 0, max_width = 0, max_height = 0;

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaGetConfigAttributes (dpy, VAProfileNone, VAEntrypointVideoProc,
      attribs, G_N_ELEMENTS (attribs));
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaGetConfigAttributes: %s", vaErrorStr (status));
    return FALSE;
  }

  for (i = 0; i < G_N_ELEMENTS (attribs); i++) {
    value = attribs[i].value;
    if (value == VA_ATTRIB_NOT_SUPPORTED)
      continue;
    switch (attribs[i].type) {
      case VAConfigAttribMaxPictureHeight:
        max_height = value;
        break;
      case VAConfigAttribMaxPictureWidth:
        max_width = value;
        break;
      case VAConfigAttribRTFormat:
        rt_formats = value;
        break;
      default:
        break;
    }
  }

  if (rt_formats_ptr && rt_formats != 0)
    *rt_formats_ptr = rt_formats;
  if (max_width > 0 && max_width < G_MAXINT)
    self->max_width = max_width;
  if (max_height > 0 && max_height < G_MAXINT)
    self->max_height = max_height;

  return TRUE;
}

/* There are formats that are not handled correctly by driver */
static gboolean
format_is_accepted (GstVaFilter * self, GstVideoFormat format)
{
  /* https://github.com/intel/media-driver/issues/690
   * https://github.com/intel/media-driver/issues/644 */
  if (!GST_VA_DISPLAY_IS_IMPLEMENTATION (self->display, INTEL_IHD))
    return TRUE;

  switch (format) {
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
      return FALSE;
    default:
      break;
  }

  return TRUE;
}

static gboolean
gst_va_filter_ensure_surface_attributes (GstVaFilter * self)
{
  GArray *surface_formats;
  GstVideoFormat format;
  VASurfaceAttrib *attribs;
  guint i, attrib_count;

  attribs =
      gst_va_get_surface_attribs (self->display, self->config, &attrib_count);
  if (!attribs)
    return FALSE;
  surface_formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));

  for (i = 0; i < attrib_count; i++) {
    if (attribs[i].value.type != VAGenericValueTypeInteger)
      continue;
    switch (attribs[i].type) {
      case VASurfaceAttribPixelFormat:
        format = gst_va_video_format_from_va_fourcc (attribs[i].value.value.i);
        if (format != GST_VIDEO_FORMAT_UNKNOWN
            && format_is_accepted (self, format))
          g_array_append_val (surface_formats, format);
        break;
      case VASurfaceAttribMinWidth:
        self->min_width = MAX (self->min_width, attribs[i].value.value.i);
        break;
      case VASurfaceAttribMaxWidth:
        if (self->max_width > 0)
          self->max_width = MIN (self->max_width, attribs[i].value.value.i);
        else
          self->max_width = attribs[i].value.value.i;
        break;
      case VASurfaceAttribMinHeight:
        self->min_height = MAX (self->min_height, attribs[i].value.value.i);
        break;
      case VASurfaceAttribMaxHeight:
        if (self->max_height > 0)
          self->max_height = MIN (self->max_height, attribs[i].value.value.i);
        else
          self->max_height = attribs[i].value.value.i;
        break;
      case VASurfaceAttribMemoryType:
        self->mem_types = attribs[i].value.value.i;
        break;
      default:
        break;
    }
  }

  if (surface_formats->len == 0)
    g_clear_pointer (&surface_formats, g_array_unref);

  self->surface_formats = surface_formats;

  g_free (attribs);

  return TRUE;
}

static gboolean
gst_va_filter_ensure_pipeline_caps (GstVaFilter * self)
{
  VADisplay dpy;
  VAStatus status;

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaQueryVideoProcPipelineCaps (dpy, self->context, NULL, 0,
      &self->pipeline_caps);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaQueryVideoProcPipelineCaps: %s",
        vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

/* Not thread-safe API */
gboolean
gst_va_filter_open (GstVaFilter * self)
{
  VAConfigAttrib attrib = {
    .type = VAConfigAttribRTFormat,
  };
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (gst_va_filter_is_open (self))
    return TRUE;

  if (!gst_va_filter_ensure_config_attributes (self, &attrib.value))
    return FALSE;

  self->image_formats = gst_va_display_get_image_formats (self->display);
  if (!self->image_formats)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaCreateConfig (dpy, VAProfileNone, VAEntrypointVideoProc, &attrib,
      1, &self->config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateConfig: %s", vaErrorStr (status));
    return FALSE;
  }

  if (!gst_va_filter_ensure_surface_attributes (self))
    goto bail;

  status = vaCreateContext (dpy, self->config, 0, 0, 0, NULL, 0,
      &self->context);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateContext: %s", vaErrorStr (status));
    goto bail;
  }

  if (!gst_va_filter_ensure_pipeline_caps (self)) {
    vaDestroyContext (dpy, self->context);
    goto bail;
  }

  return TRUE;

bail:
  {
    status = vaDestroyConfig (dpy, self->config);

    return FALSE;
  }
}

/* Not thread-safe API */
gboolean
gst_va_filter_close (GstVaFilter * self)
{
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return TRUE;

  dpy = gst_va_display_get_va_dpy (self->display);

  if (self->context != VA_INVALID_ID) {
    status = vaDestroyContext (dpy, self->context);
    if (status != VA_STATUS_SUCCESS)
      GST_ERROR_OBJECT (self, "vaDestroyContext: %s", vaErrorStr (status));
  }

  status = vaDestroyConfig (dpy, self->config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaDestroyConfig: %s", vaErrorStr (status));
    return FALSE;
  }

  g_clear_pointer (&self->available_filters, g_array_unref);
  g_clear_pointer (&self->filters, g_array_unref);

  gst_va_filter_init (self);

  return TRUE;
}

/* *INDENT-OFF* */
static const struct VaFilterCapMap {
  VAProcFilterType type;
  guint count;
  const char *name;
} filter_cap_map[] = {
#define F(name, count) { G_PASTE (VAProcFilter, name), count, G_STRINGIFY (name) }
  F(NoiseReduction, 1),
  F(Deinterlacing, VAProcDeinterlacingCount),
  F(Sharpening, 1),
  F(ColorBalance, VAProcColorBalanceCount),
  F(SkinToneEnhancement, 1),
  F(TotalColorCorrection, VAProcTotalColorCorrectionCount),
  F(HVSNoiseReduction, 0),
  F(HighDynamicRangeToneMapping, VAProcHighDynamicRangeMetadataTypeCount),
  F(3DLUT, 16),
#undef F
};
/* *INDENT-ON* */

static const struct VaFilterCapMap *
gst_va_filter_get_filter_cap (VAProcFilterType type)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (filter_cap_map); i++) {
    if (filter_cap_map[i].type == type)
      return &filter_cap_map[i];
  }

  return NULL;
}

static guint
gst_va_filter_get_filter_cap_count (VAProcFilterType type)
{
  const struct VaFilterCapMap *map = gst_va_filter_get_filter_cap (type);
  return map ? map->count : 0;
}

struct VaFilter
{
  VAProcFilterType type;
  guint num_caps;
  union
  {
    VAProcFilterCap simple;
    VAProcFilterCapDeinterlacing deint[VAProcDeinterlacingCount];
    VAProcFilterCapColorBalance cb[VAProcColorBalanceCount];
    VAProcFilterCapTotalColorCorrection cc[VAProcTotalColorCorrectionCount];
      VAProcFilterCapHighDynamicRange
        hdr[VAProcHighDynamicRangeMetadataTypeCount];
    VAProcFilterCap3DLUT lut[16];
  } caps;
};

static gboolean
gst_va_filter_ensure_filters (GstVaFilter * self)
{
  GArray *filters;
  VADisplay dpy;
  VAProcFilterType *filter_types;
  VAStatus status;
  guint i, num = VAProcFilterCount;
  gboolean ret = FALSE;

  GST_OBJECT_LOCK (self);
  if (self->available_filters) {
    GST_OBJECT_UNLOCK (self);
    return TRUE;
  }
  GST_OBJECT_UNLOCK (self);

  filter_types = g_malloc_n (num, sizeof (*filter_types));

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaQueryVideoProcFilters (dpy, self->context, filter_types, &num);
  if (status == VA_STATUS_ERROR_MAX_NUM_EXCEEDED) {
    filter_types = g_try_realloc_n (filter_types, num, sizeof (*filter_types));
    status = vaQueryVideoProcFilters (dpy, self->context, filter_types, &num);
  }
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaQueryVideoProcFilters: %s", vaErrorStr (status));
    goto bail;
  }

  if (num == 0)
    goto bail;

  filters = g_array_sized_new (FALSE, FALSE, sizeof (struct VaFilter), num);

  for (i = 0; i < num; i++) {
    guint num_caps = gst_va_filter_get_filter_cap_count (filter_types[i]);
    struct VaFilter filter = { filter_types[i], num_caps, {{{0,}}} };

    if (num_caps > 0) {
      status = vaQueryVideoProcFilterCaps (dpy, self->context, filter.type,
          &filter.caps, &filter.num_caps);
      if (status != VA_STATUS_SUCCESS) {
        GST_WARNING_OBJECT (self, "vaQueryVideoProcFiltersCaps: %s",
            vaErrorStr (status));
        continue;
      }
    }

    g_array_append_val (filters, filter);
  }

  GST_OBJECT_LOCK (self);
  g_clear_pointer (&self->available_filters, g_array_unref);
  self->available_filters = filters;
  GST_OBJECT_UNLOCK (self);

  ret = TRUE;

bail:
  g_free (filter_types);

  return ret;
}

/* *INDENT-OFF* */
static const struct _CBDesc {
  const char *name;
  const char *nick;
  const char *blurb;
  guint prop_id;
} cb_desc[VAProcColorBalanceCount] = {
  [VAProcColorBalanceHue] =
      { "hue", "Hue", "Color hue value", GST_VA_FILTER_PROP_HUE },
  [VAProcColorBalanceSaturation] =
      { "saturation", "Saturation", "Color saturation value",
        GST_VA_FILTER_PROP_SATURATION },
  [VAProcColorBalanceBrightness] =
      { "brightness", "Brightness", "Color brightness value",
        GST_VA_FILTER_PROP_BRIGHTNESS },
  [VAProcColorBalanceContrast] =
      { "contrast", "Contrast", "Color contrast value",
        GST_VA_FILTER_PROP_CONTRAST },
  [VAProcColorBalanceAutoSaturation] =
      { "auto-saturation",   "Auto-Saturation", "Enable auto saturation",
        GST_VA_FILTER_PROP_AUTO_SATURATION },
  [VAProcColorBalanceAutoBrightness] =
      { "auto-brightness", "Auto-Brightness", "Enable auto brightness",
        GST_VA_FILTER_PROP_AUTO_BRIGHTNESS    },
  [VAProcColorBalanceAutoContrast] =
      { "auto-contrast", "Auto-Contrast", "Enable auto contrast",
        GST_VA_FILTER_PROP_AUTO_CONTRAST },
};
/* *INDENT-ON* */

gboolean
gst_va_filter_install_properties (GstVaFilter * self, GObjectClass * klass)
{
  guint i;
  const GParamFlags common_flags = G_PARAM_READWRITE
      | GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS
      | GST_PARAM_MUTABLE_PLAYING | GST_PARAM_CONTROLLABLE;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!gst_va_filter_ensure_filters (self))
    return FALSE;

  for (i = 0; i < self->available_filters->len; i++) {
    const struct VaFilter *filter =
        &g_array_index (self->available_filters, struct VaFilter, i);

    switch (filter->type) {
      case VAProcFilterNoiseReduction:{
        const VAProcFilterCap *caps = &filter->caps.simple;

        g_object_class_install_property (klass, GST_VA_FILTER_PROP_DENOISE,
            g_param_spec_float ("denoise", "Noise reduction",
                "Noise reduction factor", caps->range.min_value,
                caps->range.max_value, caps->range.default_value,
                common_flags));
        break;
      }
      case VAProcFilterSharpening:{
        const VAProcFilterCap *caps = &filter->caps.simple;

        g_object_class_install_property (klass, GST_VA_FILTER_PROP_SHARPEN,
            g_param_spec_float ("sharpen", "Sharpening Level",
                "Sharpening/blurring filter", caps->range.min_value,
                caps->range.max_value, caps->range.default_value,
                common_flags));
        break;
      }
      case VAProcFilterSkinToneEnhancement:{
        const VAProcFilterCap *caps = &filter->caps.simple;
        GParamSpec *pspec;

        /* i965 filter */
        if (filter->num_caps == 0) {
          pspec = g_param_spec_boolean ("skin-tone", "Skin Tone Enhancenment",
              "Skin Tone Enhancenment filter", FALSE, common_flags);
        } else {
          pspec = g_param_spec_float ("skin-tone", "Skin Tone Enhancenment",
              "Skin Tone Enhancenment filter", caps->range.min_value,
              caps->range.max_value, caps->range.default_value, common_flags);
        }

        g_object_class_install_property (klass, GST_VA_FILTER_PROP_SKINTONE,
            pspec);
        break;
      }
      case VAProcFilterColorBalance:{
        const VAProcFilterCapColorBalance *caps = filter->caps.cb;
        GParamSpec *pspec;
        guint j, k;

        for (j = 0; j < filter->num_caps; j++) {
          k = caps[j].type;
          if (caps[j].range.min_value < caps[j].range.max_value) {
            pspec = g_param_spec_float (cb_desc[k].name, cb_desc[k].nick,
                cb_desc[k].blurb, caps[j].range.min_value,
                caps[j].range.max_value, caps[j].range.default_value,
                common_flags);
          } else {
            pspec = g_param_spec_boolean (cb_desc[k].name, cb_desc[k].nick,
                cb_desc[k].blurb, FALSE, common_flags);
          }

          g_object_class_install_property (klass, cb_desc[k].prop_id, pspec);
        }

        break;
      }
      case VAProcFilterHighDynamicRangeToneMapping:{
        guint j;
        for (j = 0; j < filter->num_caps; j++) {
          const VAProcFilterCapHighDynamicRange *caps = &filter->caps.hdr[j];
          if (caps->metadata_type == VAProcHighDynamicRangeMetadataHDR10
              && (caps->caps_flag & VA_TONE_MAPPING_HDR_TO_SDR)) {
            g_object_class_install_property (klass, GST_VA_FILTER_PROP_HDR,
                g_param_spec_boolean ("hdr-tone-mapping", "HDR tone mapping",
                    "Enable HDR to SDR tone mapping", FALSE, common_flags));
            break;
          }
        }
      }
      default:
        break;
    }
  }

  if (self->pipeline_caps.mirror_flags != VA_MIRROR_NONE
      || self->pipeline_caps.rotation_flags != VA_ROTATION_NONE) {
    g_object_class_install_property (klass, GST_VA_FILTER_PROP_VIDEO_DIR,
        g_param_spec_enum ("video-direction", "Video Direction",
            "Video direction: rotation and flipping",
            GST_TYPE_VIDEO_ORIENTATION_METHOD, GST_VIDEO_ORIENTATION_IDENTITY,
            common_flags));
  }

  return TRUE;
}

/**
 * GstVaDeinterlaceMethods:
 * @GST_VA_DEINTERLACE_BOB: Interpolating missing lines by using the
 *   adjacent lines.
 * @GST_VA_DEINTERLACE_WEAVE: Show both fields per frame. (don't use)
 * @GST_VA_DEINTERLACE_ADAPTIVE: Interpolating missing lines by using
 *   spatial/temporal references.
 * @GST_VA_DEINTERLACE_COMPENSATED: Recreating missing lines by using
 *   motion vector.
 *
 * Since: 1.20
 */
/* *INDENT-OFF* */
static const GEnumValue di_desc[] = {
  [GST_VA_DEINTERLACE_BOB] =
      { VAProcDeinterlacingBob,
        "Bob: Interpolating missing lines by using the adjacent lines.", "bob" },
  [GST_VA_DEINTERLACE_WEAVE] =
      { VAProcDeinterlacingWeave, "Weave: Show both fields per frame. (don't use)",
        "weave" },
  [GST_VA_DEINTERLACE_ADAPTIVE] =
      { VAProcDeinterlacingMotionAdaptive,
        "Adaptive: Interpolating missing lines by using spatial/temporal references.",
        "adaptive" },
  [GST_VA_DEINTERLACE_COMPENSATED] =
      { VAProcDeinterlacingMotionCompensated,
        "Compensation: Recreating missing lines by using motion vector.",
        "compensated" },
};
/* *INDENT-ON* */

static GType
gst_va_deinterlace_methods_get_type (guint num_caps,
    const VAProcFilterCapDeinterlacing * caps)
{
  guint i, j = 0;
  static GType deinterlace_methods_type = 0;
  static GEnumValue methods_types[VAProcDeinterlacingCount];

  if (deinterlace_methods_type > 0)
    return deinterlace_methods_type;

  for (i = 0; i < num_caps; i++) {
    if (caps[i].type > VAProcDeinterlacingNone
        && caps[i].type < VAProcDeinterlacingCount)
      methods_types[j++] = di_desc[caps[i].type];
  }

  /* *INDENT-OFF* */
  methods_types[j] = (GEnumValue) { 0, NULL, NULL };
  /* *INDENT-ON* */

  deinterlace_methods_type = g_enum_register_static ("GstVaDeinterlaceMethods",
      (const GEnumValue *) methods_types);

  return deinterlace_methods_type;
}

gboolean
gst_va_filter_install_deinterlace_properties (GstVaFilter * self,
    GObjectClass * klass)
{
  GType type;
  guint i;
  const GParamFlags common_flags = G_PARAM_READWRITE
      | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!gst_va_filter_ensure_filters (self))
    return FALSE;

  for (i = 0; i < self->available_filters->len; i++) {
    const struct VaFilter *filter =
        &g_array_index (self->available_filters, struct VaFilter, i);

    if (filter->type == VAProcFilterDeinterlacing) {
      guint i, default_method = 0;
      const VAProcFilterCapDeinterlacing *caps = filter->caps.deint;
      if (!caps)
        break;

      /* use the first method in the list as default */
      for (i = 0; i < filter->num_caps; i++) {
        if (caps[i].type > VAProcDeinterlacingNone
            && caps[i].type < VAProcDeinterlacingCount) {
          default_method = caps[i].type;
          break;
        }
      }

      if (default_method == 0)
        break;

      type = gst_va_deinterlace_methods_get_type (filter->num_caps, caps);
      gst_type_mark_as_plugin_api (type, 0);

      /**
       * GstVaDeinterlace:method
       *
       * Selects the different deinterlacing algorithms that can be used.
       *
       * It depends on the driver the number of available algorithms,
       * and they provide different quality and different processing
       * consumption.
       *
       * Since: 1.20
       */
      g_object_class_install_property (klass,
          GST_VA_FILTER_PROP_DEINTERLACE_METHOD,
          g_param_spec_enum ("method", "Method", "Deinterlace Method",
              type, default_method, common_flags));

      return TRUE;
    }
  }

  return FALSE;
}

gboolean
gst_va_filter_has_filter (GstVaFilter * self, VAProcFilterType type)
{
  guint i;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!gst_va_filter_ensure_filters (self))
    return FALSE;

  for (i = 0; i < self->available_filters->len; i++) {
    const struct VaFilter *filter =
        &g_array_index (self->available_filters, struct VaFilter, i);

    if (filter->type == type)
      return TRUE;
  }

  return FALSE;
}

const gpointer
gst_va_filter_get_filter_caps (GstVaFilter * self, VAProcFilterType type,
    guint * num_caps)
{
  struct VaFilter *filter = NULL;
  /* *INDENT-OFF* */
  static const VAProcFilterCap i965_ste_caps = {
    .range = {
      .min_value = 0.0,
      .max_value = 1.0,
      .default_value = 0.0,
      .step = 1.0,
    },
  };
  /* *INDENT-ON* */
  gpointer ret = NULL;
  guint i;

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!gst_va_filter_ensure_filters (self))
    return FALSE;

  GST_OBJECT_LOCK (self);
  for (i = 0; i < self->available_filters->len; i++) {
    filter = &g_array_index (self->available_filters, struct VaFilter, i);

    if (filter->type == type) {
      if (filter->num_caps > 0)
        ret = &filter->caps;
      else if (type == VAProcFilterSkinToneEnhancement && filter->num_caps == 0)
        ret = (gpointer) & i965_ste_caps;
      break;
    }
  }

  if (ret && filter && num_caps)
    *num_caps = filter->num_caps;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

guint32
gst_va_filter_get_mem_types (GstVaFilter * self)
{
  guint32 ret;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), 0);

  GST_OBJECT_LOCK (self);
  ret = self->mem_types;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

GArray *
gst_va_filter_get_surface_formats (GstVaFilter * self)
{
  GArray *ret;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), NULL);

  GST_OBJECT_LOCK (self);
  ret = self->surface_formats ? g_array_ref (self->surface_formats) : NULL;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

gboolean
gst_va_filter_set_scale_method (GstVaFilter * self, guint32 method)
{
  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  GST_OBJECT_LOCK (self);
  self->scale_method = method;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_from_video_orientation_method (GstVideoOrientationMethod orientation,
    guint * mirror, guint * rotation)
{
  switch (orientation) {
    case GST_VIDEO_ORIENTATION_IDENTITY:
      *mirror = VA_MIRROR_NONE;
      *rotation = VA_ROTATION_NONE;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      *mirror = VA_MIRROR_HORIZONTAL;
      *rotation = VA_ROTATION_NONE;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      *mirror = VA_MIRROR_VERTICAL;
      *rotation = VA_ROTATION_NONE;
      break;
    case GST_VIDEO_ORIENTATION_90R:
      *mirror = VA_MIRROR_NONE;
      *rotation = VA_ROTATION_90;
      break;
    case GST_VIDEO_ORIENTATION_180:
      *mirror = VA_MIRROR_NONE;
      *rotation = VA_ROTATION_180;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      *mirror = VA_MIRROR_NONE;
      *rotation = VA_ROTATION_270;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      *mirror = VA_MIRROR_HORIZONTAL;
      *rotation = VA_ROTATION_90;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      *mirror = VA_MIRROR_VERTICAL;
      *rotation = VA_ROTATION_90;
      break;
    default:
      return FALSE;
      break;
  }

  return TRUE;
}

gboolean
gst_va_filter_set_orientation (GstVaFilter * self,
    GstVideoOrientationMethod orientation)
{
  guint32 mirror = VA_MIRROR_NONE, rotation = VA_ROTATION_NONE;
  guint32 mirror_flags, rotation_flags;

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!_from_video_orientation_method (orientation, &mirror, &rotation))
    return FALSE;

  GST_OBJECT_LOCK (self);
  mirror_flags = self->pipeline_caps.mirror_flags;
  GST_OBJECT_UNLOCK (self);

  if (mirror != VA_MIRROR_NONE && !(mirror_flags & mirror))
    return FALSE;

  GST_OBJECT_LOCK (self);
  rotation_flags = self->pipeline_caps.rotation_flags;
  GST_OBJECT_UNLOCK (self);

  if (rotation != VA_ROTATION_NONE && !(rotation_flags & (1 << rotation)))
    return FALSE;

  GST_OBJECT_LOCK (self);
  self->orientation = orientation;
  self->mirror = mirror;
  self->rotation = rotation;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

GstVideoOrientationMethod
gst_va_filter_get_orientation (GstVaFilter * self)
{
  GstVideoOrientationMethod ret;

  GST_OBJECT_LOCK (self);
  ret = self->orientation;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

void
gst_va_filter_enable_cropping (GstVaFilter * self, gboolean cropping)
{
  GST_OBJECT_LOCK (self);
  if (cropping != self->crop_enabled)
    self->crop_enabled = cropping;
  GST_OBJECT_UNLOCK (self);
}

static inline GstCaps *
_create_base_caps (GstVaFilter * self)
{
  return gst_caps_new_simple ("video/x-raw", "width", GST_TYPE_INT_RANGE,
      self->min_width, self->max_width, "height", GST_TYPE_INT_RANGE,
      self->min_height, self->max_height, NULL);
}

GstCaps *
gst_va_filter_get_caps (GstVaFilter * self)
{
  GArray *surface_formats = NULL, *image_formats = NULL;
  GstCaps *caps, *base_caps, *feature_caps;
  GstCapsFeatures *features;
  guint32 mem_types;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), NULL);

  if (!gst_va_filter_is_open (self))
    return NULL;

  surface_formats = gst_va_filter_get_surface_formats (self);
  if (!surface_formats)
    return NULL;

  base_caps = _create_base_caps (self);

  if (!gst_caps_set_format_array (base_caps, surface_formats))
    goto fail;

  g_array_unref (surface_formats);

  caps = gst_caps_new_empty ();

  mem_types = gst_va_filter_get_mem_types (self);

  if (mem_types & VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
    feature_caps = gst_caps_copy (base_caps);
    features = gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA);
    gst_caps_set_features_simple (feature_caps, features);
    caps = gst_caps_merge (caps, feature_caps);
  }
  if (mem_types & VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
      || mem_types & VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2) {
    feature_caps = gst_va_create_dma_caps (self->display,
        VAEntrypointVideoProc, surface_formats,
        self->min_width, self->max_width, self->min_height, self->max_height);
    if (feature_caps)
      caps = gst_caps_merge (caps, feature_caps);
  }

  gst_caps_unref (base_caps);

  base_caps = _create_base_caps (self);

  GST_OBJECT_LOCK (self);
  image_formats =
      self->image_formats ? g_array_ref (self->image_formats) : NULL;
  GST_OBJECT_UNLOCK (self);

  if (image_formats) {
    if (!gst_caps_set_format_array (base_caps, image_formats))
      goto fail;
    g_array_unref (image_formats);
  }

  return gst_caps_merge (caps, base_caps);

fail:
  {
    g_clear_pointer (&surface_formats, g_array_unref);
    g_clear_pointer (&image_formats, g_array_unref);
    gst_caps_unref (base_caps);
    return NULL;
  }
}

/* from va_vpp.h */
/* *INDENT-OFF* */
static const struct ColorPropertiesMap
{
  VAProcColorStandardType standard;
  guint8 primaries;
  guint8 transfer;
  guint8 matrix;
} color_properties_map[] = {
  { VAProcColorStandardBT601, 5,  6,  5 },
  { VAProcColorStandardBT601, 6,  6,  6 },
  { VAProcColorStandardBT709, 1,  1,  1 },
  { VAProcColorStandardBT470M, 4,  4,  4 },
  { VAProcColorStandardBT470BG, 5,  5,  5 },
  { VAProcColorStandardSMPTE170M, 6,  6,  6 },
  { VAProcColorStandardSMPTE240M, 7,  7,  7 },
  { VAProcColorStandardGenericFilm, 8,  1,  1 },
  { VAProcColorStandardSRGB, 1, 13,  0 },
  /* { VAProcColorStandardSTRGB, ?, ?, ? }, */
  { VAProcColorStandardXVYCC601, 1, 11,  5 },
  { VAProcColorStandardXVYCC709, 1, 11,  1 },
  { VAProcColorStandardBT2020, 9, 14,  9 },
};
  /* *INDENT-ON* */

static guint8
_get_chroma_siting (GstVideoChromaSite chrome_site)
{
  /* *INDENT-OFF* */
  static const struct ChromaSiteMap {
    GstVideoChromaSite gst;
    guint8 va;
  } chroma_site_map[] = {
    { GST_VIDEO_CHROMA_SITE_UNKNOWN, VA_CHROMA_SITING_UNKNOWN },
    { GST_VIDEO_CHROMA_SITE_NONE, VA_CHROMA_SITING_VERTICAL_CENTER
                                  | VA_CHROMA_SITING_HORIZONTAL_CENTER },
    { GST_VIDEO_CHROMA_SITE_H_COSITED, VA_CHROMA_SITING_VERTICAL_CENTER
                                       | VA_CHROMA_SITING_HORIZONTAL_LEFT },
    { GST_VIDEO_CHROMA_SITE_V_COSITED, VA_CHROMA_SITING_VERTICAL_TOP
                                       | VA_CHROMA_SITING_VERTICAL_BOTTOM },
    { GST_VIDEO_CHROMA_SITE_COSITED, VA_CHROMA_SITING_VERTICAL_CENTER
                                     | VA_CHROMA_SITING_HORIZONTAL_LEFT
                                     | VA_CHROMA_SITING_VERTICAL_TOP
                                     | VA_CHROMA_SITING_VERTICAL_BOTTOM },
    { GST_VIDEO_CHROMA_SITE_JPEG, VA_CHROMA_SITING_VERTICAL_CENTER
                                  | VA_CHROMA_SITING_HORIZONTAL_CENTER  },
    { GST_VIDEO_CHROMA_SITE_MPEG2, VA_CHROMA_SITING_VERTICAL_CENTER
                                   | VA_CHROMA_SITING_HORIZONTAL_LEFT },
    { GST_VIDEO_CHROMA_SITE_DV, VA_CHROMA_SITING_VERTICAL_TOP
                                | VA_CHROMA_SITING_HORIZONTAL_LEFT },
  };
  /* *INDENT-ON* */
  guint i;

  for (i = 0; i < G_N_ELEMENTS (chroma_site_map); i++) {
    if (chrome_site == chroma_site_map[i].gst)
      return chroma_site_map[i].va;
  }

  return VA_CHROMA_SITING_UNKNOWN;
}

static guint8
_get_color_range (GstVideoColorRange range)
{
  /* *INDENT-OFF* */
  static const struct ColorRangeMap {
    GstVideoColorRange gst;
    guint8 va;
  } color_range_map[] = {
    { GST_VIDEO_COLOR_RANGE_UNKNOWN, VA_SOURCE_RANGE_UNKNOWN },
    { GST_VIDEO_COLOR_RANGE_0_255, VA_SOURCE_RANGE_FULL },
    { GST_VIDEO_COLOR_RANGE_16_235, VA_SOURCE_RANGE_REDUCED },
  };
  /* *INDENT-ON* */
  guint i;

  for (i = 0; i < G_N_ELEMENTS (color_range_map); i++) {
    if (range == color_range_map[i].gst)
      return color_range_map[i].va;
  }

  return VA_SOURCE_RANGE_UNKNOWN;
}

static VAProcColorStandardType
_gst_video_colorimetry_to_va (const GstVideoColorimetry * const colorimetry)
{
  if (!colorimetry
      || colorimetry->primaries == GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
    return VAProcColorStandardNone;

  if (gst_video_colorimetry_matches (colorimetry, GST_VIDEO_COLORIMETRY_BT709))
    return VAProcColorStandardBT709;

  /* NOTE: VAProcColorStandardBT2020 in VAAPI is the same as
   * GST_VIDEO_COLORIMETRY_BT2020_10 in gstreamer. */
  if (gst_video_colorimetry_matches (colorimetry,
          GST_VIDEO_COLORIMETRY_BT2020_10) ||
      gst_video_colorimetry_matches (colorimetry, GST_VIDEO_COLORIMETRY_BT2020))
    return VAProcColorStandardBT2020;

  if (gst_video_colorimetry_matches (colorimetry, GST_VIDEO_COLORIMETRY_BT601))
    return VAProcColorStandardBT601;

  if (gst_video_colorimetry_matches (colorimetry,
          GST_VIDEO_COLORIMETRY_SMPTE240M))
    return VAProcColorStandardSMPTE240M;

  if (gst_video_colorimetry_matches (colorimetry, GST_VIDEO_COLORIMETRY_SRGB))
    return VAProcColorStandardSRGB;

  return VAProcColorStandardNone;
}

static void
_config_color_properties (VAProcColorStandardType * std,
    VAProcColorProperties * props, const GstVideoInfo * info,
    VAProcColorStandardType * standards, guint32 num_standards)
{
  GstVideoColorimetry colorimetry = GST_VIDEO_INFO_COLORIMETRY (info);
  VAProcColorStandardType best;
  gboolean has_explicit;
  guint i, j, k;
  gint score, bestscore = -1, worstscore;

  best = _gst_video_colorimetry_to_va (&colorimetry);

  has_explicit = FALSE;
  for (i = 0; i < num_standards; i++) {
    /* Find the exact match standard. */
    if (standards[i] != VAProcColorStandardNone && standards[i] == best)
      break;

    if (standards[i] == VAProcColorStandardExplicit)
      has_explicit = TRUE;
  }

  if (i < num_standards) {
    *std = best;
    goto set_properties;
  } else if (has_explicit) {
    *std = VAProcColorStandardExplicit;
    goto set_properties;
  }

  worstscore = 4 * (colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_UNKNOWN
      && colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB)
      + 2 * (colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN)
      + (colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN);

  if (worstscore == 0) {
    /* No properties specified, there's not a useful choice. */
    *std = VAProcColorStandardNone;
    memset (props, 0, sizeof (VAProcColorProperties));
    return;
  }

  best = VAProcColorStandardNone;
  k = -1;
  for (i = 0; i < num_standards; i++) {
    for (j = 0; j < G_N_ELEMENTS (color_properties_map); j++) {
      if (color_properties_map[j].standard != standards[i])
        continue;

      score = 0;
      if (colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_UNKNOWN
          && colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB)
        score += 4 * (colorimetry.matrix != color_properties_map[j].matrix);
      if (colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN)
        score += 2 * (colorimetry.transfer != color_properties_map[j].transfer);
      if (colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
        score += (colorimetry.primaries != color_properties_map[j].primaries);

      if (score < worstscore && (bestscore == -1 || score < bestscore)) {
        bestscore = score;
        best = color_properties_map[j].standard;
        k = j;
      }
    }
  }

  if (best != VAProcColorStandardNone) {
    *std = best;
    colorimetry.matrix = color_properties_map[k].matrix;
    colorimetry.transfer = color_properties_map[k].transfer;
    colorimetry.primaries = color_properties_map[k].primaries;
  }

set_properties:
  /* *INDENT-OFF* */
  *props = (VAProcColorProperties) {
    .chroma_sample_location =
        _get_chroma_siting (GST_VIDEO_INFO_CHROMA_SITE (info)),
    .color_range = _get_color_range (colorimetry.range),
    .colour_primaries =
        gst_video_color_primaries_to_iso (colorimetry.primaries),
    .transfer_characteristics =
        gst_video_transfer_function_to_iso (colorimetry.transfer),
    .matrix_coefficients =
        gst_video_color_matrix_to_iso (colorimetry.matrix),
  };
  /* *INDENT-ON* */
}

gboolean
gst_va_filter_set_video_info (GstVaFilter * self, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);
  g_return_val_if_fail (out_info && in_info, FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  GST_OBJECT_LOCK (self);
  /* *INDENT-OFF* */
  self->input_region = (VARectangle) {
    .width = GST_VIDEO_INFO_WIDTH (in_info),
    .height = GST_VIDEO_INFO_HEIGHT (in_info),
  };

  self->output_region = (VARectangle) {
    .width = GST_VIDEO_INFO_WIDTH (out_info),
    .height = GST_VIDEO_INFO_HEIGHT (out_info),
  };
  /* *INDENT-ON* */

  _config_color_properties (&self->input_color_standard,
      &self->input_color_properties, in_info,
      self->pipeline_caps.input_color_standards,
      self->pipeline_caps.num_input_color_standards);
  _config_color_properties (&self->output_color_standard,
      &self->output_color_properties, out_info,
      self->pipeline_caps.output_color_standards,
      self->pipeline_caps.num_output_color_standards);
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_query_pipeline_caps (GstVaFilter * self, GArray * filters,
    VAProcPipelineCaps * caps)
{
  VABufferID *va_filters = NULL;
  VADisplay dpy;
  VAStatus status;
  guint32 num_filters = 0;

  GST_OBJECT_LOCK (self);
  if (filters) {
    num_filters = filters->len;
    va_filters = (num_filters > 0) ? (VABufferID *) filters->data : NULL;
  }
  GST_OBJECT_UNLOCK (self);

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaQueryVideoProcPipelineCaps (dpy, self->context, va_filters,
      num_filters, caps);

  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaQueryVideoProcPipelineCaps: %s",
        vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_va_filter_add_deinterlace_buffer (GstVaFilter * self,
    VAProcDeinterlacingType method, guint32 * forward, guint32 * backward)
{
  GArray *filters = NULL;
  VAProcFilterParameterBufferDeinterlacing params = {
    .type = VAProcFilterDeinterlacing,
    .algorithm = method,
  };
  VAProcPipelineCaps pipeline_caps = { 0, };
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!(method != VAProcDeinterlacingNone
          && method != VAProcDeinterlacingCount))
    return FALSE;

  if (!gst_va_filter_add_filter_buffer (self, &params, sizeof (params), 1))
    return FALSE;

  GST_OBJECT_LOCK (self);
  if (self->filters)
    filters = g_array_ref (self->filters);
  GST_OBJECT_UNLOCK (self);
  ret = _query_pipeline_caps (self, filters, &pipeline_caps);
  if (filters)
    g_array_unref (filters);
  if (!ret)
    return FALSE;

  if (forward)
    *forward = pipeline_caps.num_forward_references;
  if (backward)
    *backward = pipeline_caps.num_backward_references;

  return TRUE;
}

#ifndef GST_DISABLE_GST_DEBUG
static const gchar *
get_va_filter_name (gpointer data)
{
  VAProcFilterType type = ((VAProcFilterParameterBuffer *) data)->type;
  const struct VaFilterCapMap *m = gst_va_filter_get_filter_cap (type);

  return m ? m->name : "Unknown";
}
#endif

gboolean
gst_va_filter_add_filter_buffer (GstVaFilter * self, gpointer data, gsize size,
    guint num)
{
  VABufferID buffer;
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);
  g_return_val_if_fail (data && size > 0, FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, self->context, VAProcFilterParameterBufferType,
      size, num, data, &buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Added filter: %s", get_va_filter_name (data));

  /* lazy creation */
  GST_OBJECT_LOCK (self);
  if (!self->filters)
    self->filters = g_array_sized_new (FALSE, FALSE, sizeof (VABufferID), 16);

  g_array_append_val (self->filters, buffer);
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_destroy_filters_unlocked (GstVaFilter * self)
{
  VABufferID buffer;
  VADisplay dpy;
  VAStatus status;
  gboolean ret = TRUE;
  guint i;

  GST_TRACE_OBJECT (self, "Destroying %u filter buffers", self->filters->len);

  dpy = gst_va_display_get_va_dpy (self->display);

  for (i = 0; i < self->filters->len; i++) {
    buffer = g_array_index (self->filters, VABufferID, i);

    status = vaDestroyBuffer (dpy, buffer);
    if (status != VA_STATUS_SUCCESS) {
      ret = FALSE;
      GST_WARNING_OBJECT (self, "Failed to destroy filter buffer: %s",
          vaErrorStr (status));
    }
  }

  self->filters = g_array_set_size (self->filters, 0);

  return ret;
}

gboolean
gst_va_filter_drop_filter_buffers (GstVaFilter * self)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  GST_OBJECT_LOCK (self);
  if (self->filters)
    ret = _destroy_filters_unlocked (self);
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static VASurfaceID
_get_surface_from_buffer (GstVaFilter * self, GstBuffer * buffer)
{
  VASurfaceID surface = VA_INVALID_ID;

  if (buffer)
    surface = gst_va_buffer_get_surface (buffer);

  if (surface != VA_INVALID_ID) {
    /* @FIXME: in gallium vaQuerySurfaceStatus only seems to work with
     * encoder's surfaces */
    if (!GST_VA_DISPLAY_IS_IMPLEMENTATION (self->display, MESA_GALLIUM))
      if (!va_check_surface (self->display, surface))
        surface = VA_INVALID_ID;
  }

  return surface;
}

static gboolean
_fill_va_sample (GstVaFilter * self, GstVaSample * sample,
    GstPadDirection direction)
{
  GstVideoCropMeta *crop = NULL;

  sample->surface = _get_surface_from_buffer (self, sample->buffer);
  if (sample->surface == VA_INVALID_ID)
    return FALSE;

  /* XXX: cropping occurs only in input frames */
  if (direction == GST_PAD_SRC) {
    GST_OBJECT_LOCK (self);
    sample->rect = self->output_region;
    sample->rect.x = sample->borders_w / 2;
    sample->rect.y = sample->borders_h / 2;
    sample->rect.width -= sample->borders_w;
    sample->rect.height -= sample->borders_h;
    GST_OBJECT_UNLOCK (self);

    return TRUE;
  }

  /* if buffer has crop meta, its real size is in video meta */
  if (sample->buffer)
    crop = gst_buffer_get_video_crop_meta (sample->buffer);

  GST_OBJECT_LOCK (self);
  if (crop && self->crop_enabled) {
    /* *INDENT-OFF* */
    sample->rect = (VARectangle) {
      .x = crop->x,
      .y = crop->y,
      .width = crop->width,
      .height = crop->height
    };
    /* *INDENT-ON* */
  } else {
    sample->rect = self->input_region;
  }
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_create_pipeline_buffer (GstVaFilter * self, GstVaSample * src,
    GstVaSample * dst, GArray * filters, VABufferID * buffer)
{
  VADisplay dpy;
  VAStatus status;
  VABufferID *va_filters = NULL;
  VAProcPipelineParameterBuffer params;
  guint32 num_filters = 0;

  GST_OBJECT_LOCK (self);

  /* *INDENT-OFF* */
  if (filters) {
    num_filters = filters->len;
    va_filters = (num_filters > 0) ? (VABufferID *) filters->data : NULL;
  }
  params = (VAProcPipelineParameterBuffer) {
    .surface = src->surface,
    .surface_region = &src->rect,
    .surface_color_standard = self->input_color_standard,
    .output_region = &dst->rect,
    .output_background_color = 0xff000000, /* ARGB black */
    .output_color_standard = self->output_color_standard,
    .filters = va_filters,
    .num_filters = num_filters,
    .forward_references = src->forward_references,
    .num_forward_references = src->num_forward_references,
    .backward_references = src->backward_references,
    .num_backward_references = src->num_backward_references,
    .rotation_state = self->rotation,
    .mirror_state = self->mirror,
    .input_surface_flag = src->flags,
    .output_surface_flag = dst->flags,
    .input_color_properties = self->input_color_properties,
    .output_color_properties = self->output_color_properties,
    .filter_flags = self->scale_method,
    /* output to SDR */
    .output_hdr_metadata = NULL,
  };
  /* *INDENT-ON* */

  GST_OBJECT_UNLOCK (self);

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, self->context,
      VAProcPipelineParameterBufferType, sizeof (params), 1, &params, buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Created VABufferID %#x with %u filters: "
      "src %#x / dst %#x", *buffer, num_filters, src->surface, dst->surface);

  return TRUE;
}

gboolean
gst_va_filter_process (GstVaFilter * self, GstVaSample * src, GstVaSample * dst)
{
  GArray *filters = NULL;
  VABufferID buffer;
  VADisplay dpy;
  VAProcPipelineCaps pipeline_caps = { 0, };
  VAStatus status;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);
  g_return_val_if_fail (src, FALSE);
  g_return_val_if_fail (dst, FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  if (!(_fill_va_sample (self, src, GST_PAD_SINK)
          && _fill_va_sample (self, dst, GST_PAD_SRC)))
    return FALSE;

  GST_OBJECT_LOCK (self);
  if (self->filters)
    filters = g_array_ref (self->filters);
  GST_OBJECT_UNLOCK (self);

  if (!_query_pipeline_caps (self, filters, &pipeline_caps))
    return FALSE;

  if (!_create_pipeline_buffer (self, src, dst, filters, &buffer))
    return FALSE;

  if (filters)
    g_array_unref (filters);

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaBeginPicture (dpy, self->context, dst->surface);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaBeginPicture: %s", vaErrorStr (status));
    return FALSE;
  }

  status = vaRenderPicture (dpy, self->context, &buffer, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaRenderPicture: %s with buffer %#x",
        vaErrorStr (status), buffer);
    goto fail_end_pic;
  }

  status = vaEndPicture (dpy, self->context);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));
    goto bail;
  }

  ret = TRUE;

bail:
  status = vaDestroyBuffer (dpy, buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to destroy pipeline buffer: %s",
        vaErrorStr (status));
  }

  return ret;

fail_end_pic:
  {
    status = vaEndPicture (dpy, self->context);
    if (status != VA_STATUS_SUCCESS)
      GST_ERROR_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));
    goto bail;
  }
}

gboolean
gst_va_filter_has_compose (GstVaFilter * self)
{
  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  /* HACK(uartie): i965 can't do composition */
  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (self->display, INTEL_I965))
    return FALSE;

  /* some drivers can compose, but may not support blending (e.g. GALLIUM) */
#ifndef GST_DISABLE_GST_DEBUG
  if (!(self->pipeline_caps.blend_flags & VA_BLEND_GLOBAL_ALPHA))
    GST_WARNING_OBJECT (self, "VPP does not support alpha blending");
#endif

  return TRUE;
}

/**
 * gst_va_filter_compose:
 * @tx: the #GstVaComposeTransaction for input samples and output.
 *
 * Iterates over all inputs via #GstVaComposeTransaction:next and composes
 * them onto the #GstVaComposeTransaction:output.
 *
 * Only csc, scaling and blending filters are applied during composition.
 * All other filters are ignored here.  Use #gst_va_filter_process to apply
 * other filters.
 *
 * Returns: TRUE on successful compose, FALSE otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_filter_compose (GstVaFilter * self, GstVaComposeTransaction * tx)
{
  VADisplay dpy;
  VAStatus status;
  VASurfaceID out_surface;
  GstVaComposeSample *sample;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);
  g_return_val_if_fail (tx, FALSE);
  g_return_val_if_fail (tx->next, FALSE);
  g_return_val_if_fail (tx->output, FALSE);

  if (!gst_va_filter_is_open (self))
    return FALSE;

  out_surface = _get_surface_from_buffer (self, tx->output);
  if (out_surface == VA_INVALID_ID)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaBeginPicture (dpy, self->context, out_surface);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaBeginPicture: %s", vaErrorStr (status));
    return FALSE;
  }

  sample = tx->next (tx->user_data);
  for (; sample; sample = tx->next (tx->user_data)) {
    VAProcPipelineParameterBuffer params = { 0, };
    VABufferID buffer;
    VASurfaceID in_surface;
    VABlendState blend = { 0, };

    in_surface = _get_surface_from_buffer (self, sample->buffer);
    if (in_surface == VA_INVALID_ID)
      return FALSE;

    /* (transfer full), unref it */
    gst_buffer_unref (sample->buffer);

    GST_OBJECT_LOCK (self);
    /* *INDENT-OFF* */
    params = (VAProcPipelineParameterBuffer) {
      .surface = in_surface,
      .surface_region = &sample->input_region,
      .output_region = &sample->output_region,
      .output_background_color = 0xff000000,
      .filter_flags = self->scale_method,
    };
    /* *INDENT-ON* */
    GST_OBJECT_UNLOCK (self);

    /* only send blend state when sample is not fully opaque */
    if ((self->pipeline_caps.blend_flags & VA_BLEND_GLOBAL_ALPHA)
        && sample->alpha < 1.0) {
      /* *INDENT-OFF* */
      blend = (VABlendState) {
        .flags = VA_BLEND_GLOBAL_ALPHA,
        .global_alpha = sample->alpha,
      };
      /* *INDENT-ON* */
      params.blend_state = &blend;
    }

    status = vaCreateBuffer (dpy, self->context,
        VAProcPipelineParameterBufferType, sizeof (params), 1, &params,
        &buffer);
    if (status != VA_STATUS_SUCCESS) {
      GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
      goto fail_end_pic;
    }

    status = vaRenderPicture (dpy, self->context, &buffer, 1);
    vaDestroyBuffer (dpy, buffer);
    if (status != VA_STATUS_SUCCESS) {
      GST_ERROR_OBJECT (self, "vaRenderPicture: %s", vaErrorStr (status));
      goto fail_end_pic;
    }
  }

  status = vaEndPicture (dpy, self->context);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;

fail_end_pic:
  {
    status = vaEndPicture (dpy, self->context);
    if (status != VA_STATUS_SUCCESS)
      GST_ERROR_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));
    return FALSE;
  }
}

/**
 * gst_va_buffer_get_surface_flags:
 * @buffer: the #GstBuffer to check.
 * @info: the #GstVideoInfo with info.
 *
 * Gets the surface flags, related with interlace given @buffer and
 * @info.
 *
 * Returns: VA surface flags.
 */
guint32
gst_va_buffer_get_surface_flags (GstBuffer * buffer, GstVideoInfo * info)
{
  guint32 surface_flags = 0;

  if (GST_VIDEO_INFO_INTERLACE_MODE (info) == GST_VIDEO_INTERLACE_MODE_MIXED
      || (GST_VIDEO_INFO_INTERLACE_MODE (info) ==
          GST_VIDEO_INTERLACE_MODE_INTERLEAVED
          && GST_VIDEO_INFO_FIELD_ORDER (info) ==
          GST_VIDEO_FIELD_ORDER_UNKNOWN)) {
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
      if (GST_BUFFER_FLAG_IS_SET (buffer, GST_VIDEO_BUFFER_FLAG_TFF)) {
        surface_flags = VA_TOP_FIELD_FIRST;
      } else {
        surface_flags = VA_BOTTOM_FIELD_FIRST;
      }
    } else {
      surface_flags = VA_FRAME_PICTURE;
    }
  } else if (GST_VIDEO_INFO_FIELD_ORDER (info) ==
      GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST) {
    surface_flags = VA_BOTTOM_FIELD_FIRST;
  } else if (GST_VIDEO_INFO_FIELD_ORDER (info) ==
      GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST) {
    surface_flags = VA_TOP_FIELD_FIRST;
  }

  return surface_flags;
}

gboolean
gst_va_filter_has_video_format (GstVaFilter * self, GstVideoFormat format,
    GstCapsFeatures * feature)
{
  guint i;
  GstVideoFormat fmt;

  g_return_val_if_fail (GST_IS_VA_FILTER (self), FALSE);
  g_return_val_if_fail (format != GST_VIDEO_FORMAT_UNKNOWN, FALSE);
  g_return_val_if_fail (GST_IS_CAPS_FEATURES (feature)
      && !gst_caps_features_is_any (feature), FALSE);


  GST_OBJECT_LOCK (self);
  for (i = 0; i < self->surface_formats->len; i++) {
    fmt = g_array_index (self->surface_formats, GstVideoFormat, i);
    if (format == fmt) {
      GST_OBJECT_UNLOCK (self);
      return TRUE;
    }
  }
  GST_OBJECT_UNLOCK (self);

  if (!gst_caps_features_is_equal (feature,
          GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
    return FALSE;

  GST_OBJECT_LOCK (self);
  for (i = 0; i < self->image_formats->len; i++) {
    fmt = g_array_index (self->image_formats, GstVideoFormat, i);
    if (format == fmt) {
      GST_OBJECT_UNLOCK (self);
      return TRUE;
    }
  }
  GST_OBJECT_UNLOCK (self);

  return FALSE;
}

/**
 * GstVaScaleMethod:
 *
 * Since: 1.22
 */
GType
gst_va_scale_method_get_type (void)
{
  static gsize type = 0;
  static const GEnumValue values[] = {
    {VA_FILTER_SCALING_DEFAULT, "Default scaling method", "default"},
    {VA_FILTER_SCALING_FAST, "Fast scaling method", "fast"},
    {VA_FILTER_SCALING_HQ, "High quality scaling method", "hq"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&type)) {
    const GType _type = g_enum_register_static ("GstVaScaleMethod", values);
    g_once_init_leave (&type, _type);
  }
  return type;
}
