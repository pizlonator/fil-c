/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION: element-msdkmjpegenc
 * @title: msdkmjpegenc
 * @short_description: Intel MSDK MJPEG encoder
 *
 * MJPEG video encoder based on Intel MFX
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=1 ! msdkmjpegenc ! jpegparse ! filesink location=output.jpg
 * ```
 *
 * Since: 1.12
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_MFX_MFXDEFS_H
#  include <mfx/mfxstructures.h>
#  include <mfx/mfxjpeg.h>
#else
#  include "mfxstructures.h"
#  include "mfxjpeg.h"
#endif

#include "gstmsdkmjpegenc.h"

GST_DEBUG_CATEGORY_EXTERN (gst_msdkmjpegenc_debug);
#define GST_CAT_DEFAULT gst_msdkmjpegenc_debug

#define GST_MSDKMJPEGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkMJPEGEnc))
#define GST_MSDKMJPEGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkMJPEGEncClass))
#define GST_IS_MSDKMJPEGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKMJPEGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

enum
{
  PROP_0,
  PROP_QUALITY
};

#define DEFAULT_QUALITY 85

/* *INDENT-OFF* */
static const gchar *doc_sink_caps_str =
    GST_VIDEO_CAPS_MAKE ("{ NV12, YUY2, BGRA }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, YUY2, BGRA }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory", "{ NV12 }");
/* *INDENT-ON* */

static const gchar *doc_src_caps_str = "image/jpeg";

static GstElementClass *parent_class = NULL;

static gboolean
gst_msdkmjpegenc_set_format (GstMsdkEnc * encoder)
{
  return TRUE;
}

static gboolean
gst_msdkmjpegenc_configure (GstMsdkEnc * encoder)
{
  GstMsdkMJPEGEnc *mjpegenc = GST_MSDKMJPEGENC (encoder);

  encoder->param.mfx.CodecId = MFX_CODEC_JPEG;
  encoder->param.mfx.Quality = mjpegenc->quality;
  encoder->param.mfx.Interleaved = 1;
  encoder->param.mfx.RestartInterval = 0;
  encoder->param.mfx.BufferSizeInKB = 3072;

  return TRUE;
}

static GstCaps *
gst_msdkmjpegenc_set_src_caps (GstMsdkEnc * encoder)
{
  GstCaps *caps;

  caps = gst_caps_from_string ("image/jpeg");

  return caps;
}

static void
gst_msdkmjpegenc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMsdkMJPEGEnc *thiz = GST_MSDKMJPEGENC (object);

  GST_OBJECT_LOCK (thiz);
  switch (prop_id) {
    case PROP_QUALITY:
      g_value_set_uint (value, thiz->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
}

static void
gst_msdkmjpegenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkMJPEGEnc *thiz = GST_MSDKMJPEGENC (object);

  GST_OBJECT_LOCK (thiz);
  switch (prop_id) {
    case PROP_QUALITY:
      thiz->quality = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
}

static gboolean
gst_msdkmjpegenc_is_format_supported (GstMsdkEnc * encoder,
    GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      return TRUE;
    default:
      return FALSE;
  }
}

static void
gst_msdkmjpegenc_class_init (gpointer klass, gpointer data)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstMsdkEncClass *encoder_class;
  MsdkEncCData *cdata = data;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  encoder_class = GST_MSDKENC_CLASS (klass);

  encoder_class->set_format = gst_msdkmjpegenc_set_format;
  encoder_class->configure = gst_msdkmjpegenc_configure;
  encoder_class->set_src_caps = gst_msdkmjpegenc_set_src_caps;
  encoder_class->is_format_supported = gst_msdkmjpegenc_is_format_supported;

  gobject_class->get_property = gst_msdkmjpegenc_get_property;
  gobject_class->set_property = gst_msdkmjpegenc_set_property;

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_uint ("quality", "Quality", "Quality of encoding",
          0, 100, DEFAULT_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK MJPEG encoder",
      "Codec/Encoder/Video/Hardware",
      "MJPEG video encoder based on " MFX_API_SDK,
      "Scott D Phillips <scott.d.phillips@intel.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkmjpegenc_init (GTypeInstance * instance, gpointer g_class)
{
  GstMsdkMJPEGEnc *thiz = GST_MSDKMJPEGENC (instance);
  thiz->quality = DEFAULT_QUALITY;
}

gboolean
gst_msdkmjpegenc_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkEncCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkMJPEGEncClass),
    .class_init = gst_msdkmjpegenc_class_init,
    .instance_size = sizeof (GstMsdkMJPEGEnc),
    .instance_init = gst_msdkmjpegenc_init
  };

  cdata = g_new (MsdkEncCData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkMJPEGEnc");
  feature_name = g_strdup ("msdkmjpegenc");

  type = g_type_register_static (GST_TYPE_MSDKENC, type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
