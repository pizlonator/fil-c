/*
 * GStreamer - GStreamer SRTP decoder
 *
 * Copyright 2009-2011 Collabora Ltd.
 *  @author: Gabriel Millaire <gabriel.millaire@collabora.co.uk>
 *  @author: Olivier Crete <olivier.crete@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-srtpdec
 * @title: srtpdec
 * @see_also: srtpenc
 *
 * gstrtpdec acts as a decoder that removes security from SRTP and SRTCP
 * packets (encryption and authentication) and out RTP and RTCP. It
 * receives packet of type 'application/x-srtp' or 'application/x-srtcp'
 * on its sink pad, and outs packets of type 'application/x-rtp' or
 * 'application/x-rtcp' on its source pad.
 *
 * For each packet received, it checks if the internal SSRC is in the list
 * of streams already in use. If this is not the case, it sends a signal to
 * the user to get the needed parameters to create a new stream : master
 * key, encryption and authentication mechanisms for both RTP and RTCP. If
 * the user can't provide those parameters, the buffer is dropped and a
 * warning is emitted.
 *
 * This element uses libsrtp library. The encryption and authentication
 * mechanisms available are :
 *
 * Encryption
 * - AES_ICM 256 bits (maximum security)
 * - AES_ICM 128 bits (default)
 * - NULL
 *
 * Authentication
 * - HMAC_SHA1 80 bits (default, maximum protection)
 * - HMAC_SHA1 32 bits
 * - NULL
 *
 * Note that for SRTP protection, authentication is mandatory (non-null)
 * if encryption is used (non-null).
 *
 * Each packet received is first analysed (checked for valid SSRC) then
 * its buffer is unprotected with libsrtp, then pushed on the source pad.
 * If protection failed or the stream could not be created, the buffer
 * is dropped and a warning is emitted.
 *
 * When the maximum usage of the master key is reached, a soft-limit
 * signal is sent to the user, and new parameters (master key) are needed
 * in return. If the hard limit is reached, a flag is set and every
 * subsequent packet is dropped, until a new key is set and the stream
 * has been updated.
 *
 * If a stream is to be shared between multiple clients the SRTP
 * rollover counter for a given SSRC must be set in the caps "roc" field
 * when the request-key signal is emitted by the decoder. The rollover
 * counters should have been transmitted by a signaling protocol by some
 * other means. If no rollover counter is provided by the user, 0 is
 * used by default.
 *
 * It is possible to receive a stream protected by multiple master keys, each buffer
 * then contains a Master Key Identifier (MKI) to identify which key was used for this
 * buffer. If multiple keys are needed, the first key can be specified in the caps as
 * "srtp-key=(buffer)key1data, mki=(buffer)mki1data", then the second one can be given in
 * the same caps as "srtp-key2=(buffer)key2data, mki2=(buffer)mki2data", and more can
 * be added up to 15.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 udpsrc port=5004 caps='application/x-srtp, payload=(int)8, ssrc=(uint)1356955624, srtp-key=(buffer)012345678901234567890123456789012345678901234567890123456789, srtp-cipher=(string)aes-128-icm, srtp-auth=(string)hmac-sha1-80, srtcp-cipher=(string)aes-128-icm, srtcp-auth=(string)hmac-sha1-80' !  srtpdec ! rtppcmadepay ! alawdec ! pulsesink
 * ]| Receive PCMA SRTP packets through UDP using caps to specify
 * master key and protection.
 * |[
 * gst-launch-1.0 audiotestsrc ! alawenc ! rtppcmapay ! 'application/x-rtp, payload=(int)8, ssrc=(uint)1356955624' ! srtpenc key="012345678901234567890123456789012345678901234567890123456789" ! udpsink port=5004
 * ]| Send PCMA SRTP packets through UDP, nothing how the SSRC is forced so
 * that the receiver will recognize it.
 *
 */

#include "gstsrtpelements.h"
#include "gstsrtpdec.h"
#include <gst/rtp/gstrtpbuffer.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_srtp_dec_debug);
#define GST_CAT_DEFAULT gst_srtp_dec_debug

#define DEFAULT_REPLAY_WINDOW_SIZE 128

/* Filter signals and args */
enum
{
  SIGNAL_REQUEST_KEY = 1,
  SIGNAL_CLEAR_KEYS,
  SIGNAL_SOFT_LIMIT,
  SIGNAL_HARD_LIMIT,
  SIGNAL_REMOVE_KEY,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_REPLAY_WINDOW_SIZE,
  PROP_STATS
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate rtp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-srtp")
    );

static GstStaticPadTemplate rtp_src_template =
GST_STATIC_PAD_TEMPLATE ("rtp_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate rtcp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtcp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-srtcp")
    );

static GstStaticPadTemplate rtcp_src_template =
GST_STATIC_PAD_TEMPLATE ("rtcp_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtcp")
    );

static guint gst_srtp_dec_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GstSrtpDec, gst_srtp_dec, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_srtp_dec_debug, "srtpdec", 0, "SRTP dec");
    );
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtpdec, "srtpdec", GST_RANK_NONE,
    GST_TYPE_SRTP_DEC, srtp_element_init (plugin));

static void gst_srtp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_srtp_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_srtp_dec_clear_streams (GstSrtpDec * filter);
static void gst_srtp_dec_remove_stream (GstSrtpDec * filter, guint ssrc);

static gboolean gst_srtp_dec_sink_event_rtp (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_srtp_dec_sink_event_rtcp (GstPad * pad, GstObject * parent,
    GstEvent * event);

static gboolean gst_srtp_dec_sink_query_rtp (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_srtp_dec_sink_query_rtcp (GstPad * pad,
    GstObject * parent, GstQuery * query);


static GstIterator *gst_srtp_dec_iterate_internal_links_rtp (GstPad * pad,
    GstObject * parent);
static GstIterator *gst_srtp_dec_iterate_internal_links_rtcp (GstPad * pad,
    GstObject * parent);

static GstFlowReturn gst_srtp_dec_chain_rtp (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_srtp_dec_chain_rtcp (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

static GstStateChangeReturn gst_srtp_dec_change_state (GstElement * element,
    GstStateChange transition);

static GstSrtpDecSsrcStream *request_key_with_signal (GstSrtpDec * filter,
    guint32 ssrc, gint signal);

struct _GstSrtpDecSsrcStream
{
  guint32 ssrc;

  guint32 roc;
  GstBuffer *key;
  GstSrtpCipherType rtp_cipher;
  GstSrtpAuthType rtp_auth;
  GstSrtpCipherType rtcp_cipher;
  GstSrtpAuthType rtcp_auth;
  GArray *keys;
  guint recv_count;
  guint recv_drop_count;
};

#ifdef HAVE_SRTP2
struct GstSrtpDecKey
{
  GstBuffer *mki;
  GstBuffer *key;
};
#endif

#define STREAM_HAS_CRYPTO(stream)                       \
  (stream->rtp_cipher != GST_SRTP_CIPHER_NULL ||        \
      stream->rtcp_cipher != GST_SRTP_CIPHER_NULL ||    \
      stream->rtp_auth != GST_SRTP_AUTH_NULL ||         \
      stream->rtcp_auth != GST_SRTP_AUTH_NULL)


/* initialize the srtpdec's class */
static void
gst_srtp_dec_class_init (GstSrtpDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_srtp_dec_set_property;
  gobject_class->get_property = gst_srtp_dec_get_property;

  gst_element_class_add_static_pad_template (gstelement_class,
      &rtp_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &rtp_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &rtcp_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &rtcp_sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "SRTP decoder",
      "Filter/Network/SRTP",
      "A SRTP and SRTCP decoder",
      "Gabriel Millaire <millaire.gabriel@collabora.com>");

  /* Install callbacks */
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_srtp_dec_change_state);

  klass->clear_streams = GST_DEBUG_FUNCPTR (gst_srtp_dec_clear_streams);
  klass->remove_stream = GST_DEBUG_FUNCPTR (gst_srtp_dec_remove_stream);

  /* Install properties */
  g_object_class_install_property (gobject_class, PROP_REPLAY_WINDOW_SIZE,
      g_param_spec_uint ("replay-window-size", "Replay window size",
          "Size of the replay protection window",
          64, 0x8000, DEFAULT_REPLAY_WINDOW_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics", "Various statistics",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* Install signals */
  /**
   * GstSrtpDec::request-key:
   * @gstsrtpdec: the element on which the signal is emitted
   * @ssrc: The unique SSRC of the stream
   *
   * Signal emitted to get the parameters relevant to stream
   * with @ssrc. User should provide the key and the RTP and
   * RTCP encryption ciphers and authentication, and return
   * them wrapped in a GstCaps.
   */
  gst_srtp_dec_signals[SIGNAL_REQUEST_KEY] =
      g_signal_new ("request-key", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_CAPS, 1, G_TYPE_UINT);

  /**
   * GstSrtpDec::clear-keys:
   * @gstsrtpdec: the element on which the signal is emitted
   *
   * Clear the internal list of streams
   */
  gst_srtp_dec_signals[SIGNAL_CLEAR_KEYS] =
      g_signal_new ("clear-keys", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstSrtpDecClass, clear_streams), NULL, NULL, NULL,
      G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstSrtpDec::soft-limit:
   * @gstsrtpdec: the element on which the signal is emitted
   * @ssrc: The unique SSRC of the stream
   *
   * Signal emitted when the stream with @ssrc has reached the
   * soft limit of utilisation of it's master encryption key.
   * User should provide a new key and new RTP and RTCP encryption
   * ciphers and authentication, and return them wrapped in a
   * GstCaps.
   */
  gst_srtp_dec_signals[SIGNAL_SOFT_LIMIT] =
      g_signal_new ("soft-limit", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_CAPS, 1, G_TYPE_UINT);

  /**
   * GstSrtpDec::hard-limit:
   * @gstsrtpdec: the element on which the signal is emitted
   * @ssrc: The unique SSRC of the stream
   *
   * Signal emitted when the stream with @ssrc has reached the
   * hard limit of utilisation of it's master encryption key.
   * User should provide a new key and new RTP and RTCP encryption
   * ciphers and authentication, and return them wrapped in a
   * GstCaps. If user could not provide those parameters or signal
   * is not answered, the buffers of this stream will be dropped.
   */
  gst_srtp_dec_signals[SIGNAL_HARD_LIMIT] =
      g_signal_new ("hard-limit", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_CAPS, 1, G_TYPE_UINT);

  /**
   * GstSrtpDec::remove-key:
   * @gstsrtpdec: the element on which the signal is emitted
   * @ssrc: The SSRC for which to remove the key.
   *
   * Removes keys for a specific SSRC
   */
  gst_srtp_dec_signals[SIGNAL_REMOVE_KEY] =
      g_signal_new ("remove-key", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstSrtpDecClass, remove_stream), NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_UINT);

}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_srtp_dec_init (GstSrtpDec * filter)
{
  filter->replay_window_size = DEFAULT_REPLAY_WINDOW_SIZE;

  filter->rtp_sinkpad =
      gst_pad_new_from_static_template (&rtp_sink_template, "rtp_sink");
  gst_pad_set_event_function (filter->rtp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_sink_event_rtp));
  gst_pad_set_query_function (filter->rtp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_sink_query_rtp));
  gst_pad_set_iterate_internal_links_function (filter->rtp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_iterate_internal_links_rtp));
  gst_pad_set_chain_function (filter->rtp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_chain_rtp));

  filter->rtp_srcpad =
      gst_pad_new_from_static_template (&rtp_src_template, "rtp_src");
  gst_pad_set_iterate_internal_links_function (filter->rtp_srcpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_iterate_internal_links_rtp));

  gst_pad_set_element_private (filter->rtp_sinkpad, filter->rtp_srcpad);
  gst_pad_set_element_private (filter->rtp_srcpad, filter->rtp_sinkpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->rtp_sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->rtp_srcpad);


  filter->rtcp_sinkpad =
      gst_pad_new_from_static_template (&rtcp_sink_template, "rtcp_sink");
  gst_pad_set_event_function (filter->rtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_sink_event_rtcp));
  gst_pad_set_query_function (filter->rtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_sink_query_rtcp));
  gst_pad_set_iterate_internal_links_function (filter->rtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_iterate_internal_links_rtcp));
  gst_pad_set_chain_function (filter->rtcp_sinkpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_chain_rtcp));

  filter->rtcp_srcpad =
      gst_pad_new_from_static_template (&rtcp_src_template, "rtcp_src");
  gst_pad_set_iterate_internal_links_function (filter->rtcp_srcpad,
      GST_DEBUG_FUNCPTR (gst_srtp_dec_iterate_internal_links_rtcp));

  gst_pad_set_element_private (filter->rtcp_sinkpad, filter->rtcp_srcpad);
  gst_pad_set_element_private (filter->rtcp_srcpad, filter->rtcp_sinkpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->rtcp_sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->rtcp_srcpad);

  filter->first_session = TRUE;
}

static GstStructure *
gst_srtp_dec_create_stats (GstSrtpDec * filter)
{
  GstStructure *s;
  GValue va = G_VALUE_INIT;
  GValue v = G_VALUE_INIT;

  s = gst_structure_new_empty ("application/x-srtp-decoder-stats");

  g_value_init (&va, GST_TYPE_ARRAY);
  g_value_init (&v, GST_TYPE_STRUCTURE);

  if (filter->session) {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, filter->streams);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
      GstSrtpDecSsrcStream *stream = value;
      GstStructure *ss;
      guint32 ssrc = GPOINTER_TO_UINT (key);
      srtp_err_status_t status;
      guint32 roc;

      status = srtp_get_stream_roc (filter->session, ssrc, &roc);
      if (status != srtp_err_status_ok) {
        continue;
      }

      ss = gst_structure_new ("application/x-srtp-stream",
          "ssrc", G_TYPE_UINT, ssrc, "roc", G_TYPE_UINT, roc, "recv-count",
          G_TYPE_UINT, stream->recv_count, "recv-drop-count", G_TYPE_UINT,
          stream->recv_drop_count, NULL);

      g_value_take_boxed (&v, ss);
      gst_value_array_append_value (&va, &v);
    }
  }

  gst_structure_take_value (s, "streams", &va);
  gst_structure_set (s, "recv-count", G_TYPE_UINT, filter->recv_count, NULL);
  gst_structure_set (s, "recv-drop-count", G_TYPE_UINT,
      filter->recv_drop_count, NULL);
  GST_LOG_OBJECT (filter, "stats: recv-count %u recv-drop-count %u",
      filter->recv_count, filter->recv_drop_count);
  g_value_unset (&v);

  return s;
}

static void
gst_srtp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSrtpDec *filter = GST_SRTP_DEC (object);

  GST_OBJECT_LOCK (filter);

  switch (prop_id) {
    case PROP_REPLAY_WINDOW_SIZE:
      filter->replay_window_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (filter);
}

static void
gst_srtp_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSrtpDec *filter = GST_SRTP_DEC (object);

  GST_OBJECT_LOCK (filter);

  switch (prop_id) {
    case PROP_REPLAY_WINDOW_SIZE:
      g_value_set_uint (value, filter->replay_window_size);
      break;
    case PROP_STATS:
      g_value_take_boxed (value, gst_srtp_dec_create_stats (filter));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (filter);
}

static void
gst_srtp_dec_remove_stream (GstSrtpDec * filter, guint ssrc)
{
  GstSrtpDecSsrcStream *stream = NULL;

  if (filter->streams == NULL)
    return;

  stream = g_hash_table_lookup (filter->streams, GUINT_TO_POINTER (ssrc));

  if (stream) {
    srtp_remove_stream (filter->session, ssrc);
    g_hash_table_remove (filter->streams, GUINT_TO_POINTER (ssrc));
  }
}

static GstSrtpDecSsrcStream *
find_stream_by_ssrc (GstSrtpDec * filter, guint32 ssrc)
{
  return g_hash_table_lookup (filter->streams, GUINT_TO_POINTER (ssrc));
}

#ifdef HAVE_SRTP2
static void
clear_key (gpointer data)
{
  struct GstSrtpDecKey *key = data;

  gst_clear_buffer (&key->mki);
  gst_clear_buffer (&key->key);
}
#endif


/* get info from buffer caps
 */
static GstSrtpDecSsrcStream *
get_stream_from_caps (GstSrtpDec * filter, GstCaps * caps, guint32 ssrc)
{
  GstSrtpDecSsrcStream *stream;
  GstStructure *s;
  GstBuffer *buf;
  const gchar *rtp_cipher, *rtp_auth, *rtcp_cipher, *rtcp_auth;

  /* Create new stream structure and set default values */
  stream = g_new0 (GstSrtpDecSsrcStream, 1);
  stream->ssrc = ssrc;
  stream->key = NULL;

  /* Get info from caps */
  s = gst_caps_get_structure (caps, 0);
  if (!s)
    goto error;

  rtp_cipher = gst_structure_get_string (s, "srtp-cipher");
  rtp_auth = gst_structure_get_string (s, "srtp-auth");
  rtcp_cipher = gst_structure_get_string (s, "srtcp-cipher");
  rtcp_auth = gst_structure_get_string (s, "srtcp-auth");
  if (!rtp_cipher || !rtp_auth || !rtcp_cipher || !rtcp_auth)
    goto error;

  gst_structure_get_uint (s, "roc", &stream->roc);

  stream->rtp_cipher = enum_value_from_nick (GST_TYPE_SRTP_CIPHER_TYPE,
      rtp_cipher);
  stream->rtp_auth = enum_value_from_nick (GST_TYPE_SRTP_AUTH_TYPE, rtp_auth);
  stream->rtcp_cipher = enum_value_from_nick (GST_TYPE_SRTP_CIPHER_TYPE,
      rtcp_cipher);
  stream->rtcp_auth = enum_value_from_nick (GST_TYPE_SRTP_AUTH_TYPE, rtcp_auth);

  if ((gint) stream->rtp_cipher == -1 || (gint) stream->rtp_auth == -1 ||
      (gint) stream->rtcp_cipher == -1 || (gint) stream->rtcp_auth == -1) {
    GST_WARNING_OBJECT (filter, "Invalid caps for stream,"
        " unknown cipher or auth type");
    goto error;
  }

  /* RFC 3711 says in "3. SRTP Framework" that SRTCP message authentication
   * is MANDATORY. In case of GCM let the pipeline handle any errors.
   */
  if (stream->rtcp_cipher != GST_SRTP_CIPHER_AES_128_GCM
      && stream->rtcp_cipher != GST_SRTP_CIPHER_AES_256_GCM
      && stream->rtcp_cipher != GST_SRTP_CIPHER_NULL
      && stream->rtcp_auth == GST_SRTP_AUTH_NULL) {
    GST_WARNING_OBJECT (filter,
        "Cannot have SRTP NULL authentication with a not-NULL encryption"
        " cipher.");
    goto error;
  }

  if (gst_structure_get (s, "srtp-key", GST_TYPE_BUFFER, &buf, NULL) && buf) {
#ifdef HAVE_SRTP2
    GstBuffer *mki = NULL;
    guint i;
    gsize mki_size = 0;
#endif

    GST_DEBUG_OBJECT (filter, "Got key [%p] for SSRC %u", buf, ssrc);

#ifdef HAVE_SRTP2
    if (gst_structure_get (s, "mki", GST_TYPE_BUFFER, &mki, NULL) && mki) {
      struct GstSrtpDecKey key = {.mki = mki,.key = buf };

      mki_size = gst_buffer_get_size (mki);
      if (mki_size > SRTP_MAX_MKI_LEN) {
        GST_WARNING_OBJECT (filter, "MKI is longer than allowed (%"
            G_GSIZE_FORMAT " > %d).", mki_size, SRTP_MAX_MKI_LEN);
        gst_buffer_unref (mki);
        gst_buffer_unref (buf);
        goto error;
      }

      stream->keys =
          g_array_sized_new (FALSE, TRUE, sizeof (struct GstSrtpDecKey), 2);
      g_array_set_clear_func (stream->keys, clear_key);

      g_array_append_val (stream->keys, key);

      /* Append more MKIs */
      for (i = 1; i < SRTP_MAX_NUM_MASTER_KEYS; i++) {
        char mki_id[16];
        char key_id[16];
        g_snprintf (mki_id, 16, "mki%d", i + 1);
        g_snprintf (key_id, 16, "srtp-key%d", i + 1);

        if (gst_structure_get (s, mki_id, GST_TYPE_BUFFER, &mki,
                key_id, GST_TYPE_BUFFER, &buf, NULL)) {
          if (gst_buffer_get_size (mki) != mki_size) {
            GST_WARNING_OBJECT (filter,
                "MKIs need to all have the same size (first was %"
                G_GSIZE_FORMAT ", current is %" G_GSIZE_FORMAT ").",
                mki_size, gst_buffer_get_size (mki));
            gst_buffer_unref (mki);
            gst_buffer_unref (buf);
            goto error;
          }
          key.mki = mki;
          key.key = buf;
          g_array_append_val (stream->keys, key);
        } else {
          break;
        }
      }
    } else
#endif
    {
      stream->key = buf;
    }
  } else if (STREAM_HAS_CRYPTO (stream)) {
    goto error;
  }

  return stream;

error:
  g_free (stream);
  return NULL;
}

/* Get SRTP params by signal
 */
static GstCaps *
signal_get_srtp_params (GstSrtpDec * filter, guint32 ssrc, gint signal)
{
  GstCaps *caps = NULL;

  g_signal_emit (filter, gst_srtp_dec_signals[signal], 0, ssrc, &caps);

  if (caps != NULL)
    GST_DEBUG_OBJECT (filter, "Caps received");

  return caps;
}

/* Create a stream in the session
 */
static srtp_err_status_t
init_session_stream (GstSrtpDec * filter, guint32 ssrc,
    GstSrtpDecSsrcStream * stream)
{
  srtp_err_status_t ret;
  srtp_policy_t policy;
  GstMapInfo map;
  guchar tmp[1];
#ifdef HAVE_SRTP2
  GstMapInfo *key_maps = NULL;
  GstMapInfo *mki_maps = NULL;
#endif

  memset (&policy, 0, sizeof (srtp_policy_t));

  if (!stream)
    return srtp_err_status_bad_param;

  GST_INFO_OBJECT (filter, "Setting RTP policy...");
  set_crypto_policy_cipher_auth (stream->rtp_cipher, stream->rtp_auth,
      &policy.rtp);
  GST_INFO_OBJECT (filter, "Setting RTCP policy...");
  set_crypto_policy_cipher_auth (stream->rtcp_cipher, stream->rtcp_auth,
      &policy.rtcp);

#ifdef HAVE_SRTP2
  if (stream->keys) {
    guint i;
    srtp_master_key_t *keys;

    keys = g_alloca (sizeof (srtp_master_key_t) * stream->keys->len);
    policy.keys = g_alloca (sizeof (gpointer) * stream->keys->len);
    key_maps = g_alloca (sizeof (GstMapInfo) * stream->keys->len);
    mki_maps = g_alloca (sizeof (GstMapInfo) * stream->keys->len);

    for (i = 0; i < stream->keys->len; i++) {
      struct GstSrtpDecKey *key =
          &g_array_index (stream->keys, struct GstSrtpDecKey, i);
      policy.keys[i] = &keys[i];

      gst_buffer_map (key->mki, &mki_maps[i], GST_MAP_READ);
      gst_buffer_map (key->key, &key_maps[i], GST_MAP_READ);

      policy.keys[i]->key = (guchar *) key_maps[i].data;
      policy.keys[i]->mki_id = (guchar *) mki_maps[i].data;
      policy.keys[i]->mki_size = mki_maps[i].size;
    }
    policy.num_master_keys = stream->keys->len;
  } else
#endif
  if (stream->key) {
    gst_buffer_map (stream->key, &map, GST_MAP_READ);
    policy.key = (guchar *) map.data;
  } else {
    policy.key = tmp;
  }

  policy.ssrc.value = ssrc;
  policy.ssrc.type = ssrc_specific;
  policy.window_size = filter->replay_window_size;
  policy.next = NULL;

  /* If it is the first stream, create the session
   * If not, add the stream policy to the session
   */
  if (filter->first_session)
    ret = srtp_create (&filter->session, &policy);
  else
    ret = srtp_add_stream (filter->session, &policy);

  if (stream->key)
    gst_buffer_unmap (stream->key, &map);

#ifdef HAVE_SRTP2
  if (key_maps) {
    guint i;

    for (i = 0; i < stream->keys->len; i++) {
      struct GstSrtpDecKey *key = &g_array_index (stream->keys,
          struct GstSrtpDecKey, i);
      gst_buffer_unmap (key->mki, &mki_maps[i]);
      gst_buffer_unmap (key->key, &key_maps[i]);
    }

  }
#endif

  if (ret == srtp_err_status_ok) {
    srtp_err_status_t status;

    status = srtp_set_stream_roc (filter->session, ssrc, stream->roc);
#ifdef HAVE_SRTP2
    (void) status;              /* Ignore unused variable */
#else
    if (status == srtp_err_status_ok) {
      /* Here, we just set the ROC, but we also need to set the initial
       * RTP sequence number later, otherwise libsrtp will not be able
       * to get the right packet index. */
      g_hash_table_add (filter->streams_roc_changed, GUINT_TO_POINTER (ssrc));
    }
#endif

    filter->first_session = FALSE;
    g_hash_table_insert (filter->streams, GUINT_TO_POINTER (stream->ssrc),
        stream);
  }

  return ret;
}

/* Return a stream structure for a given buffer
 */
static GstSrtpDecSsrcStream *
validate_buffer (GstSrtpDec * filter, GstBuffer * buf, guint32 * ssrc,
    gboolean * is_rtcp)
{
  GstSrtpDecSsrcStream *stream = NULL;
  GstRTPBuffer rtpbuf = GST_RTP_BUFFER_INIT;

  if (gst_rtp_buffer_map (buf,
          GST_MAP_READ | GST_RTP_BUFFER_MAP_FLAG_SKIP_PADDING, &rtpbuf)) {
    if (gst_rtp_buffer_get_payload_type (&rtpbuf) < 64
        || gst_rtp_buffer_get_payload_type (&rtpbuf) > 80) {
      *ssrc = gst_rtp_buffer_get_ssrc (&rtpbuf);

      gst_rtp_buffer_unmap (&rtpbuf);
      *is_rtcp = FALSE;
      goto have_ssrc;
    }
    gst_rtp_buffer_unmap (&rtpbuf);
  }

  if (rtcp_buffer_get_ssrc (buf, ssrc)) {
    *is_rtcp = TRUE;
  } else {
    GST_WARNING_OBJECT (filter, "No SSRC found in buffer");
    return NULL;
  }

have_ssrc:

  stream = find_stream_by_ssrc (filter, *ssrc);

  if (stream)
    return stream;

  return request_key_with_signal (filter, *ssrc, SIGNAL_REQUEST_KEY);
}

static void
free_stream (GstSrtpDecSsrcStream * stream)
{
  if (stream->key)
    gst_buffer_unref (stream->key);
  if (stream->keys)
    g_array_free (stream->keys, TRUE);
  g_free (stream);
}

static gboolean
buffers_are_equal (GstBuffer * a, GstBuffer * b)
{
  GstMapInfo info;

  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (gst_buffer_get_size (a) != gst_buffer_get_size (b))
    return FALSE;

  if (gst_buffer_map (a, &info, GST_MAP_READ)) {
    gboolean equal;

    equal = (gst_buffer_memcmp (b, 0, info.data, info.size) == 0);
    gst_buffer_unmap (a, &info);

    return equal;
  } else {
    return FALSE;
  }
}

static gboolean
keys_are_equal (GArray * a, GArray * b)
{
#ifdef HAVE_SRTP2
  guint i;

  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (a->len != b->len)
    return FALSE;

  for (i = 0; i < a->len; i++) {
    struct GstSrtpDecKey *key_a = &g_array_index (a,
        struct GstSrtpDecKey, i);
    struct GstSrtpDecKey *key_b = &g_array_index (b,
        struct GstSrtpDecKey, i);

    if (!buffers_are_equal (key_a->mki, key_b->mki))
      return FALSE;

    if (!buffers_are_equal (key_a->key, key_b->key))
      return FALSE;
  }

  return TRUE;
#else
  return FALSE;
#endif
}

/* Create new stream from params in caps
 */
static GstSrtpDecSsrcStream *
update_session_stream_from_caps (GstSrtpDec * filter, guint32 ssrc,
    GstCaps * caps)
{
  GstSrtpDecSsrcStream *stream = NULL;
  GstSrtpDecSsrcStream *old_stream = NULL;
  srtp_err_status_t err;

  g_return_val_if_fail (GST_IS_SRTP_DEC (filter), NULL);
  g_return_val_if_fail (GST_IS_CAPS (caps), NULL);

  stream = get_stream_from_caps (filter, caps, ssrc);

  old_stream = find_stream_by_ssrc (filter, ssrc);
  if (stream && old_stream &&
      stream->rtp_cipher == old_stream->rtp_cipher &&
      stream->rtcp_cipher == old_stream->rtcp_cipher &&
      stream->rtp_auth == old_stream->rtp_auth &&
      stream->rtcp_auth == old_stream->rtcp_auth &&
      ((stream->keys && keys_are_equal (stream->keys, old_stream->keys)) ||
          buffers_are_equal (stream->key, old_stream->key))) {
    free_stream (stream);
    return old_stream;
  }

  /* Remove existing stream, if any */
  gst_srtp_dec_remove_stream (filter, ssrc);

  if (stream) {
    /* Create new session stream */
    err = init_session_stream (filter, ssrc, stream);

    if (err != srtp_err_status_ok) {
      GST_WARNING_OBJECT (filter, "Failed to create the stream (err: %d)", err);
      if (stream->key)
        gst_buffer_unref (stream->key);
      g_free (stream);
      stream = NULL;
    }
  }

  return stream;
}

static gboolean
remove_yes (gpointer key, gpointer value, gpointer user_data)
{
  return TRUE;
}

/* Clear the policy list
 */
static void
gst_srtp_dec_clear_streams (GstSrtpDec * filter)
{
  guint nb = 0;

  GST_OBJECT_LOCK (filter);

  if (!filter->first_session) {
    srtp_dealloc (filter->session);
    filter->session = NULL;
  }

  if (filter->streams)
    nb = g_hash_table_foreach_remove (filter->streams, remove_yes, NULL);

  filter->first_session = TRUE;

  GST_OBJECT_UNLOCK (filter);

  GST_DEBUG_OBJECT (filter, "Cleared %d streams", nb);
}

/* Send a signal
 */
static GstSrtpDecSsrcStream *
request_key_with_signal (GstSrtpDec * filter, guint32 ssrc, gint signal)
{
  GstCaps *caps;
  GstSrtpDecSsrcStream *stream = NULL;

  caps = signal_get_srtp_params (filter, ssrc, signal);

  if (caps) {
    stream = update_session_stream_from_caps (filter, ssrc, caps);
    if (stream)
      GST_DEBUG_OBJECT (filter, "New stream set with SSRC %u", ssrc);
    else
      GST_WARNING_OBJECT (filter, "Could not set stream with SSRC %u", ssrc);
    gst_caps_unref (caps);
  } else {
    GST_WARNING_OBJECT (filter, "Could not get caps for stream with SSRC %u",
        ssrc);
  }

  return stream;
}

static gboolean
gst_srtp_dec_sink_setcaps (GstPad * pad, GstObject * parent,
    GstCaps * caps, gboolean is_rtcp)
{
  GstSrtpDec *filter = GST_SRTP_DEC (parent);
  GstPad *otherpad;
  GstStructure *ps;
  gboolean ret = FALSE;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  ps = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_field_typed (ps, "ssrc", G_TYPE_UINT) &&
      gst_structure_has_field_typed (ps, "srtp-cipher", G_TYPE_STRING) &&
      gst_structure_has_field_typed (ps, "srtp-auth", G_TYPE_STRING) &&
      gst_structure_has_field_typed (ps, "srtcp-cipher", G_TYPE_STRING) &&
      gst_structure_has_field_typed (ps, "srtcp-auth", G_TYPE_STRING)) {
    guint ssrc;

    gst_structure_get_uint (ps, "ssrc", &ssrc);

    if (!update_session_stream_from_caps (filter, ssrc, caps)) {
      GST_WARNING_OBJECT (pad, "Could not create session from pad caps: %"
          GST_PTR_FORMAT, caps);
      return FALSE;
    }
  }

  caps = gst_caps_copy (caps);
  ps = gst_caps_get_structure (caps, 0);
  gst_structure_remove_fields (ps, "srtp-key", "srtp-cipher", "srtp-auth",
      "srtcp-cipher", "srtcp-auth", "mki", NULL);

  if (is_rtcp)
    gst_structure_set_name (ps, "application/x-rtcp");
  else
    gst_structure_set_name (ps, "application/x-rtp");

  otherpad = gst_pad_get_element_private (pad);

  ret = gst_pad_set_caps (otherpad, caps);

  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_srtp_dec_sink_event_rtp (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret;
  GstCaps *caps;
  GstSrtpDec *filter = GST_SRTP_DEC (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);
      ret = gst_srtp_dec_sink_setcaps (pad, parent, caps, FALSE);
      gst_event_unref (event);
      return ret;
    case GST_EVENT_SEGMENT:
      /* Make sure to send a caps event downstream before the segment event,
       * even if upstream didn't */
      if (!gst_pad_has_current_caps (filter->rtp_srcpad)) {
        GstCaps *caps = gst_caps_new_empty_simple ("application/x-rtp");

        gst_pad_set_caps (filter->rtp_srcpad, caps);
        gst_caps_unref (caps);
      }
      filter->rtp_has_segment = TRUE;
      break;
    case GST_EVENT_FLUSH_STOP:
      filter->rtp_has_segment = FALSE;
      break;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_srtp_dec_sink_event_rtcp (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret;
  GstCaps *caps;
  GstSrtpDec *filter = GST_SRTP_DEC (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);
      ret = gst_srtp_dec_sink_setcaps (pad, parent, caps, TRUE);
      gst_event_unref (event);
      return ret;
    case GST_EVENT_SEGMENT:
      /* Make sure to send a caps event downstream before the segment event,
       * even if upstream didn't */
      if (!gst_pad_has_current_caps (filter->rtcp_srcpad)) {
        GstCaps *caps = gst_caps_new_empty_simple ("application/x-rtcp");

        gst_pad_set_caps (filter->rtcp_srcpad, caps);
        gst_caps_unref (caps);
      }
      filter->rtcp_has_segment = TRUE;
      break;
    case GST_EVENT_FLUSH_STOP:
      filter->rtcp_has_segment = FALSE;
      break;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_srtp_dec_sink_query (GstPad * pad, GstObject * parent, GstQuery * query,
    gboolean is_rtcp)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter = NULL;
      GstCaps *other_filter = NULL;
      GstCaps *template_caps;
      GstPad *otherpad;
      GstCaps *other_caps;
      GstCaps *ret;
      int i;

      gst_query_parse_caps (query, &filter);

      otherpad = (GstPad *) gst_pad_get_element_private (pad);

      if (filter) {
        other_filter = gst_caps_copy (filter);

        for (i = 0; i < gst_caps_get_size (other_filter); i++) {
          GstStructure *ps = gst_caps_get_structure (other_filter, i);
          if (is_rtcp)
            gst_structure_set_name (ps, "application/x-rtcp");
          else
            gst_structure_set_name (ps, "application/x-rtp");
          gst_structure_remove_fields (ps, "srtp-key", "srtp-cipher",
              "srtp-auth", "srtcp-cipher", "srtcp-auth", "mki", NULL);
        }
      }


      other_caps = gst_pad_peer_query_caps (otherpad, other_filter);
      if (other_filter)
        gst_caps_unref (other_filter);
      if (!other_caps) {
        goto return_template;
      }

      template_caps = gst_pad_get_pad_template_caps (otherpad);
      ret = gst_caps_intersect_full (other_caps, template_caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (other_caps);
      gst_caps_unref (template_caps);

      ret = gst_caps_make_writable (ret);

      for (i = 0; i < gst_caps_get_size (ret); i++) {
        GstStructure *ps = gst_caps_get_structure (ret, i);
        if (is_rtcp)
          gst_structure_set_name (ps, "application/x-srtcp");
        else
          gst_structure_set_name (ps, "application/x-srtp");
      }

      if (filter) {
        GstCaps *tmp;

        tmp = gst_caps_intersect (ret, filter);
        gst_caps_unref (ret);
        ret = tmp;
      }

      gst_query_set_caps_result (query, ret);
      gst_caps_unref (ret);
      return TRUE;

    return_template:

      ret = gst_pad_get_pad_template_caps (pad);
      gst_query_set_caps_result (query, ret);
      gst_caps_unref (ret);
      return TRUE;
    }
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static gboolean
gst_srtp_dec_sink_query_rtp (GstPad * pad, GstObject * parent, GstQuery * query)
{
  return gst_srtp_dec_sink_query (pad, parent, query, FALSE);
}

static gboolean
gst_srtp_dec_sink_query_rtcp (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  return gst_srtp_dec_sink_query (pad, parent, query, TRUE);
}

static GstIterator *
gst_srtp_dec_iterate_internal_links (GstPad * pad, GstObject * parent,
    gboolean is_rtcp)
{
  GstSrtpDec *filter = GST_SRTP_DEC (parent);
  GstPad *otherpad = NULL;
  GstIterator *it = NULL;

  otherpad = (GstPad *) gst_pad_get_element_private (pad);

  if (otherpad) {
    GValue val = { 0 };

    g_value_init (&val, GST_TYPE_PAD);
    g_value_set_object (&val, otherpad);
    it = gst_iterator_new_single (GST_TYPE_PAD, &val);
    g_value_unset (&val);
  } else {
    GST_ELEMENT_ERROR (GST_ELEMENT_CAST (filter), CORE, PAD, (NULL),
        ("Unable to get linked pad"));
  }

  return it;
}

static GstIterator *
gst_srtp_dec_iterate_internal_links_rtp (GstPad * pad, GstObject * parent)
{
  return gst_srtp_dec_iterate_internal_links (pad, parent, FALSE);
}

static GstIterator *
gst_srtp_dec_iterate_internal_links_rtcp (GstPad * pad, GstObject * parent)
{
  return gst_srtp_dec_iterate_internal_links (pad, parent, TRUE);
}

static gboolean
gst_srtp_dec_push_early_events (GstSrtpDec * filter, GstPad * pad,
    GstPad * otherpad, gboolean is_rtcp)
{
  GstEvent *otherev, *ev;

  ev = gst_pad_get_sticky_event (pad, GST_EVENT_STREAM_START, 0);
  if (ev) {
    gst_event_unref (ev);
  } else {
    gchar *new_stream_id;

    otherev = gst_pad_get_sticky_event (otherpad, GST_EVENT_STREAM_START, 0);

    if (otherev) {
      const gchar *other_stream_id;

      gst_event_parse_stream_start (otherev, &other_stream_id);

      new_stream_id = g_strdup_printf ("%s/%s", other_stream_id,
          is_rtcp ? "rtcp" : "rtp");
      gst_event_unref (otherev);
    } else {
      new_stream_id = gst_element_decorate_stream_id (GST_ELEMENT (filter),
          is_rtcp ? "rtcp" : "rtp");
    }

    ev = gst_event_new_stream_start (new_stream_id);
    g_free (new_stream_id);

    gst_pad_push_event (pad, ev);
  }

  ev = gst_pad_get_sticky_event (pad, GST_EVENT_CAPS, 0);
  if (ev) {
    gst_event_unref (ev);
  } else {
    GstCaps *caps;

    if (is_rtcp)
      caps = gst_caps_new_empty_simple ("application/x-rtcp");
    else
      caps = gst_caps_new_empty_simple ("application/x-rtp");

    ev = gst_event_new_caps (caps);
    gst_pad_push_event (pad, ev);
    gst_caps_unref (caps);
  }

  ev = gst_pad_get_sticky_event (pad, GST_EVENT_SEGMENT, 0);
  if (ev) {
    gst_event_unref (ev);
  } else {
    ev = gst_pad_get_sticky_event (otherpad, GST_EVENT_SEGMENT, 0);

    if (ev) {
      gst_pad_push_event (pad, ev);
    } else if (GST_PAD_IS_FLUSHING (otherpad)) {
      /* We didn't get a Segment event from otherpad
       * and otherpad is flushing => we are most likely shutting down */
      goto err;
    } else {
      GST_WARNING_OBJECT (filter, "No Segment event to push");
      goto err;
    }
  }

  if (is_rtcp)
    filter->rtcp_has_segment = TRUE;
  else
    filter->rtp_has_segment = TRUE;

  return TRUE;

err:
  return FALSE;
}

/*
 * This function should be called while holding the filter lock
 */
static gboolean
gst_srtp_dec_decode_buffer (GstSrtpDec * filter, GstPad * pad, GstBuffer * buf,
    gboolean is_rtcp, guint32 ssrc)
{
  GstMapInfo map;
  srtp_err_status_t err;
  gint size;
  GstSrtpDecSsrcStream *stream;

  GST_LOG_OBJECT (pad, "Received %s buffer of size %" G_GSIZE_FORMAT
      " with SSRC = %u", is_rtcp ? "RTCP" : "RTP", gst_buffer_get_size (buf),
      ssrc);
  filter->recv_count++;
  /* Change buffer to remove protection */
  buf = gst_buffer_make_writable (buf);

  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  size = map.size;

unprotect:

  gst_srtp_init_event_reporter ();

  if (is_rtcp) {
#ifdef HAVE_SRTP2
    stream = find_stream_by_ssrc (filter, ssrc);

    err = srtp_unprotect_rtcp_mki (filter->session, map.data, &size,
        stream && stream->keys);
#else
    err = srtp_unprotect_rtcp (filter->session, map.data, &size);
#endif
  } else {
#ifndef HAVE_SRTP2
    /* If ROC has changed, we know we need to set the initial RTP
     * sequence number too. */
    if (g_hash_table_contains (filter->streams_roc_changed,
            GUINT_TO_POINTER (ssrc))) {
      srtp_stream_t stream;

      stream = srtp_get_stream (filter->session, htonl (ssrc));

      if (stream) {
        guint16 seqnum = 0;
        GstRTPBuffer rtpbuf = GST_RTP_BUFFER_INIT;

        gst_rtp_buffer_map (buf,
            GST_MAP_READ | GST_RTP_BUFFER_MAP_FLAG_SKIP_PADDING, &rtpbuf);
        seqnum = gst_rtp_buffer_get_seq (&rtpbuf);
        gst_rtp_buffer_unmap (&rtpbuf);

        /* We finally add the RTP sequence number to the current
         * rollover counter. */
        stream->rtp_rdbx.index &= ~0xFFFF;
        stream->rtp_rdbx.index |= seqnum;
      }

      g_hash_table_remove (filter->streams_roc_changed,
          GUINT_TO_POINTER (ssrc));
    }
#endif

#ifdef HAVE_SRTP2
    {
      stream = find_stream_by_ssrc (filter, ssrc);

      err = srtp_unprotect_mki (filter->session, map.data, &size,
          stream && stream->keys);
    }
#else
    err = srtp_unprotect (filter->session, map.data, &size);
#endif
  }
  stream = find_stream_by_ssrc (filter, ssrc);
  if (stream == NULL) {
    GST_WARNING_OBJECT (filter, "Could not find matching stream, dropping");
    goto err;
  }
  stream->recv_count++;
  /* Signal user depending on type of error */
  switch (err) {
    case srtp_err_status_ok:
      /* success! */
      break;
    case srtp_err_status_replay_fail:
      GST_DEBUG_OBJECT (filter,
          "Dropping replayed packet, probably retransmission");
      stream->recv_drop_count++;
      goto err;
    case srtp_err_status_replay_old:
      GST_DEBUG_OBJECT (filter,
          "Dropping replayed old packet, probably retransmission");
      stream->recv_drop_count++;
      goto err;
    case srtp_err_status_key_expired:{

      GST_OBJECT_UNLOCK (filter);
      stream = request_key_with_signal (filter, ssrc, SIGNAL_HARD_LIMIT);
      GST_OBJECT_LOCK (filter);

      /* Check the key request created a new stream */
      if (stream == NULL) {
        GST_WARNING_OBJECT (filter, "Hard limit reached, no new key, dropping");
        goto err;
      }

      goto unprotect;
    }
    case srtp_err_status_auth_fail:
      GST_WARNING_OBJECT (filter, "Error authentication packet, dropping");
      stream->recv_drop_count++;
      goto err;
    case srtp_err_status_cipher_fail:
      GST_WARNING_OBJECT (filter, "Error while decrypting packet, dropping");
      stream->recv_drop_count++;
      goto err;
    default:
      GST_WARNING_OBJECT (pad,
          "Unable to unprotect buffer (unprotect failed code %d)", err);
      stream->recv_drop_count++;
      goto err;
  }
  gst_buffer_unmap (buf, &map);
  gst_buffer_set_size (buf, size);
  return TRUE;

err:
  filter->recv_drop_count++;
  gst_buffer_unmap (buf, &map);
  return FALSE;
}

static GstFlowReturn
gst_srtp_dec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf,
    gboolean is_rtcp)
{
  GstSrtpDec *filter = GST_SRTP_DEC (parent);
  GstPad *otherpad;
  GstSrtpDecSsrcStream *stream = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 ssrc = 0;

  GST_OBJECT_LOCK (filter);

  /* Check if this stream exists, if not create a new stream */

  if (!(stream = validate_buffer (filter, buf, &ssrc, &is_rtcp))) {
    GST_OBJECT_UNLOCK (filter);
    GST_WARNING_OBJECT (filter, "Invalid buffer, dropping");
    goto drop_buffer;
  }

  if (!STREAM_HAS_CRYPTO (stream)) {
    GST_OBJECT_UNLOCK (filter);
    goto push_out;
  }

  if (!gst_srtp_dec_decode_buffer (filter, pad, buf, is_rtcp, ssrc)) {
    GST_OBJECT_UNLOCK (filter);
    goto drop_buffer;
  }

  GST_OBJECT_UNLOCK (filter);

  /* If all is well, we may have reached soft limit */
  if (gst_srtp_get_soft_limit_reached ())
    request_key_with_signal (filter, ssrc, SIGNAL_SOFT_LIMIT);

push_out:
  /* Push buffer to source pad */
  if (is_rtcp) {
    otherpad = filter->rtcp_srcpad;
    if (!filter->rtcp_has_segment) {
      if (!gst_srtp_dec_push_early_events (filter, filter->rtcp_srcpad,
              filter->rtp_srcpad, TRUE)) {
        ret = GST_FLOW_FLUSHING;
        goto drop_buffer;
      }
    }
  } else {
    otherpad = filter->rtp_srcpad;
    if (!filter->rtp_has_segment) {
      if (!gst_srtp_dec_push_early_events (filter, filter->rtp_srcpad,
              filter->rtcp_srcpad, FALSE)) {
        ret = GST_FLOW_FLUSHING;
        goto drop_buffer;
      }
    }
  }

  ret = gst_pad_push (otherpad, buf);

  return ret;

drop_buffer:
  /* Drop buffer, except if gst_pad_push returned OK or an error */

  gst_buffer_unref (buf);

  return ret;
}

static GstFlowReturn
gst_srtp_dec_chain_rtp (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  return gst_srtp_dec_chain (pad, parent, buf, FALSE);
}

static GstFlowReturn
gst_srtp_dec_chain_rtcp (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  return gst_srtp_dec_chain (pad, parent, buf, TRUE);
}

static GstStateChangeReturn
gst_srtp_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn res;
  GstSrtpDec *filter;

  filter = GST_SRTP_DEC (element);
  GST_OBJECT_LOCK (filter);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      filter->streams = g_hash_table_new_full (g_direct_hash, g_direct_equal,
          NULL, (GDestroyNotify) free_stream);

#ifndef HAVE_SRTP2
      filter->streams_roc_changed =
          g_hash_table_new (g_direct_hash, g_direct_equal);
#endif

      filter->rtp_has_segment = FALSE;
      filter->rtcp_has_segment = FALSE;
      filter->recv_count = 0;
      filter->recv_drop_count = 0;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  GST_OBJECT_UNLOCK (filter);

  res = GST_ELEMENT_CLASS (gst_srtp_dec_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_srtp_dec_clear_streams (filter);
      g_hash_table_unref (filter->streams);
      filter->streams = NULL;
#ifndef HAVE_SRTP2
      g_hash_table_unref (filter->streams_roc_changed);
      filter->streams_roc_changed = NULL;
#endif

      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return res;
}
