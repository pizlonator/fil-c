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

#ifndef GST_MSDK_CONTEXT_H
#define GST_MSDK_CONTEXT_H

#include "msdk.h"
#ifndef _WIN32
#include <va/va.h>
#include <va/va_drmcommon.h>
#else
#include <gst/d3d11/gstd3d11_fwd.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_MSDK_CONTEXT \
  (gst_msdk_context_get_type ())
#define GST_MSDK_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MSDK_CONTEXT, \
      GstMsdkContext))
#define GST_MSDK_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MSDK_CONTEXT, \
      GstMsdkContextClass))
#define GST_IS_MSDK_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MSDK_CONTEXT))
#define GST_IS_MSDK_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MSDK_CONTEXT))
#define GST_MSDK_CONTEXT_CAST(obj) ((GstMsdkContext*)(obj))

typedef struct _GstMsdkContext GstMsdkContext;
typedef struct _GstMsdkContextClass GstMsdkContextClass;
typedef struct _GstMsdkContextPrivate GstMsdkContextPrivate;

typedef enum {
  GST_MSDK_JOB_DECODER = 0x01,
  GST_MSDK_JOB_ENCODER = 0x02,
  GST_MSDK_JOB_VPP = 0x04,
} GstMsdkContextJobType;

/*
 * GstMsdkContext:
 */
struct _GstMsdkContext
{
  GstObject parent_instance;

  GstMsdkContextPrivate *priv;
};

/*
 * GstMsdkContextClass:
 */
struct _GstMsdkContextClass
{
  GstObjectClass parent_class;
};

GType gst_msdk_context_get_type (void);

GstMsdkContext * gst_msdk_context_new (gboolean hardware);
GstMsdkContext * gst_msdk_context_new_with_job_type (gboolean hardware,
    GstMsdkContextJobType job_type);
GstMsdkContext * gst_msdk_context_new_with_parent (GstMsdkContext * parent);
#ifndef _WIN32
GstMsdkContext * gst_msdk_context_new_with_va_display (GstObject * display_obj,
    gboolean hardware, GstMsdkContextJobType job_type);
#else
GstMsdkContext * gst_msdk_context_new_with_d3d11_device (GstD3D11Device * device,
    gboolean hardware, GstMsdkContextJobType job_type);
#endif
mfxSession gst_msdk_context_get_session (GstMsdkContext * context);
const mfxLoader * gst_msdk_context_get_loader (GstMsdkContext * context);
mfxU32 gst_msdk_context_get_impl_idx (GstMsdkContext * context);

gpointer gst_msdk_context_get_handle (GstMsdkContext * context);
#ifndef _WIN32
GstObject * gst_msdk_context_get_va_display (GstMsdkContext * context);
#else
GstD3D11Device * gst_msdk_context_get_d3d11_device (GstMsdkContext * context);
#endif

/* GstMsdkContext contains mfxFrameAllocResponses,
 * if app calls MFXVideoCORE_SetFrameAllocator.
 */
typedef struct _GstMsdkAllocResponse GstMsdkAllocResponse;

struct _GstMsdkAllocResponse {
  gint refcount;
  mfxFrameAllocResponse response;
  mfxFrameAllocRequest request;
};

GstMsdkAllocResponse *
gst_msdk_context_get_cached_alloc_responses (GstMsdkContext * context,
    mfxFrameAllocResponse * resp);

GstMsdkAllocResponse *
gst_msdk_context_get_cached_alloc_responses_by_request (GstMsdkContext * context,
    mfxFrameAllocRequest * req);

void
gst_msdk_context_add_alloc_response (GstMsdkContext * context,
    GstMsdkAllocResponse * resp);

gboolean
gst_msdk_context_remove_alloc_response (GstMsdkContext * context,
    mfxFrameAllocResponse * resp);

void
gst_msdk_context_set_alloc_pool (GstMsdkContext * context, GstBufferPool * pool);

GstBufferPool *
gst_msdk_context_get_alloc_pool (GstMsdkContext * context);

GstMsdkContextJobType
gst_msdk_context_get_job_type (GstMsdkContext * context);

void
gst_msdk_context_set_job_type (GstMsdkContext * context,
    GstMsdkContextJobType job_type);

void
gst_msdk_context_add_job_type (GstMsdkContext * context, GstMsdkContextJobType job_type);

gint
gst_msdk_context_get_shared_async_depth (GstMsdkContext * context);

void
gst_msdk_context_add_shared_async_depth (GstMsdkContext * context, gint async_depth);

void
gst_msdk_context_set_frame_allocator (GstMsdkContext * context,
    mfxFrameAllocator * allocator);

G_END_DECLS

#endif /* GST_MSDK_CONTEXT_H */
