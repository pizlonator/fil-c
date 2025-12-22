/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
 * SECTION:gstvp8decoder
 * @title: GstVp8Decoder
 * @short_description: Base class to implement stateless VP8 decoders
 * @sources:
 * - gstvp8picture.h
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gstvp8decoder.h"

GST_DEBUG_CATEGORY (gst_vp8_decoder_debug);
#define GST_CAT_DEFAULT gst_vp8_decoder_debug

struct _GstVp8DecoderPrivate
{
  gint width;
  gint height;

  gboolean had_sequence;
  GstVp8Parser parser;
  gboolean wait_keyframe;
  guint preferred_output_delay;
  /* for delayed output */
  GstQueueArray *output_queue;
  gboolean is_live;

  gboolean input_state_changed;
};

typedef struct
{
  GstVideoCodecFrame *frame;
  GstVp8Picture *picture;
  GstVp8Decoder *self;
} GstVp8DecoderOutputFrame;

#define parent_class gst_vp8_decoder_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstVp8Decoder, gst_vp8_decoder,
    GST_TYPE_VIDEO_DECODER,
    G_ADD_PRIVATE (GstVp8Decoder);
    GST_DEBUG_CATEGORY_INIT (gst_vp8_decoder_debug, "vp8decoder", 0,
        "VP8 Video Decoder"));

static gboolean gst_vp8_decoder_start (GstVideoDecoder * decoder);
static gboolean gst_vp8_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_vp8_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_vp8_decoder_negotiate (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp8_decoder_finish (GstVideoDecoder * decoder);
static gboolean gst_vp8_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp8_decoder_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp8_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static void gst_vp8_decoder_clear_output_frame (GstVp8DecoderOutputFrame *
    output_frame);
static void gst_vp8_decoder_drain_output_queue (GstVp8Decoder * self,
    guint num, GstFlowReturn * ret);
static GstFlowReturn gst_vp8_decoder_drain_internal (GstVp8Decoder * self,
    gboolean wait_keyframe);

static void
gst_vp8_decoder_class_init (GstVp8DecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_vp8_decoder_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_vp8_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_vp8_decoder_set_format);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_vp8_decoder_negotiate);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_vp8_decoder_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_vp8_decoder_flush);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_vp8_decoder_drain);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_vp8_decoder_handle_frame);
}

static void
gst_vp8_decoder_init (GstVp8Decoder * self)
{
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = gst_vp8_decoder_get_instance_private (self);
}

static gboolean
gst_vp8_decoder_start (GstVideoDecoder * decoder)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);
  GstVp8DecoderPrivate *priv = self->priv;

  gst_vp8_parser_init (&priv->parser);
  priv->wait_keyframe = TRUE;

  priv->output_queue =
      gst_queue_array_new_for_struct (sizeof (GstVp8DecoderOutputFrame), 1);
  gst_queue_array_set_clear_func (priv->output_queue,
      (GDestroyNotify) gst_vp8_decoder_clear_output_frame);

  return TRUE;
}

static void
gst_vp8_decoder_reset (GstVp8Decoder * self)
{
  GstVp8DecoderPrivate *priv = self->priv;

  gst_clear_vp8_picture (&self->last_picture);
  gst_clear_vp8_picture (&self->golden_ref_picture);
  gst_clear_vp8_picture (&self->alt_ref_picture);

  priv->wait_keyframe = TRUE;
  gst_queue_array_clear (priv->output_queue);
}

static gboolean
gst_vp8_decoder_stop (GstVideoDecoder * decoder)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);
  GstVp8DecoderPrivate *priv = self->priv;

  if (self->input_state) {
    gst_video_codec_state_unref (self->input_state);
    self->input_state = NULL;
  }

  gst_vp8_decoder_reset (self);
  gst_queue_array_free (priv->output_queue);

  return TRUE;
}

static GstFlowReturn
gst_vp8_decoder_check_codec_change (GstVp8Decoder * self,
    const GstVp8FrameHdr * frame_hdr)
{
  GstVp8DecoderPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean changed = FALSE;

  if (priv->width != frame_hdr->width || priv->height != frame_hdr->height) {
    GST_INFO_OBJECT (self, "resolution changed %dx%d", frame_hdr->width,
        frame_hdr->height);
    priv->width = frame_hdr->width;
    priv->height = frame_hdr->height;
    changed = TRUE;
  }

  if (changed || !priv->had_sequence) {
    GstVp8DecoderClass *klass = GST_VP8_DECODER_GET_CLASS (self);

    /* Drain before new sequence */
    ret = gst_vp8_decoder_drain_internal (self, FALSE);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to drain pending frames, returned %s",
          gst_flow_get_name (ret));
      return ret;
    }

    priv->had_sequence = TRUE;

    if (klass->get_preferred_output_delay) {
      priv->preferred_output_delay =
          klass->get_preferred_output_delay (self, priv->is_live);
    } else {
      priv->preferred_output_delay = 0;
    }

    g_assert (klass->new_sequence);

    ret = klass->new_sequence (self, frame_hdr,
        /* last/golden/alt 3 reference pictures + current picture */
        4 + priv->preferred_output_delay);
  }

  return ret;
}

static gboolean
gst_vp8_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);
  GstVp8DecoderPrivate *priv = self->priv;
  GstQuery *query;

  GST_DEBUG_OBJECT (decoder, "Set format");

  priv->input_state_changed = TRUE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  priv->width = GST_VIDEO_INFO_WIDTH (&state->info);
  priv->height = GST_VIDEO_INFO_HEIGHT (&state->info);

  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, NULL, NULL);
  gst_query_unref (query);

  return TRUE;
}

static gboolean
gst_vp8_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);

  /* output state must be updated by subclass using new input state already */
  self->priv->input_state_changed = FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_vp8_decoder_update_reference (GstVp8Decoder * self, GstVp8Picture * picture)
{
  GstVp8FrameHdr *frame_hdr = &picture->frame_hdr;

  if (frame_hdr->key_frame) {
    gst_vp8_picture_replace (&self->last_picture, picture);
    gst_vp8_picture_replace (&self->golden_ref_picture, picture);
    gst_vp8_picture_replace (&self->alt_ref_picture, picture);

    goto done;
  }

  if (frame_hdr->refresh_alternate_frame) {
    gst_vp8_picture_replace (&self->alt_ref_picture, picture);
  } else {
    switch (frame_hdr->copy_buffer_to_alternate) {
      case 0:
        /* do nothing */
        break;
      case 1:
        gst_vp8_picture_replace (&self->alt_ref_picture, self->last_picture);
        break;
      case 2:
        gst_vp8_picture_replace (&self->alt_ref_picture,
            self->golden_ref_picture);
        break;
      default:
        GST_WARNING_OBJECT (self, "unrecognized copy_buffer_to_alternate %d",
            frame_hdr->copy_buffer_to_alternate);
        break;
    }
  }

  if (frame_hdr->refresh_golden_frame) {
    gst_vp8_picture_replace (&self->golden_ref_picture, picture);
  } else {
    switch (frame_hdr->copy_buffer_to_golden) {
      case 0:
        /* do nothing */
        break;
      case 1:
        gst_vp8_picture_replace (&self->golden_ref_picture, self->last_picture);
        break;
      case 2:
        gst_vp8_picture_replace (&self->golden_ref_picture,
            self->alt_ref_picture);
        break;
      default:
        GST_WARNING_OBJECT (self, "unrecognized copy_buffer_to_golden %d",
            frame_hdr->copy_buffer_to_alternate);
        break;
    }
  }

  if (frame_hdr->refresh_last)
    gst_vp8_picture_replace (&self->last_picture, picture);

done:
  gst_vp8_picture_unref (picture);

  return TRUE;
}

static GstFlowReturn
gst_vp8_decoder_drain_internal (GstVp8Decoder * self, gboolean wait_keyframe)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVp8DecoderPrivate *priv = self->priv;

  gst_vp8_decoder_drain_output_queue (self, 0, &ret);
  gst_clear_vp8_picture (&self->last_picture);
  gst_clear_vp8_picture (&self->golden_ref_picture);
  gst_clear_vp8_picture (&self->alt_ref_picture);

  priv->wait_keyframe = wait_keyframe;

  return ret;
}

static GstFlowReturn
gst_vp8_decoder_finish (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "finish");

  return gst_vp8_decoder_drain_internal (GST_VP8_DECODER (decoder), TRUE);
}

static gboolean
gst_vp8_decoder_flush (GstVideoDecoder * decoder)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "flush");

  gst_vp8_decoder_reset (self);

  return TRUE;
}

static GstFlowReturn
gst_vp8_decoder_drain (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "drain");

  return gst_vp8_decoder_drain_internal (GST_VP8_DECODER (decoder), TRUE);
}

static void
gst_vp8_decoder_clear_output_frame (GstVp8DecoderOutputFrame * output_frame)
{
  if (!output_frame)
    return;

  if (output_frame->frame) {
    gst_video_decoder_release_frame (GST_VIDEO_DECODER (output_frame->self),
        output_frame->frame);
    output_frame->frame = NULL;
  }

  gst_clear_vp8_picture (&output_frame->picture);
}

static GstFlowReturn
gst_vp8_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVp8Decoder *self = GST_VP8_DECODER (decoder);
  GstVp8DecoderClass *klass = GST_VP8_DECODER_GET_CLASS (self);
  GstVp8DecoderPrivate *priv = self->priv;
  GstBuffer *in_buf = frame->input_buffer;
  GstMapInfo map;
  GstVp8FrameHdr frame_hdr;
  GstVp8ParserResult pres;
  GstVp8Picture *picture = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstFlowReturn output_ret = GST_FLOW_OK;
  GstVp8DecoderOutputFrame output_frame;

  GST_LOG_OBJECT (self,
      "handle frame, PTS: %" GST_TIME_FORMAT ", DTS: %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (in_buf)),
      GST_TIME_ARGS (GST_BUFFER_DTS (in_buf)));

  if (!gst_buffer_map (in_buf, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map buffer");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  pres = gst_vp8_parser_parse_frame_header (&priv->parser,
      &frame_hdr, map.data, map.size);

  if (pres != GST_VP8_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Cannot parser frame header");
    ret = GST_FLOW_ERROR;
    goto unmap_and_error;
  }

  if (priv->wait_keyframe) {
    if (!frame_hdr.key_frame) {
      GST_DEBUG_OBJECT (self, "Waiting initial keyframe, drop buffer %"
          GST_PTR_FORMAT, in_buf);

      gst_buffer_unmap (in_buf, &map);
      gst_video_decoder_release_frame (decoder, frame);

      return GST_FLOW_OK;
    }
  }

  priv->wait_keyframe = FALSE;

  if (frame_hdr.key_frame) {
    ret = gst_vp8_decoder_check_codec_change (self, &frame_hdr);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Subclass cannot handle codec change");
      goto unmap_and_error;
    }
  }

  picture = gst_vp8_picture_new ();
  picture->frame_hdr = frame_hdr;
  picture->data = map.data;
  picture->size = map.size;
  GST_CODEC_PICTURE_FRAME_NUMBER (picture) = frame->system_frame_number;

  if (klass->new_picture) {
    ret = klass->new_picture (self, frame, picture);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass failed to handle new picture");
      goto unmap_and_error;
    }
  }

  if (klass->start_picture) {
    ret = klass->start_picture (self, picture);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass failed to handle start picture");
      goto unmap_and_error;
    }
  }

  if (klass->decode_picture) {
    ret = klass->decode_picture (self, picture, &priv->parser);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass failed to decode current picture");
      goto unmap_and_error;
    }
  }

  if (klass->end_picture) {
    ret = klass->end_picture (self, picture);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "subclass failed to handle end picture");
      goto unmap_and_error;
    }
  }

  gst_buffer_unmap (in_buf, &map);

  gst_vp8_decoder_update_reference (self, gst_vp8_picture_ref (picture));

  if (!picture->frame_hdr.show_frame) {
    GST_LOG_OBJECT (self, "Decode only picture %p", picture);
    GST_VIDEO_CODEC_FRAME_SET_DECODE_ONLY (frame);

    gst_vp8_picture_unref (picture);

    ret = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
  } else {
    /* If subclass didn't update output state at this point,
     * marking this picture as a discont and stores current input state */
    if (priv->input_state_changed) {
      gst_vp8_picture_set_discont_state (picture, self->input_state);
      priv->input_state_changed = FALSE;
    }

    output_frame.frame = frame;
    output_frame.picture = picture;
    output_frame.self = self;
    gst_queue_array_push_tail_struct (priv->output_queue, &output_frame);
  }

  gst_vp8_decoder_drain_output_queue (self, priv->preferred_output_delay,
      &output_ret);
  if (output_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self,
        "Output returned %s", gst_flow_get_name (output_ret));
    return output_ret;
  }

  if (ret == GST_FLOW_ERROR) {
    GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
        ("Failed to decode data"), (NULL), ret);
    return ret;
  }

  return ret;

unmap_and_error:
  {
    gst_buffer_unmap (in_buf, &map);
    goto error;
  }

error:
  {
    if (picture)
      gst_vp8_picture_unref (picture);

    if (ret == GST_FLOW_ERROR) {
      GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
          ("Failed to decode data"), (NULL), ret);
    }

    gst_video_decoder_release_frame (decoder, frame);

    return ret;
  }
}

static void
gst_vp8_decoder_drain_output_queue (GstVp8Decoder * self, guint num,
    GstFlowReturn * ret)
{
  GstVp8DecoderPrivate *priv = self->priv;
  GstVp8DecoderClass *klass = GST_VP8_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  while (gst_queue_array_get_length (priv->output_queue) > num) {
    GstVp8DecoderOutputFrame *output_frame = (GstVp8DecoderOutputFrame *)
        gst_queue_array_pop_head_struct (priv->output_queue);
    /* Output queued frames whatever the return value is, in order to empty
     * the queue */
    GstFlowReturn flow_ret = klass->output_picture (self,
        output_frame->frame, output_frame->picture);

    /* Then, update @ret with new flow return value only if @ret was
     * GST_FLOW_OK. This is to avoid pattern such that
     * ```c
     * GstFlowReturn my_return = GST_FLOW_OK;
     * do something
     *
     * if (my_return == GST_FLOW_OK) {
     *   my_return = gst_vp8_decoder_drain_output_queue ();
     * } else {
     *   // Ignore flow return of this method, but current `my_return` error code
     *   gst_vp8_decoder_drain_output_queue ();
     * }
     *
     * return my_return;
     * ```
     */
    if (*ret == GST_FLOW_OK)
      *ret = flow_ret;
  }
}
