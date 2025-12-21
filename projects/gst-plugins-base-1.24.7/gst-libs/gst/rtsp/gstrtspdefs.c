/* GStreamer
 * Copyright (C) <2005,2006> Wim Taymans <wim@fluendo.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * SECTION:gstrtspdefs
 * @title: GstRtspdefs
 * @short_description: common RTSP defines
 * @see_also: gstrtspurl, gstrtspconnection
 *
 * Provides common defines for the RTSP library.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>

#include "gstrtspdefs.h"
#include <gst/gst.h>

struct rtsp_header
{
  const gchar *name;
  gboolean multiple;
};

static const gchar *rtsp_methods[] = {
  "DESCRIBE",
  "ANNOUNCE",
  "GET_PARAMETER",
  "OPTIONS",
  "PAUSE",
  "PLAY",
  "RECORD",
  "REDIRECT",
  "SETUP",
  "SET_PARAMETER",
  "TEARDOWN",
  "GET",
  "POST",
  NULL
};

static struct rtsp_header rtsp_headers[] = {
  {"Accept", TRUE},
  {"Accept-Encoding", TRUE},
  {"Accept-Language", TRUE},
  {"Allow", TRUE},
  {"Authorization", FALSE},
  {"Bandwidth", FALSE},
  {"Blocksize", FALSE},
  {"Cache-Control", TRUE},
  {"Conference", FALSE},
  {"Connection", TRUE},
  {"Content-Base", FALSE},
  {"Content-Encoding", TRUE},
  {"Content-Language", TRUE},
  {"Content-Length", FALSE},
  {"Content-Location", FALSE},
  {"Content-Type", FALSE},
  {"CSeq", FALSE},
  {"Date", FALSE},
  {"Expires", FALSE},
  {"From", FALSE},
  {"If-Modified-Since", FALSE},
  {"Last-Modified", FALSE},
  {"Proxy-Authenticate", TRUE},
  {"Proxy-Require", TRUE},
  {"Public", TRUE},
  {"Range", FALSE},
  {"Referer", FALSE},
  {"Require", TRUE},
  {"Retry-After", FALSE},
  {"RTP-Info", TRUE},
  {"Scale", FALSE},
  {"Session", FALSE},
  {"Server", FALSE},
  {"Speed", FALSE},
  {"Transport", TRUE},
  {"Unsupported", FALSE},
  {"User-Agent", FALSE},
  {"Via", TRUE},
  {"WWW-Authenticate", TRUE},

  /* Real extensions */
  {"ClientChallenge", FALSE},
  {"RealChallenge1", FALSE},
  {"RealChallenge2", FALSE},
  {"RealChallenge3", FALSE},
  {"Subscribe", FALSE},
  {"Alert", FALSE},
  {"ClientID", FALSE},
  {"CompanyID", FALSE},
  {"GUID", FALSE},
  {"RegionData", FALSE},
  {"SupportsMaximumASMBandwidth", FALSE},
  {"Language", FALSE},
  {"PlayerStarttime", FALSE},

  {"Location", FALSE},

  {"ETag", FALSE},
  {"If-Match", TRUE},

  /* WM extensions [MS-RTSP] */
  {"Accept-Charset", TRUE},
  {"Supported", TRUE},
  {"Vary", TRUE},
  {"X-Accelerate-Streaming", FALSE},
  {"X-Accept-Authentication", FALSE},
  {"X-Accept-Proxy-Authentication", FALSE},
  {"X-Broadcast-Id", FALSE},
  {"X-Burst-Streaming", FALSE},
  {"X-Notice", FALSE},
  {"X-Player-Lag-Time", FALSE},
  {"X-Playlist", FALSE},
  {"X-Playlist-Change-Notice", FALSE},
  {"X-Playlist-Gen-Id", FALSE},
  {"X-Playlist-Seek-Id", FALSE},
  {"X-Proxy-Client-Agent", FALSE},
  {"X-Proxy-Client-Verb", FALSE},
  {"X-Receding-PlaylistChange", FALSE},
  {"X-RTP-Info", FALSE},
  {"X-StartupProfile", FALSE},

  {"Timestamp", FALSE},

  {"Authentication-Info", FALSE},
  {"Host", FALSE},
  {"Pragma", TRUE},
  {"X-Server-IP-Address", FALSE},
  {"x-sessioncookie", FALSE},

  {"RTCP-Interval", FALSE},

  /* Since: 1.4 */
  {"KeyMgmt", FALSE},

  /* Since: 1.14 */
  {"Pipelined-Requests", FALSE},
  {"Media-Properties", FALSE},
  {"Seek-Style", FALSE},
  {"Accept-Ranges", FALSE},

  /* Onvif extensions */
  {"Frames", FALSE},
  {"Rate-Control", FALSE},

  {NULL, FALSE}
};

#define DEF_STATUS(c, t) \
  g_hash_table_insert (statuses, GUINT_TO_POINTER(c), (gpointer) t)

static GHashTable *
rtsp_init_status (void)
{
  GHashTable *statuses = g_hash_table_new (NULL, NULL);

  DEF_STATUS (GST_RTSP_STS_CONTINUE, "Continue");
  DEF_STATUS (GST_RTSP_STS_OK, "OK");
  DEF_STATUS (GST_RTSP_STS_CREATED, "Created");
  DEF_STATUS (GST_RTSP_STS_LOW_ON_STORAGE, "Low on Storage Space");
  DEF_STATUS (GST_RTSP_STS_MULTIPLE_CHOICES, "Multiple Choices");
  DEF_STATUS (GST_RTSP_STS_MOVED_PERMANENTLY, "Moved Permanently");
  DEF_STATUS (GST_RTSP_STS_MOVE_TEMPORARILY, "Move Temporarily");
  DEF_STATUS (GST_RTSP_STS_SEE_OTHER, "See Other");
  DEF_STATUS (GST_RTSP_STS_NOT_MODIFIED, "Not Modified");
  DEF_STATUS (GST_RTSP_STS_USE_PROXY, "Use Proxy");
  DEF_STATUS (GST_RTSP_STS_BAD_REQUEST, "Bad Request");
  DEF_STATUS (GST_RTSP_STS_UNAUTHORIZED, "Unauthorized");
  DEF_STATUS (GST_RTSP_STS_PAYMENT_REQUIRED, "Payment Required");
  DEF_STATUS (GST_RTSP_STS_FORBIDDEN, "Forbidden");
  DEF_STATUS (GST_RTSP_STS_NOT_FOUND, "Not Found");
  DEF_STATUS (GST_RTSP_STS_METHOD_NOT_ALLOWED, "Method Not Allowed");
  DEF_STATUS (GST_RTSP_STS_NOT_ACCEPTABLE, "Not Acceptable");
  DEF_STATUS (GST_RTSP_STS_PROXY_AUTH_REQUIRED,
      "Proxy Authentication Required");
  DEF_STATUS (GST_RTSP_STS_REQUEST_TIMEOUT, "Request Time-out");
  DEF_STATUS (GST_RTSP_STS_GONE, "Gone");
  DEF_STATUS (GST_RTSP_STS_LENGTH_REQUIRED, "Length Required");
  DEF_STATUS (GST_RTSP_STS_PRECONDITION_FAILED, "Precondition Failed");
  DEF_STATUS (GST_RTSP_STS_REQUEST_ENTITY_TOO_LARGE,
      "Request Entity Too Large");
  DEF_STATUS (GST_RTSP_STS_REQUEST_URI_TOO_LARGE, "Request-URI Too Large");
  DEF_STATUS (GST_RTSP_STS_UNSUPPORTED_MEDIA_TYPE, "Unsupported Media Type");
  DEF_STATUS (GST_RTSP_STS_PARAMETER_NOT_UNDERSTOOD,
      "Parameter Not Understood");
  DEF_STATUS (GST_RTSP_STS_CONFERENCE_NOT_FOUND, "Conference Not Found");
  DEF_STATUS (GST_RTSP_STS_NOT_ENOUGH_BANDWIDTH, "Not Enough Bandwidth");
  DEF_STATUS (GST_RTSP_STS_SESSION_NOT_FOUND, "Session Not Found");
  DEF_STATUS (GST_RTSP_STS_METHOD_NOT_VALID_IN_THIS_STATE,
      "Method Not Valid in This State");
  DEF_STATUS (GST_RTSP_STS_HEADER_FIELD_NOT_VALID_FOR_RESOURCE,
      "Header Field Not Valid for Resource");
  DEF_STATUS (GST_RTSP_STS_INVALID_RANGE, "Invalid Range");
  DEF_STATUS (GST_RTSP_STS_PARAMETER_IS_READONLY, "Parameter Is Read-Only");
  DEF_STATUS (GST_RTSP_STS_AGGREGATE_OPERATION_NOT_ALLOWED,
      "Aggregate operation not allowed");
  DEF_STATUS (GST_RTSP_STS_ONLY_AGGREGATE_OPERATION_ALLOWED,
      "Only aggregate operation allowed");
  DEF_STATUS (GST_RTSP_STS_UNSUPPORTED_TRANSPORT, "Unsupported transport");
  DEF_STATUS (GST_RTSP_STS_DESTINATION_UNREACHABLE, "Destination unreachable");
  DEF_STATUS (GST_RTSP_STS_KEY_MANAGEMENT_FAILURE, "Key management failure");
  DEF_STATUS (GST_RTSP_STS_INTERNAL_SERVER_ERROR, "Internal Server Error");
  DEF_STATUS (GST_RTSP_STS_NOT_IMPLEMENTED, "Not Implemented");
  DEF_STATUS (GST_RTSP_STS_BAD_GATEWAY, "Bad Gateway");
  DEF_STATUS (GST_RTSP_STS_SERVICE_UNAVAILABLE, "Service Unavailable");
  DEF_STATUS (GST_RTSP_STS_GATEWAY_TIMEOUT, "Gateway Time-out");
  DEF_STATUS (GST_RTSP_STS_RTSP_VERSION_NOT_SUPPORTED,
      "RTSP Version not supported");
  DEF_STATUS (GST_RTSP_STS_OPTION_NOT_SUPPORTED, "Option not supported");

  return statuses;
}

/**
 * gst_rtsp_strresult:
 * @result: a #GstRTSPResult
 *
 * Convert @result in a human readable string.
 *
 * Returns: a newly allocated string. g_free() after usage.
 */
gchar *
gst_rtsp_strresult (GstRTSPResult result)
{
  switch (result) {
    case GST_RTSP_OK:
      return g_strdup ("OK");
    case GST_RTSP_ESYS:
      return g_strdup ("System error");
    case GST_RTSP_ENET:
      return g_strdup ("Network error");
    case GST_RTSP_ERROR:
      return g_strdup ("Generic error");
    case GST_RTSP_EINVAL:
      return g_strdup ("Invalid parameter specified");
    case GST_RTSP_EINTR:
      return g_strdup ("Operation interrupted");
    case GST_RTSP_ENOMEM:
      return g_strdup ("Out of memory");
    case GST_RTSP_ERESOLV:
      return g_strdup ("Cannot resolve host");
    case GST_RTSP_ENOTIMPL:
      return g_strdup ("Function not implemented");
    case GST_RTSP_EPARSE:
      return g_strdup ("Parse error");
    case GST_RTSP_EWSASTART:
      return g_strdup ("Error on WSAStartup");
    case GST_RTSP_EWSAVERSION:
      return g_strdup ("Windows sockets are not version 0x202");
    case GST_RTSP_EEOF:
      return g_strdup ("Received end-of-file");
    case GST_RTSP_ENOTIP:
      return g_strdup ("Host is not a valid IP address");
    case GST_RTSP_ETIMEOUT:
      return g_strdup ("Timeout while waiting for server response");
    case GST_RTSP_ETGET:
      return g_strdup ("Tunnel GET request received");
    case GST_RTSP_ETPOST:
      return g_strdup ("Tunnel POST request received");
    case GST_RTSP_ELAST:
    default:
      return g_strdup_printf ("Unknown error (%d)", result);
  }
}

/**
 * gst_rtsp_method_as_text:
 * @method: a #GstRTSPMethod
 *
 * Convert @method to a string.
 *
 * Returns: (nullable): a string representation of @method.
 */
const gchar *
gst_rtsp_method_as_text (GstRTSPMethod method)
{
  gint i;

  if (method == GST_RTSP_INVALID)
    return NULL;

  i = 0;
  while ((method & 1) == 0) {
    i++;
    method >>= 1;
  }
  return rtsp_methods[i];
}

/**
 * gst_rtsp_version_as_text:
 * @version: a #GstRTSPVersion
 *
 * Convert @version to a string.
 *
 * Returns: a string representation of @version.
 */
const gchar *
gst_rtsp_version_as_text (GstRTSPVersion version)
{
  switch (version) {
    case GST_RTSP_VERSION_1_0:
      return "1.0";

    case GST_RTSP_VERSION_1_1:
      return "1.1";

    case GST_RTSP_VERSION_2_0:
      return "2.0";


    default:
      return "0.0";
  }
}

/**
 * gst_rtsp_header_as_text:
 * @field: a #GstRTSPHeaderField
 *
 * Convert @field to a string.
 *
 * Returns: (nullable): a string representation of @field.
 */
const gchar *
gst_rtsp_header_as_text (GstRTSPHeaderField field)
{
  if (field == GST_RTSP_HDR_INVALID)
    return NULL;
  else
    return rtsp_headers[field - 1].name;
}

/**
 * gst_rtsp_status_as_text:
 * @code: a #GstRTSPStatusCode
 *
 * Convert @code to a string.
 *
 * Returns: a string representation of @code.
 */
const gchar *
gst_rtsp_status_as_text (GstRTSPStatusCode code)
{
  static GHashTable *statuses;

  if (G_UNLIKELY (statuses == NULL))
    statuses = rtsp_init_status ();

  return g_hash_table_lookup (statuses, GUINT_TO_POINTER (code));
}

/**
 * gst_rtsp_find_header_field:
 * @header: a header string
 *
 * Convert @header to a #GstRTSPHeaderField.
 *
 * Returns: a #GstRTSPHeaderField for @header or #GST_RTSP_HDR_INVALID if the
 * header field is unknown.
 */
GstRTSPHeaderField
gst_rtsp_find_header_field (const gchar * header)
{
  gint idx;

  for (idx = 0; rtsp_headers[idx].name; idx++) {
    if (g_ascii_strcasecmp (rtsp_headers[idx].name, header) == 0) {
      return idx + 1;
    }
  }
  return GST_RTSP_HDR_INVALID;
}

/**
 * gst_rtsp_find_method:
 * @method: a method
 *
 * Convert @method to a #GstRTSPMethod.
 *
 * Returns: a #GstRTSPMethod for @method or #GST_RTSP_INVALID if the
 * method is unknown.
 */
GstRTSPMethod
gst_rtsp_find_method (const gchar * method)
{
  gint idx;

  for (idx = 0; rtsp_methods[idx]; idx++) {
    if (g_ascii_strcasecmp (rtsp_methods[idx], method) == 0) {
      return (1 << idx);
    }
  }
  return GST_RTSP_INVALID;
}

/**
 * gst_rtsp_options_as_text:
 * @options: one or more #GstRTSPMethod
 *
 * Convert @options to a string.
 *
 * Returns: a new string of @options. g_free() after usage.
 */
gchar *
gst_rtsp_options_as_text (GstRTSPMethod options)
{
  GString *str;

  str = g_string_new ("");

  if (options & GST_RTSP_OPTIONS)
    g_string_append (str, "OPTIONS, ");
  if (options & GST_RTSP_DESCRIBE)
    g_string_append (str, "DESCRIBE, ");
  if (options & GST_RTSP_ANNOUNCE)
    g_string_append (str, "ANNOUNCE, ");
  if (options & GST_RTSP_GET_PARAMETER)
    g_string_append (str, "GET_PARAMETER, ");
  if (options & GST_RTSP_PAUSE)
    g_string_append (str, "PAUSE, ");
  if (options & GST_RTSP_PLAY)
    g_string_append (str, "PLAY, ");
  if (options & GST_RTSP_RECORD)
    g_string_append (str, "RECORD, ");
  if (options & GST_RTSP_REDIRECT)
    g_string_append (str, "REDIRECT, ");
  if (options & GST_RTSP_SETUP)
    g_string_append (str, "SETUP, ");
  if (options & GST_RTSP_SET_PARAMETER)
    g_string_append (str, "SET_PARAMETER, ");
  if (options & GST_RTSP_TEARDOWN)
    g_string_append (str, "TEARDOWN, ");

  /* remove trailing ", " if there is one */
  if (str->len > 2)
    str = g_string_truncate (str, str->len - 2);

  return g_string_free (str, FALSE);
}

/**
 * gst_rtsp_options_from_text:
 * @options: a comma separated list of options
 *
 * Convert the comma separated list @options to a #GstRTSPMethod bitwise or
 * of methods. This functions is the reverse of gst_rtsp_options_as_text().
 *
 * Returns: a #GstRTSPMethod
 *
 * Since: 1.2
 */
GstRTSPMethod
gst_rtsp_options_from_text (const gchar * options)
{
  GstRTSPMethod methods;
  gchar **ostr;
  gint i;

  /* The string is like:
   * OPTIONS, DESCRIBE, ANNOUNCE, PLAY, SETUP, ...
   */
  ostr = g_strsplit (options, ",", 0);

  methods = 0;
  for (i = 0; ostr[i]; i++) {
    gchar *stripped;
    GstRTSPMethod method;

    stripped = g_strstrip (ostr[i]);
    method = gst_rtsp_find_method (stripped);

    /* keep bitfield of supported methods */
    if (method != GST_RTSP_INVALID)
      methods |= method;
  }
  g_strfreev (ostr);

  return methods;
}

/**
 * gst_rtsp_header_allow_multiple:
 * @field: a #GstRTSPHeaderField
 *
 * Check whether @field may appear multiple times in a message.
 *
 * Returns: %TRUE if multiple headers are allowed.
 */
gboolean
gst_rtsp_header_allow_multiple (GstRTSPHeaderField field)
{
  if (field == GST_RTSP_HDR_INVALID)
    return FALSE;
  else
    return rtsp_headers[field - 1].multiple;
}

static gchar *
auth_digest_compute_a1_md5 (const gchar * realm, const gchar * username,
    const gchar * password)
{
  GChecksum *md5_context = g_checksum_new (G_CHECKSUM_MD5);
  gchar *ret;

  g_checksum_update (md5_context, (const guchar *) username, strlen (username));
  g_checksum_update (md5_context, (const guchar *) ":", 1);
  g_checksum_update (md5_context, (const guchar *) realm, strlen (realm));
  g_checksum_update (md5_context, (const guchar *) ":", 1);
  g_checksum_update (md5_context, (const guchar *) password, strlen (password));
  ret = g_strdup (g_checksum_get_string (md5_context));
  g_assert (strlen (ret) == 32);
  g_checksum_free (md5_context);

  return ret;
}

/* See RFC2069, 2.1.2 */
static gchar *
auth_digest_compute_response_md5 (const gchar * method, const gchar * a1,
    const gchar * uri, const gchar * nonce)
{
  gchar hex_a1[33] = { 0, };
  gchar hex_a2[33] = { 0, };
  GChecksum *md5_context = g_checksum_new (G_CHECKSUM_MD5);
  const gchar *digest_string;
  gchar *response = NULL;

  if (strlen (a1) != 32)
    goto done;

  memcpy (hex_a1, a1, 32);
  g_checksum_reset (md5_context);

  /* compute A2 */
  g_checksum_update (md5_context, (const guchar *) method, strlen (method));
  g_checksum_update (md5_context, (const guchar *) ":", 1);
  g_checksum_update (md5_context, (const guchar *) uri, strlen (uri));
  digest_string = g_checksum_get_string (md5_context);
  g_assert (strlen (digest_string) == 32);
  memcpy (hex_a2, digest_string, 32);

  /* compute KD */
  g_checksum_reset (md5_context);
  g_checksum_update (md5_context, (const guchar *) hex_a1, strlen (hex_a1));
  g_checksum_update (md5_context, (const guchar *) ":", 1);
  g_checksum_update (md5_context, (const guchar *) nonce, strlen (nonce));
  g_checksum_update (md5_context, (const guchar *) ":", 1);

  g_checksum_update (md5_context, (const guchar *) hex_a2, 32);
  response = g_strdup (g_checksum_get_string (md5_context));

done:
  g_checksum_free (md5_context);

  return response;
}

/**
 * gst_rtsp_generate_digest_auth_response:
 * @algorithm: (nullable): Hash algorithm to use, or %NULL for MD5
 * @method: Request method, e.g. PLAY
 * @realm: Realm
 * @username: Username
 * @password: Password
 * @uri: Original request URI
 * @nonce: Nonce
 *
 * Calculates the digest auth response from the values given by the server and
 * the username and password. See RFC2069 for details.
 *
 * Currently only supported algorithm "md5".
 *
 * Returns: (nullable): Authentication response or %NULL if unsupported
 *
 * Since: 1.12
 */
gchar *
gst_rtsp_generate_digest_auth_response (const gchar * algorithm,
    const gchar * method, const gchar * realm, const gchar * username,
    const gchar * password, const gchar * uri, const gchar * nonce)
{
  gchar *ret = NULL;
  g_return_val_if_fail (method != NULL, NULL);
  g_return_val_if_fail (realm != NULL, NULL);
  g_return_val_if_fail (username != NULL, NULL);
  g_return_val_if_fail (password != NULL, NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (nonce != NULL, NULL);

  if (algorithm == NULL || g_ascii_strcasecmp (algorithm, "md5") == 0) {
    gchar *a1 = auth_digest_compute_a1_md5 (realm, username, password);
    ret = auth_digest_compute_response_md5 (method, a1, uri, nonce);
    g_free (a1);
  }

  return ret;
}

/**
 * gst_rtsp_generate_digest_auth_response_from_md5:
 * @algorithm: (nullable): Hash algorithm to use, or %NULL for MD5
 * @method: Request method, e.g. PLAY
 * @md5: The md5 sum of username:realm:password
 * @uri: Original request URI
 * @nonce: Nonce
 *
 * Calculates the digest auth response from the values given by the server and
 * the md5sum. See RFC2069 for details.
 *
 * This function is useful when the passwords are not stored in clear text,
 * but instead in the same format as the .htdigest file.
 *
 * Currently only supported algorithm "md5".
 *
 * Returns: (nullable): Authentication response or %NULL if unsupported
 *
 * Since: 1.16
 */
gchar *
gst_rtsp_generate_digest_auth_response_from_md5 (const gchar * algorithm,
    const gchar * method, const gchar * md5, const gchar * uri,
    const gchar * nonce)
{
  if (algorithm == NULL || g_ascii_strcasecmp (algorithm, "md5") == 0)
    return auth_digest_compute_response_md5 (method, md5, uri, nonce);

  return NULL;
}
