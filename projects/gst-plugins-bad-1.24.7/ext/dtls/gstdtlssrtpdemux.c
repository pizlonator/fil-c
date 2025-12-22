/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
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

#include "gstdtlselements.h"
#include "gstdtlssrtpdemux.h"

#define PACKET_IS_DTLS(b) (b > 0x13 && b < 0x40)
#define PACKET_IS_RTP(b) (b > 0x7f && b < 0xc0)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate rtp_src_template =
    GST_STATIC_PAD_TEMPLATE ("rtp_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp;"
        "application/x-rtcp;" "application/x-srtp;" "application/x-srtcp")
    );

static GstStaticPadTemplate dtls_src_template =
GST_STATIC_PAD_TEMPLATE ("dtls_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-dtls")
    );

GST_DEBUG_CATEGORY_STATIC (gst_gst_dtls_srtp_demux_debug);
#define GST_CAT_DEFAULT gst_gst_dtls_srtp_demux_debug

#define gst_dtls_srtp_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDtlsSrtpDemux, gst_dtls_srtp_demux,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_gst_dtls_srtp_demux_debug,
        "dtlssrtpdemux", 0, "DTLS SRTP Demultiplexer"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dtlssrtpdemux, "dtlssrtpdemux",
    GST_RANK_NONE, GST_TYPE_DTLS_SRTP_DEMUX, dtls_element_init (plugin));


static GstFlowReturn sink_chain (GstPad *, GstObject * self, GstBuffer *);

static void
gst_dtls_srtp_demux_class_init (GstDtlsSrtpDemuxClass * klass)
{
  GstElementClass *element_class;

  element_class = (GstElementClass *) klass;

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &rtp_src_template);
  gst_element_class_add_static_pad_template (element_class, &dtls_src_template);

  gst_element_class_set_static_metadata (element_class,
      "DTLS SRTP Demultiplexer",
      "DTLS/SRTP/Demux",
      "Demultiplexes DTLS and SRTP packets",
      "Patrik Oldsberg patrik.oldsberg@ericsson.com");
}

static void
gst_dtls_srtp_demux_init (GstDtlsSrtpDemux * self)
{
  GstPad *sink;

  sink = gst_pad_new_from_static_template (&sink_template, "sink");
  self->rtp_src =
      gst_pad_new_from_static_template (&rtp_src_template, "rtp_src");
  self->dtls_src =
      gst_pad_new_from_static_template (&dtls_src_template, "dtls_src");
  g_return_if_fail (sink);
  g_return_if_fail (self->rtp_src);
  g_return_if_fail (self->dtls_src);

  gst_pad_set_chain_function (sink, GST_DEBUG_FUNCPTR (sink_chain));

  gst_element_add_pad (GST_ELEMENT (self), sink);
  gst_element_add_pad (GST_ELEMENT (self), self->rtp_src);
  gst_element_add_pad (GST_ELEMENT (self), self->dtls_src);
}

static GstFlowReturn
sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDtlsSrtpDemux *self = GST_DTLS_SRTP_DEMUX (parent);
  guint8 first_byte;

  if (gst_buffer_get_size (buffer) == 0) {
    GST_LOG_OBJECT (self, "received buffer with size 0");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  if (gst_buffer_extract (buffer, 0, &first_byte, 1) != 1) {
    GST_WARNING_OBJECT (self, "could not extract first byte from buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  if (PACKET_IS_DTLS (first_byte)) {
    GST_LOG_OBJECT (self, "pushing dtls packet");

    return gst_pad_push (self->dtls_src, buffer);
  }

  if (PACKET_IS_RTP (first_byte)) {
    GST_LOG_OBJECT (self, "pushing rtp packet");

    return gst_pad_push (self->rtp_src, buffer);
  }

  GST_WARNING_OBJECT (self, "received invalid buffer: %x", first_byte);
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}
