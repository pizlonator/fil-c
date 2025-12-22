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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sctpreceivemeta.h"

static gboolean gst_sctp_receive_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer);
static gboolean gst_sctp_receive_meta_transform (GstBuffer * transbuf,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data);

GType
gst_sctp_receive_meta_api_get_type (void)
{
  static const gchar *tags[] = { NULL };
  static GType type;
  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstSctpReceiveMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_sctp_receive_meta_get_info (void)
{
  static const GstMetaInfo *gst_sctp_receive_meta_info = NULL;

  if (g_once_init_enter (&gst_sctp_receive_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_SCTP_RECEIVE_META_API_TYPE,
        "GstSctpReceiveMeta",
        sizeof (GstSctpReceiveMeta),
        gst_sctp_receive_meta_init,
        (GstMetaFreeFunction) NULL,
        gst_sctp_receive_meta_transform);
    g_once_init_leave (&gst_sctp_receive_meta_info, meta);
  }
  return gst_sctp_receive_meta_info;
}

static gboolean
gst_sctp_receive_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstSctpReceiveMeta *gst_sctp_receive_meta = (GstSctpReceiveMeta *) meta;
  gst_sctp_receive_meta->ppid = 0;
  return TRUE;
}

static gboolean
gst_sctp_receive_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstSctpReceiveMeta *gst_sctp_receive_meta = (GstSctpReceiveMeta *) meta;
  gst_sctp_buffer_add_receive_meta (transbuf, gst_sctp_receive_meta->ppid);
  return TRUE;
}

GstSctpReceiveMeta *
gst_sctp_buffer_add_receive_meta (GstBuffer * buffer, guint32 ppid)
{
  GstSctpReceiveMeta *gst_sctp_receive_meta = NULL;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  gst_sctp_receive_meta =
      (GstSctpReceiveMeta *) gst_buffer_add_meta (buffer,
      GST_SCTP_RECEIVE_META_INFO, NULL);
  gst_sctp_receive_meta->ppid = ppid;
  return gst_sctp_receive_meta;
}
