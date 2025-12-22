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
 * SECTION:element-qsvjpegdec
 * @title: qsvjpegdec
 *
 * Intel Quick Sync JPEG decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/jpeg/file ! parsebin ! qsvjpegdec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstqsvjpegdec.h"
#include <string>
#include <string.h>
#include <vector>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_qsv_jpeg_dec_debug);
#define GST_CAT_DEFAULT gst_qsv_jpeg_dec_debug

#define DOC_SINK_CAPS \
    "image/jpeg, width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ]"

#define DOC_SRC_CAPS_COMM \
    "format = (string) { NV12, YUY2, BGRA }, " \
    "width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ]"

#define DOC_SRC_CAPS \
    "video/x-raw(memory:D3D11Memory), " DOC_SRC_CAPS_COMM "; " \
    "video/x-raw, " DOC_SRC_CAPS_COMM

typedef struct _GstQsvJpegDec
{
  GstQsvDecoder parent;
} GstQsvJpegDec;

typedef struct _GstQsvJpegDecClass
{
  GstQsvDecoderClass parent_class;
} GstQsvJpegDecClass;

static GTypeClass *parent_class = nullptr;

#define GST_QSV_JPEG_DEC(object) ((GstQsvJpegDec *) (object))
#define GST_QSV_JPEG_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstQsvJpegDecClass))

static void
gst_qsv_jpeg_dec_class_init (GstQsvJpegDecClass * klass, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstQsvDecoderClass *qsvdec_class = GST_QSV_DECODER_CLASS (klass);
  GstQsvDecoderClassData *cdata = (GstQsvDecoderClassData *) data;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);

#ifdef G_OS_WIN32
  std::string long_name = "Intel Quick Sync Video " +
      std::string (cdata->description) + " JPEG Decoder";

  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Decoder/Video/Hardware",
      "Intel Quick Sync Video JPEG Decoder",
      "Seungha Yang <seungha@centricular.com>");
#else
  gst_element_class_set_static_metadata (element_class,
      "Intel Quick Sync Video JPEG Decoder",
      "Codec/Decoder/Video/Hardware",
      "Intel Quick Sync Video JPEG Decoder",
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

  qsvdec_class->codec_id = MFX_CODEC_JPEG;
  qsvdec_class->impl_index = cdata->impl_index;
  qsvdec_class->adapter_luid = cdata->adapter_luid;
  qsvdec_class->display_path = cdata->display_path;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_qsv_jpeg_dec_init (GstQsvJpegDec * self)
{
}

void
gst_qsv_jpeg_dec_register (GstPlugin * plugin, guint rank, guint impl_index,
    GstObject * device, mfxSession session)
{
  mfxVideoParam param;
  mfxInfoMFX *mfx;
  GstQsvResolution max_resolution;
  std::vector < std::string > supported_formats;

  GST_DEBUG_CATEGORY_INIT (gst_qsv_jpeg_dec_debug, "qsvjpegdec", 0,
      "qsvjpegdec");

  memset (&param, 0, sizeof (mfxVideoParam));
  memset (&max_resolution, 0, sizeof (GstQsvResolution));

  param.AsyncDepth = 4;
  param.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

  mfx = &param.mfx;
  mfx->CodecId = MFX_CODEC_JPEG;

  mfx->FrameInfo.FrameRateExtN = 30;
  mfx->FrameInfo.FrameRateExtD = 1;
  mfx->FrameInfo.AspectRatioW = 1;
  mfx->FrameInfo.AspectRatioH = 1;
  mfx->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_NV12);
  mfx->CodecProfile = MFX_PROFILE_JPEG_BASELINE;
  mfx->JPEGChromaFormat = MFX_CHROMAFORMAT_YUV420;
  mfx->JPEGColorFormat = MFX_JPEG_COLORFORMAT_YCbCr;

  /* Check max-resolution */
  for (guint i = 0; i < G_N_ELEMENTS (gst_qsv_resolutions); i++) {
    mfx->FrameInfo.Width = GST_ROUND_UP_16 (gst_qsv_resolutions[i].width);
    mfx->FrameInfo.Height = GST_ROUND_UP_16 (gst_qsv_resolutions[i].height);
    mfx->FrameInfo.CropW = gst_qsv_resolutions[i].width;
    mfx->FrameInfo.CropH = gst_qsv_resolutions[i].height;

    if (MFXVideoDECODE_Query (session, &param, &param) != MFX_ERR_NONE)
      break;

    max_resolution.width = gst_qsv_resolutions[i].width;
    max_resolution.height = gst_qsv_resolutions[i].height;
  }

  if (max_resolution.width == 0 || max_resolution.height == 0)
    return;

  GST_INFO ("Maximum supported resolution: %dx%d",
      max_resolution.width, max_resolution.height);

  supported_formats.push_back ("NV12");

  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_YUY2);
  mfx->JPEGChromaFormat = MFX_CHROMAFORMAT_YUV422;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE)
    supported_formats.push_back ("YUY2");

  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_BGRA);
  mfx->JPEGChromaFormat = MFX_CHROMAFORMAT_YUV444;
  mfx->JPEGColorFormat = MFX_JPEG_COLORFORMAT_RGB;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE)
    supported_formats.push_back ("BGRA");

  /* To cover both landscape and portrait,
   * select max value (width in this case) */
  guint resolution = MAX (max_resolution.width, max_resolution.height);
  std::string src_caps_str = "video/x-raw";

  src_caps_str += ", width=(int) [ 1, " + std::to_string (resolution) + " ]";
  src_caps_str += ", height=(int) [ 1, " + std::to_string (resolution) + " ]";

  /* *INDENT-OFF* */
  if (supported_formats.size () > 1) {
    src_caps_str += ", format=(string) { ";
    bool first = true;
    for (const auto &iter: supported_formats) {
      if (!first) {
        src_caps_str += ", ";
      }

      src_caps_str += iter;
      first = false;
    }
    src_caps_str += " }";
  } else {
    src_caps_str += ", format=(string) " + supported_formats[0];
  }
  /* *INDENT-ON* */

  GstCaps *src_caps = gst_caps_from_string (src_caps_str.c_str ());

  /* TODO: Add support for VA */
#ifdef G_OS_WIN32
  GstCaps *d3d11_caps = gst_caps_copy (src_caps);
  GstCapsFeatures *caps_features =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr);
  gst_caps_set_features_simple (d3d11_caps, caps_features);
  gst_caps_append (d3d11_caps, src_caps);
  src_caps = d3d11_caps;
#endif
  std::string sink_caps_str = "image/jpeg";
  sink_caps_str += ", width=(int) [ 1, " + std::to_string (resolution) + " ]";
  sink_caps_str += ", height=(int) [ 1, " + std::to_string (resolution) + " ]";

  GstCaps *sink_caps = gst_caps_from_string (sink_caps_str.c_str ());

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GstQsvDecoderClassData *cdata = g_new0 (GstQsvDecoderClassData, 1);
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->impl_index = impl_index;

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
    sizeof (GstQsvJpegDecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_qsv_jpeg_dec_class_init,
    nullptr,
    cdata,
    sizeof (GstQsvJpegDec),
    0,
    (GInstanceInitFunc) gst_qsv_jpeg_dec_init,
  };

  type_name = g_strdup ("GstQsvJpegDec");
  feature_name = g_strdup ("qsvjpegdec");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstQsvJPEGDevice%dDec", index);
    feature_name = g_strdup_printf ("qsvjpegdevice%ddec", index);
  }

  type = g_type_register_static (GST_TYPE_QSV_DECODER, type_name, &type_info,
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
