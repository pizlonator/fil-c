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

#include "gstvadecoder.h"

#include <gst/va/gstva.h>
#include <gst/va/gstvavideoformat.h>

#include "gstvacaps.h"
#include "gstvadisplay_priv.h"
#include "gstvaprofile.h"

struct _GstVaDecoder
{
  GstObject parent;

  GArray *available_profiles;
  GstCaps *srcpad_caps;
  GstCaps *sinkpad_caps;
  GstVaDisplay *display;
  VAConfigID config;
  VAContextID context;
  VAProfile profile;
  guint rt_format;
  gint coded_width;
  gint coded_height;
};

GST_DEBUG_CATEGORY_STATIC (gst_va_decoder_debug);
#define GST_CAT_DEFAULT gst_va_decoder_debug

#define gst_va_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVaDecoder, gst_va_decoder, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_va_decoder_debug, "vadecoder", 0,
        "VA Decoder"));

enum
{
  PROP_DISPLAY = 1,
  PROP_PROFILE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_CHROMA,
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES];

static gboolean _destroy_buffers (GstVaDecodePicture * pic);

static void
gst_va_decoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaDecoder *self = GST_VA_DECODER (object);

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
gst_va_decoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVaDecoder *self = GST_VA_DECODER (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;
    case PROP_PROFILE:
      g_value_set_int (value, self->profile);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_decoder_dispose (GObject * object)
{
  GstVaDecoder *self = GST_VA_DECODER (object);

  if (!gst_va_decoder_close (self))
    GST_WARNING_OBJECT (self, "VaDecoder is not successfully closed");

  g_clear_pointer (&self->available_profiles, g_array_unref);
  gst_clear_object (&self->display);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_decoder_class_init (GstVaDecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_va_decoder_set_property;
  gobject_class->get_property = gst_va_decoder_get_property;
  gobject_class->dispose = gst_va_decoder_dispose;

  g_properties[PROP_DISPLAY] =
      g_param_spec_object ("display", "GstVaDisplay", "GstVaDisplay object",
      GST_TYPE_VA_DISPLAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_PROFILE] =
      g_param_spec_int ("va-profile", "VAProfile", "VA Profile",
      VAProfileNone, 50, VAProfileNone,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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

  g_object_class_install_properties (gobject_class, N_PROPERTIES, g_properties);
}

static void
gst_va_decoder_init (GstVaDecoder * self)
{
  self->profile = VAProfileNone;
  self->config = VA_INVALID_ID;
  self->context = VA_INVALID_ID;
  self->rt_format = 0;
  self->coded_width = 0;
  self->coded_height = 0;
}

static gboolean
gst_va_decoder_initialize (GstVaDecoder * self, guint32 codec)
{
  if (self->available_profiles)
    return FALSE;

  self->available_profiles = gst_va_display_get_profiles (self->display, codec,
      VAEntrypointVLD);

  return (self->available_profiles != NULL);
}

GstVaDecoder *
gst_va_decoder_new (GstVaDisplay * display, guint32 codec)
{
  GstVaDecoder *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), NULL);

  self = g_object_new (GST_TYPE_VA_DECODER, "display", display, NULL);
  gst_object_ref_sink (self);

  if (!gst_va_decoder_initialize (self, codec))
    gst_clear_object (&self);

  return self;
}

gboolean
gst_va_decoder_is_open (GstVaDecoder * self)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  GST_OBJECT_LOCK (self);
  ret = (self->config != VA_INVALID_ID && self->profile != VAProfileNone);
  GST_OBJECT_UNLOCK (self);
  return ret;
}

gboolean
gst_va_decoder_open (GstVaDecoder * self, VAProfile profile, guint rt_format)
{
  VAConfigAttrib attrib = {
    .type = VAConfigAttribRTFormat,
    .value = rt_format,
  };
  VAConfigID config;
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (gst_va_decoder_is_open (self))
    return TRUE;

  if (!gst_va_decoder_has_profile (self, profile)) {
    GST_ERROR_OBJECT (self, "Unsupported profile: %s",
        gst_va_profile_name (profile));
    return FALSE;
  }

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateConfig (dpy, profile, VAEntrypointVLD, &attrib, 1, &config);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateConfig: %s", vaErrorStr (status));
    return FALSE;
  }

  GST_OBJECT_LOCK (self);
  self->config = config;
  self->profile = profile;
  self->rt_format = rt_format;
  GST_OBJECT_UNLOCK (self);

  /* now we should return now only this profile's caps */
  gst_caps_replace (&self->srcpad_caps, NULL);

  return TRUE;
}

gboolean
gst_va_decoder_close (GstVaDecoder * self)
{
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (!gst_va_decoder_is_open (self))
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

  GST_OBJECT_LOCK (self);
  gst_va_decoder_init (self);
  GST_OBJECT_UNLOCK (self);

  gst_caps_replace (&self->srcpad_caps, NULL);
  gst_caps_replace (&self->sinkpad_caps, NULL);

  return TRUE;
}

gboolean
gst_va_decoder_set_frame_size_with_surfaces (GstVaDecoder * self,
    gint coded_width, gint coded_height, GArray * surfaces)
{
  VAContextID context;
  VADisplay dpy;
  VAStatus status;
  VASurfaceID *render_targets = NULL;
  gint num_render_targets = 0;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  GST_OBJECT_LOCK (self);
  if (self->context != VA_INVALID_ID) {
    GST_OBJECT_UNLOCK (self);
    GST_INFO_OBJECT (self, "decoder already has a context");
    return TRUE;
  }
  GST_OBJECT_UNLOCK (self);

  if (!gst_va_decoder_is_open (self)) {
    GST_ERROR_OBJECT (self, "decoder has not been opened yet");
    return FALSE;
  }

  if (surfaces) {
    num_render_targets = surfaces->len;
    render_targets = (VASurfaceID *) surfaces->data;
  }

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaCreateContext (dpy, self->config, coded_width, coded_height,
      VA_PROGRESSIVE, render_targets, num_render_targets, &context);

  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateContext: %s", vaErrorStr (status));
    return FALSE;
  }

  GST_OBJECT_LOCK (self);
  self->context = context;
  self->coded_width = coded_width;
  self->coded_height = coded_height;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

gboolean
gst_va_decoder_set_frame_size (GstVaDecoder * self, gint coded_width,
    gint coded_height)
{
  return gst_va_decoder_set_frame_size_with_surfaces (self, coded_width,
      coded_height, NULL);
}

/* This function is only used by codecs where frame size can change
 * without a context reset, as for example VP9 */
gboolean
gst_va_decoder_update_frame_size (GstVaDecoder * self, gint coded_width,
    gint coded_height)
{
  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (!gst_va_decoder_is_open (self)) {
    GST_ERROR_OBJECT (self, "decoder has not been opened yet");
    return FALSE;
  }

  GST_OBJECT_LOCK (self);
  if (self->context == VA_INVALID_ID) {
    GST_OBJECT_UNLOCK (self);
    GST_INFO_OBJECT (self, "decoder does not have a context");
    return FALSE;
  }
  GST_OBJECT_UNLOCK (self);


  GST_OBJECT_LOCK (self);
  self->coded_width = coded_width;
  self->coded_height = coded_height;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_get_codec_caps (GstVaDecoder * self)
{
  GstCaps *sinkpad_caps = NULL, *srcpad_caps = NULL;

  if (!gst_va_decoder_is_open (self)
      && GST_IS_VA_DISPLAY_WRAPPED (self->display)) {
    if (gst_va_caps_from_profiles (self->display, self->available_profiles,
            VAEntrypointVLD, &sinkpad_caps, &srcpad_caps)) {
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
gst_va_decoder_get_srcpad_caps (GstVaDecoder * self)
{
  GstCaps *srcpad_caps = NULL;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (g_atomic_pointer_get (&self->srcpad_caps))
    return gst_caps_ref (self->srcpad_caps);

  if (_get_codec_caps (self))
    return gst_caps_ref (self->srcpad_caps);

  if (gst_va_decoder_is_open (self)) {
    srcpad_caps = gst_va_create_raw_caps_from_config (self->display,
        self->config);
    if (!srcpad_caps) {
      GST_WARNING_OBJECT (self, "Invalid configuration caps");
      return NULL;
    }
    gst_caps_replace (&self->srcpad_caps, srcpad_caps);
    gst_caps_unref (srcpad_caps);

    return gst_caps_ref (self->srcpad_caps);
  }

  return NULL;
}

GstCaps *
gst_va_decoder_get_sinkpad_caps (GstVaDecoder * self)
{
  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (g_atomic_pointer_get (&self->sinkpad_caps))
    return gst_caps_ref (self->sinkpad_caps);

  if (_get_codec_caps (self))
    return gst_caps_ref (self->sinkpad_caps);

  return NULL;
}

gboolean
gst_va_decoder_has_profile (GstVaDecoder * self, VAProfile profile)
{
  gint i;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (profile == VAProfileNone)
    return FALSE;

  for (i = 0; i < self->available_profiles->len; i++) {
    if (g_array_index (self->available_profiles, VAProfile, i) == profile)
      return TRUE;
  }

  return FALSE;
}

gint
gst_va_decoder_get_mem_types (GstVaDecoder * self)
{
  VASurfaceAttrib *attribs;
  guint i, attrib_count;
  gint ret = 0;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), 0);

  if (!gst_va_decoder_is_open (self))
    return 0;

  attribs = gst_va_get_surface_attribs (self->display, self->config,
      &attrib_count);
  if (!attribs)
    return 0;

  for (i = 0; i < attrib_count; i++) {
    if (attribs[i].value.type != VAGenericValueTypeInteger)
      continue;
    switch (attribs[i].type) {
      case VASurfaceAttribMemoryType:
        ret = attribs[i].value.value.i;
        break;
      default:
        break;
    }
  }

  g_free (attribs);

  return ret;
}

GArray *
gst_va_decoder_get_surface_formats (GstVaDecoder * self)
{
  GArray *formats;
  GstVideoFormat format;
  VASurfaceAttrib *attribs;
  guint i, attrib_count;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), NULL);

  if (!gst_va_decoder_is_open (self))
    return NULL;

  attribs = gst_va_get_surface_attribs (self->display, self->config,
      &attrib_count);
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

gboolean
gst_va_decoder_add_param_buffer (GstVaDecoder * self, GstVaDecodePicture * pic,
    gint type, gpointer data, gsize size)
{
  VABufferID buffer;
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);
  g_return_val_if_fail (self->context != VA_INVALID_ID, FALSE);
  g_return_val_if_fail (pic && data && size > 0, FALSE);

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, self->context, type, size, 1, data, &buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return FALSE;
  }

  g_array_append_val (pic->buffers, buffer);
  return TRUE;
}

gboolean
gst_va_decoder_add_slice_buffer_with_n_params (GstVaDecoder * self,
    GstVaDecodePicture * pic, gpointer params_data, gsize params_size,
    guint params_num, gpointer slice_data, gsize slice_size)
{
  VABufferID params_buffer, slice_buffer;
  VADisplay dpy;
  VAStatus status;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);
  g_return_val_if_fail (self->context != VA_INVALID_ID, FALSE);
  g_return_val_if_fail (pic && slice_data && slice_size > 0
      && params_data && params_size > 0, FALSE);

  dpy = gst_va_display_get_va_dpy (self->display);
  status = vaCreateBuffer (dpy, self->context, VASliceParameterBufferType,
      params_size, params_num, params_data, &params_buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return FALSE;
  }

  status = vaCreateBuffer (dpy, self->context, VASliceDataBufferType,
      slice_size, 1, slice_data, &slice_buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "vaCreateBuffer: %s", vaErrorStr (status));
    return FALSE;
  }

  g_array_append_val (pic->slices, params_buffer);
  g_array_append_val (pic->slices, slice_buffer);

  return TRUE;
}

gboolean
gst_va_decoder_add_slice_buffer (GstVaDecoder * self, GstVaDecodePicture * pic,
    gpointer params_data, gsize params_size, gpointer slice_data,
    gsize slice_size)
{
  return gst_va_decoder_add_slice_buffer_with_n_params (self, pic, params_data,
      params_size, 1, slice_data, slice_size);
}

gboolean
gst_va_decoder_decode_with_aux_surface (GstVaDecoder * self,
    GstVaDecodePicture * pic, gboolean use_aux)
{
  VADisplay dpy;
  VAStatus status;
  VASurfaceID surface = VA_INVALID_ID;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);
  g_return_val_if_fail (self->context != VA_INVALID_ID, FALSE);
  g_return_val_if_fail (pic, FALSE);

  if (use_aux) {
    surface = gst_va_decode_picture_get_aux_surface (pic);
  } else {
    surface = gst_va_decode_picture_get_surface (pic);
  }
  if (surface == VA_INVALID_ID) {
    GST_ERROR_OBJECT (self, "Decode picture without VASurfaceID");
    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Decode to surface %#x", surface);

  dpy = gst_va_display_get_va_dpy (self->display);

  status = vaBeginPicture (dpy, self->context, surface);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING_OBJECT (self, "vaBeginPicture: %s", vaErrorStr (status));
    goto fail_end_pic;
  }

  if (pic->buffers->len > 0) {
    status = vaRenderPicture (dpy, self->context,
        (VABufferID *) pic->buffers->data, pic->buffers->len);
    if (status != VA_STATUS_SUCCESS) {
      GST_WARNING_OBJECT (self, "vaRenderPicture: %s", vaErrorStr (status));
      goto fail_end_pic;
    }
  }

  if (pic->slices->len > 0) {
    status = vaRenderPicture (dpy, self->context,
        (VABufferID *) pic->slices->data, pic->slices->len);
    if (status != VA_STATUS_SUCCESS) {
      GST_WARNING_OBJECT (self, "vaRenderPicture: %s", vaErrorStr (status));
      goto fail_end_pic;
    }
  }

  status = vaEndPicture (dpy, self->context);
  if (status != VA_STATUS_SUCCESS)
    GST_WARNING_OBJECT (self, "vaEndPicture: %s", vaErrorStr (status));
  else
    ret = TRUE;

bail:
  _destroy_buffers (pic);

  return ret;

fail_end_pic:
  {
    status = vaEndPicture (dpy, self->context);
    goto bail;
  }
}

gboolean
gst_va_decoder_decode (GstVaDecoder * self, GstVaDecodePicture * pic)
{
  return gst_va_decoder_decode_with_aux_surface (self, pic, FALSE);
}

gboolean
gst_va_decoder_config_is_equal (GstVaDecoder * self, VAProfile new_profile,
    guint new_rtformat, gint new_width, gint new_height)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  /* @TODO: Check if current buffers are large enough, and reuse
   * them */
  GST_OBJECT_LOCK (self);
  ret = (self->profile == new_profile && self->rt_format == new_rtformat
      && self->coded_width == new_width && self->coded_height == new_height);
  GST_OBJECT_UNLOCK (self);

  return ret;
}

gboolean
gst_va_decoder_get_config (GstVaDecoder * self, VAProfile * profile,
    guint * rt_format, gint * width, gint * height)
{
  g_return_val_if_fail (GST_IS_VA_DECODER (self), FALSE);

  if (!gst_va_decoder_is_open (self))
    return FALSE;

  GST_OBJECT_LOCK (self);
  if (profile)
    *profile = self->profile;
  if (rt_format)
    *rt_format = self->rt_format;
  if (width)
    *width = self->coded_width;
  if (height)
    *height = self->coded_height;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_destroy_buffers (GstVaDecodePicture * pic)
{
  GstVaDisplay *display;
  VABufferID buffer;
  VADisplay dpy;
  VAStatus status;
  guint i;
  gboolean ret = TRUE;

  display = gst_va_buffer_peek_display (pic->gstbuffer);
  if (!display)
    return FALSE;
  dpy = gst_va_display_get_va_dpy (display);

  if (pic->buffers) {
    for (i = 0; i < pic->buffers->len; i++) {
      buffer = g_array_index (pic->buffers, VABufferID, i);
      status = vaDestroyBuffer (dpy, buffer);
      if (status != VA_STATUS_SUCCESS) {
        ret = FALSE;
        GST_WARNING ("Failed to destroy parameter buffer: %s",
            vaErrorStr (status));
      }
    }

    pic->buffers = g_array_set_size (pic->buffers, 0);
  }

  if (pic->slices) {
    for (i = 0; i < pic->slices->len; i++) {
      buffer = g_array_index (pic->slices, VABufferID, i);
      status = vaDestroyBuffer (dpy, buffer);
      if (status != VA_STATUS_SUCCESS) {
        ret = FALSE;
        GST_WARNING ("Failed to destroy slice buffer: %s", vaErrorStr (status));
      }
    }

    pic->slices = g_array_set_size (pic->slices, 0);
  }

  return ret;
}

GstVaDecodePicture *
gst_va_decode_picture_new (GstVaDecoder * self, GstBuffer * buffer)
{
  GstVaDecodePicture *pic;

  g_return_val_if_fail (buffer && GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (self && GST_IS_VA_DECODER (self), NULL);

  pic = g_new (GstVaDecodePicture, 1);
  pic->gstbuffer = gst_buffer_ref (buffer);
  pic->buffers = g_array_sized_new (FALSE, FALSE, sizeof (VABufferID), 16);
  pic->slices = g_array_sized_new (FALSE, FALSE, sizeof (VABufferID), 64);

  return pic;
}

VASurfaceID
gst_va_decode_picture_get_surface (GstVaDecodePicture * pic)
{
  g_return_val_if_fail (pic, VA_INVALID_ID);
  g_return_val_if_fail (pic->gstbuffer, VA_INVALID_ID);

  return gst_va_buffer_get_surface (pic->gstbuffer);
}

VASurfaceID
gst_va_decode_picture_get_aux_surface (GstVaDecodePicture * pic)
{
  g_return_val_if_fail (pic, VA_INVALID_ID);
  g_return_val_if_fail (pic->gstbuffer, VA_INVALID_ID);

  return gst_va_buffer_get_aux_surface (pic->gstbuffer);
}

void
gst_va_decode_picture_free (GstVaDecodePicture * pic)
{
  g_return_if_fail (pic);

  _destroy_buffers (pic);

  gst_buffer_unref (pic->gstbuffer);
  g_clear_pointer (&pic->buffers, g_array_unref);
  g_clear_pointer (&pic->slices, g_array_unref);

  g_free (pic);
}

GstVaDecodePicture *
gst_va_decode_picture_dup (GstVaDecodePicture * pic)
{
  GstVaDecodePicture *dup;

  g_return_val_if_fail (pic, NULL);

  dup = g_new0 (GstVaDecodePicture, 1);

  /* dups only need gstbuffer */
  dup->gstbuffer = gst_buffer_ref (pic->gstbuffer);
  return dup;
}
