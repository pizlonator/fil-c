/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

/*
 * Copyright 2015 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * SECTION:gstvp9decoder
 * @title: Gstvp9Decoder
 * @short_description: Base class to implement stateless VP9 decoders
 * @sources:
 * - gstvp9picture.h
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/base/base.h>
#include "gstvp9decoder.h"

GST_DEBUG_CATEGORY (gst_vp9_decoder_debug);
#define GST_CAT_DEFAULT gst_vp9_decoder_debug

struct _GstVp9DecoderPrivate
{
  gint frame_width;
  gint frame_height;
  gint render_width;
  gint render_height;
  GstVP9Profile profile;

  gboolean had_sequence;

  GstVp9StatefulParser *parser;
  GstVp9Dpb *dpb;

  gboolean support_non_kf_change;

  gboolean wait_keyframe;
  /* controls how many frames to delay when calling output_picture() */
  guint preferred_output_delay;
  GstQueueArray *output_queue;
  gboolean is_live;

  gboolean input_state_changed;
};

typedef struct
{
  GstVideoCodecFrame *frame;
  GstVp9Picture *picture;
  GstVp9Decoder *self;
} GstVp9DecoderOutputFrame;

#define parent_class gst_vp9_decoder_parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstVp9Decoder, gst_vp9_decoder,
    GST_TYPE_VIDEO_DECODER,
    G_ADD_PRIVATE (GstVp9Decoder);
    GST_DEBUG_CATEGORY_INIT (gst_vp9_decoder_debug, "vp9decoder", 0,
        "VP9 Video Decoder"));

static gboolean gst_vp9_decoder_start (GstVideoDecoder * decoder);
static gboolean gst_vp9_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_vp9_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_vp9_decoder_negotiate (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp9_decoder_finish (GstVideoDecoder * decoder);
static gboolean gst_vp9_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp9_decoder_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_vp9_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

static void
gst_vp9_decoder_clear_output_frame (GstVp9DecoderOutputFrame * output_frame);
static void gst_vp9_decoder_drain_output_queue (GstVp9Decoder * self,
    guint num, GstFlowReturn * ret);
static GstFlowReturn gst_vp9_decoder_drain_internal (GstVp9Decoder * self,
    gboolean wait_keyframe);

static void
gst_vp9_decoder_class_init (GstVp9DecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_vp9_decoder_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_vp9_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_vp9_decoder_set_format);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_vp9_decoder_negotiate);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_vp9_decoder_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_vp9_decoder_flush);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_vp9_decoder_drain);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_vp9_decoder_handle_frame);
}

static void
gst_vp9_decoder_init (GstVp9Decoder * self)
{
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (self), TRUE);

  self->priv = gst_vp9_decoder_get_instance_private (self);

  /* Assume subclass can support non-keyframe format change by default */
  self->priv->support_non_kf_change = TRUE;
}

static gboolean
gst_vp9_decoder_start (GstVideoDecoder * decoder)
{
  GstVp9Decoder *self = GST_VP9_DECODER (decoder);
  GstVp9DecoderPrivate *priv = self->priv;

  priv->parser = gst_vp9_stateful_parser_new ();
  priv->dpb = gst_vp9_dpb_new ();
  priv->wait_keyframe = TRUE;
  priv->profile = GST_VP9_PROFILE_UNDEFINED;
  priv->frame_width = 0;
  priv->frame_height = 0;
  priv->render_width = 0;
  priv->render_height = 0;

  priv->output_queue =
      gst_queue_array_new_for_struct (sizeof (GstVp9DecoderOutputFrame), 1);
  gst_queue_array_set_clear_func (priv->output_queue,
      (GDestroyNotify) gst_vp9_decoder_clear_output_frame);

  return TRUE;
}

static gboolean
gst_vp9_decoder_stop (GstVideoDecoder * decoder)
{
  GstVp9Decoder *self = GST_VP9_DECODER (decoder);
  GstVp9DecoderPrivate *priv = self->priv;

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&priv->parser, gst_vp9_stateful_parser_free);
  g_clear_pointer (&priv->dpb, gst_vp9_dpb_free);
  gst_queue_array_free (priv->output_queue);

  return TRUE;
}

static gboolean
gst_vp9_decoder_is_format_change (GstVp9Decoder * self,
    const GstVp9FrameHeader * frame_hdr)
{
  GstVp9DecoderPrivate *priv = self->priv;

  if (priv->frame_width != frame_hdr->width
      || priv->frame_height != frame_hdr->height) {
    GST_INFO_OBJECT (self, "frame resolution changed %dx%d", frame_hdr->width,
        frame_hdr->height);
    return TRUE;
  }

  if (priv->render_width != frame_hdr->render_width
      || priv->render_height != frame_hdr->render_height) {
    GST_INFO_OBJECT (self, "render resolution changed %dx%d",
        frame_hdr->render_width, frame_hdr->render_height);
    return TRUE;
  }

  if (priv->profile != frame_hdr->profile) {
    GST_INFO_OBJECT (self, "profile changed %d", frame_hdr->profile);
    return TRUE;
  }

  return FALSE;
}

static GstFlowReturn
gst_vp9_decoder_check_codec_change (GstVp9Decoder * self,
    const GstVp9FrameHeader * frame_hdr)
{
  GstVp9DecoderPrivate *priv = self->priv;
  GstVp9DecoderClass *klass = GST_VP9_DECODER_GET_CLASS (self);
  GstFlowReturn ret = GST_FLOW_OK;

  g_assert (klass->new_sequence);

  if (priv->had_sequence && !gst_vp9_decoder_is_format_change (self, frame_hdr)) {
    return GST_FLOW_OK;
  }

  priv->frame_width = frame_hdr->width;
  priv->frame_height = frame_hdr->height;
  priv->render_width = frame_hdr->render_width;
  priv->render_height = frame_hdr->render_height;
  priv->profile = frame_hdr->profile;

  /* Drain before new sequence */
  ret = gst_vp9_decoder_drain_internal (self, FALSE);
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

  ret = klass->new_sequence (self, frame_hdr,
      /* +1 for the current frame */
      GST_VP9_REF_FRAMES + 1 + priv->preferred_output_delay);

  if (ret != GST_FLOW_OK)
    priv->had_sequence = FALSE;

  return ret;
}

static gboolean
gst_vp9_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstVp9Decoder *self = GST_VP9_DECODER (decoder);
  GstVp9DecoderPrivate *priv = self->priv;
  GstQuery *query;

  GST_DEBUG_OBJECT (decoder, "Set format");

  priv->input_state_changed = TRUE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);

  self->input_state = gst_video_codec_state_ref (state);

  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, NULL, NULL);
  gst_query_unref (query);

  return TRUE;
}

static gboolean
gst_vp9_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstVp9Decoder *self = GST_VP9_DECODER (decoder);

  /* output state must be updated by subclass using new input state already */
  self->priv->input_state_changed = FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static void
gst_vp9_decoder_reset (GstVp9Decoder * self)
{
  GstVp9DecoderPrivate *priv = self->priv;

  if (priv->dpb)
    gst_vp9_dpb_clear (priv->dpb);

  priv->wait_keyframe = TRUE;
  gst_queue_array_clear (priv->output_queue);
}

static GstFlowReturn
gst_vp9_decoder_drain_internal (GstVp9Decoder * self, gboolean wait_keyframe)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVp9DecoderPrivate *priv = self->priv;

  gst_vp9_decoder_drain_output_queue (self, 0, &ret);
  if (priv->dpb)
    gst_vp9_dpb_clear (priv->dpb);

  priv->wait_keyframe = wait_keyframe;

  return ret;
}

static GstFlowReturn
gst_vp9_decoder_finish (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "finish");

  return gst_vp9_decoder_drain_internal (GST_VP9_DECODER (decoder), TRUE);
}

static gboolean
gst_vp9_decoder_flush (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "flush");

  gst_vp9_decoder_reset (GST_VP9_DECODER (decoder));

  return TRUE;
}

static GstFlowReturn
gst_vp9_decoder_drain (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "drain");

  return gst_vp9_decoder_drain_internal (GST_VP9_DECODER (decoder), TRUE);
}

static void
gst_vp9_decoder_clear_output_frame (GstVp9DecoderOutputFrame * output_frame)
{
  if (!output_frame)
    return;

  if (output_frame->frame) {
    gst_video_decoder_release_frame (GST_VIDEO_DECODER (output_frame->self),
        output_frame->frame);
    output_frame->frame = NULL;
  }

  gst_clear_vp9_picture (&output_frame->picture);
}

static GstFlowReturn
gst_vp9_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVp9Decoder *self = GST_VP9_DECODER (decoder);
  GstVp9DecoderClass *klass = GST_VP9_DECODER_GET_CLASS (self);
  GstVp9DecoderPrivate *priv = self->priv;
  GstBuffer *in_buf = frame->input_buffer;
  GstVp9FrameHeader frame_hdr;
  GstVp9Picture *picture = NULL;
  GstVp9ParserResult pres;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;
  GstFlowReturn output_ret = GST_FLOW_OK;
  gboolean intra_only = FALSE;
  gboolean check_codec_change = FALSE;
  GstVp9DecoderOutputFrame output_frame;

  GST_LOG_OBJECT (self, "handle frame %" GST_PTR_FORMAT, in_buf);

  if (!gst_buffer_map (in_buf, &map, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Cannot map input buffer");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  pres =
      gst_vp9_stateful_parser_parse_uncompressed_frame_header (priv->parser,
      &frame_hdr, map.data, map.size);

  if (pres != GST_VP9_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Failed to parsing frame header");
    ret = GST_FLOW_ERROR;
    goto unmap_and_error;
  }

  if (self->parse_compressed_headers && !frame_hdr.show_existing_frame) {
    pres =
        gst_vp9_stateful_parser_parse_compressed_frame_header (priv->parser,
        &frame_hdr, map.data + frame_hdr.frame_header_length_in_bytes,
        map.size);

    if (pres != GST_VP9_PARSER_OK) {
      GST_ERROR_OBJECT (self, "Failed to parse the compressed frame header");
      goto unmap_and_error;
    }
  }

  if (frame_hdr.show_existing_frame) {
    /* This is a non-intra, dummy frame */
    intra_only = FALSE;
  } else if (frame_hdr.frame_type == GST_VP9_KEY_FRAME || frame_hdr.intra_only) {
    intra_only = TRUE;
  }

  if (intra_only) {
    if (frame_hdr.frame_type == GST_VP9_KEY_FRAME) {
      /* Always check codec change per keyframe */
      check_codec_change = TRUE;
    } else if (priv->wait_keyframe) {
      /* Or, if we are waiting for leading keyframe, but this is intra-only,
       * try decoding this frame, it's allowed as per spec */
      check_codec_change = TRUE;
    }
  }

  if (priv->wait_keyframe && !intra_only) {
    GST_DEBUG_OBJECT (self, "Drop frame before initial keyframe");
    gst_buffer_unmap (in_buf, &map);

    gst_video_decoder_release_frame (decoder, frame);;

    return GST_FLOW_OK;
  }

  if (check_codec_change) {
    ret = gst_vp9_decoder_check_codec_change (self, &frame_hdr);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Subclass cannot handle codec change");
      goto unmap_and_error;
    }
  } else if (!frame_hdr.show_existing_frame && !priv->support_non_kf_change &&
      gst_vp9_decoder_is_format_change (self, &frame_hdr)) {
    GST_DEBUG_OBJECT (self, "Drop frame on non-keyframe format change");

    gst_buffer_unmap (in_buf, &map);
    gst_video_decoder_release_frame (decoder, frame);

    /* Drains frames if any and waits for keyframe again */
    return gst_vp9_decoder_drain_internal (self, TRUE);
  }

  if (!priv->had_sequence) {
    GST_WARNING_OBJECT (self, "No handled frame header, drop frame");
    goto unmap_and_error;
  }

  priv->wait_keyframe = FALSE;

  if (frame_hdr.show_existing_frame) {
    GstVp9Picture *pic_to_dup;

    if (frame_hdr.frame_to_show_map_idx >= GST_VP9_REF_FRAMES ||
        !priv->dpb->pic_list[frame_hdr.frame_to_show_map_idx]) {
      GST_ERROR_OBJECT (self, "Invalid frame_to_show_map_idx %d",
          frame_hdr.frame_to_show_map_idx);
      goto unmap_and_error;
    }

    /* If not implemented by subclass, we can just drop this picture
     * since this frame header indicates the frame index to be duplicated
     * and also this frame header doesn't affect reference management */
    if (!klass->duplicate_picture) {
      gst_buffer_unmap (in_buf, &map);
      GST_VIDEO_CODEC_FRAME_SET_DECODE_ONLY (frame);

      gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
      return GST_FLOW_OK;
    }

    pic_to_dup = priv->dpb->pic_list[frame_hdr.frame_to_show_map_idx];
    picture = klass->duplicate_picture (self, frame, pic_to_dup);

    if (!picture) {
      GST_ERROR_OBJECT (self, "subclass didn't provide duplicated picture");
      goto unmap_and_error;
    }

    GST_CODEC_PICTURE_COPY_FRAME_NUMBER (picture, pic_to_dup);
  } else {
    picture = gst_vp9_picture_new ();
    picture->frame_hdr = frame_hdr;
    GST_CODEC_PICTURE_FRAME_NUMBER (picture) = frame->system_frame_number;
    picture->data = map.data;
    picture->size = map.size;

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
      ret = klass->decode_picture (self, picture, priv->dpb);
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

    /* Just pass our picture to dpb object.
     * Even if this picture does not need to be added to dpb
     * (i.e., not a reference frame), gst_vp9_dpb_add() will take care of
     * the case as well */
    gst_vp9_dpb_add (priv->dpb, gst_vp9_picture_ref (picture));
  }

  gst_buffer_unmap (in_buf, &map);

  if (!frame_hdr.show_frame && !frame_hdr.show_existing_frame) {
    GST_LOG_OBJECT (self, "Decode only picture %p", picture);
    GST_VIDEO_CODEC_FRAME_SET_DECODE_ONLY (frame);

    gst_vp9_picture_unref (picture);

    ret = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
  } else {
    /* If subclass didn't update output state at this point,
     * marking this picture as a discont and stores current input state */
    if (priv->input_state_changed) {
      gst_vp9_picture_set_discont_state (picture, self->input_state);
      priv->input_state_changed = FALSE;
    }

    output_frame.frame = frame;
    output_frame.picture = picture;
    output_frame.self = self;
    gst_queue_array_push_tail_struct (priv->output_queue, &output_frame);
  }

  gst_vp9_decoder_drain_output_queue (self,
      priv->preferred_output_delay, &output_ret);
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
      gst_vp9_picture_unref (picture);

    if (ret == GST_FLOW_ERROR) {
      GST_VIDEO_DECODER_ERROR (self, 1, STREAM, DECODE,
          ("Failed to decode data"), (NULL), ret);
    }

    gst_video_decoder_release_frame (decoder, frame);

    return ret;
  }
}

static void
gst_vp9_decoder_drain_output_queue (GstVp9Decoder * self, guint num,
    GstFlowReturn * ret)
{
  GstVp9DecoderPrivate *priv = self->priv;
  GstVp9DecoderClass *klass = GST_VP9_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  while (gst_queue_array_get_length (priv->output_queue) > num) {
    GstVp9DecoderOutputFrame *output_frame = (GstVp9DecoderOutputFrame *)
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
     *   my_return = gst_vp9_decoder_drain_output_queue ();
     * } else {
     *   // Ignore flow return of this method, but current `my_return` error code
     *   gst_vp9_decoder_drain_output_queue ();
     * }
     *
     * return my_return;
     * ```
     */
    if (*ret == GST_FLOW_OK)
      *ret = flow_ret;
  }
}

/**
 * gst_vp9_decoder_set_non_keyframe_format_change_support:
 * @decoder: a #GstVp9Decoder
 * @support: whether subclass can support non-keyframe format change
 *
 * Called to set non-keyframe format change awareness
 *
 * Since: 1.20
 */
void
gst_vp9_decoder_set_non_keyframe_format_change_support (GstVp9Decoder * decoder,
    gboolean support)
{
  g_return_if_fail (GST_IS_VP9_DECODER (decoder));

  decoder->priv->support_non_kf_change = support;
}
