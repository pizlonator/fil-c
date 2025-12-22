/* GStreamer
 *  Copyright (C) 2022 Intel Corporation
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

#include "gstvabaseenc.h"

#include <gst/va/gstva.h>
#include <gst/va/vasurfaceimage.h>
#include <gst/va/gstvavideoformat.h>

#include "vacompat.h"
#include "gstvabase.h"
#include "gstvacaps.h"
#include "gstvapluginutils.h"

#define GST_CAT_DEFAULT gst_va_base_enc_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _GstVaBaseEncPrivate
{
  GstVideoInfo sinkpad_info;
  GstBufferPool *raw_pool;
};

enum
{
  PROP_DEVICE_PATH = 1,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/**
 * GstVaBaseEnc:
 *
 * A base class implementation for VA-API Encoders.
 *
 * Since: 1.22
 */
/* *INDENT-OFF* */
#define gst_va_base_enc_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstVaBaseEnc, gst_va_base_enc,
    GST_TYPE_VIDEO_ENCODER, G_ADD_PRIVATE (GstVaBaseEnc)
    GST_DEBUG_CATEGORY_INIT (gst_va_base_enc_debug,
        "vabaseenc", 0, "vabaseenc element"););
/* *INDENT-ON* */

extern GRecMutex GST_VA_SHARED_LOCK;

static void
gst_va_base_enc_reset_state_default (GstVaBaseEnc * base)
{
  base->frame_duration = GST_CLOCK_TIME_NONE;

  base->width = 0;
  base->height = 0;
  base->profile = VAProfileNone;
  base->rt_format = 0;
  base->codedbuf_size = 0;
  g_atomic_int_set (&base->reconf, FALSE);
}

static void
_flush_all_frames (GstVaBaseEnc * base)
{
  g_queue_clear_full (&base->reorder_list,
      (GDestroyNotify) gst_video_codec_frame_unref);
  g_queue_clear_full (&base->output_list,
      (GDestroyNotify) gst_video_codec_frame_unref);
  g_queue_clear_full (&base->ref_list,
      (GDestroyNotify) gst_video_codec_frame_unref);
}

static gboolean
gst_va_base_enc_open (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (venc);
  gboolean ret = FALSE;

  if (!gst_va_ensure_element_data (venc, klass->render_device_path,
          &base->display))
    return FALSE;

  g_object_notify (G_OBJECT (base), "device-path");

  if (!g_atomic_pointer_get (&base->encoder)) {
    GstVaEncoder *va_encoder;

    va_encoder = gst_va_encoder_new (base->display, klass->codec,
        klass->entrypoint);
    if (va_encoder)
      ret = TRUE;

    gst_object_replace ((GstObject **) (&base->encoder),
        (GstObject *) va_encoder);
    gst_clear_object (&va_encoder);
  } else {
    ret = TRUE;
  }

  return ret;
}

static gboolean
gst_va_base_enc_start (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  gst_va_base_enc_reset_state (base);

  base->input_state = NULL;

  return TRUE;
}

static gboolean
gst_va_base_enc_close (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  gst_clear_object (&base->encoder);
  gst_clear_object (&base->display);

  return TRUE;
}

static gboolean
gst_va_base_enc_stop (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  _flush_all_frames (base);

  if (!gst_va_encoder_close (base->encoder)) {
    GST_ERROR_OBJECT (base, "Failed to close the VA encoder");
    return FALSE;
  }

  if (base->priv->raw_pool)
    gst_buffer_pool_set_active (base->priv->raw_pool, FALSE);
  gst_clear_object (&base->priv->raw_pool);

  if (base->input_state)
    gst_video_codec_state_unref (base->input_state);

  return TRUE;
}

static GstCaps *
gst_va_base_enc_get_caps (GstVideoEncoder * venc, GstCaps * filter)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  GstCaps *caps = NULL, *tmp;

  if (base->encoder)
    caps = gst_va_encoder_get_sinkpad_caps (base->encoder);

  if (caps) {
    if (filter) {
      tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = tmp;
    }
  } else {
    caps = gst_video_encoder_proxy_getcaps (venc, NULL, filter);
  }

  GST_LOG_OBJECT (base, "Returning caps %" GST_PTR_FORMAT, caps);
  return caps;
}

static GstBufferPool *
_get_sinkpad_pool (GstElement * element, gpointer data)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (element);
  GstAllocator *allocator;
  GstAllocationParams params = { 0, };
  guint usage_hint;
  GArray *surface_formats = NULL;
  GstCaps *caps = NULL;

  if (base->priv->raw_pool)
    return base->priv->raw_pool;

  g_assert (base->input_state);
  caps = gst_caps_copy (base->input_state->caps);

  if (!gst_va_base_convert_caps_to_va (caps)) {
    GST_ERROR_OBJECT (base, "Invalid caps %" GST_PTR_FORMAT, caps);
    gst_caps_unref (caps);
    return NULL;
  }

  gst_allocation_params_init (&params);

  surface_formats = gst_va_encoder_get_surface_formats (base->encoder);

  allocator = gst_va_allocator_new (base->display, surface_formats);

  usage_hint = va_get_surface_usage_hint (base->display,
      VAEntrypointEncSlice, GST_PAD_SINK, FALSE);

  base->priv->raw_pool = gst_va_pool_new_with_config (caps, 1, 0, usage_hint,
      GST_VA_FEATURE_AUTO, allocator, &params);
  gst_clear_caps (&caps);

  if (!base->priv->raw_pool) {
    gst_object_unref (allocator);
    return NULL;
  }

  gst_va_allocator_get_format (allocator, &base->priv->sinkpad_info, NULL,
      NULL);

  gst_object_unref (allocator);

  if (!gst_buffer_pool_set_active (base->priv->raw_pool, TRUE)) {
    GST_WARNING_OBJECT (base, "Failed to activate sinkpad pool");
    return NULL;
  }

  return base->priv->raw_pool;
}

static GstFlowReturn
gst_va_base_enc_import_input_buffer (GstVaBaseEnc * base,
    GstBuffer * inbuf, GstBuffer ** buf)
{
  GstVaBufferImporter importer = {
    .element = GST_ELEMENT_CAST (base),
#ifndef GST_DISABLE_GST_DEBUG
    .debug_category = GST_CAT_DEFAULT,
#endif
    .display = base->display,
    .entrypoint = GST_VA_BASE_ENC_ENTRYPOINT (base),
    .in_drm_info = &base->in_drm_info,
    .sinkpad_info = &base->priv->sinkpad_info,
    .get_sinkpad_pool = _get_sinkpad_pool,
  };

  g_return_val_if_fail (GST_IS_VA_BASE_ENC (base), GST_FLOW_ERROR);

  return gst_va_buffer_importer_import (&importer, inbuf, buf);
}

GstBuffer *
gst_va_base_enc_create_output_buffer (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, const guint8 * prefix_data,
    guint prefix_data_len)
{
  guint coded_size;
  goffset offset;
  GstBuffer *buf = NULL;
  VASurfaceID surface;
  VACodedBufferSegment *seg, *seg_list;

  /* Wait for encoding to finish */
  surface = gst_va_encode_picture_get_raw_surface (picture);
  if (!va_sync_surface (base->display, surface))
    goto error;

  seg_list = NULL;
  if (!va_map_buffer (base->display, picture->coded_buffer,
          GST_MAP_READ, (gpointer *) & seg_list))
    goto error;

  if (!seg_list) {
    va_unmap_buffer (base->display, picture->coded_buffer);
    GST_WARNING_OBJECT (base, "coded buffer has no segment list");
    goto error;
  }

  coded_size = 0;
  for (seg = seg_list; seg; seg = seg->next)
    coded_size += seg->size;

  buf = gst_video_encoder_allocate_output_buffer (GST_VIDEO_ENCODER_CAST (base),
      coded_size + prefix_data_len);
  if (!buf) {
    va_unmap_buffer (base->display, picture->coded_buffer);
    GST_ERROR_OBJECT (base, "Failed to allocate output buffer, size %d",
        coded_size);
    goto error;
  }

  offset = 0;
  if (prefix_data) {
    g_assert (prefix_data_len > 0);
    gst_buffer_fill (buf, offset, prefix_data, prefix_data_len);
    offset += prefix_data_len;
  }

  for (seg = seg_list; seg; seg = seg->next) {
    gsize write_size;

    write_size = gst_buffer_fill (buf, offset, seg->buf, seg->size);
    if (write_size != seg->size) {
      GST_WARNING_OBJECT (base, "Segment size is %d, but copied %"
          G_GSIZE_FORMAT, seg->size, write_size);
      break;
    }
    offset += seg->size;
  }

  va_unmap_buffer (base->display, picture->coded_buffer);

  return buf;

error:
  gst_clear_buffer (&buf);
  return NULL;
}

/* Return 0 means error and -1 means not enough data. */
gint
gst_va_base_enc_copy_output_data (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, guint8 * data, gint size)
{
  guint coded_size;
  VASurfaceID surface;
  VACodedBufferSegment *seg, *seg_list;
  gint ret_sz = 0;

  /* Wait for encoding to finish */
  surface = gst_va_encode_picture_get_raw_surface (picture);
  if (!va_sync_surface (base->display, surface))
    goto out;

  seg_list = NULL;
  if (!va_map_buffer (base->display, picture->coded_buffer,
          GST_MAP_READ, (gpointer *) & seg_list))
    goto out;

  if (!seg_list) {
    va_unmap_buffer (base->display, picture->coded_buffer);
    GST_WARNING_OBJECT (base, "coded buffer has no segment list");
    goto out;
  }

  coded_size = 0;
  for (seg = seg_list; seg; seg = seg->next)
    coded_size += seg->size;

  if (coded_size > size) {
    GST_DEBUG_OBJECT (base, "Not enough space for coded data");
    ret_sz = -1;
    goto out;
  }

  for (seg = seg_list; seg; seg = seg->next) {
    memcpy (data + ret_sz, seg->buf, seg->size);
    ret_sz += seg->size;
  }

  va_unmap_buffer (base->display, picture->coded_buffer);

out:
  return ret_sz;
}

static GstAllocator *
_allocator_from_caps (GstVaBaseEnc * base, GstCaps * caps)
{
  GstAllocator *allocator = NULL;

  if (gst_caps_is_dmabuf (caps)) {
    allocator = gst_va_dmabuf_allocator_new (base->display);
  } else {
    GArray *surface_formats =
        gst_va_encoder_get_surface_formats (base->encoder);
    allocator = gst_va_allocator_new (base->display, surface_formats);
  }

  return allocator;
}

static gboolean
gst_va_base_enc_propose_allocation (GstVideoEncoder * venc, GstQuery * query)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  GstAllocator *allocator = NULL;
  GstAllocationParams params = { 0, };
  GstBufferPool *pool;
  GstCaps *caps;
  gboolean need_pool = FALSE;
  guint size = 0, usage_hint;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps)
    return FALSE;

  usage_hint = va_get_surface_usage_hint (base->display,
      VAEntrypointEncSlice, GST_PAD_SINK, gst_video_is_dma_drm_caps (caps));

  gst_allocation_params_init (&params);

  if (!(allocator = _allocator_from_caps (base, caps)))
    return FALSE;

  pool = gst_va_pool_new_with_config (caps, 1, 0, usage_hint,
      GST_VA_FEATURE_AUTO, allocator, &params);
  if (!pool) {
    gst_object_unref (allocator);
    goto config_failed;
  }

  if (!gst_va_pool_get_buffer_size (pool, &size))
    goto config_failed;

  gst_query_add_allocation_param (query, allocator, &params);
  gst_query_add_allocation_pool (query, pool, size, 1, 0);

  GST_DEBUG_OBJECT (base,
      "proposing %" GST_PTR_FORMAT " with allocator %" GST_PTR_FORMAT,
      pool, allocator);

  gst_object_unref (allocator);
  gst_object_unref (pool);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (base, "failed to set config");
    return FALSE;
  }
}

static GstFlowReturn
_push_buffer_to_downstream (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaBaseEncClass *base_class = GST_VA_BASE_ENC_GET_CLASS (base);
  GstFlowReturn ret;
  gboolean complete = TRUE;

  if (!base_class->prepare_output (base, frame, &complete)) {
    GST_ERROR_OBJECT (base, "Failed to prepare output");
    goto error;
  }

  if (frame->output_buffer)
    GST_LOG_OBJECT (base, "Push to downstream: frame system_frame_number: %d,"
        " pts: %" GST_TIME_FORMAT ", dts: %" GST_TIME_FORMAT
        " duration: %" GST_TIME_FORMAT ", buffer size: %" G_GSIZE_FORMAT,
        frame->system_frame_number, GST_TIME_ARGS (frame->pts),
        GST_TIME_ARGS (frame->dts), GST_TIME_ARGS (frame->duration),
        gst_buffer_get_size (frame->output_buffer));

  if (complete) {
    ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (base), frame);
  } else {
    /* Allow to output later and no data here. */
    g_assert (!frame->output_buffer);
    ret = GST_FLOW_OK;
  }

  return ret;

error:
  gst_clear_buffer (&frame->output_buffer);
  gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (base), frame);

  return GST_FLOW_ERROR;
}

static GstFlowReturn
_push_out_one_buffer (GstVaBaseEnc * base)
{
  GstVideoCodecFrame *frame_out;
  GstFlowReturn ret;
  guint32 system_frame_number;

  frame_out = g_queue_pop_head (&base->output_list);
  gst_video_codec_frame_unref (frame_out);

  system_frame_number = frame_out->system_frame_number;

  ret = _push_buffer_to_downstream (base, frame_out);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (base, "fails to push one buffer, system_frame_number "
        "%d: %s", system_frame_number, gst_flow_get_name (ret));
  }

  return ret;
}

static GstFlowReturn
gst_va_base_enc_drain (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  GstVaBaseEncClass *base_class = GST_VA_BASE_ENC_GET_CLASS (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoCodecFrame *frame_enc = NULL;
  gboolean is_last = FALSE;

  GST_DEBUG_OBJECT (base, "Encoder is draining");

  /* Kickout all cached frames */
  if (!base_class->reorder_frame (base, NULL, TRUE, &frame_enc)) {
    ret = GST_FLOW_ERROR;
    goto error_and_purge_all;
  }

  while (frame_enc) {
    is_last = FALSE;

    if (g_queue_is_empty (&base->reorder_list))
      is_last = TRUE;

    ret = base_class->encode_frame (base, frame_enc, is_last);
    if (ret != GST_FLOW_OK)
      goto error_and_purge_all;

    frame_enc = NULL;

    ret = _push_out_one_buffer (base);
    if (ret != GST_FLOW_OK)
      goto error_and_purge_all;

    if (!base_class->reorder_frame (base, NULL, TRUE, &frame_enc)) {
      ret = GST_FLOW_ERROR;
      goto error_and_purge_all;
    }
  }

  g_assert (g_queue_is_empty (&base->reorder_list));

  /* Output all frames. */
  while (!g_queue_is_empty (&base->output_list)) {
    ret = _push_out_one_buffer (base);
    if (ret != GST_FLOW_OK)
      goto error_and_purge_all;
  }

  /* Also clear the reference list. */
  g_queue_clear_full (&base->ref_list,
      (GDestroyNotify) gst_video_codec_frame_unref);

  gst_queue_array_clear (base->dts_queue);

  return GST_FLOW_OK;

error_and_purge_all:
  if (frame_enc) {
    gst_clear_buffer (&frame_enc->output_buffer);
    gst_video_encoder_finish_frame (venc, frame_enc);
  }

  if (!g_queue_is_empty (&base->output_list)) {
    GST_WARNING_OBJECT (base, "Still %d frame in the output list"
        " after drain", g_queue_get_length (&base->output_list));
    while (!g_queue_is_empty (&base->output_list)) {
      frame_enc = g_queue_pop_head (&base->output_list);
      gst_video_codec_frame_unref (frame_enc);
      gst_clear_buffer (&frame_enc->output_buffer);
      gst_video_encoder_finish_frame (venc, frame_enc);
    }
  }

  if (!g_queue_is_empty (&base->reorder_list)) {
    GST_WARNING_OBJECT (base, "Still %d frame in the reorder list"
        " after drain", g_queue_get_length (&base->reorder_list));
    while (!g_queue_is_empty (&base->reorder_list)) {
      frame_enc = g_queue_pop_head (&base->reorder_list);
      gst_video_codec_frame_unref (frame_enc);
      gst_clear_buffer (&frame_enc->output_buffer);
      gst_video_encoder_finish_frame (venc, frame_enc);
    }
  }

  /* Also clear the reference list. */
  g_queue_clear_full (&base->ref_list,
      (GDestroyNotify) gst_video_codec_frame_unref);

  gst_queue_array_clear (base->dts_queue);

  return ret;
}

static gboolean
gst_va_base_enc_reset (GstVaBaseEnc * base)
{
  GstVaBaseEncClass *base_class = GST_VA_BASE_ENC_GET_CLASS (base);

  GST_DEBUG_OBJECT (base, "Reconfiguration");
  if (gst_va_base_enc_drain (GST_VIDEO_ENCODER (base)) != GST_FLOW_OK)
    return FALSE;

  if (!base_class->reconfig (base)) {
    GST_ERROR_OBJECT (base, "Error at reconfiguration error");
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_va_base_enc_handle_frame (GstVideoEncoder * venc,
    GstVideoCodecFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  GstVaBaseEncClass *base_class = GST_VA_BASE_ENC_GET_CLASS (base);
  GstFlowReturn ret;
  GstBuffer *in_buf = NULL;
  GstVideoCodecFrame *frame_encode = NULL;

  GST_LOG_OBJECT (venc,
      "handle frame id %d, dts %" GST_TIME_FORMAT ", pts %" GST_TIME_FORMAT,
      frame->system_frame_number,
      GST_TIME_ARGS (GST_BUFFER_DTS (frame->input_buffer)),
      GST_TIME_ARGS (GST_BUFFER_PTS (frame->input_buffer)));

  if (g_atomic_int_compare_and_exchange (&base->reconf, TRUE, FALSE)) {
    if (!gst_va_base_enc_reset (base)) {
      gst_video_encoder_finish_frame (venc, frame);
      return GST_FLOW_ERROR;
    }
  }

  ret = gst_va_base_enc_import_input_buffer (base,
      frame->input_buffer, &in_buf);
  if (ret != GST_FLOW_OK)
    goto error_buffer_invalid;

  gst_buffer_replace (&frame->input_buffer, in_buf);
  gst_clear_buffer (&in_buf);

  if (!base_class->new_frame (base, frame))
    goto error_new_frame;

  if (!base_class->reorder_frame (base, frame, FALSE, &frame_encode))
    goto error_reorder;

  /* pass it to reorder list and we should not use it again. */
  frame = NULL;

  while (frame_encode) {
    ret = base_class->encode_frame (base, frame_encode, FALSE);
    if (ret != GST_FLOW_OK)
      goto error_encode;

    while (g_queue_get_length (&base->output_list) > 0)
      ret = _push_out_one_buffer (base);

    frame_encode = NULL;
    if (!base_class->reorder_frame (base, NULL, FALSE, &frame_encode))
      goto error_reorder;
  }

  return ret;

error_buffer_invalid:
  {
    GST_ELEMENT_ERROR (venc, STREAM, ENCODE,
        ("Failed to import the input frame: %s.", gst_flow_get_name (ret)),
        (NULL));
    gst_clear_buffer (&in_buf);
    gst_clear_buffer (&frame->output_buffer);
    gst_video_encoder_finish_frame (venc, frame);
    return ret;
  }
error_new_frame:
  {
    GST_ELEMENT_ERROR (venc, STREAM, ENCODE,
        ("Failed to create the input frame."), (NULL));
    gst_clear_buffer (&frame->output_buffer);
    gst_video_encoder_finish_frame (venc, frame);
    return GST_FLOW_ERROR;
  }
error_reorder:
  {
    GST_ELEMENT_ERROR (venc, STREAM, ENCODE,
        ("Failed to reorder the input frame."), (NULL));
    if (frame) {
      gst_clear_buffer (&frame->output_buffer);
      gst_video_encoder_finish_frame (venc, frame);
    }
    return GST_FLOW_ERROR;
  }
error_encode:
  {
    GST_ELEMENT_ERROR (venc, STREAM, ENCODE,
        ("Failed to encode the frame %s.", gst_flow_get_name (ret)), (NULL));
    gst_clear_buffer (&frame_encode->output_buffer);
    gst_video_encoder_finish_frame (venc, frame_encode);
    return ret;
  }
}

static GstFlowReturn
gst_va_base_enc_finish (GstVideoEncoder * venc)
{
  return gst_va_base_enc_drain (venc);
}

static gboolean
gst_va_base_enc_set_format (GstVideoEncoder * venc, GstVideoCodecState * state)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  g_return_val_if_fail (state->caps != NULL, FALSE);

  if (!gst_video_is_dma_drm_caps (state->caps)) {
    gst_video_info_dma_drm_init (&base->in_drm_info);
    base->in_info = state->info;
  } else {
    GstVideoInfo info;

    if (!gst_video_info_dma_drm_from_caps (&base->in_drm_info, state->caps))
      return FALSE;
    if (!gst_va_dma_drm_info_to_video_info (&base->in_drm_info, &info))
      return FALSE;
    base->in_info = info;
  }

  if (base->input_state)
    gst_video_codec_state_unref (base->input_state);
  base->input_state = gst_video_codec_state_ref (state);

  if (!gst_va_base_enc_reset (base))
    return FALSE;

  /* Sub class should open the encoder if reconfig succeeds. */
  return gst_va_encoder_is_open (base->encoder);
}

static gboolean
gst_va_base_enc_flush (GstVideoEncoder * venc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  _flush_all_frames (GST_VA_BASE_ENC (venc));

  gst_queue_array_clear (base->dts_queue);

  return TRUE;
}

static gboolean
_query_context (GstVaBaseEnc * base, GstQuery * query)
{
  GstVaDisplay *display = NULL;
  gboolean ret;

  gst_object_replace ((GstObject **) & display, (GstObject *) base->display);
  ret = gst_va_handle_context_query (GST_ELEMENT_CAST (base), query, display);
  gst_clear_object (&display);

  return ret;
}

static gboolean
gst_va_base_enc_src_query (GstVideoEncoder * venc, GstQuery * query)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      ret = _query_context (base, query);
      break;
    }
    case GST_QUERY_CAPS:{
      GstCaps *caps = NULL, *tmp, *filter = NULL;
      GstVaEncoder *va_encoder = NULL;
      gboolean fixed_caps;

      gst_object_replace ((GstObject **) & va_encoder,
          (GstObject *) base->encoder);

      gst_query_parse_caps (query, &filter);

      fixed_caps = GST_PAD_IS_FIXED_CAPS (GST_VIDEO_ENCODER_SRC_PAD (venc));

      if (!fixed_caps && va_encoder)
        caps = gst_va_encoder_get_srcpad_caps (va_encoder);

      gst_clear_object (&va_encoder);

      if (caps) {
        if (filter) {
          tmp = gst_caps_intersect_full (filter, caps,
              GST_CAPS_INTERSECT_FIRST);
          gst_caps_unref (caps);
          caps = tmp;
        }

        GST_LOG_OBJECT (base, "Returning caps %" GST_PTR_FORMAT, caps);
        gst_query_set_caps_result (query, caps);
        gst_caps_unref (caps);
        ret = TRUE;
        break;
      }
      /* else jump to default */
    }
    default:
      ret = GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (venc, query);
      break;
  }

  return ret;
}

static gboolean
gst_va_base_enc_sink_query (GstVideoEncoder * venc, GstQuery * query)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (venc);

  if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT)
    return _query_context (base, query);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (venc, query);
}

static void
gst_va_base_enc_set_context (GstElement * element, GstContext * context)
{
  GstVaDisplay *old_display, *new_display;
  GstVaBaseEnc *base = GST_VA_BASE_ENC (element);
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);
  gboolean ret;

  old_display = base->display ? gst_object_ref (base->display) : NULL;

  ret = gst_va_handle_set_context (element, context, klass->render_device_path,
      &base->display);

  new_display = base->display ? gst_object_ref (base->display) : NULL;

  if (!ret || (old_display && new_display && old_display != new_display
          && base->encoder)) {
    GST_WARNING_OBJECT (element, "Can't replace VA display while operating");
  }

  gst_clear_object (&old_display);
  gst_clear_object (&new_display);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static void
gst_va_base_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (object);
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);

  switch (prop_id) {
    case PROP_DEVICE_PATH:{
      if (!base->display)
        g_value_set_string (value, klass->render_device_path);
      else if (GST_IS_VA_DISPLAY_PLATFORM (base->display))
        g_object_get_property (G_OBJECT (base->display), "path", value);
      else
        g_value_set_string (value, NULL);

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_va_base_enc_init (GstVaBaseEnc * self)
{
  g_queue_init (&self->reorder_list);
  g_queue_init (&self->ref_list);
  g_queue_init (&self->output_list);
  gst_video_info_init (&self->in_info);

  self->dts_queue = gst_queue_array_new_for_struct (sizeof (GstClockTime), 8);

  self->priv = gst_va_base_enc_get_instance_private (self);
}

static void
gst_va_base_enc_dispose (GObject * object)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (object);

  _flush_all_frames (GST_VA_BASE_ENC (object));
  gst_va_base_enc_close (GST_VIDEO_ENCODER (object));
  g_clear_pointer (&base->dts_queue, gst_queue_array_free);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_va_base_enc_class_init (GstVaBaseEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *encoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->get_property = gst_va_base_enc_get_property;
  gobject_class->dispose = gst_va_base_enc_dispose;

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_va_base_enc_set_context);

  encoder_class->open = GST_DEBUG_FUNCPTR (gst_va_base_enc_open);
  encoder_class->close = GST_DEBUG_FUNCPTR (gst_va_base_enc_close);
  encoder_class->start = GST_DEBUG_FUNCPTR (gst_va_base_enc_start);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_va_base_enc_stop);
  encoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_va_base_enc_get_caps);
  encoder_class->src_query = GST_DEBUG_FUNCPTR (gst_va_base_enc_src_query);
  encoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_va_base_enc_sink_query);
  encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_va_base_enc_propose_allocation);
  encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_va_base_enc_handle_frame);
  encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_va_base_enc_set_format);
  encoder_class->finish = GST_DEBUG_FUNCPTR (gst_va_base_enc_finish);
  encoder_class->flush = GST_DEBUG_FUNCPTR (gst_va_base_enc_flush);

  klass->reset_state = GST_DEBUG_FUNCPTR (gst_va_base_enc_reset_state_default);

  /**
   * GstVaBaseEnc:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   */
  properties[PROP_DEVICE_PATH] = g_param_spec_string ("device-path",
      "Device Path", GST_VA_DEVICE_PATH_PROP_DESC, NULL,
      GST_PARAM_DOC_SHOW_DEFAULT | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);

  gst_type_mark_as_plugin_api (GST_TYPE_VA_BASE_ENC, 0);
}

/*********************** Helper Functions ****************************/
gboolean
gst_va_base_enc_add_rate_control_parameter (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, guint32 rc_mode,
    guint max_bitrate_bits, guint target_percentage,
    guint32 qp_i, guint32 min_qp, guint32 max_qp, guint32 mbbrc)
{
  uint32_t window_size;
  struct VAEncMiscParameterRateControlWrap
  {
    VAEncMiscParameterType type;
    VAEncMiscParameterRateControl rate_control;
  } rate_control;

  if (rc_mode == VA_RC_NONE || rc_mode == VA_RC_CQP)
    return TRUE;

  window_size = rc_mode == VA_RC_VBR ? max_bitrate_bits / 2 : max_bitrate_bits;

  /* *INDENT-OFF* */
  rate_control = (struct VAEncMiscParameterRateControlWrap) {
    .type = VAEncMiscParameterTypeRateControl,
    .rate_control = {
      .bits_per_second = max_bitrate_bits,
      .target_percentage = target_percentage,
      .window_size = window_size,
      .initial_qp = qp_i,
      .min_qp = min_qp,
      .max_qp = max_qp,
      .rc_flags.bits.mb_rate_control = mbbrc,
      .quality_factor = 0,
    },
  };
  /* *INDENT-ON* */

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncMiscParameterBufferType, &rate_control, sizeof (rate_control))) {
    GST_ERROR_OBJECT (base, "Failed to create the race control parameter");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_va_base_enc_add_quality_level_parameter (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, guint target_usage)
{
  /* *INDENT-OFF* */
  struct
  {
    VAEncMiscParameterType type;
    VAEncMiscParameterBufferQualityLevel ql;
  } quality_level = {
    .type = VAEncMiscParameterTypeQualityLevel,
    .ql.quality_level = target_usage,
  };
  /* *INDENT-ON* */

  if (target_usage == 0)
    return TRUE;

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncMiscParameterBufferType, &quality_level,
          sizeof (quality_level))) {
    GST_ERROR_OBJECT (base, "Failed to create the quality level parameter");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_va_base_enc_add_frame_rate_parameter (GstVaBaseEnc * base,
    GstVaEncodePicture * picture)
{
  /* *INDENT-OFF* */
  struct
  {
    VAEncMiscParameterType type;
    VAEncMiscParameterFrameRate fr;
  } framerate = {
    .type = VAEncMiscParameterTypeFrameRate,
    /* denominator = framerate >> 16 & 0xffff;
     * numerator   = framerate & 0xffff; */
    .fr.framerate =
        (GST_VIDEO_INFO_FPS_N (&base->in_info) & 0xffff) |
        ((GST_VIDEO_INFO_FPS_D (&base->in_info) & 0xffff) << 16)
  };
  /* *INDENT-ON* */

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncMiscParameterBufferType, &framerate, sizeof (framerate))) {
    GST_ERROR_OBJECT (base, "Failed to create the frame rate parameter");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_va_base_enc_add_hrd_parameter (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, guint32 rc_mode, guint cpb_length_bits)
{
  /* *INDENT-OFF* */
  struct
  {
    VAEncMiscParameterType type;
    VAEncMiscParameterHRD hrd;
  } hrd = {
    .type = VAEncMiscParameterTypeHRD,
    .hrd = {
      .buffer_size = cpb_length_bits,
      .initial_buffer_fullness = cpb_length_bits / 2,
    },
  };
  /* *INDENT-ON* */

  if (rc_mode == VA_RC_NONE || rc_mode == VA_RC_CQP || rc_mode == VA_RC_VCM)
    return TRUE;

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncMiscParameterBufferType, &hrd, sizeof (hrd))) {
    GST_ERROR_OBJECT (base, "Failed to create the HRD parameter");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_va_base_enc_add_trellis_parameter (GstVaBaseEnc * base,
    GstVaEncodePicture * picture, gboolean use_trellis)
{
  /* *INDENT-OFF* */
  struct
  {
    VAEncMiscParameterType type;
    VAEncMiscParameterQuantization tr;
  } trellis = {
    .type = VAEncMiscParameterTypeQuantization,
    .tr.quantization_flags.bits = {
       .disable_trellis = 0,
       .enable_trellis_I = 1,
       .enable_trellis_B = 1,
       .enable_trellis_P = 1,
    },
  };
  /* *INDENT-ON* */

  if (!use_trellis)
    return TRUE;

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncMiscParameterBufferType, &trellis, sizeof (trellis))) {
    GST_ERROR_OBJECT (base, "Failed to create the trellis parameter");
    return FALSE;
  }

  return TRUE;
}

void
gst_va_base_enc_add_codec_tag (GstVaBaseEnc * base, const gchar * codec_name)
{
  GstVideoEncoder *venc = GST_VIDEO_ENCODER (base);
  GstTagList *tags = gst_tag_list_new_empty ();
  const gchar *encoder_name;
  guint bitrate = 0;

  g_object_get (venc, "bitrate", &bitrate, NULL);
  if (bitrate > 0)
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_NOMINAL_BITRATE,
        bitrate, NULL);

  if ((encoder_name =
          gst_element_class_get_metadata (GST_ELEMENT_GET_CLASS (venc),
              GST_ELEMENT_METADATA_LONGNAME)))
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER,
        encoder_name, NULL);

  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_CODEC, codec_name,
      NULL);

  gst_video_encoder_merge_tags (venc, tags, GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);
}

void
gst_va_base_enc_push_dts (GstVaBaseEnc * base,
    GstVideoCodecFrame * frame, guint max_reorder_num)
{
  /* We need to manually insert max_reorder_num slots before the
     first frame to ensure DTS never bigger than PTS. */
  if (gst_queue_array_get_length (base->dts_queue) == 0 && max_reorder_num > 0) {
    GstClockTime dts_diff = 0, dts;

    if (GST_CLOCK_TIME_IS_VALID (frame->duration))
      dts_diff = frame->duration;

    if (GST_CLOCK_TIME_IS_VALID (base->frame_duration))
      dts_diff = MAX (base->frame_duration, dts_diff);

    while (max_reorder_num > 0) {
      if (GST_CLOCK_TIME_IS_VALID (frame->pts)) {
        dts = frame->pts - dts_diff * max_reorder_num;
      } else {
        dts = frame->pts;
      }

      gst_queue_array_push_tail_struct (base->dts_queue, &dts);
      max_reorder_num--;
    }
  }

  gst_queue_array_push_tail_struct (base->dts_queue, &frame->pts);
}

GstClockTime
gst_va_base_enc_pop_dts (GstVaBaseEnc * base)
{
  GstClockTime dts;

  g_return_val_if_fail (gst_queue_array_get_length (base->dts_queue) > 0,
      GST_CLOCK_TIME_NONE);

  dts = *((GstClockTime *) gst_queue_array_pop_head_struct (base->dts_queue));
  return dts;
}

void
gst_va_base_enc_reset_state (GstVaBaseEnc * base)
{
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);

  g_assert (klass->reset_state);
  klass->reset_state (base);
}

/* *INDENT-OFF* */
#define UPDATE_PROPERTY                         \
  GST_OBJECT_LOCK (base);                       \
  if (*old_val == new_val) {                    \
    GST_OBJECT_UNLOCK (base);                   \
    return;                                     \
  }                                             \
  *old_val = new_val;                                                   \
  GST_OBJECT_UNLOCK (base);                                             \
  if (pspec)                                                            \
    g_object_notify_by_pspec (G_OBJECT (base), pspec);                  \

void
gst_va_base_enc_update_property_uint (GstVaBaseEnc * base, guint32 * old_val,
    guint32 new_val, GParamSpec * pspec)
{
  UPDATE_PROPERTY
}

void
gst_va_base_enc_update_property_bool (GstVaBaseEnc * base, gboolean * old_val,
    gboolean new_val, GParamSpec * pspec)
{
  UPDATE_PROPERTY
}

#undef UPDATE_PROPERTY
/* *INDENT-ON* */
