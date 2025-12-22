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

#ifndef GST_MSDK_ALLOCATOR_LIBVA_H_
#define GST_MSDK_ALLOCATOR_LIBVA_H_

#include "msdk.h"
#include "gstmsdkallocator.h"

G_BEGIN_DECLS

gboolean
gst_msdk_get_dmabuf_info_from_surface (mfxFrameSurface1 * surface, gint *handle, gsize *size);

gboolean
gst_msdk_export_dmabuf_to_vasurface (GstMsdkContext *context,
    GstVideoInfo *vinfo, gint fd, VASurfaceID *surface_id);

gboolean
gst_msdk_replace_mfx_memid (GstMsdkContext *context,
    mfxFrameSurface1 *mfx_surface, VASurfaceID surface_id);

void
gst_msdk_get_supported_modifiers (GstMsdkContext * context,
    GstMsdkContextJobType job_type, GstVideoFormat format, GValue * modifiers);

G_END_DECLS

#endif /* GST_MSDK_ALLOCATOR_LIBVA_H_ */
