/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Author: Sreerenj Balachaandran <sreerenj.balachandran@intel.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION: element-msdkvpp
 * @title: msdkvpp
 * @short_description: MSDK Video Postprocessor
 *
 * A MediaSDK Video Postprocessing Filter
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! msdkvpp ! glimagesink
 * ```
 *
 * Since: 1.16
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>

#include "gstmsdkvpp.h"
#include "gstmsdkcaps.h"
#include "gstmsdkcontextutil.h"
#include "gstmsdkvpputil.h"
#include "gstmsdkallocator.h"

#ifndef _WIN32
#include <libdrm/drm_fourcc.h>
#include "gstmsdkallocator_libva.h"
#include <gst/va/gstvaallocator.h>
#include <gst/va/gstvavideoformat.h>
#else
#include <gst/d3d11/gstd3d11.h>
#endif


GST_DEBUG_CATEGORY_EXTERN (gst_msdkvpp_debug);
#define GST_CAT_DEFAULT gst_msdkvpp_debug

#define GST_MSDKVPP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkVPP))
#define GST_MSDKVPP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkVPPClass))
#define GST_IS_MSDKVPP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKVPP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

enum
{
  PROP_0,
  PROP_HARDWARE,
  PROP_ASYNC_DEPTH,
  PROP_DENOISE,
#ifndef GST_REMOVE_DEPRECATED
  PROP_ROTATION,
#endif
  PROP_DEINTERLACE_MODE,
  PROP_DEINTERLACE_METHOD,
  PROP_HUE,
  PROP_SATURATION,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
  PROP_DETAIL,
#ifndef GST_REMOVE_DEPRECATED
  PROP_MIRRORING,
#endif
  PROP_SCALING_MODE,
  PROP_FORCE_ASPECT_RATIO,
  PROP_FRC_ALGORITHM,
  PROP_VIDEO_DIRECTION,
  PROP_CROP_LEFT,
  PROP_CROP_RIGHT,
  PROP_CROP_TOP,
  PROP_CROP_BOTTOM,
  PROP_HDR_TONE_MAPPING,
  PROP_N,
};

#define PROP_HARDWARE_DEFAULT            TRUE
#define PROP_ASYNC_DEPTH_DEFAULT         1
#define PROP_DENOISE_DEFAULT             0
#ifndef GST_REMOVE_DEPRECATED
#define PROP_ROTATION_DEFAULT            MFX_ANGLE_0
#define PROP_MIRRORING_DEFAULT           MFX_MIRRORING_DISABLED
#endif
#define PROP_DEINTERLACE_MODE_DEFAULT    GST_MSDKVPP_DEINTERLACE_MODE_AUTO
#define PROP_DEINTERLACE_METHOD_DEFAULT  MFX_DEINTERLACING_BOB
#define PROP_HUE_DEFAULT                 0
#define PROP_SATURATION_DEFAULT          1
#define PROP_BRIGHTNESS_DEFAULT          0
#define PROP_CONTRAST_DEFAULT            1
#define PROP_DETAIL_DEFAULT              0
#define PROP_SCALING_MODE_DEFAULT        MFX_SCALING_MODE_DEFAULT
#define PROP_FORCE_ASPECT_RATIO_DEFAULT  TRUE
#define PROP_FRC_ALGORITHM_DEFAULT       _MFX_FRC_ALGORITHM_NONE
#define PROP_VIDEO_DIRECTION_DEFAULT     GST_VIDEO_ORIENTATION_IDENTITY
#define PROP_CROP_LEFT_DEFAULT           0
#define PROP_CROP_RIGHT_DEFAULT          0
#define PROP_CROP_TOP_DEFAULT            0
#define PROP_CROP_BOTTOM_DEFAULT         0
#define PROP_HDR_TONE_MAPPING_DEFAULT    0

/* 8 should enough for a normal encoder */
#define SRC_POOL_SIZE_DEFAULT            8

/* *INDENT-OFF* */
static const gchar *doc_sink_caps_str =
    GST_VIDEO_CAPS_MAKE (
        "{ NV12, YV12, I420, P010_10LE, YUY2, UYVY, BGRA, BGRx, RGB16, VUYA, "
        "Y210, Y410, P012_LE, Y212_LE, Y412_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, YV12, I420, P010_10LE, YUY2, UYVY, BGRA, BGRx, RGB16, VUYA, "
        "Y210, Y410, P012_LE, Y212_LE, Y412_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory",
        "{ NV12, VUYA, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory",
        "{ NV12, VUYA, P010_10LE }");

static const gchar *doc_src_caps_str =
    GST_VIDEO_CAPS_MAKE (
        "{ NV12, BGRA, YUY2, UYVY, VUYA, BGRx, P010_10LE, BGR10A2_LE, YV12, "
        "Y410, Y210, RGBP, BGRP, P012_LE, Y212_LE, Y412_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, BGRA, YUY2, UYVY, VUYA, BGRx, P010_10LE, BGR10A2_LE, YV12, "
        "Y410, Y210, RGBP, BGRP, P012_LE, Y212_LE, Y412_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory",
        "{ NV12, VUYA, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory",
        "{ NV12, VUYA, P010_10LE }");
/* *INDENT-ON* */

static GstElementClass *parent_class = NULL;

typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
} MsdkVPPCData;

static void
free_msdk_surface (gpointer p)
{
  GstMsdkSurface *surface = (GstMsdkSurface *) p;
  if (surface->buf)
    gst_buffer_unref (surface->buf);
  g_slice_free (GstMsdkSurface, surface);
}

static void
release_msdk_surface (GstMsdkVPP * thiz, GstMsdkSurface * surface,
    GList ** list)
{
  if (surface->surface) {
    if (surface->surface->Data.Locked) {
      *list = g_list_append (*list, surface);
    } else {
      free_msdk_surface (surface);
    }
  }
}

static void
release_in_surface (GstMsdkVPP * thiz, GstMsdkSurface * surface,
    gboolean locked_by_others)
{
  if (locked_by_others) {
    /* mfxFrameSurface1 locked by others, others will hold the surface->buf reference */
    /* we are good to release it here */
    free_msdk_surface (surface);
  } else {
    release_msdk_surface (thiz, surface, &thiz->locked_in_surfaces);
  }
}

static void
release_out_surface (GstMsdkVPP * thiz, GstMsdkSurface * surface)
{
  release_msdk_surface (thiz, surface, &thiz->locked_out_surfaces);
}

static void
free_unlocked_msdk_surfaces_from_list (GstMsdkVPP * thiz, GList ** list)
{
  GList *l;
  GstMsdkSurface *surface;

  for (l = *list; l;) {
    GList *next = l->next;
    surface = l->data;
    if (surface->surface->Data.Locked == 0) {
      free_msdk_surface (surface);
      *list = g_list_delete_link (*list, l);
    }
    l = next;
  }
}

static void
free_unlocked_msdk_surfaces (GstMsdkVPP * thiz)
{
  free_unlocked_msdk_surfaces_from_list (thiz, &thiz->locked_in_surfaces);
  free_unlocked_msdk_surfaces_from_list (thiz, &thiz->locked_out_surfaces);
}

static void
free_all_msdk_surfaces (GstMsdkVPP * thiz)
{
  g_list_free_full (thiz->locked_in_surfaces, free_msdk_surface);
  thiz->locked_in_surfaces = NULL;
  g_list_free_full (thiz->locked_out_surfaces, free_msdk_surface);
  thiz->locked_out_surfaces = NULL;
}

static void
gst_msdkvpp_add_extra_param (GstMsdkVPP * thiz, mfxExtBuffer * param)
{
  if (thiz->num_extra_params < MAX_EXTRA_PARAMS) {
    thiz->extra_params[thiz->num_extra_params] = param;
    thiz->num_extra_params++;
  }
}

static gboolean
gst_msdkvpp_context_prepare (GstMsdkVPP * thiz)
{
  /* Try to find an existing context from the pipeline. This may (indirectly)
   * invoke gst_msdkvpp_set_context, which will set thiz->context. */
  if (!gst_msdk_context_find (GST_ELEMENT_CAST (thiz), &thiz->context))
    return FALSE;

  if (thiz->context == thiz->old_context) {
    GST_INFO_OBJECT (thiz, "Found old context %" GST_PTR_FORMAT
        ", reusing as-is", thiz->context);
    return TRUE;
  }

  GST_INFO_OBJECT (thiz, "Found context %" GST_PTR_FORMAT " from neighbour",
      thiz->context);

  /* Check GST_MSDK_JOB_VPP and GST_MSDK_JOB_ENCODER together to avoid sharing context
   * between VPP and ENCODER
   * Example:
   * gst-launch-1.0 videotestsrc ! msdkvpp ! video/x-raw,format=YUY2 ! msdkh264enc ! fakesink
   */
  if (!(gst_msdk_context_get_job_type (thiz->context) & (GST_MSDK_JOB_VPP |
              GST_MSDK_JOB_ENCODER))) {
    gst_msdk_context_add_job_type (thiz->context, GST_MSDK_JOB_VPP);
    return TRUE;
  }

  /* Found an existing context that's already being used as VPP, so clone the
   * MFX session inside it to create a new one */
  {
    GstMsdkContext *parent_context, *msdk_context;

    GST_INFO_OBJECT (thiz, "Creating new context %" GST_PTR_FORMAT " with "
        "joined session", thiz->context);
    parent_context = thiz->context;
    msdk_context = gst_msdk_context_new_with_parent (parent_context);

    if (!msdk_context) {
      GST_ERROR_OBJECT (thiz, "Failed to create a context with parent context "
          "as %" GST_PTR_FORMAT, parent_context);
      return FALSE;
    }

    thiz->context = msdk_context;
    gst_object_unref (parent_context);
  }

  return TRUE;
}

static gboolean
ensure_context (GstBaseTransform * trans)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);

  if (!gst_msdkvpp_context_prepare (thiz)) {
    if (!gst_msdk_ensure_new_context (GST_ELEMENT_CAST (thiz),
            thiz->hardware, GST_MSDK_JOB_VPP, &thiz->context))
      return FALSE;
    GST_INFO_OBJECT (thiz, "Creating new context %" GST_PTR_FORMAT,
        thiz->context);
  }

  /* Save the current context in a separate field so that we know whether it
   * has changed between calls to _start() */
  gst_object_replace ((GstObject **) & thiz->old_context,
      (GstObject *) thiz->context);

  gst_msdk_context_add_shared_async_depth (thiz->context, thiz->async_depth);

  return TRUE;
}

static GstBuffer *
create_output_buffer (GstMsdkVPP * thiz)
{
  GstBuffer *outbuf;
  GstFlowReturn ret;
  GstBufferPool *pool = thiz->srcpad_buffer_pool;

  g_return_val_if_fail (pool != NULL, NULL);

  if (!gst_buffer_pool_is_active (pool) &&
      !gst_buffer_pool_set_active (pool, TRUE))
    goto error_activate_pool;

  outbuf = NULL;
  ret = gst_buffer_pool_acquire_buffer (pool, &outbuf, NULL);
  if (ret != GST_FLOW_OK || !outbuf)
    goto error_create_buffer;

  return outbuf;

  /* ERRORS */
error_activate_pool:
  {
    GST_ERROR_OBJECT (thiz, "failed to activate output video buffer pool");
    return NULL;
  }
error_create_buffer:
  {
    GST_ERROR_OBJECT (thiz, "failed to create output video buffer");
    return NULL;
  }
}

static GstFlowReturn
gst_msdkvpp_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf_ptr)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);

  if (gst_base_transform_is_passthrough (trans)) {
    *outbuf_ptr = inbuf;
    return GST_FLOW_OK;
  }

  *outbuf_ptr = create_output_buffer (thiz);
  return *outbuf_ptr ? GST_FLOW_OK : GST_FLOW_ERROR;
}

#ifndef _WIN32
static GstBufferPool *
gst_msdk_create_va_pool (GstMsdkVPP * thiz, GstVideoInfo * info,
    GstMsdkContext * msdk_context, guint min_buffers, GstPadDirection direction)
{
  GstBufferPool *pool = NULL;
  GstAllocator *allocator;
  GArray *formats = NULL;
  GstAllocationParams alloc_params = { 0, 31, 0, 0 };
  GstVaDisplay *display = NULL;
  GstCaps *aligned_caps = NULL;
  guint usage_hint = VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC;
  gboolean use_dmabuf = FALSE;
  guint64 modifier = DRM_FORMAT_MOD_INVALID;

  display = (GstVaDisplay *) gst_msdk_context_get_va_display (msdk_context);

  if (direction == GST_PAD_SINK) {
    use_dmabuf = thiz->use_sinkpad_dmabuf;
    modifier = thiz->sink_modifier;
  } else if (direction == GST_PAD_SRC) {
    use_dmabuf = thiz->use_srcpad_dmabuf;
    modifier = thiz->src_modifier;
  }

  if (use_dmabuf)
    allocator = gst_va_dmabuf_allocator_new (display);
  else {
    /* From attrib query, va surface format doesn't support RGB565, so leave
     * the formats as NULL when creating va allocator for RGB565 */
    if (GST_VIDEO_INFO_FORMAT (info) != GST_VIDEO_FORMAT_RGB16) {
      formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));
      g_array_append_val (formats, GST_VIDEO_INFO_FORMAT (info));
    }
    allocator = gst_va_allocator_new (display, formats);
  }

  gst_object_unref (display);

  if (!allocator) {
    GST_ERROR ("Failed to create allocator");
    if (formats)
      g_array_unref (formats);
    return NULL;
  }

  if (use_dmabuf && modifier != DRM_FORMAT_MOD_INVALID) {
    aligned_caps = gst_msdkcaps_video_info_to_drm_caps (info, modifier);
    usage_hint |= VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ |
        VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE;
    gst_caps_set_features (aligned_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL));
  } else
    aligned_caps = gst_video_info_to_caps (info);

  pool = gst_va_pool_new_with_config (aligned_caps, min_buffers, 0, usage_hint,
      GST_VA_FEATURE_AUTO, allocator, &alloc_params);

  gst_object_unref (allocator);
  gst_caps_unref (aligned_caps);

  return pool;
}
#else
static GstBufferPool *
gst_msdk_create_d3d11_pool (GstMsdkVPP * thiz, GstVideoInfo * info,
    guint num_buffers, gboolean propose)
{
  GstBufferPool *pool = NULL;
  GstD3D11Device *device;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  GstD3D11Format device_format;
  guint bind_flags = 0;
  GstCaps *aligned_caps = NULL;
  GstVideoInfo aligned_info;
  gint aligned_width;
  gint aligned_height;

  device = gst_msdk_context_get_d3d11_device (thiz->context);

  aligned_width = GST_ROUND_UP_16 (info->width);
  aligned_height = GST_ROUND_UP_32 (info->height);

  gst_video_info_set_interlaced_format (&aligned_info,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_INTERLACE_MODE (info),
      aligned_width, aligned_height);

  gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (&aligned_info),
      &device_format);
  if (!propose
      && ((device_format.format_support[0] & D3D11_FORMAT_SUPPORT_RENDER_TARGET)
          == D3D11_FORMAT_SUPPORT_RENDER_TARGET)) {
    bind_flags = D3D11_BIND_RENDER_TARGET;
  }

  aligned_caps = gst_video_info_to_caps (&aligned_info);

  pool = gst_d3d11_buffer_pool_new (device);
  config = gst_buffer_pool_get_config (pool);
  params = gst_d3d11_allocation_params_new (device, &aligned_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags,
      D3D11_RESOURCE_MISC_SHARED);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, aligned_caps,
      GST_VIDEO_INFO_SIZE (&aligned_info), num_buffers, 0);
  gst_buffer_pool_set_config (pool, config);

  gst_caps_unref (aligned_caps);
  GST_LOG_OBJECT (thiz, "Creating d3d11 pool");

  return pool;
}
#endif

static GstBufferPool *
gst_msdkvpp_create_buffer_pool (GstMsdkVPP * thiz, GstPadDirection direction,
    GstCaps * caps, guint min_num_buffers, gboolean propose)
{
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstVideoInfo info;
  GstVideoInfo *pool_info = NULL;
  GstVideoAlignment align;

  if (direction == GST_PAD_SINK) {
    pool_info = &thiz->sinkpad_buffer_pool_info;
  } else if (direction == GST_PAD_SRC) {
    pool_info = &thiz->srcpad_buffer_pool_info;
  }

  if (!gst_msdkcaps_video_info_from_caps (caps, &info, NULL)) {
    goto error_no_video_info;
  }

  gst_msdk_set_video_alignment (&info, 0, 0, &align);
  gst_video_info_align (&info, &align);

#ifndef _WIN32
  pool = gst_msdk_create_va_pool (thiz, &info, thiz->context, min_num_buffers,
      direction);
#else
  pool = gst_msdk_create_d3d11_pool (thiz, &info, min_num_buffers, propose);
#endif
  if (!thiz->use_video_memory)
    pool = gst_video_buffer_pool_new ();

  if (!pool)
    goto error_no_pool;

  config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));

  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (&info), min_num_buffers, 0);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_video_alignment (config, &align);

  if (!gst_buffer_pool_set_config (pool, config))
    goto error_pool_config;

  /* Updating pool_info with info which used to config pool */
  *pool_info = info;

  return pool;

error_no_pool:
  {
    GST_INFO_OBJECT (thiz, "Failed to create bufferpool");
    return NULL;
  }
error_no_video_info:
  {
    GST_INFO_OBJECT (thiz, "Failed to get Video info from caps");
    return NULL;
  }
error_pool_config:
  {
    GST_INFO_OBJECT (thiz, "Failed to set config");
    gst_object_unref (pool);
    return NULL;
  }
}

static GstBufferPool *
create_src_pool (GstMsdkVPP * thiz, GstQuery * query, GstCaps * caps)
{
  GstBufferPool *pool = NULL;
  guint size = 0, min_buffers = 0, max_buffers = 0;
  gboolean update_pool = FALSE;
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  mfxFrameAllocRequest request;

  /* Check whether the query has pool */
  if (gst_query_get_n_allocation_pools (query) > 0) {
    update_pool = TRUE;
    gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);
  }
  if (pool) {
    GstStructure *config = NULL;
    /* get the configured pool properties inorder to set in query */
    config = gst_buffer_pool_get_config (pool);
    gst_object_unref (pool);

    gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
        &max_buffers);
    if (gst_buffer_pool_config_get_allocator (config, &allocator, &params))
      gst_query_add_allocation_param (query, allocator, &params);
    gst_structure_free (config);
  } else {
    /* if we have tee after msdkvpp, we will not have pool for src pad,
       we need assign size for the internal pool
       gst-launch-1.0 -v videotestsrc  ! msdkvpp ! tee ! msdkh264enc ! fakesink silent=false
     */
    min_buffers = SRC_POOL_SIZE_DEFAULT;
  }

  /* Always create a pool for vpp out buffers. For vpp, we don't use
   * external mfxFrameAllocator for video-memory allocation. */
  request = thiz->request[1];
  min_buffers += thiz->async_depth + request.NumFrameSuggested;
  request.NumFrameSuggested = min_buffers;

  pool =
      gst_msdkvpp_create_buffer_pool (thiz, GST_PAD_SRC, caps, min_buffers,
      FALSE);
  if (!pool)
    return NULL;
  /* we do not support dynamic buffer count change */
  max_buffers = min_buffers;
  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min_buffers,
        max_buffers);
  else
    gst_query_add_allocation_pool (query, pool, size, min_buffers, max_buffers);

  return pool;
}

static gboolean
gst_msdkvpp_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  GstCaps *caps;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (thiz, "Failed to parse the decide_allocation caps");
    return FALSE;
  }

  /* We allocate the memory of type that downstream allocation requests */
#ifndef _WIN32
  if (gst_msdkcaps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    GST_INFO_OBJECT (thiz, "MSDK VPP srcpad uses DMABuf memory");
    thiz->use_srcpad_dmabuf = TRUE;
  }
#endif

  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL))
    thiz->add_video_meta = TRUE;
  else
    thiz->add_video_meta = FALSE;

  gst_clear_object (&thiz->srcpad_buffer_pool);
  thiz->srcpad_buffer_pool = create_src_pool (thiz, query, caps);
  if (!thiz->srcpad_buffer_pool)
    return FALSE;

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  /* Fixme if downstream doesn't have videometa support, msdkvpp should
   * copy the output buffers */

  return TRUE;
}

static gboolean
gst_msdkvpp_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstAllocator *allocator = NULL;
  GstCaps *caps;
  GstStructure *config;
  gboolean need_pool;
  GstAllocationParams params;
  guint size;
  guint min_buffers = thiz->async_depth + 1;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps) {
    GST_ERROR_OBJECT (thiz, "Failed to parse the allocation caps");
    return FALSE;
  }

  if (!gst_msdkcaps_video_info_from_caps (caps, &info, NULL)) {
    GST_ERROR_OBJECT (thiz, "Failed to get video info");
    return FALSE;
  }

  /* if upstream allocation query supports dmabuf-capsfeatures,
   * we do allocate dmabuf backed memory */
  if (gst_msdkcaps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    GST_INFO_OBJECT (thiz, "MSDK VPP srcpad uses DMABuf memory");
    thiz->use_sinkpad_dmabuf = TRUE;
  }

  if (need_pool) {
    /* alwys provide a new pool for upstream to help re-negotiation
     * more info here: https://bugzilla.gnome.org/show_bug.cgi?id=748344 */
    pool = gst_msdkvpp_create_buffer_pool (thiz, GST_PAD_SINK, caps,
        min_buffers, TRUE);
  }

  /* Update the internal pool if any allocation attribute changed */
  if (!gst_video_info_is_equal (&thiz->sinkpad_buffer_pool_info, &info)) {
    gst_object_unref (thiz->sinkpad_buffer_pool);
    thiz->sinkpad_buffer_pool = gst_msdkvpp_create_buffer_pool (thiz,
        GST_PAD_SINK, caps, min_buffers, FALSE);
  }

  /* get the size and allocator params from configured pool and set it in query */
  if (!need_pool)
    pool = gst_object_ref (thiz->sinkpad_buffer_pool);
  config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  if (gst_buffer_pool_config_get_allocator (config, &allocator, &params))
    gst_query_add_allocation_param (query, allocator, &params);
  gst_structure_free (config);

  /* if upstream doesn't have a pool requirement, set only
   *  size, min_buffers and max_buffers in query */
  gst_query_add_allocation_pool (query, need_pool ? pool : NULL, size,
      min_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
      decide_query, query);
}

static GstMsdkSurface *
gst_msdkvpp_get_surface_from_pool (GstMsdkVPP * thiz, GstBufferPool * pool,
    GstBuffer * buf)
{
  GstBuffer *upload_buf;
  GstMsdkSurface *msdk_surface = NULL;
  GstVideoFrame src_frame, dst_frame;

  if (!gst_buffer_pool_is_active (pool) &&
      !gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (pool, "failed to activate buffer pool");
    return NULL;
  }

  if (gst_buffer_pool_acquire_buffer (pool, &upload_buf, NULL) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (pool, "failed to acquire a buffer from pool");
    return NULL;
  }

  if (!gst_video_frame_map (&src_frame, &thiz->sinkpad_info, buf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (thiz, "failed to map the frame for source");
    gst_buffer_unref (upload_buf);
    return NULL;
  }

  if (!gst_video_frame_map (&dst_frame, &thiz->sinkpad_buffer_pool_info,
          upload_buf, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (thiz, "failed to map the frame for destination");
    gst_video_frame_unmap (&src_frame);
    gst_buffer_unref (upload_buf);
    return NULL;
  }

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&src_frame); i++) {
    guint src_width_in_bytes, src_height;
    guint dst_width_in_bytes, dst_height;
    guint width_in_bytes, height;
    guint src_stride, dst_stride;
    guint8 *src_data, *dst_data;

    src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&src_frame, i);

    dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    dst_stride = GST_VIDEO_FRAME_COMP_STRIDE (&dst_frame, i);

    width_in_bytes = MIN (src_width_in_bytes, dst_width_in_bytes);
    height = MIN (src_height, dst_height);

    src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);
    dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += dst_stride;
      src_data += src_stride;
    }
  }

  gst_video_frame_unmap (&dst_frame);
  gst_video_frame_unmap (&src_frame);

  if (thiz->use_video_memory) {
    msdk_surface = gst_msdk_import_to_msdk_surface (upload_buf, thiz->context,
        &thiz->sinkpad_info, GST_MAP_READ);
  } else {
    msdk_surface =
        gst_msdk_import_sys_mem_to_msdk_surface (upload_buf,
        &thiz->sinkpad_buffer_pool_info);
  }

  if (msdk_surface)
    msdk_surface->buf = upload_buf;

  return msdk_surface;
}

static GstMsdkSurface *
get_msdk_surface_from_input_buffer (GstMsdkVPP * thiz, GstBuffer * inbuf)
{
  GstMsdkSurface *msdk_surface = NULL;

  msdk_surface = gst_msdk_import_to_msdk_surface (inbuf, thiz->context,
      &thiz->sinkpad_info, GST_MAP_READ);
  if (msdk_surface) {
    msdk_surface->buf = gst_buffer_ref (inbuf);
    return msdk_surface;
  }

  /* If upstream hasn't accpeted the proposed msdk bufferpool,
   * just copy frame to msdk buffer and take a surface from it.
   */

  return gst_msdkvpp_get_surface_from_pool (thiz, thiz->sinkpad_buffer_pool,
      inbuf);
}

static GstFlowReturn
gst_msdkvpp_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  GstClockTime timestamp;
  GstFlowReturn ret = GST_FLOW_OK;
  mfxSession session;
  mfxSyncPoint sync_point = NULL;
  mfxStatus status;
  mfxFrameInfo *in_info = NULL;
  GstMsdkSurface *in_surface = NULL;
  GstMsdkSurface *out_surface = NULL;
  GstBuffer *outbuf_new = NULL;
  gboolean locked_by_others;
  gboolean create_new_surface = FALSE;

  free_unlocked_msdk_surfaces (thiz);

  in_surface = get_msdk_surface_from_input_buffer (thiz, inbuf);
  if (!in_surface)
    return GST_FLOW_ERROR;

  if (!in_surface->surface) {
    GST_ERROR_OBJECT (thiz, "mfx surface is NULL for the current input buffer");
    free_msdk_surface (in_surface);
    return GST_FLOW_ERROR;
  }
  locked_by_others = !!in_surface->surface->Data.Locked;

  /* always convert timestamp of input surface as msdk timestamp */
  if (inbuf->pts == GST_CLOCK_TIME_NONE)
    in_surface->surface->Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
  else
    in_surface->surface->Data.TimeStamp =
        gst_util_uint64_scale_round (inbuf->pts, 90000, GST_SECOND);

  if (thiz->use_video_memory) {
    out_surface = gst_msdk_import_to_msdk_surface (outbuf, thiz->context,
        &thiz->srcpad_info, GST_MAP_WRITE);
  } else {
    out_surface =
        gst_msdk_import_sys_mem_to_msdk_surface (outbuf, &thiz->srcpad_info);
  }

  if (out_surface) {
    out_surface->buf = gst_buffer_ref (outbuf);
  } else {
    GST_ERROR_OBJECT (thiz, "Failed to get msdk outsurface!");
    free_msdk_surface (in_surface);
    return GST_FLOW_ERROR;
  }

  /* update surface crop info (NOTE: msdk min frame size is 2x2) */
  in_info = &in_surface->surface->Info;
  if ((thiz->crop_left + thiz->crop_right >= in_info->CropW - 1)
      || (thiz->crop_top + thiz->crop_bottom >= in_info->CropH - 1)) {
    GST_WARNING_OBJECT (thiz, "ignoring crop... cropping too much!");
  } else if (!in_surface->from_qdata) {
    /* We only fill crop info when it is a new surface.
     * If the surface is a cached one, it already has crop info,
     * and we should avoid updating again.
     */
    in_info->CropX = thiz->crop_left;
    in_info->CropY = thiz->crop_top;
    in_info->CropW -= thiz->crop_left + thiz->crop_right;
    in_info->CropH -= thiz->crop_top + thiz->crop_bottom;
  }

  session = gst_msdk_context_get_session (thiz->context);

  /* outer loop is for handling FrameRate Control and deinterlace use cases */
  do {
    for (;;) {
      status =
          MFXVideoVPP_RunFrameVPPAsync (session, in_surface->surface,
          out_surface->surface, NULL, &sync_point);
      timestamp = out_surface->surface->Data.TimeStamp;

      if (status != MFX_WRN_DEVICE_BUSY)
        break;
      /* If device is busy, wait 1ms and retry, as per MSDK's recommendation */
      g_usleep (1000);
    }

    if (timestamp == MFX_TIMESTAMP_UNKNOWN)
      timestamp = GST_CLOCK_TIME_NONE;
    else
      timestamp = gst_util_uint64_scale_round (timestamp, GST_SECOND, 90000);

    if (status == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
      GST_WARNING_OBJECT (thiz, "VPP returned: %s",
          msdk_status_to_string (status));
    else if (status != MFX_ERR_NONE && status != MFX_ERR_MORE_DATA
        && status != MFX_ERR_MORE_SURFACE)
      goto vpp_error;

    /* No output generated */
    if (status == MFX_ERR_MORE_DATA)
      goto error_more_data;

    /* Wait for vpp operation to complete, the magic number 300000 below
     * is used in MSDK samples
     * #define MSDK_VPP_WAIT_INTERVAL 300000
     */
    if (sync_point &&
        MFXVideoCORE_SyncOperation (session, sync_point,
            300000) != MFX_ERR_NONE)
      GST_WARNING_OBJECT (thiz, "failed to do sync operation");
    /* push new output buffer forward after sync operation */
    if (create_new_surface) {
      create_new_surface = FALSE;
      ret = gst_pad_push (GST_BASE_TRANSFORM_SRC_PAD (trans), outbuf_new);
      if (ret != GST_FLOW_OK)
        goto error_push_buffer;
    }

    /* More than one output buffers are generated */
    if (status == MFX_ERR_MORE_SURFACE) {
      outbuf_new = create_output_buffer (thiz);
      GST_BUFFER_TIMESTAMP (outbuf_new) = timestamp;
      GST_BUFFER_DURATION (outbuf_new) = thiz->buffer_duration;

      release_out_surface (thiz, out_surface);
      if (thiz->use_video_memory) {
        out_surface =
            gst_msdk_import_to_msdk_surface (outbuf_new, thiz->context,
            &thiz->srcpad_buffer_pool_info, GST_MAP_WRITE);
      } else {
        out_surface =
            gst_msdk_import_sys_mem_to_msdk_surface (outbuf_new,
            &thiz->srcpad_buffer_pool_info);
      }

      if (out_surface) {
        out_surface->buf = gst_buffer_ref (outbuf_new);
        create_new_surface = TRUE;
      } else {
        GST_ERROR_OBJECT (thiz, "Failed to get msdk outsurface!");
        release_in_surface (thiz, in_surface, locked_by_others);
        return GST_FLOW_ERROR;
      }
    } else {
      GST_BUFFER_TIMESTAMP (outbuf) = timestamp;
      GST_BUFFER_DURATION (outbuf) = thiz->buffer_duration;
    }
  } while (status == MFX_ERR_MORE_SURFACE);

  goto transform_end;

vpp_error:
  GST_ERROR_OBJECT (thiz, "MSDK Failed to do VPP");
  ret = GST_FLOW_ERROR;
  goto transform_end;

error_more_data:
  GST_WARNING_OBJECT (thiz,
      "MSDK Requires additional input for processing, "
      "Retruning FLOW_DROPPED since no output buffer was generated");
  ret = GST_BASE_TRANSFORM_FLOW_DROPPED;
  goto transform_end;

error_push_buffer:
  GST_DEBUG_OBJECT (thiz, "failed to push output buffer: %s",
      gst_flow_get_name (ret));

transform_end:
  release_in_surface (thiz, in_surface, locked_by_others);
  release_out_surface (thiz, out_surface);

  return ret;
}

static void
gst_msdkvpp_close (GstMsdkVPP * thiz)
{
  mfxStatus status;

  if (!thiz->context)
    return;

  GST_DEBUG_OBJECT (thiz, "Closing VPP 0x%p", thiz->context);
  status = MFXVideoVPP_Close (gst_msdk_context_get_session (thiz->context));
  if (status != MFX_ERR_NONE && status != MFX_ERR_NOT_INITIALIZED) {
    GST_WARNING_OBJECT (thiz, "VPP close failed (%s)",
        msdk_status_to_string (status));
  }
  free_all_msdk_surfaces (thiz);

  gst_clear_object (&thiz->context);

  memset (&thiz->param, 0, sizeof (thiz->param));

  gst_clear_object (&thiz->sinkpad_buffer_pool);
  gst_clear_object (&thiz->srcpad_buffer_pool);

  thiz->buffer_duration = GST_CLOCK_TIME_NONE;
  gst_video_info_init (&thiz->sinkpad_info);
  gst_video_info_init (&thiz->srcpad_info);
}

static void
ensure_filters (GstMsdkVPP * thiz)
{
  /* Denoise */
  if (thiz->flags & GST_MSDK_FLAG_DENOISE) {
    mfxExtVPPDenoise *mfx_denoise = &thiz->mfx_denoise;
    mfx_denoise->Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
    mfx_denoise->Header.BufferSz = sizeof (mfxExtVPPDenoise);
    mfx_denoise->DenoiseFactor = thiz->denoise_factor;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_denoise);
  }

  /* Rotation */
  if (thiz->rotation != MFX_ANGLE_0) {
    mfxExtVPPRotation *mfx_rotation = &thiz->mfx_rotation;
    mfx_rotation->Header.BufferId = MFX_EXTBUFF_VPP_ROTATION;
    mfx_rotation->Header.BufferSz = sizeof (mfxExtVPPRotation);
    mfx_rotation->Angle = thiz->rotation;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_rotation);
  }

  /* Deinterlace */
  if (thiz->flags & GST_MSDK_FLAG_DEINTERLACE) {
    mfxExtVPPDeinterlacing *mfx_deinterlace = &thiz->mfx_deinterlace;
    mfx_deinterlace->Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
    mfx_deinterlace->Header.BufferSz = sizeof (mfxExtVPPDeinterlacing);
    mfx_deinterlace->Mode = thiz->deinterlace_method;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_deinterlace);
  }

  /* Colorbalance(ProcAmp) */
  if (thiz->flags & (GST_MSDK_FLAG_HUE | GST_MSDK_FLAG_SATURATION |
          GST_MSDK_FLAG_BRIGHTNESS | GST_MSDK_FLAG_CONTRAST)) {
    mfxExtVPPProcAmp *mfx_procamp = &thiz->mfx_procamp;
    mfx_procamp->Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
    mfx_procamp->Header.BufferSz = sizeof (mfxExtVPPProcAmp);
    mfx_procamp->Hue = thiz->hue;
    mfx_procamp->Saturation = thiz->saturation;
    mfx_procamp->Brightness = thiz->brightness;
    mfx_procamp->Contrast = thiz->contrast;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_procamp);
  }

  /* Detail/Edge enhancement */
  if (thiz->flags & GST_MSDK_FLAG_DETAIL) {
    mfxExtVPPDetail *mfx_detail = &thiz->mfx_detail;
    mfx_detail->Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
    mfx_detail->Header.BufferSz = sizeof (mfxExtVPPDetail);
    mfx_detail->DetailFactor = thiz->detail;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_detail);
  }

  /* Mirroring */
  if (thiz->mirroring != MFX_MIRRORING_DISABLED) {
    mfxExtVPPMirroring *mfx_mirroring = &thiz->mfx_mirroring;
    mfx_mirroring->Header.BufferId = MFX_EXTBUFF_VPP_MIRRORING;
    mfx_mirroring->Header.BufferSz = sizeof (mfxExtVPPMirroring);
    mfx_mirroring->Type = thiz->mirroring;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_mirroring);
  }

  /* Scaling Mode */
  if (thiz->flags & GST_MSDK_FLAG_SCALING_MODE) {
    gboolean scaling_mode_is_compute = FALSE;
#if (MFX_VERSION >= 2007)
    if (thiz->scaling_mode == MFX_SCALING_MODE_INTEL_GEN_COMPUTE)
      scaling_mode_is_compute = TRUE;
#endif
    if (MFX_RUNTIME_VERSION_ATLEAST (thiz->version, 2, 7) ||
        !scaling_mode_is_compute) {
      mfxExtVPPScaling *mfx_scaling = &thiz->mfx_scaling;
      mfx_scaling->Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
      mfx_scaling->Header.BufferSz = sizeof (mfxExtVPPScaling);
      mfx_scaling->ScalingMode = thiz->scaling_mode;
      gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_scaling);
    } else {
      GST_WARNING_OBJECT (thiz,
          "Compute scaling mode not supported, ignore it...");
    }
  }

  /* FRC */
  if (thiz->flags & GST_MSDK_FLAG_FRC) {
    mfxExtVPPFrameRateConversion *mfx_frc = &thiz->mfx_frc;
    mfx_frc->Header.BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
    mfx_frc->Header.BufferSz = sizeof (mfxExtVPPFrameRateConversion);
    mfx_frc->Algorithm = thiz->frc_algm;
    gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mfx_frc);
  }

  /* Color properties */
#if (MFX_VERSION >= 2000)
  if (MFX_RUNTIME_VERSION_ATLEAST (thiz->version, 2, 0)) {
    GstVideoInfo *in_vinfo = &thiz->sinkpad_info;
    GstVideoInfo *out_vinfo = &thiz->srcpad_info;
    mfxExtVideoSignalInfo *in_vsi = &thiz->in_vsi;
    mfxExtVideoSignalInfo *out_vsi = &thiz->out_vsi;
    mfxExtMasteringDisplayColourVolume *mdcv = &thiz->mdcv;
    mfxExtContentLightLevelInfo *cll = &thiz->cll;
    const guint chroma_den = 50000;
    const guint luma_den = 10000;
    gint tmap = 0;

    if (in_vinfo->colorimetry.primaries || in_vinfo->colorimetry.transfer
        || in_vinfo->colorimetry.matrix || in_vinfo->colorimetry.range) {
      in_vsi->Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO_IN;
      in_vsi->Header.BufferSz = sizeof (in_vsi);
      in_vsi->ColourDescriptionPresent = 1;
      in_vsi->VideoFullRange =
          (in_vinfo->colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255);
      in_vsi->ColourPrimaries =
          gst_video_color_primaries_to_iso (in_vinfo->colorimetry.primaries);
      in_vsi->TransferCharacteristics =
          gst_video_transfer_function_to_iso (in_vinfo->colorimetry.transfer);
      in_vsi->MatrixCoefficients =
          gst_video_color_matrix_to_iso (in_vinfo->colorimetry.matrix);
      gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) in_vsi);
    }

    if (thiz->hdr_tone_mapping) {
      if (thiz->have_mdcv) {
        mdcv->Header.BufferId = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME_IN;
        mdcv->Header.BufferSz = sizeof (mfxExtMasteringDisplayColourVolume);

        mdcv->DisplayPrimariesX[0] =
            MIN ((thiz->mdcv_info.display_primaries[1].x * chroma_den),
            chroma_den);
        mdcv->DisplayPrimariesY[0] =
            MIN ((thiz->mdcv_info.display_primaries[1].y * chroma_den),
            chroma_den);
        mdcv->DisplayPrimariesX[1] =
            MIN ((thiz->mdcv_info.display_primaries[2].x * chroma_den),
            chroma_den);
        mdcv->DisplayPrimariesY[1] =
            MIN ((thiz->mdcv_info.display_primaries[2].y * chroma_den),
            chroma_den);
        mdcv->DisplayPrimariesX[2] =
            MIN ((thiz->mdcv_info.display_primaries[0].x * chroma_den),
            chroma_den);
        mdcv->DisplayPrimariesY[2] =
            MIN ((thiz->mdcv_info.display_primaries[0].y * chroma_den),
            chroma_den);

        mdcv->WhitePointX =
            MIN ((thiz->mdcv_info.white_point.x * chroma_den), chroma_den);
        mdcv->WhitePointY =
            MIN ((thiz->mdcv_info.white_point.y * chroma_den), chroma_den);

        /* From vpl spec, MaxDisplayMasteringLuminance is in the unit of 1 nits,
         * MinDisplayMasteringLuminance is in the unit of 0.0001 nits.
         */
        mdcv->MaxDisplayMasteringLuminance =
            thiz->mdcv_info.max_display_mastering_luminance;
        mdcv->MinDisplayMasteringLuminance =
            thiz->mdcv_info.min_display_mastering_luminance * luma_den;

        gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) mdcv);
        tmap = 1;
      }

      if (thiz->have_cll) {
        cll->Header.BufferId = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
        cll->Header.BufferSz = sizeof (mfxExtContentLightLevelInfo);

        cll->MaxContentLightLevel =
            MIN (thiz->cll_info.max_content_light_level, 65535);
        cll->MaxPicAverageLightLevel =
            MIN (thiz->cll_info.max_frame_average_light_level, 65535);

        gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) cll);
        tmap = 1;
      }
    }

    if (tmap) {
      out_vinfo->colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      out_vinfo->colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;
      out_vinfo->colorimetry.range = GST_VIDEO_COLOR_RANGE_16_235;
      out_vinfo->colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }

    if (out_vinfo->colorimetry.primaries || out_vinfo->colorimetry.transfer
        || out_vinfo->colorimetry.matrix || out_vinfo->colorimetry.range) {
      out_vsi->Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO_OUT;
      out_vsi->Header.BufferSz = sizeof (out_vsi);
      out_vsi->ColourDescriptionPresent = 1;
      out_vsi->VideoFullRange =
          (out_vinfo->colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255);
      out_vsi->ColourPrimaries =
          gst_video_color_primaries_to_iso (out_vinfo->colorimetry.primaries);
      out_vsi->TransferCharacteristics =
          gst_video_transfer_function_to_iso (out_vinfo->colorimetry.transfer);
      out_vsi->MatrixCoefficients =
          gst_video_color_matrix_to_iso (out_vinfo->colorimetry.matrix);
      gst_msdkvpp_add_extra_param (thiz, (mfxExtBuffer *) out_vsi);
    }
  }
#endif
}

static void
gst_msdkvpp_set_passthrough (GstMsdkVPP * thiz)
{
  gboolean passthrough = TRUE;

  /* no passthrough if any of the filter algorithm is enabled */
  if (thiz->flags)
    passthrough = FALSE;

  /* vpp could be needed in some specific circumstances, for eg:
   * input surface is dmabuf and output must be videomemory. So far
   * the underline iHD driver doesn't seems to support dmabuf mapping,
   * so we could explicitly ask msdkvpp to provide non-dambuf videomemory
   * surfaces as output thourgh capsfileters */
  if (thiz->need_vpp)
    passthrough = FALSE;

  /* no passthrough if there is change in out width,height or format */
  if (GST_VIDEO_INFO_WIDTH (&thiz->sinkpad_info) !=
      GST_VIDEO_INFO_WIDTH (&thiz->srcpad_info)
      || GST_VIDEO_INFO_HEIGHT (&thiz->sinkpad_info) !=
      GST_VIDEO_INFO_HEIGHT (&thiz->srcpad_info)
      || GST_VIDEO_INFO_FORMAT (&thiz->sinkpad_info) !=
      GST_VIDEO_INFO_FORMAT (&thiz->srcpad_info))
    passthrough = FALSE;

  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (thiz), passthrough);
}

static gboolean
gst_msdkvpp_initialize (GstMsdkVPP * thiz)
{
  mfxSession session;
  mfxStatus status;
  mfxFrameAllocRequest *request = &thiz->request[0];

  if (!thiz->context) {
    GST_WARNING_OBJECT (thiz, "No MSDK Context");
    return FALSE;
  }

  GST_OBJECT_LOCK (thiz);
  session = gst_msdk_context_get_session (thiz->context);
  status = MFXQueryVersion (session, &thiz->version);
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "VPP failed to query version");
    goto no_vpp;
  }

  /* Close the current session if the session has been initialized,
   * otherwise the subsequent function call of MFXVideoVPP_Init() will
   * fail
   */
  if (thiz->initialized) {
    MFXVideoVPP_Close (session);

    memset (&thiz->param, 0, sizeof (thiz->param));
    memset (&thiz->extra_params, 0, sizeof (thiz->extra_params));
    thiz->num_extra_params = 0;
  }

  if (thiz->use_video_memory) {
    gst_msdk_set_frame_allocator (thiz->context);
    thiz->param.IOPattern =
        MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  } else {
    thiz->param.IOPattern =
        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  /* update input video attributes */
  gst_msdk_set_mfx_frame_info_from_video_info (&thiz->param.vpp.In,
      &thiz->sinkpad_info);

  /* update output video attributes, only CSC and Scaling are supported for now */
  gst_msdk_set_mfx_frame_info_from_video_info (&thiz->param.vpp.Out,
      &thiz->srcpad_info);

  /* use msdk frame rarte control if there is a mismatch in In & OUt fps  */
  if (GST_VIDEO_INFO_FPS_N (&thiz->srcpad_info) &&
      (GST_VIDEO_INFO_FPS_N (&thiz->sinkpad_info) !=
          GST_VIDEO_INFO_FPS_N (&thiz->srcpad_info)
          || GST_VIDEO_INFO_FPS_D (&thiz->sinkpad_info) !=
          GST_VIDEO_INFO_FPS_D (&thiz->srcpad_info))) {
    thiz->flags |= GST_MSDK_FLAG_FRC;
    /* manually set distributed timestamp as frc algorithm
     * as it is more resonable for framerate conversion
     */
    thiz->frc_algm = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;
  }

  /* work-around to avoid zero fps in msdk structure */
  if (!thiz->param.vpp.In.FrameRateExtN)
    thiz->param.vpp.In.FrameRateExtN = 30;
  if (!thiz->param.vpp.Out.FrameRateExtN)
    thiz->param.vpp.Out.FrameRateExtN = thiz->param.vpp.In.FrameRateExtN;

  /* set vpp out picstruct as progressive if deinterlacing enabled */
  if (thiz->flags & GST_MSDK_FLAG_DEINTERLACE)
    thiz->param.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  /* Enable the required filters */
  ensure_filters (thiz);

  /* Add extended buffers */
  if (thiz->num_extra_params) {
    thiz->param.NumExtParam = thiz->num_extra_params;
    thiz->param.ExtParam = thiz->extra_params;
  }

  /* validate parameters and allow MFX to make adjustments */
  status = MFXVideoVPP_Query (session, &thiz->param, &thiz->param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Video VPP Query failed (%s)",
        msdk_status_to_string (status));
    goto no_vpp;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Video VPP Query returned: %s",
        msdk_status_to_string (status));
  }

  status = MFXVideoVPP_QueryIOSurf (session, &thiz->param, request);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "VPP Query IO surfaces failed (%s)",
        msdk_status_to_string (status));
    goto no_vpp;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "VPP Query IO surfaces returned: %s",
        msdk_status_to_string (status));
  }

  if (thiz->use_video_memory) {
    /* Input surface pool pre-allocation */
    request[0].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    if (thiz->use_sinkpad_dmabuf)
      request[0].Type |= MFX_MEMTYPE_EXPORT_FRAME;

    /* Output surface pool pre-allocation */
    request[1].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    if (thiz->use_srcpad_dmabuf)
      request[1].Type |= MFX_MEMTYPE_EXPORT_FRAME;
  }

  thiz->in_num_surfaces = request[0].NumFrameSuggested;

  status = MFXVideoVPP_Init (session, &thiz->param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Init failed (%s)", msdk_status_to_string (status));
    goto no_vpp;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Init returned: %s",
        msdk_status_to_string (status));
  }

  thiz->initialized = TRUE;
  GST_OBJECT_UNLOCK (thiz);
  return TRUE;

no_vpp:
  GST_OBJECT_UNLOCK (thiz);
  gst_clear_object (&thiz->context);
  return FALSE;
}

static gboolean
gst_msdkvpp_set_caps (GstBaseTransform * trans, GstCaps * caps,
    GstCaps * out_caps)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  GstVideoInfo in_info, out_info;
  gboolean sinkpad_info_changed = FALSE;
  gboolean srcpad_info_changed = FALSE;
  gboolean deinterlace;

  if (!gst_caps_features_is_equal (gst_caps_get_features (caps, 0),
          gst_caps_get_features (out_caps, 0)))
    thiz->need_vpp = 1;

  thiz->use_sinkpad_dmabuf = gst_msdkcaps_has_feature (caps,
      GST_CAPS_FEATURE_MEMORY_DMABUF) ? TRUE : FALSE;
  thiz->use_srcpad_dmabuf = gst_msdkcaps_has_feature (out_caps,
      GST_CAPS_FEATURE_MEMORY_DMABUF) ? TRUE : FALSE;

  if (!gst_msdkcaps_video_info_from_caps (caps, &in_info, &thiz->sink_modifier))
    goto error_no_video_info;
  if (!gst_msdkcaps_video_info_from_caps (out_caps,
          &out_info, &thiz->src_modifier))
    goto error_no_video_info;

  if (!gst_video_info_is_equal (&in_info, &thiz->sinkpad_info))
    sinkpad_info_changed = TRUE;
  if (!gst_video_info_is_equal (&out_info, &thiz->srcpad_info))
    srcpad_info_changed = TRUE;

  if (!sinkpad_info_changed && !srcpad_info_changed && thiz->initialized)
    return TRUE;

  thiz->sinkpad_info = in_info;
  thiz->srcpad_info = out_info;

  thiz->use_video_memory = TRUE;

  /* check for deinterlace requirement */
  deinterlace = gst_msdkvpp_is_deinterlace_enabled (thiz, &in_info);
  if (deinterlace)
    thiz->flags |= GST_MSDK_FLAG_DEINTERLACE;

  thiz->buffer_duration = GST_VIDEO_INFO_FPS_N (&out_info) > 0 ?
      gst_util_uint64_scale (GST_SECOND, GST_VIDEO_INFO_FPS_D (&out_info),
      GST_VIDEO_INFO_FPS_N (&out_info)) : 0;

  thiz->have_mdcv = thiz->have_cll = FALSE;
  if (gst_video_mastering_display_info_from_caps (&thiz->mdcv_info, caps))
    thiz->have_mdcv = TRUE;

  if (gst_video_content_light_level_from_caps (&thiz->cll_info, caps))
    thiz->have_cll = TRUE;

  if (!gst_msdkvpp_initialize (thiz))
    return FALSE;

  if (!thiz->hdr_tone_mapping) {
    if (thiz->have_mdcv) {
      if (!gst_video_mastering_display_info_add_to_caps (&thiz->mdcv_info,
              out_caps))
        GST_WARNING ("Failed to add mastering display info to caps");
    }

    if (thiz->have_cll) {
      if (!gst_video_content_light_level_add_to_caps (&thiz->cll_info,
              out_caps))
        GST_WARNING ("Failed to add content light level to caps");
    }
  }

  /* set passthrough according to filter operation change */
  gst_msdkvpp_set_passthrough (thiz);

  /* Ensure sinkpad buffer pool */
  if (thiz->sinkpad_buffer_pool)
    gst_object_unref (thiz->sinkpad_buffer_pool);

  thiz->sinkpad_buffer_pool =
      gst_msdkvpp_create_buffer_pool (thiz, GST_PAD_SINK, caps,
      thiz->in_num_surfaces, FALSE);
  if (!thiz->sinkpad_buffer_pool) {
    GST_ERROR_OBJECT (thiz, "Failed to ensure the sinkpad buffer pool");
    return FALSE;
  }

  return TRUE;

error_no_video_info:
  GST_ERROR_OBJECT (thiz, "Failed to get video info from caps");
  return FALSE;
}

static GstCaps *
gst_msdkvpp_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  GstCaps *result = NULL;

  if (direction == GST_PAD_SRC) {
    result = gst_caps_fixate (othercaps);
  } else {
    /*
     * Override mirroring & rotation properties once video-direction
     * is set explicitly
     */
    if (thiz->flags & GST_MSDK_FLAG_VIDEO_DIRECTION)
      gst_msdk_get_mfx_video_orientation_from_video_direction
          (thiz->video_direction, &thiz->mirroring, &thiz->rotation);

    result = gst_msdkvpp_fixate_srccaps (thiz, caps, othercaps);
  }

  GST_DEBUG_OBJECT (trans, "fixated to %" GST_PTR_FORMAT, result);
  gst_caps_unref (othercaps);

  return result;
}

/* Generic code for now, requires changes in future when we
 * add hardware query for supported formats, Framerate control etc */
static GstCaps *
gst_msdkvpp_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *out_caps = NULL;
  GstCaps *tmp_caps;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp_caps =
        gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SRC_PAD (trans));
  } else {
    tmp_caps =
        gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SINK_PAD (trans));
  }

  if (!out_caps)
    out_caps = tmp_caps;

  if (out_caps && filter) {
    GstCaps *intersection;

    intersection = gst_caps_intersect_full (out_caps, filter,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (out_caps);
    out_caps = intersection;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, out_caps);
  return out_caps;
}

static gboolean
gst_msdkvpp_start (GstBaseTransform * trans)
{
  if (!ensure_context (trans))
    return FALSE;
  return TRUE;
}

static gboolean
gst_msdkvpp_stop (GstBaseTransform * trans)
{
  gst_msdkvpp_close (GST_MSDKVPP (trans));
  return TRUE;
}

static gboolean
gst_msdkvpp_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (trans);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      GstMsdkContext *msdk_context = NULL;

      gst_object_replace ((GstObject **) & msdk_context,
          (GstObject *) thiz->context);
      ret = gst_msdk_handle_context_query (GST_ELEMENT_CAST (trans),
          query, msdk_context);
      gst_clear_object (&msdk_context);
      break;
    }
    default:
      ret = GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans,
          direction, query);
      break;
  }

  return ret;
}

static void
gst_msdkvpp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (object);

  switch (prop_id) {
    case PROP_HARDWARE:
      thiz->hardware = g_value_get_boolean (value);
      break;
    case PROP_ASYNC_DEPTH:
      thiz->async_depth = g_value_get_uint (value);
      break;
    case PROP_DENOISE:
      thiz->denoise_factor = g_value_get_uint (value);
      thiz->flags |= GST_MSDK_FLAG_DENOISE;
      break;
#ifndef GST_REMOVE_DEPRECATED
    case PROP_ROTATION:
      thiz->rotation = g_value_get_enum (value);
      thiz->flags |= GST_MSDK_FLAG_ROTATION;
      break;
    case PROP_MIRRORING:
      thiz->mirroring = g_value_get_enum (value);
      thiz->flags |= GST_MSDK_FLAG_MIRRORING;
      break;
#endif
    case PROP_DEINTERLACE_MODE:
      thiz->deinterlace_mode = g_value_get_enum (value);
      break;
    case PROP_DEINTERLACE_METHOD:
      thiz->deinterlace_method = g_value_get_enum (value);
      break;
    case PROP_HUE:
      thiz->hue = g_value_get_float (value);
      thiz->flags |= GST_MSDK_FLAG_HUE;
      break;
    case PROP_SATURATION:
      thiz->saturation = g_value_get_float (value);
      thiz->flags |= GST_MSDK_FLAG_SATURATION;
      break;
    case PROP_BRIGHTNESS:
      thiz->brightness = g_value_get_float (value);
      thiz->flags |= GST_MSDK_FLAG_BRIGHTNESS;
      break;
    case PROP_CONTRAST:
      thiz->contrast = g_value_get_float (value);
      thiz->flags |= GST_MSDK_FLAG_CONTRAST;
      break;
    case PROP_DETAIL:
      thiz->detail = g_value_get_uint (value);
      thiz->flags |= GST_MSDK_FLAG_DETAIL;
      break;
    case PROP_SCALING_MODE:
      thiz->scaling_mode = g_value_get_enum (value);
      thiz->flags |= GST_MSDK_FLAG_SCALING_MODE;
      break;
    case PROP_FORCE_ASPECT_RATIO:
      thiz->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_FRC_ALGORITHM:
      thiz->frc_algm = g_value_get_enum (value);
      break;
    case PROP_VIDEO_DIRECTION:
      thiz->video_direction = g_value_get_enum (value);
      thiz->flags |= GST_MSDK_FLAG_VIDEO_DIRECTION;
      break;
    case PROP_CROP_LEFT:
      thiz->crop_left = g_value_get_uint (value);
      break;
    case PROP_CROP_RIGHT:
      thiz->crop_right = g_value_get_uint (value);
      break;
    case PROP_CROP_TOP:
      thiz->crop_top = g_value_get_uint (value);
      break;
    case PROP_CROP_BOTTOM:
      thiz->crop_bottom = g_value_get_uint (value);
      break;
    case PROP_HDR_TONE_MAPPING:
      thiz->hdr_tone_mapping = g_value_get_boolean (value);
      thiz->flags |= (thiz->hdr_tone_mapping ? GST_MSDK_FLAG_TONE_MAPPING : 0);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_msdkvpp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (object);

  switch (prop_id) {
    case PROP_HARDWARE:
      g_value_set_boolean (value, thiz->hardware);
      break;
    case PROP_ASYNC_DEPTH:
      g_value_set_uint (value, thiz->async_depth);
      break;
    case PROP_DENOISE:
      g_value_set_uint (value, thiz->denoise_factor);
      break;
#ifndef GST_REMOVE_DEPRECATED
    case PROP_ROTATION:
      g_value_set_enum (value, thiz->rotation);
      break;
    case PROP_MIRRORING:
      g_value_set_enum (value, thiz->mirroring);
      break;
#endif
    case PROP_DEINTERLACE_MODE:
      g_value_set_enum (value, thiz->deinterlace_mode);
      break;
    case PROP_DEINTERLACE_METHOD:
      g_value_set_enum (value, thiz->deinterlace_method);
      break;
    case PROP_HUE:
      g_value_set_float (value, thiz->hue);
      break;
    case PROP_SATURATION:
      g_value_set_float (value, thiz->saturation);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_float (value, thiz->brightness);
      break;
    case PROP_CONTRAST:
      g_value_set_float (value, thiz->contrast);
      break;
    case PROP_DETAIL:
      g_value_set_uint (value, thiz->detail);
      break;
    case PROP_SCALING_MODE:
      g_value_set_enum (value, thiz->scaling_mode);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, thiz->keep_aspect);
      break;
    case PROP_FRC_ALGORITHM:
      g_value_set_enum (value, thiz->frc_algm);
      break;
    case PROP_VIDEO_DIRECTION:
      g_value_set_enum (value, thiz->video_direction);
      break;
    case PROP_CROP_LEFT:
      g_value_set_uint (value, thiz->crop_left);
      break;
    case PROP_CROP_RIGHT:
      g_value_set_uint (value, thiz->crop_right);
      break;
    case PROP_CROP_TOP:
      g_value_set_uint (value, thiz->crop_top);
      break;
    case PROP_CROP_BOTTOM:
      g_value_set_uint (value, thiz->crop_bottom);
      break;
    case PROP_HDR_TONE_MAPPING:
      g_value_set_boolean (value, thiz->hdr_tone_mapping);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_msdkvpp_dispose (GObject * object)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (object);

  gst_clear_object (&thiz->old_context);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_msdkvpp_set_context (GstElement * element, GstContext * context)
{
  GstMsdkContext *msdk_context = NULL;
  GstMsdkVPP *thiz = GST_MSDKVPP (element);

  if (gst_msdk_context_get_context (context, &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  } else
#ifndef _WIN32
    if (gst_msdk_context_from_external_va_display (context,
          thiz->hardware, 0 /* GST_MSDK_JOB_VPP will be set later */ ,
          &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  }
#else
    if (gst_msdk_context_from_external_d3d11_device (context,
          thiz->hardware, 0 /* GST_MSDK_JOB_VPP will be set later */ ,
          &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  }
#endif

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static void
_msdkvpp_install_properties (GObjectClass * gobject_class)
{
  GParamSpec *obj_properties[PROP_N] = { NULL, };

  obj_properties[PROP_HARDWARE] =
      g_param_spec_boolean ("hardware", "Hardware", "Enable hardware VPP",
      PROP_HARDWARE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_ASYNC_DEPTH] =
      g_param_spec_uint ("async-depth", "Async Depth",
      "Depth of asynchronous pipeline",
      1, 1, PROP_ASYNC_DEPTH_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_DENOISE] =
      g_param_spec_uint ("denoise", "Denoising factor",
      "Denoising Factor",
      0, 100, PROP_DENOISE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

#ifndef GST_REMOVE_DEPRECATED
  obj_properties[PROP_ROTATION] =
      g_param_spec_enum ("rotation", "Rotation",
      "Rotation Angle (DEPRECATED, use video-direction instead)",
      gst_msdkvpp_rotation_get_type (), PROP_ROTATION_DEFAULT,
      G_PARAM_DEPRECATED | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_MIRRORING] =
      g_param_spec_enum ("mirroring", "Mirroring",
      "The Mirroring type (DEPRECATED, use video-direction instead)",
      gst_msdkvpp_mirroring_get_type (), PROP_MIRRORING_DEFAULT,
      G_PARAM_DEPRECATED | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

#endif

  obj_properties[PROP_DEINTERLACE_MODE] =
      g_param_spec_enum ("deinterlace-mode", "Deinterlace Mode",
      "Deinterlace mode to use", gst_msdkvpp_deinterlace_mode_get_type (),
      PROP_DEINTERLACE_MODE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_DEINTERLACE_METHOD] =
      g_param_spec_enum ("deinterlace-method", "Deinterlace Method",
      "Deinterlace method to use", gst_msdkvpp_deinterlace_method_get_type (),
      PROP_DEINTERLACE_METHOD_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_HUE] =
      g_param_spec_float ("hue", "Hue",
      "The hue of the video",
      -180, 180, PROP_HUE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SATURATION] =
      g_param_spec_float ("saturation", "Saturation",
      "The Saturation of the video",
      0, 10, PROP_SATURATION_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BRIGHTNESS] =
      g_param_spec_float ("brightness", "Brightness",
      "The Brightness of the video",
      -100, 100, PROP_BRIGHTNESS_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CONTRAST] =
      g_param_spec_float ("contrast", "Contrast",
      "The Contrast of the video",
      0, 10, PROP_CONTRAST_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_DETAIL] =
      g_param_spec_uint ("detail", "Detail",
      "The factor of detail/edge enhancement filter algorithm",
      0, 100, PROP_DETAIL_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SCALING_MODE] =
      g_param_spec_enum ("scaling-mode", "Scaling Mode",
      "The Scaling mode to use", gst_msdkvpp_scaling_mode_get_type (),
      PROP_SCALING_MODE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_FORCE_ASPECT_RATIO] =
      g_param_spec_boolean ("force-aspect-ratio", "Force Aspect Ratio",
      "When enabled, scaling will respect original aspect ratio",
      PROP_FORCE_ASPECT_RATIO_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_FRC_ALGORITHM] =
      g_param_spec_enum ("frc-algorithm", "FrameRateControl Algorithm",
      "The Framerate Control Alogorithm to use",
      gst_msdkvpp_frc_algorithm_get_type (), PROP_FRC_ALGORITHM_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /*
   * The video-direction to use, expressed as an enum value. See
   * #GstVideoOrientationMethod.
   */
  obj_properties[PROP_VIDEO_DIRECTION] = g_param_spec_enum ("video-direction",
      "Video Direction", "Video direction: rotation and flipping"
#ifndef GST_REMOVE_DEPRECATED
      ", it will override both mirroring & rotation properties if set explicitly"
#endif
      ,
      GST_TYPE_VIDEO_ORIENTATION_METHOD,
      PROP_VIDEO_DIRECTION_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CROP_LEFT] = g_param_spec_uint ("crop-left",
      "Crop Left", "Pixels to crop at left",
      0, G_MAXUINT16, PROP_CROP_LEFT_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CROP_RIGHT] = g_param_spec_uint ("crop-right",
      "Crop Right", "Pixels to crop at right",
      0, G_MAXUINT16, PROP_CROP_RIGHT_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CROP_TOP] = g_param_spec_uint ("crop-top",
      "Crop Top", "Pixels to crop at top",
      0, G_MAXUINT16, PROP_CROP_TOP_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CROP_BOTTOM] = g_param_spec_uint ("crop-bottom",
      "Crop Bottom", "Pixels to crop at bottom",
      0, G_MAXUINT16, PROP_CROP_BOTTOM_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  /**
   * GstMsdkVPP:hdr-tone-mapping:
   *
   * Since: 1.24
   */
  obj_properties[PROP_HDR_TONE_MAPPING] =
      g_param_spec_boolean ("hdr-tone-mapping", "HDR tone mapping",
      "Enable HDR to SDR tone mapping (supported from TGL platforms)",
      PROP_HDR_TONE_MAPPING_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_N, obj_properties);
}


static void
gst_msdkvpp_class_init (gpointer klass, gpointer data)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *trans_class;
  MsdkVPPCData *cdata = data;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_msdkvpp_set_property;
  gobject_class->get_property = gst_msdkvpp_get_property;
  gobject_class->dispose = gst_msdkvpp_dispose;

  _msdkvpp_install_properties (gobject_class);

  trans_class->start = GST_DEBUG_FUNCPTR (gst_msdkvpp_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_msdkvpp_stop);
  trans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_msdkvpp_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_msdkvpp_fixate_caps);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_msdkvpp_set_caps);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_msdkvpp_transform);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_msdkvpp_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_msdkvpp_decide_allocation);
  trans_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_msdkvpp_prepare_output_buffer);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_msdkvpp_query);

  element_class->set_context = gst_msdkvpp_set_context;

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK Video Postprocessor",
      "Filter/Converter/Video;Filter/Converter/Video/Scaler;"
      "Filter/Effect/Video;Filter/Effect/Video/Deinterlace",
      "Video Postprocessing Filter based on " MFX_API_SDK,
      "Sreerenj Balachandrn <sreerenj.balachandran@intel.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkvpp_init (GTypeInstance * instance, gpointer g_class)
{
  GstMsdkVPP *thiz = GST_MSDKVPP (instance);
  thiz->initialized = FALSE;
  thiz->hardware = PROP_HARDWARE_DEFAULT;
  thiz->async_depth = PROP_ASYNC_DEPTH_DEFAULT;
  thiz->denoise_factor = PROP_DENOISE_DEFAULT;
#ifndef GST_REMOVE_DEPRECATED
  thiz->rotation = PROP_ROTATION_DEFAULT;
  thiz->mirroring = PROP_MIRRORING_DEFAULT;
#else
  thiz->rotation = MFX_ANGLE_0;
  thiz->mirroring = MFX_MIRRORING_DISABLED;
#endif
  thiz->deinterlace_mode = PROP_DEINTERLACE_MODE_DEFAULT;
  thiz->deinterlace_method = PROP_DEINTERLACE_METHOD_DEFAULT;
  thiz->buffer_duration = GST_CLOCK_TIME_NONE;
  thiz->hue = PROP_HUE_DEFAULT;
  thiz->saturation = PROP_SATURATION_DEFAULT;
  thiz->brightness = PROP_BRIGHTNESS_DEFAULT;
  thiz->contrast = PROP_CONTRAST_DEFAULT;
  thiz->detail = PROP_DETAIL_DEFAULT;
  thiz->scaling_mode = PROP_SCALING_MODE_DEFAULT;
  thiz->keep_aspect = PROP_FORCE_ASPECT_RATIO_DEFAULT;
  thiz->frc_algm = PROP_FRC_ALGORITHM_DEFAULT;
  thiz->video_direction = PROP_VIDEO_DIRECTION_DEFAULT;
  thiz->crop_left = PROP_CROP_LEFT_DEFAULT;
  thiz->crop_right = PROP_CROP_RIGHT_DEFAULT;
  thiz->crop_top = PROP_CROP_TOP_DEFAULT;
  thiz->crop_bottom = PROP_CROP_BOTTOM_DEFAULT;
  thiz->hdr_tone_mapping = PROP_HDR_TONE_MAPPING_DEFAULT;
#ifndef _WIN32
  thiz->sink_modifier = DRM_FORMAT_MOD_INVALID;
  thiz->src_modifier = DRM_FORMAT_MOD_INVALID;
#endif
  gst_video_info_init (&thiz->sinkpad_info);
  gst_video_info_init (&thiz->srcpad_info);
}

gboolean
gst_msdkvpp_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkVPPCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkVPPClass),
    .class_init = gst_msdkvpp_class_init,
    .instance_size = sizeof (GstMsdkVPP),
    .instance_init = gst_msdkvpp_init
  };

  cdata = g_new (MsdkVPPCData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkVPP");
  feature_name = g_strdup ("msdkvpp");

  type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
      type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
