/* GStreamer
 *  Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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
 * SECTION:element-vampeg2dec
 * @title: vampeg2dec
 * @short_description: A VA-API based Mpeg2 video decoder
 *
 * vampeg2dec decodes Mpeg2 bitstreams to VA surfaces using the
 * installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The decoding surfaces can be mapped onto main memory as video
 * frames.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=sample.mpg ! parsebin ! vampeg2dec ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvampeg2dec.h"

#include "gstvabasedec.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_mpeg2dec_debug);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_va_mpeg2dec_debug
#else
#define GST_CAT_DEFAULT NULL
#endif

#define GST_VA_MPEG2_DEC(obj)           ((GstVaMpeg2Dec *) obj)
#define GST_VA_MPEG2_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaMpeg2DecClass))
#define GST_VA_MPEG2_DEC_CLASS(klass)   ((GstVaMpeg2DecClass *) klass)

typedef struct _GstVaMpeg2Dec GstVaMpeg2Dec;
typedef struct _GstVaMpeg2DecClass GstVaMpeg2DecClass;

struct _GstVaMpeg2DecClass
{
  GstVaBaseDecClass parent_class;
};

struct _GstVaMpeg2Dec
{
  GstVaBaseDec parent;

  gboolean progressive;
  GstMpegVideoSequenceHdr seq;
};

static GstElementClass *parent_class = NULL;

/* *INDENT-OFF* */
static const gchar *src_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12 }");
/* *INDENT-ON* */

static const gchar *sink_caps_str = "video/x-mpeg2";

static VAProfile
_map_profile (GstMpegVideoProfile profile)
{
  VAProfile p = VAProfileNone;

  switch (profile) {
    case GST_MPEG_VIDEO_PROFILE_SIMPLE:
      p = VAProfileMPEG2Simple;
      break;
    case GST_MPEG_VIDEO_PROFILE_MAIN:
      p = VAProfileMPEG2Main;
      break;
    default:
      p = VAProfileNone;
      break;
  }

  return p;
}

static VAProfile
_get_profile (GstVaMpeg2Dec * self, GstMpegVideoProfile profile,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceScalableExt * seq_scalable_ext)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  VAProfile hw_profile;

  hw_profile = _map_profile (profile);
  if (hw_profile == VAProfileNone)
    return hw_profile;

  /* promote the profile if hw does not support, until we get one */
  do {
    if (gst_va_decoder_has_profile (base->decoder, hw_profile))
      return hw_profile;

    /* Otherwise, try to map to a higher profile */
    switch (profile) {
      case GST_MPEG_VIDEO_PROFILE_SIMPLE:
        hw_profile = VAProfileMPEG2Main;
        break;
      case GST_MPEG_VIDEO_PROFILE_HIGH:
        /* Try to map to main profile if no high profile specific bits used */
        if (!seq_scalable_ext && (seq_ext && seq_ext->chroma_format == 1)) {
          hw_profile = VAProfileMPEG2Main;
          break;
        }
        /* fall-through */
      default:
        GST_ERROR_OBJECT (self, "profile %d is unsupported.", profile);
        hw_profile = VAProfileNone;
        break;
    }
  } while (hw_profile != VAProfileNone);

  return hw_profile;
}

static guint
_get_rtformat (GstVaMpeg2Dec * self, GstMpegVideoChromaFormat chroma_format)
{
  guint ret = 0;

  switch (chroma_format) {
    case GST_MPEG_VIDEO_CHROMA_420:
      ret = VA_RT_FORMAT_YUV420;
      break;
    case GST_MPEG_VIDEO_CHROMA_422:
      ret = VA_RT_FORMAT_YUV422;
      break;
    case GST_MPEG_VIDEO_CHROMA_444:
      ret = VA_RT_FORMAT_YUV444;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unsupported chroma format: %d ", chroma_format);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_va_mpeg2_dec_new_sequence (GstMpeg2Decoder * decoder,
    const GstMpegVideoSequenceHdr * seq,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceDisplayExt * seq_display_ext,
    const GstMpegVideoSequenceScalableExt * seq_scalable_ext, gint max_dpb_size)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstVideoInfo *info = &base->output_info;
  VAProfile profile;
  GstMpegVideoProfile mpeg_profile;
  gboolean negotiation_needed = FALSE;
  guint rt_format;
  gint width, height;
  gboolean progressive;

  self->seq = *seq;

  width = seq->width;
  height = seq->height;
  if (seq_ext) {
    width = (width & 0x0fff) | ((guint32) seq_ext->horiz_size_ext << 12);
    height = (height & 0x0fff) | ((guint32) seq_ext->vert_size_ext << 12);
  }

  mpeg_profile = GST_MPEG_VIDEO_PROFILE_MAIN;
  if (seq_ext)
    mpeg_profile = seq_ext->profile;

  profile = _get_profile (self, mpeg_profile, seq_ext, seq_scalable_ext);
  if (profile == VAProfileNone)
    return GST_FLOW_NOT_NEGOTIATED;

  rt_format = _get_rtformat (self,
      seq_ext ? seq_ext->chroma_format : GST_MPEG_VIDEO_CHROMA_420);
  if (rt_format == 0)
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_va_decoder_config_is_equal (base->decoder, profile,
          rt_format, width, height)) {
    base->profile = profile;
    base->rt_format = rt_format;
    GST_VIDEO_INFO_WIDTH (info) = base->width = width;
    GST_VIDEO_INFO_HEIGHT (info) = base->height = height;

    negotiation_needed = TRUE;

    GST_INFO_OBJECT (self, "Format changed to %s [%x] (%dx%d)",
        gst_va_profile_name (profile), rt_format, base->width, base->height);
  }

  progressive = seq_ext ? seq_ext->progressive : 1;
  if (self->progressive != progressive) {
    self->progressive = progressive;
    GST_VIDEO_INFO_INTERLACE_MODE (info) = progressive ?
        GST_VIDEO_INTERLACE_MODE_PROGRESSIVE : GST_VIDEO_INTERLACE_MODE_MIXED;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Interlaced mode changed to %d", !progressive);
  }

  base->need_valign = FALSE;
  base->min_buffers = 2 + 4;    /* max num pic references + scratch surfaces */
  base->need_negotiation = negotiation_needed;
  g_clear_pointer (&base->input_state, gst_video_codec_state_unref);
  base->input_state = gst_video_codec_state_ref (decoder->input_state);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_mpeg2_dec_new_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture)
{
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *pic;
  GstFlowReturn ret;

  ret = gst_va_base_dec_prepare_output_frame (base, frame);
  if (ret != GST_FLOW_OK)
    goto error;

  pic = gst_va_decode_picture_new (base->decoder, frame->output_buffer);

  gst_mpeg2_picture_set_user_data (picture, pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  GST_LOG_OBJECT (self, "New va decode picture %p - %#x", pic,
      gst_va_decode_picture_get_surface (pic));

  return GST_FLOW_OK;

error:
  {
    GST_WARNING_OBJECT (self, "Failed to allocated output buffer, return %s",
        gst_flow_get_name (ret));
    return ret;
  }
}

static GstFlowReturn
gst_va_mpeg2_dec_new_field_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * first_field, GstMpeg2Picture * second_field)
{
  GstVaDecodePicture *first_pic, *second_pic;
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  first_pic = gst_mpeg2_picture_get_user_data (first_field);
  if (!first_pic)
    return GST_FLOW_ERROR;

  second_pic = gst_va_decode_picture_new (base->decoder, first_pic->gstbuffer);
  gst_mpeg2_picture_set_user_data (second_field, second_pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  GST_LOG_OBJECT (self, "New va decode picture %p - %#x", second_pic,
      gst_va_decode_picture_get_surface (second_pic));

  return GST_FLOW_OK;
}

static inline guint32
_pack_f_code (guint8 f_code[2][2])
{
  return (((guint32) f_code[0][0] << 12)
      | ((guint32) f_code[0][1] << 8)
      | ((guint32) f_code[1][0] << 4)
      | (f_code[1][1]));
}

static inline void
_copy_quant_matrix (guint8 dst[64], const guint8 src[64])
{
  memcpy (dst, src, 64);
}

static gboolean
gst_va_mpeg2_dec_add_quant_matrix (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstMpegVideoQuantMatrixExt *const quant_matrix = slice->quant_matrix;
  guint8 *intra_quant_matrix = NULL;
  guint8 *non_intra_quant_matrix = NULL;
  guint8 *chroma_intra_quant_matrix = NULL;
  guint8 *chroma_non_intra_quant_matrix = NULL;
  VAIQMatrixBufferMPEG2 iq_matrix = { 0 };
  GstVaDecodePicture *va_pic;

  intra_quant_matrix = self->seq.intra_quantizer_matrix;
  non_intra_quant_matrix = self->seq.non_intra_quantizer_matrix;

  if (quant_matrix) {
    if (quant_matrix->load_intra_quantiser_matrix)
      intra_quant_matrix = quant_matrix->intra_quantiser_matrix;
    if (quant_matrix->load_non_intra_quantiser_matrix)
      non_intra_quant_matrix = quant_matrix->non_intra_quantiser_matrix;
    if (quant_matrix->load_chroma_intra_quantiser_matrix)
      chroma_intra_quant_matrix = quant_matrix->chroma_intra_quantiser_matrix;
    if (quant_matrix->load_chroma_non_intra_quantiser_matrix)
      chroma_non_intra_quant_matrix =
          quant_matrix->chroma_non_intra_quantiser_matrix;
  }

  iq_matrix.load_intra_quantiser_matrix = intra_quant_matrix != NULL;
  if (intra_quant_matrix)
    _copy_quant_matrix (iq_matrix.intra_quantiser_matrix, intra_quant_matrix);

  iq_matrix.load_non_intra_quantiser_matrix = non_intra_quant_matrix != NULL;
  if (non_intra_quant_matrix)
    _copy_quant_matrix (iq_matrix.non_intra_quantiser_matrix,
        non_intra_quant_matrix);

  iq_matrix.load_chroma_intra_quantiser_matrix =
      chroma_intra_quant_matrix != NULL;
  if (chroma_intra_quant_matrix)
    _copy_quant_matrix (iq_matrix.chroma_intra_quantiser_matrix,
        chroma_intra_quant_matrix);

  iq_matrix.load_chroma_non_intra_quantiser_matrix =
      chroma_non_intra_quant_matrix != NULL;
  if (chroma_non_intra_quant_matrix)
    _copy_quant_matrix (iq_matrix.chroma_non_intra_quantiser_matrix,
        chroma_non_intra_quant_matrix);

  va_pic = gst_mpeg2_picture_get_user_data (picture);
  return gst_va_decoder_add_param_buffer (base->decoder, va_pic,
      VAIQMatrixBufferType, &iq_matrix, sizeof (iq_matrix));
}

static inline uint32_t
_is_frame_start (GstMpeg2Picture * picture)
{
  return (!picture->first_field
      || (picture->structure == GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME))
      ? 1 : 0;
}

static inline VASurfaceID
_get_surface_id (GstMpeg2Picture * picture)
{
  GstVaDecodePicture *va_pic;

  if (!picture)
    return VA_INVALID_ID;

  va_pic = gst_mpeg2_picture_get_user_data (picture);
  if (!va_pic)
    return VA_INVALID_ID;
  return gst_va_decode_picture_get_surface (va_pic);
}

static GstFlowReturn
gst_va_mpeg2_dec_start_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice,
    GstMpeg2Picture * prev_picture, GstMpeg2Picture * next_picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstVaDecodePicture *va_pic;
  VAPictureParameterBufferMPEG2 pic_param;

  va_pic = gst_mpeg2_picture_get_user_data (picture);

  /* *INDENT-OFF* */
  pic_param = (VAPictureParameterBufferMPEG2) {
    .horizontal_size = base->width,
    .vertical_size = base->height,
    .forward_reference_picture = VA_INVALID_ID,
    .backward_reference_picture = VA_INVALID_ID,
    .picture_coding_type = slice->pic_hdr->pic_type,
    .f_code = _pack_f_code (slice->pic_ext->f_code),
    .picture_coding_extension.bits = {
      .is_first_field = _is_frame_start (picture),
      .intra_dc_precision = slice->pic_ext->intra_dc_precision,
      .picture_structure = slice->pic_ext->picture_structure,
      .top_field_first = slice->pic_ext->top_field_first,
      .frame_pred_frame_dct = slice->pic_ext->frame_pred_frame_dct,
      .concealment_motion_vectors = slice->pic_ext->concealment_motion_vectors,
      .q_scale_type = slice->pic_ext->q_scale_type,
      .intra_vlc_format = slice->pic_ext->intra_vlc_format,
      .alternate_scan = slice->pic_ext->alternate_scan,
      .repeat_first_field = slice->pic_ext->repeat_first_field,
      .progressive_frame = slice->pic_ext->progressive_frame,
    },
  };
  /* *INDENT-ON* */

  switch (picture->type) {
    case GST_MPEG_VIDEO_PICTURE_TYPE_B:{
      VASurfaceID surface = _get_surface_id (next_picture);
      if (surface == VA_INVALID_ID) {
        GST_WARNING_OBJECT (self, "Missing the backward reference picture");
        if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, MESA_GALLIUM))
          return GST_FLOW_ERROR;
        else if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_IHD))
          surface = gst_va_decode_picture_get_surface (va_pic);
      }
      pic_param.backward_reference_picture = surface;
    }
      /* fall-through */
    case GST_MPEG_VIDEO_PICTURE_TYPE_P:{
      VASurfaceID surface = _get_surface_id (prev_picture);
      if (surface == VA_INVALID_ID) {
        GST_WARNING_OBJECT (self, "Missing the forward reference picture");
        if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, MESA_GALLIUM))
          return GST_FLOW_ERROR;
        else if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_IHD))
          surface = gst_va_decode_picture_get_surface (va_pic);
      }
      pic_param.forward_reference_picture = surface;
    }
    default:
      break;
  }

  if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
          VAPictureParameterBufferType, &pic_param, sizeof (pic_param)))
    return GST_FLOW_ERROR;

  if (!gst_va_mpeg2_dec_add_quant_matrix (decoder, picture, slice))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_mpeg2_dec_decode_slice (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstMpegVideoSliceHdr *header = &slice->header;
  GstMpegVideoPacket *packet = &slice->packet;
  GstVaDecodePicture *va_pic;
  VASliceParameterBufferMPEG2 slice_param;

  /* *INDENT-OFF* */
  slice_param = (VASliceParameterBufferMPEG2) {
    .slice_data_size = slice->size,
    .slice_data_offset = 0,
    .slice_data_flag = VA_SLICE_DATA_FLAG_ALL,
    .macroblock_offset = header->header_size + 32,
    .slice_horizontal_position = header->mb_column,
    .slice_vertical_position = header->mb_row,
    .quantiser_scale_code = header->quantiser_scale_code,
    .intra_slice_flag = header->intra_slice,
  };
  /* *INDENT-ON* */

  va_pic = gst_mpeg2_picture_get_user_data (picture);
  if (!gst_va_decoder_add_slice_buffer (base->decoder, va_pic,
          &slice_param, sizeof (slice_param),
          (guint8 *) (packet->data + slice->sc_offset), slice->size))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_mpeg2_dec_end_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *va_pic;

  GST_LOG_OBJECT (base, "end picture %p, (poc %d)",
      picture, picture->pic_order_cnt);

  va_pic = gst_mpeg2_picture_get_user_data (picture);

  if (!gst_va_decoder_decode (base->decoder, va_pic))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_mpeg2_dec_output_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaMpeg2Dec *self = GST_VA_MPEG2_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  gboolean ret;

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  ret = gst_va_base_dec_process_output (base, frame,
      GST_CODEC_PICTURE (picture)->discont_state, picture->buffer_flags);
  gst_mpeg2_picture_unref (picture);

  if (ret)
    return gst_video_decoder_finish_frame (vdec, frame);
  return GST_FLOW_ERROR;
}

static void
gst_va_mpeg2_dec_init (GTypeInstance * instance, gpointer g_class)
{
  gst_va_base_dec_init (GST_VA_BASE_DEC (instance), GST_CAT_DEFAULT);
  GST_VA_MPEG2_DEC (instance)->progressive = 1;
}

static void
gst_va_mpeg2_dec_dispose (GObject * object)
{
  gst_va_base_dec_close (GST_VIDEO_DECODER (object));
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_mpeg2_dec_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstMpeg2DecoderClass *mpeg2decoder_class = GST_MPEG2_DECODER_CLASS (g_class);
  struct CData *cdata = class_data;
  gchar *long_name;

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API Mpeg2 Decoder in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API Mpeg2 Decoder");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Video/Hardware",
      "VA-API based Mpeg2 video decoder", "He Junyan <junyan.he@intel.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_class);

  /**
   * GstVaMpeg2Dec:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   *
   * Since: 1.22
   */
  gst_va_base_dec_class_init (GST_VA_BASE_DEC_CLASS (g_class), MPEG2,
      cdata->render_device_path, cdata->sink_caps, cdata->src_caps,
      src_doc_caps, sink_doc_caps);

  gobject_class->dispose = gst_va_mpeg2_dec_dispose;

  mpeg2decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_new_sequence);
  mpeg2decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_new_picture);
  mpeg2decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_new_field_picture);
  mpeg2decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_start_picture);
  mpeg2decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_decode_slice);
  mpeg2decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_end_picture);
  mpeg2decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_va_mpeg2_dec_output_picture);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_mpeg2dec_debug, "vampeg2dec", 0,
      "VA Mpeg2 decoder");

  return NULL;
}

gboolean
gst_va_mpeg2_dec_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaMpeg2DecClass),
    .class_init = gst_va_mpeg2_dec_class_init,
    .instance_size = sizeof (GstVaMpeg2Dec),
    .instance_init = gst_va_mpeg2_dec_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (sink_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (src_caps), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaMpeg2Dec", "GstVa%sMpeg2Dec",
      &type_name, "vampeg2dec", "va%smpeg2dec", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_MPEG2_DECODER,
      type_name, &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
