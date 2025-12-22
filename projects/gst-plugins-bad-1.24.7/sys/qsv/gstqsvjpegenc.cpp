/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-qsvjpegenc
 * @title: qsvvp9enc
 *
 * Intel Quick Sync JPEG encoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! qsvjpegenc ! qtmux ! filesink location=out.mp4
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstqsvjpegenc.h"
#include <vector>
#include <string>
#include <set>
#include <string.h>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_qsv_jpeg_enc_debug);
#define GST_CAT_DEFAULT gst_qsv_jpeg_enc_debug

enum
{
  PROP_0,
  PROP_QUALITY,
};

#define DEFAULT_JPEG_QUALITY 85

#define DOC_SINK_CAPS_COMM \
    "format = (string) { NV12, YUY2, BGRA }, " \
    "width = (int) [ 16, 16384 ], height = (int) [ 16, 16384 ]"

#define DOC_SINK_CAPS \
    "video/x-raw(memory:D3D11Memory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw(memory:VAMemory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw, " DOC_SINK_CAPS_COMM

#define DOC_SRC_CAPS \
    "image/jpeg, width = (int) [ 16, 16384 ], height = (int) [ 16, 16384 ]"

typedef struct _GstQsvJpegEncClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint impl_index;
  gint64 adapter_luid;
  gchar *display_path;
  gchar *description;
  gboolean interlaved;
} GstQsvJpegEncClassData;

typedef struct _GstQsvJpegEnc
{
  GstQsvEncoder parent;

  GMutex prop_lock;
  /* protected by prop_lock */
  gboolean property_updated;

  /* properties */
  guint quality;
} GstQsvJpegEnc;

typedef struct _GstQsvJpegEncClass
{
  GstQsvEncoderClass parent_class;
  gboolean interlaved;
} GstQsvJpegEncClass;

static GstElementClass *parent_class = nullptr;

#define GST_QSV_JPEG_ENC(object) ((GstQsvJpegEnc *) (object))
#define GST_QSV_JPEG_ENC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstQsvJpegEncClass))

static void gst_qsv_jpeg_enc_finalize (GObject * object);
static void gst_qsv_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qsv_jpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_qsv_jpeg_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param,
    GPtrArray * extra_params);
static gboolean gst_qsv_jpeg_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session);
static GstQsvEncoderReconfigure
gst_qsv_jpeg_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params);

static void
gst_qsv_jpeg_enc_class_init (GstQsvJpegEncClass * klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstQsvEncoderClass *qsvenc_class = GST_QSV_ENCODER_CLASS (klass);
  GstQsvJpegEncClassData *cdata = (GstQsvJpegEncClassData *) data;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  qsvenc_class->codec_id = MFX_CODEC_JPEG;
  qsvenc_class->impl_index = cdata->impl_index;
  qsvenc_class->adapter_luid = cdata->adapter_luid;
  qsvenc_class->display_path = cdata->display_path;

  object_class->finalize = gst_qsv_jpeg_enc_finalize;
  object_class->set_property = gst_qsv_jpeg_enc_set_property;
  object_class->get_property = gst_qsv_jpeg_enc_get_property;

  g_object_class_install_property (object_class, PROP_QUALITY,
      g_param_spec_uint ("quality", "Quality",
          "Encoding quality, 100 for best quality",
          1, 100, DEFAULT_JPEG_QUALITY, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

#ifdef G_OS_WIN32
  std::string long_name = "Intel Quick Sync Video " +
      std::string (cdata->description) + " JPEG Encoder";

  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video JPEG Encoder",
      "Seungha Yang <seungha@centricular.com>");
#else
  gst_element_class_set_static_metadata (element_class,
      "Intel Quick Sync Video JPEG Encoder",
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video JPEG Encoder",
      "Seungha Yang <seungha@centricular.com>");
#endif

  pad_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps);
  doc_caps = gst_caps_from_string (DOC_SINK_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  pad_templ = gst_pad_template_new ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps);
  doc_caps = gst_caps_from_string (DOC_SRC_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  qsvenc_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_jpeg_enc_set_format);
  qsvenc_class->set_output_state =
      GST_DEBUG_FUNCPTR (gst_qsv_jpeg_enc_set_output_state);
  qsvenc_class->check_reconfigure =
      GST_DEBUG_FUNCPTR (gst_qsv_jpeg_enc_check_reconfigure);

  klass->interlaved = cdata->interlaved;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata->description);
  g_free (cdata);
}

static void
gst_qsv_jpeg_enc_init (GstQsvJpegEnc * self)
{
  self->quality = DEFAULT_JPEG_QUALITY;

  g_mutex_init (&self->prop_lock);
}

static void
gst_qsv_jpeg_enc_finalize (GObject * object)
{
  GstQsvJpegEnc *self = GST_QSV_JPEG_ENC (object);

  g_mutex_clear (&self->prop_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qsv_jpeg_enc_check_update_uint (GstQsvJpegEnc * self, guint * old_val,
    guint new_val)
{
  if (*old_val == new_val)
    return;

  *old_val = new_val;
  self->property_updated = TRUE;
}

static void
gst_qsv_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQsvJpegEnc *self = GST_QSV_JPEG_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_QUALITY:
      gst_qsv_jpeg_enc_check_update_uint (self, &self->quality,
          g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static void
gst_qsv_jpeg_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstQsvJpegEnc *self = GST_QSV_JPEG_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_QUALITY:
      g_value_set_uint (value, self->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static gboolean
gst_qsv_jpeg_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvJpegEnc *self = GST_QSV_JPEG_ENC (encoder);
  GstQsvJpegEncClass *klass = GST_QSV_JPEG_ENC_GET_CLASS (self);
  GstVideoInfo *info = &state->info;
  mfxFrameInfo *frame_info;
  GstVideoFormat format;

  frame_info = &param->mfx.FrameInfo;

  frame_info->Width = frame_info->CropW = info->width;
  frame_info->Height = frame_info->CropH = info->height;

  frame_info->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  if (GST_VIDEO_INFO_FPS_N (info) > 0 && GST_VIDEO_INFO_FPS_D (info) > 0) {
    frame_info->FrameRateExtN = GST_VIDEO_INFO_FPS_N (info);
    frame_info->FrameRateExtD = GST_VIDEO_INFO_FPS_D (info);
  } else {
    /* HACK: Same as x264enc */
    frame_info->FrameRateExtN = 25;
    frame_info->FrameRateExtD = 1;
  }

  frame_info->AspectRatioW = GST_VIDEO_INFO_PAR_N (info);
  frame_info->AspectRatioH = GST_VIDEO_INFO_PAR_D (info);

  format = GST_VIDEO_INFO_FORMAT (info);
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_BGRA:
      gst_qsv_frame_info_set_format (frame_info, format);
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected format %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));
      return FALSE;
  }

  g_mutex_lock (&self->prop_lock);
  param->mfx.CodecId = MFX_CODEC_JPEG;
  param->mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
  param->mfx.Quality = self->quality;
  if (klass->interlaved)
    param->mfx.Interleaved = 1;
  else
    param->mfx.Interleaved = 0;
  param->mfx.RestartInterval = 0;
  param->ExtParam = (mfxExtBuffer **) extra_params->pdata;
  param->NumExtParam = extra_params->len;

  self->property_updated = FALSE;
  g_mutex_unlock (&self->prop_lock);

  return TRUE;
}

static gboolean
gst_qsv_jpeg_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session)
{
  GstCaps *caps;
  GstTagList *tags;
  GstVideoCodecState *out_state;

  caps = gst_caps_from_string ("image/jpeg");
  out_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      caps, state);
  gst_video_codec_state_unref (out_state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "qsvjpegenc",
      nullptr);

  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder),
      tags, GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

static GstQsvEncoderReconfigure
gst_qsv_jpeg_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvJpegEnc *self = GST_QSV_JPEG_ENC (encoder);
  GstQsvEncoderReconfigure ret = GST_QSV_ENCODER_RECONFIGURE_NONE;

  g_mutex_lock (&self->prop_lock);
  if (self->property_updated)
    ret = GST_QSV_ENCODER_RECONFIGURE_FULL;

  self->property_updated = FALSE;
  g_mutex_unlock (&self->prop_lock);

  return ret;
}

void
gst_qsv_jpeg_enc_register (GstPlugin * plugin, guint rank, guint impl_index,
    GstObject * device, mfxSession session)
{
  mfxVideoParam param;
  mfxInfoMFX *mfx;
  std::vector < std::string > supported_formats;
  GstQsvResolution max_resolution;
  mfxStatus status;
  gboolean interlaved = TRUE;

  GST_DEBUG_CATEGORY_INIT (gst_qsv_jpeg_enc_debug,
      "qsvjpegenc", 0, "qsvjpegenc");

  memset (&param, 0, sizeof (mfxVideoParam));
  memset (&max_resolution, 0, sizeof (GstQsvResolution));

  param.AsyncDepth = 4;
  param.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  mfx = &param.mfx;
  mfx->LowPower = MFX_CODINGOPTION_UNKNOWN;
  mfx->CodecId = MFX_CODEC_JPEG;
  mfx->CodecProfile = MFX_PROFILE_JPEG_BASELINE;
  mfx->Quality = DEFAULT_JPEG_QUALITY;
  mfx->Interleaved = 1;
  mfx->RestartInterval = 0;

  mfx->FrameInfo.Width = mfx->FrameInfo.CropW = GST_ROUND_UP_16 (320);
  mfx->FrameInfo.Height = mfx->FrameInfo.CropH = GST_ROUND_UP_16 (240);

  mfx->FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  mfx->FrameInfo.FourCC = MFX_FOURCC_NV12;
  mfx->FrameInfo.FrameRateExtN = 30;
  mfx->FrameInfo.FrameRateExtD = 1;
  mfx->FrameInfo.AspectRatioW = 1;
  mfx->FrameInfo.AspectRatioH = 1;
  mfx->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  status = MFXVideoENCODE_Query (session, &param, &param);
  if (status == MFX_WRN_PARTIAL_ACCELERATION) {
    mfx->Interleaved = 0;
    interlaved = FALSE;

    status = MFXVideoENCODE_Query (session, &param, &param);
  }

  if (status != MFX_ERR_NONE)
    return;

  supported_formats.push_back ("NV12");

  mfx->FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
  mfx->FrameInfo.FourCC = MFX_FOURCC_YUY2;
  status = MFXVideoENCODE_Query (session, &param, &param);
  if (status == MFX_ERR_NONE)
    supported_formats.push_back ("YUY2");

  mfx->FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
  mfx->FrameInfo.FourCC = MFX_FOURCC_RGB4;
  status = MFXVideoENCODE_Query (session, &param, &param);

  if (status == MFX_ERR_NONE)
    supported_formats.push_back ("BGRA");

  mfx->FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  mfx->FrameInfo.FourCC = MFX_FOURCC_NV12;

  /* Check max-resolution */
  for (guint i = 0; i < G_N_ELEMENTS (gst_qsv_resolutions); i++) {
    mfx->FrameInfo.Width = mfx->FrameInfo.CropW = gst_qsv_resolutions[i].width;
    mfx->FrameInfo.Height = mfx->FrameInfo.CropH =
        gst_qsv_resolutions[i].height;

    if (MFXVideoENCODE_Query (session, &param, &param) != MFX_ERR_NONE)
      break;

    max_resolution.width = gst_qsv_resolutions[i].width;
    max_resolution.height = gst_qsv_resolutions[i].height;
  }

  if (max_resolution.width == 0 || max_resolution.height == 0)
    return;

  GST_INFO ("Maximum supported resolution: %dx%d",
      max_resolution.width, max_resolution.height);

  /* To cover both landscape and portrait,
   * select max value (width in this case) */
  guint resolution = MAX (max_resolution.width, max_resolution.height);
  std::string sink_caps_str = "video/x-raw";

  sink_caps_str += ", width=(int) [ 16, " + std::to_string (resolution) + " ]";
  sink_caps_str += ", height=(int) [ 16, " + std::to_string (resolution) + " ]";

  /* *INDENT-OFF* */
  if (supported_formats.size () > 1) {
    sink_caps_str += ", format=(string) { ";
    bool first = true;
    for (const auto &iter: supported_formats) {
      if (!first) {
        sink_caps_str += ", ";
      }

      sink_caps_str += iter;
      first = false;
    }
    sink_caps_str += " }";
  } else {
    sink_caps_str += ", format=(string) " + supported_formats[0];
  }
  /* *INDENT-ON* */

  GstCaps *sink_caps = gst_caps_from_string (sink_caps_str.c_str ());

#ifdef G_OS_WIN32
  GstCaps *d3d11_caps = gst_caps_copy (sink_caps);
  GstCapsFeatures *caps_features =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr);
  gst_caps_set_features_simple (d3d11_caps, caps_features);
  gst_caps_append (d3d11_caps, sink_caps);
  sink_caps = d3d11_caps;
#else
  GstCaps *va_caps = gst_caps_copy (sink_caps);
  GstCapsFeatures *caps_features =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_VA, nullptr);
  gst_caps_set_features_simple (va_caps, caps_features);
  gst_caps_append (va_caps, sink_caps);
  sink_caps = va_caps;
#endif

  std::string src_caps_str = "image/jpeg";
  src_caps_str += ", width=(int) [ 16, " + std::to_string (resolution) + " ]";
  src_caps_str += ", height=(int) [ 16, " + std::to_string (resolution) + " ]";

  GstCaps *src_caps = gst_caps_from_string (src_caps_str.c_str ());

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GstQsvJpegEncClassData *cdata = g_new0 (GstQsvJpegEncClassData, 1);
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->impl_index = impl_index;
  cdata->interlaved = interlaved;

#ifdef G_OS_WIN32
  g_object_get (device, "adapter-luid", &cdata->adapter_luid,
      "description", &cdata->description, nullptr);
#else
  g_object_get (device, "path", &cdata->display_path, nullptr);
#endif

  GType type;
  gchar *type_name;
  gchar *feature_name;
  GTypeInfo type_info = {
    sizeof (GstQsvJpegEncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_qsv_jpeg_enc_class_init,
    nullptr,
    cdata,
    sizeof (GstQsvJpegEnc),
    0,
    (GInstanceInitFunc) gst_qsv_jpeg_enc_init,
  };

  type_name = g_strdup ("GstQsvJpegEnc");
  feature_name = g_strdup ("qsvjpegenc");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstQsvJpegDevice%dEnc", index);
    feature_name = g_strdup_printf ("qsvjpegdevice%denc", index);
  }

  type = g_type_register_static (GST_TYPE_QSV_ENCODER, type_name, &type_info,
      (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
