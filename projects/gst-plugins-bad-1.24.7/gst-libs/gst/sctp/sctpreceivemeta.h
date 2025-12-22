/*
 * Copyright (c) 2015, Collabora Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef __GST_SCTP_RECEIVE_META_H__
#define __GST_SCTP_RECEIVE_META_H__

#include <gst/gst.h>
#include <gst/sctp/sctp-prelude.h>

G_BEGIN_DECLS

#define GST_SCTP_RECEIVE_META_API_TYPE (gst_sctp_receive_meta_api_get_type())
#define GST_SCTP_RECEIVE_META_INFO (gst_sctp_receive_meta_get_info())
typedef struct _GstSctpReceiveMeta GstSctpReceiveMeta;

struct _GstSctpReceiveMeta
{
  GstMeta meta;

  guint32 ppid;
};

GST_SCTP_API
GType gst_sctp_receive_meta_api_get_type (void);
GST_SCTP_API
const GstMetaInfo *gst_sctp_receive_meta_get_info (void);
GST_SCTP_API
GstSctpReceiveMeta *gst_sctp_buffer_add_receive_meta (GstBuffer * buffer,
    guint32 ppid);

#define gst_sctp_buffer_get_receive_meta(b) ((GstSctpReceiveMeta *)gst_buffer_get_meta((b), GST_SCTP_RECEIVE_META_API_TYPE))

G_END_DECLS

#endif /* __GST_SCTP_RECEIVE_META_H__ */
