/* GStreamer
 *  Copyright (C) 2021 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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

#include "gstvaencoder.h"

#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>

#include "gstvacaps.h"
#include "gstvaprofile.h"
#include "gstvadisplay_priv.h"
#include "vacompat.h"

struct _GstVaEncoder
{
  GstObject parent;

  GArray *available_profiles;
  GstCaps *srcpad_caps;
  GstCaps *sinkpad_caps;
  GstVaDisplay *display;
  VAConfigID config;
  VAContextID context;
  VAProfile profile;
  VAEntrypoint entrypoint;
  guint rt_format;
  gint coded_width;
  gint coded_height;
  gint codedbuf_size;

  GstBufferPool *recon_pool;
};

GST_DEBUG_CATEGORY_STATIC (gst_va_encoder_debug);
#define GST_CAT_DEFAULT gst_va_encoder_debug

#define gst_va_encoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVaEncoder, gst_va_encoder, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_va_encoder_debug, "vaencoder", 0,
        "VA Encoder"));

enum
{
  PROP_DISPLAY = 1,
  PROP_PROFILE,
  PROP_ENTRYPOINT,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_CHROMA,
  PROP_CODED_BUF_SIZE,
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES];

static gboolean
_destroy_buffer (GstVaDisplay * display, VABufferID buffer)
{
  VAStatus status;
  gboolean ret = TRUE;
  VADisplay dpy = gst_va_display_get_va_dpy (display);

  status = vaDestroyBuffer (dpy, buffer);
  if (status != VA_STATUS_SUCCESS) {
    ret = FALSE;
    GST_WARNING ("Failed to destroy the buffer: %s", vaErrorStr (status));
  }

  return ret;
}

static VABufferID
_create_buffer (GstVaEncoder * self, gint type, gpointer data, gsize size)
{
  VAStatus status;
  VADisplay dpy = gst_va_display_get_va_dpy (self->display);
  VABufferID buffer;
  VAContextID context;

  GST_OBJECT_LOCK (self);
  context = self->context;
  GST_OBJECT_UNLOCK (self);

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, context, type, size, 1, data, &buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return VA_INVALID_ID;
  }

  return buffer;
}

static void
gst_va_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaEncoder *self = GST_VA_ENCODER (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_DISPLAY:{
      g_assert (!self->display);
      self->display = g_value_dup_object (value);
      break;
    }
    case PROP_ENTRYPOINT:{
      self->entrypoint = g_value_get_int (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_encoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVaEncoder *self = GST_VA_ENCODER (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;
    case PROP_PROFILE:
      g_value_set_int (value, self->profile);
      break;
    case PROP_ENTRYPOINT:
      g_value_set_int (value, self->entrypoint);
      break;
    case PROP_CHROMA:
      g_value_set_uint (value, self->rt_format);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, self->coded_width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, self->coded_height);
      break;
    case PROP_CODED_BUF_SIZE:
      g_value_set_int (value, self->codedbuf_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_encoder_init (GstVaEncoder * self)
{
  self->profile = VAProfileNone;
  self->config = VA_INVALID_ID;
}

static void
gst_va_encoder_reset (GstVaEncoder * self)
{
  self->profile = VAProfileNone;
  self->config = VA_INVALID_ID;
  self->context = VA_INVALID_ID;
  self->rt_format = 0;
  self->coded_width = 0;
  self->coded_height = 0;
  self->codedbuf_size = 0;
}

static inline gboolean
_is_open_unlocked (GstVaEncoder * self)
{
  return (self->config != VA_INVALID_ID && self->profile != VAProfileNone);
}

gboolean
gst_va_encoder_is_open (GstVaEncoder * self)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  GST_OBJECT_LOCK (self);
  ret = _is_open_unlocked (self);
  GST_OBJECT_UNLOCK (self);
  return ret;
}

gboolean
gst_va_encoder_close (GstVaEncoder * self)
{
  VADisplay dpy;
  VAStatus status;
  VAConfigID config = VA_INVALID_ID;
  VAContextID context = VA_INVALID_ID;
  GstBufferPool *recon_pool = NULL;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  GST_OBJECT_LOCK (self);
  if (!_is_open_unlocked (self)) {
    GST_OBJECT_UNLOCK (self);
    return TRUE;
  }

  config = self->config;
  context = self->context;

  recon_pool = self->recon_pool;
  self->recon_pool = NULL;

  gst_va_encoder_reset (self);
  GST_OBJECT_UNLOCK (self);

  gst_buffer_pool_set_active (recon_pool, FALSE);
  g_clear_pointer (&recon_pool, gst_object_unref);

  dpy = gst_va_display_get_va_dpy (self->display);

  if (context != VA_INVALID_ID) {
    status = vaDestroyContext (dpy, context);
    if (status != VA_STATUS_SUCCESS)
      GST_ERROR_OBJECT (self, "vaDestroyContext: %s", vaErrorStr (status));
  }

  status = vaDestroyConfig (dpy, config);
  if (status != VA_STATUS_SUCCESS)
    GST_ERROR_OBJECT (self, "vaDestroyConfig: %s", vaErrorStr (status));

  gst_caps_replace (&self->srcpad_caps, NULL);
  gst_caps_replace (&self->sinkpad_caps, NULL);

  return TRUE;
}

/* for querying the customized surface alignment */
guint
gst_va_encoder_get_surface_alignment (GstVaDisplay * display,
    VAProfile profile, VAEntrypoint entrypoint)
{
  guint alignment = 0;
#if VA_CHECK_VERSION(1, 21, 0)
  VAConfigAttrib *attrib = NULL;
  VASurfaceAttrib *attr_list;
  guint i, count;
  VAConfigID config;
  VADisplay dpy;
  VAStatus status;

  dpy = gst_va_display_get_va_dpy (display);
  status = vaCreateConfig (dpy, profile, entrypoint, attrib, 0, &config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaCreateConfig: %s", vaErrorStr (status));
    return alignment;
  }
  attr_list = gst_va_get_surface_attribs (display, config, &count);
  if (!attr_list)
    goto bail;

  for (i = 0; i < count; i++) {
    if (attr_list[i].type == VASurfaceAttribAlignmentSize) {
      alignment = attr_list[i].value.value.i;
      GST_INFO_OBJECT (display,
          "Using customized surface alignment [%dx%d]\n",
          1 << (alignment & 0xf), 1 << ((alignment & 0xf0) >> 4));
      break;
    }
  }
  g_free (attr_list);

bail:
  status = vaDestroyConfig (dpy, config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (display, "vaDestroyConfig: %s", vaErrorStr (status));
    return alignment;
  }
#endif
  return alignment;
}

static GArray *
_get_surface_formats (GstVaDisplay * display, VAConfigID config)
{
  GArray *formats;
  GstVideoFormat format;
  VASurfaceAttrib *attribs;
  guint i, attrib_count;

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
      default:
        break;
    }
  }

  g_free (attribs);

  if (formats->len == 0) {
    g_array_unref (formats);
    return NULL;
  }

  return formats;
}

static GstBufferPool *
_create_reconstruct_pool (GstVaDisplay * display, GArray * surface_formats,
    GstVideoFormat format, gint coded_width, gint coded_height,
    guint max_buffers)
{
  GstAllocator *allocator = NULL;
  guint usage_hint;
  GstVideoInfo info;
  GstAllocationParams params = { 0, };
  GstBufferPool *pool;
  GstCaps *caps = NULL;

  gst_video_info_set_format (&info, format, coded_width, coded_height);

  usage_hint = va_get_surface_usage_hint (display,
      VAEntrypointEncSlice, GST_PAD_SINK, FALSE);

  caps = gst_video_info_to_caps (&info);
  gst_caps_set_features_simple (caps,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA));

  allocator = gst_va_allocator_new (display, surface_formats);

  pool = gst_va_pool_new_with_config (caps, 0, max_buffers, usage_hint,
      GST_VA_FEATURE_AUTO, allocator, &params);

  gst_clear_object (&allocator);
  gst_clear_caps (&caps);

  return pool;
}

gboolean
gst_va_encoder_open (GstVaEncoder * self, VAProfile profile,
    GstVideoFormat video_format, guint rt_format, gint coded_width,
    gint coded_height, gint codedbuf_size, guint max_reconstruct_surfaces,
    guint rc_ctrl, guint32 packed_headers)
{
  /* *INDENT-OFF* */
  VAConfigAttrib attribs[3] = {
    { .type = VAConfigAttribRTFormat, .value = rt_format, },
  };
  /* *INDENT-ON* */
  VAConfigID config = VA_INVALID_ID;
  VAContextID context = VA_INVALID_ID;
  VADisplay dpy;
  GArray *surface_formats = NULL;
  VAStatus status;
  GstBufferPool *recon_pool = NULL;
  guint attrib_idx = 1;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);
  g_return_val_if_fail (codedbuf_size > 0, FALSE);

  if (gst_va_encoder_is_open (self))
    return TRUE;

  if (!gst_va_encoder_has_profile (self, profile)) {
    GST_ERROR_OBJECT (self, "Unsupported profile: %s, entrypoint: %d",
        gst_va_profile_name (profile), self->entrypoint);
    return FALSE;
  }

  if (rc_ctrl != VA_RC_NONE) {
    attribs[attrib_idx].type = VAConfigAttribRateControl;
    attribs[attrib_idx].value = rc_ctrl;
    attrib_idx++;
  }

  if (packed_headers > 0) {
    attribs[attrib_idx].type = VAConfigAttribEncPackedHeaders;
    attribs[attrib_idx].value = packed_headers;
    attrib_idx++;
  }

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaCreateConfig (dpy, profile, self->entrypoint, attribs, attrib_idx,
      &config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateConfig: %s", vaErrorStr (status));
    goto error;
  }

  surface_formats = _get_surface_formats (self->display, config);
  if (!surface_formats) {
    GST_ERROR_OBJECT (self, "Failed to get surface formats");
    goto error;
  }

  recon_pool = _create_reconstruct_pool (self->display, surface_formats,
      video_format, coded_width, coded_height, max_reconstruct_surfaces);
  if (!recon_pool) {
    GST_ERROR_OBJECT (self, "Failed to create reconstruct pool");
    goto error;
  }

  if (!gst_buffer_pool_set_active (recon_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to activate reconstruct pool");
    goto error;
  }

  status = vaCreateContext (dpy, config, coded_width, coded_height,
      VA_PROGRESSIVE, NULL, 0, &context);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateConfig: %s", vaErrorStr (status));
    goto error;
  }

  GST_OBJECT_LOCK (self);

  self->config = config;
  self->context = context;
  self->profile = profile;
  self->rt_format = rt_format;
  self->coded_width = coded_width;
  self->coded_height = coded_height;
  self->codedbuf_size = codedbuf_size;
  gst_object_replace ((GstObject **) & self->recon_pool,
      (GstObject *) recon_pool);

  GST_OBJECT_UNLOCK (self);

  g_clear_pointer (&recon_pool, gst_object_unref);
  /* now we should return now only this profile's caps */
  gst_caps_replace (&self->srcpad_caps, NULL);

  return TRUE;

error:
  g_clear_pointer (&recon_pool, gst_object_unref);

  if (config != VA_INVALID_ID)
    vaDestroyConfig (dpy, config);

  if (context != VA_INVALID_ID)
    vaDestroyContext (dpy, context);

  return FALSE;
}

static void
gst_va_encoder_dispose (GObject * object)
{
  GstVaEncoder *self = GST_VA_ENCODER (object);

  gst_va_encoder_close (self);

  g_clear_pointer (&self->available_profiles, g_array_unref);
  gst_clear_object (&self->display);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_encoder_class_init (GstVaEncoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_va_encoder_set_property;
  gobject_class->get_property = gst_va_encoder_get_property;
  gobject_class->dispose = gst_va_encoder_dispose;

  g_properties[PROP_DISPLAY] =
      g_param_spec_object ("display", "GstVaDisplay", "GstVaDisplay object",
      GST_TYPE_VA_DISPLAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_PROFILE] =
      g_param_spec_int ("va-profile", "VAProfile", "VA Profile",
      VAProfileNone, 50, VAProfileNone,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_ENTRYPOINT] =
      g_param_spec_int ("va-entrypoint", "VAEntrypoint", "VA Entrypoint",
      0, 14, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_CHROMA] =
      g_param_spec_uint ("va-rt-format", "VARTFormat", "VA RT Format",
      VA_RT_FORMAT_YUV420, VA_RT_FORMAT_PROTECTED, VA_RT_FORMAT_YUV420,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_WIDTH] =
      g_param_spec_int ("coded-width", "coded-picture-width",
      "coded picture width", 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_HEIGHT] =
      g_param_spec_int ("coded-height", "coded-picture-height",
      "coded picture height", 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_CODED_BUF_SIZE] =
      g_param_spec_int ("coded-buf-size", "coded-buffer-size",
      "coded buffer size", 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, g_properties);
}

static gboolean
gst_va_encoder_initialize (GstVaEncoder * self, guint32 codec)
{
  if (self->available_profiles)
    return FALSE;

  self->available_profiles =
      gst_va_display_get_profiles (self->display, codec, self->entrypoint);

  if (!self->available_profiles)
    return FALSE;

  if (self->available_profiles->len == 0) {
    g_clear_pointer (&self->available_profiles, g_array_unref);
    return FALSE;
  }

  return TRUE;
}

GstVaEncoder *
gst_va_encoder_new (GstVaDisplay * display, guint32 codec,
    VAEntrypoint entrypoint)
{
  GstVaEncoder *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), NULL);

  self = g_object_new (GST_TYPE_VA_ENCODER, "display", display,
      "va-entrypoint", entrypoint, NULL);
  gst_object_ref_sink (self);

  if (!gst_va_encoder_initialize (self, codec))
    gst_clear_object (&self);

  return self;
}

gboolean
gst_va_encoder_get_reconstruct_pool_config (GstVaEncoder * self,
    GstCaps ** caps, guint * max_surfaces)
{
  GstStructure *config;
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (!gst_va_encoder_is_open (self))
    return FALSE;

  if (!self->recon_pool)
    return FALSE;

  config = gst_buffer_pool_get_config (self->recon_pool);
  ret = gst_buffer_pool_config_get_params (config, caps, NULL, NULL,
      max_surfaces);
  gst_structure_free (config);
  return ret;
}

gboolean
gst_va_encoder_has_profile (GstVaEncoder * self, VAProfile profile)
{
  VAProfile p;
  gint i;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  for (i = 0; i < self->available_profiles->len; i++) {
    p = g_array_index (self->available_profiles, VAProfile, i);
    if (p == profile)
      return TRUE;
  }

  return FALSE;
}

gint32
gst_va_encoder_get_max_slice_num (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncMaxSlices };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), -1);

  if (profile == VAProfileNone)
    return -1;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query encoding slices: %s",
        vaErrorStr (status));
    return -1;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support encoding picture as "
        "multiple slices");
    return -1;
  }

  return attrib.value;
}

gint32
gst_va_encoder_get_slice_structure (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncSliceStructure };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), 0);

  if (profile == VAProfileNone)
    return -1;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query encoding slice structure: %s",
        vaErrorStr (status));
    return 0;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support slice structure");
    return 0;
  }

  return attrib.value;
}

gboolean
gst_va_encoder_get_max_num_reference (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint,
    guint32 * list0, guint32 * list1)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncMaxRefFrames };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (profile == VAProfileNone)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query reference frames: %s",
        vaErrorStr (status));
    return FALSE;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    if (list0)
      *list0 = 0;
    if (list1)
      *list1 = 0;

    return TRUE;
  }

  if (list0)
    *list0 = attrib.value & 0xffff;
  if (list1)
    *list1 = (attrib.value >> 16) & 0xffff;

  return TRUE;
}

guint
gst_va_encoder_get_prediction_direction (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribPredictionDirection };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), 0);

  if (profile == VAProfileNone)
    return 0;

  if (entrypoint != self->entrypoint)
    return 0;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query prediction direction: %s",
        vaErrorStr (status));
    return 0;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support query"
        " prediction direction");
    return 0;
  }

  return attrib.value & (VA_PREDICTION_DIRECTION_PREVIOUS |
      VA_PREDICTION_DIRECTION_FUTURE | VA_PREDICTION_DIRECTION_BI_NOT_EMPTY);
}

guint32
gst_va_encoder_get_rate_control_mode (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribRateControl };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), 0);

  if (profile == VAProfileNone)
    return 0;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query rate control mode: %s",
        vaErrorStr (status));
    return 0;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support any rate control modes");
    return 0;
  }

  return attrib.value;
}

guint32
gst_va_encoder_get_quality_level (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncQualityRange };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), 0);

  if (profile == VAProfileNone)
    return 0;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query the quality level: %s",
        vaErrorStr (status));
    return 0;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support quality attribute");
    return 0;
  }

  return attrib.value;
}

gboolean
gst_va_encoder_has_trellis (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncQuantization };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (profile == VAProfileNone)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query the trellis: %s",
        vaErrorStr (status));
    return FALSE;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support trellis");
    return FALSE;
  }

  return attrib.value & VA_ENC_QUANTIZATION_TRELLIS_SUPPORTED;
}

gboolean
gst_va_encoder_has_tile (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncTileSupport };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (profile == VAProfileNone)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "Failed to query the tile: %s",
        vaErrorStr (status));
    return FALSE;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support tile");
    return FALSE;
  }

  return attrib.value > 0;
}

guint32
gst_va_encoder_get_rtformat (GstVaEncoder * self,
    VAProfile profile, VAEntrypoint entrypoint)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribRTFormat };

  if (profile == VAProfileNone)
    return 0;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Failed to query rt format: %s",
        vaErrorStr (status));
    return 0;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support any rt format");
    return 0;
  }

  return attrib.value;
}

gboolean
gst_va_encoder_get_packed_headers (GstVaEncoder * self, VAProfile profile,
    VAEntrypoint entrypoint, guint * packed_headers)
{
  VAStatus status;
  VADisplay dpy;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncPackedHeaders };

  if (profile == VAProfileNone)
    return FALSE;

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaGetConfigAttributes (dpy, profile, entrypoint, &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Failed to query packed headers: %s",
        vaErrorStr (status));
    return FALSE;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_WARNING_OBJECT (self, "Driver does not support any packed headers");
    return FALSE;
  }

  if (packed_headers)
    *packed_headers = attrib.value;
  return TRUE;
}

/* Add packed header such as SPS, PPS, SEI, etc. If adding slice header,
   it is attached to the last slice parameter. */
gboolean
gst_va_encoder_add_packed_header (GstVaEncoder * self, GstVaEncodePicture * pic,
    gint type, gpointer data, gsize size_in_bits, gboolean has_emulation_bytes)
{
  VABufferID buffer;
  VAEncPackedHeaderParameterBuffer param = {
    .type = type,
    .bit_length = size_in_bits,
    .has_emulation_bytes = has_emulation_bytes,
  };

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);
  g_return_val_if_fail (self->context != VA_INVALID_ID, FALSE);
  g_return_val_if_fail (pic && data && size_in_bits > 0, FALSE);
  g_return_val_if_fail (type >= VAEncPackedHeaderSequence
      && type <= VAEncPackedHeaderRawData, FALSE);

  if (!gst_va_encoder_is_open (self)) {
    GST_ERROR_OBJECT (self, "encoder has not been opened yet");
    return FALSE;
  }

  buffer = _create_buffer (self, VAEncPackedHeaderParameterBufferType, &param,
      sizeof (param));
  if (buffer == VA_INVALID_ID)
    return FALSE;

  g_array_append_val (pic->params, buffer);

  buffer = _create_buffer (self, VAEncPackedHeaderDataBufferType, data,
      (size_in_bits + 7) / 8);
  if (buffer == VA_INVALID_ID)
    return FALSE;

  g_array_append_val (pic->params, buffer);

  return TRUE;
}

gboolean
gst_va_encoder_add_param (GstVaEncoder * self, GstVaEncodePicture * pic,
    VABufferType type, gpointer data, gsize size)
{
  VABufferID buffer;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);
  g_return_val_if_fail (self->context != VA_INVALID_ID, FALSE);
  g_return_val_if_fail (pic && data && size > 0, FALSE);

  if (!gst_va_encoder_is_open (self)) {
    GST_ERROR_OBJECT (self, "encoder has not been opened yet");
    return FALSE;
  }

  buffer = _create_buffer (self, type, data, size);
  if (buffer == VA_INVALID_ID)
    return FALSE;

  g_array_append_val (pic->params, buffer);

  return TRUE;
}

GArray *
gst_va_encoder_get_surface_formats (GstVaEncoder * self)
{
  g_return_val_if_fail (GST_IS_VA_ENCODER (self), NULL);

  if (!gst_va_encoder_is_open (self))
    return NULL;

  return _get_surface_formats (self->display, self->config);
}

static gboolean
_get_codec_caps (GstVaEncoder * self)
{
  GstCaps *sinkpad_caps = NULL, *srcpad_caps = NULL;

  if (!gst_va_encoder_is_open (self)
      && GST_IS_VA_DISPLAY_WRAPPED (self->display)) {
    if (gst_va_caps_from_profiles (self->display, self->available_profiles,
            self->entrypoint, &srcpad_caps, &sinkpad_caps)) {
      gst_caps_replace (&self->sinkpad_caps, sinkpad_caps);
      gst_caps_replace (&self->srcpad_caps, srcpad_caps);
      gst_caps_unref (srcpad_caps);
      gst_caps_unref (sinkpad_caps);

      return TRUE;
    }
  }

  return FALSE;
}

GstCaps *
gst_va_encoder_get_sinkpad_caps (GstVaEncoder * self)
{
  GstCaps *sinkpad_caps = NULL;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (g_atomic_pointer_get (&self->sinkpad_caps))
    return gst_caps_ref (self->sinkpad_caps);

  if (_get_codec_caps (self))
    return gst_caps_ref (self->sinkpad_caps);

  if (gst_va_encoder_is_open (self)) {
    sinkpad_caps = gst_va_create_raw_caps_from_config (self->display,
        self->config);
    if (!sinkpad_caps) {
      GST_WARNING_OBJECT (self, "Invalid configuration caps");
      return NULL;
    }
    gst_caps_replace (&self->sinkpad_caps, sinkpad_caps);
    gst_caps_unref (sinkpad_caps);

    return gst_caps_ref (self->sinkpad_caps);
  }

  return NULL;
}

GstCaps *
gst_va_encoder_get_srcpad_caps (GstVaEncoder * self)
{
  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  if (g_atomic_pointer_get (&self->srcpad_caps))
    return gst_caps_ref (self->srcpad_caps);

  if (_get_codec_caps (self))
    return gst_caps_ref (self->srcpad_caps);

  if (gst_va_encoder_is_open (self)) {
    VAProfile profile;
    VAEntrypoint entrypoint;
    GstCaps *caps;

    GST_OBJECT_LOCK (self);
    profile = self->profile;
    entrypoint = self->entrypoint;
    GST_OBJECT_UNLOCK (self);

    caps = gst_va_create_coded_caps (self->display, profile, entrypoint, NULL);
    if (caps) {
      gst_caps_replace (&self->srcpad_caps, caps);
      return gst_caps_ref (self->srcpad_caps);
    }
  }

  return NULL;
}

static gboolean
_destroy_all_buffers (GstVaEncodePicture * pic)
{
  GstVaDisplay *display;
  VABufferID buffer;
  guint i;
  gboolean ret = TRUE;

  display = gst_va_buffer_peek_display (pic->raw_buffer);
  if (!display)
    return FALSE;

  for (i = 0; i < pic->params->len; i++) {
    buffer = g_array_index (pic->params, VABufferID, i);
    ret &= _destroy_buffer (display, buffer);
  }
  pic->params = g_array_set_size (pic->params, 0);

  return ret;
}

gboolean
gst_va_encoder_encode (GstVaEncoder * self, GstVaEncodePicture * pic)
{
  VADisplay dpy;
  VAStatus status;
  VASurfaceID surface;
  VAContextID context;
  gboolean ret = FALSE;

  g_return_val_if_fail (pic, FALSE);

  GST_OBJECT_LOCK (self);

  if (!_is_open_unlocked (self)) {
    GST_OBJECT_UNLOCK (self);
    GST_ERROR_OBJECT (self, "encoder has not been opened yet");
    return FALSE;
  }

  context = self->context;
  GST_OBJECT_UNLOCK (self);

  surface = gst_va_encode_picture_get_raw_surface (pic);
  if (surface == VA_INVALID_ID) {
    GST_ERROR_OBJECT (self, "Encode picture without valid raw surface");
    goto bail;
  }

  GST_TRACE_OBJECT (self, "Encode the surface %#x", surface);

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaBeginPicture (dpy, context, surface);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "vaBeginPicture: %s", vaErrorStr (status));
    goto bail;
  }

  if (pic->params->len > 0) {
    status = vaRenderPicture (dpy, context, (VABufferID *) pic->params->data,
        pic->params->len);
    if (status != VA_STATUS_SUCCESS) {
      GST_WARNING_OBJECT (self, "vaRenderPicture: %s", vaErrorStr (status));
      goto fail_end_pic;
    }
  }

  status = vaEndPicture (dpy, context);
  ret = (status == VA_STATUS_SUCCESS);
  if (!ret)
    GST_WARNING_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));

bail:
  _destroy_all_buffers (pic);

  return ret;

fail_end_pic:
  {
    _destroy_all_buffers (pic);
    status = vaEndPicture (dpy, context);
    ret = FALSE;
    goto bail;
  }
}

VASurfaceID
gst_va_encode_picture_get_reconstruct_surface (GstVaEncodePicture * pic)
{
  g_return_val_if_fail (pic, VA_INVALID_ID);
  g_return_val_if_fail (pic->reconstruct_buffer, VA_INVALID_ID);

  return gst_va_buffer_get_surface (pic->reconstruct_buffer);
}

VASurfaceID
gst_va_encode_picture_get_raw_surface (GstVaEncodePicture * pic)
{
  g_return_val_if_fail (pic, VA_INVALID_ID);
  g_return_val_if_fail (pic->raw_buffer, VA_INVALID_ID);

  return gst_va_buffer_get_surface (pic->raw_buffer);
}

GstVaEncodePicture *
gst_va_encode_picture_new (GstVaEncoder * self, GstBuffer * raw_buffer)
{
  GstVaEncodePicture *pic;
  VABufferID coded_buffer;
  VADisplay dpy;
  VAStatus status;
  gint codedbuf_size;
  GstBufferPool *recon_pool = NULL;
  GstBuffer *reconstruct_buffer = NULL;
  GstFlowReturn ret;
  GstBufferPoolAcquireParams buffer_pool_params = {
    .flags = GST_BUFFER_POOL_ACQUIRE_FLAG_DONTWAIT,
  };

  g_return_val_if_fail (self && GST_IS_VA_ENCODER (self), NULL);
  g_return_val_if_fail (raw_buffer && GST_IS_BUFFER (raw_buffer), NULL);

  GST_OBJECT_LOCK (self);

  if (!_is_open_unlocked (self)) {
    GST_OBJECT_UNLOCK (self);
    GST_ERROR_OBJECT (self, "encoder has not been opened yet");
    return NULL;
  }

  if (self->codedbuf_size <= 0) {
    GST_ERROR_OBJECT (self, "codedbuf_size: %d, is invalid",
        self->codedbuf_size);
    GST_OBJECT_UNLOCK (self);
    return NULL;
  }
  codedbuf_size = self->codedbuf_size;

  recon_pool = gst_object_ref (self->recon_pool);

  GST_OBJECT_UNLOCK (self);

  ret = gst_buffer_pool_acquire_buffer (recon_pool, &reconstruct_buffer,
      &buffer_pool_params);
  gst_clear_object (&recon_pool);

  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to create the reconstruct picture");
    gst_clear_buffer (&reconstruct_buffer);
    return NULL;
  }

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, self->context, VAEncCodedBufferType,
      codedbuf_size, 1, NULL, &coded_buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    gst_clear_buffer (&reconstruct_buffer);
    return NULL;
  }

  pic = g_new (GstVaEncodePicture, 1);
  pic->raw_buffer = gst_buffer_ref (raw_buffer);
  pic->reconstruct_buffer = reconstruct_buffer;
  pic->coded_buffer = coded_buffer;

  pic->params = g_array_sized_new (FALSE, FALSE, sizeof (VABufferID), 8);

  return pic;
}

void
gst_va_encode_picture_free (GstVaEncodePicture * pic)
{
  GstVaDisplay *display;

  g_return_if_fail (pic);

  _destroy_all_buffers (pic);

  display = gst_va_buffer_peek_display (pic->raw_buffer);
  if (!display)
    return;

  if (pic->coded_buffer != VA_INVALID_ID)
    _destroy_buffer (display, pic->coded_buffer);

  gst_buffer_unref (pic->raw_buffer);
  gst_buffer_unref (pic->reconstruct_buffer);

  g_clear_pointer (&pic->params, g_array_unref);

  g_free (pic);
}

/* currently supported rate controls */
static const GEnumValue rate_control_map[] = {
  {VA_RC_CBR, "Constant Bitrate", "cbr"},
  {VA_RC_VBR, "Variable Bitrate", "vbr"},
  {VA_RC_VCM, "Video Conferencing Mode (Non HRD compliant)", "vcm"},
  {VA_RC_CQP, "Constant Quantizer", "cqp"},
  /* {VA_RC_VBR_CONSTRAINED, "VBR with peak rate higher than average bitrate", */
  /*  "vbr-constrained"}, */
  /* {VA_RC_ICQ, "Intelligent Constant Quality", "icq"}, */
  /* {VA_RC_MB, "Macroblock based rate control", "mb"}, */
  /* {VA_RC_CFS, "Constant Frame Size", "cfs"}, */
  /* {VA_RC_PARALLEL, "Parallel BRC", "parallel"}, */
  /* {VA_RC_QVBR, "Quality defined VBR", "qvbr"}, */
  /* {VA_RC_AVBR, "Average VBR", "avbr"}, */
};

static gint
_guint32_cmp (gconstpointer a, gconstpointer b)
{
  return *((const guint32 *) a) - *((const guint32 *) b);
}

gboolean
gst_va_encoder_get_rate_control_enum (GstVaEncoder * self,
    GEnumValue ratectl[16])
{
  guint i, j, k = 0;
  guint32 rc, rc_prev = 0;
  VAProfile profile;
  GArray *rcs;

  g_return_val_if_fail (GST_IS_VA_ENCODER (self), FALSE);

  /* reseve the number of supported rate controls per profile */
  rcs = g_array_sized_new (FALSE, FALSE, sizeof (guint32),
      G_N_ELEMENTS (rate_control_map) * self->available_profiles->len);

  for (i = 0; i < self->available_profiles->len; i++) {
    profile = g_array_index (self->available_profiles, VAProfile, i);
    rc = gst_va_encoder_get_rate_control_mode (self, profile, self->entrypoint);
    if (rc == 0)
      continue;

    for (j = 0; j < G_N_ELEMENTS (rate_control_map); j++) {
      if (rc & rate_control_map[j].value)
        rcs = g_array_append_val (rcs, rate_control_map[j].value);
    }
  }

  if (rcs->len == 0) {
    g_clear_pointer (&rcs, g_array_unref);
    return FALSE;
  }

  g_array_sort (rcs, _guint32_cmp);

  for (i = 0; i < rcs->len; i++) {
    rc = g_array_index (rcs, guint32, i);
    if (rc == rc_prev)
      continue;

    for (j = 0; j < G_N_ELEMENTS (rate_control_map); j++) {
      if (rc == rate_control_map[j].value && k < 15)
        ratectl[k++] = rate_control_map[j];
    }

    rc_prev = rc;
  }

  g_clear_pointer (&rcs, g_array_unref);
  if (k == 0)
    return FALSE;
  /* *INDENT-OFF* */
  ratectl[k] = (GEnumValue) { 0, NULL, NULL };
  /* *INDENT-ON* */
  return TRUE;
}
