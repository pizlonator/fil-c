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
 * SECTION:element-curltlssink
 * @title: curltlssink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl.
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

#include "gstcurlbasesink.h"
#include "gstcurltlssink.h"

/* Default values */
#define GST_CAT_DEFAULT                gst_curl_tls_sink_debug
#define DEFAULT_INSECURE               TRUE


/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_tls_sink_debug);

enum
{
  PROP_0,
  PROP_CA_CERT,
  PROP_CA_PATH,
  PROP_CRYPTO_ENGINE,
  PROP_INSECURE
};


/* Object class function declarations */

static void gst_curl_tls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_tls_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_curl_tls_sink_finalize (GObject * gobject);
static gboolean gst_curl_tls_sink_set_options_unlocked
    (GstCurlBaseSink * bcsink);

#define gst_curl_tls_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlTlsSink, gst_curl_tls_sink, GST_TYPE_CURL_BASE_SINK);

/* private functions */

static void
gst_curl_tls_sink_class_init (GstCurlTlsSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_tls_sink_debug, "curltlssink", 0,
      "curl tls sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl tls sink",
      "Sink/Network",
      "Upload data over TLS protocol using libcurl",
      "Patricia Muscalu <patricia@axis.com>");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_tls_sink_finalize);

  gobject_class->set_property = gst_curl_tls_sink_set_property;
  gobject_class->get_property = gst_curl_tls_sink_get_property;

  klass->set_options_unlocked = gst_curl_tls_sink_set_options_unlocked;

  g_object_class_install_property (gobject_class, PROP_CA_CERT,
      g_param_spec_string ("ca-cert",
          "CA certificate",
          "CA certificate to use in order to verify the peer",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CA_PATH,
      g_param_spec_string ("ca-path",
          "CA path",
          "CA directory path to use in order to verify the peer",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CRYPTO_ENGINE,
      g_param_spec_string ("crypto-engine",
          "OpenSSL crypto engine",
          "OpenSSL crypto engine to use for cipher operations",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INSECURE,
      g_param_spec_boolean ("insecure",
          "Perform insecure SSL connections",
          "Allow curl to perform insecure SSL connections",
          DEFAULT_INSECURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_CURL_TLS_SINK, 0);
}

static void
gst_curl_tls_sink_init (GstCurlTlsSink * sink)
{
  sink->ca_cert = NULL;
  sink->ca_path = NULL;
  sink->crypto_engine = NULL;
  sink->insecure = DEFAULT_INSECURE;
}

static void
gst_curl_tls_sink_finalize (GObject * gobject)
{
  GstCurlTlsSink *this = GST_CURL_TLS_SINK (gobject);

  GST_DEBUG ("finalizing curltlssink");

  g_free (this->ca_cert);
  g_free (this->ca_path);
  g_free (this->crypto_engine);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_curl_tls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlTlsSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_TLS_SINK (object));
  sink = GST_CURL_TLS_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);
  if (cur_state != GST_STATE_PLAYING && cur_state != GST_STATE_PAUSED) {
    GST_OBJECT_LOCK (sink);

    switch (prop_id) {
      case PROP_CA_CERT:
        g_free (sink->ca_cert);
        sink->ca_cert = g_value_dup_string (value);
        sink->insecure = FALSE;
        GST_DEBUG_OBJECT (sink, "ca_cert set to %s", sink->ca_cert);
        break;
      case PROP_CA_PATH:
        g_free (sink->ca_path);
        sink->ca_path = g_value_dup_string (value);
        sink->insecure = FALSE;
        GST_DEBUG_OBJECT (sink, "ca_path set to %s", sink->ca_path);
        break;
      case PROP_CRYPTO_ENGINE:
        g_free (sink->crypto_engine);
        sink->crypto_engine = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "crypto_engine set to %s", sink->crypto_engine);
        break;
      case PROP_INSECURE:
        sink->insecure = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "insecure set to %d", sink->insecure);
        break;
    }

    GST_OBJECT_UNLOCK (sink);

    return;
  }

  GST_OBJECT_UNLOCK (sink);
}

static void
gst_curl_tls_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlTlsSink *sink;

  g_return_if_fail (GST_IS_CURL_TLS_SINK (object));
  sink = GST_CURL_TLS_SINK (object);

  switch (prop_id) {
    case PROP_CA_CERT:
      g_value_set_string (value, sink->ca_cert);
      break;
    case PROP_CA_PATH:
      g_value_set_string (value, sink->ca_path);
      break;
    case PROP_CRYPTO_ENGINE:
      g_value_set_string (value, sink->crypto_engine);
      break;
    case PROP_INSECURE:
      g_value_set_boolean (value, sink->insecure);
      break;
    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}

static gboolean
gst_curl_tls_sink_set_options_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlTlsSink *sink = GST_CURL_TLS_SINK (bcsink);
  CURLcode res;

  if (!g_str_has_prefix (bcsink->url, "http")) {
    res = curl_easy_setopt (bcsink->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set SSL level: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  /* crypto engine */
  if ((sink->crypto_engine == NULL) ||
      (strcmp (sink->crypto_engine, "auto") == 0)) {
    res = curl_easy_setopt (bcsink->curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    if (res != CURLE_OK) {
      bcsink->error =
          g_strdup_printf ("failed to set default crypto engine: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  } else {
    res = curl_easy_setopt (bcsink->curl, CURLOPT_SSLENGINE,
        sink->crypto_engine);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set crypto engine: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  /* note that, using ca-path can allow libcurl to make SSL-connections much
   * more efficiently than using ca-cert if the ca-cert file contains many CA
   * certificates. */
  if (sink->ca_cert != NULL && strlen (sink->ca_cert)) {
    GST_DEBUG ("setting ca cert");
    res = curl_easy_setopt (bcsink->curl, CURLOPT_CAINFO, sink->ca_cert);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set certificate: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  if (sink->ca_path != NULL && strlen (sink->ca_path)) {
    GST_DEBUG ("setting ca path");
    res = curl_easy_setopt (bcsink->curl, CURLOPT_CAPATH, sink->ca_path);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set certificate path: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  if (!sink->insecure) {
    /* identify authenticity of the peer's certificate */
    res = curl_easy_setopt (bcsink->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set verification of peer: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
    /* when CURLOPT_SSL_VERIFYHOST is 2, the commonName or subjectAltName
     * fields are verified */
    res = curl_easy_setopt (bcsink->curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (res != CURLE_OK) {
      bcsink->error =
          g_strdup_printf
          ("failed to set verification of server certificate: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  } else {
    /* allow "insecure" SSL connections and transfers */
    if (sink->insecure) {
      res = curl_easy_setopt (bcsink->curl, CURLOPT_SSL_VERIFYPEER, 0L);
      if (res != CURLE_OK) {
        bcsink->error =
            g_strdup_printf ("failed to set verification of peer: %s",
            curl_easy_strerror (res));
        return FALSE;
      }

      res = curl_easy_setopt (bcsink->curl, CURLOPT_SSL_VERIFYHOST, 0L);
      if (res != CURLE_OK) {
        bcsink->error =
            g_strdup_printf
            ("failed to set verification of server certificate: %s",
            curl_easy_strerror (res));
        return FALSE;
      }
    }
  }

  return TRUE;
}
