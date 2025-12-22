/* GStreamer AIFF muxer
 * Copyright (C) 2009 Robert Swain <robert.swain@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-aiffmux
 * @title: aiffmux
 *
 * Format an audio stream into the Audio Interchange File Format
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <gst/gst.h>
#include <gst/base/gstbytewriter.h>

#include "aiffelements.h"
#include "aiffmux.h"

GST_DEBUG_CATEGORY (aiffmux_debug);
#define GST_CAT_DEFAULT aiffmux_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = { S8, S16BE, S24BE, S32BE },"
        "channels = (int) [ 1, MAX ], " "rate = (int) [ 1, MAX ]")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-aiff")
    );

#define gst_aiff_mux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAiffMux, gst_aiff_mux, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (aiffmux_debug, "aiffmux", 0, "AIFF muxer"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (aiffmux, "aiffmux", GST_RANK_PRIMARY,
    GST_TYPE_AIFF_MUX, aiff_element_init (plugin));

static GstStateChangeReturn
gst_aiff_mux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstAiffMux *aiffmux = GST_AIFF_MUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_audio_info_init (&aiffmux->info);
      aiffmux->length = 0;
      aiffmux->sent_header = FALSE;
      aiffmux->overflow = FALSE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  return ret;
}

static void
gst_aiff_mux_class_init (GstAiffMuxClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "AIFF audio muxer", "Muxer/Audio", "Multiplex raw audio into AIFF",
      "Robert Swain <robert.swain@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_aiff_mux_change_state);
}

#define AIFF_FORM_HEADER_LEN 8 + 4
#define AIFF_COMM_HEADER_LEN 8 + 18
#define AIFF_SSND_HEADER_LEN 8 + 8
#define AIFF_HEADER_LEN \
  (AIFF_FORM_HEADER_LEN + AIFF_COMM_HEADER_LEN + AIFF_SSND_HEADER_LEN)

static void
gst_aiff_mux_write_form_header (GstAiffMux * aiffmux, guint32 audio_data_size,
    GstByteWriter * writer)
{
  guint64 cur_size;

  /* ckID == 'FORM' */
  gst_byte_writer_put_uint32_le_unchecked (writer,
      GST_MAKE_FOURCC ('F', 'O', 'R', 'M'));

  /* AIFF chunks must be even aligned */
  cur_size = AIFF_HEADER_LEN - 8 + audio_data_size;
  if ((cur_size & 1) && cur_size + 1 < G_MAXUINT32) {
    cur_size += 1;
  }

  gst_byte_writer_put_uint32_be_unchecked (writer, cur_size);
  /* formType == 'AIFF' */
  gst_byte_writer_put_uint32_le_unchecked (writer,
      GST_MAKE_FOURCC ('A', 'I', 'F', 'F'));
}

/*
 * BEGIN: Code borrowed from FFmpeg's libavutil/intfloat_readwrite.{c,h}
 * Copyright (c) 2005 Michael Niedermayer <michaelni@gmx.at>
 */

/* IEEE 80 bits extended float */
typedef struct AVExtFloat
{
  guint8 exponent[2];
  guint8 mantissa[8];
} AVExtFloat;

/* Courtesy http://www.devx.com/tips/Tip/42853 */
static inline gint
gst_aiff_mux_isinf (gdouble x)
{
  volatile gdouble temp = x;
  if ((temp == x) && ((temp - x) != 0.0))
    return (x < 0.0 ? -1 : 1);
  else
    return 0;
}

static void
gst_aiff_mux_write_ext (GstByteWriter * writer, double d)
{
  struct AVExtFloat ext = { {0} };
  gint e, i;
  gdouble f;
  guint64 m;

  f = fabs (frexp (d, &e));
  if (f >= 0.5 && f < 1) {
    e += 16382;
    ext.exponent[0] = e >> 8;
    ext.exponent[1] = e;
    m = (guint64) ldexp (f, 64);
    for (i = 0; i < 8; i++)
      ext.mantissa[i] = m >> (56 - (i << 3));
  } else if (f != 0.0) {
    ext.exponent[0] = 0x7f;
    ext.exponent[1] = 0xff;
    if (!gst_aiff_mux_isinf (f))
      ext.mantissa[0] = ~0;
  }
  if (d < 0)
    ext.exponent[0] |= 0x80;

  gst_byte_writer_put_data_unchecked (writer, ext.exponent, 2);
  gst_byte_writer_put_data_unchecked (writer, ext.mantissa, 8);
}

/*
 * END: Code borrowed from FFmpeg's libavutil/intfloat_readwrite.{c,h}
 */

static void
gst_aiff_mux_write_comm_header (GstAiffMux * aiffmux, guint32 audio_data_size,
    GstByteWriter * writer)
{
  guint16 channels;
  guint16 width, depth;
  gdouble rate;

  channels = GST_AUDIO_INFO_CHANNELS (&aiffmux->info);
  width = GST_AUDIO_INFO_WIDTH (&aiffmux->info);
  depth = GST_AUDIO_INFO_DEPTH (&aiffmux->info);
  rate = GST_AUDIO_INFO_RATE (&aiffmux->info);

  gst_byte_writer_put_uint32_le_unchecked (writer,
      GST_MAKE_FOURCC ('C', 'O', 'M', 'M'));
  gst_byte_writer_put_uint32_be_unchecked (writer, 18);
  gst_byte_writer_put_uint16_be_unchecked (writer, channels);
  /* numSampleFrames value will be overwritten when known */
  gst_byte_writer_put_uint32_be_unchecked (writer,
      audio_data_size / (width / 8 * channels));
  gst_byte_writer_put_uint16_be_unchecked (writer, depth);
  gst_aiff_mux_write_ext (writer, rate);
}

static void
gst_aiff_mux_write_ssnd_header (GstAiffMux * aiffmux, guint32 audio_data_size,
    GstByteWriter * writer)
{
  gst_byte_writer_put_uint32_le_unchecked (writer,
      GST_MAKE_FOURCC ('S', 'S', 'N', 'D'));
  /* ckSize will be overwritten when known */
  gst_byte_writer_put_uint32_be_unchecked (writer,
      audio_data_size + AIFF_SSND_HEADER_LEN - 8);
  /* offset and blockSize are set to 0 as we don't support block-aligned sample data yet */
  gst_byte_writer_put_uint32_be_unchecked (writer, 0);
  gst_byte_writer_put_uint32_be_unchecked (writer, 0);
}

static GstFlowReturn
gst_aiff_mux_push_header (GstAiffMux * aiffmux, guint32 audio_data_size)
{
  GstFlowReturn ret;
  GstBuffer *outbuf;
  GstByteWriter writer;
  GstSegment seg;

  /* seek to beginning of file */
  gst_segment_init (&seg, GST_FORMAT_BYTES);

  if (gst_pad_push_event (aiffmux->srcpad,
          gst_event_new_segment (&seg)) == FALSE) {
    GST_ELEMENT_WARNING (aiffmux, STREAM, MUX,
        ("An output stream seeking error occurred when multiplexing."),
        ("Failed to seek to beginning of stream to write header."));
  }

  GST_DEBUG_OBJECT (aiffmux, "writing header with datasize=%u",
      audio_data_size);

  gst_byte_writer_init_with_size (&writer, AIFF_HEADER_LEN, TRUE);

  gst_aiff_mux_write_form_header (aiffmux, audio_data_size, &writer);
  gst_aiff_mux_write_comm_header (aiffmux, audio_data_size, &writer);
  gst_aiff_mux_write_ssnd_header (aiffmux, audio_data_size, &writer);

  outbuf = gst_byte_writer_reset_and_get_buffer (&writer);

  ret = gst_pad_push (aiffmux->srcpad, outbuf);

  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (aiffmux, "push header failed: flow = %s",
        gst_flow_get_name (ret));
  }

  return ret;
}

static GstFlowReturn
gst_aiff_mux_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstAiffMux *aiffmux = GST_AIFF_MUX (parent);
  GstFlowReturn flow = GST_FLOW_OK;
  guint64 cur_size;
  gsize buf_size;

  if (!GST_AUDIO_INFO_CHANNELS (&aiffmux->info))
    goto not_negotiated;

  if (G_UNLIKELY (aiffmux->overflow))
    goto overflow;

  if (!aiffmux->sent_header) {
    /* use bogus size initially, we'll write the real
     * header when we get EOS and know the exact length */
    flow = gst_aiff_mux_push_header (aiffmux, 0x7FFF0000);
    if (flow != GST_FLOW_OK)
      goto flow_error;

    GST_DEBUG_OBJECT (aiffmux, "wrote dummy header");
    aiffmux->sent_header = TRUE;
  }

  /* AIFF has an audio data size limit of slightly under 4 GB.
     A value of audiosize + AIFF_HEADER_LEN - 8 is written, so
     I'll error out if writing data that makes this overflow. */
  cur_size = aiffmux->length + AIFF_HEADER_LEN - 8;
  buf_size = gst_buffer_get_size (buf);

  if (G_UNLIKELY (cur_size + buf_size >= G_MAXUINT32)) {
    GST_ERROR_OBJECT (aiffmux, "AIFF only supports about 4 GB worth of "
        "audio data, dropping any further data on the floor");
    GST_ELEMENT_WARNING (aiffmux, STREAM, MUX, ("AIFF has a 4GB size limit"),
        ("AIFF only supports about 4 GB worth of audio data, "
            "dropping any further data on the floor"));
    aiffmux->overflow = TRUE;
    goto overflow;
  }

  GST_LOG_OBJECT (aiffmux,
      "pushing %" G_GSIZE_FORMAT " bytes raw audio, ts=%" GST_TIME_FORMAT,
      buf_size, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));

  buf = gst_buffer_make_writable (buf);

  GST_BUFFER_OFFSET (buf) = AIFF_HEADER_LEN + aiffmux->length;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  aiffmux->length += buf_size;

  flow = gst_pad_push (aiffmux->srcpad, buf);

  return flow;

not_negotiated:
  {
    GST_WARNING_OBJECT (aiffmux, "no input format negotiated");
    gst_buffer_unref (buf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
overflow:
  {
    GST_WARNING_OBJECT (aiffmux, "output file too large, dropping buffer");
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }
flow_error:
  {
    GST_DEBUG_OBJECT (aiffmux, "got flow error %s", gst_flow_get_name (flow));
    gst_buffer_unref (buf);
    return flow;
  }
}

static gboolean
gst_aiff_mux_set_caps (GstAiffMux * aiffmux, GstCaps * caps)
{
  GstCaps *outcaps;
  GstAudioInfo info;

  if (aiffmux->sent_header) {
    GST_WARNING_OBJECT (aiffmux, "cannot change format mid-stream");
    return FALSE;
  }

  GST_DEBUG_OBJECT (aiffmux, "got caps: %" GST_PTR_FORMAT, caps);

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (aiffmux, "caps incomplete");
    return FALSE;
  }

  aiffmux->info = info;

  GST_LOG_OBJECT (aiffmux,
      "accepted caps: chans=%d depth=%d rate=%d",
      GST_AUDIO_INFO_CHANNELS (&info), GST_AUDIO_INFO_DEPTH (&info),
      GST_AUDIO_INFO_RATE (&info));

  outcaps = gst_static_pad_template_get_caps (&src_factory);
  gst_pad_push_event (aiffmux->srcpad, gst_event_new_caps (outcaps));
  gst_caps_unref (outcaps);

  return TRUE;
}


static gboolean
gst_aiff_mux_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstAiffMux *aiffmux;

  aiffmux = GST_AIFF_MUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:{
      guint64 cur_size;
      GST_DEBUG_OBJECT (aiffmux, "got EOS");

      cur_size = aiffmux->length + AIFF_HEADER_LEN - 8;

      /* ID3 chunk must be even aligned */
      if ((aiffmux->length & 1) && cur_size + 1 < G_MAXUINT32) {
        GstFlowReturn ret;
        guint8 *data = g_new0 (guint8, 1);
        GstBuffer *buffer = gst_buffer_new_wrapped (data, 1);
        GST_BUFFER_OFFSET (buffer) = AIFF_HEADER_LEN + aiffmux->length;
        GST_BUFFER_OFFSET_END (buffer) = GST_BUFFER_OFFSET_NONE;
        ret = gst_pad_push (aiffmux->srcpad, buffer);
        if (ret != GST_FLOW_OK) {
          GST_WARNING_OBJECT (aiffmux, "failed to push padding byte: %s",
              gst_flow_get_name (ret));
        }
      }

      /* write header with correct length values */
      gst_aiff_mux_push_header (aiffmux, aiffmux->length);

      /* and forward the EOS event */
      res = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_aiff_mux_set_caps (aiffmux, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
      /* Just drop it, it's probably in TIME format
       * anyway. We'll send our own newsegment event */
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
  return res;
}

static void
gst_aiff_mux_init (GstAiffMux * aiffmux)
{
  aiffmux->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (aiffmux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_mux_chain));
  gst_pad_set_event_function (aiffmux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiff_mux_event));
  gst_element_add_pad (GST_ELEMENT (aiffmux), aiffmux->sinkpad);

  aiffmux->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps (aiffmux->srcpad);
  gst_element_add_pad (GST_ELEMENT (aiffmux), aiffmux->srcpad);
}
