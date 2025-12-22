/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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

#include "gstvabasedec.h"

#include <gst/va/gstva.h>
#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>

#include "gstvacaps.h"
#include "gstvapluginutils.h"

#define GST_CAT_DEFAULT (base->debug_category)
#define GST_VA_BASE_DEC_GET_PARENT_CLASS(obj) (GST_VA_BASE_DEC_GET_CLASS(obj)->parent_decoder_class)

static void
gst_va_base_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaBaseDec *self = GST_VA_BASE_DEC (object);
  GstVaBaseDecClass *klass = GST_VA_BASE_DEC_GET_CLASS (self);

  switch (prop_id) {
    case GST_VA_DEC_PROP_DEVICE_PATH:{
      if (!self->display)
        g_value_set_string (value, klass->render_device_path);
      else if (GST_IS_VA_DISPLAY_PLATFORM (self->display))
        g_object_get_property (G_OBJECT (self->display), "path", value);
      else
        g_value_set_string (value, NULL);

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
gst_va_base_dec_open (GstVideoDecoder * decoder)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaBaseDecClass *klass = GST_VA_BASE_DEC_GET_CLASS (decoder);
  gboolean ret = FALSE;

  if (!gst_va_ensure_element_data (decoder, klass->render_device_path,
          &base->display))
    return FALSE;

  g_object_notify (G_OBJECT (decoder), "device-path");

  if (!g_atomic_pointer_get (&base->decoder)) {
    GstVaDecoder *va_decoder;

    va_decoder = gst_va_decoder_new (base->display, klass->codec);
    if (va_decoder)
      ret = TRUE;

    gst_object_replace ((GstObject **) (&base->decoder),
        (GstObject *) va_decoder);
    gst_clear_object (&va_decoder);
  } else {
    ret = TRUE;
  }

  base->apply_video_crop = FALSE;

  return ret;
}

gboolean
gst_va_base_dec_close (GstVideoDecoder * decoder)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  gst_clear_object (&base->decoder);
  gst_clear_object (&base->display);

  g_object_notify (G_OBJECT (decoder), "device-path");

  return TRUE;
}

static gboolean
gst_va_base_dec_stop (GstVideoDecoder * decoder)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  if (!gst_va_decoder_close (base->decoder))
    return FALSE;

  g_clear_pointer (&base->output_state, gst_video_codec_state_unref);
  g_clear_pointer (&base->input_state, gst_video_codec_state_unref);

  if (base->other_pool)
    gst_buffer_pool_set_active (base->other_pool, FALSE);
  gst_clear_object (&base->other_pool);

  g_clear_pointer (&base->convert, gst_video_converter_free);

  return GST_VIDEO_DECODER_CLASS (GST_VA_BASE_DEC_GET_PARENT_CLASS
      (decoder))->stop (decoder);
}

static GstCaps *
gst_va_base_dec_getcaps (GstVideoDecoder * decoder, GstCaps * filter)
{
  GstCaps *caps = NULL, *tmp;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecoder *va_decoder = NULL;

  gst_object_replace ((GstObject **) & va_decoder, (GstObject *) base->decoder);

  if (va_decoder) {
    caps = gst_va_decoder_get_sinkpad_caps (va_decoder);
    gst_object_unref (va_decoder);
  }

  if (caps) {
    if (filter) {
      tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = tmp;
    }
    GST_LOG_OBJECT (base, "Returning caps %" GST_PTR_FORMAT, caps);
  } else {
    caps = gst_video_decoder_proxy_getcaps (decoder, NULL, filter);
  }

  return caps;
}

static gboolean
_query_context (GstVaBaseDec * self, GstQuery * query)
{
  GstVaDisplay *display = NULL;
  gboolean ret;

  gst_object_replace ((GstObject **) & display, (GstObject *) self->display);
  ret = gst_va_handle_context_query (GST_ELEMENT_CAST (self), query, display);
  gst_clear_object (&display);

  return ret;
}

static gboolean
gst_va_base_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      ret = _query_context (base, query);
      break;
    }
    case GST_QUERY_CAPS:{
      GstCaps *caps = NULL, *tmp, *filter = NULL;
      GstVaDecoder *va_decoder = NULL;
      gboolean fixed_caps;

      gst_object_replace ((GstObject **) & va_decoder,
          (GstObject *) base->decoder);

      gst_query_parse_caps (query, &filter);

      fixed_caps = GST_PAD_IS_FIXED_CAPS (GST_VIDEO_DECODER_SRC_PAD (decoder));

      if (!fixed_caps && va_decoder)
        caps = gst_va_decoder_get_srcpad_caps (va_decoder);

      gst_clear_object (&va_decoder);

      if (caps) {
        if (filter) {
          tmp =
              gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
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
      ret = GST_VIDEO_DECODER_CLASS (GST_VA_BASE_DEC_GET_PARENT_CLASS
          (decoder))->src_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_va_base_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT)
    return _query_context (GST_VA_BASE_DEC (decoder), query);
  return GST_VIDEO_DECODER_CLASS (GST_VA_BASE_DEC_GET_PARENT_CLASS
      (decoder))->sink_query (decoder, query);
}

static GstAllocator *
_create_allocator (GstVaBaseDec * base, GstCaps * caps)
{
  GstAllocator *allocator = NULL;

  if (gst_caps_is_dmabuf (caps))
    allocator = gst_va_dmabuf_allocator_new (base->display);
  else {
    GArray *surface_formats =
        gst_va_decoder_get_surface_formats (base->decoder);
    allocator = gst_va_allocator_new (base->display, surface_formats);
    gst_va_allocator_set_hacks (allocator, base->hacks);
  }

  return allocator;
}

static gboolean
_need_video_crop (GstVaBaseDec * base)
{

  if (base->need_valign &&
      (base->valign.padding_left > 0 || base->valign.padding_top > 0))
    return TRUE;

  return FALSE;
}

static GstVaFeature
_use_derived (GstVaBaseDec * base)
{
  /* i965 driver has a very poor performance reading derived images */
  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_I965))
    return GST_VA_FEATURE_DISABLED;
  return GST_VA_FEATURE_AUTO;
}

/* This path for pool setting is a little complicated but not commonly
   used. We deliberately separate it from the main path of pool setting. */
static gboolean
_decide_allocation_for_video_crop (GstVideoDecoder * decoder,
    GstQuery * query, GstCaps * caps)
{
  GstAllocator *allocator = NULL, *other_allocator = NULL;
  GstAllocationParams other_params, params;
  gboolean update_pool = FALSE, update_allocator = FALSE;
  GstBufferPool *pool = NULL, *other_pool = NULL;
  guint size = 0, min, max, usage_hint;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  gboolean ret = FALSE;
  gboolean dont_use_other_pool = FALSE;
  GstCaps *va_caps = NULL;

  /* If others provide a valid allocator, just use it. */
  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &other_allocator,
        &other_params);
    GstVaDisplay *display;

    display = gst_va_allocator_peek_display (other_allocator);
    /* We should not use allocator and pool from other display. */
    if (display != base->display) {
      gst_clear_object (&other_allocator);
      dont_use_other_pool = TRUE;
    }

    update_allocator = TRUE;
  } else {
    gst_allocation_params_init (&other_params);
  }

  /* If others provide a valid pool, just use it. */
  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &other_pool, &size, &min,
        &max);
    if (dont_use_other_pool)
      gst_clear_object (&other_pool);

    min += base->min_buffers;
    update_pool = TRUE;
  } else {
    min = base->min_buffers;
    max = 0;
  }

  /* Ensure that the other pool is ready */
  if (gst_caps_is_raw (caps)) {
    if (GST_IS_VA_POOL (other_pool)) {
      gst_clear_object (&other_pool);
      size = 0;
    }

    if (!other_pool) {
      if (other_allocator && (GST_IS_VA_DMABUF_ALLOCATOR (other_allocator)
              || GST_IS_VA_ALLOCATOR (other_allocator)))
        gst_clear_object (&other_allocator);

      GST_DEBUG_OBJECT (base, "making new other pool for copy");
      base->other_pool =
          gst_va_create_other_pool (other_allocator, &other_params, caps, size);
    } else {
      gst_object_replace ((GstObject **) & base->other_pool,
          (GstObject *) other_pool);
    }
    if (!base->other_pool) {
      GST_ERROR_OBJECT (base, "Couldn't configure other pool for copy");
      goto cleanup;
    }
  } else {
    GstStructure *other_config;

    if (!GST_IS_VA_POOL (other_pool))
      gst_clear_object (&other_pool);

    if (!other_pool)
      other_pool = gst_va_pool_new ();

    if (other_allocator && !(GST_IS_VA_DMABUF_ALLOCATOR (other_allocator)
            || GST_IS_VA_ALLOCATOR (other_allocator)))
      gst_clear_object (&other_allocator);

    if (!other_allocator) {
      other_allocator = _create_allocator (base, caps);
      if (!other_allocator)
        goto cleanup;
    }

    other_config = gst_buffer_pool_get_config (other_pool);

    gst_buffer_pool_config_set_params (other_config, caps, 0, min, max);
    gst_buffer_pool_config_set_allocator (other_config, other_allocator,
        &other_params);
    /* Always support VideoMeta but no VideoCropMeta here. */
    gst_buffer_pool_config_add_option (other_config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    gst_buffer_pool_config_set_va_allocation_params (other_config,
        VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC, _use_derived (base));

    if (!gst_buffer_pool_set_config (other_pool, other_config))
      goto cleanup;

    gst_object_replace ((GstObject **) & base->other_pool,
        (GstObject *) other_pool);
  }

  /* Now setup the buffer pool for decoder */

  va_caps = gst_caps_copy (caps);
  gst_caps_set_features_simple (va_caps,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA));

  if (!(allocator = _create_allocator (base, va_caps)))
    goto cleanup;

  gst_allocation_params_init (&params);

  pool = gst_va_pool_new ();
  {
    GstStructure *config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_set_params (config, caps, 0, min, max);
    gst_buffer_pool_config_set_allocator (config, allocator, &params);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    if (_need_video_crop (base))
      gst_buffer_pool_config_set_va_alignment (config, &base->valign);

    usage_hint = va_get_surface_usage_hint (base->display,
        VAEntrypointVLD, GST_PAD_SRC, gst_video_is_dma_drm_caps (caps));

    gst_buffer_pool_config_set_va_allocation_params (config,
        usage_hint, _use_derived (base));

    if (!gst_buffer_pool_set_config (pool, config))
      goto cleanup;
    if (!gst_va_pool_get_buffer_size (pool, &size))
      goto cleanup;
  }

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  GST_WARNING_OBJECT (base, "We need to copy the output buffer manually "
      "because of the top/left alignment, which may have low performance. "
      "The element which supports VideoCropMeta such as 'vapostproc' can "
      "avoid this.");
  base->copy_frames = TRUE;
  base->apply_video_crop = TRUE;

  ret = TRUE;

cleanup:
  gst_clear_object (&allocator);
  gst_clear_object (&other_allocator);
  gst_clear_object (&pool);
  gst_clear_object (&other_pool);
  gst_clear_caps (&va_caps);

  return ret;
}

/* We only support system pool and va pool. For va pool, its allocator
 * should be va allocator or dma allocator.
 *   If output caps is memory:VAMemory, the pool should be a va pool
 *   with va allocator.
 *   If output caps is memory:DMABuf, the pool should be a va pool
 *   with dma allocator.
 *   We may need the other_pool to copy the decoder picture to the
 *   output buffer. We need to do this copy when:
 *   1). The output caps is raw(system mem), but the downstream does
 *   not support VideoMeta and the strides and offsets of the va pool
 *   are different from the system memory pool, which means that the
 *   gst_video_frame_map() can not map the buffer correctly. Then we
 *   need a va pool with va allocator as an the internal pool and create
 *   a system pool as the other_pool to copy frames to system mem and
 *   output it.
 *   2). The decoder has crop_top/left value > 0(e.g. the conformance
 *   window in the H265). Which means that the real output picture
 *   locates in the middle of the decoded buffer. If the downstream can
 *   support VideoCropMeta, a VideoCropMeta is added to notify the
 *   real picture's coordinate and size. But if not, we need to copy
 *   it manually and the other_pool is needed. We always assume that
 *   decoded picture starts from top-left corner, and so there is no
 *   need to do this if crop_bottom/right value > 0.
 *
 * 1. if crop_top/left value > 0 and the downstream does not support the
 *    VideoCropMeta, we always have the other_pool to do the copy(The pool
 *    may be provided by the downstream element, or created by ourself if
 *    no suitable one found).
 * 2. get allocator in query
 *    2.1 if allocator is not ours and caps is raw, keep it for other_pool.
 * 3. get pool in query
 *    3.1 if pool is not va, downstream doesn't support video meta and
 *        caps are raw, keep it as other_pool.
 *    3.2 if there's no pool in query and and caps is raw, create other_pool
 *        as GstVideoPool with the non-va from query and query's params.
 * 4. create our allocator and pool if they aren't in query
 * 5. add or update pool and allocator in query
 * 6. set our custom pool configuration
 */
static gboolean
gst_va_base_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstAllocator *allocator = NULL, *other_allocator = NULL;
  GstAllocationParams other_params, params;
  GstBufferPool *pool = NULL, *other_pool = NULL;
  GstCaps *caps = NULL;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  guint other_size = 0, size = 0, min, max, usage_hint;
  gboolean update_pool = FALSE, update_allocator = FALSE;
  gboolean has_videometa, has_video_crop_meta;
  gboolean dont_use_other_pool = FALSE, ret = FALSE;

  g_assert (base->min_buffers > 0);

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps)
    goto wrong_caps;

  has_videometa = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, NULL);
  has_video_crop_meta = has_videometa && gst_query_find_allocation_meta (query,
      GST_VIDEO_CROP_META_API_TYPE, NULL);

  if (gst_video_is_dma_drm_caps (caps) && !has_videometa) {
    GST_ERROR_OBJECT (base,
        "DMABuf caps negotiated without the mandatory support of VideoMeta ");
    return FALSE;
  }

  /* 1. The output picture locates in the middle of the decoded buffer,
     but the downstream element does not support VideoCropMeta, we
     definitely need a copy.
     2. Some codec such as H265, it does not clean the DPB when new SPS
     comes. The new SPS may set the crop window to top-left corner and
     so no video crop is needed here. But we may still have cached frames
     in DPB which need a copy.
     3. For DMA kind memory, because we may not be able to map this buffer,
     just disable the copy for crop. This may cause some alignment garbage. */
  if (!gst_video_is_dma_drm_caps (caps) &&
      ((_need_video_crop (base) && !has_video_crop_meta) ||
          base->apply_video_crop)) {
    return _decide_allocation_for_video_crop (decoder, query, caps);
  }

  if (gst_query_get_n_allocation_params (query) > 0) {
    GstVaDisplay *display;

    gst_query_parse_nth_allocation_param (query, 0, &allocator, &other_params);
    display = gst_va_allocator_peek_display (allocator);
    if (!display) {
      /* save the allocator for the other pool */
      other_allocator = allocator;
      allocator = NULL;
    } else if (display != base->display) {
      /* The allocator and pool belong to other display, we should not use. */
      gst_clear_object (&allocator);
      dont_use_other_pool = TRUE;
    }

    update_allocator = TRUE;
  } else {
    gst_allocation_params_init (&other_params);
  }

  gst_allocation_params_init (&params);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_VA_POOL (pool)) {
        GST_DEBUG_OBJECT (base,
            "may need other pool for copy frames %" GST_PTR_FORMAT, pool);
        other_pool = pool;
        pool = NULL;
        other_size = size;
      } else if (dont_use_other_pool) {
        gst_clear_object (&pool);
      }
    }

    min += base->min_buffers;
    update_pool = TRUE;
  } else {
    min = base->min_buffers;
    max = 0;
  }

  if (!allocator) {
    if (!(allocator = _create_allocator (base, caps)))
      goto cleanup;
  }

  if (!pool)
    pool = gst_va_pool_new ();

  {
    GstStructure *config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_set_params (config, caps, 0, min, max);
    gst_buffer_pool_config_set_allocator (config, allocator, &params);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    if (base->need_valign)
      gst_buffer_pool_config_set_va_alignment (config, &base->valign);

    usage_hint = va_get_surface_usage_hint (base->display,
        VAEntrypointVLD, GST_PAD_SRC, gst_video_is_dma_drm_caps (caps));

    gst_buffer_pool_config_set_va_allocation_params (config,
        usage_hint, _use_derived (base));

    if (!gst_buffer_pool_set_config (pool, config))
      goto cleanup;
    if (!gst_va_pool_get_buffer_size (pool, &size))
      goto cleanup;
  }

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  base->copy_frames = (!has_videometa && gst_va_pool_requires_video_meta (pool)
      && gst_caps_is_raw (caps));
  if (base->copy_frames) {
    if (other_pool) {
      gst_object_replace ((GstObject **) & base->other_pool,
          (GstObject *) other_pool);
    } else {
      gst_clear_object (&base->other_pool);
      base->other_pool =
          gst_va_create_other_pool (other_allocator, &other_params, caps,
          other_size);
    }
    if (!base->other_pool) {
      GST_ERROR_OBJECT (base, "Couldn't configure other pool for copy");
      goto cleanup;
    }
    GST_DEBUG_OBJECT (base, "Use the other pool for copy %" GST_PTR_FORMAT,
        base->other_pool);
  } else {
    gst_clear_object (&base->other_pool);
  }

  ret = TRUE;

cleanup:
  gst_clear_object (&allocator);
  gst_clear_object (&other_allocator);
  gst_clear_object (&pool);
  gst_clear_object (&other_pool);

  /* There's no need to chain decoder's method since all what is
   * needed is done. */
  return ret;

wrong_caps:
  {
    GST_WARNING_OBJECT (base, "No valid caps");
    return FALSE;
  }
}

static void
gst_va_base_dec_set_context (GstElement * element, GstContext * context)
{
  GstVaDisplay *old_display, *new_display;
  GstVaBaseDec *base = GST_VA_BASE_DEC (element);
  GstVaBaseDecClass *klass = GST_VA_BASE_DEC_GET_CLASS (base);
  gboolean ret;

  old_display = base->display ? gst_object_ref (base->display) : NULL;
  ret = gst_va_handle_set_context (element, context, klass->render_device_path,
      &base->display);
  new_display = base->display ? gst_object_ref (base->display) : NULL;

  if (!ret
      || (old_display && new_display && old_display != new_display
          && base->decoder)) {
    GST_WARNING_OBJECT (base, "Can't replace VA display while operating");
  }

  gst_clear_object (&old_display);
  gst_clear_object (&new_display);

  GST_ELEMENT_CLASS (GST_VA_BASE_DEC_GET_PARENT_CLASS
      (element))->set_context (element, context);
}

static gboolean
gst_va_base_dec_negotiate (GstVideoDecoder * decoder)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  /* Ignore downstream renegotiation request. */
  if (!base->need_negotiation)
    return TRUE;

  base->need_negotiation = FALSE;

  if (!gst_va_decoder_config_is_equal (base->decoder, base->profile,
          base->rt_format, base->width, base->height)) {
    if (gst_va_decoder_is_open (base->decoder) &&
        !gst_va_decoder_close (base->decoder))
      return FALSE;
    if (!gst_va_decoder_open (base->decoder, base->profile, base->rt_format))
      return FALSE;
    if (!gst_va_decoder_set_frame_size (base->decoder, base->width,
            base->height))
      return FALSE;
  }

  if (!gst_va_base_dec_set_output_state (base))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (GST_VA_BASE_DEC_GET_PARENT_CLASS (decoder))
      ->negotiate (decoder);
}

void
gst_va_base_dec_init (GstVaBaseDec * base, GstDebugCategory * cat)
{
  base->debug_category = cat;
  gst_video_info_init (&base->output_info);
}

void
gst_va_base_dec_class_init (GstVaBaseDecClass * klass, GstVaCodecs codec,
    const gchar * render_device_path, GstCaps * sink_caps, GstCaps * src_caps,
    GstCaps * doc_src_caps, GstCaps * doc_sink_caps)
{
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  klass->parent_decoder_class = g_type_class_peek_parent (klass);

  klass->codec = codec;
  klass->render_device_path = g_strdup (render_device_path);

  sink_pad_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      sink_caps);
  gst_element_class_add_pad_template (element_class, sink_pad_templ);

  if (doc_sink_caps) {
    gst_pad_template_set_documentation_caps (sink_pad_templ, doc_sink_caps);
    gst_caps_unref (doc_sink_caps);
  }

  src_pad_templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      src_caps);
  gst_element_class_add_pad_template (element_class, src_pad_templ);

  if (doc_src_caps) {
    gst_pad_template_set_documentation_caps (src_pad_templ, doc_src_caps);
    gst_caps_unref (doc_src_caps);
  }

  object_class->get_property = gst_va_base_dec_get_property;

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_va_base_dec_set_context);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_va_base_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_va_base_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_va_base_dec_stop);
  decoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_va_base_dec_getcaps);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_va_base_dec_src_query);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_va_base_dec_sink_query);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_va_base_dec_decide_allocation);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_va_base_dec_negotiate);

  g_object_class_install_property (object_class, GST_VA_DEC_PROP_DEVICE_PATH,
      g_param_spec_string ("device-path", "Device Path",
          GST_VA_DEVICE_PATH_PROP_DESC, NULL, GST_PARAM_DOC_SHOW_DEFAULT |
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static inline GstVideoFormat
_get_video_format_from_value (const GValue * format, gboolean drm_format,
    guint64 * modifier)
{
  guint32 fourcc;
  const char *fmt;

  g_assert (G_VALUE_HOLDS_STRING (format));

  fmt = g_value_get_string (format);

  if (drm_format) {
    fourcc = gst_video_dma_drm_fourcc_from_string (fmt, modifier);
    return gst_va_video_format_from_drm_fourcc (fourcc);
  }

  return gst_video_format_from_string (fmt);
}

static GstVideoFormat
_find_video_format_from_chroma (const GValue * formats, guint chroma_type,
    gboolean drm_format, guint64 * modifier)
{
  GstVideoFormat fmt = GST_VIDEO_FORMAT_UNKNOWN;
  guint i, num_values;

  if (!formats)
    return GST_VIDEO_FORMAT_UNKNOWN;

  if (G_VALUE_HOLDS_STRING (formats)) {
    fmt = _get_video_format_from_value (formats, drm_format, modifier);
    if (gst_va_chroma_from_video_format (fmt) == chroma_type)
      return fmt;
  } else if (GST_VALUE_HOLDS_LIST (formats)) {
    const GValue *format;

    num_values = gst_value_list_get_size (formats);
    for (i = 0; i < num_values; i++) {
      format = gst_value_list_get_value (formats, i);
      fmt = _get_video_format_from_value (format, drm_format, modifier);

      if (gst_va_chroma_from_video_format (fmt) == chroma_type)
        return fmt;
    }
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

static GstVideoFormat
_caps_video_format_from_chroma (GstCaps * caps, guint chroma_type)
{
  guint i, num_structures;
  GstCapsFeatures *feats;
  GstStructure *structure;
  const GValue *format;
  GstVideoFormat fmt;

  num_structures = gst_caps_get_size (caps);
  for (i = 0; i < num_structures; i++) {
    feats = gst_caps_get_features (caps, i);
    if (!gst_caps_features_is_equal (feats,
            GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
      continue;
    structure = gst_caps_get_structure (caps, i);
    format = gst_structure_get_value (structure, "format");

    fmt = _find_video_format_from_chroma (format, chroma_type, FALSE, NULL);
    if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
      continue;
    if (gst_va_chroma_from_video_format (fmt) == chroma_type)
      return fmt;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

/* ordered list of capsfeature preference */
enum
{ VA, DMABUF, SYSMEM };

void
gst_va_base_dec_get_preferred_format_and_caps_features (GstVaBaseDec * base,
    GstVideoFormat * format, GstCapsFeatures ** capsfeatures,
    guint64 * modifier)
{
  GstCaps *peer_caps, *allowed_caps;
  GstCapsFeatures *features;
  guint num_structures, i, j;
  gboolean is_any;
  /* array designators might not be supported by some compilers */
  const GQuark feats[] = {
    /* [VA] = */ g_quark_from_string (GST_CAPS_FEATURE_MEMORY_VA),
    /* [DMABUF] = */ g_quark_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF),
    /* [SYSMEM] = */
    g_quark_from_string (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY),
  };

  g_return_if_fail (base);

  /* verify if peer caps is any */
  {
    peer_caps =
        gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (base), NULL);
    is_any = gst_caps_is_any (peer_caps);
    gst_clear_caps (&peer_caps);
  }

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD (base));
  GST_DEBUG_OBJECT (base, "Allowed caps %" GST_PTR_FORMAT, allowed_caps);

  /* if peer caps is any, returns format according memory to system caps in
   * allowed caps
   *
   * if downstream doesn't support system memory negotiation will fail later.
   */
  if (is_any) {
    GstVideoFormat fmt =
        _caps_video_format_from_chroma (allowed_caps, base->rt_format);
    features = GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY;

    if (format)
      *format = fmt;
    if (capsfeatures && fmt != GST_VIDEO_FORMAT_UNKNOWN)
      *capsfeatures = gst_caps_features_copy (features);
    goto bail;
  }

  /* iterate allowed caps to find the first "capable" capability according our
   * ordered list of preferred caps features */
  num_structures = gst_caps_get_size (allowed_caps);
  for (i = 0; i < G_N_ELEMENTS (feats); i++) {
    for (j = 0; j < num_structures; j++) {
      GstStructure *structure;
      const GValue *formats;
      GstVideoFormat fmt;
      guint64 mod = 0;

      features = gst_caps_get_features (allowed_caps, j);
      if (!gst_caps_features_contains_id (features, feats[i]))
        continue;

      structure = gst_caps_get_structure (allowed_caps, j);
      if (i == DMABUF)
        formats = gst_structure_get_value (structure, "drm-format");
      else
        formats = gst_structure_get_value (structure, "format");

      fmt = _find_video_format_from_chroma (formats, base->rt_format,
          i == DMABUF, &mod);

      /* if doesn't found a proper format let's try other structure */
      if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      if (format)
        *format = fmt;
      if (i == DMABUF && modifier)
        *modifier = mod;
      if (capsfeatures)
        *capsfeatures = gst_caps_features_new_id (feats[i], NULL);

      goto bail;
    }
  }

  /* no matching format, let's fail */
  if (i == G_N_ELEMENTS (feats))
    *format = GST_VIDEO_FORMAT_UNKNOWN;

bail:
  gst_caps_unref (allowed_caps);
}

static gboolean
_copy_buffer_and_apply_video_crop (GstVaBaseDec * base,
    GstVideoFrame * src_frame, GstVideoFrame * dest_frame,
    GstVideoCropMeta * video_crop)
{
  GstVideoInfo dst_info = dest_frame->info;

  dst_info.fps_n = src_frame->info.fps_n;
  dst_info.fps_d = src_frame->info.fps_d;

  if (base->convert) {
    gboolean new_convert = FALSE;
    gint x = 0, y = 0, width = 0, height = 0;
    const GstStructure *config = gst_video_converter_get_config (base->convert);

    if (!gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_X, &x)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_Y, &y)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_WIDTH,
            &width)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT,
            &height))
      new_convert = TRUE;

    new_convert |= (video_crop->x != x);
    new_convert |= (video_crop->y != y);
    new_convert |= (video_crop->width != width);
    new_convert |= (video_crop->height != height);

    /* No need to check dest, it always has (0,0) -> (width, height) */

    if (new_convert)
      g_clear_pointer (&base->convert, gst_video_converter_free);
  }

  if (!base->convert) {
    base->convert = gst_video_converter_new (&src_frame->info, &dst_info,
        gst_structure_new ("options",
            GST_VIDEO_CONVERTER_OPT_DITHER_METHOD,
            GST_TYPE_VIDEO_DITHER_METHOD, GST_VIDEO_DITHER_NONE,
            GST_VIDEO_CONVERTER_OPT_DITHER_QUANTIZATION,
            G_TYPE_UINT, 0,
            GST_VIDEO_CONVERTER_OPT_CHROMA_MODE,
            GST_TYPE_VIDEO_CHROMA_MODE, GST_VIDEO_CHROMA_MODE_NONE,
            GST_VIDEO_CONVERTER_OPT_MATRIX_MODE,
            GST_TYPE_VIDEO_MATRIX_MODE, GST_VIDEO_MATRIX_MODE_NONE,
            GST_VIDEO_CONVERTER_OPT_SRC_X, G_TYPE_INT, video_crop->x,
            GST_VIDEO_CONVERTER_OPT_SRC_Y, G_TYPE_INT, video_crop->y,
            GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, G_TYPE_INT, video_crop->width,
            GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT, G_TYPE_INT, video_crop->height,
            GST_VIDEO_CONVERTER_OPT_DEST_X, G_TYPE_INT, 0,
            GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 0,
            GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, video_crop->width,
            GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT, video_crop->height,
            NULL));

    if (!base->convert) {
      GST_WARNING_OBJECT (base, "failed to create a video convert");
      return FALSE;
    }
  }

  gst_video_converter_frame (base->convert, src_frame, dest_frame);

  return TRUE;
}

gboolean
gst_va_base_dec_copy_output_buffer (GstVaBaseDec * base,
    GstVideoCodecFrame * codec_frame)
{
  GstVideoFrame src_frame;
  GstVideoFrame dest_frame;
  GstVideoInfo dest_vinfo;
  GstVideoInfo *src_vinfo;
  GstBuffer *buffer = NULL;
  GstVideoCropMeta *video_crop;
  GstFlowReturn ret;

  g_return_val_if_fail (base && base->output_state, FALSE);

  if (!base->other_pool)
    return FALSE;

  if (!gst_buffer_pool_set_active (base->other_pool, TRUE))
    return FALSE;

  src_vinfo = &base->output_state->info;
  gst_video_info_set_format (&dest_vinfo, GST_VIDEO_INFO_FORMAT (src_vinfo),
      GST_VIDEO_INFO_WIDTH (src_vinfo), GST_VIDEO_INFO_HEIGHT (src_vinfo));

  ret = gst_buffer_pool_acquire_buffer (base->other_pool, &buffer, NULL);
  if (ret != GST_FLOW_OK)
    goto fail;
  if (!gst_video_frame_map (&src_frame, src_vinfo, codec_frame->output_buffer,
          GST_MAP_READ))
    goto fail;
  if (!gst_video_frame_map (&dest_frame, &dest_vinfo, buffer, GST_MAP_WRITE)) {
    gst_video_frame_unmap (&src_frame);
    goto fail;
  }

  video_crop = gst_buffer_get_video_crop_meta (codec_frame->output_buffer);
  if (video_crop) {
    if (!_copy_buffer_and_apply_video_crop (base,
            &src_frame, &dest_frame, video_crop)) {
      gst_video_frame_unmap (&src_frame);
      gst_video_frame_unmap (&dest_frame);
      GST_ERROR_OBJECT (base, "fail to apply the video crop.");
      goto fail;
    }
  } else {
    /* gst_video_frame_copy can crop this, but does not know, so let
     * make it think it's all right */
    GST_VIDEO_INFO_WIDTH (&src_frame.info) = GST_VIDEO_INFO_WIDTH (src_vinfo);
    GST_VIDEO_INFO_HEIGHT (&src_frame.info) = GST_VIDEO_INFO_HEIGHT (src_vinfo);

    if (!gst_video_frame_copy (&dest_frame, &src_frame)) {
      gst_video_frame_unmap (&src_frame);
      gst_video_frame_unmap (&dest_frame);
      goto fail;
    }
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dest_frame);
  gst_buffer_replace (&codec_frame->output_buffer, buffer);
  gst_buffer_unref (buffer);

  return TRUE;

fail:
  if (buffer)
    gst_buffer_unref (buffer);

  GST_ERROR_OBJECT (base, "Failed copy output buffer.");
  return FALSE;
}

gboolean
gst_va_base_dec_process_output (GstVaBaseDec * base, GstVideoCodecFrame * frame,
    GstVideoCodecState * input_state, GstVideoBufferFlags buffer_flags)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (base);

  if (input_state) {
    g_clear_pointer (&base->input_state, gst_video_codec_state_unref);
    base->input_state = gst_video_codec_state_ref (input_state);

    base->need_negotiation = TRUE;
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (base, "Could not re-negotiate with updated state");
      return FALSE;
    }
  }

  if (base->copy_frames)
    gst_va_base_dec_copy_output_buffer (base, frame);

  if (buffer_flags != 0) {
#ifndef GST_DISABLE_GST_DEBUG
    gboolean interlaced =
        (buffer_flags & GST_VIDEO_BUFFER_FLAG_INTERLACED) != 0;
    gboolean tff = (buffer_flags & GST_VIDEO_BUFFER_FLAG_TFF) != 0;

    GST_TRACE_OBJECT (base,
        "apply buffer flags 0x%x (interlaced %d, top-field-first %d)",
        buffer_flags, interlaced, tff);
#endif
    GST_BUFFER_FLAG_SET (frame->output_buffer, buffer_flags);
  }

  return TRUE;
}

GstFlowReturn
gst_va_base_dec_prepare_output_frame (GstVaBaseDec * base,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (base);

  if (base->need_negotiation) {
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (base, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  if (frame)
    return gst_video_decoder_allocate_output_frame (vdec, frame);
  return GST_FLOW_OK;
}

gboolean
gst_va_base_dec_set_output_state (GstVaBaseDec * base)
{
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (base);
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  guint64 modifier;
  GstCapsFeatures *capsfeatures = NULL;
  GstVideoInfo *info = &base->output_info;

  if (base->output_state)
    gst_video_codec_state_unref (base->output_state);

  gst_va_base_dec_get_preferred_format_and_caps_features (base, &format,
      &capsfeatures, &modifier);
  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  base->output_state =
      gst_video_decoder_set_interlaced_output_state (decoder, format,
      GST_VIDEO_INFO_INTERLACE_MODE (info), GST_VIDEO_INFO_WIDTH (info),
      GST_VIDEO_INFO_HEIGHT (info), base->input_state);

  /* set caps feature */
  if (capsfeatures && gst_caps_features_contains (capsfeatures,
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    base->output_state->caps =
        gst_va_video_info_to_dma_caps (&base->output_state->info, modifier);
  } else {
    base->output_state->caps =
        gst_video_info_to_caps (&base->output_state->info);
  }

  if (capsfeatures)
    gst_caps_set_features_simple (base->output_state->caps, capsfeatures);

  GST_INFO_OBJECT (base, "Negotiated caps %" GST_PTR_FORMAT,
      base->output_state->caps);

  return TRUE;
}
