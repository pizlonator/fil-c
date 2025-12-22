/* GStreamer
 *  Copyright (C) 2020 Collabora
 *     Author: Daniel Almeida <daniel.almeida@collabora.com>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstv4l2codecallocator.h"
#include "gstv4l2codecav1dec.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2format.h"
#include "linux/v4l2-controls.h"

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define V4L2_MIN_KERNEL_VER_MAJOR 6
#define V4L2_MIN_KERNEL_VER_MINOR 7
#define V4L2_MIN_KERNEL_VERSION KERNEL_VERSION(V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR, 0)

GST_DEBUG_CATEGORY_STATIC (v4l2_av1dec_debug);
#define GST_CAT_DEFAULT v4l2_av1dec_debug

/* Used to mark picture that have been outputted */
#define FLAG_PICTURE_HOLDS_BUFFER GST_MINI_OBJECT_FLAG_LAST

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-av1, alignment=frame"));

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

struct _GstV4l2CodecAV1Dec
{
  GstAV1Decoder parent;
  GstV4l2Decoder *decoder;
  GstVideoCodecState *output_state;
  GstVideoInfo vinfo;
  GstVideoInfoDmaDrm vinfo_drm;

  GstV4l2CodecAllocator *sink_allocator;
  GstV4l2CodecAllocator *src_allocator;
  GstV4l2CodecPool *src_pool;
  gint min_pool_size;
  gboolean has_videometa;
  gboolean streaming;
  gboolean copy_frames;

  gint frame_width;
  gint frame_height;
  gint render_width;
  gint render_height;
  guint bit_depth;
  guint profile;
  guint16 operating_point_idc;

  struct v4l2_ctrl_av1_sequence v4l2_sequence;
  struct v4l2_ctrl_av1_frame v4l2_frame;
  struct v4l2_ctrl_av1_film_grain v4l2_film_grain;

  gboolean need_sequence;

  GArray *tile_group_entries;

  gboolean fill_film_grain;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecAV1Dec, gst_v4l2_codec_av1_dec,
    GST_TYPE_AV1_DECODER);

#define parent_class gst_v4l2_codec_av1_dec_parent_class

static GstFlowReturn
gst_v4l2_codec_av1_dec_ensure_bitstream (GstV4l2CodecAV1Dec * self)
{
  if (self->bitstream)
    goto done;

  self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

  if (!self->bitstream) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to decode AV1 stream."), (NULL));
    return GST_FLOW_ERROR;
  }

  if (!gst_memory_map (self->bitstream, &self->bitstream_map, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Could not access bitstream memory for writing"), (NULL));
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    return GST_FLOW_ERROR;
  }

done:
  /* We use this field to track how much we have written */
  self->bitstream_map.size = 0;

  return GST_FLOW_OK;
}

static void
gst_v4l2_codec_av1_reset_bitstream (GstV4l2CodecAV1Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }
}

static gboolean
gst_v4l2_decoder_av1_api_check (GstV4l2Decoder * decoder)
{
  guint i, ret_size;
  /* *INDENT-OFF* */
  #define SET_ID(cid) .id = (cid), .name = #cid
  struct
  {
    const gchar *name;
    unsigned int id;
    unsigned int size;
    gboolean optional;
  } controls[] = {
    {
      SET_ID (V4L2_CID_STATELESS_AV1_FRAME),
      .size = sizeof(struct v4l2_ctrl_av1_frame),
    }, {
      SET_ID (V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY),
      .size = sizeof(struct v4l2_ctrl_av1_tile_group_entry),
    }, {
      SET_ID (V4L2_CID_STATELESS_AV1_SEQUENCE),
      .size = sizeof(struct v4l2_ctrl_av1_sequence),
    }, {
      SET_ID (V4L2_CID_STATELESS_AV1_FILM_GRAIN),
      .size = sizeof(struct v4l2_ctrl_av1_film_grain),
      .optional = TRUE,
    }
  };
  #undef SET_ID
  /* *INDENT-ON* */

  /*
   * Compatibility check: make sure the pointer controls are
   * the right size.
   */
  for (i = 0; i < G_N_ELEMENTS (controls); i++) {
    gboolean control_found;

    control_found = gst_v4l2_decoder_query_control_size (decoder,
        controls[i].id, &ret_size);

    if (!controls[i].optional && !control_found) {
      GST_WARNING ("Driver is missing %s support.", controls[i].name);
      return FALSE;
    }

    if (control_found && ret_size != controls[i].size) {
      GST_WARNING ("%s control size mismatch: got %d bytes but %d expected.",
          controls[i].name, ret_size, controls[i].size);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_v4l2_codec_av1_dec_open (GstVideoDecoder * decoder)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open AV1 decoder"),
        ("gst_v4l2_decoder_open() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  self->fill_film_grain =
      gst_v4l2_decoder_query_control_size (self->decoder,
      V4L2_CID_STATELESS_AV1_FILM_GRAIN, NULL);
  return TRUE;
}

static gboolean
gst_v4l2_codec_av1_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  gst_v4l2_decoder_close (self->decoder);
  return TRUE;
}

static void
gst_v4l2_codec_av1_dec_streamoff (GstV4l2CodecAV1Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_av1_dec_reset_allocation (GstV4l2CodecAV1Dec * self)
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
gst_v4l2_codec_av1_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
  gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);

  gst_v4l2_codec_av1_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gboolean
gst_v4l2_codec_av1_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  GstAV1Decoder *av1dec = GST_AV1_DECODER (decoder);

  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_AV1_SEQUENCE,
      .ptr = &self->v4l2_sequence,
      .size = sizeof (self->v4l2_sequence),
    },
  };
  /* *INDENT-ON* */

  GstCaps *peer_caps, *filter, *caps;
  GstStaticCaps *static_filter;

  /* Ignore downstream renegotiation request. */
  if (self->streaming)
    goto done;

  GST_DEBUG_OBJECT (self, "Negotiate");

  gst_v4l2_codec_av1_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_AV1_FRAME,
          self->frame_width, self->frame_height, self->bit_depth)) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure AV1 decoder"),
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
        ("Unsupported pixel format"),
        ("No support for %ux%u", self->frame_width, self->frame_height));
    gst_caps_unref (caps);
    return FALSE;
  }
  gst_caps_unref (caps);

done:
  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  self->output_state =
      gst_v4l2_decoder_set_output_state (GST_VIDEO_DECODER (self), &self->vinfo,
      &self->vinfo_drm, self->render_width, self->render_height,
      av1dec->input_state);

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
gst_v4l2_codec_av1_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
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

  num_bitstream = 1 +
      MAX (1, gst_v4l2_decoder_get_render_delay (self->decoder));

  self->sink_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SINK, num_bitstream);
  if (!self->sink_allocator) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to allocate sink buffers."), (NULL));
    return FALSE;
  }

  self->src_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SRC, self->min_pool_size + min);
  if (!self->src_allocator) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to allocate source buffers."), (NULL));
    g_clear_object (&self->sink_allocator);
    return FALSE;
  }

  self->src_pool = gst_v4l2_codec_pool_new (self->src_allocator, &self->vinfo);

no_internal_changes:
  /* Our buffer pool is internal, we will let the base class create a video
   * pool, and use it if we are running out of buffers or if downstream does
   * not support GstVideoMeta */
  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static guint
gst_v4l2_codec_av1_dec_get_preferred_output_delay (GstAV1Decoder * decoder,
    gboolean live)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  guint delay;

  if (live)
    delay = 0;
  else
    /* Just one for now, perhaps we can make this configurable in the future. */
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);

  return delay;
}

static void
gst_v4l2_codec_av1_dec_set_flushing (GstV4l2CodecAV1Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_av1_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_av1_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_av1_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_av1_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_av1_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_av1_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_av1_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_av1_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_av1_dec_fill_sequence_params (GstV4l2CodecAV1Dec * self,
    const GstAV1SequenceHeaderOBU * seq_hdr)
{
  /* *INDENT-OFF* */
  self->v4l2_sequence = (struct v4l2_ctrl_av1_sequence) {
    .flags =
      (seq_hdr->still_picture ? V4L2_AV1_SEQUENCE_FLAG_STILL_PICTURE : 0) |
      (seq_hdr->use_128x128_superblock ? V4L2_AV1_SEQUENCE_FLAG_USE_128X128_SUPERBLOCK : 0) |
      (seq_hdr->enable_filter_intra ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_FILTER_INTRA : 0) |
      (seq_hdr->enable_intra_edge_filter ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTRA_EDGE_FILTER : 0) |
      (seq_hdr->enable_interintra_compound ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTERINTRA_COMPOUND : 0) |
      (seq_hdr->enable_masked_compound ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_MASKED_COMPOUND : 0) |
      (seq_hdr->enable_warped_motion ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_WARPED_MOTION : 0) |
      (seq_hdr->enable_dual_filter ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_DUAL_FILTER : 0) |
      (seq_hdr->enable_order_hint ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_ORDER_HINT : 0) |
      (seq_hdr->enable_jnt_comp ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_JNT_COMP : 0) |
      (seq_hdr->enable_ref_frame_mvs ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_REF_FRAME_MVS : 0) |
      (seq_hdr->enable_superres ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_SUPERRES : 0) |
      (seq_hdr->enable_cdef ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_CDEF : 0) |
      (seq_hdr->enable_restoration ? V4L2_AV1_SEQUENCE_FLAG_ENABLE_RESTORATION : 0) |
      (seq_hdr->color_config.mono_chrome ? V4L2_AV1_SEQUENCE_FLAG_MONO_CHROME : 0) |
      (seq_hdr->color_config.color_range ? V4L2_AV1_SEQUENCE_FLAG_COLOR_RANGE : 0) |
      (seq_hdr->color_config.subsampling_x ? V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_X : 0) |
      (seq_hdr->color_config.subsampling_y ? V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_Y : 0) |
      (seq_hdr->film_grain_params_present ? V4L2_AV1_SEQUENCE_FLAG_FILM_GRAIN_PARAMS_PRESENT : 0) |
      (seq_hdr->color_config.separate_uv_delta_q ? V4L2_AV1_SEQUENCE_FLAG_SEPARATE_UV_DELTA_Q : 0),
      .seq_profile = seq_hdr->seq_profile,
      .order_hint_bits = seq_hdr->order_hint_bits,
      .bit_depth = seq_hdr->bit_depth,
      .max_frame_width_minus_1 = seq_hdr->max_frame_width_minus_1,
      .max_frame_height_minus_1 = seq_hdr->max_frame_height_minus_1,
  };
  /* *INDENT-ON* */
}

static void
gst_v4l2_codec_av1_fill_refs (GstV4l2CodecAV1Dec * self,
    const GstAV1FrameHeaderOBU * frame_hdr, const GstAV1Dpb * reference_frames)
{
  gint i;

  G_STATIC_ASSERT (G_N_ELEMENTS (self->v4l2_frame.reference_frame_ts) ==
      G_N_ELEMENTS (reference_frames->pic_list));
  G_STATIC_ASSERT (sizeof (self->v4l2_frame.ref_frame_idx) ==
      sizeof (frame_hdr->ref_frame_idx));
  g_return_if_fail (reference_frames != NULL);

  for (i = 0; i < G_N_ELEMENTS (reference_frames->pic_list); i++) {
    GstAV1Picture *ref_pic = reference_frames->pic_list[i];

    /* the decoder might not have filled all slots in the first few frames */
    self->v4l2_frame.reference_frame_ts[i] =
        ref_pic ? GST_CODEC_PICTURE_TS_NS (ref_pic) : 0;
  }

  memcpy (self->v4l2_frame.ref_frame_idx, frame_hdr->ref_frame_idx,
      sizeof (frame_hdr->ref_frame_idx));
}

static void
gst_v4l2_codec_av1_fill_tile_info (GstV4l2CodecAV1Dec * self,
    const GstAV1TileInfo * ti)
{
  struct v4l2_av1_tile_info *v4l2_ti = &self->v4l2_frame.tile_info;

  G_STATIC_ASSERT (sizeof (v4l2_ti->mi_col_starts) ==
      sizeof (ti->mi_col_starts));
  G_STATIC_ASSERT (sizeof (v4l2_ti->mi_row_starts) ==
      sizeof (ti->mi_row_starts));
  G_STATIC_ASSERT (sizeof (v4l2_ti->width_in_sbs_minus_1) ==
      sizeof (ti->width_in_sbs_minus_1));
  G_STATIC_ASSERT (sizeof (v4l2_ti->height_in_sbs_minus_1) ==
      sizeof (ti->height_in_sbs_minus_1));

  memcpy (v4l2_ti->mi_col_starts, ti->mi_col_starts,
      sizeof (v4l2_ti->mi_col_starts));
  memcpy (v4l2_ti->mi_row_starts, ti->mi_row_starts,
      sizeof (v4l2_ti->mi_row_starts));
  memcpy (v4l2_ti->width_in_sbs_minus_1, ti->width_in_sbs_minus_1,
      sizeof (v4l2_ti->width_in_sbs_minus_1));
  memcpy (v4l2_ti->height_in_sbs_minus_1, ti->height_in_sbs_minus_1,
      sizeof (v4l2_ti->height_in_sbs_minus_1));
}

static void
gst_v4l2_codec_av1_fill_loop_filter (GstV4l2CodecAV1Dec * self,
    const GstAV1LoopFilterParams * lf)
{
  struct v4l2_av1_loop_filter *v4l2_lf = &self->v4l2_frame.loop_filter;

  G_STATIC_ASSERT (sizeof (v4l2_lf->level) == sizeof (lf->loop_filter_level));
  G_STATIC_ASSERT (sizeof (v4l2_lf->ref_deltas) ==
      sizeof (lf->loop_filter_ref_deltas));
  G_STATIC_ASSERT (sizeof (v4l2_lf->mode_deltas) ==
      sizeof (lf->loop_filter_mode_deltas));

  memcpy (v4l2_lf->level, lf->loop_filter_level, sizeof (v4l2_lf->level));
  memcpy (v4l2_lf->ref_deltas, lf->loop_filter_ref_deltas,
      sizeof (v4l2_lf->ref_deltas));
  memcpy (v4l2_lf->mode_deltas, lf->loop_filter_mode_deltas,
      sizeof (v4l2_lf->mode_deltas));
}

static void
gst_v4l2_codec_av1_fill_segmentation (GstV4l2CodecAV1Dec * self,
    const GstAV1SegmenationParams * seg)
{
  struct v4l2_av1_segmentation *v4l2_seg = &self->v4l2_frame.segmentation;
  guint32 i;
  guint32 j;

  G_STATIC_ASSERT (sizeof (v4l2_seg->feature_data) ==
      sizeof (seg->feature_data));

  for (i = 0; i < G_N_ELEMENTS (v4l2_seg->feature_enabled); i++)
    for (j = 0; j < V4L2_AV1_SEG_LVL_MAX; j++)
      v4l2_seg->feature_enabled[i] |= (seg->feature_enabled[i][j] << j);

  memcpy (v4l2_seg->feature_data, seg->feature_data,
      sizeof (v4l2_seg->feature_data));
}

static void
gst_v4l2_codec_av1_fill_cdef (GstV4l2CodecAV1Dec * self,
    const GstAV1CDEFParams * cdef)
{
  struct v4l2_av1_cdef *v4l2_cdef = &self->v4l2_frame.cdef;

  G_STATIC_ASSERT (sizeof (v4l2_cdef->y_pri_strength) ==
      sizeof (cdef->cdef_y_pri_strength));
  G_STATIC_ASSERT (sizeof (v4l2_cdef->y_sec_strength) ==
      sizeof (cdef->cdef_y_sec_strength));
  G_STATIC_ASSERT (sizeof (v4l2_cdef->uv_pri_strength) ==
      sizeof (cdef->cdef_uv_pri_strength));
  G_STATIC_ASSERT (sizeof (v4l2_cdef->uv_sec_strength) ==
      sizeof (cdef->cdef_uv_sec_strength));

  memcpy (v4l2_cdef->y_pri_strength, cdef->cdef_y_pri_strength,
      sizeof (v4l2_cdef->y_pri_strength));
  memcpy (v4l2_cdef->y_sec_strength, cdef->cdef_y_sec_strength,
      sizeof (v4l2_cdef->y_sec_strength));
  memcpy (v4l2_cdef->uv_pri_strength, cdef->cdef_uv_pri_strength,
      sizeof (v4l2_cdef->uv_pri_strength));
  memcpy (v4l2_cdef->uv_sec_strength, cdef->cdef_uv_sec_strength,
      sizeof (v4l2_cdef->uv_sec_strength));
}

static void
gst_v4l2_codec_av1_fill_loop_restoration (GstV4l2CodecAV1Dec * self,
    const GstAV1LoopRestorationParams * lr)
{
  struct v4l2_av1_loop_restoration *v4l2_lr =
      &self->v4l2_frame.loop_restoration;

  G_STATIC_ASSERT (sizeof (v4l2_lr->loop_restoration_size) ==
      sizeof (lr->loop_restoration_size));

  memcpy (v4l2_lr->loop_restoration_size, lr->loop_restoration_size,
      sizeof (v4l2_lr->loop_restoration_size));
}

static void
gst_v4l2_codec_av1_fill_global_motion (GstV4l2CodecAV1Dec * self,
    const GstAV1GlobalMotionParams * gm)
{
  struct v4l2_av1_global_motion *v4l2_gm = &self->v4l2_frame.global_motion;
  gint i;

  G_STATIC_ASSERT (sizeof (v4l2_gm->type) == sizeof (gm->gm_type));
  G_STATIC_ASSERT (sizeof (v4l2_gm->params) == sizeof (gm->gm_params));
  G_STATIC_ASSERT (G_N_ELEMENTS (v4l2_gm->flags) == GST_AV1_SEG_LVL_MAX);

  for (i = 0; i < G_N_ELEMENTS (v4l2_gm->flags); i++) {
    v4l2_gm->flags[i] =
        (gm->is_global[i] ? V4L2_AV1_GLOBAL_MOTION_FLAG_IS_GLOBAL : 0) |
        (gm->is_rot_zoom[i] ? V4L2_AV1_GLOBAL_MOTION_FLAG_IS_ROT_ZOOM : 0) |
        (gm->is_translation[i] ? V4L2_AV1_GLOBAL_MOTION_FLAG_IS_TRANSLATION :
        0);

    switch (gm->gm_type[i]) {
      case GST_AV1_WARP_MODEL_IDENTITY:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_IDENTITY;
        break;
      case GST_AV1_WARP_MODEL_TRANSLATION:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_TRANSLATION;
        break;
      case GST_AV1_WARP_MODEL_ROTZOOM:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_ROTZOOM;
        break;
      case GST_AV1_WARP_MODEL_AFFINE:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_AFFINE;
        break;
    }
    v4l2_gm->invalid |= (gm->invalid[i] << i);
  }

  memcpy (v4l2_gm->type, gm->gm_type, sizeof (v4l2_gm->type));
  memcpy (v4l2_gm->params, gm->gm_params, sizeof (v4l2_gm->params));
}

static void
gst_v4l2_codec_av1_fill_film_grain (GstV4l2CodecAV1Dec * self,
    const GstAV1FilmGrainParams * fg)
{
  struct v4l2_ctrl_av1_film_grain *v4l2_fg = &self->v4l2_film_grain;

  *v4l2_fg = (struct v4l2_ctrl_av1_film_grain) {
    .flags =
        (fg->apply_grain ? V4L2_AV1_FILM_GRAIN_FLAG_APPLY_GRAIN : 0) |
        (fg->update_grain ? V4L2_AV1_FILM_GRAIN_FLAG_UPDATE_GRAIN : 0) |
        (fg->chroma_scaling_from_luma ?
        V4L2_AV1_FILM_GRAIN_FLAG_CHROMA_SCALING_FROM_LUMA : 0) |
        (fg->overlap_flag ? V4L2_AV1_FILM_GRAIN_FLAG_OVERLAP : 0) |
        (fg->clip_to_restricted_range ?
        V4L2_AV1_FILM_GRAIN_FLAG_CLIP_TO_RESTRICTED_RANGE : 0),

    .grain_seed = fg->grain_seed,
    .film_grain_params_ref_idx = fg->film_grain_params_ref_idx,
    .num_y_points = fg->num_y_points,
    .num_cb_points = fg->num_cb_points,
    .num_cr_points = fg->num_cr_points,
    .grain_scaling_minus_8 = fg->grain_scaling_minus_8,
    .ar_coeff_lag = fg->ar_coeff_lag,
    .ar_coeff_shift_minus_6 = fg->ar_coeff_shift_minus_6,
    .grain_scale_shift = fg->grain_scale_shift,
    .cb_mult = fg->cb_mult,
    .cb_luma_mult = fg->cb_luma_mult,
    .cb_offset = fg->cb_offset,
    .cr_mult = fg->cr_mult,
    .cr_luma_mult = fg->cr_luma_mult,
    .cr_offset = fg->cr_offset
  };

  G_STATIC_ASSERT (sizeof (v4l2_fg->point_y_value) ==
      sizeof (fg->point_y_value));
  G_STATIC_ASSERT (sizeof (v4l2_fg->point_y_scaling) ==
      sizeof (fg->point_y_scaling));
  G_STATIC_ASSERT (sizeof (v4l2_fg->point_cb_value) ==
      sizeof (fg->point_cb_value));
  G_STATIC_ASSERT (sizeof (v4l2_fg->point_cb_scaling) ==
      sizeof (fg->point_cb_scaling));
  G_STATIC_ASSERT (sizeof (v4l2_fg->point_cr_value) ==
      sizeof (fg->point_cr_value));
  G_STATIC_ASSERT (sizeof (v4l2_fg->point_cr_scaling) ==
      sizeof (fg->point_cr_scaling));
  G_STATIC_ASSERT (sizeof (v4l2_fg->ar_coeffs_y_plus_128) ==
      sizeof (fg->ar_coeffs_y_plus_128));
  G_STATIC_ASSERT (sizeof (v4l2_fg->ar_coeffs_cb_plus_128) ==
      sizeof (fg->ar_coeffs_cb_plus_128));
  G_STATIC_ASSERT (sizeof (v4l2_fg->ar_coeffs_cr_plus_128) ==
      sizeof (fg->ar_coeffs_cr_plus_128));

  memcpy (v4l2_fg->point_y_value, fg->point_y_value,
      sizeof (v4l2_fg->point_y_value));
  memcpy (v4l2_fg->point_y_scaling, fg->point_y_scaling,
      sizeof (v4l2_fg->point_y_scaling));
  memcpy (v4l2_fg->point_cb_value, fg->point_cb_value,
      sizeof (v4l2_fg->point_cb_value));
  memcpy (v4l2_fg->point_cb_scaling, fg->point_cb_scaling,
      sizeof (v4l2_fg->point_cb_scaling));
  memcpy (v4l2_fg->point_cr_value, fg->point_cr_value,
      sizeof (v4l2_fg->point_cr_value));
  memcpy (v4l2_fg->point_cr_scaling, fg->point_cr_scaling,
      sizeof (v4l2_fg->point_cr_scaling));
  memcpy (v4l2_fg->ar_coeffs_y_plus_128, fg->ar_coeffs_y_plus_128,
      sizeof (v4l2_fg->ar_coeffs_y_plus_128));
  memcpy (v4l2_fg->ar_coeffs_cb_plus_128, fg->ar_coeffs_cb_plus_128,
      sizeof (v4l2_fg->ar_coeffs_cb_plus_128));
  memcpy (v4l2_fg->ar_coeffs_cr_plus_128, fg->ar_coeffs_cr_plus_128,
      sizeof (v4l2_fg->ar_coeffs_cr_plus_128));
}

static void
gst_v4l2_codec_av1_dec_fill_frame_hdr (GstV4l2CodecAV1Dec * self,
    const GstAV1Picture * pic, const GstAV1Dpb * reference_frames)
{
  const GstAV1FrameHeaderOBU *f = &pic->frame_hdr;
  const GstAV1TileInfo *ti = &f->tile_info;
  const GstAV1QuantizationParams *q = &f->quantization_params;
  const GstAV1SegmenationParams *seg = &f->segmentation_params; /* FIXME: send patch upstream to fix spelling on the parser s/segmenation/segmentation */
  const GstAV1LoopFilterParams *lf = &f->loop_filter_params;
  const GstAV1LoopRestorationParams *lr = &f->loop_restoration_params;
  guint i;

  G_STATIC_ASSERT (sizeof (self->v4l2_frame.
          loop_restoration.frame_restoration_type) ==
      sizeof (lr->frame_restoration_type));
  G_STATIC_ASSERT (sizeof (self->v4l2_frame.buffer_removal_time) ==
      sizeof (f->buffer_removal_time));
  G_STATIC_ASSERT (sizeof (self->v4l2_frame.order_hints) ==
      sizeof (f->order_hints));
  G_STATIC_ASSERT (sizeof (self->v4l2_frame.skip_mode_frame) ==
      sizeof (f->skip_mode_frame));

  self->v4l2_frame = (struct v4l2_ctrl_av1_frame) {
  /* *INDENT-OFF* */
    .flags =
      (f->show_frame ? V4L2_AV1_FRAME_FLAG_SHOW_FRAME : 0) |
      (f->showable_frame ? V4L2_AV1_FRAME_FLAG_SHOWABLE_FRAME : 0) |
      (f->error_resilient_mode ? V4L2_AV1_FRAME_FLAG_ERROR_RESILIENT_MODE : 0) |
      (f->disable_cdf_update ? V4L2_AV1_FRAME_FLAG_DISABLE_CDF_UPDATE : 0) |
      (f->allow_screen_content_tools ? V4L2_AV1_FRAME_FLAG_ALLOW_SCREEN_CONTENT_TOOLS : 0) |
      (f->force_integer_mv ? V4L2_AV1_FRAME_FLAG_FORCE_INTEGER_MV : 0) |
      (f->allow_intrabc ? V4L2_AV1_FRAME_FLAG_ALLOW_INTRABC : 0) |
      (f->use_superres ? V4L2_AV1_FRAME_FLAG_USE_SUPERRES : 0) |
      (f->allow_high_precision_mv ? V4L2_AV1_FRAME_FLAG_ALLOW_HIGH_PRECISION_MV : 0) |
      (f->is_motion_mode_switchable ? V4L2_AV1_FRAME_FLAG_IS_MOTION_MODE_SWITCHABLE : 0) |
      (f->use_ref_frame_mvs ? V4L2_AV1_FRAME_FLAG_USE_REF_FRAME_MVS : 0) |
      (f->disable_frame_end_update_cdf ? V4L2_AV1_FRAME_FLAG_DISABLE_FRAME_END_UPDATE_CDF : 0) |
      (f->allow_warped_motion ? V4L2_AV1_FRAME_FLAG_ALLOW_WARPED_MOTION : 0) |
      (f->reference_select ? V4L2_AV1_FRAME_FLAG_REFERENCE_SELECT : 0) |
      (f->reduced_tx_set ? V4L2_AV1_FRAME_FLAG_REDUCED_TX_SET : 0) |
      (f->skip_mode_frame[0] > 0 ? V4L2_AV1_FRAME_FLAG_SKIP_MODE_ALLOWED : 0) |
      (f->skip_mode_present ? V4L2_AV1_FRAME_FLAG_SKIP_MODE_PRESENT : 0) |
      (f->frame_size_override_flag ? V4L2_AV1_FRAME_FLAG_FRAME_SIZE_OVERRIDE : 0) |
      (f->buffer_removal_time_present_flag ? V4L2_AV1_FRAME_FLAG_BUFFER_REMOVAL_TIME_PRESENT : 0) |
      (f->frame_refs_short_signaling ? V4L2_AV1_FRAME_FLAG_FRAME_REFS_SHORT_SIGNALING : 0),

    .order_hint = f->order_hint,
    .superres_denom = f->superres_denom,
    .upscaled_width = f->upscaled_width,
    .frame_width_minus_1 = f->frame_width - 1,
    .frame_height_minus_1 = f->frame_height - 1,
    .render_width_minus_1 = f->render_width - 1,
    .render_height_minus_1 = f->render_height - 1,
    .current_frame_id = f->current_frame_id,
    .primary_ref_frame = f->primary_ref_frame,
    .refresh_frame_flags = f->refresh_frame_flags,

    .tile_info = (struct v4l2_av1_tile_info) {
      .flags =
        (f->tile_info.uniform_tile_spacing_flag ? V4L2_AV1_TILE_INFO_FLAG_UNIFORM_TILE_SPACING : 0),
      .tile_size_bytes = f->tile_info.tile_size_bytes,
      .context_update_tile_id = f->tile_info.context_update_tile_id,
      .tile_cols = f->tile_info.tile_cols,
      .tile_rows = f->tile_info.tile_rows,
    },

    .quantization = (struct v4l2_av1_quantization) {
      .flags =
        (q->diff_uv_delta ? V4L2_AV1_QUANTIZATION_FLAG_DIFF_UV_DELTA : 0) |
        (q->using_qmatrix ? V4L2_AV1_QUANTIZATION_FLAG_USING_QMATRIX : 0) |
        (q->delta_q_present ? V4L2_AV1_QUANTIZATION_FLAG_DELTA_Q_PRESENT : 0),

      .base_q_idx = q->base_q_idx,
      .delta_q_y_dc = q->delta_q_y_dc,
      .delta_q_u_dc = q->delta_q_u_dc,
      .delta_q_u_ac = q->delta_q_u_ac,
      .delta_q_v_dc = q->delta_q_v_dc,
      .delta_q_v_ac = q->delta_q_v_ac,
      .qm_y = q->qm_y,
      .qm_u = q->qm_u,
      .qm_v = q->qm_v,
      .delta_q_res = q->delta_q_res,
    },

    .segmentation = (struct v4l2_av1_segmentation)  {
      .flags =
        (seg->segmentation_enabled ? V4L2_AV1_SEGMENTATION_FLAG_ENABLED : 0) |
        (seg->segmentation_update_map ? V4L2_AV1_SEGMENTATION_FLAG_UPDATE_MAP : 0) |
        (seg->segmentation_temporal_update ? V4L2_AV1_SEGMENTATION_FLAG_TEMPORAL_UPDATE : 0) |
        (seg->segmentation_update_data ? V4L2_AV1_SEGMENTATION_FLAG_UPDATE_DATA : 0) |
        (seg->seg_id_pre_skip ? V4L2_AV1_SEGMENTATION_FLAG_SEG_ID_PRE_SKIP : 0),

      .last_active_seg_id = seg->last_active_seg_id,
    },

    .loop_filter = (struct v4l2_av1_loop_filter) {
      .flags =
        (lf->loop_filter_delta_enabled ? V4L2_AV1_LOOP_FILTER_FLAG_DELTA_ENABLED : 0) |
        (lf->loop_filter_delta_update ? V4L2_AV1_LOOP_FILTER_FLAG_DELTA_UPDATE : 0) |
        (lf->delta_lf_present ? V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_PRESENT : 0) |
        (lf->delta_lf_multi ? V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_MULTI : 0),

        .sharpness = lf->loop_filter_sharpness,
        .delta_lf_res = lf->delta_lf_res,
    },

    .cdef = (struct v4l2_av1_cdef) {
      .damping_minus_3 = f->cdef_params.cdef_damping - 3, /* FIXME: is minus 3 really needed? */
      .bits = f->cdef_params.cdef_bits,
    },

    .loop_restoration = (struct v4l2_av1_loop_restoration) {
      .flags =
        (lr->uses_lr ? V4L2_AV1_LOOP_RESTORATION_FLAG_USES_LR : 0) |
        (lr->frame_restoration_type[1] ? V4L2_AV1_LOOP_RESTORATION_FLAG_USES_CHROMA_LR : 0),
      .lr_unit_shift = lr->lr_unit_shift,
      .lr_uv_shift = lr->lr_uv_shift,
    }
  /* *INDENT-ON* */
  };

  switch (f->frame_type) {
    case GST_AV1_KEY_FRAME:
      self->v4l2_frame.frame_type = V4L2_AV1_KEY_FRAME;
      break;
    case GST_AV1_INTER_FRAME:
      self->v4l2_frame.frame_type = V4L2_AV1_INTER_FRAME;
      break;
    case GST_AV1_INTRA_ONLY_FRAME:
      self->v4l2_frame.frame_type = V4L2_AV1_INTRA_ONLY_FRAME;
      break;
    case GST_AV1_SWITCH_FRAME:
      self->v4l2_frame.frame_type = V4L2_AV1_SWITCH_FRAME;
      break;
  }

  switch (f->interpolation_filter) {
    case GST_AV1_INTERPOLATION_FILTER_EIGHTTAP:
      self->v4l2_frame.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP;
      break;
    case GST_AV1_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH:
      self->v4l2_frame.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH;
      break;
    case GST_AV1_INTERPOLATION_FILTER_EIGHTTAP_SHARP:
      self->v4l2_frame.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SHARP;
      break;
    case GST_AV1_INTERPOLATION_FILTER_BILINEAR:
      self->v4l2_frame.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_BILINEAR;
      break;
    case GST_AV1_INTERPOLATION_FILTER_SWITCHABLE:
      self->v4l2_frame.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_SWITCHABLE;
      break;
  }

  switch (f->tx_mode) {
    case GST_AV1_TX_MODE_ONLY_4x4:
      self->v4l2_frame.tx_mode = V4L2_AV1_TX_MODE_ONLY_4X4;
      break;
    case GST_AV1_TX_MODE_LARGEST:
      self->v4l2_frame.tx_mode = V4L2_AV1_TX_MODE_LARGEST;
      break;
    case GST_AV1_TX_MODE_SELECT:
      self->v4l2_frame.tx_mode = V4L2_AV1_TX_MODE_SELECT;
      break;
  }

  for (i = 0; i < V4L2_AV1_NUM_PLANES_MAX; i++)
    switch (lr->frame_restoration_type[i]) {
      case GST_AV1_FRAME_RESTORE_NONE:
        self->v4l2_frame.loop_restoration.frame_restoration_type[i] =
            V4L2_AV1_FRAME_RESTORE_NONE;
        break;
      case GST_AV1_FRAME_RESTORE_WIENER:
        self->v4l2_frame.loop_restoration.frame_restoration_type[i] =
            V4L2_AV1_FRAME_RESTORE_WIENER;
        break;
      case GST_AV1_FRAME_RESTORE_SGRPROJ:
        self->v4l2_frame.loop_restoration.frame_restoration_type[i] =
            V4L2_AV1_FRAME_RESTORE_SGRPROJ;
        break;
      case GST_AV1_FRAME_RESTORE_SWITCHABLE:
        self->v4l2_frame.loop_restoration.frame_restoration_type[i] =
            V4L2_AV1_FRAME_RESTORE_SWITCHABLE;
        break;
    }

  gst_v4l2_codec_av1_fill_refs (self, f, reference_frames);
  gst_v4l2_codec_av1_fill_tile_info (self, ti);
  gst_v4l2_codec_av1_fill_segmentation (self, seg);
  gst_v4l2_codec_av1_fill_loop_filter (self, lf);
  gst_v4l2_codec_av1_fill_cdef (self, &f->cdef_params);
  gst_v4l2_codec_av1_fill_loop_restoration (self, &f->loop_restoration_params);
  gst_v4l2_codec_av1_fill_global_motion (self, &f->global_motion_params);

  if (self->fill_film_grain)
    gst_v4l2_codec_av1_fill_film_grain (self, &f->film_grain_params);

  memcpy (self->v4l2_frame.buffer_removal_time, f->buffer_removal_time,
      sizeof (self->v4l2_frame.buffer_removal_time));
  memcpy (self->v4l2_frame.order_hints, f->order_hints,
      sizeof (self->v4l2_frame.order_hints));
  memcpy (self->v4l2_frame.skip_mode_frame, f->skip_mode_frame,
      sizeof (self->v4l2_frame.skip_mode_frame));
}

static GstFlowReturn
gst_v4l2_codec_av1_dec_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size)
{

  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  /* we'll use this as a hint to allow smaller resolution to be decoded
   * without sending new caps, but some better signalling would be nice from
   * the base class. */
  self->operating_point_idc = seq_hdr->operating_points[0].idc;

  gst_v4l2_codec_av1_dec_fill_sequence_params (self, seq_hdr);
  self->min_pool_size = max_dpb_size;
  self->need_sequence = TRUE;

  return GST_FLOW_OK;
}

static void
gst_v4l2_codec_av1_dec_reset_picture (GstV4l2CodecAV1Dec * self)
{
  gst_v4l2_codec_av1_reset_bitstream (self);
  g_array_set_size (self->tile_group_entries, 0);
}

static GstFlowReturn
gst_v4l2_codec_av1_dec_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  GstAV1FrameHeaderOBU *frame_hdr = &picture->frame_hdr;
  struct v4l2_ctrl_av1_sequence *seq_hdr = &self->v4l2_sequence;
  gboolean negotiation_needed = FALSE;
  gint max_width;
  gint max_height;

  max_width = seq_hdr->max_frame_width_minus_1 + 1;
  max_height = seq_hdr->max_frame_height_minus_1 + 1;

  if (self->vinfo.finfo->format == GST_VIDEO_FORMAT_UNKNOWN)
    negotiation_needed = TRUE;

  /* FIXME the base class could signal this, but let's assume that when we
   * have spatial layers, that smaller resolution will never be shown, and
   * that the max size can be assumed to be render size. */
  if ((self->operating_point_idc >> 8)) {
    if (self->frame_width != max_width || self->frame_height != max_height) {
      self->frame_width = self->render_width = max_width;
      self->frame_height = self->render_height = max_height;

      negotiation_needed = TRUE;

      GST_INFO_OBJECT (self, "max {width|height} changed to %dx%d",
          self->frame_width, self->frame_height);
    }

    if (self->frame_height < frame_hdr->frame_height || self->frame_width <
        frame_hdr->upscaled_width) {
      GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
          ("SVC AV1 frame is larger then sequence max size."),
          ("Frame have size %dx%d but the max size is %dx%d",
              frame_hdr->upscaled_width, frame_hdr->frame_height,
              max_width, max_height));
      return GST_FLOW_ERROR;
    }
  } else if (self->frame_width != frame_hdr->upscaled_width ||
      self->frame_height != frame_hdr->frame_height ||
      self->render_width != frame_hdr->render_width ||
      self->render_height != frame_hdr->render_height) {

    self->frame_width = frame_hdr->upscaled_width;
    self->frame_height = frame_hdr->frame_height;
    self->render_width = frame_hdr->render_width;
    self->render_height = frame_hdr->render_height;

    negotiation_needed = TRUE;

    GST_INFO_OBJECT (self, "frame {width|height} changed to %dx%d",
        self->frame_width, self->frame_height);
    GST_INFO_OBJECT (self, "render {width|height} changed to %dx%d",
        self->render_width, self->render_height);
  }

  if (self->bit_depth != seq_hdr->bit_depth) {
    GST_DEBUG_OBJECT (self, "bit-depth changed from %d to %d", self->bit_depth,
        seq_hdr->bit_depth);
    self->bit_depth = seq_hdr->bit_depth;
    negotiation_needed = TRUE;
  }

  if (self->profile != GST_AV1_PROFILE_UNDEFINED &&
      seq_hdr->seq_profile != self->profile) {
    GST_DEBUG_OBJECT (self, "profile changed from %d to %d", self->profile,
        seq_hdr->seq_profile);
    self->profile = seq_hdr->seq_profile;
    negotiation_needed = TRUE;
  }

  if (seq_hdr->bit_depth != self->bit_depth) {
    GST_DEBUG_OBJECT (self, "bit-depth changed from %d to %d",
        self->bit_depth, seq_hdr->bit_depth);
    self->bit_depth = seq_hdr->bit_depth;
    negotiation_needed = TRUE;
  }

  if (negotiation_needed) {
    if (frame_hdr->frame_type != GST_AV1_KEY_FRAME) {
      GST_ERROR_OBJECT (self,
          "Inter-frame resolution changes are not yet supported in v4l2");
      return GST_FLOW_ERROR;
    }

    gst_v4l2_codec_av1_dec_streamoff (self);
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_ERROR;
    }

    /* Check if we can zero-copy buffers */
    if (!self->has_videometa) {
      GstVideoInfo ref_vinfo;
      gint i;

      gst_video_info_set_format (&ref_vinfo,
          GST_VIDEO_INFO_FORMAT (&self->vinfo),
          self->render_width, self->render_height);

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
  }

  /*
   * if for any reason the base class has dropped the frame midway through
   * decoding, then make sure we start off with a clean slate and that the
   * GstMemory is unmapped.
   */
  gst_v4l2_codec_av1_dec_reset_picture (self);
  return gst_v4l2_codec_av1_dec_ensure_bitstream (self);
}

static GstAV1Picture *
gst_v4l2_codec_av1_dec_duplicate_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstAV1Picture *new_picture;

  GST_DEBUG_OBJECT (decoder, "Duplicate picture %u",
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));

  new_picture = gst_av1_picture_new ();
  new_picture->frame_hdr = picture->frame_hdr;
  GST_CODEC_PICTURE_COPY_FRAME_NUMBER (new_picture, picture);

  if (GST_MINI_OBJECT_FLAG_IS_SET (picture, FLAG_PICTURE_HOLDS_BUFFER)) {
    GstBuffer *output_buffer = gst_av1_picture_get_user_data (picture);
    if (output_buffer) {
      frame->output_buffer = gst_buffer_ref (output_buffer);
      gst_av1_picture_set_user_data (new_picture,
          gst_buffer_ref (frame->output_buffer),
          (GDestroyNotify) gst_buffer_unref);
    }

    GST_MINI_OBJECT_FLAG_SET (new_picture, FLAG_PICTURE_HOLDS_BUFFER);
  } else {
    GstV4l2Request *request = gst_av1_picture_get_user_data (picture);
    gst_av1_picture_set_user_data (new_picture, gst_v4l2_request_ref (request),
        (GDestroyNotify) gst_v4l2_request_unref);
    frame->output_buffer = gst_v4l2_request_dup_pic_buf (request);
  }

  return new_picture;
}

static GstFlowReturn
gst_v4l2_codec_av1_dec_start_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Dpb * dpb)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);

  gst_v4l2_codec_av1_dec_fill_frame_hdr (self, picture, dpb);

  return GST_FLOW_OK;
}

static GstFlowReturn
_copy_into_bitstream_buffer (GstV4l2CodecAV1Dec * self, guint8 * src, gsize len)
{
  guint8 *bitstream_ptr = self->bitstream_map.data + self->bitstream_map.size;

  if (self->bitstream_map.size + len > self->bitstream_map.maxsize) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space left on the bitstream buffer."), (NULL));
    gst_v4l2_codec_av1_dec_reset_picture (self);
    return GST_FLOW_ERROR;
  }

  memcpy (bitstream_ptr, src, len);
  self->bitstream_map.size += len;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_av1_dec_decode_tile (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Tile * tile)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  GstAV1TileGroupOBU *tile_group = &tile->tile_group;
  struct v4l2_ctrl_av1_tile_group_entry v4l2_tile_group_entry;
  guint32 obu_offset = self->bitstream_map.size;
  gint i;

  for (i = tile_group->tg_start; i <= tile_group->tg_end; i++) {
    v4l2_tile_group_entry = (struct v4l2_ctrl_av1_tile_group_entry) {
      .tile_offset = tile_group->entry[i].tile_offset + obu_offset,
      .tile_size = tile_group->entry[i].tile_size,
      .tile_row = tile_group->entry[i].tile_row,
      .tile_col = tile_group->entry[i].tile_col,
    };

    GST_DEBUG_OBJECT (self,
        "Decoded tile group entry %d of size %d at offset %d, rows: %d, cols %d",
        self->tile_group_entries->len, v4l2_tile_group_entry.tile_size,
        v4l2_tile_group_entry.tile_offset, v4l2_tile_group_entry.tile_row,
        v4l2_tile_group_entry.tile_col);

    g_array_append_val (self->tile_group_entries, v4l2_tile_group_entry);
  }

  return _copy_into_bitstream_buffer (self, tile->obu.data, tile->obu.obu_size);
}

static GstFlowReturn
gst_v4l2_codec_av1_dec_end_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  GstVideoCodecFrame *frame;
  GstV4l2Request *request;
  GstFlowReturn flow_ret;
  guint count = 1;
  gsize bytesused;

  struct v4l2_ctrl_av1_tile_group_entry tge = { };

  /* *INDENT-OFF* */
  struct v4l2_ext_control decode_params_control[] = {
    {
      .id = V4L2_CID_STATELESS_AV1_FRAME,
      .ptr = &self->v4l2_frame,
      .size = sizeof(self->v4l2_frame),
    },
    {}, /* tile groups */
    {}, /* tile group entries */
    {}, /* sequence */
    {}, /* film grain */
  };
  /* *INDENT-ON* */

  if (self->tile_group_entries->len > 0) {
    decode_params_control[count++] = (struct v4l2_ext_control) {
      .id = V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY,
      .ptr = self->tile_group_entries->data,
      .size =
          g_array_get_element_size (self->tile_group_entries) *
          self->tile_group_entries->len,
    };
  } else {
    decode_params_control[count++] = (struct v4l2_ext_control) {
      .id = V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY,
      .ptr = &tge,
      .size = sizeof (tge),
    };
  }

  if (self->need_sequence) {
    decode_params_control[count++] = (struct v4l2_ext_control) {
      .id = V4L2_CID_STATELESS_AV1_SEQUENCE,
      .ptr = &self->v4l2_sequence,
      .size = sizeof (self->v4l2_sequence),
    };

    self->need_sequence = FALSE;
  }


  if (self->fill_film_grain) {
    decode_params_control[count++] = (struct v4l2_ext_control) {
      .id = V4L2_CID_STATELESS_AV1_FILM_GRAIN,
      .ptr = &self->v4l2_film_grain,
      .size = sizeof (self->v4l2_film_grain),
    };
  }

  bytesused = self->bitstream_map.size;
  gst_memory_unmap (self->bitstream, &self->bitstream_map);
  self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  gst_memory_resize (self->bitstream, 0, bytesused);

  frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));
  g_return_val_if_fail (frame, FALSE);

  flow_ret = gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (self->src_pool),
      &frame->output_buffer, NULL);
  if (flow_ret != GST_FLOW_OK) {
    if (flow_ret == GST_FLOW_FLUSHING)
      GST_DEBUG_OBJECT (self, "Frame decoding aborted, we are flushing.");
    else
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
          ("No more picture buffer available."), (NULL));
    goto fail;
  }

  request = gst_v4l2_decoder_alloc_request (self->decoder,
      GST_CODEC_PICTURE_FRAME_NUMBER (picture), self->bitstream,
      frame->output_buffer);

  gst_video_codec_frame_unref (frame);

  if (!request) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Failed to allocate a media request object."), (NULL));
    goto fail;
  }

  if (!gst_v4l2_decoder_set_controls (self->decoder, request,
          decode_params_control, count)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the bitstream parameters."), (NULL));
    goto fail;
  }

  if (!gst_v4l2_request_queue (request, 0)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the decode request."), (NULL));
    goto fail;
  }

  gst_av1_picture_set_user_data (picture, request,
      (GDestroyNotify) gst_v4l2_request_unref);
  gst_v4l2_codec_av1_dec_reset_picture (self);
  return GST_FLOW_OK;

fail:
  gst_v4l2_codec_av1_dec_reset_picture (self);
  return GST_FLOW_ERROR;
}

static gboolean
gst_v4l2_codec_av1_dec_copy_output_buffer (GstV4l2CodecAV1Dec * self,
    GstVideoCodecFrame * codec_frame)
{
  GstVideoFrame src_frame;
  GstVideoFrame dest_frame;
  GstVideoInfo dest_vinfo;
  GstBuffer *buffer;

  gst_video_info_set_format (&dest_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
      self->render_width, self->render_height);

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

  /* gst_video_frame_copy can crop this, but does not know, so let make it
   * think it's all right */
  GST_VIDEO_INFO_WIDTH (&src_frame.info) = self->render_width;
  GST_VIDEO_INFO_HEIGHT (&src_frame.info) = self->render_height;

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
gst_v4l2_codec_av1_dec_output_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = NULL;
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

  if (!GST_MINI_OBJECT_FLAG_IS_SET (picture, FLAG_PICTURE_HOLDS_BUFFER))
    request = gst_av1_picture_get_user_data (picture);

  if (request) {
    ret = gst_v4l2_request_set_done (request);
    if (ret == 0) {
      GST_ELEMENT_ERROR (self, STREAM, DECODE,
          ("Decoding frame took too long"), (NULL));
      goto error;
    } else if (ret < 0) {
      GST_ELEMENT_ERROR (self, STREAM, DECODE,
          ("Decoding request failed: %s", g_strerror (errno)), (NULL));
      goto error;
    }

    if (gst_v4l2_request_failed (request)) {
      GST_ELEMENT_ERROR (self, STREAM, DECODE,
          ("Failed to decode frame %u", codec_picture->system_frame_number),
          (NULL));
      goto error;
    }

    /* Hold on reference buffers for the rest of the picture lifetime */
    gst_av1_picture_set_user_data (picture,
        gst_buffer_ref (frame->output_buffer),
        (GDestroyNotify) gst_buffer_unref);

    GST_MINI_OBJECT_FLAG_SET (picture, FLAG_PICTURE_HOLDS_BUFFER);
  }

  /* This may happen if we duplicate a picture witch failed to decode */
  if (!frame->output_buffer) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Failed to decode frame %u", codec_picture->system_frame_number),
        (NULL));
    goto error;
  }

  if (self->copy_frames)
    gst_v4l2_codec_av1_dec_copy_output_buffer (self, frame);

  gst_av1_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_av1_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_av1_dec_dispose (GObject * object)
{
  GstV4l2CodecAV1Dec *self = GST_V4L2_CODEC_AV1_DEC (object);

  g_clear_object (&self->decoder);
  g_clear_pointer (&self->tile_group_entries, g_array_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_av1_dec_subclass_init (GstV4l2CodecAV1DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstAV1DecoderClass *av1decoder_class = GST_AV1_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_av1_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_av1_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_av1_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless AV1 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based AV1 video decoder",
      "Daniel Almeida <daniel.almeida@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_sink_event);

  av1decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_new_sequence);
  av1decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_new_picture);
  av1decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_start_picture);
  av1decoder_class->decode_tile =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_decode_tile);
  av1decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_end_picture);
  av1decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_output_picture);
  av1decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_duplicate_picture);
  av1decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_av1_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

static void
gst_v4l2_codec_av1_dec_subinit (GstV4l2CodecAV1Dec * self,
    GstV4l2CodecAV1DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
  self->tile_group_entries =
      g_array_new (FALSE, TRUE, sizeof (struct v4l2_ctrl_av1_tile_group_entry));
}

static void
gst_v4l2_codec_av1_dec_class_init (GstV4l2CodecAV1DecClass * klass)
{
}

static void
gst_v4l2_codec_av1_dec_init (GstV4l2CodecAV1Dec * self)
{
}

void
gst_v4l2_codec_av1_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  GstCaps *src_caps;

  GST_DEBUG_CATEGORY_INIT (v4l2_av1dec_debug, "v4l2codecs-av1dec", 0,
      "V4L2 stateless AV1 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_AV1_FRAME,
          320, 240, 8))
    return;

  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering AV1 decoder since it produces no "
        "supported format");
    goto done;
  }

  /* TODO uncomment this when AV1 get included in Linus tree */
#if 0
  version = gst_v4l2_decoder_get_version (decoder);
  if (version < V4L2_MIN_KERNEL_VERSION)
    GST_WARNING ("V4L2 API v%u.%u too old, at least v%u.%u required",
        (version >> 16) & 0xff, (version >> 8) & 0xff,
        V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR);
#endif

  if (!gst_v4l2_decoder_av1_api_check (decoder)) {
    GST_WARNING ("Not registering AV1 decoder as it failed ABI check.");
    goto done;
  }

  gst_v4l2_decoder_register (plugin, GST_TYPE_V4L2_CODEC_AV1_DEC,
      (GClassInitFunc) gst_v4l2_codec_av1_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_av1_dec_subinit,
      "v4l2sl%sav1dec", device, rank, NULL);

done:
  gst_caps_unref (src_caps);
}
