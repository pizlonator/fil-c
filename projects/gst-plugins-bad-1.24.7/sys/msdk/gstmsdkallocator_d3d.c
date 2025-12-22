/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel Corporation
 * Copyright (c) 2018, Igalia S.L.
 * All rights reserved.
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
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGDECE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <gst/d3d11/gstd3d11.h>
#include "gstmsdkallocator.h"

#define GST_MSDK_FRAME_SURFACE gst_msdk_frame_surface_quark_get ()
static GQuark
gst_msdk_frame_surface_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark = (gsize) g_quark_from_static_string ("GstMsdkFrameSurface");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}

mfxStatus
gst_msdk_frame_alloc (mfxHDL pthis, mfxFrameAllocRequest * req,
    mfxFrameAllocResponse * resp)
{
  mfxStatus status = MFX_ERR_NONE;
  gint i;
  GstMsdkSurface *msdk_surface = NULL;
  mfxMemId *mids = NULL;
  GstMsdkContext *context = (GstMsdkContext *) pthis;
  GstMsdkAllocResponse *msdk_resp = NULL;
  mfxU32 fourcc = req->Info.FourCC;
  mfxU16 surfaces_num = req->NumFrameSuggested;
  GList *tmp_list = NULL;
  GList *l;
  GstMsdkSurface *tmp_surface = NULL;

  /* MFX_MAKEFOURCC('V','P','8','S') is used for MFX_FOURCC_VP9_SEGMAP surface
   * in MSDK and this surface is an internal surface. The external allocator
   * shouldn't be used for this surface allocation
   *
   * See https://github.com/Intel-Media-SDK/MediaSDK/issues/762
   */
  if (req->Type & MFX_MEMTYPE_INTERNAL_FRAME
      && fourcc == MFX_MAKEFOURCC ('V', 'P', '8', 'S'))
    return MFX_ERR_UNSUPPORTED;

  if (req->Type & MFX_MEMTYPE_EXTERNAL_FRAME) {
    GstMsdkAllocResponse *cached =
        gst_msdk_context_get_cached_alloc_responses_by_request (context, req);
    if (cached) {
      /* check if enough frames were allocated */
      if (req->NumFrameSuggested > cached->response.NumFrameActual)
        return MFX_ERR_MEMORY_ALLOC;

      *resp = cached->response;
      g_atomic_int_inc (&cached->refcount);
      return MFX_ERR_NONE;
    }
  }

  if (!(req->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET |
              MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
    return MFX_ERR_UNSUPPORTED;

  mids = (mfxMemId *) g_slice_alloc0 (surfaces_num * sizeof (mfxMemId));
  msdk_resp =
      (GstMsdkAllocResponse *) g_slice_alloc0 (sizeof (GstMsdkAllocResponse));

  if (fourcc != MFX_FOURCC_P8) {
    GstBufferPool *pool;
    GstVideoFormat format;
    GstStructure *config;
    GstVideoInfo info;
    GstCaps *caps;
    GstVideoAlignment align;
    GstD3D11Device *device;
    GstD3D11AllocationParams *params;

    device = gst_msdk_context_get_d3d11_device (context);

    format = gst_msdk_get_video_format_from_mfx_fourcc (fourcc);
    gst_video_info_set_format (&info, format, req->Info.CropW, req->Info.CropH);

    gst_video_alignment_reset (&align);
    gst_msdk_set_video_alignment
        (&info, req->Info.Width, req->Info.Height, &align);
    gst_video_info_align (&info, &align);

    caps = gst_video_info_to_caps (&info);

    pool = gst_msdk_context_get_alloc_pool (context);
    if (!pool) {
      goto error_alloc;
    }

    config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));
    params = gst_d3d11_allocation_params_new (device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT,
        D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0);
    gst_d3d11_allocation_params_alignment (params, &align);
    gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
    gst_d3d11_allocation_params_free (params);
    gst_buffer_pool_config_set_params (config, caps,
        GST_VIDEO_INFO_SIZE (&info), surfaces_num, surfaces_num);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR ("Failed to set pool config");
      gst_object_unref (pool);
      goto error_alloc;
    }

    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ERROR ("Failed to activate pool");
      gst_object_unref (pool);
      goto error_alloc;
    }

    for (i = 0; i < surfaces_num; i++) {
      GstBuffer *buf;

      if (gst_buffer_pool_acquire_buffer (pool, &buf, NULL) != GST_FLOW_OK) {
        GST_ERROR ("Failed to allocate buffer");
        gst_buffer_pool_set_active (pool, FALSE);
        gst_object_unref (pool);
        goto error_alloc;
      }

      msdk_surface =
          gst_msdk_import_to_msdk_surface (buf, context, &info, GST_MAP_WRITE);
      if (msdk_surface)
        msdk_surface->buf = buf;
      mids[i] = msdk_surface->surface->Data.MemId;
      tmp_list = g_list_prepend (tmp_list, msdk_surface);
    }
  }
  resp->mids = mids;
  resp->NumFrameActual = surfaces_num;

  msdk_resp->response = *resp;
  msdk_resp->request = *req;
  msdk_resp->refcount = 1;

  gst_msdk_context_add_alloc_response (context, msdk_resp);

  /* We need to put all the buffers back to the pool */
  for (l = tmp_list; l; l = l->next) {
    tmp_surface = (GstMsdkSurface *) l->data;
    gst_buffer_unref (tmp_surface->buf);
  }

  return status;

error_alloc:
  g_slice_free1 (surfaces_num * sizeof (mfxMemId), mids);
  g_slice_free1 (sizeof (GstMsdkAllocResponse), msdk_resp);
  return MFX_ERR_MEMORY_ALLOC;
}

mfxStatus
gst_msdk_frame_free (mfxHDL pthis, mfxFrameAllocResponse * resp)
{
  GstMsdkContext *context = (GstMsdkContext *) pthis;
  GstMsdkAllocResponse *cached = NULL;

  cached = gst_msdk_context_get_cached_alloc_responses (context, resp);

  if (cached) {
    if (!g_atomic_int_dec_and_test (&cached->refcount))
      return MFX_ERR_NONE;
  } else
    return MFX_ERR_NONE;

  if (!gst_msdk_context_remove_alloc_response (context, resp))
    return MFX_ERR_NONE;

  g_slice_free1 (resp->NumFrameActual * sizeof (mfxMemId), resp->mids);

  return MFX_ERR_NONE;
}

mfxStatus
gst_msdk_frame_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * data)
{
  return MFX_ERR_NONE;
}

mfxStatus
gst_msdk_frame_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  return MFX_ERR_NONE;
}

mfxStatus
gst_msdk_frame_get_hdl (mfxHDL pthis, mfxMemId mid, mfxHDL * hdl)
{
  GstMsdkMemoryID *mem_id;
  mfxHDLPair *pair;

  if (!hdl || !mid)
    return MFX_ERR_INVALID_HANDLE;

  mem_id = (GstMsdkMemoryID *) mid;
  pair = (mfxHDLPair *) hdl;
  pair->first = (mfxHDL) mem_id->texture;
  pair->second = (mfxHDL) GUINT_TO_POINTER (mem_id->subresource_index);

  return MFX_ERR_NONE;
}

GstMsdkSurface *
gst_msdk_import_to_msdk_surface (GstBuffer * buf, GstMsdkContext * msdk_context,
    GstVideoInfo * vinfo, guint map_flag)
{
  GstMemory *mem = NULL;
  mfxFrameInfo frame_info = { 0, };
  GstMsdkSurface *msdk_surface = NULL;
  mfxFrameSurface1 *mfx_surface = NULL;
  GstMsdkMemoryID *msdk_mid = NULL;
  GstMapInfo map_info;

  mem = gst_buffer_peek_memory (buf, 0);
  msdk_surface = g_slice_new0 (GstMsdkSurface);

  if (!gst_is_d3d11_memory (mem) || gst_buffer_n_memory (buf) > 1) {
    /* d3d11 buffer should hold single memory object */
    g_slice_free (GstMsdkSurface, msdk_surface);
    return NULL;
  }

  if (!gst_buffer_map (buf, &map_info, map_flag | GST_MAP_D3D11)) {
    GST_ERROR ("Failed to map buffer");
    g_slice_free (GstMsdkSurface, msdk_surface);
    return NULL;
  }

  /* If buffer has qdata pointing to mfxFrameSurface1, directly extract it */
  if ((mfx_surface = gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (mem),
              GST_MSDK_FRAME_SURFACE))) {
    msdk_surface->from_qdata = TRUE;
    msdk_surface->surface = mfx_surface;
    gst_buffer_unmap (buf, &map_info);
    return msdk_surface;
  }

  mfx_surface = g_slice_new0 (mfxFrameSurface1);
  msdk_mid = g_slice_new0 (GstMsdkMemoryID);
  mfx_surface->Data.MemId = (mfxMemId) msdk_mid;

  msdk_mid->texture = (ID3D11Texture2D *) (gpointer) map_info.data;
  msdk_mid->subresource_index = GPOINTER_TO_UINT (map_info.user_data[0]);

  gst_buffer_unmap (buf, &map_info);

  gst_msdk_set_mfx_frame_info_from_video_info (&frame_info, vinfo);
  mfx_surface->Info = frame_info;

  /* Set mfxFrameSurface1 as qdata in buffer */
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem),
      GST_MSDK_FRAME_SURFACE, mfx_surface, NULL);

  msdk_surface->surface = mfx_surface;

  return msdk_surface;
}

void
gst_msdk_set_frame_allocator (GstMsdkContext * context)
{
  mfxFrameAllocator gst_msdk_frame_allocator = {
    .pthis = context,
    .Alloc = gst_msdk_frame_alloc,
    .Lock = gst_msdk_frame_lock,
    .Unlock = gst_msdk_frame_unlock,
    .GetHDL = gst_msdk_frame_get_hdl,
    .Free = gst_msdk_frame_free,
  };

  gst_msdk_context_set_frame_allocator (context, &gst_msdk_frame_allocator);
}
