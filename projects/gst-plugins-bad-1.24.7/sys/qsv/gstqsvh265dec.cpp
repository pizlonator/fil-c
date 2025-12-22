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
 * SECTION:element-qsvh265dec
 * @title: qsvh264dec
 *
 * Intel Quick Sync H.265 decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h265/file ! parsebin ! qsvh265dec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstqsvh265dec.h"
#include <gst/codecparsers/gsth265parser.h>
#include <string>
#include <string.h>
#include <vector>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_qsv_h265_dec_debug);
#define GST_CAT_DEFAULT gst_qsv_h265_dec_debug

#define DOC_SINK_CAPS \
    "video/x-h265, width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ], " \
    "stream-format = (string) { byte-stream, hev1, hvc1 }, " \
    "alignment = (string) au, profile = (string) { main, main-10, main-12, " \
    "main-422-10, main-422-12, main-444, main-444-10, main-444-12 }"

#define DOC_SRC_CAPS_COMM \
    "format = (string) { NV12, P010_10LE, P012_LE, YUY2, Y210, Y212_LE, " \
    "VUYA, RBGA, Y410, BGR10A2_LE, Y412_LE, BGRA64_LE }, " \
    "width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ]"

#define DOC_SRC_CAPS \
    "video/x-raw(memory:D3D11Memory), " DOC_SRC_CAPS_COMM "; " \
    "video/x-raw, " DOC_SRC_CAPS_COMM

typedef struct _GstQsvH265Dec
{
  GstQsvDecoder parent;
  GstH265Parser *parser;
  gboolean packetized;
  gboolean nal_length_size;

  GstBuffer *vps_nals[GST_H265_MAX_VPS_COUNT];
  GstBuffer *sps_nals[GST_H265_MAX_SPS_COUNT];
  GstBuffer *pps_nals[GST_H265_MAX_PPS_COUNT];
} GstQsvH265Dec;

typedef struct _GstQsvH265DecClass
{
  GstQsvDecoderClass parent_class;
} GstQsvH265DecClass;

static GTypeClass *parent_class = nullptr;

#define GST_QSV_H265_DEC(object) ((GstQsvH265Dec *) (object))
#define GST_QSV_H265_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstQsvH265DecClass))

static gboolean gst_qsv_h265_dec_start (GstVideoDecoder * decoder);
static gboolean gst_qsv_h265_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_qsv_h265_dec_set_format (GstQsvDecoder * decoder,
    GstVideoCodecState * state);
static GstBuffer *gst_qsv_h265_dec_process_input (GstQsvDecoder * decoder,
    gboolean need_codec_data, GstBuffer * buffer);

static void
gst_qsv_h265_dec_class_init (GstQsvH265DecClass * klass, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *videodec_class = GST_VIDEO_DECODER_CLASS (klass);
  GstQsvDecoderClass *qsvdec_class = GST_QSV_DECODER_CLASS (klass);
  GstQsvDecoderClassData *cdata = (GstQsvDecoderClassData *) data;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);

#ifdef G_OS_WIN32
  std::string long_name = "Intel Quick Sync Video " +
      std::string (cdata->description) + " H.265 Decoder";

  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Decoder/Video/Hardware",
      "Intel Quick Sync Video H.265 Decoder",
      "Seungha Yang <seungha@centricular.com>");
#else
  gst_element_class_set_static_metadata (element_class,
      "Intel Quick Sync Video H.265 Decoder",
      "Codec/Decoder/Video/Hardware",
      "Intel Quick Sync Video H.265 Decoder",
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

  videodec_class->start = GST_DEBUG_FUNCPTR (gst_qsv_h265_dec_start);
  videodec_class->stop = GST_DEBUG_FUNCPTR (gst_qsv_h265_dec_stop);

  qsvdec_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_h265_dec_set_format);
  qsvdec_class->process_input =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_dec_process_input);

  qsvdec_class->codec_id = MFX_CODEC_HEVC;
  qsvdec_class->impl_index = cdata->impl_index;
  qsvdec_class->adapter_luid = cdata->adapter_luid;
  qsvdec_class->display_path = cdata->display_path;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_qsv_h265_dec_init (GstQsvH265Dec * self)
{
}

static gboolean
gst_qsv_h265_dec_start (GstVideoDecoder * decoder)
{
  GstQsvH265Dec *self = GST_QSV_H265_DEC (decoder);

  self->parser = gst_h265_parser_new ();

  return TRUE;
}

static void
gst_qsv_h265_dec_clear_codec_data (GstQsvH265Dec * self)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (self->vps_nals); i++)
    gst_clear_buffer (&self->vps_nals[i]);

  for (i = 0; i < G_N_ELEMENTS (self->sps_nals); i++)
    gst_clear_buffer (&self->sps_nals[i]);

  for (i = 0; i < G_N_ELEMENTS (self->pps_nals); i++)
    gst_clear_buffer (&self->pps_nals[i]);
}

static gboolean
gst_qsv_h265_dec_stop (GstVideoDecoder * decoder)
{
  GstQsvH265Dec *self = GST_QSV_H265_DEC (decoder);

  gst_qsv_h265_dec_clear_codec_data (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static void
gst_qsv_h265_dec_store_nal (GstQsvH265Dec * self, guint id,
    GstH265NalUnitType nal_type, GstH265NalUnit * nalu)
{
  GstBuffer *buf, **store;
  guint size = nalu->size, store_size;
  static const guint8 start_code[] = { 0, 0, 1 };

  if (nal_type == GST_H265_NAL_VPS) {
    store_size = GST_H265_MAX_VPS_COUNT;
    store = self->vps_nals;
    GST_DEBUG_OBJECT (self, "storing vps %u", id);
  } else if (nal_type == GST_H265_NAL_SPS) {
    store_size = GST_H265_MAX_SPS_COUNT;
    store = self->sps_nals;
    GST_DEBUG_OBJECT (self, "storing sps %u", id);
  } else if (nal_type == GST_H265_NAL_PPS) {
    store_size = GST_H265_MAX_PPS_COUNT;
    store = self->pps_nals;
    GST_DEBUG_OBJECT (self, "storing pps %u", id);
  } else {
    return;
  }

  if (id >= store_size) {
    GST_DEBUG_OBJECT (self, "unable to store nal, id out-of-range %d", id);
    return;
  }

  buf = gst_buffer_new_allocate (nullptr, size + sizeof (start_code), nullptr);
  gst_buffer_fill (buf, 0, start_code, sizeof (start_code));
  gst_buffer_fill (buf, sizeof (start_code), nalu->data + nalu->offset, size);

  if (store[id])
    gst_buffer_unref (store[id]);

  store[id] = buf;
}

static gboolean
gst_qsv_h265_dec_parse_codec_data (GstQsvH265Dec * self, const guint8 * data,
    gsize size)
{
  GstH265Parser *parser = self->parser;
  GstH265ParserResult pres;
  GstH265VPS vps;
  GstH265SPS sps;
  GstH265PPS pps;
  gboolean ret = FALSE;
  GstH265DecoderConfigRecord *config = nullptr;

  pres = gst_h265_parser_parse_decoder_config_record (parser,
      data, size, &config);
  if (pres != GST_H265_PARSER_OK) {
    GST_WARNING_OBJECT (self, "Failed to parse hvcC data");
    return FALSE;
  }

  self->nal_length_size = config->length_size_minus_one + 1;
  GST_DEBUG_OBJECT (self, "nal length size %u", self->nal_length_size);

  for (guint i = 0; i < config->nalu_array->len; i++) {
    GstH265DecoderConfigRecordNalUnitArray *array =
        &g_array_index (config->nalu_array,
        GstH265DecoderConfigRecordNalUnitArray, i);

    for (guint j = 0; j < array->nalu->len; j++) {
      GstH265NalUnit *nalu = &g_array_index (array->nalu, GstH265NalUnit, j);

      switch (nalu->type) {
        case GST_H265_NAL_VPS:
          pres = gst_h265_parser_parse_vps (parser, nalu, &vps);
          if (pres != GST_H265_PARSER_OK) {
            GST_WARNING_OBJECT (self, "Failed to parse VPS");
            goto out;
          }

          gst_qsv_h265_dec_store_nal (self, vps.id,
              (GstH265NalUnitType) nalu->type, nalu);
          break;
        case GST_H265_NAL_SPS:
          pres = gst_h265_parser_parse_sps (self->parser, nalu, &sps, FALSE);
          if (pres != GST_H265_PARSER_OK) {
            GST_WARNING_OBJECT (self, "Failed to parse SPS");
            goto out;
          }

          gst_qsv_h265_dec_store_nal (self, sps.id,
              (GstH265NalUnitType) nalu->type, nalu);
          break;
        case GST_H265_NAL_PPS:
          pres = gst_h265_parser_parse_pps (parser, nalu, &pps);
          if (pres != GST_H265_PARSER_OK) {
            GST_WARNING_OBJECT (self, "Failed to parse PPS");
            goto out;
          }

          gst_qsv_h265_dec_store_nal (self, pps.id,
              (GstH265NalUnitType) nalu->type, nalu);
          break;
        default:
          break;
      }
    }
  }

  ret = TRUE;

out:
  gst_h265_decoder_config_record_free (config);
  return ret;
}

static gboolean
gst_qsv_h265_dec_set_format (GstQsvDecoder * decoder,
    GstVideoCodecState * state)
{
  GstQsvH265Dec *self = GST_QSV_H265_DEC (decoder);
  GstStructure *s;
  const gchar *str;
  GstMapInfo map;

  gst_qsv_h265_dec_clear_codec_data (self);
  self->packetized = FALSE;

  s = gst_caps_get_structure (state->caps, 0);
  str = gst_structure_get_string (s, "stream-format");
  if ((g_strcmp0 (str, "hev1") == 0 || g_strcmp0 (str, "hvc1") == 0) &&
      state->codec_data) {
    self->packetized = TRUE;
    /* Will be updated */
    self->nal_length_size = 4;
  }

  if (!self->packetized)
    return TRUE;

  if (!gst_buffer_map (state->codec_data, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map codec data");
    return FALSE;
  }

  gst_qsv_h265_dec_parse_codec_data (self, map.data, map.size);
  gst_buffer_unmap (state->codec_data, &map);

  return TRUE;
}

static GstBuffer *
gst_qsv_h265_dec_process_input (GstQsvDecoder * decoder,
    gboolean need_codec_data, GstBuffer * buffer)
{
  GstQsvH265Dec *self = GST_QSV_H265_DEC (decoder);
  GstH265Parser *parser = self->parser;
  GstH265NalUnit nalu;
  GstH265ParserResult pres;
  GstMapInfo map;
  GstH265VPS vps;
  GstH265SPS sps;
  GstH265PPS pps;
  gboolean have_vps = FALSE;
  gboolean have_sps = FALSE;
  gboolean have_pps = FALSE;
  guint i;
  GstBuffer *new_buf;
  static const guint8 start_code[] = { 0, 0, 1 };

  if (!self->packetized)
    return gst_buffer_ref (buffer);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map input buffer");
    return nullptr;
  }

  memset (&nalu, 0, sizeof (GstH265NalUnit));
  new_buf = gst_buffer_new ();

  do {
    GstMemory *mem;
    guint8 *data;
    gsize size;

    pres = gst_h265_parser_identify_nalu_hevc (parser, map.data,
        nalu.offset + nalu.size, map.size, self->nal_length_size, &nalu);

    if (pres == GST_H265_PARSER_NO_NAL_END)
      pres = GST_H265_PARSER_OK;

    switch (nalu.type) {
      case GST_H265_NAL_VPS:
        pres = gst_h265_parser_parse_vps (parser, &nalu, &vps);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_vps = TRUE;
        gst_qsv_h265_dec_store_nal (self, vps.id,
            (GstH265NalUnitType) nalu.type, &nalu);
        break;
      case GST_H265_NAL_SPS:
        pres = gst_h265_parser_parse_sps (parser, &nalu, &sps, FALSE);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_sps = TRUE;
        gst_qsv_h265_dec_store_nal (self,
            sps.id, (GstH265NalUnitType) nalu.type, &nalu);
        break;
      case GST_H265_NAL_PPS:
        pres = gst_h265_parser_parse_pps (parser, &nalu, &pps);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_pps = TRUE;
        gst_qsv_h265_dec_store_nal (self,
            pps.id, (GstH265NalUnitType) nalu.type, &nalu);
        break;
      default:
        break;
    }

    size = sizeof (start_code) + nalu.size;
    data = (guint8 *) g_malloc (size);
    memcpy (data, start_code, sizeof (start_code));
    memcpy (data + sizeof (start_code), nalu.data + nalu.offset, nalu.size);

    mem = gst_memory_new_wrapped ((GstMemoryFlags) 0, data, size, 0, size,
        data, (GDestroyNotify) g_free);
    gst_buffer_append_memory (new_buf, mem);
  } while (pres == GST_H265_PARSER_OK);

  gst_buffer_unmap (buffer, &map);

  if (need_codec_data) {
    GstBuffer *tmp = gst_buffer_new ();

    if (!have_vps) {
      for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
        if (!self->vps_nals[i])
          continue;

        tmp = gst_buffer_append (tmp, gst_buffer_ref (self->vps_nals[i]));
      }
    }

    if (!have_sps) {
      for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
        if (!self->sps_nals[i])
          continue;

        tmp = gst_buffer_append (tmp, gst_buffer_ref (self->sps_nals[i]));
      }
    }

    if (!have_pps) {
      for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
        if (!self->pps_nals[i])
          continue;

        tmp = gst_buffer_append (tmp, gst_buffer_ref (self->pps_nals[i]));
      }
    }

    new_buf = gst_buffer_append (tmp, new_buf);
  }

  return new_buf;
}

void
gst_qsv_h265_dec_register (GstPlugin * plugin, guint rank, guint impl_index,
    GstObject * device, mfxSession session)
{
  mfxVideoParam param;
  mfxInfoMFX *mfx;
  GstQsvResolution max_resolution;
  std::vector < std::string > supported_profiles;
  std::vector < std::string > supported_formats;

  GST_DEBUG_CATEGORY_INIT (gst_qsv_h265_dec_debug,
      "qsvh265dec", 0, "qsvh265dec");

  memset (&param, 0, sizeof (mfxVideoParam));
  memset (&max_resolution, 0, sizeof (GstQsvResolution));

  param.AsyncDepth = 4;
  param.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

  mfx = &param.mfx;
  mfx->CodecId = MFX_CODEC_HEVC;

  mfx->FrameInfo.FrameRateExtN = 30;
  mfx->FrameInfo.FrameRateExtD = 1;
  mfx->FrameInfo.AspectRatioW = 1;
  mfx->FrameInfo.AspectRatioH = 1;
  mfx->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_NV12);
  mfx->CodecProfile = MFX_PROFILE_HEVC_MAIN;

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

  supported_profiles.push_back ("main");
  supported_formats.push_back ("NV12");

  /* main-10 */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_P010_10LE);
  mfx->CodecProfile = MFX_PROFILE_HEVC_MAIN10;
  mfx->FrameInfo.Width = GST_ROUND_UP_16 (gst_qsv_resolutions[0].width);
  mfx->FrameInfo.Height = GST_ROUND_UP_16 (gst_qsv_resolutions[0].height);
  mfx->FrameInfo.CropW = gst_qsv_resolutions[0].width;
  mfx->FrameInfo.CropH = gst_qsv_resolutions[0].height;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    supported_profiles.push_back ("main-10");
    supported_formats.push_back ("P010_10LE");
  }

  /* main-12 */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_P012_LE);
  mfx->CodecProfile = MFX_PROFILE_HEVC_REXT;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    supported_profiles.push_back ("main-12");
    supported_formats.push_back ("P012_LE");
  }

  /* main-422-{10,12} */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_YUY2);
  mfx->CodecProfile = MFX_PROFILE_HEVC_REXT;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_Y210);
    if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
      supported_profiles.push_back ("main-422-10");
      supported_formats.push_back ("YUY2");
      supported_formats.push_back ("Y210");

      gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_Y212_LE);
      if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
        supported_profiles.push_back ("main-422-12");
        supported_formats.push_back ("Y212_LE");
      }
    }
  }

  /* main-444 */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_VUYA);
  mfx->CodecProfile = MFX_PROFILE_HEVC_REXT;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    supported_profiles.push_back ("main-444");
    supported_formats.push_back ("VUYA");
    supported_formats.push_back ("RBGA");
  }

  /* main-444-10 */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_Y410);
  mfx->CodecProfile = MFX_PROFILE_HEVC_REXT;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    supported_profiles.push_back ("main-444-10");
    supported_formats.push_back ("Y410");
    supported_formats.push_back ("BGR10A2_LE");
  }

  /* main-444-12 */
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_Y412_LE);
  mfx->CodecProfile = MFX_PROFILE_HEVC_REXT;
  if (MFXVideoDECODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    supported_profiles.push_back ("main-444-12");
    supported_formats.push_back ("Y412_LE");
    supported_formats.push_back ("BGRA64_LE");
  }

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

  std::string sink_caps_str = "video/x-h265";
  sink_caps_str += ", width=(int) [ 1, " + std::to_string (resolution) + " ]";
  sink_caps_str += ", height=(int) [ 1, " + std::to_string (resolution) + " ]";

  sink_caps_str += ", stream-format=(string) { byte-stream, hev1, hvc1 }";
  sink_caps_str += ", alignment=(string) au";
  /* *INDENT-OFF* */
  if (supported_profiles.size () > 1) {
    sink_caps_str += ", profile=(string) { ";
    bool first = true;
    for (const auto &iter: supported_profiles) {
      if (!first) {
        sink_caps_str += ", ";
      }

      sink_caps_str += iter;
      first = false;
    }
    sink_caps_str += " }";
  } else {
    sink_caps_str += ", profile=(string) " + supported_profiles[0];
  }
  /* *INDENT-ON* */

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
    sizeof (GstQsvH265DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_qsv_h265_dec_class_init,
    nullptr,
    cdata,
    sizeof (GstQsvH265Dec),
    0,
    (GInstanceInitFunc) gst_qsv_h265_dec_init,
  };

  type_name = g_strdup ("GstQsvH265Dec");
  feature_name = g_strdup ("qsvh265dec");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstQsvH265Device%dDec", index);
    feature_name = g_strdup_printf ("qsvh265device%ddec", index);
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
