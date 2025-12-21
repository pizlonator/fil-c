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
 * SECTION:gstsdpmessage
 * @title: GstSDPMessage
 * @short_description: Helper methods for dealing with SDP messages
 *
 * The GstSDPMessage helper functions makes it easy to parse and create SDP
 * messages.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <gst/rtp/gstrtppayloads.h>
#include <gst/pbutils/pbutils.h>
#include "gstsdpmessage.h"

#define FREE_STRING(field)              g_free (field); (field) = NULL
#define REPLACE_STRING(field, val)      FREE_STRING(field); (field) = g_strdup (val)

static void
free_string (gchar ** str)
{
  FREE_STRING (*str);
}

#define INIT_ARRAY(field, type, init_func)              \
G_STMT_START {                                          \
  if (field) {                                          \
    guint i;                                            \
    for(i = 0; i < (field)->len; i++)                   \
      init_func (&g_array_index ((field), type, i));    \
    g_array_set_size ((field), 0);                      \
  }                                                     \
  else                                                  \
    (field) = g_array_new (FALSE, TRUE, sizeof (type)); \
} G_STMT_END

#define FREE_ARRAY(field)         \
G_STMT_START {                    \
  if (field)                      \
    g_array_free ((field), TRUE); \
  (field) = NULL;                 \
} G_STMT_END

#define DEFINE_STRING_SETTER(field)                                     \
GstSDPResult gst_sdp_message_set_##field (GstSDPMessage *msg, const gchar *val) { \
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);                   \
  g_free (msg->field);                                                  \
  msg->field = g_strdup (val);                                          \
  return GST_SDP_OK;                                                    \
}
#define DEFINE_STRING_GETTER(field)                                     \
const gchar* gst_sdp_message_get_##field (const GstSDPMessage *msg) {   \
  g_return_val_if_fail (msg != NULL, NULL);                             \
  return msg->field;                                                    \
}

#define DEFINE_ARRAY_LEN(field)                                         \
guint gst_sdp_message_##field##_len (const GstSDPMessage *msg) {        \
  g_return_val_if_fail (msg != NULL, 0);                                \
  return msg->field->len;                                               \
}
#define DEFINE_ARRAY_GETTER(method, field, type)                        \
const type * gst_sdp_message_get_##method (const GstSDPMessage *msg, guint idx) {  \
  g_return_val_if_fail (msg != NULL, NULL);                             \
  return &g_array_index (msg->field, type, idx);                        \
}
#define DEFINE_PTR_ARRAY_GETTER(method, field, type)                    \
const type gst_sdp_message_get_##method (const GstSDPMessage *msg, guint idx) {    \
  g_return_val_if_fail (msg != NULL, (type) 0);                         \
  return g_array_index (msg->field, type, idx);                         \
}
#define DEFINE_ARRAY_INSERT(method, field, intype, dup_method, type)         \
GstSDPResult gst_sdp_message_insert_##method (GstSDPMessage *msg, gint idx, intype val) {   \
  type vt;                                                              \
  type* v = &vt;                                                         \
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);                   \
  dup_method (v, val);                                                  \
  if (idx == -1)                                                        \
    g_array_append_val (msg->field, vt);                                \
  else                                                                  \
    g_array_insert_val (msg->field, idx, vt);                           \
  return GST_SDP_OK;                                                    \
}

#define DEFINE_ARRAY_REPLACE(method, field, intype, free_method, dup_method, type)         \
GstSDPResult gst_sdp_message_replace_##method (GstSDPMessage *msg, guint idx, intype val) {   \
  type *v;                                                              \
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);                   \
  v = &g_array_index (msg->field, type, idx);                           \
  free_method (v);                                                      \
  dup_method (v, val);                                                  \
  return GST_SDP_OK;                                                    \
}
#define DEFINE_ARRAY_REMOVE(method, field, type, free_method)                        \
GstSDPResult gst_sdp_message_remove_##method (GstSDPMessage *msg, guint idx) {  \
  type *v;                                                              \
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);                   \
  v = &g_array_index (msg->field, type, idx);                           \
  free_method (v);                                                      \
  g_array_remove_index (msg->field, idx);                               \
  return GST_SDP_OK;                                                    \
}
#define DEFINE_ARRAY_ADDER(method, type)                                \
GstSDPResult gst_sdp_message_add_##method (GstSDPMessage *msg, const type val) {   \
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);                   \
  return gst_sdp_message_insert_##method (msg, -1, val);                \
}

#define dup_string(v,val) ((*v) = g_strdup (val))
#define INIT_STR_ARRAY(field) \
    INIT_ARRAY (field, gchar *, free_string)
#define DEFINE_STR_ARRAY_GETTER(method, field) \
    DEFINE_PTR_ARRAY_GETTER(method, field, gchar *)
#define DEFINE_STR_ARRAY_INSERT(method, field) \
    DEFINE_ARRAY_INSERT (method, field, const gchar *, dup_string, gchar *)
#define DEFINE_STR_ARRAY_ADDER(method, field) \
    DEFINE_ARRAY_ADDER (method, gchar *)
#define DEFINE_STR_ARRAY_REPLACE(method, field) \
    DEFINE_ARRAY_REPLACE (method, field, const gchar *, free_string, dup_string, gchar *)
#define DEFINE_STR_ARRAY_REMOVE(method, field) \
    DEFINE_ARRAY_REMOVE (method, field, gchar *, free_string)

static GstSDPMessage *gst_sdp_message_boxed_copy (GstSDPMessage * orig);
static void gst_sdp_message_boxed_free (GstSDPMessage * msg);

G_DEFINE_BOXED_TYPE (GstSDPMessage, gst_sdp_message, gst_sdp_message_boxed_copy,
    gst_sdp_message_boxed_free);

static GstSDPMessage *
gst_sdp_message_boxed_copy (GstSDPMessage * orig)
{
  GstSDPMessage *copy;

  if (!orig)
    return NULL;

  if (gst_sdp_message_copy (orig, &copy) == GST_SDP_OK)
    return copy;

  return NULL;
}

static void
gst_sdp_message_boxed_free (GstSDPMessage * msg)
{
  gst_sdp_message_free (msg);
}

static void
gst_sdp_origin_init (GstSDPOrigin * origin)
{
  FREE_STRING (origin->username);
  FREE_STRING (origin->sess_id);
  FREE_STRING (origin->sess_version);
  FREE_STRING (origin->nettype);
  FREE_STRING (origin->addrtype);
  FREE_STRING (origin->addr);
}

static void
gst_sdp_key_init (GstSDPKey * key)
{
  FREE_STRING (key->type);
  FREE_STRING (key->data);
}

/**
 * gst_sdp_message_new:
 * @msg: (out) (transfer full): pointer to new #GstSDPMessage
 *
 * Allocate a new GstSDPMessage and store the result in @msg.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_new (GstSDPMessage ** msg)
{
  GstSDPMessage *newmsg;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  newmsg = g_new0 (GstSDPMessage, 1);

  *msg = newmsg;

  return gst_sdp_message_init (newmsg);
}

/**
 * gst_sdp_message_new_from_text:
 * @text: A dynamically allocated string representing the SDP description
 * @msg: (out) (transfer full): pointer to new #GstSDPMessage
 *
 * Parse @text and create a new SDPMessage from these.
 *
 * Returns: a #GstSDPResult.
 * Since: 1.16
 */
GstSDPResult
gst_sdp_message_new_from_text (const gchar * text, GstSDPMessage ** msg)
{
  GstSDPResult res;

  if ((res = gst_sdp_message_new (msg)) != GST_SDP_OK)
    return res;

  res =
      gst_sdp_message_parse_buffer ((const guint8 *) text, strlen (text), *msg);

  return res;
}

/**
 * gst_sdp_message_init:
 * @msg: (out caller-allocates): a #GstSDPMessage
 *
 * Initialize @msg so that its contents are as if it was freshly allocated
 * with gst_sdp_message_new(). This function is mostly used to initialize a message
 * allocated on the stack. gst_sdp_message_uninit() undoes this operation.
 *
 * When this function is invoked on newly allocated data (with malloc or on the
 * stack), its contents should be set to 0 before calling this function.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_init (GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  FREE_STRING (msg->version);
  gst_sdp_origin_init (&msg->origin);
  FREE_STRING (msg->session_name);
  FREE_STRING (msg->information);
  FREE_STRING (msg->uri);
  INIT_STR_ARRAY (msg->emails);
  INIT_STR_ARRAY (msg->phones);
  gst_sdp_connection_clear (&msg->connection);
  INIT_ARRAY (msg->bandwidths, GstSDPBandwidth, gst_sdp_bandwidth_clear);
  INIT_ARRAY (msg->times, GstSDPTime, gst_sdp_time_clear);
  INIT_ARRAY (msg->zones, GstSDPZone, gst_sdp_zone_clear);
  gst_sdp_key_init (&msg->key);
  INIT_ARRAY (msg->attributes, GstSDPAttribute, gst_sdp_attribute_clear);
  INIT_ARRAY (msg->medias, GstSDPMedia, gst_sdp_media_uninit);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_uninit:
 * @msg: a #GstSDPMessage
 *
 * Free all resources allocated in @msg. @msg should not be used anymore after
 * this function. This function should be used when @msg was allocated on the
 * stack and initialized with gst_sdp_message_init().
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_uninit (GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_message_init (msg);

  FREE_ARRAY (msg->emails);
  FREE_ARRAY (msg->phones);
  FREE_ARRAY (msg->bandwidths);
  FREE_ARRAY (msg->times);
  FREE_ARRAY (msg->zones);
  FREE_ARRAY (msg->attributes);
  FREE_ARRAY (msg->medias);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_copy:
 * @msg: a #GstSDPMessage
 * @copy: (out) (transfer full): pointer to new #GstSDPMessage
 *
 * Allocate a new copy of @msg and store the result in @copy. The value in
 * @copy should be release with gst_sdp_message_free function.
 *
 * Returns: a #GstSDPResult
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_message_copy (const GstSDPMessage * msg, GstSDPMessage ** copy)
{
  GstSDPMessage *cp;
  guint i, len;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_message_new (copy);

  cp = *copy;

  REPLACE_STRING (cp->version, msg->version);
  gst_sdp_message_set_origin (cp, msg->origin.username, msg->origin.sess_id,
      msg->origin.sess_version, msg->origin.nettype, msg->origin.addrtype,
      msg->origin.addr);
  REPLACE_STRING (cp->session_name, msg->session_name);
  REPLACE_STRING (cp->information, msg->information);
  REPLACE_STRING (cp->uri, msg->uri);

  len = gst_sdp_message_emails_len (msg);
  for (i = 0; i < len; i++) {
    gst_sdp_message_add_email (cp, gst_sdp_message_get_email (msg, i));
  }

  len = gst_sdp_message_phones_len (msg);
  for (i = 0; i < len; i++) {
    gst_sdp_message_add_phone (cp, gst_sdp_message_get_phone (msg, i));
  }

  gst_sdp_message_set_connection (cp, msg->connection.nettype,
      msg->connection.addrtype, msg->connection.address, msg->connection.ttl,
      msg->connection.addr_number);

  len = gst_sdp_message_bandwidths_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPBandwidth *bw = gst_sdp_message_get_bandwidth (msg, i);
    gst_sdp_message_add_bandwidth (cp, bw->bwtype, bw->bandwidth);
  }

  len = gst_sdp_message_times_len (msg);
  for (i = 0; i < len; i++) {
    const gchar **repeat = NULL;
    const GstSDPTime *time = gst_sdp_message_get_time (msg, i);

    if (time->repeat != NULL) {
      guint j;

      repeat = g_malloc0 ((time->repeat->len + 1) * sizeof (gchar *));
      for (j = 0; j < time->repeat->len; j++) {
        repeat[j] = g_array_index (time->repeat, char *, j);
      }
      repeat[j] = NULL;
    }

    gst_sdp_message_add_time (cp, time->start, time->stop, repeat);

    g_free ((gchar **) repeat);
  }

  len = gst_sdp_message_zones_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPZone *zone = gst_sdp_message_get_zone (msg, i);
    gst_sdp_message_add_zone (cp, zone->time, zone->typed_time);
  }

  gst_sdp_message_set_key (cp, msg->key.type, msg->key.data);

  len = gst_sdp_message_attributes_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPAttribute *attr = gst_sdp_message_get_attribute (msg, i);
    gst_sdp_message_add_attribute (cp, attr->key, attr->value);
  }

  len = gst_sdp_message_medias_len (msg);
  for (i = 0; i < len; i++) {
    GstSDPMedia *media_copy;
    const GstSDPMedia *media = gst_sdp_message_get_media (msg, i);

    if (gst_sdp_media_copy (media, &media_copy) == GST_SDP_OK) {
      gst_sdp_message_add_media (cp, media_copy);
      gst_sdp_media_free (media_copy);
    }
  }

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_free:
 * @msg: a #GstSDPMessage
 *
 * Free all resources allocated by @msg. @msg should not be used anymore after
 * this function. This function should be used when @msg was dynamically
 * allocated with gst_sdp_message_new().
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_free (GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_message_uninit (msg);
  g_free (msg);

  return GST_SDP_OK;
}

/**
 * gst_sdp_address_is_multicast:
 * @nettype: a network type
 * @addrtype: an address type
 * @addr: an address
 *
 * Check if the given @addr is a multicast address.
 *
 * Returns: TRUE when @addr is multicast.
 */
gboolean
gst_sdp_address_is_multicast (const gchar * nettype, const gchar * addrtype,
    const gchar * addr)
{
  gboolean ret = FALSE;
  GInetAddress *iaddr;

  g_return_val_if_fail (addr, FALSE);

  /* we only support IN */
  if (nettype && strcmp (nettype, "IN") != 0)
    return FALSE;

  /* guard against parse failures */
  if ((iaddr = g_inet_address_new_from_string (addr)) == NULL)
    return FALSE;

  ret = g_inet_address_get_is_multicast (iaddr);
  g_object_unref (iaddr);

  return ret;
}

/**
 * gst_sdp_message_as_text:
 * @msg: a #GstSDPMessage
 *
 * Convert the contents of @msg to a text string.
 *
 * Returns: (transfer full): A dynamically allocated string representing the SDP description.
 */
gchar *
gst_sdp_message_as_text (const GstSDPMessage * msg)
{
  /* change all vars so they match rfc? */
  GString *lines;
  guint i;

  g_return_val_if_fail (msg != NULL, NULL);

  lines = g_string_new ("");

  if (msg->version)
    g_string_append_printf (lines, "v=%s\r\n", msg->version);

  if (msg->origin.sess_id && msg->origin.sess_version && msg->origin.nettype &&
      msg->origin.addrtype && msg->origin.addr)
    g_string_append_printf (lines, "o=%s %s %s %s %s %s\r\n",
        msg->origin.username ? msg->origin.username : "-", msg->origin.sess_id,
        msg->origin.sess_version, msg->origin.nettype, msg->origin.addrtype,
        msg->origin.addr);

  if (msg->session_name)
    g_string_append_printf (lines, "s=%s\r\n", msg->session_name);

  if (msg->information)
    g_string_append_printf (lines, "i=%s\r\n", msg->information);

  if (msg->uri)
    g_string_append_printf (lines, "u=%s\r\n", msg->uri);

  for (i = 0; i < gst_sdp_message_emails_len (msg); i++)
    g_string_append_printf (lines, "e=%s\r\n",
        gst_sdp_message_get_email (msg, i));

  for (i = 0; i < gst_sdp_message_phones_len (msg); i++)
    g_string_append_printf (lines, "p=%s\r\n",
        gst_sdp_message_get_phone (msg, i));

  if (msg->connection.nettype && msg->connection.addrtype &&
      msg->connection.address) {
    g_string_append_printf (lines, "c=%s %s %s", msg->connection.nettype,
        msg->connection.addrtype, msg->connection.address);
    if (gst_sdp_address_is_multicast (msg->connection.nettype,
            msg->connection.addrtype, msg->connection.address)) {
      /* only add ttl for IP4 */
      if (strcmp (msg->connection.addrtype, "IP4") == 0)
        g_string_append_printf (lines, "/%u", msg->connection.ttl);
      if (msg->connection.addr_number > 1)
        g_string_append_printf (lines, "/%u", msg->connection.addr_number);
    }
    g_string_append_printf (lines, "\r\n");
  }

  for (i = 0; i < gst_sdp_message_bandwidths_len (msg); i++) {
    const GstSDPBandwidth *bandwidth = gst_sdp_message_get_bandwidth (msg, i);

    g_string_append_printf (lines, "b=%s:%u\r\n", bandwidth->bwtype,
        bandwidth->bandwidth);
  }

  if (gst_sdp_message_times_len (msg) == 0) {
    g_string_append_printf (lines, "t=0 0\r\n");
  } else {
    for (i = 0; i < gst_sdp_message_times_len (msg); i++) {
      const GstSDPTime *times = gst_sdp_message_get_time (msg, i);

      g_string_append_printf (lines, "t=%s %s\r\n", times->start, times->stop);

      if (times->repeat != NULL) {
        guint j;

        g_string_append_printf (lines, "r=%s",
            g_array_index (times->repeat, gchar *, 0));
        for (j = 1; j < times->repeat->len; j++)
          g_string_append_printf (lines, " %s",
              g_array_index (times->repeat, gchar *, j));
        g_string_append_printf (lines, "\r\n");
      }
    }
  }

  if (gst_sdp_message_zones_len (msg) > 0) {
    const GstSDPZone *zone = gst_sdp_message_get_zone (msg, 0);

    g_string_append_printf (lines, "z=%s %s", zone->time, zone->typed_time);
    for (i = 1; i < gst_sdp_message_zones_len (msg); i++) {
      zone = gst_sdp_message_get_zone (msg, i);
      g_string_append_printf (lines, " %s %s", zone->time, zone->typed_time);
    }
    g_string_append_printf (lines, "\r\n");
  }

  if (msg->key.type) {
    g_string_append_printf (lines, "k=%s", msg->key.type);
    if (msg->key.data)
      g_string_append_printf (lines, ":%s", msg->key.data);
    g_string_append_printf (lines, "\r\n");
  }

  for (i = 0; i < gst_sdp_message_attributes_len (msg); i++) {
    const GstSDPAttribute *attr = gst_sdp_message_get_attribute (msg, i);

    if (attr->key) {
      g_string_append_printf (lines, "a=%s", attr->key);
      if (attr->value && attr->value[0] != '\0')
        g_string_append_printf (lines, ":%s", attr->value);
      g_string_append_printf (lines, "\r\n");
    }
  }

  for (i = 0; i < gst_sdp_message_medias_len (msg); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (msg, i);
    gchar *sdp_media_str;

    sdp_media_str = gst_sdp_media_as_text (media);
    g_string_append_printf (lines, "%s", sdp_media_str);
    g_free (sdp_media_str);
  }

  return g_string_free (lines, FALSE);
}

static int
hex_to_int (gchar c)
{
  return c >= '0' && c <= '9' ? c - '0'
      : c >= 'A' && c <= 'F' ? c - 'A' + 10
      : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
}

/**
 * gst_sdp_message_parse_uri:
 * @uri: the start of the uri
 * @msg: (transfer none): the result #GstSDPMessage
 *
 * Parse the null-terminated @uri and store the result in @msg.
 *
 * The uri should be of the form:
 *
 *  scheme://[address[:ttl=ttl][:noa=noa]]/[sessionname]
 *               [#type=value *[&type=value]]
 *
 *  where value is url encoded. This looslely resembles
 *  http://tools.ietf.org/html/draft-fujikawa-sdp-url-01
 *
 * Returns: #GST_SDP_OK on success.
 */
GstSDPResult
gst_sdp_message_parse_uri (const gchar * uri, GstSDPMessage * msg)
{
  GstSDPResult res;
  gchar *message;
  const gchar *colon, *slash, *hash, *p;
  GString *lines;

  g_return_val_if_fail (uri != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  colon = strstr (uri, "://");
  if (!colon)
    goto no_colon;

  /* FIXME connection info goes here */

  slash = strstr (colon + 3, "/");
  if (!slash)
    goto no_slash;

  /* FIXME session name goes here */

  hash = strstr (slash + 1, "#");
  if (!hash)
    goto no_hash;

  lines = g_string_new ("");

  /* unescape */
  for (p = hash + 1; *p; p++) {
    if (*p == '&')
      g_string_append_printf (lines, "\r\n");
    else if (*p == '+')
      g_string_append_c (lines, ' ');
    else if (*p == '%') {
      gchar a, b;

      if ((a = p[1])) {
        if ((b = p[2])) {
          g_string_append_c (lines, (hex_to_int (a) << 4) | hex_to_int (b));
          p += 2;
        }
      } else {
        p++;
      }
    } else
      g_string_append_c (lines, *p);
  }

  message = g_string_free (lines, FALSE);
  res =
      gst_sdp_message_parse_buffer ((const guint8 *) message, strlen (message),
      msg);
  g_free (message);

  return res;

  /* ERRORS */
no_colon:
  {
    return GST_SDP_EINVAL;
  }
no_slash:
  {
    return GST_SDP_EINVAL;
  }
no_hash:
  {
    return GST_SDP_EINVAL;
  }
}

static const guchar acceptable[96] = {
  /* X0   X1    X2    X3    X4    X5    X6    X7    X8    X9    XA    XB    XC    XD    XE    XF */
  0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00,       /* 2X  !"#$%&'()*+,-./   */
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 3X 0123456789:;<=>?   */
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,       /* 4X @ABCDEFGHIJKLMNO   */
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,       /* 5X PQRSTUVWXYZ[\]^_   */
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,       /* 6X `abcdefghijklmno   */
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00        /* 7X pqrstuvwxyz{|}~DEL */
};

static const gchar hex[16] = "0123456789ABCDEF";

#define ACCEPTABLE_CHAR(a) (((guchar)(a))>=32 && ((guchar)(a))<128 && acceptable[(((guchar)a))-32])

/**
 * gst_sdp_message_as_uri:
 * @scheme: the uri scheme
 * @msg: the #GstSDPMessage
 *
 * Creates a uri from @msg with the given @scheme. The uri has the format:
 *
 *  \@scheme:///[#type=value *[&type=value]]
 *
 *  Where each value is url encoded.
 *
 * Returns: (transfer full): a uri for @msg.
 */
gchar *
gst_sdp_message_as_uri (const gchar * scheme, const GstSDPMessage * msg)
{
  gchar *serialized, *p;
  gchar *res;
  GString *lines;
  gboolean first;

  g_return_val_if_fail (scheme != NULL, NULL);
  g_return_val_if_fail (msg != NULL, NULL);

  serialized = gst_sdp_message_as_text (msg);

  lines = g_string_new ("");
  g_string_append_printf (lines, "%s:///#", scheme);

  /* now escape */
  first = TRUE;
  for (p = serialized; *p; p++) {
    if (first) {
      g_string_append_printf (lines, "%c=", *p);
      if (*(p + 1))
        p++;
      first = FALSE;
      continue;
    }
    if (*p == '\r')
      continue;
    else if (*p == '\n') {
      if (*(p + 1))
        g_string_append_c (lines, '&');
      first = TRUE;
    } else if (*p == ' ')
      g_string_append_c (lines, '+');
    else if (ACCEPTABLE_CHAR (*p))
      g_string_append_c (lines, *p);
    else {
      /* escape */
      g_string_append_printf (lines, "%%%c%c", hex[*p >> 4], hex[*p & 0xf]);
    }
  }

  res = g_string_free (lines, FALSE);
  g_free (serialized);

  return res;
}

/**
 * gst_sdp_message_set_version:
 * @msg: a #GstSDPMessage
 * @version: the version
 *
 * Set the version in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_SETTER (version);
/**
 * gst_sdp_message_get_version:
 * @msg: a #GstSDPMessage
 *
 * Get the version in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_GETTER (version);

/**
 * gst_sdp_message_set_origin:
 * @msg: a #GstSDPMessage
 * @username: the user name
 * @sess_id: a session id
 * @sess_version: a session version
 * @nettype: a network type
 * @addrtype: an address type
 * @addr: an address
 *
 * Configure the SDP origin in @msg with the given parameters.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_message_set_origin (GstSDPMessage * msg, const gchar * username,
    const gchar * sess_id, const gchar * sess_version, const gchar * nettype,
    const gchar * addrtype, const gchar * addr)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  REPLACE_STRING (msg->origin.username, username);
  REPLACE_STRING (msg->origin.sess_id, sess_id);
  REPLACE_STRING (msg->origin.sess_version, sess_version);
  REPLACE_STRING (msg->origin.nettype, nettype);
  REPLACE_STRING (msg->origin.addrtype, addrtype);
  REPLACE_STRING (msg->origin.addr, addr);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_get_origin:
 * @msg: a #GstSDPMessage
 *
 * Get the origin of @msg.
 *
 * Returns: a #GstSDPOrigin. The result remains valid as long as @msg is valid.
 */
const GstSDPOrigin *
gst_sdp_message_get_origin (const GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, NULL);

  return &msg->origin;
}

/**
 * gst_sdp_message_set_session_name:
 * @msg: a #GstSDPMessage
 * @session_name: the session name
 *
 * Set the session name in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_SETTER (session_name);
/**
 * gst_sdp_message_get_session_name:
 * @msg: a #GstSDPMessage
 *
 * Get the session name in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_GETTER (session_name);
/**
 * gst_sdp_message_set_information:
 * @msg: a #GstSDPMessage
 * @information: the information
 *
 * Set the information in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_SETTER (information);
/**
 * gst_sdp_message_get_information:
 * @msg: a #GstSDPMessage
 *
 * Get the information in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_GETTER (information);
/**
 * gst_sdp_message_set_uri:
 * @msg: a #GstSDPMessage
 * @uri: the URI
 *
 * Set the URI in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_SETTER (uri);
/**
 * gst_sdp_message_get_uri:
 * @msg: a #GstSDPMessage
 *
 * Get the URI in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STRING_GETTER (uri);

/**
 * gst_sdp_message_emails_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of emails in @msg.
 *
 * Returns: the number of emails in @msg.
 */
DEFINE_ARRAY_LEN (emails);
/**
 * gst_sdp_message_get_email:
 * @msg: a #GstSDPMessage
 * @idx: an email index
 *
 * Get the email with number @idx from @msg.
 *
 * Returns: the email at position @idx.
 */
DEFINE_STR_ARRAY_GETTER (email, emails);

/**
 * gst_sdp_message_insert_email:
 * @msg: a #GstSDPMessage
 * @idx: an index
 * @email: an email
 *
 * Insert @email into the array of emails in @msg at index @idx.
 * When -1 is given as @idx, the email is inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_INSERT (email, emails);

/**
 * gst_sdp_message_replace_email:
 * @msg: a #GstSDPMessage
 * @idx: an email index
 * @email: an email
 *
 * Replace the email in @msg at index @idx with @email.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_REPLACE (email, emails);

/**
 * gst_sdp_message_remove_email:
 * @msg: a #GstSDPMessage
 * @idx: an email index
 *
 * Remove the email in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_REMOVE (email, emails);

/**
 * gst_sdp_message_add_email:
 * @msg: a #GstSDPMessage
 * @email: an email
 *
 * Add @email to the list of emails in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STR_ARRAY_ADDER (email, emails);

/**
 * gst_sdp_message_phones_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of phones in @msg.
 *
 * Returns: the number of phones in @msg.
 */
DEFINE_ARRAY_LEN (phones);
/**
 * gst_sdp_message_get_phone:
 * @msg: a #GstSDPMessage
 * @idx: a phone index
 *
 * Get the phone with number @idx from @msg.
 *
 * Returns: the phone at position @idx.
 */
DEFINE_STR_ARRAY_GETTER (phone, phones);

/**
 * gst_sdp_message_insert_phone:
 * @msg: a #GstSDPMessage
 * @idx: a phone index
 * @phone: a phone
 *
 * Insert @phone into the array of phone numbers in @msg at index @idx.
 * When -1 is given as @idx, the phone is inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_INSERT (phone, phones);

/**
 * gst_sdp_message_replace_phone:
 * @msg: a #GstSDPMessage
 * @idx: a phone index
 * @phone: a phone
 *
 * Replace the phone number in @msg at index @idx with @phone.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_REPLACE (phone, phones);

/**
 * gst_sdp_message_remove_phone:
 * @msg: a #GstSDPMessage
 * @idx: a phone index
 *
 * Remove the phone number in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_STR_ARRAY_REMOVE (phone, phones);

/**
 * gst_sdp_message_add_phone:
 * @msg: a #GstSDPMessage
 * @phone: a phone
 *
 * Add @phone to the list of phones in @msg.
 *
 * Returns: a #GstSDPResult.
 */
DEFINE_STR_ARRAY_ADDER (phone, phones);


/**
 * gst_sdp_message_set_connection:
 * @msg: a #GstSDPMessage
 * @nettype: the type of network. "IN" is defined to have the meaning
 * "Internet".
 * @addrtype: the type of address.
 * @address: the address
 * @ttl: the time to live of the address
 * @addr_number: the number of layers
 *
 * Configure the SDP connection in @msg with the given parameters.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_set_connection (GstSDPMessage * msg, const gchar * nettype,
    const gchar * addrtype, const gchar * address, guint ttl, guint addr_number)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  REPLACE_STRING (msg->connection.nettype, nettype);
  REPLACE_STRING (msg->connection.addrtype, addrtype);
  REPLACE_STRING (msg->connection.address, address);
  msg->connection.ttl = ttl;
  msg->connection.addr_number = addr_number;

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_get_connection:
 * @msg: a #GstSDPMessage
 *
 * Get the connection of @msg.
 *
 * Returns: a #GstSDPConnection. The result remains valid as long as @msg is valid.
 */
const GstSDPConnection *
gst_sdp_message_get_connection (const GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, NULL);

  return &msg->connection;
}

/**
 * gst_sdp_bandwidth_set:
 * @bw: a #GstSDPBandwidth
 * @bwtype: the bandwidth modifier type
 * @bandwidth: the bandwidth in kilobits per second
 *
 * Set bandwidth information in @bw.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_bandwidth_set (GstSDPBandwidth * bw, const gchar * bwtype,
    guint bandwidth)
{
  g_return_val_if_fail (bw != NULL, GST_SDP_EINVAL);

  bw->bwtype = g_strdup (bwtype);
  bw->bandwidth = bandwidth;
  return GST_SDP_OK;
}

/**
 * gst_sdp_bandwidth_clear:
 * @bw: a #GstSDPBandwidth
 *
 * Reset the bandwidth information in @bw.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_bandwidth_clear (GstSDPBandwidth * bw)
{
  g_return_val_if_fail (bw != NULL, GST_SDP_EINVAL);

  FREE_STRING (bw->bwtype);
  bw->bandwidth = 0;
  return GST_SDP_OK;
}

/**
 * gst_sdp_message_bandwidths_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of bandwidth information in @msg.
 *
 * Returns: the number of bandwidth information in @msg.
 */
DEFINE_ARRAY_LEN (bandwidths);
/**
 * gst_sdp_message_get_bandwidth:
 * @msg: a #GstSDPMessage
 * @idx: the bandwidth index
 *
 * Get the bandwidth at index @idx from @msg.
 *
 * Returns: a #GstSDPBandwidth.
 */
DEFINE_ARRAY_GETTER (bandwidth, bandwidths, GstSDPBandwidth);

#define DUP_BANDWIDTH(v, val) memcpy (v, val, sizeof (GstSDPBandwidth))
#define FREE_BANDWIDTH(v) gst_sdp_bandwidth_clear(v)

/**
 * gst_sdp_message_insert_bandwidth:
 * @msg: a #GstSDPMessage
 * @idx: an index
 * @bw: the bandwidth
 *
 * Insert bandwidth parameters into the array of bandwidths in @msg
 * at index @idx.
 * When -1 is given as @idx, the bandwidth is inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_INSERT (bandwidth, bandwidths, GstSDPBandwidth *, DUP_BANDWIDTH,
    GstSDPBandwidth);

/**
 * gst_sdp_message_replace_bandwidth:
 * @msg: a #GstSDPMessage
 * @idx: the bandwidth index
 * @bw: the bandwidth
 *
 * Replace the bandwidth information in @msg at index @idx with @bw.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REPLACE (bandwidth, bandwidths, GstSDPBandwidth *, FREE_BANDWIDTH,
    DUP_BANDWIDTH, GstSDPBandwidth);

/**
 * gst_sdp_message_remove_bandwidth:
 * @msg: a #GstSDPMessage
 * @idx: the bandwidth index
 *
 * Remove the bandwidth information in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REMOVE (bandwidth, bandwidths, GstSDPBandwidth, FREE_BANDWIDTH);

/**
 * gst_sdp_message_add_bandwidth:
 * @msg: a #GstSDPMessage
 * @bwtype: the bandwidth modifier type
 * @bandwidth: the bandwidth in kilobits per second
 *
 * Add the specified bandwidth information to @msg.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_add_bandwidth (GstSDPMessage * msg, const gchar * bwtype,
    guint bandwidth)
{
  GstSDPBandwidth bw;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_bandwidth_set (&bw, bwtype, bandwidth);
  return gst_sdp_message_insert_bandwidth (msg, -1, &bw);
}

/**
 * gst_sdp_time_set:
 * @t: a #GstSDPTime
 * @start: the start time
 * @stop: the stop time
 * @repeat: (array zero-terminated=1): the repeat times
 *
 * Set time information @start, @stop and @repeat in @t.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_time_set (GstSDPTime * t, const gchar * start,
    const gchar * stop, const gchar ** repeat)
{
  g_return_val_if_fail (t != NULL, GST_SDP_EINVAL);

  t->start = g_strdup (start);
  t->stop = g_strdup (stop);
  if (repeat) {
    t->repeat = g_array_new (FALSE, TRUE, sizeof (gchar *));
    for (; *repeat; repeat++) {
      gchar *r = g_strdup (*repeat);

      g_array_append_val (t->repeat, r);
    }
  } else
    t->repeat = NULL;

  return GST_SDP_OK;
}

/**
 * gst_sdp_time_clear:
 * @t: a #GstSDPTime
 *
 * Reset the time information in @t.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_time_clear (GstSDPTime * t)
{
  g_return_val_if_fail (t != NULL, GST_SDP_EINVAL);

  FREE_STRING (t->start);
  FREE_STRING (t->stop);
  INIT_STR_ARRAY (t->repeat);
  FREE_ARRAY (t->repeat);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_times_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of time information entries in @msg.
 *
 * Returns: the number of time information entries in @msg.
 */
DEFINE_ARRAY_LEN (times);

/**
 * gst_sdp_message_get_time:
 * @msg: a #GstSDPMessage
 * @idx: the time index
 *
 * Get time information with index @idx from @msg.
 *
 * Returns: a #GstSDPTime.
 */
DEFINE_ARRAY_GETTER (time, times, GstSDPTime);

#define DUP_TIME(v, val) memcpy (v, val, sizeof (GstSDPTime))
#define FREE_TIME(v) gst_sdp_time_clear(v)

/**
 * gst_sdp_message_insert_time:
 * @msg: a #GstSDPMessage
 * @idx: an index
 * @t: a #GstSDPTime
 *
 * Insert time parameters into the array of times in @msg
 * at index @idx.
 * When -1 is given as @idx, the times are inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_INSERT (time, times, GstSDPTime *, DUP_TIME, GstSDPTime);

/**
 * gst_sdp_message_replace_time:
 * @msg: a #GstSDPMessage
 * @idx: the index
 * @t: a #GstSDPTime
 *
 * Replace the time information in @msg at index @idx with @t.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REPLACE (time, times, GstSDPTime *, FREE_TIME,
    DUP_TIME, GstSDPTime);

/**
 * gst_sdp_message_remove_time:
 * @msg: a #GstSDPMessage
 * @idx: the index
 *
 * Remove the time information in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REMOVE (time, times, GstSDPTime, FREE_TIME);

/**
 * gst_sdp_message_add_time:
 * @msg: a #GstSDPMessage
 * @start: the start time
 * @stop: the stop time
 * @repeat: (array zero-terminated=1): the repeat times
 *
 * Add time information @start and @stop to @msg.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_add_time (GstSDPMessage * msg, const gchar * start,
    const gchar * stop, const gchar ** repeat)
{
  GstSDPTime times;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_time_set (&times, start, stop, repeat);
  g_array_append_val (msg->times, times);

  return GST_SDP_OK;
}

/**
 * gst_sdp_zone_set:
 * @zone: a #GstSDPZone
 * @adj_time: the NTP time that a time zone adjustment happens
 * @typed_time: the offset from the time when the session was first scheduled
 *
 * Set zone information in @zone.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_zone_set (GstSDPZone * zone, const gchar * adj_time,
    const gchar * typed_time)
{
  g_return_val_if_fail (zone != NULL, GST_SDP_EINVAL);

  zone->time = g_strdup (adj_time);
  zone->typed_time = g_strdup (typed_time);
  return GST_SDP_OK;
}

/**
 * gst_sdp_zone_clear:
 * @zone: a #GstSDPZone
 *
 * Reset the zone information in @zone.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_zone_clear (GstSDPZone * zone)
{
  g_return_val_if_fail (zone != NULL, GST_SDP_EINVAL);

  FREE_STRING (zone->time);
  FREE_STRING (zone->typed_time);
  return GST_SDP_OK;
}

/**
 * gst_sdp_message_zones_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of time zone information entries in @msg.
 *
 * Returns: the number of time zone information entries in @msg.
 */
DEFINE_ARRAY_LEN (zones);
/**
 * gst_sdp_message_get_zone:
 * @msg: a #GstSDPMessage
 * @idx: the zone index
 *
 * Get time zone information with index @idx from @msg.
 *
 * Returns: a #GstSDPZone.
 */
DEFINE_ARRAY_GETTER (zone, zones, GstSDPZone);

#define DUP_ZONE(v, val) memcpy (v, val, sizeof (GstSDPZone))
#define FREE_ZONE(v) gst_sdp_zone_clear(v)

/**
 * gst_sdp_message_insert_zone:
 * @msg: a #GstSDPMessage
 * @idx: an index
 * @zone: a #GstSDPZone
 *
 * Insert zone parameters into the array of zones in @msg
 * at index @idx.
 * When -1 is given as @idx, the zone is inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_INSERT (zone, zones, GstSDPZone *, DUP_ZONE, GstSDPZone);

/**
 * gst_sdp_message_replace_zone:
 * @msg: a #GstSDPMessage
 * @idx: the index
 * @zone: a #GstSDPZone
 *
 * Replace the zone information in @msg at index @idx with @zone.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REPLACE (zone, zones, GstSDPZone *, FREE_ZONE,
    DUP_ZONE, GstSDPZone);

/**
 * gst_sdp_message_remove_zone:
 * @msg: a #GstSDPMessage
 * @idx: the index
 *
 * Remove the zone information in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REMOVE (zone, zones, GstSDPZone, FREE_ZONE);

/**
 * gst_sdp_message_add_zone:
 * @msg: a #GstSDPMessage
 * @adj_time: the NTP time that a time zone adjustment happens
 * @typed_time: the offset from the time when the session was first scheduled
 *
 * Add time zone information to @msg.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_add_zone (GstSDPMessage * msg, const gchar * adj_time,
    const gchar * typed_time)
{
  GstSDPZone zone;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  gst_sdp_zone_set (&zone, adj_time, typed_time);
  g_array_append_val (msg->zones, zone);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_set_key:
 * @msg: a #GstSDPMessage
 * @type: the encryption type
 * @data: the encryption data
 *
 * Adds the encryption information to @msg.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_set_key (GstSDPMessage * msg, const gchar * type,
    const gchar * data)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  REPLACE_STRING (msg->key.type, type);
  REPLACE_STRING (msg->key.data, data);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_get_key:
 * @msg: a #GstSDPMessage
 *
 * Get the encryption information from @msg.
 *
 * Returns: a #GstSDPKey.
 */
const GstSDPKey *
gst_sdp_message_get_key (const GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, NULL);

  return &msg->key;
}

/**
 * gst_sdp_attribute_set:
 * @attr: a #GstSDPAttribute
 * @key: the key
 * @value: (nullable): the value
 *
 * Set the attribute with @key and @value.
 *
 * Returns: @GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_attribute_set (GstSDPAttribute * attr, const gchar * key,
    const gchar * value)
{
  g_return_val_if_fail (attr != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (key != NULL, GST_SDP_EINVAL);

  attr->key = g_strdup (key);
  attr->value = g_strdup (value);
  return GST_SDP_OK;
}

/**
 * gst_sdp_attribute_clear:
 * @attr: a #GstSDPAttribute
 *
 * Clear the attribute.
 *
 * Returns: @GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_attribute_clear (GstSDPAttribute * attr)
{
  g_return_val_if_fail (attr != NULL, GST_SDP_EINVAL);

  FREE_STRING (attr->key);
  FREE_STRING (attr->value);
  return GST_SDP_OK;
}

/**
 * gst_sdp_message_attributes_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of attributes in @msg.
 *
 * Returns: the number of attributes in @msg.
 */
DEFINE_ARRAY_LEN (attributes);

/**
 * gst_sdp_message_get_attribute:
 * @msg: a #GstSDPMessage
 * @idx: the index
 *
 * Get the attribute at position @idx in @msg.
 *
 * Returns: the #GstSDPAttribute at position @idx.
 */
DEFINE_ARRAY_GETTER (attribute, attributes, GstSDPAttribute);

/**
 * gst_sdp_message_get_attribute_val_n:
 * @msg: a #GstSDPMessage
 * @key: the key
 * @nth: the index
 *
 * Get the @nth attribute with key @key in @msg.
 *
 * Returns: (nullable): the attribute value of the @nth attribute with @key.
 */
const gchar *
gst_sdp_message_get_attribute_val_n (const GstSDPMessage * msg,
    const gchar * key, guint nth)
{
  guint i;

  g_return_val_if_fail (msg != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (i = 0; i < msg->attributes->len; i++) {
    GstSDPAttribute *attr;

    attr = &g_array_index (msg->attributes, GstSDPAttribute, i);
    if (!strcmp (attr->key, key)) {
      if (nth == 0)
        return attr->value;
      else
        nth--;
    }
  }
  return NULL;
}

/**
 * gst_sdp_message_get_attribute_val:
 * @msg: a #GstSDPMessage
 * @key: the key
 *
 * Get the first attribute with key @key in @msg.
 *
 * Returns: (nullable): the attribute value of the first attribute with @key.
 */
const gchar *
gst_sdp_message_get_attribute_val (const GstSDPMessage * msg, const gchar * key)
{
  return gst_sdp_message_get_attribute_val_n (msg, key, 0);
}

#define DUP_ATTRIBUTE(v, val) memcpy (v, val, sizeof (GstSDPAttribute))
#define FREE_ATTRIBUTE(v) gst_sdp_attribute_clear(v)

/**
 * gst_sdp_message_insert_attribute:
 * @msg: a #GstSDPMessage
 * @idx: an index
 * @attr: a #GstSDPAttribute
 *
 * Insert attribute into the array of attributes in @msg
 * at index @idx.
 * When -1 is given as @idx, the attribute is inserted at the end.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_INSERT (attribute, attributes, GstSDPAttribute *, DUP_ATTRIBUTE,
    GstSDPAttribute);

/**
 * gst_sdp_message_replace_attribute:
 * @msg: a #GstSDPMessage
 * @idx: the index
 * @attr: a #GstSDPAttribute
 *
 * Replace the attribute in @msg at index @idx with @attr.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REPLACE (attribute, attributes, GstSDPAttribute *, FREE_ATTRIBUTE,
    DUP_ATTRIBUTE, GstSDPAttribute);

/**
 * gst_sdp_message_remove_attribute:
 * @msg: a #GstSDPMessage
 * @idx: the index
 *
 * Remove the attribute in @msg at index @idx.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.2
 */
DEFINE_ARRAY_REMOVE (attribute, attributes, GstSDPAttribute, FREE_ATTRIBUTE);

/**
 * gst_sdp_message_add_attribute:
 * @msg: a #GstSDPMessage
 * @key: the key
 * @value: (nullable): the value
 *
 * Add the attribute with @key and @value to @msg.
 *
 * Returns: @GST_SDP_OK.
 */
GstSDPResult
gst_sdp_message_add_attribute (GstSDPMessage * msg, const gchar * key,
    const gchar * value)
{
  GstSDPAttribute attr;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (key != NULL, GST_SDP_EINVAL);

  gst_sdp_attribute_set (&attr, key, value);
  g_array_append_val (msg->attributes, attr);

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_medias_len:
 * @msg: a #GstSDPMessage
 *
 * Get the number of media descriptions in @msg.
 *
 * Returns: the number of media descriptions in @msg.
 */
DEFINE_ARRAY_LEN (medias);
/**
 * gst_sdp_message_get_media:
 * @msg: a #GstSDPMessage
 * @idx: the index
 *
 * Get the media description at index @idx in @msg.
 *
 * Returns: a #GstSDPMedia.
 */
DEFINE_ARRAY_GETTER (media, medias, GstSDPMedia);

/**
 * gst_sdp_message_add_media:
 * @msg: a #GstSDPMessage
 * @media: a #GstSDPMedia to add
 *
 * Adds @media to the array of medias in @msg. This function takes ownership of
 * the contents of @media so that @media will have to be reinitialized with
 * gst_sdp_media_init() before it can be used again.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_add_media (GstSDPMessage * msg, GstSDPMedia * media)
{
  guint len;
  GstSDPMedia *nmedia;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  len = msg->medias->len;
  g_array_set_size (msg->medias, len + 1);
  nmedia = &g_array_index (msg->medias, GstSDPMedia, len);

  memcpy (nmedia, media, sizeof (GstSDPMedia));
  memset (media, 0, sizeof (GstSDPMedia));

  return GST_SDP_OK;
}

/**
 * gst_sdp_message_remove_media:
 * @msg: a #GstSDPMessage
 * @idx: the media index
 *
 * Remove the media at @idx from the array of medias in @msg if found.
 *
 * Returns: #GST_SDP_OK when the specified media is found at @idx and removed,
 * #GST_SDP_EINVAL otherwise.
 *
 * Since: 1.24
 */
GstSDPResult
gst_sdp_message_remove_media (GstSDPMessage * msg, guint idx)
{
  GstSDPMedia *media = NULL;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx <= gst_sdp_message_medias_len (msg),
      GST_SDP_EINVAL);

  media = &g_array_index (msg->medias, GstSDPMedia, idx);
  gst_sdp_media_uninit (media);
  g_array_remove_index (msg->medias, idx);
  return GST_SDP_OK;
}

/* media access */

/**
 * gst_sdp_media_new:
 * @media: (out) (transfer full): pointer to new #GstSDPMedia
 *
 * Allocate a new GstSDPMedia and store the result in @media.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_new (GstSDPMedia ** media)
{
  GstSDPMedia *newmedia;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  newmedia = g_new0 (GstSDPMedia, 1);

  *media = newmedia;

  return gst_sdp_media_init (newmedia);
}

/**
 * gst_sdp_media_init:
 * @media: (out caller-allocates): a #GstSDPMedia
 *
 * Initialize @media so that its contents are as if it was freshly allocated
 * with gst_sdp_media_new(). This function is mostly used to initialize a media
 * allocated on the stack. gst_sdp_media_uninit() undoes this operation.
 *
 * When this function is invoked on newly allocated data (with malloc or on the
 * stack), its contents should be set to 0 before calling this function.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_init (GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  FREE_STRING (media->media);
  media->port = 0;
  media->num_ports = 0;
  FREE_STRING (media->proto);
  INIT_STR_ARRAY (media->fmts);
  FREE_STRING (media->information);
  INIT_ARRAY (media->connections, GstSDPConnection, gst_sdp_connection_clear);
  INIT_ARRAY (media->bandwidths, GstSDPBandwidth, gst_sdp_bandwidth_clear);
  gst_sdp_key_init (&media->key);
  INIT_ARRAY (media->attributes, GstSDPAttribute, gst_sdp_attribute_clear);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_uninit:
 * @media: a #GstSDPMedia
 *
 * Free all resources allocated in @media. @media should not be used anymore after
 * this function. This function should be used when @media was allocated on the
 * stack and initialized with gst_sdp_media_init().
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_uninit (GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  gst_sdp_media_init (media);
  FREE_ARRAY (media->fmts);
  FREE_ARRAY (media->connections);
  FREE_ARRAY (media->bandwidths);
  FREE_ARRAY (media->attributes);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_free:
 * @media: a #GstSDPMedia
 *
 * Free all resources allocated by @media. @media should not be used anymore after
 * this function. This function should be used when @media was dynamically
 * allocated with gst_sdp_media_new().
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_free (GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  gst_sdp_media_uninit (media);
  g_free (media);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_copy:
 * @media: a #GstSDPMedia
 * @copy: (out) (transfer full): pointer to new #GstSDPMedia
 *
 * Allocate a new copy of @media and store the result in @copy. The value in
 * @copy should be release with gst_sdp_media_free function.
 *
 * Returns: a #GstSDPResult
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_copy (const GstSDPMedia * media, GstSDPMedia ** copy)
{
  GstSDPMedia *cp;
  guint i, len;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  gst_sdp_media_new (copy);

  cp = *copy;

  REPLACE_STRING (cp->media, media->media);
  cp->port = media->port;
  cp->num_ports = media->num_ports;
  REPLACE_STRING (cp->proto, media->proto);

  len = gst_sdp_media_formats_len (media);
  for (i = 0; i < len; i++) {
    gst_sdp_media_add_format (cp, gst_sdp_media_get_format (media, i));
  }

  REPLACE_STRING (cp->information, media->information);

  len = gst_sdp_media_connections_len (media);
  for (i = 0; i < len; i++) {
    const GstSDPConnection *connection =
        gst_sdp_media_get_connection (media, i);
    gst_sdp_media_add_connection (cp, connection->nettype, connection->addrtype,
        connection->address, connection->ttl, connection->addr_number);
  }

  len = gst_sdp_media_bandwidths_len (media);
  for (i = 0; i < len; i++) {
    const GstSDPBandwidth *bw = gst_sdp_media_get_bandwidth (media, i);
    gst_sdp_media_add_bandwidth (cp, bw->bwtype, bw->bandwidth);
  }

  gst_sdp_media_set_key (cp, media->key.type, media->key.data);

  len = gst_sdp_media_attributes_len (media);
  for (i = 0; i < len; i++) {
    const GstSDPAttribute *att = gst_sdp_media_get_attribute (media, i);
    gst_sdp_media_add_attribute (cp, att->key, att->value);
  }

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_as_text:
 * @media: a #GstSDPMedia
 *
 * Convert the contents of @media to a text string.
 *
 * Returns: (transfer full): A dynamically allocated string representing the media.
 */
gchar *
gst_sdp_media_as_text (const GstSDPMedia * media)
{
  GString *lines;
  guint i;

  g_return_val_if_fail (media != NULL, NULL);

  lines = g_string_new ("");

  if (media->media)
    g_string_append_printf (lines, "m=%s", media->media);

  g_string_append_printf (lines, " %u", media->port);

  if (media->num_ports > 1)
    g_string_append_printf (lines, "/%u", media->num_ports);

  g_string_append_printf (lines, " %s", media->proto);

  for (i = 0; i < gst_sdp_media_formats_len (media); i++)
    g_string_append_printf (lines, " %s", gst_sdp_media_get_format (media, i));
  g_string_append_printf (lines, "\r\n");

  if (media->information)
    g_string_append_printf (lines, "i=%s", media->information);

  for (i = 0; i < gst_sdp_media_connections_len (media); i++) {
    const GstSDPConnection *conn = gst_sdp_media_get_connection (media, i);

    if (conn->nettype && conn->addrtype && conn->address) {
      g_string_append_printf (lines, "c=%s %s %s", conn->nettype,
          conn->addrtype, conn->address);
      if (gst_sdp_address_is_multicast (conn->nettype, conn->addrtype,
              conn->address)) {
        /* only add TTL for IP4 multicast */
        if (strcmp (conn->addrtype, "IP4") == 0)
          g_string_append_printf (lines, "/%u", conn->ttl);
        if (conn->addr_number > 1)
          g_string_append_printf (lines, "/%u", conn->addr_number);
      }
      g_string_append_printf (lines, "\r\n");
    }
  }

  for (i = 0; i < gst_sdp_media_bandwidths_len (media); i++) {
    const GstSDPBandwidth *bandwidth = gst_sdp_media_get_bandwidth (media, i);

    g_string_append_printf (lines, "b=%s:%u\r\n", bandwidth->bwtype,
        bandwidth->bandwidth);
  }

  if (media->key.type) {
    g_string_append_printf (lines, "k=%s", media->key.type);
    if (media->key.data)
      g_string_append_printf (lines, ":%s", media->key.data);
    g_string_append_printf (lines, "\r\n");
  }

  for (i = 0; i < gst_sdp_media_attributes_len (media); i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);

    if (attr->key) {
      g_string_append_printf (lines, "a=%s", attr->key);
      if (attr->value && attr->value[0] != '\0')
        g_string_append_printf (lines, ":%s", attr->value);
      g_string_append_printf (lines, "\r\n");
    }
  }

  return g_string_free (lines, FALSE);
}

/**
 * gst_sdp_media_get_media:
 * @media: a #GstSDPMedia
 *
 * Get the media description of @media.
 *
 * Returns: the media description.
 */
const gchar *
gst_sdp_media_get_media (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, NULL);

  return media->media;
}

/**
 * gst_sdp_media_set_media:
 * @media: a #GstSDPMedia
 * @med: the media description
 *
 * Set the media description of @media to @med.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_set_media (GstSDPMedia * media, const gchar * med)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (med != NULL, GST_SDP_EINVAL);

  g_free (media->media);
  media->media = g_strdup (med);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_get_port:
 * @media: a #GstSDPMedia
 *
 * Get the port number for @media.
 *
 * Returns: the port number of @media.
 */
guint
gst_sdp_media_get_port (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->port;
}

/**
 * gst_sdp_media_get_num_ports:
 * @media: a #GstSDPMedia
 *
 * Get the number of ports for @media.
 *
 * Returns: the number of ports for @media.
 */
guint
gst_sdp_media_get_num_ports (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->num_ports;
}

/**
 * gst_sdp_media_set_port_info:
 * @media: a #GstSDPMedia
 * @port: the port number
 * @num_ports: the number of ports
 *
 * Set the port information in @media.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_set_port_info (GstSDPMedia * media, guint port, guint num_ports)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  media->port = port;
  media->num_ports = num_ports;

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_get_proto:
 * @media: a #GstSDPMedia
 *
 * Get the transport protocol of @media
 *
 * Returns: the transport protocol of @media.
 */
const gchar *
gst_sdp_media_get_proto (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, NULL);

  return media->proto;
}

/**
 * gst_sdp_media_set_proto:
 * @media: a #GstSDPMedia
 * @proto: the media transport protocol
 *
 * Set the media transport protocol of @media to @proto.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_set_proto (GstSDPMedia * media, const gchar * proto)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  g_free (media->proto);
  media->proto = g_strdup (proto);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_formats_len:
 * @media: a #GstSDPMedia
 *
 * Get the number of formats in @media.
 *
 * Returns: the number of formats in @media.
 */
guint
gst_sdp_media_formats_len (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->fmts->len;
}

/**
 * gst_sdp_media_get_format:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Get the format information at position @idx in @media.
 *
 * Returns: the format at position @idx.
 */
const gchar *
gst_sdp_media_get_format (const GstSDPMedia * media, guint idx)
{
  g_return_val_if_fail (media != NULL, NULL);

  if (idx >= media->fmts->len)
    return NULL;
  return g_array_index (media->fmts, gchar *, idx);
}

/**
 * gst_sdp_media_insert_format:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @format: the format
 *
 * Insert the format information to @media at @idx. When @idx is -1,
 * the format is appended.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_insert_format (GstSDPMedia * media, gint idx,
    const gchar * format)
{
  gchar *fmt;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (format != NULL, GST_SDP_EINVAL);

  fmt = g_strdup (format);

  if (idx == -1)
    g_array_append_val (media->fmts, fmt);
  else
    g_array_insert_val (media->fmts, idx, fmt);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_replace_format:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @format: the format
 *
 * Replace the format information in @media at @idx with @format.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_replace_format (GstSDPMedia * media, guint idx,
    const gchar * format)
{
  gchar **old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (format != NULL, GST_SDP_EINVAL);

  old = &g_array_index (media->fmts, gchar *, idx);
  g_free (*old);
  *old = g_strdup (format);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_remove_format:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Remove the format information in @media at @idx.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_remove_format (GstSDPMedia * media, guint idx)
{
  gchar **old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  old = &g_array_index (media->fmts, gchar *, idx);
  g_free (*old);
  g_array_remove_index (media->fmts, idx);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_add_format:
 * @media: a #GstSDPMedia
 * @format: the format
 *
 * Add the format information to @media.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_add_format (GstSDPMedia * media, const gchar * format)
{
  gchar *fmt;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (format != NULL, GST_SDP_EINVAL);

  fmt = g_strdup (format);

  g_array_append_val (media->fmts, fmt);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_get_information:
 * @media: a #GstSDPMedia
 *
 * Get the information of @media
 *
 * Returns: the information of @media.
 */
const gchar *
gst_sdp_media_get_information (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, NULL);

  return media->information;
}

/**
 * gst_sdp_media_set_information:
 * @media: a #GstSDPMedia
 * @information: the media information
 *
 * Set the media information of @media to @information.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_set_information (GstSDPMedia * media, const gchar * information)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  g_free (media->information);
  media->information = g_strdup (information);

  return GST_SDP_OK;
}

/**
 * gst_sdp_connection_set:
 * @conn: a #GstSDPConnection
 * @nettype: the type of network. "IN" is defined to have the meaning
 * "Internet".
 * @addrtype: the type of address.
 * @address: the address
 * @ttl: the time to live of the address
 * @addr_number: the number of layers
 *
 * Set the connection with the given parameters.
 *
 * Returns: @GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_connection_set (GstSDPConnection * conn, const gchar * nettype,
    const gchar * addrtype, const gchar * address, guint ttl, guint addr_number)
{
  g_return_val_if_fail (conn != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (nettype != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (addrtype != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (address != NULL, GST_SDP_EINVAL);

  conn->nettype = g_strdup (nettype);
  conn->addrtype = g_strdup (addrtype);
  conn->address = g_strdup (address);
  conn->ttl = ttl;
  conn->addr_number = addr_number;
  return GST_SDP_OK;
}

/**
 * gst_sdp_connection_clear:
 * @conn: a #GstSDPConnection
 *
 * Clear the connection.
 *
 * Returns: @GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_connection_clear (GstSDPConnection * conn)
{
  g_return_val_if_fail (conn != NULL, GST_SDP_EINVAL);

  FREE_STRING (conn->nettype);
  FREE_STRING (conn->addrtype);
  FREE_STRING (conn->address);
  conn->ttl = 0;
  conn->addr_number = 0;
  return GST_SDP_OK;
}


/**
 * gst_sdp_media_connections_len:
 * @media: a #GstSDPMedia
 *
 * Get the number of connection fields in @media.
 *
 * Returns: the number of connections in @media.
 */
guint
gst_sdp_media_connections_len (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->connections->len;
}

/**
 * gst_sdp_media_get_connection:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Get the connection at position @idx in @media.
 *
 * Returns: the #GstSDPConnection at position @idx.
 */
const GstSDPConnection *
gst_sdp_media_get_connection (const GstSDPMedia * media, guint idx)
{
  g_return_val_if_fail (media != NULL, NULL);
  g_return_val_if_fail (idx < media->connections->len, NULL);

  return &g_array_index (media->connections, GstSDPConnection, idx);
}

/**
 * gst_sdp_media_insert_connection:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @conn: a #GstSDPConnection
 *
 * Insert the connection information to @media at @idx. When @idx is -1,
 * the connection is appended.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_insert_connection (GstSDPMedia * media, gint idx,
    GstSDPConnection * conn)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (conn != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx == -1
      || idx < media->connections->len, GST_SDP_EINVAL);

  if (idx == -1)
    g_array_append_val (media->connections, *conn);
  else
    g_array_insert_val (media->connections, idx, *conn);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_replace_connection:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @conn: a #GstSDPConnection
 *
 * Replace the connection information in @media at @idx with @conn.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_replace_connection (GstSDPMedia * media, guint idx,
    GstSDPConnection * conn)
{
  GstSDPConnection *old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (conn != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->connections->len, GST_SDP_EINVAL);

  old = &g_array_index (media->connections, GstSDPConnection, idx);
  gst_sdp_connection_clear (old);
  *old = *conn;

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_remove_connection:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Remove the connection information in @media at @idx.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_remove_connection (GstSDPMedia * media, guint idx)
{
  GstSDPConnection *old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->connections->len, GST_SDP_EINVAL);

  old = &g_array_index (media->connections, GstSDPConnection, idx);
  gst_sdp_connection_clear (old);
  g_array_remove_index (media->connections, idx);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_add_connection:
 * @media: a #GstSDPMedia
 * @nettype: the type of network. "IN" is defined to have the meaning
 * "Internet".
 * @addrtype: the type of address.
 * @address: the address
 * @ttl: the time to live of the address
 * @addr_number: the number of layers
 *
 * Add the given connection parameters to @media.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_add_connection (GstSDPMedia * media, const gchar * nettype,
    const gchar * addrtype, const gchar * address, guint ttl, guint addr_number)
{
  GstSDPConnection conn;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (nettype != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (addrtype != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (address != NULL, GST_SDP_EINVAL);

  gst_sdp_connection_set (&conn, nettype, addrtype, address, ttl, addr_number);
  g_array_append_val (media->connections, conn);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_bandwidths_len:
 * @media: a #GstSDPMedia
 *
 * Get the number of bandwidth fields in @media.
 *
 * Returns: the number of bandwidths in @media.
 */
guint
gst_sdp_media_bandwidths_len (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->bandwidths->len;
}

/**
 * gst_sdp_media_get_bandwidth:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Get the bandwidth at position @idx in @media.
 *
 * Returns: the #GstSDPBandwidth at position @idx.
 */
const GstSDPBandwidth *
gst_sdp_media_get_bandwidth (const GstSDPMedia * media, guint idx)
{
  g_return_val_if_fail (media != NULL, NULL);

  return &g_array_index (media->bandwidths, GstSDPBandwidth, idx);
}

/**
 * gst_sdp_media_insert_bandwidth:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @bw: a #GstSDPBandwidth
 *
 * Insert the bandwidth information to @media at @idx. When @idx is -1,
 * the bandwidth is appended.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_insert_bandwidth (GstSDPMedia * media, gint idx,
    GstSDPBandwidth * bw)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (bw != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx == -1
      || idx < media->bandwidths->len, GST_SDP_EINVAL);

  if (idx == -1)
    g_array_append_val (media->bandwidths, *bw);
  else
    g_array_insert_val (media->bandwidths, idx, *bw);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_replace_bandwidth:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @bw: a #GstSDPBandwidth
 *
 * Replace the bandwidth information in @media at @idx with @bw.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_replace_bandwidth (GstSDPMedia * media, guint idx,
    GstSDPBandwidth * bw)
{
  GstSDPBandwidth *old;
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (bw != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->bandwidths->len, GST_SDP_EINVAL);

  old = &g_array_index (media->bandwidths, GstSDPBandwidth, idx);
  gst_sdp_bandwidth_clear (old);
  *old = *bw;

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_remove_bandwidth:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Remove the bandwidth information in @media at @idx.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_remove_bandwidth (GstSDPMedia * media, guint idx)
{
  GstSDPBandwidth *old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->bandwidths->len, GST_SDP_EINVAL);

  old = &g_array_index (media->bandwidths, GstSDPBandwidth, idx);
  gst_sdp_bandwidth_clear (old);
  g_array_remove_index (media->bandwidths, idx);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_add_bandwidth:
 * @media: a #GstSDPMedia
 * @bwtype: the bandwidth modifier type
 * @bandwidth: the bandwidth in kilobits per second
 *
 * Add the bandwidth information with @bwtype and @bandwidth to @media.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_add_bandwidth (GstSDPMedia * media, const gchar * bwtype,
    guint bandwidth)
{
  GstSDPBandwidth bw;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (bwtype != NULL, GST_SDP_EINVAL);

  gst_sdp_bandwidth_set (&bw, bwtype, bandwidth);
  g_array_append_val (media->bandwidths, bw);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_set_key:
 * @media: a #GstSDPMedia
 * @type: the encryption type
 * @data: the encryption data
 *
 * Adds the encryption information to @media.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_media_set_key (GstSDPMedia * media, const gchar * type,
    const gchar * data)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  g_free (media->key.type);
  media->key.type = g_strdup (type);
  g_free (media->key.data);
  media->key.data = g_strdup (data);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_get_key:
 * @media: a #GstSDPMedia
 *
 * Get the encryption information from @media.
 *
 * Returns: a #GstSDPKey.
 */
const GstSDPKey *
gst_sdp_media_get_key (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, NULL);

  return &media->key;
}

/**
 * gst_sdp_media_attributes_len:
 * @media: a #GstSDPMedia
 *
 * Get the number of attribute fields in @media.
 *
 * Returns: the number of attributes in @media.
 */
guint
gst_sdp_media_attributes_len (const GstSDPMedia * media)
{
  g_return_val_if_fail (media != NULL, 0);

  return media->attributes->len;
}

/**
 * gst_sdp_media_add_attribute:
 * @media: a #GstSDPMedia
 * @key: a key
 * @value: (nullable): a value
 *
 * Add the attribute with @key and @value to @media.
 *
 * Returns: #GST_SDP_OK.
 */
GstSDPResult
gst_sdp_media_add_attribute (GstSDPMedia * media, const gchar * key,
    const gchar * value)
{
  GstSDPAttribute attr;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (key != NULL, GST_SDP_EINVAL);

  gst_sdp_attribute_set (&attr, key, value);
  g_array_append_val (media->attributes, attr);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_get_attribute:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Get the attribute at position @idx in @media.
 *
 * Returns: the #GstSDPAttribute at position @idx.
 */
const GstSDPAttribute *
gst_sdp_media_get_attribute (const GstSDPMedia * media, guint idx)
{
  g_return_val_if_fail (media != NULL, NULL);
  g_return_val_if_fail (idx < media->attributes->len, NULL);

  return &g_array_index (media->attributes, GstSDPAttribute, idx);
}

/**
 * gst_sdp_media_get_attribute_val_n:
 * @media: a #GstSDPMedia
 * @key: a key
 * @nth: an index
 *
 * Get the @nth attribute value for @key in @media.
 *
 * Returns: (nullable): the @nth attribute value.
 */
const gchar *
gst_sdp_media_get_attribute_val_n (const GstSDPMedia * media, const gchar * key,
    guint nth)
{
  guint i;

  g_return_val_if_fail (media != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (i = 0; i < media->attributes->len; i++) {
    GstSDPAttribute *attr;

    attr = &g_array_index (media->attributes, GstSDPAttribute, i);
    if (!strcmp (attr->key, key)) {
      if (nth == 0)
        return attr->value;
      else
        nth--;
    }
  }
  return NULL;
}

/**
 * gst_sdp_media_get_attribute_val:
 * @media: a #GstSDPMedia
 * @key: a key
 *
 * Get the first attribute value for @key in @media.
 *
 * Returns: (nullable): the first attribute value for @key.
 */
const gchar *
gst_sdp_media_get_attribute_val (const GstSDPMedia * media, const gchar * key)
{
  g_return_val_if_fail (media != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return gst_sdp_media_get_attribute_val_n (media, key, 0);
}

/**
 * gst_sdp_media_insert_attribute:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @attr: a #GstSDPAttribute
 *
 * Insert the attribute to @media at @idx. When @idx is -1,
 * the attribute is appended.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_insert_attribute (GstSDPMedia * media, gint idx,
    GstSDPAttribute * attr)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (attr != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx == -1
      || idx < media->attributes->len, GST_SDP_EINVAL);

  if (idx == -1)
    g_array_append_val (media->attributes, *attr);
  else
    g_array_insert_val (media->attributes, idx, *attr);

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_replace_attribute:
 * @media: a #GstSDPMedia
 * @idx: an index
 * @attr: a #GstSDPAttribute
 *
 * Replace the attribute in @media at @idx with @attr.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_replace_attribute (GstSDPMedia * media, guint idx,
    GstSDPAttribute * attr)
{
  GstSDPAttribute *old;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (attr != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->attributes->len, GST_SDP_EINVAL);

  old = &g_array_index (media->attributes, GstSDPAttribute, idx);
  gst_sdp_attribute_clear (old);
  *old = *attr;

  return GST_SDP_OK;
}

/**
 * gst_sdp_media_remove_attribute:
 * @media: a #GstSDPMedia
 * @idx: an index
 *
 * Remove the attribute in @media at @idx.
 *
 * Returns: #GST_SDP_OK.
 *
 * Since: 1.2
 */
GstSDPResult
gst_sdp_media_remove_attribute (GstSDPMedia * media, guint idx)
{
  GstSDPAttribute *old;
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (idx < media->attributes->len, GST_SDP_EINVAL);

  old = &g_array_index (media->attributes, GstSDPAttribute, idx);
  gst_sdp_attribute_clear (old);
  g_array_remove_index (media->attributes, idx);

  return GST_SDP_OK;
}

static void
read_string (gchar * dest, guint size, gchar ** src)
{
  guint idx;

  idx = 0;
  /* skip spaces */
  while (g_ascii_isspace (**src))
    (*src)++;

  while (!g_ascii_isspace (**src) && **src != '\0') {
    if (idx < size - 1)
      dest[idx++] = **src;
    (*src)++;
  }
  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_del (gchar * dest, guint size, gchar del, gchar ** src)
{
  guint idx;

  idx = 0;
  /* skip spaces */
  while (g_ascii_isspace (**src))
    (*src)++;

  while (**src != del && **src != '\0') {
    if (idx < size - 1)
      dest[idx++] = **src;
    (*src)++;
  }
  if (size > 0)
    dest[idx] = '\0';
}

enum
{
  SDP_SESSION,
  SDP_MEDIA,
};

typedef struct
{
  guint state;
  GstSDPMessage *msg;
  GstSDPMedia *media;
} SDPContext;

static gboolean
gst_sdp_parse_line (SDPContext * c, gchar type, gchar * buffer)
{
  gchar str[8192];
  gchar *p = buffer;

#define READ_STRING(field) \
  do { read_string (str, sizeof (str), &p); REPLACE_STRING (field, str); } while (0)
#define READ_UINT(field) \
  do { read_string (str, sizeof (str), &p); field = strtoul (str, NULL, 10); } while (0)

  switch (type) {
    case 'v':
      if (buffer[0] != '0')
        GST_WARNING ("wrong SDP version");
      gst_sdp_message_set_version (c->msg, buffer);
      break;
    case 'o':
      READ_STRING (c->msg->origin.username);
      READ_STRING (c->msg->origin.sess_id);
      READ_STRING (c->msg->origin.sess_version);
      READ_STRING (c->msg->origin.nettype);
      READ_STRING (c->msg->origin.addrtype);
      READ_STRING (c->msg->origin.addr);
      break;
    case 's':
      REPLACE_STRING (c->msg->session_name, buffer);
      break;
    case 'i':
      if (c->state == SDP_SESSION) {
        REPLACE_STRING (c->msg->information, buffer);
      } else {
        REPLACE_STRING (c->media->information, buffer);
      }
      break;
    case 'u':
      REPLACE_STRING (c->msg->uri, buffer);
      break;
    case 'e':
      gst_sdp_message_add_email (c->msg, buffer);
      break;
    case 'p':
      gst_sdp_message_add_phone (c->msg, buffer);
      break;
    case 'c':
    {
      GstSDPConnection conn;
      gchar *str2;

      memset (&conn, 0, sizeof (conn));

      str2 = p;
      while ((str2 = strchr (str2, '/')))
        *str2++ = ' ';
      READ_STRING (conn.nettype);
      READ_STRING (conn.addrtype);
      READ_STRING (conn.address);
      /* only read TTL for IP4 */
      if (strcmp (conn.addrtype, "IP4") == 0)
        READ_UINT (conn.ttl);
      READ_UINT (conn.addr_number);

      if (c->state == SDP_SESSION) {
        gst_sdp_message_set_connection (c->msg, conn.nettype, conn.addrtype,
            conn.address, conn.ttl, conn.addr_number);
      } else {
        gst_sdp_media_add_connection (c->media, conn.nettype, conn.addrtype,
            conn.address, conn.ttl, conn.addr_number);
      }
      gst_sdp_connection_clear (&conn);
      break;
    }
    case 'b':
    {
      gchar str2[32];

      read_string_del (str, sizeof (str), ':', &p);
      if (*p != '\0')
        p++;
      read_string (str2, sizeof (str2), &p);
      if (c->state == SDP_SESSION)
        gst_sdp_message_add_bandwidth (c->msg, str, atoi (str2));
      else
        gst_sdp_media_add_bandwidth (c->media, str, atoi (str2));
      break;
    }
    case 't':
      break;
    case 'k':
      read_string_del (str, sizeof (str), ':', &p);
      if (*p != '\0')
        p++;
      if (c->state == SDP_SESSION)
        gst_sdp_message_set_key (c->msg, str, p);
      else
        gst_sdp_media_set_key (c->media, str, p);
      break;
    case 'a':
      read_string_del (str, sizeof (str), ':', &p);
      if (*p != '\0')
        p++;
      if (c->state == SDP_SESSION)
        gst_sdp_message_add_attribute (c->msg, str, p);
      else
        gst_sdp_media_add_attribute (c->media, str, p);
      break;
    case 'm':
    {
      gchar *slash;
      GstSDPMedia nmedia;

      c->state = SDP_MEDIA;
      memset (&nmedia, 0, sizeof (nmedia));
      gst_sdp_media_init (&nmedia);

      /* m=<media> <port>/<number of ports> <proto> <fmt> ... */
      READ_STRING (nmedia.media);
      read_string (str, sizeof (str), &p);
      slash = g_strrstr (str, "/");
      if (slash) {
        *slash = '\0';
        nmedia.port = atoi (str);
        nmedia.num_ports = atoi (slash + 1);
      } else {
        nmedia.port = atoi (str);
        nmedia.num_ports = 0;
      }
      READ_STRING (nmedia.proto);
      do {
        read_string (str, sizeof (str), &p);
        gst_sdp_media_add_format (&nmedia, str);
      } while (*p != '\0');

      gst_sdp_message_add_media (c->msg, &nmedia);
      c->media =
          &g_array_index (c->msg->medias, GstSDPMedia, c->msg->medias->len - 1);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

/**
 * gst_sdp_message_parse_buffer:
 * @data: (array length=size): the start of the buffer
 * @size: the size of the buffer
 * @msg: (transfer none): the result #GstSDPMessage
 *
 * Parse the contents of @size bytes pointed to by @data and store the result in
 * @msg.
 *
 * Returns: #GST_SDP_OK on success.
 */
GstSDPResult
gst_sdp_message_parse_buffer (const guint8 * data, guint size,
    GstSDPMessage * msg)
{
  gchar *p, *s;
  SDPContext c;
  gchar type;
  gchar *buffer = NULL;
  guint bufsize = 0;
  guint len = 0;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (data != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (size != 0, GST_SDP_EINVAL);

  c.state = SDP_SESSION;
  c.msg = msg;
  c.media = NULL;

#define SIZE_CHECK_GUARD \
  G_STMT_START { \
    if (p - (gchar *) data >= size) \
      goto out; \
  } G_STMT_END

  p = (gchar *) data;
  while (TRUE) {
    while (p - (gchar *) data < size && g_ascii_isspace (*p))
      p++;

    SIZE_CHECK_GUARD;

    type = *p++;
    if (type == '\0')
      break;

    SIZE_CHECK_GUARD;

    if (*p != '=')
      goto line_done;
    p++;

    SIZE_CHECK_GUARD;

    s = p;
    while (p - (gchar *) data < size && *p != '\n' && *p != '\r' && *p != '\0')
      p++;

    len = p - s;
    if (bufsize <= len) {
      buffer = g_realloc (buffer, len + 1);
      bufsize = len + 1;
    }
    memcpy (buffer, s, len);
    buffer[len] = '\0';

    gst_sdp_parse_line (&c, type, buffer);

    SIZE_CHECK_GUARD;

  line_done:
    while (p - (gchar *) data < size && *p != '\n' && *p != '\0')
      p++;

    SIZE_CHECK_GUARD;

    if (*p == '\n')
      p++;
  }

#undef SIZE_CHECK_GUARD

out:
  g_free (buffer);

  return GST_SDP_OK;
}

static void
print_media (GstSDPMedia * media)
{
  g_print ("   media:       '%s'\n", GST_STR_NULL (media->media));
  g_print ("   port:        '%u'\n", media->port);
  g_print ("   num_ports:   '%u'\n", media->num_ports);
  g_print ("   proto:       '%s'\n", GST_STR_NULL (media->proto));
  if (media->fmts->len > 0) {
    guint i;

    g_print ("   formats:\n");
    for (i = 0; i < media->fmts->len; i++) {
      g_print ("    format  '%s'\n", g_array_index (media->fmts, gchar *, i));
    }
  }
  g_print ("   information: '%s'\n", GST_STR_NULL (media->information));
  if (media->connections->len > 0) {
    guint i;

    g_print ("   connections:\n");
    for (i = 0; i < media->connections->len; i++) {
      GstSDPConnection *conn =
          &g_array_index (media->connections, GstSDPConnection, i);

      g_print ("    nettype:      '%s'\n", GST_STR_NULL (conn->nettype));
      g_print ("    addrtype:     '%s'\n", GST_STR_NULL (conn->addrtype));
      g_print ("    address:      '%s'\n", GST_STR_NULL (conn->address));
      g_print ("    ttl:          '%u'\n", conn->ttl);
      g_print ("    addr_number:  '%u'\n", conn->addr_number);
    }
  }
  if (media->bandwidths->len > 0) {
    guint i;

    g_print ("   bandwidths:\n");
    for (i = 0; i < media->bandwidths->len; i++) {
      GstSDPBandwidth *bw =
          &g_array_index (media->bandwidths, GstSDPBandwidth, i);

      g_print ("    type:         '%s'\n", GST_STR_NULL (bw->bwtype));
      g_print ("    bandwidth:    '%u'\n", bw->bandwidth);
    }
  }
  g_print ("   key:\n");
  g_print ("    type:       '%s'\n", GST_STR_NULL (media->key.type));
  g_print ("    data:       '%s'\n", GST_STR_NULL (media->key.data));
  if (media->attributes->len > 0) {
    guint i;

    g_print ("   attributes:\n");
    for (i = 0; i < media->attributes->len; i++) {
      GstSDPAttribute *attr =
          &g_array_index (media->attributes, GstSDPAttribute, i);

      g_print ("    attribute '%s' : '%s'\n", attr->key, attr->value);
    }
  }
}

/**
 * gst_sdp_message_dump:
 * @msg: a #GstSDPMessage
 *
 * Dump the parsed contents of @msg to stdout.
 *
 * Returns: a #GstSDPResult.
 */
GstSDPResult
gst_sdp_message_dump (const GstSDPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  g_print ("sdp packet %p:\n", msg);
  g_print (" version:       '%s'\n", GST_STR_NULL (msg->version));
  g_print (" origin:\n");
  g_print ("  username:     '%s'\n", GST_STR_NULL (msg->origin.username));
  g_print ("  sess_id:      '%s'\n", GST_STR_NULL (msg->origin.sess_id));
  g_print ("  sess_version: '%s'\n", GST_STR_NULL (msg->origin.sess_version));
  g_print ("  nettype:      '%s'\n", GST_STR_NULL (msg->origin.nettype));
  g_print ("  addrtype:     '%s'\n", GST_STR_NULL (msg->origin.addrtype));
  g_print ("  addr:         '%s'\n", GST_STR_NULL (msg->origin.addr));
  g_print (" session_name:  '%s'\n", GST_STR_NULL (msg->session_name));
  g_print (" information:   '%s'\n", GST_STR_NULL (msg->information));
  g_print (" uri:           '%s'\n", GST_STR_NULL (msg->uri));

  if (msg->emails->len > 0) {
    guint i;

    g_print (" emails:\n");
    for (i = 0; i < msg->emails->len; i++) {
      g_print ("  email '%s'\n", g_array_index (msg->emails, gchar *, i));
    }
  }
  if (msg->phones->len > 0) {
    guint i;

    g_print (" phones:\n");
    for (i = 0; i < msg->phones->len; i++) {
      g_print ("  phone '%s'\n", g_array_index (msg->phones, gchar *, i));
    }
  }
  g_print (" connection:\n");
  g_print ("  nettype:      '%s'\n", GST_STR_NULL (msg->connection.nettype));
  g_print ("  addrtype:     '%s'\n", GST_STR_NULL (msg->connection.addrtype));
  g_print ("  address:      '%s'\n", GST_STR_NULL (msg->connection.address));
  g_print ("  ttl:          '%u'\n", msg->connection.ttl);
  g_print ("  addr_number:  '%u'\n", msg->connection.addr_number);
  if (msg->bandwidths->len > 0) {
    guint i;

    g_print (" bandwidths:\n");
    for (i = 0; i < msg->bandwidths->len; i++) {
      GstSDPBandwidth *bw =
          &g_array_index (msg->bandwidths, GstSDPBandwidth, i);

      g_print ("  type:         '%s'\n", GST_STR_NULL (bw->bwtype));
      g_print ("  bandwidth:    '%u'\n", bw->bandwidth);
    }
  }
  g_print (" key:\n");
  g_print ("  type:         '%s'\n", GST_STR_NULL (msg->key.type));
  g_print ("  data:         '%s'\n", GST_STR_NULL (msg->key.data));
  if (msg->attributes->len > 0) {
    guint i;

    g_print (" attributes:\n");
    for (i = 0; i < msg->attributes->len; i++) {
      GstSDPAttribute *attr =
          &g_array_index (msg->attributes, GstSDPAttribute, i);

      g_print ("  attribute '%s' : '%s'\n", attr->key, attr->value);
    }
  }
  if (msg->medias->len > 0) {
    guint i;

    g_print (" medias:\n");
    for (i = 0; i < msg->medias->len; i++) {
      g_print ("  media %u:\n", i);
      print_media (&g_array_index (msg->medias, GstSDPMedia, i));
    }
  }
  return GST_SDP_OK;
}

static const gchar *
gst_sdp_get_attribute_for_pt (const GstSDPMedia * media, const gchar * name,
    gint pt)
{
  guint i;

  for (i = 0;; i++) {
    const gchar *attr;
    gint val;

    if ((attr = gst_sdp_media_get_attribute_val_n (media, name, i)) == NULL)
      break;

    if (sscanf (attr, "%d ", &val) != 1)
      continue;

    if (val == pt)
      return attr;
  }
  return NULL;
}

/* this may modify the input string (and resets) */
#define PARSE_INT(p, del, res)          \
G_STMT_START {                          \
  gchar *t = p;                         \
  p = strstr (p, del);                  \
  if (p == NULL)                        \
    res = -1;                           \
  else {                                \
    char prev = *p;                     \
    *p = '\0';                          \
    res = atoi (t);                     \
    *p = prev;                          \
    p++;                                \
  }                                     \
} G_STMT_END

/* this may modify the string without reset */
#define PARSE_STRING(p, del, res)       \
G_STMT_START {                          \
  gchar *t = p;                         \
  p = strstr (p, del);                  \
  if (p == NULL) {                      \
    res = NULL;                         \
    p = t;                              \
  }                                     \
  else {                                \
    *p = '\0';                          \
    p++;                                \
    res = t;                            \
  }                                     \
} G_STMT_END

#define SKIP_SPACES(p)                  \
  while (*p && g_ascii_isspace (*p))    \
    p++;

/* rtpmap contains:
 *
 *  <payload> <encoding_name>/<clock_rate>[/<encoding_params>]
 */
static gboolean
gst_sdp_parse_rtpmap (const gchar * rtpmap, gint * payload, gchar ** name,
    gint * rate, gchar ** params)
{
  gchar *p, *t, *orig_value;

  p = orig_value = g_strdup (rtpmap);

  PARSE_INT (p, " ", *payload);
  if (*payload == -1)
    goto fail;

  SKIP_SPACES (p);
  if (*p == '\0')
    goto fail;

  PARSE_STRING (p, "/", *name);
  if (*name == NULL) {
    GST_DEBUG ("no rate, name %s", p);
    /* no rate, assume -1 then, this is not supposed to happen but RealMedia
     * streams seem to omit the rate. */
    *name = g_strdup (p);
    *rate = -1;
    *params = NULL;
    goto out;
  } else {
    *name = g_strdup (*name);
  }

  t = p;
  p = strstr (p, "/");
  if (p == NULL) {
    *rate = atoi (t);
    *params = NULL;
    goto out;
  }
  p++;
  *rate = atoi (t);

  if (*p == '\0')
    *params = NULL;
  else
    *params = g_strdup (p);

out:
  g_free (orig_value);
  return TRUE;

fail:
  g_free (orig_value);
  return FALSE;
}

/**
 * gst_sdp_media_add_rtcp_fb_attributes_from_media:
 * @media: a #GstSDPMedia
 * @pt: payload type
 * @caps: a #GstCaps
 *
 * Parse given @media for "rtcp-fb" attributes and add it to the @caps.
 *
 * Mapping of caps from SDP fields:
 *
 * a=rtcp-fb:(payload) (param1) [param2]...
 *
 * Returns: a #GstSDPResult.
 */
static GstSDPResult
gst_sdp_media_add_rtcp_fb_attributes_from_media (const GstSDPMedia * media,
    gint pt, GstCaps * caps)
{
  const gchar *rtcp_fb;
  gchar *p, *to_free;
  gint payload, i;
  GstStructure *s;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  s = gst_caps_get_structure (caps, 0);

  for (i = 0;; i++) {
    gboolean all_formats = FALSE;

    if ((rtcp_fb = gst_sdp_media_get_attribute_val_n (media,
                "rtcp-fb", i)) == NULL)
      break;

    /* p is now of the format <payload> attr... */
    to_free = p = g_strdup (rtcp_fb);

    /* check if it applies to all formats */
    if (*p == '*') {
      p++;
      all_formats = TRUE;
    } else {
      PARSE_INT (p, " ", payload);
    }

    if (all_formats || (payload != -1 && payload == pt)) {
      gchar *tmp, *key;

      SKIP_SPACES (p);

      /* replace space with '-' */
      key = g_strdup_printf ("rtcp-fb-%s", p);

      for (tmp = key; (tmp = strchr (tmp, ' ')); ++tmp) {
        *tmp = '-';
      }

      gst_structure_set (s, key, G_TYPE_BOOLEAN, TRUE, NULL);
      GST_DEBUG ("adding caps: %s=TRUE", key);
      g_free (key);
    }
    g_free (to_free);
  }
  return GST_SDP_OK;
}

static void
gst_sdp_media_caps_adjust_h264 (GstCaps * caps)
{
  long int spsint;
  guint8 sps[2];
  const gchar *profile_level_id;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  if (g_strcmp0 (gst_structure_get_string (s, "encoding-name"), "H264") ||
      g_strcmp0 (gst_structure_get_string (s, "level-asymmetry-allowed"), "1"))
    return;

  profile_level_id = gst_structure_get_string (s, "profile-level-id");
  if (!profile_level_id)
    return;

  spsint = strtol (profile_level_id, NULL, 16);
  sps[0] = spsint >> 16;
  sps[1] = spsint >> 8;

  GST_DEBUG ("'level-asymmetry-allowed' is set so we shouldn't care about "
      "'profile-level-id' and only set a 'profile' instead");
  gst_structure_set (s, "profile", G_TYPE_STRING,
      gst_codec_utils_h264_get_profile (sps, 2), NULL);

  gst_structure_remove_fields (s, "level-asymmetry-allowed", "profile-level-id",
      NULL);
}

/**
 * gst_sdp_media_get_caps_from_media:
 * @media: a #GstSDPMedia
 * @pt: a payload type
 *
 * Mapping of caps from SDP fields:
 *
 * a=rtpmap:(payload) (encoding_name)/(clock_rate)[/(encoding_params)]
 *
 * a=framesize:(payload) (width)-(height)
 *
 * a=fmtp:(payload) (param)[=(value)];...
 *
 * Note that the extmap, ssrc and rid attributes are set only by gst_sdp_media_attributes_to_caps().
 *
 * Returns: (transfer full) (nullable): a #GstCaps, or %NULL if an error happened
 *
 * Since: 1.8
 */
GstCaps *
gst_sdp_media_get_caps_from_media (const GstSDPMedia * media, gint pt)
{
  GstCaps *caps;
  const gchar *rtpmap;
  gchar *fmtp = NULL;
  gchar *framesize = NULL;
  gchar *name = NULL;
  gint rate = -1;
  gchar *params = NULL;
  gchar *tmp;
  GstStructure *s;
  gint payload = 0;
  gboolean ret;

  g_return_val_if_fail (media != NULL, NULL);

  /* get and parse rtpmap */
  rtpmap = gst_sdp_get_attribute_for_pt (media, "rtpmap", pt);

  if (rtpmap) {
    ret = gst_sdp_parse_rtpmap (rtpmap, &payload, &name, &rate, &params);
    if (!ret) {
      GST_ERROR ("error parsing rtpmap, ignoring");
      rtpmap = NULL;
    }
  }
  /* dynamic payloads need rtpmap or we fail */
  if (rtpmap == NULL && pt >= 96)
    goto no_rtpmap;

  /* check if we have a rate, if not, we need to look up the rate from the
   * default rates based on the payload types. */
  /* Some broken RTSP server puts a rate of 0, also use the default in that
   * case */
  if (rate <= 0) {
    const GstRTPPayloadInfo *info;

    if (GST_RTP_PAYLOAD_IS_DYNAMIC (pt)) {
      /* dynamic types, use media and encoding_name */
      tmp = g_ascii_strdown (media->media, -1);
      info = gst_rtp_payload_info_for_name (tmp, name);
      g_free (tmp);
    } else {
      /* static types, use payload type */
      info = gst_rtp_payload_info_for_pt (pt);
    }

    if (info) {
      if ((rate = info->clock_rate) == 0)
        rate = -1;
    }
    /* we fail if we cannot find one */
    if (rate == -1)
      goto no_rate;
  }

  tmp = g_ascii_strdown (media->media, -1);
  caps = gst_caps_new_simple ("application/x-unknown",
      "media", G_TYPE_STRING, tmp, "payload", G_TYPE_INT, pt, NULL);
  g_free (tmp);
  s = gst_caps_get_structure (caps, 0);

  gst_structure_set (s, "clock-rate", G_TYPE_INT, rate, NULL);

  /* encoding name must be upper case */
  if (name != NULL) {
    tmp = g_ascii_strup (name, -1);
    gst_structure_set (s, "encoding-name", G_TYPE_STRING, tmp, NULL);
    g_free (tmp);
  }

  /* params must be lower case */
  if (params != NULL) {
    tmp = g_ascii_strdown (params, -1);
    gst_structure_set (s, "encoding-params", G_TYPE_STRING, tmp, NULL);
    g_free (tmp);
  }

  /* parse optional fmtp: field */
  if ((fmtp = g_strdup (gst_sdp_get_attribute_for_pt (media, "fmtp", pt)))) {
    gchar *p;
    gint payload = 0;

    p = fmtp;

    /* p is now of the format <payload> <param>[=<value>];... */
    PARSE_INT (p, " ", payload);
    if (payload != -1 && payload == pt) {
      gchar **pairs;
      gint i;

      /* <param>[=<value>] are separated with ';' */
      pairs = g_strsplit (p, ";", 0);
      for (i = 0; pairs[i]; i++) {
        gchar *valpos;
        const gchar *val, *key;
        gint j;
        const gchar *reserved_keys[] =
            { "media", "payload", "clock-rate", "encoding-name",
          "encoding-params"
        };

        /* the key may not have a '=', the value can have other '='s */
        valpos = strstr (pairs[i], "=");
        if (valpos) {
          /* we have a '=' and thus a value, remove the '=' with \0 */
          *valpos = '\0';
          /* value is everything between '=' and ';'. We split the pairs at ;
           * boundaries so we can take the remainder of the value. Some servers
           * put spaces around the value which we strip off here. Alternatively
           * we could strip those spaces in the depayloaders should these spaces
           * actually carry any meaning in the future. */
          val = g_strstrip (valpos + 1);
        } else {
          /* simple <param>;.. is translated into <param>=1;... */
          val = "1";
        }
        /* strip the key of spaces, convert key to lowercase but not the value. */
        key = g_strstrip (pairs[i]);

        /* skip keys from the fmtp, which we already use ourselves for the
         * caps. Some software is adding random things like clock-rate into
         * the fmtp, and we would otherwise here set a string-typed clock-rate
         * in the caps... and thus fail to create valid RTP caps
         */
        for (j = 0; j < G_N_ELEMENTS (reserved_keys); j++) {
          if (g_ascii_strcasecmp (reserved_keys[j], key) == 0) {
            key = "";
            break;
          }
        }

        if (strlen (key) > 0) {
          tmp = g_ascii_strdown (key, -1);
          gst_structure_set (s, tmp, G_TYPE_STRING, val, NULL);
          g_free (tmp);
        }
      }
      g_strfreev (pairs);
    }
  }

  /* parse framesize: field */
  if ((framesize =
          g_strdup (gst_sdp_media_get_attribute_val (media, "framesize")))) {
    gchar *p;

    /* p is now of the format <payload> <width>-<height> */
    p = framesize;

    PARSE_INT (p, " ", payload);
    if (payload != -1 && payload == pt) {
      gst_structure_set (s, "a-framesize", G_TYPE_STRING, p, NULL);
    }
  }

  gst_sdp_media_caps_adjust_h264 (caps);

  /* parse rtcp-fb: field */
  gst_sdp_media_add_rtcp_fb_attributes_from_media (media, pt, caps);

out:
  g_free (framesize);
  g_free (fmtp);
  g_free (name);
  g_free (params);
  return caps;

  /* ERRORS */
no_rtpmap:
  {
    GST_ERROR ("rtpmap type not given for dynamic payload %d", pt);
    caps = NULL;
    goto out;
  }
no_rate:
  {
    GST_ERROR ("rate unknown for payload type %d", pt);
    caps = NULL;
    goto out;
  }
}

/**
 * gst_sdp_media_set_media_from_caps:
 * @caps: a #GstCaps
 * @media: (out caller-allocates): a #GstSDPMedia
 *
 * Mapping of caps to SDP fields:
 *
 * a=rtpmap:(payload) (encoding_name) or (clock_rate)[or (encoding_params)]
 *
 * a=framesize:(payload) (width)-(height)
 *
 * a=fmtp:(payload) (param)[=(value)];...
 *
 * a=rtcp-fb:(payload) (param1) [param2]...
 *
 * a=extmap:(id)[/direction] (extensionname) (extensionattributes)
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.8
 */
GstSDPResult
gst_sdp_media_set_media_from_caps (const GstCaps * caps, GstSDPMedia * media)
{
  const gchar *caps_str, *caps_enc, *caps_params;
  gchar *tmp;
  gint caps_pt, caps_rate;
  guint n_fields, j;
  gboolean first, nack, nack_pli, ccm_fir, transport_cc;
  GString *fmtp;
  GstStructure *s;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  s = gst_caps_get_structure (caps, 0);
  if (s == NULL) {
    GST_ERROR ("ignoring stream without media type");
    goto error;
  }

  /* get media type and payload for the m= line */
  caps_str = gst_structure_get_string (s, "media");
  if (!caps_str) {
    GST_ERROR ("ignoring stream without media type");
    goto error;
  }
  gst_sdp_media_set_media (media, caps_str);

  if (!gst_structure_get_int (s, "payload", &caps_pt)) {
    GST_ERROR ("ignoring stream without payload type");
    goto error;
  }
  tmp = g_strdup_printf ("%d", caps_pt);
  gst_sdp_media_add_format (media, tmp);
  g_free (tmp);

  /* get clock-rate, media type and params for the rtpmap attribute */
  if (!gst_structure_get_int (s, "clock-rate", &caps_rate)) {
    GST_ERROR ("ignoring stream without clock rate");
    goto error;
  }
  caps_enc = gst_structure_get_string (s, "encoding-name");
  caps_params = gst_structure_get_string (s, "encoding-params");

  if (caps_enc) {
    if (caps_params)
      tmp = g_strdup_printf ("%d %s/%d/%s", caps_pt, caps_enc, caps_rate,
          caps_params);
    else
      tmp = g_strdup_printf ("%d %s/%d", caps_pt, caps_enc, caps_rate);

    gst_sdp_media_add_attribute (media, "rtpmap", tmp);
    g_free (tmp);
  }

  /* get rtcp-fb attributes */
  if (gst_structure_get_boolean (s, "rtcp-fb-nack", &nack)) {
    if (nack) {
      tmp = g_strdup_printf ("%d nack", caps_pt);
      gst_sdp_media_add_attribute (media, "rtcp-fb", tmp);
      g_free (tmp);
      GST_DEBUG ("adding rtcp-fb-nack to pt=%d", caps_pt);
    }
  }

  if (gst_structure_get_boolean (s, "rtcp-fb-nack-pli", &nack_pli)) {
    if (nack_pli) {
      tmp = g_strdup_printf ("%d nack pli", caps_pt);
      gst_sdp_media_add_attribute (media, "rtcp-fb", tmp);
      g_free (tmp);
      GST_DEBUG ("adding rtcp-fb-nack-pli to pt=%d", caps_pt);
    }
  }

  if (gst_structure_get_boolean (s, "rtcp-fb-ccm-fir", &ccm_fir)) {
    if (ccm_fir) {
      tmp = g_strdup_printf ("%d ccm fir", caps_pt);
      gst_sdp_media_add_attribute (media, "rtcp-fb", tmp);
      g_free (tmp);
      GST_DEBUG ("adding rtcp-fb-ccm-fir to pt=%d", caps_pt);
    }
  }

  if (gst_structure_get_boolean (s, "rtcp-fb-transport-cc", &transport_cc)) {
    if (transport_cc) {
      tmp = g_strdup_printf ("%d transport-cc", caps_pt);
      gst_sdp_media_add_attribute (media, "rtcp-fb", tmp);
      g_free (tmp);
      GST_DEBUG ("adding rtcp-fb-transport-cc to pt=%d", caps_pt);
    }
  }

  /* collect all other properties and add them to fmtp, extmap or attributes */
  fmtp = g_string_new ("");
  g_string_append_printf (fmtp, "%d ", caps_pt);
  first = TRUE;
  n_fields = gst_structure_n_fields (s);
  for (j = 0; j < n_fields; j++) {
    const gchar *fname, *fval;

    fname = gst_structure_nth_field_name (s, j);

    /* filter out standard properties */
    if (!strcmp (fname, "media"))
      continue;
    if (!strcmp (fname, "payload"))
      continue;
    if (!strcmp (fname, "clock-rate"))
      continue;
    if (!strcmp (fname, "encoding-name"))
      continue;
    if (!strcmp (fname, "encoding-params"))
      continue;
    if (!strcmp (fname, "ssrc"))
      continue;
    if (!strcmp (fname, "timestamp-offset"))
      continue;
    if (!strcmp (fname, "seqnum-offset"))
      continue;
    if (g_str_has_prefix (fname, "srtp-"))
      continue;
    if (g_str_has_prefix (fname, "srtcp-"))
      continue;
    /* handled later */
    if (g_str_has_prefix (fname, "x-gst-rtsp-server-rtx-time"))
      continue;
    if (g_str_has_prefix (fname, "rtcp-fb-"))
      continue;
    if (g_str_has_prefix (fname, "ssrc-"))
      continue;

    if (!strcmp (fname, "a-framesize")) {
      /* a-framesize attribute */
      if ((fval = gst_structure_get_string (s, fname))) {
        tmp = g_strdup_printf ("%d %s", caps_pt, fval);
        gst_sdp_media_add_attribute (media, fname + 2, tmp);
        g_free (tmp);
      }
      continue;
    }

    if (g_str_has_prefix (fname, "a-")) {
      /* attribute */
      if ((fval = gst_structure_get_string (s, fname)))
        gst_sdp_media_add_attribute (media, fname + 2, fval);
      continue;
    }
    if (g_str_has_prefix (fname, "x-")) {
      /* attribute */
      if ((fval = gst_structure_get_string (s, fname)))
        gst_sdp_media_add_attribute (media, fname, fval);
      continue;
    }

    /* extmap */
    if (g_str_has_prefix (fname, "extmap-")) {
      gchar *endptr;
      guint id = strtoull (fname + 7, &endptr, 10);
      const GValue *arr;

      if (*endptr != '\0' || id == 0 || id == 15 || id > 9999)
        continue;

      if ((fval = gst_structure_get_string (s, fname))) {
        gchar *extmap = g_strdup_printf ("%u %s", id, fval);
        gst_sdp_media_add_attribute (media, "extmap", extmap);
        g_free (extmap);
      } else if ((arr = gst_structure_get_value (s, fname))
          && G_VALUE_HOLDS (arr, GST_TYPE_ARRAY)
          && gst_value_array_get_size (arr) == 3) {
        const GValue *val;
        const gchar *direction, *extensionname, *extensionattributes;

        val = gst_value_array_get_value (arr, 0);
        direction = g_value_get_string (val);

        val = gst_value_array_get_value (arr, 1);
        extensionname = g_value_get_string (val);

        val = gst_value_array_get_value (arr, 2);
        extensionattributes = g_value_get_string (val);

        if (!extensionname || *extensionname == '\0')
          continue;

        if (direction && *direction != '\0' && extensionattributes
            && *extensionattributes != '\0') {
          gchar *extmap =
              g_strdup_printf ("%u/%s %s %s", id, direction, extensionname,
              extensionattributes);
          gst_sdp_media_add_attribute (media, "extmap", extmap);
          g_free (extmap);
        } else if (direction && *direction != '\0') {
          gchar *extmap =
              g_strdup_printf ("%u/%s %s", id, direction, extensionname);
          gst_sdp_media_add_attribute (media, "extmap", extmap);
          g_free (extmap);
        } else if (extensionattributes && *extensionattributes != '\0') {
          gchar *extmap = g_strdup_printf ("%u %s %s", id, extensionname,
              extensionattributes);
          gst_sdp_media_add_attribute (media, "extmap", extmap);
          g_free (extmap);
        } else {
          gchar *extmap = g_strdup_printf ("%u %s", id, extensionname);
          gst_sdp_media_add_attribute (media, "extmap", extmap);
          g_free (extmap);
        }
      }
      continue;
    }

    /* rid values */
    if (g_str_has_prefix (fname, "rid-")) {
      const char *rid_id = &fname[strlen ("rid-")];
      const GValue *arr;

      if (!rid_id || !*rid_id)
        continue;

      if ((fval = gst_structure_get_string (s, fname))) {
        char *rid_val = g_strdup_printf ("%s %s", rid_id, fval);
        gst_sdp_media_add_attribute (media, "rid", rid_val);
        g_free (rid_val);
      } else if ((arr = gst_structure_get_value (s, fname))
          && GST_VALUE_HOLDS_ARRAY (arr)
          && gst_value_array_get_size (arr) > 1) {
        const gchar *direction, *param;
        GString *str;
        guint i, n;

        str = g_string_new (NULL);

        g_string_append_printf (str, "%s ", rid_id);

        n = gst_value_array_get_size (arr);
        for (i = 0; i < n; i++) {
          const GValue *val = gst_value_array_get_value (arr, i);
          if (i == 0) {
            direction = g_value_get_string (val);
            g_string_append_printf (str, "%s", direction);
          } else {
            param = g_value_get_string (val);
            if (i == 1)
              g_string_append_c (str, ' ');
            else
              g_string_append_c (str, ';');
            g_string_append_printf (str, "%s", param);
          }
        }
        gst_sdp_media_add_attribute (media, "rid", str->str);
        g_string_free (str, TRUE);
      } else {
        GST_WARNING ("caps field %s is an unsupported format", fname);
      }
      continue;
    }

    if ((fval = gst_structure_get_string (s, fname))) {

      /* "profile" is our internal representation of the notion of
       * "level-asymmetry-allowed" with caps, convert it back to the SDP
       * representation */
      if (!g_strcmp0 (gst_structure_get_string (s, "encoding-name"), "H264")
          && !g_strcmp0 (fname, "profile")) {
        fname = "level-asymmetry-allowed";
        fval = "1";
      }

      g_string_append_printf (fmtp, "%s%s=%s", first ? "" : ";", fname, fval);
      first = FALSE;
    }
  }

  if (!first) {
    tmp = g_string_free (fmtp, FALSE);
    gst_sdp_media_add_attribute (media, "fmtp", tmp);
    g_free (tmp);
  } else {
    g_string_free (fmtp, TRUE);
  }

  return GST_SDP_OK;

  /* ERRORS */
error:
  {
    GST_DEBUG ("ignoring stream");
    return GST_SDP_EINVAL;
  }
}

/**
 * gst_sdp_make_keymgmt:
 * @uri: a #gchar URI
 * @base64: a #gchar base64-encoded key data
 *
 * Makes key management data
 *
 * Returns: (transfer full): a #gchar key-mgmt data,
 *
 * Since: 1.8
 */
gchar *
gst_sdp_make_keymgmt (const gchar * uri, const gchar * base64)
{
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (base64 != NULL, NULL);

  return g_strdup_printf ("prot=mikey;uri=\"%s\";data=\"%s\"", uri, base64);
}

static gboolean
gst_sdp_parse_keymgmt (const gchar * keymgmt, GstMIKEYMessage ** mikey)
{
  gsize size;
  guchar *data;
  gchar *orig_value;
  gchar *p, *kmpid;

  p = orig_value = g_strdup (keymgmt);

  SKIP_SPACES (p);
  if (*p == '\0') {
    g_free (orig_value);
    return FALSE;
  }

  PARSE_STRING (p, " ", kmpid);
  if (kmpid == NULL || !g_str_equal (kmpid, "mikey")) {
    g_free (orig_value);
    return FALSE;
  }
  data = g_base64_decode (p, &size);
  g_free (orig_value);          /* Don't need this any more */

  if (data == NULL)
    return FALSE;

  *mikey = gst_mikey_message_new_from_data (data, size, NULL, NULL);
  g_free (data);

  return (*mikey != NULL);
}

static GstSDPResult
sdp_add_attributes_to_keymgmt (GArray * attributes, GstMIKEYMessage ** mikey)
{
  GstSDPResult res = GST_SDP_OK;

  if (attributes->len > 0) {
    guint i;
    for (i = 0; i < attributes->len; i++) {
      GstSDPAttribute *attr = &g_array_index (attributes, GstSDPAttribute, i);

      if (g_str_equal (attr->key, "key-mgmt")) {
        res = gst_sdp_parse_keymgmt (attr->value, mikey);
        break;
      }
    }
  }

  return res;
}

/**
 * gst_sdp_message_parse_keymgmt:
 * @msg: a #GstSDPMessage
 * @mikey: (out) (transfer full): pointer to new #GstMIKEYMessage
 *
 * Creates a new #GstMIKEYMessage after parsing the key-mgmt attribute
 * from a #GstSDPMessage.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.8.1
 */
GstSDPResult
gst_sdp_message_parse_keymgmt (const GstSDPMessage * msg,
    GstMIKEYMessage ** mikey)
{
  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);

  return sdp_add_attributes_to_keymgmt (msg->attributes, mikey);
}

/**
 * gst_sdp_media_parse_keymgmt:
 * @media: a #GstSDPMedia
 * @mikey: (out) (transfer full): pointer to new #GstMIKEYMessage
 *
 * Creates a new #GstMIKEYMessage after parsing the key-mgmt attribute
 * from a #GstSDPMedia.
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.8.1
 */
GstSDPResult
gst_sdp_media_parse_keymgmt (const GstSDPMedia * media,
    GstMIKEYMessage ** mikey)
{
  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);

  return sdp_add_attributes_to_keymgmt (media->attributes, mikey);
}

static GstSDPResult
sdp_add_attributes_to_caps (GArray * attributes, GstCaps * caps)
{
  if (attributes->len > 0) {
    GstStructure *s;
    guint i;

    s = gst_caps_get_structure (caps, 0);

    for (i = 0; i < attributes->len; i++) {
      GstSDPAttribute *attr = &g_array_index (attributes, GstSDPAttribute, i);
      gchar *tofree, *key;

      key = attr->key;

      /* skip some of the attribute we already handle */
      if (!strcmp (key, "fmtp"))
        continue;
      if (!strcmp (key, "rtpmap"))
        continue;
      if (!strcmp (key, "control"))
        continue;
      if (!strcmp (key, "range"))
        continue;
      if (!strcmp (key, "framesize"))
        continue;
      if (!strcmp (key, "key-mgmt"))
        continue;
      if (!strcmp (key, "extmap"))
        continue;
      if (!strcmp (key, "ssrc"))
        continue;
      if (!strcmp (key, "rid"))
        continue;
      if (!strcmp (key, "source-filter"))
        continue;

      /* string must be valid UTF8 */
      if (!g_utf8_validate (attr->value, -1, NULL))
        continue;

      if (!g_str_has_prefix (key, "x-"))
        tofree = key = g_strdup_printf ("a-%s", key);
      else
        tofree = NULL;

      GST_DEBUG ("adding caps: %s=%s", key, attr->value);
      gst_structure_set (s, key, G_TYPE_STRING, attr->value, NULL);
      g_free (tofree);
    }
  }

  return GST_SDP_OK;
}

static GstSDPResult
gst_sdp_media_add_extmap_attributes (GArray * attributes, GstCaps * caps)
{
  const gchar *extmap;
  gchar *p, *tmp, *to_free;
  guint id, i;
  GstStructure *s;

  g_return_val_if_fail (attributes != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  s = gst_caps_get_structure (caps, 0);

  for (i = 0; i < attributes->len; i++) {
    GstSDPAttribute *attr;
    const gchar *direction, *extensionname, *extensionattributes;

    attr = &g_array_index (attributes, GstSDPAttribute, i);
    if (strcmp (attr->key, "extmap") != 0)
      continue;

    extmap = attr->value;

    /* p is now of the format id[/direction] extensionname [extensionattributes] */
    to_free = p = g_strdup (extmap);

    id = strtoul (p, &tmp, 10);
    if (id == 0 || id == 15 || id > 9999 || (*tmp != ' ' && *tmp != '/')) {
      GST_ERROR ("Invalid extmap '%s'", to_free);
      goto next;
    } else if (*tmp == '/') {
      p = tmp;
      p++;

      PARSE_STRING (p, " ", direction);

      /* Invalid format */
      if (direction == NULL || *direction == '\0') {
        GST_ERROR ("Invalid extmap '%s'", to_free);
        goto next;
      }
    } else {
      /* At the space */
      p = tmp;
      direction = "";
    }

    SKIP_SPACES (p);

    tmp = strstr (p, " ");
    if (tmp == NULL) {
      extensionname = p;
      extensionattributes = "";
    } else {
      extensionname = p;
      *tmp = '\0';
      p = tmp + 1;
      SKIP_SPACES (p);
      extensionattributes = p;
    }

    if (extensionname == NULL || *extensionname == '\0') {
      GST_ERROR ("Invalid extmap '%s'", to_free);
      goto next;
    }

    if (*direction != '\0' || *extensionattributes != '\0') {
      GValue arr = G_VALUE_INIT;
      GValue val = G_VALUE_INIT;
      gchar *key;

      key = g_strdup_printf ("extmap-%u", id);

      g_value_init (&arr, GST_TYPE_ARRAY);
      g_value_init (&val, G_TYPE_STRING);

      g_value_set_string (&val, direction);
      gst_value_array_append_value (&arr, &val);

      g_value_set_string (&val, extensionname);
      gst_value_array_append_value (&arr, &val);

      g_value_set_string (&val, extensionattributes);
      gst_value_array_append_value (&arr, &val);

      gst_structure_set_value (s, key, &arr);
      g_value_unset (&val);
      g_value_unset (&arr);
      GST_DEBUG ("adding caps: %s=<%s,%s,%s>", key, direction, extensionname,
          extensionattributes);
      g_free (key);
    } else {
      gchar *key;

      key = g_strdup_printf ("extmap-%u", id);
      gst_structure_set (s, key, G_TYPE_STRING, extensionname, NULL);
      GST_DEBUG ("adding caps: %s=%s", key, extensionname);
      g_free (key);
    }

  next:
    g_free (to_free);
  }
  return GST_SDP_OK;
}

/* parses Source-specific media SDP attributes (RFC5576) into caps */
static GstSDPResult
gst_sdp_media_add_ssrc_attributes (GArray * attributes, GstCaps * caps)
{
  gchar *p, *tmp, *to_free;
  guint i;
  GstStructure *s;

  g_return_val_if_fail (attributes != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  s = gst_caps_get_structure (caps, 0);

  for (i = 0; i < attributes->len; i++) {
    const gchar *value;
    GstSDPAttribute *attr;
    guint32 ssrc;
    gchar *ssrc_val, *ssrc_attr;
    gchar *key;

    attr = &g_array_index (attributes, GstSDPAttribute, i);
    if (strcmp (attr->key, "ssrc") != 0)
      continue;

    value = attr->value;

    /* p is now of the format ssrc attribute[:value] */
    to_free = p = g_strdup (value);

    ssrc = strtoul (p, &tmp, 10);
    if (*tmp != ' ') {
      GST_ERROR ("Invalid ssrc attribute '%s'", to_free);
      goto next;
    }

    /* At the space */
    p = tmp;

    SKIP_SPACES (p);

    tmp = strstr (p, ":");
    if (tmp == NULL) {
      ssrc_attr = tmp;
      ssrc_val = (gchar *) "";
    } else {
      ssrc_attr = p;
      *tmp = '\0';
      p = tmp + 1;
      ssrc_val = p;
    }

    if (ssrc_attr == NULL || *ssrc_attr == '\0') {
      GST_ERROR ("Invalid ssrc attribute '%s'", to_free);
      goto next;
    }

    key = g_strdup_printf ("ssrc-%u-%s", ssrc, ssrc_attr);
    gst_structure_set (s, key, G_TYPE_STRING, ssrc_val, NULL);
    GST_DEBUG ("adding caps: %s=%s", key, ssrc_val);
    g_free (key);

  next:
    g_free (to_free);
  }
  return GST_SDP_OK;
}

/* parses RID SDP attributes (RFC8851) into caps */
static GstSDPResult
gst_sdp_media_add_rid_attributes (GArray * attributes, GstCaps * caps)
{
  const gchar *rid;
  char *p, *to_free;
  guint i;
  GstStructure *s;

  g_return_val_if_fail (attributes != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);
  g_return_val_if_fail (gst_caps_is_writable (caps), GST_SDP_EINVAL);

  s = gst_caps_get_structure (caps, 0);

  for (i = 0; i < attributes->len; i++) {
    GstSDPAttribute *attr;
    const char *direction, *params, *id;
    const char *tmp;

    attr = &g_array_index (attributes, GstSDPAttribute, i);
    if (strcmp (attr->key, "rid") != 0)
      continue;

    rid = attr->value;

    /* p is now of the format id dir ;-separated-params */
    to_free = p = g_strdup (rid);

    PARSE_STRING (p, " ", id);
    if (id == NULL || *id == '\0') {
      GST_ERROR ("Invalid rid \'%s\'", to_free);
      goto next;
    }
    tmp = id;
    while (*tmp && (*tmp == '-' || *tmp == '_' || g_ascii_isalnum (*tmp)))
      tmp++;
    if (*tmp != '\0') {
      GST_ERROR ("Invalid rid-id \'%s\'", id);
      goto next;
    }

    SKIP_SPACES (p);

    PARSE_STRING (p, " ", direction);
    if (direction == NULL || *direction == '\0') {
      direction = p;
      params = NULL;
    } else {
      SKIP_SPACES (p);

      params = p;
    }

    if (direction == NULL || *direction == '\0'
        || (g_strcmp0 (direction, "send") != 0
            && g_strcmp0 (direction, "recv") != 0)) {
      GST_ERROR ("Invalid rid direction \'%s\'", p);
      goto next;
    }

    if (params && *params != '\0') {
      GValue arr = G_VALUE_INIT;
      GValue val = G_VALUE_INIT;
      gchar *key;
#if !defined(GST_DISABLE_DEBUG)
      GString *debug_params = g_string_new (NULL);
      int i = 0;
#endif

      key = g_strdup_printf ("rid-%s", id);

      g_value_init (&arr, GST_TYPE_ARRAY);
      g_value_init (&val, G_TYPE_STRING);

      g_value_set_string (&val, direction);
      gst_value_array_append_and_take_value (&arr, &val);
      val = (GValue) G_VALUE_INIT;

      while (*p) {
        const char *param;
        gboolean done = FALSE;

        PARSE_STRING (p, ";", param);

        if (param) {
        } else if (*p) {
          param = p;
          done = TRUE;
        } else {
          break;
        }

        g_value_init (&val, G_TYPE_STRING);
        g_value_set_string (&val, param);
        gst_value_array_append_and_take_value (&arr, &val);
        val = (GValue) G_VALUE_INIT;
#if !defined(GST_DISABLE_DEBUG)
        if (i++ > 0)
          g_string_append_c (debug_params, ',');
        g_string_append (debug_params, param);
#endif

        if (done)
          break;
      }

      gst_structure_take_value (s, key, &arr);
      arr = (GValue) G_VALUE_INIT;
#if !defined(GST_DISABLE_DEBUG)
      {
        char *debug_str = g_string_free (debug_params, FALSE);
        GST_DEBUG ("adding caps: %s=<%s,%s>", key, direction, debug_str);
        g_free (debug_str);
      }
#endif
      g_free (key);
    } else {
      gchar *key;

      key = g_strdup_printf ("rid-%s", id);
      gst_structure_set (s, key, G_TYPE_STRING, direction, NULL);
      GST_DEBUG ("adding caps: %s=%s", key, direction);
      g_free (key);
    }

  next:
    g_clear_pointer (&to_free, g_free);
  }
  return GST_SDP_OK;
}

/**
 * gst_sdp_message_attributes_to_caps:
 * @msg: a #GstSDPMessage
 * @caps: a #GstCaps
 *
 * Mapping of attributes of #GstSDPMessage to #GstCaps
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.8
 */
GstSDPResult
gst_sdp_message_attributes_to_caps (const GstSDPMessage * msg, GstCaps * caps)
{
  GstSDPResult res;
  GstMIKEYMessage *mikey = NULL;

  g_return_val_if_fail (msg != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  gst_sdp_message_parse_keymgmt (msg, &mikey);
  if (mikey) {
    if (gst_mikey_message_to_caps (mikey, caps)) {
      res = GST_SDP_EINVAL;
      goto done;
    }
  }

  res = sdp_add_attributes_to_caps (msg->attributes, caps);

  if (res == GST_SDP_OK) {
    /* parse global extmap field */
    res = gst_sdp_media_add_extmap_attributes (msg->attributes, caps);
  }

done:
  if (mikey)
    gst_mikey_message_unref (mikey);
  return res;
}

/**
 * gst_sdp_media_attributes_to_caps:
 * @media: a #GstSDPMedia
 * @caps: a #GstCaps
 *
 * Mapping of attributes of #GstSDPMedia to #GstCaps
 *
 * Returns: a #GstSDPResult.
 *
 * Since: 1.8
 */
GstSDPResult
gst_sdp_media_attributes_to_caps (const GstSDPMedia * media, GstCaps * caps)
{
  GstSDPResult res;
  GstMIKEYMessage *mikey = NULL;

  g_return_val_if_fail (media != NULL, GST_SDP_EINVAL);
  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), GST_SDP_EINVAL);

  gst_sdp_media_parse_keymgmt (media, &mikey);
  if (mikey) {
    if (!gst_mikey_message_to_caps (mikey, caps)) {
      res = GST_SDP_EINVAL;
      goto done;
    }
  }

  res = sdp_add_attributes_to_caps (media->attributes, caps);

  if (res == GST_SDP_OK) {
    /* parse media extmap field */
    res = gst_sdp_media_add_extmap_attributes (media->attributes, caps);
  }

  if (res == GST_SDP_OK) {
    /* parse media ssrc field */
    res = gst_sdp_media_add_ssrc_attributes (media->attributes, caps);
  }

  if (res == GST_SDP_OK) {
    /* parse media rid fields */
    res = gst_sdp_media_add_rid_attributes (media->attributes, caps);
  }

done:
  if (mikey)
    gst_mikey_message_unref (mikey);
  return res;
}
