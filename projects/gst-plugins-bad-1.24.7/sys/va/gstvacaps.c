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

#include "gstvacaps.h"

#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>
#include <va/va_drmcommon.h>

#include "gstvadisplay_priv.h"
#include "gstvaprofile.h"

GST_DEBUG_CATEGORY_EXTERN (gstva_debug);
#define GST_CAT_DEFAULT gstva_debug

static const guint va_rt_format_list[] = {
#define R(name) G_PASTE (VA_RT_FORMAT_, name)
  R (YUV420),
  R (YUV422),
  R (YUV444),
  R (YUV411),
  R (YUV400),
  R (YUV420_10),
  R (YUV422_10),
  R (YUV444_10),
  R (YUV420_12),
  R (YUV422_12),
  R (YUV444_12),
  R (YUV420_10BPP),
  R (RGB16),
  R (RGB32),
  R (RGBP),
  R (RGB32_10),
  R (RGB32_10BPP),
  R (PROTECTED),
#undef R
};

VASurfaceAttrib *
gst_va_get_surface_attribs (GstVaDisplay * display, VAConfigID config,
    guint * attrib_count)
{
  VADisplay dpy;
  VASurfaceAttrib *attribs;
  VAStatus status;

  dpy = gst_va_display_get_va_dpy (display);

  status = vaQuerySurfaceAttributes (dpy, config, NULL, attrib_count);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaQuerySurfaceAttributes: %s",
        vaErrorStr (status));
    return NULL;
  }

  attribs = g_new (VASurfaceAttrib, *attrib_count);

  status = vaQuerySurfaceAttributes (dpy, config, attribs, attrib_count);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaQuerySurfaceAttributes: %s",
        vaErrorStr (status));
    goto bail;
  }

  return attribs;

bail:
  g_free (attribs);
  return NULL;
}

static inline void
_value_list_append_string (GValue * list, const gchar * str)
{
  GValue item = G_VALUE_INIT;

  g_value_init (&item, G_TYPE_STRING);
  g_value_set_string (&item, str);
  gst_value_list_append_value (list, &item);
  g_value_unset (&item);
}

gboolean
gst_caps_set_format_array (GstCaps * caps, GArray * formats)
{
  GstVideoFormat fmt;
  GValue v_formats = G_VALUE_INIT;
  const gchar *format;
  guint i;

  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (formats, FALSE);

  if (formats->len == 1) {
    fmt = g_array_index (formats, GstVideoFormat, 0);
    if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
      return FALSE;
    format = gst_video_format_to_string (fmt);
    if (!format)
      return FALSE;

    g_value_init (&v_formats, G_TYPE_STRING);
    g_value_set_string (&v_formats, format);
  } else if (formats->len > 1) {

    gst_value_list_init (&v_formats, formats->len);

    for (i = 0; i < formats->len; i++) {
      fmt = g_array_index (formats, GstVideoFormat, i);
      if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
        continue;
      format = gst_video_format_to_string (fmt);
      if (!format)
        continue;

      _value_list_append_string (&v_formats, format);
    }
  } else {
    return FALSE;
  }

  gst_caps_set_value (caps, "format", &v_formats);
  g_value_unset (&v_formats);

  return TRUE;
}

static gboolean
gst_caps_set_drm_format_array (GstCaps * caps, GPtrArray * formats)
{
  GValue v_formats = G_VALUE_INIT;
  const gchar *format;
  guint i;

  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (formats, FALSE);

  if (formats->len == 1) {
    format = g_ptr_array_index (formats, 0);
    g_value_init (&v_formats, G_TYPE_STRING);
    g_value_set_string (&v_formats, format);
  } else if (formats->len > 1) {

    gst_value_list_init (&v_formats, formats->len);

    for (i = 0; i < formats->len; i++) {
      format = g_ptr_array_index (formats, i);
      _value_list_append_string (&v_formats, format);
    }
  } else {
    return FALSE;
  }

  gst_caps_set_value (caps, "drm-format", &v_formats);
  g_value_unset (&v_formats);

  return TRUE;
}

/* Fix raw frames ill reported by drivers.
 *
 * Mesa Gallium reports P010 and P016 for H264 encoder:
 * https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/19443
 *
 * Intel i965: reports I420 and YV12
 * XXX: add issue or pr
 */
static gboolean
fix_raw_formats (GstVaDisplay * display, VAProfile profile,
    VAEntrypoint entrypoint, GArray * formats)
{
  GstVideoFormat format;

  if (!(GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_I965) ||
          GST_VA_DISPLAY_IS_IMPLEMENTATION (display, MESA_GALLIUM)))
    return TRUE;

  if (gst_va_profile_codec (profile) != H264
      || entrypoint != VAEntrypointEncSlice)
    return TRUE;

  formats = g_array_set_size (formats, 0);
  format = GST_VIDEO_FORMAT_NV12;
  g_array_append_val (formats, format);
  return TRUE;
}

GstCaps *
gst_va_create_dma_caps (GstVaDisplay * display, VAEntrypoint entrypoint,
    GArray * formats, gint min_width, gint max_width,
    gint min_height, gint max_height)
{
  guint usage_hint;
  guint64 modifier;
  guint32 fourcc;
  GstVideoFormat fmt;
  gchar *drm_fmt_str;
  GPtrArray *drm_formats_str;
  GstCaps *caps = NULL;
  guint i;

  usage_hint = va_get_surface_usage_hint (display,
      entrypoint, GST_PAD_UNKNOWN, TRUE);

  drm_formats_str = g_ptr_array_new_with_free_func (g_free);

  for (i = 0; i < formats->len; i++) {
    fmt = g_array_index (formats, GstVideoFormat, i);

    fourcc = gst_va_drm_fourcc_from_video_format (fmt);
    if (fourcc == DRM_FORMAT_INVALID)
      continue;

    modifier = gst_va_dmabuf_get_modifier_for_format (display, fmt, usage_hint);
    if (modifier == DRM_FORMAT_MOD_INVALID)
      continue;

    drm_fmt_str = gst_video_dma_drm_fourcc_to_string (fourcc, modifier);

    g_ptr_array_add (drm_formats_str, drm_fmt_str);
  }

  if (drm_formats_str->len == 0)
    goto out;

  caps = gst_caps_new_simple ("video/x-raw", "width", GST_TYPE_INT_RANGE,
      min_width, max_width, "height", GST_TYPE_INT_RANGE, min_height,
      max_height, NULL);

  gst_caps_set_features_simple (caps,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF));

  gst_caps_set_simple (caps, "format", G_TYPE_STRING, "DMA_DRM", NULL);

  if (!gst_caps_set_drm_format_array (caps, drm_formats_str)) {
    gst_clear_caps (&caps);
    goto out;
  }

out:
  g_ptr_array_unref (drm_formats_str);
  return caps;
}

static gboolean
_get_entrypoint_from_config (GstVaDisplay * display, VAConfigID config,
    VAProfile * profile_out, VAEntrypoint * entrypoint_out)
{
  VADisplay dpy;
  VAStatus status;
  VAProfile profile;
  VAEntrypoint entrypoint;
  VAConfigAttrib *attribs;
  int num_attribs = 0;

  dpy = gst_va_display_get_va_dpy (display);

  attribs = g_new (VAConfigAttrib, vaMaxNumConfigAttributes (dpy));
  status = vaQueryConfigAttributes (dpy, config, &profile, &entrypoint, attribs,
      &num_attribs);
  g_free (attribs);

  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaQueryConfigAttributes: %s",
        vaErrorStr (status));
    return FALSE;
  }

  if (profile_out)
    *profile_out = profile;
  if (entrypoint_out)
    *entrypoint_out = entrypoint;

  return TRUE;
}

GstCaps *
gst_va_create_raw_caps_from_config (GstVaDisplay * display, VAConfigID config)
{
  GArray *formats;
  GstCaps *caps = NULL, *base_caps, *feature_caps;
  GstCapsFeatures *features;
  GstVideoFormat format;
  VASurfaceAttrib *attribs;
  VAEntrypoint entrypoint;
  VAProfile profile;
  guint i, attrib_count, mem_type = 0;
  gint min_width = 1, max_width = G_MAXINT;
  gint min_height = 1, max_height = G_MAXINT;

  if (!_get_entrypoint_from_config (display, config, &profile, &entrypoint))
    return NULL;

  attribs = gst_va_get_surface_attribs (display, config, &attrib_count);
  if (!attribs)
    return NULL;
  formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));

  for (i = 0; i < attrib_count; i++) {
    if (attribs[i].value.type != VAGenericValueTypeInteger)
      continue;
    switch (attribs[i].type) {
      case VASurfaceAttribPixelFormat:
        format = gst_va_video_format_from_va_fourcc (attribs[i].value.value.i);
        if (format != GST_VIDEO_FORMAT_UNKNOWN)
          g_array_append_val (formats, format);
        break;
      case VASurfaceAttribMinWidth:
        min_width = MAX (min_width, attribs[i].value.value.i);
        break;
      case VASurfaceAttribMaxWidth:
        max_width = attribs[i].value.value.i;
        break;
      case VASurfaceAttribMinHeight:
        min_height = MAX (min_height, attribs[i].value.value.i);
        break;
      case VASurfaceAttribMaxHeight:
        max_height = attribs[i].value.value.i;
        break;
      case VASurfaceAttribMemoryType:
        mem_type = attribs[i].value.value.i;
        break;
      default:
        break;
    }
  }

  /* if driver doesn't report surface formats for current
   * chroma. Gallium AMD bug for 4:2:2 */
  if (formats->len == 0)
    goto bail;

  if (!fix_raw_formats (display, profile, entrypoint, formats))
    goto bail;

  base_caps = gst_caps_new_simple ("video/x-raw", "width", GST_TYPE_INT_RANGE,
      min_width, max_width, "height", GST_TYPE_INT_RANGE, min_height,
      max_height, NULL);

  if (!gst_caps_set_format_array (base_caps, formats)) {
    gst_caps_unref (base_caps);
    goto bail;
  }

  caps = gst_caps_new_empty ();

  if (mem_type & VA_SURFACE_ATTRIB_MEM_TYPE_VA) {
    feature_caps = gst_caps_copy (base_caps);
    features = gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA);
    gst_caps_set_features_simple (feature_caps, features);
    caps = gst_caps_merge (caps, feature_caps);
  }
  if (mem_type & VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
      || mem_type & VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2) {
    feature_caps = gst_va_create_dma_caps (display, entrypoint, formats,
        min_width, max_width, min_height, max_height);
    if (feature_caps)
      caps = gst_caps_merge (caps, feature_caps);
  }

  /* raw caps */
  caps = gst_caps_merge (caps, gst_caps_copy (base_caps));

  gst_caps_unref (base_caps);

bail:
  g_array_unref (formats);
  g_free (attribs);

  return caps;
}

static GstCaps *
gst_va_create_raw_caps (GstVaDisplay * display, VAProfile profile,
    VAEntrypoint entrypoint, guint rt_format)
{
  GstCaps *caps;
  VAConfigAttrib attrib = {
    .type = VAConfigAttribRTFormat,
    .value = rt_format,
  };
  VAConfigID config;
  VADisplay dpy;
  VAStatus status;

  dpy = gst_va_display_get_va_dpy (display);

  status = vaCreateConfig (dpy, profile, entrypoint, &attrib, 1, &config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaCreateConfig: %s", vaErrorStr (status));
    return NULL;
  }

  caps = gst_va_create_raw_caps_from_config (display, config);

  status = vaDestroyConfig (dpy, config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaDestroyConfig: %s", vaErrorStr (status));
    return NULL;
  }

  return caps;
}

gboolean
gst_va_video_info_from_caps (GstVideoInfo * info, guint64 * modifier,
    GstCaps * caps)
{
  GstVideoInfoDmaDrm drm_info;

  if (!gst_video_is_dma_drm_caps (caps))
    return gst_video_info_from_caps (info, caps);

  if (!gst_video_info_dma_drm_from_caps (&drm_info, caps))
    return FALSE;

  if (!gst_va_dma_drm_info_to_video_info (&drm_info, info))
    return FALSE;

  if (modifier)
    *modifier = drm_info.drm_modifier;

  return TRUE;
}

GstCaps *
gst_va_video_info_to_dma_caps (GstVideoInfo * info, guint64 modifier)
{
  GstVideoInfoDmaDrm drm_info;

  gst_video_info_dma_drm_init (&drm_info);
  drm_info.vinfo = *info;
  drm_info.drm_fourcc =
      gst_va_drm_fourcc_from_video_format (GST_VIDEO_INFO_FORMAT (info));
  drm_info.drm_modifier = modifier;

  return gst_video_info_dma_drm_to_caps (&drm_info);
}

/* the purpose of this function is to find broken configurations in
 * JPEG decoders: if the driver doesn't expose a pixel format for a
 * config with a specific sampling, that sampling is not valid */
static inline gboolean
_config_has_pixel_formats (GstVaDisplay * display, VAProfile profile,
    VAEntrypoint entrypoint, guint32 rt_format)
{
  guint i, fourcc, count;
  gboolean found = FALSE;
  VAConfigAttrib attrs = {
    .type = VAConfigAttribRTFormat,
    .value = rt_format,
  };
  VAConfigID config;
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VASurfaceAttrib *attr_list;
  VAStatus status;

  status = vaCreateConfig (dpy, profile, entrypoint, &attrs, 1, &config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "Failed to create JPEG config");
    return FALSE;
  }
  attr_list = gst_va_get_surface_attribs (display, config, &count);
  if (!attr_list)
    goto bail;

  /* XXX: JPEG decoders handle RGB16 and RGB32 chromas, but they use
   * RGBP pixel format, which its chroma is RGBP (not 16 nor 32). So
   * if the requested chroma is 16 or 32 it's locally overloaded as
   * RGBP. */
  if (rt_format == VA_RT_FORMAT_RGB16 || rt_format == VA_RT_FORMAT_RGB32)
    rt_format = VA_RT_FORMAT_RGBP;

  for (i = 0; i < count; i++) {
    if (attr_list[i].type == VASurfaceAttribPixelFormat) {
      fourcc = attr_list[i].value.value.i;
      /* ignore pixel formats without requested chroma */
      found = (gst_va_chroma_from_va_fourcc (fourcc) == rt_format);
      if (found)
        break;
    }
  }
  g_free (attr_list);

bail:
  status = vaDestroyConfig (dpy, config);
  if (status != VA_STATUS_SUCCESS)
    GST_WARNING_OBJECT (display, "Failed to destroy JPEG config");

  return found;
}

static void
_add_jpeg_fields (GstVaDisplay * display, GstCaps * caps, VAProfile profile,
    VAEntrypoint entrypoint, guint32 rt_formats)
{
  guint i, size;
  GValue colorspace = G_VALUE_INIT, sampling = G_VALUE_INIT;
  gboolean rgb, gray, yuv;

  rgb = gray = yuv = FALSE;

  gst_value_list_init (&colorspace, 3);
  gst_value_list_init (&sampling, 3);

  for (i = 0; rt_formats && i < G_N_ELEMENTS (va_rt_format_list); i++) {
    if (rt_formats & va_rt_format_list[i]) {
      if (!_config_has_pixel_formats (display, profile, entrypoint,
              va_rt_format_list[i]))
        continue;

#define APPEND_YUV G_STMT_START \
        if (!yuv) { _value_list_append_string (&colorspace, "sYUV"); yuv = TRUE; } \
      G_STMT_END

      switch (va_rt_format_list[i]) {
        case VA_RT_FORMAT_YUV420:
          APPEND_YUV;
          _value_list_append_string (&sampling, "YCbCr-4:2:0");
          break;
        case VA_RT_FORMAT_YUV422:
          APPEND_YUV;
          _value_list_append_string (&sampling, "YCbCr-4:2:2");
          break;
        case VA_RT_FORMAT_YUV444:
          APPEND_YUV;
          _value_list_append_string (&sampling, "YCbCr-4:4:4");
          break;
        case VA_RT_FORMAT_YUV411:
          APPEND_YUV;
          _value_list_append_string (&sampling, "YCbCr-4:1:1");
          break;
        case VA_RT_FORMAT_YUV400:
          if (!gray) {
            _value_list_append_string (&colorspace, "GRAY");
            _value_list_append_string (&sampling, "GRAYSCALE");
            gray = TRUE;
          }
          break;
        case VA_RT_FORMAT_RGBP:
        case VA_RT_FORMAT_RGB16:
        case VA_RT_FORMAT_RGB32:
          if (!rgb) {
            _value_list_append_string (&colorspace, "sRGB");
            _value_list_append_string (&sampling, "RGB");
            _value_list_append_string (&sampling, "BGR");
            rgb = TRUE;
          }
          break;
        default:
          break;
      }
#undef APPEND_YUV
    }
  }

  size = gst_value_list_get_size (&colorspace);
  if (size == 1) {
    gst_caps_set_value (caps, "colorspace",
        gst_value_list_get_value (&colorspace, 0));
  } else if (size > 1) {
    gst_caps_set_value (caps, "colorspace", &colorspace);
  }

  size = gst_value_list_get_size (&sampling);
  if (size == 1) {
    gst_caps_set_value (caps, "sampling",
        gst_value_list_get_value (&sampling, 0));
  } else if (size > 1) {
    gst_caps_set_value (caps, "sampling", &sampling);
  }

  g_value_unset (&colorspace);
  g_value_unset (&sampling);
}

GstCaps *
gst_va_create_coded_caps (GstVaDisplay * display, VAProfile profile,
    VAEntrypoint entrypoint, guint32 * rt_formats_ptr)
{
  GstCaps *caps;
  /* *INDENT-OFF* */
  VAConfigAttrib attribs[] = {
    { .type = VAConfigAttribMaxPictureWidth, },
    { .type = VAConfigAttribMaxPictureHeight, },
    { .type = VAConfigAttribRTFormat, },
  };
  /* *INDENT-ON* */
  VADisplay dpy;
  VAStatus status;
  guint32 value, rt_formats = 0;
  gint i, max_width = -1, max_height = -1;

  dpy = gst_va_display_get_va_dpy (display);

  status = vaGetConfigAttributes (dpy, profile, entrypoint, attribs,
      G_N_ELEMENTS (attribs));
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaGetConfigAttributes: %s",
        vaErrorStr (status));
    return NULL;
  }

  for (i = 0; i < G_N_ELEMENTS (attribs); i++) {
    value = attribs[i].value;
    if (value == VA_ATTRIB_NOT_SUPPORTED)
      continue;
    switch (attribs[i].type) {
      case VAConfigAttribMaxPictureHeight:
        if (value <= G_MAXINT)
          max_height = value;
        break;
      case VAConfigAttribMaxPictureWidth:
        if (value <= G_MAXINT)
          max_width = value;
        break;
      case VAConfigAttribRTFormat:
        rt_formats = value;
        break;
      default:
        break;
    }
  }

  if (rt_formats_ptr)
    *rt_formats_ptr = rt_formats;

  caps = gst_va_profile_caps (profile, entrypoint);
  if (!caps)
    return NULL;

  if (rt_formats > 0 && gst_va_profile_codec (profile) == JPEG)
    _add_jpeg_fields (display, caps, profile, entrypoint, rt_formats);

  if (max_width == -1 || max_height == -1)
    return caps;

  gst_caps_set_simple (caps, "width", GST_TYPE_INT_RANGE, 1, max_width,
      "height", GST_TYPE_INT_RANGE, 1, max_height, NULL);

  return caps;
}

static GstCaps *
_regroup_raw_caps (GstCaps * caps)
{
  GstCaps *sys_caps, *va_caps, *dma_caps, *tmp;
  guint size, i;

  if (gst_caps_is_any (caps) || gst_caps_is_empty (caps))
    return caps;

  size = gst_caps_get_size (caps);
  if (size <= 1)
    return caps;

  /* We need to simplify caps by features. */
  sys_caps = gst_caps_new_empty ();
  va_caps = gst_caps_new_empty ();
  dma_caps = gst_caps_new_empty ();
  for (i = 0; i < size; i++) {
    GstCapsFeatures *ft;

    tmp = gst_caps_copy_nth (caps, i);
    ft = gst_caps_get_features (tmp, 0);
    if (gst_caps_features_contains (ft, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      dma_caps = gst_caps_merge (dma_caps, tmp);
    } else if (gst_caps_features_contains (ft, GST_CAPS_FEATURE_MEMORY_VA)) {
      va_caps = gst_caps_merge (va_caps, tmp);
    } else {
      sys_caps = gst_caps_merge (sys_caps, tmp);
    }
  }

  sys_caps = gst_caps_simplify (sys_caps);
  va_caps = gst_caps_simplify (va_caps);
  dma_caps = gst_caps_simplify (dma_caps);

  va_caps = gst_caps_merge (va_caps, dma_caps);
  va_caps = gst_caps_merge (va_caps, sys_caps);

  gst_caps_unref (caps);

  return va_caps;
}

gboolean
gst_va_caps_from_profiles (GstVaDisplay * display, GArray * profiles,
    VAEntrypoint entrypoint, GstCaps ** codedcaps_ptr, GstCaps ** rawcaps_ptr)
{
  GstCaps *codedcaps, *rawcaps;
  VAProfile profile;
  gboolean ret;
  gint i, j, k;
  guint32 rt_formats;
  gint min_width = 1, max_width = G_MAXINT;
  gint min_height = 1, max_height = G_MAXINT;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), FALSE);
  g_return_val_if_fail (profiles, FALSE);

  codedcaps = gst_caps_new_empty ();
  rawcaps = gst_caps_new_empty ();

  for (i = 0; i < profiles->len; i++) {
    GstCaps *profile_codedcaps;

    profile = g_array_index (profiles, VAProfile, i);
    profile_codedcaps = gst_va_create_coded_caps (display, profile, entrypoint,
        &rt_formats);
    if (!profile_codedcaps)
      continue;

    for (j = 0; rt_formats && j < G_N_ELEMENTS (va_rt_format_list); j++) {
      if (rt_formats & va_rt_format_list[j]) {
        GstCaps *profile_rawcaps = gst_va_create_raw_caps (display, profile,
            entrypoint, va_rt_format_list[j]);

        if (!profile_rawcaps)
          continue;

        /* fetch width and height ranges */
        {
          guint num_structures = gst_caps_get_size (profile_rawcaps);

          for (k = 0; k < num_structures; k++) {
            GstStructure *st = gst_caps_get_structure (profile_rawcaps, k);
            if (!st)
              continue;
            if (gst_structure_has_field (st, "width")
                && gst_structure_has_field (st, "height")) {
              const GValue *w = gst_structure_get_value (st, "width");
              const GValue *h = gst_structure_get_value (st, "height");

              min_width = MAX (min_width, gst_value_get_int_range_min (w));
              max_width = MIN (max_width, gst_value_get_int_range_max (w));
              min_height = MAX (min_height, gst_value_get_int_range_min (h));
              max_height = MIN (max_height, gst_value_get_int_range_max (h));
            }
          }
        }

        rawcaps = gst_caps_merge (rawcaps, profile_rawcaps);
      }
    }

    /* check frame size range was specified otherwise use the one used
     * by the rawcaps */
    {
      guint num_structures = gst_caps_get_size (profile_codedcaps);

      for (k = 0; k < num_structures; k++) {
        GstStructure *st = gst_caps_get_structure (profile_codedcaps, k);
        if (!st)
          continue;
        if (!gst_structure_has_field (st, "width"))
          gst_structure_set (st, "width", GST_TYPE_INT_RANGE, min_width,
              max_width, NULL);
        if (!gst_structure_has_field (st, "height"))
          gst_structure_set (st, "height", GST_TYPE_INT_RANGE, min_height,
              max_height, NULL);
      }
    }

    codedcaps = gst_caps_merge (codedcaps, profile_codedcaps);
  }

  if (gst_caps_is_empty (rawcaps))
    gst_caps_replace (&rawcaps, NULL);
  if (gst_caps_is_empty (codedcaps))
    gst_caps_replace (&codedcaps, NULL);

  if ((ret = codedcaps && rawcaps)) {
    rawcaps = _regroup_raw_caps (rawcaps);
    codedcaps = gst_caps_simplify (codedcaps);

    if (rawcaps_ptr)
      *rawcaps_ptr = gst_caps_ref (rawcaps);
    if (codedcaps_ptr)
      *codedcaps_ptr = gst_caps_ref (codedcaps);
  }

  if (codedcaps)
    gst_caps_unref (codedcaps);
  if (rawcaps)
    gst_caps_unref (rawcaps);

  return ret;
}

static inline gboolean
_caps_is (GstCaps * caps, const gchar * feature)
{
  GstCapsFeatures *features;

  if (!gst_caps_is_fixed (caps))
    return FALSE;

  features = gst_caps_get_features (caps, 0);
  return gst_caps_features_contains (features, feature);
}

gboolean
gst_caps_is_dmabuf (GstCaps * caps)
{
  return _caps_is (caps, GST_CAPS_FEATURE_MEMORY_DMABUF);
}

gboolean
gst_caps_is_vamemory (GstCaps * caps)
{
  return _caps_is (caps, GST_CAPS_FEATURE_MEMORY_VA);
}

gboolean
gst_caps_is_raw (GstCaps * caps)
{
  return _caps_is (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
}
