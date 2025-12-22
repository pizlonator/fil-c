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

#ifndef GST_MSDK_CONTEXT_UTIL_H
#define GST_MSDK_CONTEXT_UTIL_H

#include "msdk.h"
#include "gstmsdkcontext.h"
#ifndef _WIN32
#include <va/va.h>
#endif

#define GST_MSDK_CONTEXT_TYPE_NAME "gst.msdk.Context"

G_BEGIN_DECLS

gboolean
gst_msdk_context_find (GstElement * element, GstMsdkContext ** context_ptr);

gboolean
gst_msdk_ensure_new_context (GstElement * element, gboolean hardware, GstMsdkContextJobType job, GstMsdkContext ** context_ptr);

gboolean
gst_msdk_context_get_context (GstContext * context, GstMsdkContext ** msdk_context);

#ifndef _WIN32
gboolean
gst_msdk_context_from_external_va_display (GstContext * context, gboolean hardware,
    GstMsdkContextJobType job_type, GstMsdkContext ** msdk_context);
#else
gboolean
gst_msdk_context_from_external_d3d11_device (GstContext * context, gboolean hardware,
    GstMsdkContextJobType job_type, GstMsdkContext ** msdk_context);
#endif

gboolean
gst_msdk_handle_context_query (GstElement * element, GstQuery * query, GstMsdkContext * msdk_context);

G_END_DECLS

#endif /* GST_MSDK_CONTEXT_UTIL_H */
