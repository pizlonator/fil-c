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

#ifndef __GST_SCTP_ENC_H__
#define __GST_SCTP_ENC_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include "sctpassociation.h"

G_BEGIN_DECLS

#define GST_TYPE_SCTP_ENC (gst_sctp_enc_get_type())
#define GST_SCTP_ENC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SCTP_ENC, GstSctpEnc))
#define GST_SCTP_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SCTP_ENC, GstSctpEncClass))
#define GST_IS_SCTP_ENC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SCTP_ENC))
#define GST_IS_SCTP_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SCTP_ENC))
typedef struct _GstSctpEnc GstSctpEnc;
typedef struct _GstSctpEncClass GstSctpEncClass;
typedef struct _GstSctpEncPrivate GstSctpEncPrivate;

struct _GstSctpEnc
{
  GstElement element;

  GstPad *src_pad;
  GstFlowReturn src_ret;
  gboolean need_stream_start_caps, need_segment;
  guint32 sctp_association_id;
  guint16 remote_sctp_port;
  gboolean use_sock_stream;

  GstSctpAssociation *sctp_association;
  GstDataQueue *outbound_sctp_packet_queue;

  GQueue pending_pads;

  gulong signal_handler_state_changed;
};

struct _GstSctpEncClass
{
  GstElementClass parent_class;

  void (*on_sctp_association_is_established) (GstSctpEnc * sctp_enc,
      gboolean established);
    guint64 (*on_get_stream_bytes_sent) (GstSctpEnc * sctp_enc,
      guint stream_id);

};

GType gst_sctp_enc_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (sctpenc);

G_END_DECLS

#endif /* __GST_SCTP_ENC_H__ */
