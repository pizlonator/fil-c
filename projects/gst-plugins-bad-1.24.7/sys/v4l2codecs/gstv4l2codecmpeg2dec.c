/* GStreamer
 * Copyright (C) 2020 Daniel Almeida <daniel.almeida@collabora.com>
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
#include <config.h>
#endif

#include "gstv4l2codecallocator.h"
#include "gstv4l2codecmpeg2dec.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2format.h"
#include "linux/v4l2-controls.h"

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define V4L2_MIN_KERNEL_VER_MAJOR 5
#define V4L2_MIN_KERNEL_VER_MINOR 14
#define V4L2_MIN_KERNEL_VERSION \
    KERNEL_VERSION(V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR, 0)

#define MPEG2_BITDEPTH 8

GST_DEBUG_CATEGORY_STATIC (v4l2_mpeg2dec_debug);
#define GST_CAT_DEFAULT v4l2_mpeg2dec_debug

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "systemstream=(boolean) false, "
        "mpegversion=(int) 2, " "profile=(string) {main, simple} "));

#define SRC_CAPS \
    GST_VIDEO_DMA_DRM_CAPS_MAKE " ; " \
    GST_VIDEO_CAPS_MAKE (GST_V4L2_DEFAULT_VIDEO_FORMATS)

#define SRC_CAPS_NO_DRM \
    GST_VIDEO_CAPS_MAKE (GST_V4L2_DEFAULT_VIDEO_FORMATS)

static GstStaticCaps static_src_caps = GST_STATIC_CAPS (SRC_CAPS);
static GstStaticCaps static_src_caps_no_drm = GST_STATIC_CAPS (SRC_CAPS_NO_DRM);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SRC_NAME,
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS));

struct _GstV4l2CodecMpeg2Dec
{
  GstMpeg2Decoder parent;

  GstV4l2Decoder *decoder;
  GstVideoCodecState *output_state;
  GstVideoInfo vinfo;
  GstVideoInfoDmaDrm vinfo_drm;

  guint16 width;
  guint16 height;
  guint chroma_format;
  gboolean interlaced;
  GstMpegVideoProfile profile;
  guint16 vbv_buffer_size;
  gboolean need_sequence;
  gboolean need_quantiser;

  struct v4l2_ctrl_mpeg2_sequence v4l2_sequence;
  struct v4l2_ctrl_mpeg2_picture v4l2_picture;
  struct v4l2_ctrl_mpeg2_quantisation v4l2_quantisation;

  GstV4l2CodecAllocator *sink_allocator;
  GstV4l2CodecAllocator *src_allocator;
  GstV4l2CodecPool *src_pool;
  gint min_pool_size;
  gboolean has_videometa;
  gboolean streaming;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;

  gboolean copy_frames;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecMpeg2Dec, gst_v4l2_codec_mpeg2_dec,
    GST_TYPE_MPEG2_DECODER);

#define parent_class gst_v4l2_codec_mpeg2_dec_parent_class

static guint
gst_v4l2_codec_mpeg2_dec_get_preferred_output_delay (GstMpeg2Decoder * decoder,
    gboolean is_live)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  guint delay;

  if (is_live)
    delay = 0;
  else
    /* Just one for now, perhaps we can make this configurable in the future. */
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);

  return delay;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_open (GstVideoDecoder * decoder)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  guint version;

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open mpeg2 decoder"),
        ("gst_v4l2_decoder_open() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  version = gst_v4l2_decoder_get_version (self->decoder);
  if (version < V4L2_MIN_KERNEL_VERSION) {
    GST_ERROR_OBJECT (self,
        "V4L2 API v%u.%u too old, at least v%u.%u required",
        (version >> 16) & 0xff, (version >> 8) & 0xff,
        V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR);

    gst_v4l2_decoder_close (self->decoder);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  return gst_v4l2_decoder_close (self->decoder);
}

static void
gst_v4l2_codec_mpeg2_dec_streamoff (GstV4l2CodecMpeg2Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_mpeg2_dec_reset_allocation (GstV4l2CodecMpeg2Dec * self)
{
  if (self->sink_allocator) {
    gst_v4l2_codec_allocator_detach (self->sink_allocator);
    g_clear_object (&self->sink_allocator);
  }

  if (self->src_allocator) {
    gst_v4l2_codec_allocator_detach (self->src_allocator);
    g_clear_object (&self->src_allocator);
    g_clear_object (&self->src_pool);
  }
}

static gboolean
gst_v4l2_codec_mpeg2_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);

  gst_v4l2_codec_mpeg2_dec_streamoff (self);
  gst_v4l2_codec_mpeg2_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gint
get_pixel_bitdepth (GstV4l2CodecMpeg2Dec * self)
{
  gint depth;

  switch (self->chroma_format) {
    case 0:
      /* 4:0:0 */
      depth = MPEG2_BITDEPTH;
      break;
    case 1:
      /* 4:2:0 */
      depth = MPEG2_BITDEPTH + MPEG2_BITDEPTH / 2;
      break;
    case 2:
      /* 4:2:2 */
      depth = 2 * MPEG2_BITDEPTH;
      break;
    case 3:
      /* 4:4:4 */
      depth = 3 * MPEG2_BITDEPTH;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported chroma format %i",
          self->chroma_format);
      depth = 0;
      break;
  }

  return depth;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  GstMpeg2Decoder *mpeg2dec = GST_MPEG2_DECODER (decoder);
  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_MPEG2_SEQUENCE,
      .ptr = &self->v4l2_sequence,
      .size = sizeof(self->v4l2_sequence),
    },
    {
      .id = V4L2_CID_STATELESS_MPEG2_QUANTISATION,
      .ptr = &self->v4l2_quantisation,
      .size = sizeof(self->v4l2_quantisation),
    },
  };

  /* *INDENT-ON* */
  GstCaps *peer_caps, *filter, *caps;
  GstStaticCaps *static_filter;

  /* Ignore downstream renegotiation request. */
  if (self->streaming)
    goto done;

  GST_DEBUG_OBJECT (self, "Negotiate");

  gst_v4l2_codec_mpeg2_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_MPEG2_SLICE,
          self->width, self->height, get_pixel_bitdepth (self))) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure mpeg2 decoder"),
        ("gst_v4l2_decoder_set_sink_fmt() failed: %s", g_strerror (errno)));
    gst_v4l2_decoder_close (self->decoder);
    return FALSE;
  }

  if (!gst_v4l2_decoder_set_controls (self->decoder, NULL, control,
          G_N_ELEMENTS (control))) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver does not support the selected stream."), (NULL));
    return FALSE;
  }

  /* If the peer has ANY caps only advertise system memory caps */
  peer_caps = gst_pad_peer_query_caps (decoder->srcpad, NULL);
  static_filter =
      gst_caps_is_any (peer_caps) ? &static_src_caps_no_drm : &static_src_caps;
  gst_caps_unref (peer_caps);

  filter = gst_v4l2_decoder_enum_src_formats (self->decoder, static_filter);
  if (!filter) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("No supported decoder output formats"), (NULL));
    return FALSE;
  }
  GST_DEBUG_OBJECT (self, "Supported output formats: %" GST_PTR_FORMAT, filter);

  caps = gst_pad_peer_query_caps (decoder->srcpad, filter);
  gst_caps_unref (filter);
  GST_DEBUG_OBJECT (self, "Peer supported formats: %" GST_PTR_FORMAT, caps);

  if (!gst_v4l2_decoder_select_src_format (self->decoder, caps, &self->vinfo,
          &self->vinfo_drm)) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Unsupported bitdepth/chroma format"),
        ("No support for %ux%u chroma IDC %i", self->width,
            self->height, self->chroma_format));
    gst_caps_unref (caps);
    return FALSE;
  }
  gst_caps_unref (caps);

done:
  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  self->output_state =
      gst_v4l2_decoder_set_output_state (GST_VIDEO_DECODER (self), &self->vinfo,
      &self->vinfo_drm, self->width, self->height, mpeg2dec->input_state);

  if (self->interlaced)
    self->output_state->info.interlace_mode =
        GST_VIDEO_INTERLACE_MODE_INTERLEAVED;

  if (GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder)) {
    if (self->streaming)
      return TRUE;

    if (!gst_v4l2_decoder_streamon (self->decoder, GST_PAD_SINK)) {
      GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
          ("Could not enable the decoder driver."),
          ("VIDIOC_STREAMON(SINK) failed: %s", g_strerror (errno)));
      return FALSE;
    }

    if (!gst_v4l2_decoder_streamon (self->decoder, GST_PAD_SRC)) {
      GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
          ("Could not enable the decoder driver."),
          ("VIDIOC_STREAMON(SRC) failed: %s", g_strerror (errno)));
      return FALSE;
    }

    self->streaming = TRUE;

    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  GstCaps *caps = NULL;
  guint min = 0, num_bitstream;

  /* If we are streaming here, then it means there is nothing allocation
   * related in the new state and allocation can be ignored */
  if (self->streaming)
    goto no_internal_changes;

  g_clear_object (&self->src_pool);
  g_clear_object (&self->src_allocator);

  self->has_videometa = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, NULL);

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (self, "No valid caps");
    return FALSE;
  }

  if (gst_video_is_dma_drm_caps (caps) && !self->has_videometa) {
    GST_ERROR_OBJECT (self,
        "DMABuf caps negotiated without the mandatory support of VideoMeta");
    return FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_parse_nth_allocation_pool (query, 0, NULL, NULL, &min, NULL);

  min = MAX (2, min);
  /* note the dpb size is fixed at 2 */
  num_bitstream = 1 +
      MAX (1, gst_v4l2_decoder_get_render_delay (self->decoder));

  self->sink_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SINK, num_bitstream);
  self->src_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SRC, self->min_pool_size + min + 4);
  self->src_pool = gst_v4l2_codec_pool_new (self->src_allocator, &self->vinfo);

no_internal_changes:
  /* Our buffer pool is internal, we will let the base class create a video
   * pool, and use it if we are running out of buffers or if downstream does
   * not support GstVideoMeta */
  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}


static GstFlowReturn
gst_v4l2_codec_mpeg2_dec_new_sequence (GstMpeg2Decoder * decoder,
    const GstMpegVideoSequenceHdr * seq,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceDisplayExt * seq_display_ext,
    const GstMpegVideoSequenceScalableExt * seq_scalable_ext, gint max_dpb_size)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  gboolean negotiation_needed = FALSE;
  gboolean interlaced;
  guint16 width;
  guint16 height;
  guint16 vbv_buffer_size;
  GstMpegVideoProfile mpeg_profile;

  GST_LOG_OBJECT (self, "New sequence");

  interlaced = seq_ext ? !seq_ext->progressive : FALSE;
  if (self->interlaced != interlaced) {
    GST_INFO_OBJECT (self, "interlaced sequence change");
    self->interlaced = interlaced;
    negotiation_needed = TRUE;
  }

  width = seq->width;
  height = seq->height;
  vbv_buffer_size = seq->vbv_buffer_size_value;
  if (seq_ext) {
    width = (width & 0x0fff) | ((guint32) seq_ext->horiz_size_ext << 12);
    height = (height & 0x0fff) | ((guint32) seq_ext->vert_size_ext << 12);
    vbv_buffer_size = (vbv_buffer_size & 0x03ff) | ((guint32)
        seq_ext->vbv_buffer_size_extension << 10);
  }

  if (self->width != width || self->height != height) {
    GST_INFO_OBJECT (self, "resolution change %dx%d -> %dx%d",
        self->width, self->height, width, height);
    self->width = width;
    self->height = height;
    negotiation_needed = TRUE;
  }

  if (self->vbv_buffer_size != vbv_buffer_size) {
    GST_INFO_OBJECT (self, "vbv buffer size change %d -> %d",
        self->vbv_buffer_size, vbv_buffer_size);
    self->vbv_buffer_size = vbv_buffer_size;
    negotiation_needed = TRUE;
  }

  mpeg_profile = GST_MPEG_VIDEO_PROFILE_MAIN;
  if (seq_ext)
    mpeg_profile = seq_ext->profile;

  if (mpeg_profile != GST_MPEG_VIDEO_PROFILE_MAIN &&
      mpeg_profile != GST_MPEG_VIDEO_PROFILE_SIMPLE) {
    GST_ERROR_OBJECT (self, "Cannot support profile %d", mpeg_profile);
    return GST_FLOW_ERROR;
  }

  if (self->profile != mpeg_profile) {
    GST_INFO_OBJECT (self, "Profile change %d -> %d",
        self->profile, mpeg_profile);
    self->profile = mpeg_profile;
    self->streaming = TRUE;
  }

  if (self->vinfo.finfo->format == GST_VIDEO_FORMAT_UNKNOWN)
    negotiation_needed = TRUE;

  /* copy quantiser from the sequence header,
   * if none is provided this will copy the default ones
   * added by the parser
   */
  memcpy (self->v4l2_quantisation.intra_quantiser_matrix,
      seq->intra_quantizer_matrix,
      sizeof (self->v4l2_quantisation.intra_quantiser_matrix));;
  memcpy (self->v4l2_quantisation.non_intra_quantiser_matrix,
      seq->non_intra_quantizer_matrix,
      sizeof (self->v4l2_quantisation.non_intra_quantiser_matrix));;

  /* *INDENT-OFF* */
  self->v4l2_sequence = (struct v4l2_ctrl_mpeg2_sequence) {
    .horizontal_size = self->width,
    .vertical_size = self->height,
    .vbv_buffer_size = self->vbv_buffer_size * 16 * 1024,
    .profile_and_level_indication =
        seq_ext ? (seq_ext->profile << 4) | (seq_ext->
        level << 1) | seq_ext->profile_level_escape_bit : 0,
    .chroma_format = seq_ext ? seq_ext->chroma_format : 0,
    .flags = seq_ext->progressive ? V4L2_MPEG2_SEQ_FLAG_PROGRESSIVE : 0,
  };
  /* *INDENT-ON* */

  if (negotiation_needed) {
    gst_v4l2_codec_mpeg2_dec_streamoff (self);
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_ERROR;
    }
  } else {
    self->need_sequence = TRUE;
    self->need_quantiser = TRUE;
  }

  /* Check if we can zero-copy buffers */
  if (!self->has_videometa) {
    GstVideoInfo ref_vinfo;
    gint i;

    gst_video_info_set_format (&ref_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
        self->width, self->height);

    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&self->vinfo); i++) {
      if (self->vinfo.stride[i] != ref_vinfo.stride[i] ||
          self->vinfo.offset[i] != ref_vinfo.offset[i]) {
        GST_WARNING_OBJECT (self,
            "GstVideoMeta support required, copying frames.");
        self->copy_frames = TRUE;
        break;
      }
    }
  } else {
    self->copy_frames = FALSE;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_ensure_bitstream (GstV4l2CodecMpeg2Dec * self)
{
  if (self->bitstream)
    goto done;

  self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

  if (!self->bitstream) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to decode mpeg2 stream."), (NULL));
    return FALSE;
  }

  if (!gst_memory_map (self->bitstream, &self->bitstream_map, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Could not access bitstream memory for writing"), (NULL));
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    return FALSE;
  }

done:
  /* We use this field to track how much we have written */
  self->bitstream_map.size = 0;

  return TRUE;
}

static inline void
_parse_picture_coding_type (struct v4l2_ctrl_mpeg2_picture *v4l2_picture,
    GstMpeg2Picture * mpeg2_picture)
{
  switch (mpeg2_picture->type) {
    case GST_MPEG_VIDEO_PICTURE_TYPE_I:
      v4l2_picture->picture_coding_type = V4L2_MPEG2_PIC_CODING_TYPE_I;
      break;
    case GST_MPEG_VIDEO_PICTURE_TYPE_P:
      v4l2_picture->picture_coding_type = V4L2_MPEG2_PIC_CODING_TYPE_P;
      break;
    case GST_MPEG_VIDEO_PICTURE_TYPE_B:
      v4l2_picture->picture_coding_type = V4L2_MPEG2_PIC_CODING_TYPE_B;
      break;
    case GST_MPEG_VIDEO_PICTURE_TYPE_D:
      v4l2_picture->picture_coding_type = V4L2_MPEG2_PIC_CODING_TYPE_D;
      break;
  }
}

static inline void
_parse_picture_structure (struct v4l2_ctrl_mpeg2_picture *v4l2_picture,
    GstMpeg2Slice * slice)
{
  if (!slice->pic_ext)
    return;
  switch (slice->pic_ext->picture_structure) {
    case GST_MPEG_VIDEO_PICTURE_STRUCTURE_TOP_FIELD:
      v4l2_picture->picture_structure = V4L2_MPEG2_PIC_TOP_FIELD;
      break;
    case GST_MPEG_VIDEO_PICTURE_STRUCTURE_BOTTOM_FIELD:
      v4l2_picture->picture_structure = V4L2_MPEG2_PIC_BOTTOM_FIELD;
      break;
    case GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME:
      v4l2_picture->picture_structure = V4L2_MPEG2_PIC_FRAME;
      break;
  }
}

static GstFlowReturn
gst_v4l2_codec_mpeg2_dec_start_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice,
    GstMpeg2Picture * prev_picture, GstMpeg2Picture * next_picture)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);

  /* FIXME base class should not call us if negotiation failed */
  if (!self->sink_allocator)
    return GST_FLOW_ERROR;

  if (!gst_v4l2_codec_mpeg2_dec_ensure_bitstream (self))
    return GST_FLOW_ERROR;


  /* *INDENT-OFF* */
  self->v4l2_picture = (struct v4l2_ctrl_mpeg2_picture) {
    .backward_ref_ts = next_picture ?
        GST_CODEC_PICTURE_TS_NS (next_picture) : GST_CLOCK_TIME_NONE,
    .forward_ref_ts = prev_picture ?
        GST_CODEC_PICTURE_TS_NS (prev_picture) : GST_CLOCK_TIME_NONE,
    .intra_dc_precision = slice->pic_ext ? slice->pic_ext->intra_dc_precision : 0,
    .flags = (slice->pic_ext && slice->pic_ext->top_field_first ? V4L2_MPEG2_PIC_FLAG_TOP_FIELD_FIRST : 0) |
             (slice->pic_ext && slice->pic_ext->frame_pred_frame_dct ? V4L2_MPEG2_PIC_FLAG_FRAME_PRED_DCT : 0 ) |
             (slice->pic_ext && slice->pic_ext->concealment_motion_vectors ? V4L2_MPEG2_PIC_FLAG_CONCEALMENT_MV : 0) |
             (slice->pic_ext && slice->pic_ext->q_scale_type ? V4L2_MPEG2_PIC_FLAG_Q_SCALE_TYPE : 0) |
             (slice->pic_ext && slice->pic_ext->intra_vlc_format ? V4L2_MPEG2_PIC_FLAG_INTRA_VLC : 0) |
             (slice->pic_ext && slice->pic_ext->alternate_scan ? V4L2_MPEG2_PIC_FLAG_ALT_SCAN : 0) |
             (slice->pic_ext && slice->pic_ext->repeat_first_field ? V4L2_MPEG2_PIC_FLAG_REPEAT_FIRST : 0) |
             (slice->pic_ext && slice->pic_ext->progressive_frame ? V4L2_MPEG2_PIC_FLAG_PROGRESSIVE : 0),
  };
  /* *INDENT-ON* */

  _parse_picture_coding_type (&self->v4l2_picture, picture);
  _parse_picture_structure (&self->v4l2_picture, slice);

  /* slices share pic_ext and quant_matrix for the picture which might be there or not */
  if (slice->pic_ext)
    memcpy (&self->v4l2_picture.f_code, slice->pic_ext->f_code,
        sizeof (self->v4l2_picture.f_code));

  /* overwrite the sequence ones if needed, see 6.1.1.6 for reference */
  if (slice->quant_matrix) {
    if (slice->quant_matrix->load_intra_quantiser_matrix)
      memcpy (self->v4l2_quantisation.intra_quantiser_matrix,
          slice->quant_matrix->intra_quantiser_matrix,
          sizeof (self->v4l2_quantisation.intra_quantiser_matrix));
    if (slice->quant_matrix->load_non_intra_quantiser_matrix)
      memcpy (self->v4l2_quantisation.non_intra_quantiser_matrix,
          slice->quant_matrix->non_intra_quantiser_matrix,
          sizeof (self->v4l2_quantisation.non_intra_quantiser_matrix));
    if (slice->quant_matrix->load_chroma_intra_quantiser_matrix)
      memcpy (self->v4l2_quantisation.chroma_intra_quantiser_matrix,
          slice->quant_matrix->chroma_intra_quantiser_matrix,
          sizeof (self->v4l2_quantisation.chroma_intra_quantiser_matrix));
    if (slice->quant_matrix->load_chroma_non_intra_quantiser_matrix)
      memcpy (self->v4l2_quantisation.chroma_non_intra_quantiser_matrix,
          slice->quant_matrix->chroma_non_intra_quantiser_matrix,
          sizeof (self->v4l2_quantisation.chroma_non_intra_quantiser_matrix));

    self->need_quantiser |= (slice->quant_matrix->load_intra_quantiser_matrix ||
        slice->quant_matrix->load_non_intra_quantiser_matrix ||
        slice->quant_matrix->load_chroma_intra_quantiser_matrix ||
        slice->quant_matrix->load_chroma_non_intra_quantiser_matrix);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_copy_output_buffer (GstV4l2CodecMpeg2Dec * self,
    GstVideoCodecFrame * codec_frame)
{
  GstVideoFrame src_frame;
  GstVideoFrame dest_frame;
  GstVideoInfo dest_vinfo;
  GstBuffer *buffer;

  gst_video_info_set_format (&dest_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
      self->width, self->height);

  buffer = gst_video_decoder_allocate_output_buffer (GST_VIDEO_DECODER (self));
  if (!buffer)
    goto fail;

  if (!gst_video_frame_map (&src_frame, &self->vinfo,
          codec_frame->output_buffer, GST_MAP_READ))
    goto fail;

  if (!gst_video_frame_map (&dest_frame, &dest_vinfo, buffer, GST_MAP_WRITE)) {
    gst_video_frame_unmap (&dest_frame);
    goto fail;
  }

  GST_VIDEO_INFO_WIDTH (&src_frame.info) = self->width;
  GST_VIDEO_INFO_HEIGHT (&src_frame.info) = self->height;

  if (!gst_video_frame_copy (&dest_frame, &src_frame)) {
    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&dest_frame);
    goto fail;
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dest_frame);
  gst_buffer_replace (&codec_frame->output_buffer, buffer);
  gst_buffer_unref (buffer);

  return TRUE;

fail:
  GST_ERROR_OBJECT (self, "Failed copy output buffer.");
  return FALSE;
}

static GstFlowReturn
gst_v4l2_codec_mpeg2_dec_output_picture (GstMpeg2Decoder * decoder,
    GstVideoCodecFrame * frame, GstMpeg2Picture * picture)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = gst_mpeg2_picture_get_user_data (picture);
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  gint ret;

  if (codec_picture->discont_state) {
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (vdec, "Could not re-negotiate with updated state");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "Output picture %u",
      codec_picture->system_frame_number);

  ret = gst_v4l2_request_set_done (request);
  if (ret == 0) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Decoding frame %u took too long", codec_picture->system_frame_number),
        (NULL));
    goto error;
  } else if (ret < 0) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Decoding request failed: %s", g_strerror (errno)), (NULL));
    goto error;
  }
  g_return_val_if_fail (frame->output_buffer, GST_FLOW_ERROR);

  if (gst_v4l2_request_failed (request)) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Failed to decode frame %u", codec_picture->system_frame_number),
        (NULL));
    goto error;
  }

  /* Hold on reference buffers for the rest of the picture lifetime */
  gst_mpeg2_picture_set_user_data (picture,
      gst_buffer_ref (frame->output_buffer), (GDestroyNotify) gst_buffer_unref);

  if (self->copy_frames)
    gst_v4l2_codec_mpeg2_dec_copy_output_buffer (self, frame);

  gst_mpeg2_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_mpeg2_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_mpeg2_dec_reset_picture (GstV4l2CodecMpeg2Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }
}

static gboolean
gst_v4l2_codec_mpeg2_dec_ensure_output_buffer (GstV4l2CodecMpeg2Dec * self,
    GstVideoCodecFrame * frame)
{
  GstBuffer *buffer;
  GstFlowReturn flow_ret;

  if (frame->output_buffer)
    return TRUE;

  flow_ret = gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (self->src_pool),
      &buffer, NULL);
  if (flow_ret != GST_FLOW_OK) {
    if (flow_ret == GST_FLOW_FLUSHING)
      GST_DEBUG_OBJECT (self, "Frame decoding aborted, we are flushing.");
    else
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
          ("No more picture buffer available."), (NULL));
    return FALSE;
  }

  frame->output_buffer = buffer;
  return TRUE;
}

static gboolean
gst_v4l2_codec_mpeg2_dec_submit_bitstream (GstV4l2CodecMpeg2Dec * self,
    GstMpeg2Picture * picture)
{
  GstV4l2Request *prev_request = NULL, *request = NULL;
  gsize bytesused;
  gboolean ret = FALSE;
  guint count = 0;
  guint flags = 0;

  /* *INDENT-OFF* */
  /* Reserve space for controls */
  struct v4l2_ext_control control[] = {
    { }, /* sequence */
    { }, /* picture */
    { }, /* slice */
    { }, /* quantization */
  };
  /* *INDENT-ON* */

  if (picture->structure != GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME) {
    if (picture->first_field)
      prev_request = gst_mpeg2_picture_get_user_data (picture->first_field);
    else
      flags = V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF;
  }

  bytesused = self->bitstream_map.size;
  gst_memory_unmap (self->bitstream, &self->bitstream_map);
  self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  gst_memory_resize (self->bitstream, 0, bytesused);

  if (prev_request) {
    request = gst_v4l2_decoder_alloc_sub_request (self->decoder, prev_request,
        self->bitstream);
  } else {
    GstVideoCodecFrame *frame;
    guint32 system_frame_number = GST_CODEC_PICTURE_FRAME_NUMBER (picture);

    frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
        system_frame_number);
    g_return_val_if_fail (frame, FALSE);

    if (!gst_v4l2_codec_mpeg2_dec_ensure_output_buffer (self, frame))
      goto done;

    request = gst_v4l2_decoder_alloc_request (self->decoder,
        system_frame_number, self->bitstream, frame->output_buffer);

    gst_video_codec_frame_unref (frame);
  }

  if (!request) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Failed to allocate a media request object."), (NULL));
    goto done;
  }

  if (self->need_sequence) {
    control[count].id = V4L2_CID_STATELESS_MPEG2_SEQUENCE;
    control[count].ptr = &self->v4l2_sequence;
    control[count].size = sizeof (self->v4l2_sequence);
    count++;
    self->need_sequence = FALSE;
  }

  control[count].id = V4L2_CID_STATELESS_MPEG2_PICTURE;
  control[count].ptr = &self->v4l2_picture;
  control[count].size = sizeof (self->v4l2_picture);
  count++;

  if (self->need_quantiser) {
    control[count].id = V4L2_CID_STATELESS_MPEG2_QUANTISATION;
    control[count].ptr = &self->v4l2_quantisation;
    control[count].size = sizeof (self->v4l2_quantisation);
    count++;
    self->need_quantiser = FALSE;
  }

  if (!gst_v4l2_decoder_set_controls (self->decoder, request, control, count)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the bitstream parameters."), (NULL));
    goto done;
  }

  if (!gst_v4l2_request_queue (request, flags)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the decode request."), (NULL));
    goto done;
  }

  gst_mpeg2_picture_set_user_data (picture, g_steal_pointer (&request),
      (GDestroyNotify) gst_v4l2_request_unref);

  ret = TRUE;

done:
  if (request)
    gst_v4l2_request_unref (request);

  gst_v4l2_codec_mpeg2_dec_reset_picture (self);

  return ret;
}

static GstFlowReturn
gst_v4l2_codec_mpeg2_dec_decode_slice (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture, GstMpeg2Slice * slice)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  const gsize slice_size = slice->size;
  const gsize slice_offset = slice->sc_offset;
  const guint8 *slice_ptr = slice->packet.data + slice_offset;
  guint8 *bitstream_ptr = self->bitstream_map.data + self->bitstream_map.size;

  if (self->bitstream_map.size + slice_size > self->bitstream_map.maxsize) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space for slice."), (NULL));
    gst_v4l2_codec_mpeg2_dec_reset_picture (self);
    return GST_FLOW_ERROR;
  }

  memcpy (bitstream_ptr, slice_ptr, slice_size);
  self->bitstream_map.size += slice_size;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_mpeg2_dec_end_picture (GstMpeg2Decoder * decoder,
    GstMpeg2Picture * picture)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);
  /* FIXME might need to make this lazier in case we get an unpaired field */
  if (!gst_v4l2_codec_mpeg2_dec_submit_bitstream (self, picture))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static void
gst_v4l2_codec_mpeg2_dec_set_flushing (GstV4l2CodecMpeg2Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_mpeg2_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_mpeg2_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_mpeg2_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_mpeg2_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_mpeg2_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_mpeg2_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_mpeg2_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_mpeg2_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_mpeg2_dec_init (GstV4l2CodecMpeg2Dec * self)
{
}

static void
gst_v4l2_codec_mpeg2_dec_subinit (GstV4l2CodecMpeg2Dec * self,
    GstV4l2CodecMpeg2DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
}

static void
gst_v4l2_codec_mpeg2_dec_dispose (GObject * object)
{
  GstV4l2CodecMpeg2Dec *self = GST_V4L2_CODEC_MPEG2_DEC (object);

  g_clear_object (&self->decoder);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_mpeg2_dec_class_init (GstV4l2CodecMpeg2DecClass * klass)
{
}

static void
gst_v4l2_codec_mpeg2_dec_subclass_init (GstV4l2CodecMpeg2DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstMpeg2DecoderClass *mpeg2decoder_class = GST_MPEG2_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_mpeg2_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_mpeg2_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_mpeg2_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless Mpeg2 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based Mpeg2 video decoder",
      "Daniel Almeida <daniel.almeida@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_sink_event);

  mpeg2decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_new_sequence);
  mpeg2decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_output_picture);
  mpeg2decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_start_picture);
  mpeg2decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_decode_slice);
  mpeg2decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_end_picture);
  mpeg2decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_mpeg2_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

void
gst_v4l2_codec_mpeg2_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  GstCaps *src_caps;

  GST_DEBUG_CATEGORY_INIT (v4l2_mpeg2dec_debug, "v4l2codecs-mpeg2dec", 0,
      "V4L2 stateless mpeg2 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_MPEG2_SLICE,
          320, 240, 8))
    return;
  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering MPEG2 decoder since it produces no "
        "supported format");
    goto done;
  }

  gst_v4l2_decoder_register (plugin, GST_TYPE_V4L2_CODEC_MPEG2_DEC,
      (GClassInitFunc) gst_v4l2_codec_mpeg2_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_mpeg2_dec_subinit,
      "v4l2sl%smpeg2dec", device, rank, NULL);

done:
  gst_caps_unref (src_caps);
}
