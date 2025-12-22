/* GStreamer Intel MSDK plugin
 * Copyright (c) 2023, Intel Corporation.
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
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GST_MSDKCAPS_H__
#define __GST_MSDKCAPS_H__

#include "msdk.h"
#include "gstmsdkcontext.h"

#include <mfxjpeg.h>
#include <mfxvp8.h>

G_BEGIN_DECLS

gboolean
gst_msdkcaps_has_feature (const GstCaps * caps, const gchar * feature);

gboolean
gst_msdkcaps_enc_create_caps (GstMsdkContext * context,
    gpointer enc_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps);

gboolean
gst_msdkcaps_dec_create_caps (GstMsdkContext * context,
    gpointer dec_description, guint codec_id,
    GstCaps ** sink_caps, GstCaps ** src_caps);

gboolean
gst_msdkcaps_vpp_create_caps (GstMsdkContext * context,
    gpointer vpp_description, GstCaps ** sink_caps, GstCaps ** src_caps);

gboolean
gst_msdkcaps_enc_create_static_caps (GstMsdkContext * context,
    guint codec_id, GstCaps ** sink_caps, GstCaps ** src_caps);

gboolean
gst_msdkcaps_dec_create_static_caps (GstMsdkContext * context,
    guint codec_id, GstCaps ** sink_caps, GstCaps ** src_caps);

gboolean
gst_msdkcaps_vpp_create_static_caps (GstMsdkContext * context,
    GstCaps ** sink_caps, GstCaps ** src_caps);

void
gst_msdkcaps_pad_template_init (GstElementClass * klass,
    GstCaps * sink_caps, GstCaps * src_caps,
    const gchar * doc_sink_caps_str, const gchar * doc_src_caps_str);

gboolean
gst_msdkcaps_set_strings (GstCaps * caps,
    const gchar * features, const char * field, const gchar * strings);

gboolean
gst_msdkcaps_video_info_from_caps (const GstCaps * caps,
    GstVideoInfo * info, guint64 * modifier);

#ifndef _WIN32
GstCaps *
gst_msdkcaps_video_info_to_drm_caps (GstVideoInfo * info, guint64 modifier);

guint64
get_msdkcaps_get_modifier (const GstCaps * caps);
#endif

G_END_DECLS

#endif /* __GST_MSDKCAPS_H__ */
