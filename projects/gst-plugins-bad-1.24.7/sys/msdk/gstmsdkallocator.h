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

#ifndef GST_MSDK_ALLOCATOR_H_
#define GST_MSDK_ALLOCATOR_H_

#include "msdk.h"
#include "gstmsdkcontext.h"

G_BEGIN_DECLS

typedef struct _GstMsdkMemoryID GstMsdkMemoryID;
typedef struct _GstMsdkSurface GstMsdkSurface;

struct _GstMsdkMemoryID {
  mfxU32 fourcc;

#ifndef _WIN32
  VASurfaceID surface;
  VAImage image;
  VADRMPRIMESurfaceDescriptor desc;
#else
  ID3D11Texture2D *texture;
  guint subresource_index;
  gint pitch;
  guint offset;
#endif
};

struct _GstMsdkSurface
{
  mfxFrameSurface1 *surface;
  GstBuffer *buf;
  gboolean from_qdata;
};

GstMsdkSurface *
gst_msdk_import_sys_mem_to_msdk_surface (GstBuffer * buf, const GstVideoInfo * info);

mfxStatus gst_msdk_frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req, mfxFrameAllocResponse *resp);
mfxStatus gst_msdk_frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp);
mfxStatus gst_msdk_frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus gst_msdk_frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus gst_msdk_frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL *hdl);

void gst_msdk_set_frame_allocator (GstMsdkContext * context);

GstMsdkSurface *
gst_msdk_import_to_msdk_surface (GstBuffer * buf, GstMsdkContext * msdk_context,
    GstVideoInfo * vinfo, guint map_flag);

GQuark
gst_msdk_frame_surface_quark_get (void);

G_END_DECLS

#endif /* GST_MSDK_ALLOCATOR_H_ */
