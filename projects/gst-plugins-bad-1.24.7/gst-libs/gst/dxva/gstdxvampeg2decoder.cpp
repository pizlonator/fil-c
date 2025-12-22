/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdxvampeg2decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_mpeg2_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_mpeg2_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaMpeg2DecoderPrivate
{
  DXVA_PictureParameters pic_params;
  DXVA_QmatrixData iq_matrix;

  std::vector<DXVA_SliceInfo> slice_list;
  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  gboolean disable_postproc = FALSE;

  gboolean submit_iq_data;

  gint width = 0;
  gint height = 0;
  guint width_in_mb = 0;
  guint height_in_mb = 0;
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstMpegVideoSequenceHdr seq;
  GstMpegVideoProfile profile = GST_MPEG_VIDEO_PROFILE_MAIN;
  gboolean interlaced = FALSE;

  gboolean configured = FALSE;
};
/* *INDENT-ON* */

static void gst_dxva_mpeg2_decoder_finalize (GObject * object);

static gboolean gst_dxva_mpeg2_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn gst_dxva_mpeg2_decoder_new_sequence (GstMpeg2Decoder *
    decoder, const GstMpegVideoSequenceHdr * seq,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceDisplayExt * seq_display_ext,
    const GstMpegVideoSequenceScalableExt * seq_scalable_ext,
    gint max_dpb_size);
static GstFlowReturn
gst_dxva_mpeg2_decoder_new_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture);
static GstFlowReturn
gst_dxva_mpeg2_decoder_new_field_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * first_field, GstMpeg2Picture * second_field);
static GstFlowReturn
gst_dxva_mpeg2_decoder_start_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice,
    GstMpeg2Picture * prev_picture, GstMpeg2Picture * next_picture);
static GstFlowReturn
gst_dxva_mpeg2_decoder_decode_slice (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice);
static GstFlowReturn
gst_dxva_mpeg2_decoder_end_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture);
static GstFlowReturn
gst_dxva_mpeg2_decoder_output_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture);

#define gst_dxva_mpeg2_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaMpeg2Decoder,
    gst_dxva_mpeg2_decoder, GST_TYPE_MPEG2_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_mpeg2_decoder_debug, "dxvampeg2decoder",
        0, "dxvampeg2decoder"));

static void
gst_dxva_mpeg2_decoder_class_init (GstDxvaMpeg2DecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstMpeg2DecoderClass *mpeg2decoder_class = GST_MPEG2_DECODER_CLASS (klass);

  gobject_class->finalize = gst_dxva_mpeg2_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_start);

  mpeg2decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_new_sequence);
  mpeg2decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_new_picture);
  mpeg2decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_new_field_picture);
  mpeg2decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_start_picture);
  mpeg2decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_decode_slice);
  mpeg2decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_end_picture);
  mpeg2decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_mpeg2_decoder_output_picture);
}

static void
gst_dxva_mpeg2_decoder_init (GstDxvaMpeg2Decoder * self)
{
  self->priv = new GstDxvaMpeg2DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_mpeg2_decoder_finalize (GObject * object)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (object);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_mpeg2_decoder_reset (GstDxvaMpeg2Decoder * self)
{
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;

  priv->width = 0;
  priv->height = 0;
  priv->width_in_mb = 0;
  priv->height_in_mb = 0;
  priv->out_format = GST_VIDEO_FORMAT_UNKNOWN;
  priv->profile = GST_MPEG_VIDEO_PROFILE_MAIN;
  priv->interlaced = FALSE;
  priv->configured = FALSE;
}

static gboolean
gst_dxva_mpeg2_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);

  gst_dxva_mpeg2_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_new_sequence (GstMpeg2Decoder * decoder,
    const GstMpegVideoSequenceHdr * seq,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceDisplayExt * seq_display_ext,
    const GstMpegVideoSequenceScalableExt * seq_scalable_ext, gint max_dpb_size)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);
  gboolean interlaced;
  gboolean modified = FALSE;
  gint width, height;
  GstMpegVideoProfile mpeg_profile;
  GstVideoInfo info;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  interlaced = seq_ext ? !seq_ext->progressive : FALSE;
  if (priv->interlaced != interlaced) {
    GST_INFO_OBJECT (self, "interlaced sequence change, %d -> %d",
        priv->interlaced, interlaced);
    priv->interlaced = interlaced;
    modified = TRUE;
  }

  width = seq->width;
  height = seq->height;
  if (seq_ext) {
    width = (width & 0x0fff) | ((guint32) seq_ext->horiz_size_ext << 12);
    height = (height & 0x0fff) | ((guint32) seq_ext->vert_size_ext << 12);
  }

  if (priv->width != width || priv->height != height) {
    GST_INFO_OBJECT (self, "resolution change %dx%d -> %dx%d",
        priv->width, priv->height, width, height);
    priv->width = width;
    priv->height = height;
    priv->width_in_mb = GST_ROUND_UP_16 (width) >> 4;
    priv->height_in_mb = GST_ROUND_UP_16 (height) >> 4;
    modified = TRUE;
  }

  mpeg_profile = GST_MPEG_VIDEO_PROFILE_MAIN;
  if (seq_ext)
    mpeg_profile = (GstMpegVideoProfile) seq_ext->profile;

  if (mpeg_profile != GST_MPEG_VIDEO_PROFILE_MAIN &&
      mpeg_profile != GST_MPEG_VIDEO_PROFILE_SIMPLE) {
    GST_ERROR_OBJECT (self, "Cannot support profile %d", mpeg_profile);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (priv->profile != mpeg_profile) {
    GST_INFO_OBJECT (self, "Profile change %d -> %d",
        priv->profile, mpeg_profile);
    priv->profile = mpeg_profile;
    modified = TRUE;
  }

  if (!modified && priv->configured)
    return GST_FLOW_OK;

  priv->out_format = GST_VIDEO_FORMAT_NV12;

  gst_video_info_set_interlaced_format (&info,
      priv->out_format, priv->interlaced ? GST_VIDEO_INTERLACE_MODE_MIXED :
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, priv->width, priv->height);

  g_assert (klass->configure);
  ret = klass->configure (self, decoder->input_state, &info, 0, 0, priv->width,
      priv->height, max_dpb_size);

  if (ret == GST_FLOW_OK) {
    priv->configured = TRUE;
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_WARNING_OBJECT (self, "Couldn't negotiate with new sequence");
      ret = GST_FLOW_NOT_NEGOTIATED;
    }
  } else {
    priv->configured = FALSE;
  }

  return ret;
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_new_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_new_field_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * first_field, GstMpeg2Picture * second_field)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);

  g_assert (klass->duplicate_picture);

  return klass->duplicate_picture (self, GST_CODEC_PICTURE (first_field),
      GST_CODEC_PICTURE (second_field));
}

static inline WORD
_pack_f_codes (guint8 f_code[2][2])
{
  return (((WORD) f_code[0][0] << 12)
      | ((WORD) f_code[0][1] << 8)
      | ((WORD) f_code[1][0] << 4)
      | (f_code[1][1]));
}

static inline WORD
_pack_pce_elements (GstMpeg2Slice * slice)
{
  return (((WORD) slice->pic_ext->intra_dc_precision << 14)
      | ((WORD) slice->pic_ext->picture_structure << 12)
      | ((WORD) slice->pic_ext->top_field_first << 11)
      | ((WORD) slice->pic_ext->frame_pred_frame_dct << 10)
      | ((WORD) slice->pic_ext->concealment_motion_vectors << 9)
      | ((WORD) slice->pic_ext->q_scale_type << 8)
      | ((WORD) slice->pic_ext->intra_vlc_format << 7)
      | ((WORD) slice->pic_ext->alternate_scan << 6)
      | ((WORD) slice->pic_ext->repeat_first_field << 5)
      | ((WORD) slice->pic_ext->chroma_420_type << 4)
      | ((WORD) slice->pic_ext->progressive_frame << 3));
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_start_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice,
    GstMpeg2Picture * prev_picture, GstMpeg2Picture * next_picture)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);
  DXVA_PictureParameters *pic_params = &priv->pic_params;
  DXVA_QmatrixData *iq_matrix = &priv->iq_matrix;
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  gboolean is_field =
      picture->structure != GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME;
  GstFlowReturn ret;
  guint8 picture_id;

  g_assert (klass->start_picture);
  g_assert (klass->get_picture_id);

  ret = klass->start_picture (self, codec_picture, &picture_id);
  if (ret != GST_FLOW_OK)
    return ret;

  priv->slice_list.resize (0);
  priv->bitstream_buffer.resize (0);
  g_ptr_array_set_size (priv->ref_pics, 0);

  memset (pic_params, 0, sizeof (DXVA_PictureParameters));
  memset (iq_matrix, 0, sizeof (DXVA_QmatrixData));

  /* Fill DXVA_PictureParameters */
  pic_params->wDecodedPictureIndex = picture_id;
  pic_params->wForwardRefPictureIndex = 0xffff;
  pic_params->wBackwardRefPictureIndex = 0xffff;

  /* d3d12 does not support post processing */
  if (priv->disable_postproc)
    pic_params->wDeblockedPictureIndex = 0xffff;

  switch (picture->type) {
    case GST_MPEG_VIDEO_PICTURE_TYPE_B:
    {
      if (next_picture) {
        picture_id = klass->get_picture_id (self,
            GST_CODEC_PICTURE (next_picture));
        if (picture_id != 0xff) {
          pic_params->wBackwardRefPictureIndex = picture_id;
          g_ptr_array_add (priv->ref_pics, next_picture);
        }
      }
    }
      /* fall-through */
    case GST_MPEG_VIDEO_PICTURE_TYPE_P:
    {
      if (prev_picture) {
        picture_id = klass->get_picture_id (self,
            GST_CODEC_PICTURE (prev_picture));
        if (picture_id != 0xff) {
          pic_params->wForwardRefPictureIndex = picture_id;
          g_ptr_array_add (priv->ref_pics, prev_picture);
        }
      }
    }
    default:
      break;
  }

  pic_params->wPicWidthInMBminus1 = priv->width_in_mb - 1;
  pic_params->wPicHeightInMBminus1 = (priv->height_in_mb >> is_field) - 1;
  pic_params->bMacroblockWidthMinus1 = 15;
  pic_params->bMacroblockHeightMinus1 = 15;
  pic_params->bBlockWidthMinus1 = 7;
  pic_params->bBlockHeightMinus1 = 7;
  pic_params->bBPPminus1 = 7;
  pic_params->bPicStructure = (BYTE) picture->structure;
  if (picture->first_field && is_field) {
    pic_params->bSecondField = TRUE;
  }
  pic_params->bPicIntra = picture->type == GST_MPEG_VIDEO_PICTURE_TYPE_I;
  pic_params->bPicBackwardPrediction =
      picture->type == GST_MPEG_VIDEO_PICTURE_TYPE_B;
  /* FIXME: 1 -> 4:2:0, 2 -> 4:2:2, 3 -> 4:4:4 */
  pic_params->bChromaFormat = 1;
  pic_params->bPicScanFixed = 1;
  pic_params->bPicScanMethod = slice->pic_ext->alternate_scan;
  pic_params->wBitstreamFcodes = _pack_f_codes (slice->pic_ext->f_code);
  pic_params->wBitstreamPCEelements = _pack_pce_elements (slice);

  /* Fill DXVA_QmatrixData */
  if (slice->quant_matrix &&
      /* The value in bNewQmatrix[0] and bNewQmatrix[1] must not both be zero.
       * https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/dxva/ns-dxva-_dxva_qmatrixdata
       */
      (slice->quant_matrix->load_intra_quantiser_matrix ||
          slice->quant_matrix->load_non_intra_quantiser_matrix)) {
    GstMpegVideoQuantMatrixExt *quant_matrix = slice->quant_matrix;

    if (quant_matrix->load_intra_quantiser_matrix) {
      iq_matrix->bNewQmatrix[0] = 1;
      for (guint i = 0; i < 64; i++) {
        iq_matrix->Qmatrix[0][i] = quant_matrix->intra_quantiser_matrix[i];
      }
    }

    if (quant_matrix->load_non_intra_quantiser_matrix) {
      iq_matrix->bNewQmatrix[1] = 1;
      for (guint i = 0; i < 64; i++) {
        iq_matrix->Qmatrix[1][i] = quant_matrix->non_intra_quantiser_matrix[i];
      }
    }

    if (quant_matrix->load_chroma_intra_quantiser_matrix) {
      iq_matrix->bNewQmatrix[2] = 1;
      for (guint i = 0; i < 64; i++) {
        iq_matrix->Qmatrix[2][i] =
            quant_matrix->chroma_intra_quantiser_matrix[i];
      }
    }

    if (quant_matrix->load_chroma_non_intra_quantiser_matrix) {
      iq_matrix->bNewQmatrix[3] = 1;
      for (guint i = 0; i < 64; i++) {
        iq_matrix->Qmatrix[3][i] =
            quant_matrix->chroma_non_intra_quantiser_matrix[i];
      }
    }

    priv->submit_iq_data = TRUE;
  } else {
    priv->submit_iq_data = FALSE;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_decode_slice (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;
  GstMpegVideoSliceHdr *header = &slice->header;
  GstMpegVideoPacket *packet = &slice->packet;
  DXVA_SliceInfo slice_info = { 0, };

  g_assert (packet->offset >= 4);

  slice_info.wHorizontalPosition = header->mb_column;
  slice_info.wVerticalPosition = header->mb_row;
  /* including start code 4 bytes */
  slice_info.dwSliceBitsInBuffer = 8 * (packet->size + 4);
  slice_info.dwSliceDataLocation = priv->bitstream_buffer.size ();
  /* XXX: We don't have information about the number of MBs in this slice.
   * Just store offset here, and actual number will be calculated later */
  slice_info.wNumberMBsInSlice =
      (header->mb_row * priv->width_in_mb) + header->mb_column;
  slice_info.wQuantizerScaleCode = header->quantiser_scale_code;
  slice_info.wMBbitOffset = header->header_size + 32;

  priv->slice_list.push_back (slice_info);

  size_t pos = priv->bitstream_buffer.size ();
  priv->bitstream_buffer.resize (pos + packet->size + 4);
  memcpy (&priv->bitstream_buffer[0] + pos, packet->data + packet->offset - 4,
      packet->size + 4);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_end_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);
  GstDxvaDecodingArgs args;
  gboolean is_field =
      picture->structure != GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME;
  guint mb_count = priv->width_in_mb * (priv->height_in_mb >> is_field);

  if (priv->bitstream_buffer.empty ()) {
    GST_ERROR_OBJECT (self, "No bitstream buffer to submit");
    return GST_FLOW_ERROR;
  }

  memset (&args, 0, sizeof (GstDxvaDecodingArgs));

  DXVA_SliceInfo *first = &priv->slice_list[0];
  for (size_t i = 0; i < priv->slice_list.size (); i++) {
    DXVA_SliceInfo *slice = first + i;

    /* Update the number of MBs per slice */
    if (i == priv->slice_list.size () - 1) {
      slice->wNumberMBsInSlice = mb_count - slice->wNumberMBsInSlice;
    } else {
      DXVA_SliceInfo *next = first + i + 1;
      slice->wNumberMBsInSlice =
          next->wNumberMBsInSlice - slice->wNumberMBsInSlice;
    }
  }

  args.picture_params = &priv->pic_params;
  args.picture_params_size = sizeof (DXVA_PictureParameters);
  args.slice_control = &priv->slice_list[0];
  args.slice_control_size = sizeof (DXVA_SliceInfo) * priv->slice_list.size ();
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();
  if (priv->submit_iq_data) {
    args.inverse_quantization_matrix = &priv->iq_matrix;
    args.inverse_quantization_matrix_size = sizeof (DXVA_QmatrixData);
  }

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_mpeg2_decoder_output_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture)
{
  GstDxvaMpeg2Decoder *self = GST_DXVA_MPEG2_DECODER (decoder);
  GstDxvaMpeg2DecoderPrivate *priv = self->priv;
  GstDxvaMpeg2DecoderClass *klass = GST_DXVA_MPEG2_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self, "Outputting picture %p", picture);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      picture->buffer_flags, priv->width, priv->height);
}

void
gst_dxva_mpeg2_decoder_disable_postproc (GstDxvaMpeg2Decoder * decoder)
{
  g_return_if_fail (GST_IS_DXVA_MPEG2_DECODER (decoder));

  decoder->priv->disable_postproc = TRUE;
}
