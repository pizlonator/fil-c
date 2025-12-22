/* GStreamer
 * Copyright (C) 2011 Axis Communications <dev-gstreamer@axis.com>
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
 * SECTION:element-curlsink
 * @title: curlsink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl as a client to upload data to
 * an SMTP server.
 *
 * ## Example launch line
 *
 * Upload a JPEG file to an SMTP server.
 *
 * |[
 * gst-launch-1.0 filesrc location=image.jpg ! jpegparse ! curlsmtpsink  \
 *     file-name=image.jpg  \
 *     location=smtp://smtp.gmail.com:507 \
 *     user=test passwd=test  \
 *     subject=my image \
 *     mail-from="me@gmail.com" \
 *     mail-rcpt="you@gmail.com,she@gmail.com" \
 *     use-ssl=TRUE  \
 *     insecure=TRUE
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#if HAVE_PWD_H
#include <pwd.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <unistd.h>
#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>

#include "gstcurlelements.h"
#include "gstcurltlssink.h"
#include "gstcurlsmtpsink.h"

/* Default values */
#define GST_CAT_DEFAULT                gst_curl_smtp_sink_debug
#define DEFAULT_USE_SSL                FALSE
#define DEFAULT_NBR_ATTACHMENTS        1

/* MIME definitions */
#define MIME_VERSION                   "MIME-version: 1.0"
#define BOUNDARY_STRING                "curlsink-boundary"
#define BOUNDARY_STRING_END            "--curlsink-boundary--"

#define MAIL_RCPT_DELIMITER            ","

/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_smtp_sink_debug);

enum
{
  PROP_0,
  PROP_MAIL_RCPT,
  PROP_MAIL_FROM,
  PROP_SUBJECT,
  PROP_MESSAGE_BODY,
  PROP_POP_USER_NAME,
  PROP_POP_USER_PASSWD,
  PROP_POP_LOCATION,
  PROP_NBR_ATTACHMENTS,
  PROP_CONTENT_TYPE,
  PROP_USE_SSL
};


/* Object class function declarations */
static void gst_curl_smtp_sink_finalize (GObject * gobject);
static void gst_curl_smtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_smtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_curl_smtp_sink_set_payload_headers_unlocked (GstCurlBaseSink
    * sink);
static gboolean
gst_curl_smtp_sink_set_transfer_options_unlocked (GstCurlBaseSink * sink);
static void gst_curl_smtp_sink_set_mime_type (GstCurlBaseSink * bcsink,
    GstCaps * caps);
static gboolean gst_curl_smtp_sink_prepare_transfer (GstCurlBaseSink * bcsink);
static size_t gst_curl_smtp_sink_transfer_data_buffer (GstCurlBaseSink * sink,
    void *curl_ptr, size_t block_size, guint * last_chunk);
static size_t gst_curl_smtp_sink_flush_data_unlocked (GstCurlBaseSink * bcsink,
    void *curl_ptr, size_t block_size, gboolean new_file,
    gboolean close_transfer);

/* private functions */

static size_t transfer_payload_headers (GstCurlSmtpSink * sink, void *curl_ptr,
    size_t block_size);

#define gst_curl_smtp_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlSmtpSink, gst_curl_smtp_sink, GST_TYPE_CURL_TLS_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (curlsmtpsink, "curlsmtpsink",
    GST_RANK_NONE, GST_TYPE_CURL_SMTP_SINK, curl_element_init (plugin));

static void
gst_curl_smtp_sink_notify_transfer_end_unlocked (GstCurlSmtpSink * sink)
{
  GST_LOG ("transfer completed: %d", sink->transfer_end);
  sink->transfer_end = TRUE;
  g_cond_signal (&sink->cond_transfer_end);
}

static void
gst_curl_smtp_sink_wait_for_transfer_end_unlocked (GstCurlSmtpSink * sink)
{
  GST_LOG ("waiting for final data do be sent: %d", sink->transfer_end);

  while (!sink->transfer_end) {
    g_cond_wait (&sink->cond_transfer_end, GST_OBJECT_GET_LOCK (sink));
  }
  GST_LOG ("final data sent");
}

static void
add_final_boundary_unlocked (GstCurlSmtpSink * sink)
{
  GByteArray *array;
  gchar *boundary_end;
  gsize len;
  gint save, state;
  gchar *data_out;

  GST_DEBUG ("adding final boundary");

  array = sink->base64_chunk->chunk_array;
  g_assert (array);

  /* it will need up to 5 bytes if line-breaking is enabled
   * additional byte is needed for <CR> as it is not automatically added by
   * glib */
  data_out = g_malloc (6);
  save = sink->base64_chunk->save;
  state = sink->base64_chunk->state;
  len = g_base64_encode_close (TRUE, data_out, &state, &save);

  /* workaround */
  data_out[len - 1] = '\r';
  data_out[len] = '\n';

  /* +1 for CR */
  g_byte_array_append (array, (guint8 *) data_out, (guint) (len + 1));
  g_free (data_out);

  boundary_end = g_strdup_printf ("\r\n%s\r\n", BOUNDARY_STRING_END);
  g_byte_array_append (array, (guint8 *) boundary_end, strlen (boundary_end));
  g_free (boundary_end);

  sink->final_boundary_added = TRUE;
}

static gboolean
gst_curl_smtp_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstCurlBaseSink *bcsink = GST_CURL_BASE_SINK (bsink);
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bsink);

  switch (event->type) {
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (sink, "received EOS");
      gst_curl_base_sink_set_live (bcsink, FALSE);

      GST_OBJECT_LOCK (sink);
      sink->eos = TRUE;
      if (bcsink->flow_ret == GST_FLOW_OK && sink->base64_chunk != NULL
          && !sink->final_boundary_added) {
        add_final_boundary_unlocked (sink);
        gst_curl_base_sink_transfer_thread_notify_unlocked (bcsink);
        GST_FIXME_OBJECT (sink, "if gstpoll errors in transfer thread, then "
            "this wait will never timeout because the transfer thread does "
            "not signal it upon errors");
        gst_curl_smtp_sink_wait_for_transfer_end_unlocked (sink);
      }
      GST_OBJECT_UNLOCK (sink);
      break;

    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (bsink, event);
}

static gboolean
gst_curl_smtp_sink_has_buffered_data_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  Base64Chunk *chunk;
  GByteArray *array = NULL;
  gboolean ret = FALSE;

  chunk = sink->base64_chunk;

  if (chunk) {
    array = chunk->chunk_array;
    if (array)
      ret = (array->len == 0 && sink->final_boundary_added) ? FALSE : TRUE;
  }

  return ret;
}

static void
gst_curl_smtp_sink_class_init (GstCurlSmtpSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
  GstCurlBaseSinkClass *gstcurlbasesink_class = (GstCurlBaseSinkClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_smtp_sink_debug, "curlsmtpsink", 0,
      "curl smtp sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl smtp sink",
      "Sink/Network",
      "Upload data over SMTP protocol using libcurl",
      "Patricia Muscalu <patricia@axis.com>");

  gstcurlbasesink_class->set_protocol_dynamic_options_unlocked =
      gst_curl_smtp_sink_set_payload_headers_unlocked;
  gstcurlbasesink_class->set_options_unlocked =
      gst_curl_smtp_sink_set_transfer_options_unlocked;
  gstcurlbasesink_class->set_mime_type = gst_curl_smtp_sink_set_mime_type;
  gstcurlbasesink_class->prepare_transfer = gst_curl_smtp_sink_prepare_transfer;
  gstcurlbasesink_class->transfer_data_buffer =
      gst_curl_smtp_sink_transfer_data_buffer;
  gstcurlbasesink_class->flush_data_unlocked =
      gst_curl_smtp_sink_flush_data_unlocked;
  gstcurlbasesink_class->has_buffered_data_unlocked =
      gst_curl_smtp_sink_has_buffered_data_unlocked;

  gstbasesink_class->event = gst_curl_smtp_sink_event;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_smtp_sink_finalize);
  gobject_class->set_property = gst_curl_smtp_sink_set_property;
  gobject_class->get_property = gst_curl_smtp_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_MAIL_RCPT,
      g_param_spec_string ("mail-rcpt", "Mail recipient",
          "Single address that the given mail should get sent to", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAIL_FROM,
      g_param_spec_string ("mail-from", "Mail sender",
          "Single address that the given mail should get sent from", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONTENT_TYPE,
      g_param_spec_string ("content-type", "Content type",
          "The mime type of the body of the request", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SUBJECT,
      g_param_spec_string ("subject", "UTF-8 encoded mail subject",
          "Mail subject", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MESSAGE_BODY,
      g_param_spec_string ("message-body", "UTF-8 encoded message body",
          "Message body", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_SSL,
      g_param_spec_boolean ("use-ssl", "Use SSL",
          "Use SSL/TLS for the connection", DEFAULT_USE_SSL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_NBR_ATTACHMENTS,
      g_param_spec_int ("nbr-attachments", "Number attachments",
          "Number attachments to send", G_MININT, G_MAXINT,
          DEFAULT_NBR_ATTACHMENTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POP_USER_NAME,
      g_param_spec_string ("pop-user", "User name",
          "User name to use for POP server authentication", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POP_USER_PASSWD,
      g_param_spec_string ("pop-passwd", "User password",
          "User password to use for POP server authentication", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_POP_LOCATION,
      g_param_spec_string ("pop-location", "POP location",
          "URL POP used for authentication", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_curl_smtp_sink_init (GstCurlSmtpSink * sink)
{
  sink->curl_recipients = NULL;
  sink->mail_rcpt = NULL;
  sink->mail_from = NULL;
  sink->subject = NULL;
  sink->message_body = NULL;
  sink->payload_headers = NULL;
  sink->base64_chunk = NULL;

  g_cond_init (&sink->cond_transfer_end);
  sink->transfer_end = FALSE;
  sink->eos = FALSE;
  sink->final_boundary_added = FALSE;

  sink->reset_transfer_options = FALSE;
  sink->use_ssl = DEFAULT_USE_SSL;

  sink->pop_user = NULL;
  sink->pop_passwd = NULL;
  sink->pop_location = NULL;
  sink->pop_curl = NULL;
}

static void
gst_curl_smtp_sink_finalize (GObject * gobject)
{
  GstCurlSmtpSink *this = GST_CURL_SMTP_SINK (gobject);

  GST_DEBUG ("finalizing curlsmtpsink");

  if (this->curl_recipients != NULL) {
    curl_slist_free_all (this->curl_recipients);
  }
  g_free (this->mail_rcpt);
  g_free (this->mail_from);
  g_free (this->subject);
  g_free (this->message_body);
  g_free (this->content_type);

  g_cond_clear (&this->cond_transfer_end);

  if (this->base64_chunk != NULL) {
    if (this->base64_chunk->chunk_array != NULL) {
      g_byte_array_free (this->base64_chunk->chunk_array, TRUE);
    }
    g_free (this->base64_chunk);
  }

  if (this->payload_headers != NULL) {
    g_byte_array_free (this->payload_headers, TRUE);
  }

  g_free (this->pop_user);
  g_free (this->pop_passwd);
  if (this->pop_curl != NULL) {
    curl_easy_cleanup (this->pop_curl);
    this->pop_curl = NULL;
  }
  g_free (this->pop_location);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_curl_smtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlSmtpSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_SMTP_SINK (object));
  sink = GST_CURL_SMTP_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);
  if (cur_state != GST_STATE_PLAYING && cur_state != GST_STATE_PAUSED) {
    GST_OBJECT_LOCK (sink);

    switch (prop_id) {
      case PROP_MAIL_RCPT:
        g_free (sink->mail_rcpt);
        sink->mail_rcpt = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "mail-rcpt set to %s", sink->mail_rcpt);
        break;
      case PROP_MAIL_FROM:
        g_free (sink->mail_from);
        sink->mail_from = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "mail-from set to %s", sink->mail_from);
        break;
      case PROP_SUBJECT:
        g_free (sink->subject);
        sink->subject = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "subject set to %s", sink->subject);
        break;
      case PROP_MESSAGE_BODY:
        g_free (sink->message_body);
        sink->message_body = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "message-body set to %s", sink->message_body);
        break;
      case PROP_CONTENT_TYPE:
        g_free (sink->content_type);
        sink->content_type = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "content-type set to %s", sink->content_type);
        break;
      case PROP_USE_SSL:
        sink->use_ssl = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "use-ssl set to %d", sink->use_ssl);
        break;
      case PROP_NBR_ATTACHMENTS:
        sink->nbr_attachments = g_value_get_int (value);
        sink->curr_attachment = 1;
        GST_DEBUG_OBJECT (sink, "nbr-attachments set to %d",
            sink->nbr_attachments);
        break;
      case PROP_POP_USER_NAME:
        g_free (sink->pop_user);
        sink->pop_user = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "pop-user set to %s", sink->pop_user);
        break;
      case PROP_POP_USER_PASSWD:
        g_free (sink->pop_passwd);
        sink->pop_passwd = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "pop-passwd set to %s", sink->pop_passwd);
        break;
      case PROP_POP_LOCATION:
        g_free (sink->pop_location);
        sink->pop_location = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "pop-location set to %s", sink->pop_location);
        break;

      default:
        GST_DEBUG_OBJECT (sink, "invalid property id %d", prop_id);
        break;
    }

    GST_OBJECT_UNLOCK (sink);

    return;
  }

  /* in PLAYING or PAUSED state */
  GST_OBJECT_LOCK (sink);

  switch (prop_id) {
    case PROP_CONTENT_TYPE:
      g_free (sink->content_type);
      sink->content_type = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "content type set to %s", sink->content_type);
      break;
    default:
      GST_WARNING_OBJECT (sink, "cannot set property when PLAYING");
      break;
  }

  GST_OBJECT_UNLOCK (sink);
}

static void
gst_curl_smtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlSmtpSink *sink;

  g_return_if_fail (GST_IS_CURL_SMTP_SINK (object));
  sink = GST_CURL_SMTP_SINK (object);

  switch (prop_id) {
    case PROP_MAIL_RCPT:
      g_value_set_string (value, sink->mail_rcpt);
      break;
    case PROP_MAIL_FROM:
      g_value_set_string (value, sink->mail_from);
      break;
    case PROP_SUBJECT:
      g_value_set_string (value, sink->subject);
      break;
    case PROP_MESSAGE_BODY:
      g_value_set_string (value, sink->message_body);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, sink->content_type);
      break;
    case PROP_USE_SSL:
      g_value_set_boolean (value, sink->use_ssl);
      break;
    case PROP_NBR_ATTACHMENTS:
      g_value_set_int (value, sink->nbr_attachments);
      break;
    case PROP_POP_USER_NAME:
      g_value_set_string (value, sink->pop_user);
      break;
    case PROP_POP_USER_PASSWD:
      g_value_set_string (value, sink->pop_passwd);
      break;
    case PROP_POP_LOCATION:
      g_value_set_string (value, sink->pop_location);
      break;

    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}

static gboolean
gst_curl_smtp_sink_set_payload_headers_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  gchar *hdrs;
  gboolean append_headers = FALSE;

  if (sink->reset_transfer_options) {
    g_assert (!bcsink->is_live);
    sink->reset_transfer_options = FALSE;

    /* all data has been sent in the previous transfer, setup headers for
     * a new transfer */
    gst_curl_smtp_sink_set_transfer_options_unlocked (bcsink);
    append_headers = TRUE;
  }

  if (sink->payload_headers == NULL) {
    sink->payload_headers = g_byte_array_new ();
    append_headers = TRUE;
  }

  if (sink->base64_chunk == NULL) {
    g_assert (!bcsink->is_live);
    /* we are just about to send the very first attachment in this transfer.
     * This is the only place where base64_chunk and its array are allocated.
     */
    sink->base64_chunk = g_malloc (sizeof (Base64Chunk));
    sink->base64_chunk->chunk_array = g_byte_array_new ();
    append_headers = TRUE;
  } else {
    g_assert (sink->base64_chunk->chunk_array != NULL);
  }

  sink->base64_chunk->state = 0;
  sink->base64_chunk->save = 0;

  if (G_UNLIKELY (!append_headers)) {
    g_byte_array_free (sink->base64_chunk->chunk_array, TRUE);
    sink->base64_chunk->chunk_array = NULL;
    g_free (sink->base64_chunk);
    sink->base64_chunk = NULL;
    return FALSE;
  }

  hdrs = g_strdup_printf ("\r\n\r\n--%s\r\n"
      "Content-Type: application/octet-stream; name=\"%s\"\r\n"
      /* TODO: support for other encodings */
      "Content-Transfer-Encoding: BASE64\r\n"
      "Content-Disposition: attachment; filename=\"%s\"\r\n\r\n"
      "\r\n", BOUNDARY_STRING, bcsink->file_name, bcsink->file_name);
  g_byte_array_append (sink->payload_headers, (guint8 *) hdrs, strlen (hdrs));
  g_free (hdrs);

  return TRUE;
}

/* MIME encoded-word syntax (RFC 2047):
 * =?charset?encoding?encoded text?= */
static gchar *
generate_encoded_word (gchar * str)
{
  gchar *encoded_word;

  g_assert (str);

  if (g_utf8_validate (str, -1, NULL)) {
    gchar *base64_str;

    base64_str = g_base64_encode ((const guchar *) str, strlen (str));
    encoded_word = g_strdup_printf ("=?utf-8?B?%s?=", base64_str);
    g_free (base64_str);
  } else {
    GST_WARNING ("string is not a valid UTF-8 string");
    encoded_word = g_strdup (str);
  }

  /* TODO: 75 character limit */
  return encoded_word;
}

/* Setup header fields (From:/To:/Date: etc) and message body for the e-mail.
 * This data is supposed to be sent to libcurl just before any media data.
 * This function is called once for each e-mail:
 * 1. we are about the send the first attachment
 * 2. we have sent all the attachments and continue sending new ones within
 *    a new e-mail (transfer options have been reset). */
static gboolean
gst_curl_smtp_sink_set_transfer_options_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  GstCurlTlsSinkClass *parent_class;
  gchar *request_headers;
  GDateTime *date;
  gchar *date_str;
  gchar **tmp_list = NULL;
  gchar *subject_header = NULL;
  gchar *message_body = NULL;
  gchar *rcpt_header = NULL;
  gchar *enc_rcpt;
  gchar *from_header = NULL;
  gchar *enc_from;
  gint i;
  CURLcode res;

  g_assert (sink->payload_headers == NULL);
  g_assert (sink->mail_rcpt != NULL);
  g_assert (sink->mail_from != NULL);

  /* time */
  date = g_date_time_new_now_local ();
  date_str = g_date_time_format (date, "%a, %e %b %Y %H:%M:%S %z");
  g_date_time_unref (date);

  /* recipient, sender and subject are all UTF-8 strings, which are additionally
   * base64-encoded */

  /* recipient */
  enc_rcpt = generate_encoded_word (sink->mail_rcpt);
  rcpt_header = g_strdup_printf ("%s <%s>", enc_rcpt, sink->mail_rcpt);
  g_free (enc_rcpt);

  /* sender */
  enc_from = generate_encoded_word (sink->mail_from);
  from_header = g_strdup_printf ("%s <%s>", enc_from, sink->mail_from);
  g_free (enc_from);

  /* subject */
  if (sink->subject != NULL) {
    subject_header = generate_encoded_word (sink->subject);
  }

  /* message */
  if (sink->message_body != NULL) {
    message_body = g_base64_encode ((const guchar *) sink->message_body,
        strlen (sink->message_body));
  }

  request_headers = g_strdup_printf (
      /* headers */
      "To: %s\r\n"
      "From: %s\r\n"
      "Subject: %s\r\n"
      "Date: %s\r\n"
      MIME_VERSION "\r\n"
      "Content-Type: multipart/mixed; boundary=%s\r\n" "\r\n"
      /* body headers */
      "--" BOUNDARY_STRING "\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Transfer-Encoding: BASE64\r\n"
      /* message body */
      "\r\n%s\r\n",
      rcpt_header,
      from_header,
      subject_header ? subject_header : "",
      date_str, BOUNDARY_STRING, message_body ? message_body : "");

  sink->payload_headers = g_byte_array_new ();

  g_byte_array_append (sink->payload_headers, (guint8 *) request_headers,
      strlen (request_headers));
  g_free (date_str);
  g_free (subject_header);
  g_free (message_body);
  g_free (rcpt_header);
  g_free (from_header);
  g_free (request_headers);

  res = curl_easy_setopt (bcsink->curl, CURLOPT_MAIL_FROM, sink->mail_from);
  if (res != CURLE_OK) {
    bcsink->error =
        g_strdup_printf ("failed to set SMTP sender email address: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  if (sink->curl_recipients != NULL) {
    curl_slist_free_all (sink->curl_recipients);
    sink->curl_recipients = NULL;
  }

  tmp_list = g_strsplit_set (sink->mail_rcpt, MAIL_RCPT_DELIMITER, -1);
  for (i = 0; i < g_strv_length (tmp_list); i++) {
    sink->curl_recipients = curl_slist_append (sink->curl_recipients,
        tmp_list[i]);
  }
  g_strfreev (tmp_list);

  /* note that the CURLOPT_MAIL_RCPT takes a list, not a char array */
  res = curl_easy_setopt (bcsink->curl, CURLOPT_MAIL_RCPT,
      sink->curl_recipients);
  if (res != CURLE_OK) {
    bcsink->error =
        g_strdup_printf ("failed to set SMTP recipient email address: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  res = curl_easy_setopt (bcsink->curl, CURLOPT_UPLOAD, 1L);
  if (res != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to prepare for upload: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  parent_class = GST_CURL_TLS_SINK_GET_CLASS (sink);

  if (sink->use_ssl) {
    return parent_class->set_options_unlocked (bcsink);
  }

  return TRUE;
}

/* FIXME: exactly the same function as in http sink */
static void
gst_curl_smtp_sink_set_mime_type (GstCurlBaseSink * bcsink, GstCaps * caps)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  GstStructure *structure;
  const gchar *mime_type;

  if (sink->content_type != NULL) {
    return;
  }

  structure = gst_caps_get_structure (caps, 0);
  mime_type = gst_structure_get_name (structure);
  sink->content_type = g_strdup (mime_type);
}

static size_t
gst_curl_smtp_sink_flush_data_unlocked (GstCurlBaseSink * bcsink,
    void *curl_ptr, size_t block_size, gboolean new_file,
    gboolean close_transfer)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  Base64Chunk *chunk = sink->base64_chunk;
  gint state = chunk->state;
  gint save = chunk->save;
  GByteArray *array = chunk->chunk_array;
  size_t bytes_to_send;
  gint len;
  gchar *data_out;

  GST_DEBUG
      ("live: %d, num attachments: %d, curr_attachment: %d, "
      "eos: %d, close_transfer: %d, final boundary: %d, array_len: %d",
      bcsink->is_live, sink->nbr_attachments, sink->curr_attachment,
      sink->eos, close_transfer, sink->final_boundary_added, array->len);


  if ((bcsink->is_live && (sink->curr_attachment == sink->nbr_attachments))
      || (sink->nbr_attachments == 1) || sink->eos
      || sink->final_boundary_added) {
    bcsink->is_live = FALSE;
    sink->reset_transfer_options = TRUE;
    sink->final_boundary_added = FALSE;
    sink->curr_attachment = 1;

    GST_DEBUG ("returning 0, no more data to send in this transfer");

    return 0;
  }

  /* it will need up to 5 bytes if line-breaking is enabled, however an
   * additional byte is needed for <CR> as it is not automatically added by
   * glib */
  data_out = g_malloc (6);
  len = g_base64_encode_close (TRUE, data_out, &state, &save);
  chunk->state = state;
  chunk->save = save;
  /* workaround */
  data_out[len - 1] = '\r';
  data_out[len] = '\n';
  /* +1 for CR */
  g_byte_array_append (array, (guint8 *) data_out, (guint) (len + 1));
  g_free (data_out);

  if (new_file) {
    sink->curr_attachment++;
    bcsink->is_live = TRUE;

    /* reset flag */
    bcsink->new_file = FALSE;

    /* set payload headers for new file */
    gst_curl_smtp_sink_set_payload_headers_unlocked (bcsink);
  }


  if (close_transfer && !sink->final_boundary_added)
    add_final_boundary_unlocked (sink);

  bytes_to_send = MIN (block_size, array->len);
  memcpy ((guint8 *) curl_ptr, array->data, bytes_to_send);
  g_byte_array_remove_range (array, 0, bytes_to_send);

  return bytes_to_send;
}

static size_t
transfer_chunk (void *curl_ptr, TransferBuffer * buffer, Base64Chunk * chunk,
    size_t block_size, guint * last_chunk)
{
  size_t bytes_to_send;
  const guchar *data_in = buffer->ptr;
  size_t data_in_offset = buffer->offset;
  gint state = chunk->state;
  gint save = chunk->save;
  GByteArray *array = chunk->chunk_array;
  gchar *data_out;

  bytes_to_send = MIN (block_size, buffer->len);

  if (bytes_to_send == 0) {
    bytes_to_send = MIN (block_size, array->len);
  }

  /* base64 encode data */
  if (buffer->len > 0) {
    gsize len;
    gchar *ptr_in;
    gchar *ptr_out;
    gsize size_out;
    gint i;

    /* if line-breaking is enabled, at least: ((len / 3 + 1) * 4 + 4) / 72 + 1
     * bytes of extra space is required. However, additional <CR>'s are
     * required, thus we need ((len / 3 + 2) * 4 + 4) / 72 + 2 extra bytes.
     */
    size_out = (bytes_to_send / 3 + 1) * 4 + 4 + bytes_to_send +
        ((bytes_to_send / 3 + 2) * 4 + 4) / 72 + 2;

    data_out = g_malloc (size_out);
    len = g_base64_encode_step (data_in + data_in_offset, bytes_to_send, TRUE,
        data_out, &state, &save);
    chunk->state = state;
    chunk->save = save;

    /* LF->CRLF filter */
    ptr_in = ptr_out = data_out;
    for (i = 0; i < len; i++) {
      if (*ptr_in == '\n') {
        *ptr_in = '\r';
        g_byte_array_append (array, (guint8 *) ptr_out, ptr_in - ptr_out);
        g_byte_array_append (array, (guint8 *) "\r\n", strlen ("\r\n"));
        ptr_out = ptr_in + 1;
      }
      ptr_in++;
    }
    if (ptr_in - ptr_out) {
      g_byte_array_append (array, (guint8 *) ptr_out, ptr_in - ptr_out);
    }

    g_free (data_out);
    data_out = NULL;

    buffer->offset += bytes_to_send;
    buffer->len -= bytes_to_send;

    bytes_to_send = MIN (block_size, array->len);
    memcpy ((guint8 *) curl_ptr, array->data, bytes_to_send);
    g_byte_array_remove_range (array, 0, bytes_to_send);

    if (array->len == 0) {
      *last_chunk = 1;

    }

    return bytes_to_send;
  }

  /* at this point all data has been encoded */
  memcpy ((guint8 *) curl_ptr, array->data, bytes_to_send);
  g_byte_array_remove_range (array, 0, bytes_to_send);
  if (array->len == 0) {
    *last_chunk = 1;
  }

  return bytes_to_send;
}

static size_t
gst_curl_smtp_sink_transfer_data_buffer (GstCurlBaseSink * bcsink,
    void *curl_ptr, size_t block_size, guint * last_chunk)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  size_t bytes_to_send;

  if (sink->payload_headers && sink->payload_headers->len) {
    return transfer_payload_headers (sink, curl_ptr, block_size);
  }

  if (sink->base64_chunk != NULL) {
    bytes_to_send =
        transfer_chunk (curl_ptr, bcsink->transfer_buf, sink->base64_chunk,
        block_size, last_chunk);

    /* if last chunk of current buffer and max attachments per mail is reached
     * then add final boundary */
    if (*last_chunk && sink->curr_attachment == sink->nbr_attachments &&
        !sink->final_boundary_added) {
      add_final_boundary_unlocked (sink);
      /* now that we've added the final boundary to the array we have on more
       * chunk to send */
      *last_chunk = 0;
    }

    GST_OBJECT_LOCK (sink);
    if (sink->eos) {
      gst_curl_smtp_sink_notify_transfer_end_unlocked (sink);
    }
    GST_OBJECT_UNLOCK (sink);

    return bytes_to_send;
  }

  /* we should never get here */
  return 0;
}

static size_t
transfer_payload_headers (GstCurlSmtpSink * sink,
    void *curl_ptr, size_t block_size)
{
  size_t bytes_to_send;
  GByteArray *headers = sink->payload_headers;

  bytes_to_send = MIN (block_size, headers->len);
  memcpy ((guint8 *) curl_ptr, headers->data, bytes_to_send);
  g_byte_array_remove_range (headers, 0, bytes_to_send);


  if (headers->len == 0) {
    g_byte_array_free (headers, TRUE);
    sink->payload_headers = NULL;
  }

  return bytes_to_send;
}

static gboolean
gst_curl_smtp_sink_prepare_transfer (GstCurlBaseSink * bcsink)
{
  GstCurlSmtpSink *sink = GST_CURL_SMTP_SINK (bcsink);
  CURLcode res;
  gboolean ret = TRUE;

  if (sink->pop_location && strlen (sink->pop_location)) {
    if ((sink->pop_curl = curl_easy_init ()) == NULL) {
      bcsink->error = g_strdup ("POP protocol: failed to create handler");
      return FALSE;
    }

    res = curl_easy_setopt (sink->pop_curl, CURLOPT_URL, sink->pop_location);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set URL: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    if (sink->pop_user != NULL && strlen (sink->pop_user) &&
        sink->pop_passwd != NULL && strlen (sink->pop_passwd)) {
      res = curl_easy_setopt (sink->pop_curl, CURLOPT_USERNAME, sink->pop_user);
      if (res != CURLE_OK) {
        bcsink->error = g_strdup_printf ("failed to set user name: %s",
            curl_easy_strerror (res));
        return FALSE;
      }

      res = curl_easy_setopt (sink->pop_curl, CURLOPT_PASSWORD,
          sink->pop_passwd);
      if (res != CURLE_OK) {
        bcsink->error = g_strdup_printf ("failed to set user name: %s",
            curl_easy_strerror (res));
        return FALSE;
      }
    }
  }

  if (sink->pop_curl != NULL) {
    /* ready to initialize connection to POP server */
    res = curl_easy_perform (sink->pop_curl);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("POP transfer failed: %s",
          curl_easy_strerror (res));
      ret = FALSE;
    }

    curl_easy_cleanup (sink->pop_curl);
    sink->pop_curl = NULL;
  }

  return ret;
}
