/* GStreamer
 * Copyright (C) <2005,2006> Wim Taymans <wim@fluendo.com>
 *               <2006> Lutz Mueller <lutz at topfrose dot de>
 *               <2015> Tim-Philipp Müller <tim@centricular.com>
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
 * SECTION:gstrtspmessage
 * @title: GstRTSPMessage
 * @short_description: RTSP messages
 * @see_also: gstrtspconnection
 *
 * Provides methods for creating and parsing request, response and data messages.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/gstutils.h>
#include "gstrtspmessage.h"

typedef struct _RTSPKeyValue
{
  GstRTSPHeaderField field;
  gchar *value;
  gchar *custom_key;            /* custom header string (field is INVALID then) */
} RTSPKeyValue;

static void
key_value_foreach (GArray * array, GFunc func, gpointer user_data)
{
  guint i;

  g_return_if_fail (array != NULL);

  for (i = 0; i < array->len; i++) {
    (*func) (&g_array_index (array, RTSPKeyValue, i), user_data);
  }
}

static void
key_value_append (const RTSPKeyValue * kv, GArray * array)
{
  RTSPKeyValue kvcopy;
  g_return_if_fail (kv != NULL);
  g_return_if_fail (array != NULL);

  kvcopy.field = kv->field;
  kvcopy.value = g_strdup (kv->value);
  kvcopy.custom_key = g_strdup (kv->custom_key);

  g_array_append_val (array, kvcopy);
}

static GstRTSPMessage *
gst_rtsp_message_boxed_copy (GstRTSPMessage * orig)
{
  GstRTSPMessage *copy;

  if (gst_rtsp_message_copy (orig, &copy) == GST_RTSP_OK)
    return copy;

  return NULL;
}

static void
gst_rtsp_message_boxed_free (GstRTSPMessage * msg)
{
  gst_rtsp_message_free (msg);
}

G_DEFINE_BOXED_TYPE (GstRTSPMessage, gst_rtsp_msg, gst_rtsp_message_boxed_copy,
    gst_rtsp_message_boxed_free);

/**
 * gst_rtsp_message_new:
 * @msg: (out) (transfer full): a location for the new #GstRTSPMessage
 *
 * Create a new initialized #GstRTSPMessage. Free with gst_rtsp_message_free().
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_new (GstRTSPMessage ** msg)
{
  GstRTSPMessage *newmsg;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  newmsg = g_new0 (GstRTSPMessage, 1);

  *msg = newmsg;

  return gst_rtsp_message_init (newmsg);
}

/**
 * gst_rtsp_message_init:
 * @msg: (transfer none): a #GstRTSPMessage
 *
 * Initialize @msg. This function is mostly used when @msg is allocated on the
 * stack. The reverse operation of this is gst_rtsp_message_unset().
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_init (GstRTSPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  gst_rtsp_message_unset (msg);

  msg->type = GST_RTSP_MESSAGE_INVALID;
  msg->hdr_fields = g_array_new (FALSE, FALSE, sizeof (RTSPKeyValue));

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_get_type:
 * @msg: a #GstRTSPMessage
 *
 * Get the message type of @msg.
 *
 * Returns: the message type.
 */
GstRTSPMsgType
gst_rtsp_message_get_type (GstRTSPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_MESSAGE_INVALID);

  return msg->type;
}

/**
 * gst_rtsp_message_new_request:
 * @msg: (out) (transfer full): a location for the new #GstRTSPMessage
 * @method: the request method to use
 * @uri: (transfer none): the uri of the request
 *
 * Create a new #GstRTSPMessage with @method and @uri and store the result
 * request message in @msg. Free with gst_rtsp_message_free().
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_new_request (GstRTSPMessage ** msg, GstRTSPMethod method,
    const gchar * uri)
{
  GstRTSPMessage *newmsg;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (uri != NULL, GST_RTSP_EINVAL);

  newmsg = g_new0 (GstRTSPMessage, 1);

  *msg = newmsg;

  return gst_rtsp_message_init_request (newmsg, method, uri);
}

/**
 * gst_rtsp_message_init_request:
 * @msg: (transfer none): a #GstRTSPMessage
 * @method: the request method to use
 * @uri: (transfer none): the uri of the request
 *
 * Initialize @msg as a request message with @method and @uri. To clear @msg
 * again, use gst_rtsp_message_unset().
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_init_request (GstRTSPMessage * msg, GstRTSPMethod method,
    const gchar * uri)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (uri != NULL, GST_RTSP_EINVAL);

  gst_rtsp_message_unset (msg);

  msg->type = GST_RTSP_MESSAGE_REQUEST;
  msg->type_data.request.method = method;
  msg->type_data.request.uri = g_strdup (uri);
  msg->type_data.request.version = GST_RTSP_VERSION_1_0;
  msg->hdr_fields = g_array_new (FALSE, FALSE, sizeof (RTSPKeyValue));

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_parse_request:
 * @msg: a #GstRTSPMessage
 * @method: (out) (optional): location to hold the method
 * @uri: (out) (optional) (transfer none): location to hold the uri
 * @version: (out) (optional) (transfer none): location to hold the version
 *
 * Parse the request message @msg and store the values @method, @uri and
 * @version. The result locations can be %NULL if one is not interested in its
 * value.
 *
 * @uri remains valid for as long as @msg is valid and unchanged.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_parse_request (GstRTSPMessage * msg,
    GstRTSPMethod * method, const gchar ** uri, GstRTSPVersion * version)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (msg->type == GST_RTSP_MESSAGE_REQUEST ||
      msg->type == GST_RTSP_MESSAGE_HTTP_REQUEST, GST_RTSP_EINVAL);

  if (method)
    *method = msg->type_data.request.method;
  if (uri)
    *uri = msg->type_data.request.uri;
  if (version)
    *version = msg->type_data.request.version;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_new_response:
 * @msg: (out) (transfer full): a location for the new #GstRTSPMessage
 * @code: the status code
 * @reason: (transfer none) (nullable): the status reason or %NULL
 * @request: (transfer none) (nullable): the request that triggered the response or %NULL
 *
 * Create a new response #GstRTSPMessage with @code and @reason and store the
 * result message in @msg. Free with gst_rtsp_message_free().
 *
 * When @reason is %NULL, the default reason for @code will be used.
 *
 * When @request is not %NULL, the relevant headers will be copied to the new
 * response message.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_new_response (GstRTSPMessage ** msg, GstRTSPStatusCode code,
    const gchar * reason, const GstRTSPMessage * request)
{
  GstRTSPMessage *newmsg;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  newmsg = g_new0 (GstRTSPMessage, 1);

  *msg = newmsg;

  return gst_rtsp_message_init_response (newmsg, code, reason, request);
}

/**
 * gst_rtsp_message_init_response:
 * @msg: (transfer none): a #GstRTSPMessage
 * @code: the status code
 * @reason: (transfer none) (nullable): the status reason or %NULL
 * @request: (transfer none) (nullable): the request that triggered the response or %NULL
 *
 * Initialize @msg with @code and @reason.
 *
 * When @reason is %NULL, the default reason for @code will be used.
 *
 * When @request is not %NULL, the relevant headers will be copied to the new
 * response message.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_init_response (GstRTSPMessage * msg, GstRTSPStatusCode code,
    const gchar * reason, const GstRTSPMessage * request)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  gst_rtsp_message_unset (msg);

  if (reason == NULL)
    reason = gst_rtsp_status_as_text (code);

  msg->type = GST_RTSP_MESSAGE_RESPONSE;
  msg->type_data.response.code = code;
  msg->type_data.response.reason = g_strdup (reason);
  msg->type_data.response.version = GST_RTSP_VERSION_1_0;
  msg->hdr_fields = g_array_new (FALSE, FALSE, sizeof (RTSPKeyValue));

  if (request) {
    if (request->type == GST_RTSP_MESSAGE_HTTP_REQUEST) {
      msg->type = GST_RTSP_MESSAGE_HTTP_RESPONSE;
      if (request->type_data.request.version != GST_RTSP_VERSION_INVALID)
        msg->type_data.response.version = request->type_data.request.version;
      else
        msg->type_data.response.version = GST_RTSP_VERSION_1_1;
    } else {
      gchar *header;

      /* copy CSEQ */
      if (gst_rtsp_message_get_header (request, GST_RTSP_HDR_CSEQ, &header,
              0) == GST_RTSP_OK) {
        gst_rtsp_message_add_header (msg, GST_RTSP_HDR_CSEQ, header);
      }

      /* copy session id */
      if (gst_rtsp_message_get_header (request, GST_RTSP_HDR_SESSION, &header,
              0) == GST_RTSP_OK) {
        char *pos;

        header = g_strdup (header);
        if ((pos = strchr (header, ';'))) {
          *pos = '\0';
        }
        g_strchomp (header);
        gst_rtsp_message_take_header (msg, GST_RTSP_HDR_SESSION, header);
      }

      /* FIXME copy more headers? */
    }
  }

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_parse_response:
 * @msg: a #GstRTSPMessage
 * @code: (out) (optional): location to hold the status code
 * @reason: (out) (optional) (transfer none): location to hold the status reason
 * @version: (out) (optional) (transfer none): location to hold the version
 *
 * Parse the response message @msg and store the values @code, @reason and
 * @version. The result locations can be %NULL if one is not interested in its
 * value.
 *
 * @reason remains valid for as long as @msg is valid and unchanged.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_parse_response (GstRTSPMessage * msg,
    GstRTSPStatusCode * code, const gchar ** reason, GstRTSPVersion * version)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (msg->type == GST_RTSP_MESSAGE_RESPONSE ||
      msg->type == GST_RTSP_MESSAGE_HTTP_RESPONSE, GST_RTSP_EINVAL);

  if (code)
    *code = msg->type_data.response.code;
  if (reason)
    *reason = msg->type_data.response.reason;
  if (version)
    *version = msg->type_data.response.version;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_new_data:
 * @msg: (out) (transfer full): a location for the new #GstRTSPMessage
 * @channel: the channel
 *
 * Create a new data #GstRTSPMessage with @channel and store the
 * result message in @msg. Free with gst_rtsp_message_free().
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_new_data (GstRTSPMessage ** msg, guint8 channel)
{
  GstRTSPMessage *newmsg;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  newmsg = g_new0 (GstRTSPMessage, 1);

  *msg = newmsg;

  return gst_rtsp_message_init_data (newmsg, channel);
}

/**
 * gst_rtsp_message_init_data:
 * @msg: (transfer none): a #GstRTSPMessage
 * @channel: a channel
 *
 * Initialize a new data #GstRTSPMessage for @channel.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_init_data (GstRTSPMessage * msg, guint8 channel)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  gst_rtsp_message_unset (msg);

  msg->type = GST_RTSP_MESSAGE_DATA;
  msg->type_data.data.channel = channel;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_parse_data:
 * @msg: a #GstRTSPMessage
 * @channel: (out) (optional): location to hold the channel
 *
 * Parse the data message @msg and store the channel in @channel.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_parse_data (GstRTSPMessage * msg, guint8 * channel)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (msg->type == GST_RTSP_MESSAGE_DATA, GST_RTSP_EINVAL);

  if (channel)
    *channel = msg->type_data.data.channel;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_unset:
 * @msg: a #GstRTSPMessage
 *
 * Unset the contents of @msg so that it becomes an uninitialized
 * #GstRTSPMessage again. This function is mostly used in combination with
 * gst_rtsp_message_init_request(), gst_rtsp_message_init_response() and
 * gst_rtsp_message_init_data() on stack allocated #GstRTSPMessage structures.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_unset (GstRTSPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  switch (msg->type) {
    case GST_RTSP_MESSAGE_INVALID:
      break;
    case GST_RTSP_MESSAGE_REQUEST:
    case GST_RTSP_MESSAGE_HTTP_REQUEST:
      g_free (msg->type_data.request.uri);
      break;
    case GST_RTSP_MESSAGE_RESPONSE:
    case GST_RTSP_MESSAGE_HTTP_RESPONSE:
      g_free (msg->type_data.response.reason);
      break;
    case GST_RTSP_MESSAGE_DATA:
      break;
    default:
      g_return_val_if_reached (GST_RTSP_EINVAL);
  }

  if (msg->hdr_fields != NULL) {
    guint i;

    for (i = 0; i < msg->hdr_fields->len; i++) {
      RTSPKeyValue *keyval = &g_array_index (msg->hdr_fields, RTSPKeyValue, i);

      g_free (keyval->value);
      g_free (keyval->custom_key);
    }
    g_array_free (msg->hdr_fields, TRUE);
  }
  g_free (msg->body);
  gst_buffer_replace (&msg->body_buffer, NULL);

  memset (msg, 0, sizeof (GstRTSPMessage));

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_free:
 * @msg: a #GstRTSPMessage
 *
 * Free the memory used by @msg.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_free (GstRTSPMessage * msg)
{
  GstRTSPResult res;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  res = gst_rtsp_message_unset (msg);
  if (res == GST_RTSP_OK)
    g_free (msg);

  return res;
}

/**
 * gst_rtsp_message_copy:
 * @msg: a #GstRTSPMessage
 * @copy: (out) (nullable) (transfer full): pointer to new #GstRTSPMessage
 *
 * Allocate a new copy of @msg and store the result in @copy. The value in
 * @copy should be release with gst_rtsp_message_free function.
 *
 * Returns: a #GstRTSPResult
 *
 * Since: 1.14
 */
GstRTSPResult
gst_rtsp_message_copy (const GstRTSPMessage * msg, GstRTSPMessage ** copy)
{
  GstRTSPResult ret;
  GstRTSPMessage *cp;

  g_return_val_if_fail (copy != NULL, GST_RTSP_EINVAL);

  *copy = NULL;

  if (msg == NULL)
    return GST_RTSP_EINVAL;

  ret = gst_rtsp_message_new (copy);
  if (ret != GST_RTSP_OK)
    return ret;

  cp = *copy;

  cp->type = msg->type;
  switch (cp->type) {
    case GST_RTSP_MESSAGE_INVALID:
      break;
    case GST_RTSP_MESSAGE_REQUEST:
    case GST_RTSP_MESSAGE_HTTP_REQUEST:
      cp->type_data.request.method = msg->type_data.request.method;
      cp->type_data.request.uri = g_strdup (msg->type_data.request.uri);
      cp->type_data.request.version = msg->type_data.request.version;
      break;
    case GST_RTSP_MESSAGE_RESPONSE:
    case GST_RTSP_MESSAGE_HTTP_RESPONSE:
      cp->type_data.response.code = msg->type_data.response.code;
      cp->type_data.response.reason = g_strdup (msg->type_data.response.reason);
      cp->type_data.response.version = msg->type_data.response.version;
      break;
    case GST_RTSP_MESSAGE_DATA:
      cp->type_data.data.channel = msg->type_data.data.channel;
      break;
    default:
      return GST_RTSP_EINVAL;
  }

  key_value_foreach (msg->hdr_fields, (GFunc) key_value_append, cp->hdr_fields);
  if (msg->body)
    gst_rtsp_message_set_body (cp, msg->body, msg->body_size);
  else
    gst_rtsp_message_set_body_buffer (cp, msg->body_buffer);

  return GST_RTSP_OK;
}


/**
 * gst_rtsp_message_take_header:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 * @value: (transfer full): the value of the header
 *
 * Add a header with key @field and @value to @msg. This function takes
 * ownership of @value.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_take_header (GstRTSPMessage * msg, GstRTSPHeaderField field,
    gchar * value)
{
  RTSPKeyValue key_value;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (value != NULL, GST_RTSP_EINVAL);

  key_value.field = field;
  key_value.value = value;
  key_value.custom_key = NULL;

  g_array_append_val (msg->hdr_fields, key_value);

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_add_header:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 * @value: (transfer none): the value of the header
 *
 * Add a header with key @field and @value to @msg. This function takes a copy
 * of @value.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_add_header (GstRTSPMessage * msg, GstRTSPHeaderField field,
    const gchar * value)
{
  return gst_rtsp_message_take_header (msg, field, g_strdup (value));
}

/**
 * gst_rtsp_message_remove_header:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 * @indx: the index of the header
 *
 * Remove the @indx header with key @field from @msg. If @indx equals -1, all
 * headers will be removed.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_message_remove_header (GstRTSPMessage * msg, GstRTSPHeaderField field,
    gint indx)
{
  GstRTSPResult res = GST_RTSP_ENOTIMPL;
  guint i = 0;
  gint cnt = 0;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  while (i < msg->hdr_fields->len) {
    RTSPKeyValue *key_value = &g_array_index (msg->hdr_fields, RTSPKeyValue, i);

    if (key_value->field == field && (indx == -1 || cnt++ == indx)) {
      g_free (key_value->value);
      g_array_remove_index (msg->hdr_fields, i);
      res = GST_RTSP_OK;
      if (indx != -1)
        break;
    } else {
      i++;
    }
  }
  return res;
}

/**
 * gst_rtsp_message_get_header:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 * @value: (out) (transfer none) (optional) (nullable): pointer to hold the result
 * @indx: the index of the header
 *
 * Get the @indx header value with key @field from @msg. The result in @value
 * stays valid as long as it remains present in @msg.
 *
 * Returns: #GST_RTSP_OK when @field was found, #GST_RTSP_ENOTIMPL if the key
 * was not found.
 */
GstRTSPResult
gst_rtsp_message_get_header (const GstRTSPMessage * msg,
    GstRTSPHeaderField field, gchar ** value, gint indx)
{
  guint i;
  gint cnt = 0;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  if (value)
    *value = NULL;

  /* no header initialized, there are no headers */
  if (msg->hdr_fields == NULL)
    return GST_RTSP_ENOTIMPL;

  for (i = 0; i < msg->hdr_fields->len; i++) {
    RTSPKeyValue *key_value = &g_array_index (msg->hdr_fields, RTSPKeyValue, i);

    if (key_value->field == field && cnt++ == indx) {
      if (value)
        *value = key_value->value;
      return GST_RTSP_OK;
    }
  }

  return GST_RTSP_ENOTIMPL;
}

/**
 * gst_rtsp_message_add_header_by_name:
 * @msg: a #GstRTSPMessage
 * @header: (transfer none): header string
 * @value: (transfer none): the value of the header
 *
 * Add a header with key @header and @value to @msg. This function takes a copy
 * of @value.
 *
 * Returns: a #GstRTSPResult.
 *
 * Since: 1.6
 */
GstRTSPResult
gst_rtsp_message_add_header_by_name (GstRTSPMessage * msg,
    const gchar * header, const gchar * value)
{
  GstRTSPHeaderField field;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (header != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (value != NULL, GST_RTSP_EINVAL);

  field = gst_rtsp_find_header_field (header);
  if (field != GST_RTSP_HDR_INVALID)
    return gst_rtsp_message_take_header (msg, field, g_strdup (value));

  return gst_rtsp_message_take_header_by_name (msg, header, g_strdup (value));
}

/**
 * gst_rtsp_message_take_header_by_name:
 * @msg: a #GstRTSPMessage
 * @header: (transfer none): a header string
 * @value: (transfer full): the value of the header
 *
 * Add a header with key @header and @value to @msg. This function takes
 * ownership of @value, but not of @header.
 *
 * Returns: a #GstRTSPResult.
 *
 * Since: 1.6
 */
GstRTSPResult
gst_rtsp_message_take_header_by_name (GstRTSPMessage * msg,
    const gchar * header, gchar * value)
{
  RTSPKeyValue key_value;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (header != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (value != NULL, GST_RTSP_EINVAL);

  key_value.field = GST_RTSP_HDR_INVALID;
  key_value.value = value;
  key_value.custom_key = g_strdup (header);

  g_array_append_val (msg->hdr_fields, key_value);

  return GST_RTSP_OK;
}

/* returns -1 if not found, otherwise index position within msg->hdr_fields */
static gint
gst_rtsp_message_find_header_by_name (GstRTSPMessage * msg,
    const gchar * header, gint index)
{
  GstRTSPHeaderField field;
  gint cnt = 0;
  guint i;

  /* no header initialized, there are no headers */
  if (msg->hdr_fields == NULL)
    return -1;

  field = gst_rtsp_find_header_field (header);
  for (i = 0; i < msg->hdr_fields->len; i++) {
    RTSPKeyValue *key_val;

    key_val = &g_array_index (msg->hdr_fields, RTSPKeyValue, i);

    if (key_val->field != field)
      continue;

    if (key_val->custom_key != NULL &&
        g_ascii_strcasecmp (key_val->custom_key, header) != 0)
      continue;

    if (index < 0 || cnt++ == index)
      return i;
  }

  return -1;
}

/**
 * gst_rtsp_message_remove_header_by_name:
 * @msg: a #GstRTSPMessage
 * @header: the header string
 * @index: the index of the header
 *
 * Remove the @index header with key @header from @msg. If @index equals -1,
 * all matching headers will be removed.
 *
 * Returns: a #GstRTSPResult
 *
 * Since: 1.6
 */
GstRTSPResult
gst_rtsp_message_remove_header_by_name (GstRTSPMessage * msg,
    const gchar * header, gint index)
{
  GstRTSPResult res = GST_RTSP_ENOTIMPL;
  RTSPKeyValue *kv;
  gint pos;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (header != NULL, GST_RTSP_EINVAL);

  do {
    pos = gst_rtsp_message_find_header_by_name (msg, header, index);

    if (pos < 0)
      break;

    kv = &g_array_index (msg->hdr_fields, RTSPKeyValue, pos);
    g_free (kv->value);
    g_free (kv->custom_key);
    g_array_remove_index (msg->hdr_fields, pos);
    res = GST_RTSP_OK;
  } while (index < 0);

  return res;
}

/**
 * gst_rtsp_message_get_header_by_name:
 * @msg: a #GstRTSPMessage
 * @header: a #GstRTSPHeaderField
 * @value: (out) (transfer none) (optional) (nullable): pointer to hold the result
 * @index: the index of the header
 *
 * Get the @index header value with key @header from @msg. The result in @value
 * stays valid as long as it remains present in @msg.
 *
 * Returns: #GST_RTSP_OK when @field was found, #GST_RTSP_ENOTIMPL if the key
 * was not found.
 *
 * Since: 1.6
 */
GstRTSPResult
gst_rtsp_message_get_header_by_name (GstRTSPMessage * msg,
    const gchar * header, gchar ** value, gint index)
{
  RTSPKeyValue *key_val;
  gint pos;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (header != NULL, GST_RTSP_EINVAL);

  if (value)
    *value = NULL;

  pos = gst_rtsp_message_find_header_by_name (msg, header, index);

  if (pos < 0)
    return GST_RTSP_ENOTIMPL;

  key_val = &g_array_index (msg->hdr_fields, RTSPKeyValue, pos);

  if (value)
    *value = key_val->value;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_append_headers:
 * @msg: a #GstRTSPMessage
 * @str: (transfer none): a string
 *
 * Append the currently configured headers in @msg to the #GString @str suitable
 * for transmission.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_append_headers (const GstRTSPMessage * msg, GString * str)
{
  guint i;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (str != NULL, GST_RTSP_EINVAL);

  for (i = 0; i < msg->hdr_fields->len; i++) {
    RTSPKeyValue *key_value;
    const gchar *keystr;

    key_value = &g_array_index (msg->hdr_fields, RTSPKeyValue, i);

    if (key_value->custom_key != NULL)
      keystr = key_value->custom_key;
    else
      keystr = gst_rtsp_header_as_text (key_value->field);

    g_string_append_printf (str, "%s: %s\r\n", keystr, key_value->value);
  }
  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_set_body:
 * @msg: a #GstRTSPMessage
 * @data: (array length=size) (transfer none): the data
 * @size: the size of @data
 *
 * Set the body of @msg to a copy of @data. Any existing body or body buffer
 * will be replaced by the new body.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_set_body (GstRTSPMessage * msg, const guint8 * data,
    guint size)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  return gst_rtsp_message_take_body (msg, g_memdup2 (data, size), size);
}

/**
 * gst_rtsp_message_take_body:
 * @msg: a #GstRTSPMessage
 * @data: (array length=size) (transfer full): the data
 * @size: the size of @data
 *
 * Set the body of @msg to @data and @size. This method takes ownership of
 * @data. Any existing body or body buffer will be replaced by the new body.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_take_body (GstRTSPMessage * msg, guint8 * data, guint size)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (data != NULL || size == 0, GST_RTSP_EINVAL);

  gst_buffer_replace (&msg->body_buffer, NULL);
  g_free (msg->body);

  msg->body = data;
  msg->body_size = size;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_get_body:
 * @msg: a #GstRTSPMessage
 * @data: (out) (transfer none) (array length=size): location for the data
 * @size: (out): location for the size of @data
 *
 * Get the body of @msg. @data remains valid for as long as @msg is valid and
 * unchanged.
 *
 * If the message body was set as a #GstBuffer before this will cause the data
 * to be copied and stored in the message. The #GstBuffer will no longer be
 * kept in the message.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_get_body (const GstRTSPMessage * msg, guint8 ** data,
    guint * size)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (data != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (size != NULL, GST_RTSP_EINVAL);

  if (msg->body_buffer) {
    gsize size;

    gst_buffer_extract_dup (msg->body_buffer, 0,
        gst_buffer_get_size (msg->body_buffer),
        (gpointer *) & ((GstRTSPMessage *) msg)->body, &size);
    gst_buffer_replace (&((GstRTSPMessage *) msg)->body_buffer, NULL);
    ((GstRTSPMessage *) msg)->body_size = size;
  }

  *data = msg->body;
  *size = msg->body_size;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_steal_body:
 * @msg: a #GstRTSPMessage
 * @data: (out) (transfer full) (array length=size): location for the data
 * @size: (out): location for the size of @data
 *
 * Take the body of @msg and store it in @data and @size. After this method,
 * the body and size of @msg will be set to %NULL and 0 respectively.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_steal_body (GstRTSPMessage * msg, guint8 ** data, guint * size)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (data != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (size != NULL, GST_RTSP_EINVAL);

  if (msg->body_buffer) {
    gsize size;

    gst_buffer_extract_dup (msg->body_buffer, 0,
        gst_buffer_get_size (msg->body_buffer),
        (gpointer *) & msg->body, &size);
    gst_buffer_replace (&msg->body_buffer, NULL);
    msg->body_size = size;
  }

  *data = msg->body;
  *size = msg->body_size;

  msg->body = NULL;
  msg->body_size = 0;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_set_body_buffer:
 * @msg: a #GstRTSPMessage
 * @buffer: a #GstBuffer
 *
 * Set the body of @msg to @buffer. Any existing body or body buffer
 * will be replaced by the new body.
 *
 * Returns: #GST_RTSP_OK.
 *
 * Since: 1.16
 */
GstRTSPResult
gst_rtsp_message_set_body_buffer (GstRTSPMessage * msg, GstBuffer * buffer)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  return gst_rtsp_message_take_body_buffer (msg,
      buffer ? gst_buffer_ref (buffer) : NULL);
}

/**
 * gst_rtsp_message_take_body_buffer:
 * @msg: a #GstRTSPMessage
 * @buffer: (transfer full): a #GstBuffer
 *
 * Set the body of @msg to @buffer. This method takes ownership of @buffer.
 * Any existing body or body buffer will be replaced by the new body.
 *
 * Returns: #GST_RTSP_OK.
 *
 * Since: 1.16
 */
GstRTSPResult
gst_rtsp_message_take_body_buffer (GstRTSPMessage * msg, GstBuffer * buffer)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  g_free (msg->body);
  msg->body = NULL;
  if (msg->body_buffer)
    gst_buffer_unref (msg->body_buffer);
  msg->body_buffer = buffer;
  msg->body_size = buffer ? gst_buffer_get_size (buffer) : 0;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_get_body_buffer:
 * @msg: a #GstRTSPMessage
 * @buffer: (out) (transfer none): location for the buffer
 *
 * Get the body of @msg. @buffer remains valid for as long as @msg is valid and
 * unchanged.
 *
 * If body data was set from raw memory instead of a #GstBuffer this function
 * will always return %NULL. The caller can check if there is a body buffer by
 * calling gst_rtsp_message_has_body_buffer().
 *
 * Returns: #GST_RTSP_OK.
 *
 * Since: 1.16
 */
GstRTSPResult
gst_rtsp_message_get_body_buffer (const GstRTSPMessage * msg,
    GstBuffer ** buffer)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (buffer != NULL, GST_RTSP_EINVAL);

  *buffer = msg->body_buffer;

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_steal_body_buffer:
 * @msg: a #GstRTSPMessage
 * @buffer: (out) (transfer full): location for the buffer
 *
 * Take the body of @msg and store it in @buffer. After this method,
 * the body and size of @msg will be set to %NULL and 0 respectively.
 *
 * If body data was set from raw memory instead of a #GstBuffer this function
 * will always return %NULL. The caller can check if there is a body buffer by
 * calling gst_rtsp_message_has_body_buffer().
 *
 * Returns: #GST_RTSP_OK.
 *
 * Since: 1.16
 */
GstRTSPResult
gst_rtsp_message_steal_body_buffer (GstRTSPMessage * msg, GstBuffer ** buffer)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (buffer != NULL, GST_RTSP_EINVAL);

  if (msg->body_buffer) {
    *buffer = msg->body_buffer;
    msg->body_buffer = NULL;
    msg->body_size = 0;
  } else {
    *buffer = NULL;
  }

  return GST_RTSP_OK;
}

/**
 * gst_rtsp_message_has_body_buffer:
 * @msg: a #GstRTSPMessage
 *
 * Checks if @msg has a body and the body is stored as #GstBuffer.
 *
 * Returns: %TRUE if @msg has a body and it's stored as #GstBuffer, %FALSE
 * otherwise.
 *
 * Since: 1.16
 */
gboolean
gst_rtsp_message_has_body_buffer (const GstRTSPMessage * msg)
{
  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  return msg->body_buffer != NULL;
}

static void
dump_key_value (gpointer data, gpointer user_data G_GNUC_UNUSED)
{
  RTSPKeyValue *key_value = (RTSPKeyValue *) data;
  const gchar *key_string;

  if (key_value->custom_key != NULL)
    key_string = key_value->custom_key;
  else
    key_string = gst_rtsp_header_as_text (key_value->field);

  g_print ("   key: '%s', value: '%s'\n", key_string, key_value->value);
}

/**
 * gst_rtsp_message_dump:
 * @msg: a #GstRTSPMessage
 *
 * Dump the contents of @msg to stdout.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_message_dump (GstRTSPMessage * msg)
{
  guint8 *data = NULL;
  guint size;
  GstBuffer *body_buffer = NULL;

#define PRINT_BODY G_STMT_START { \
  gst_rtsp_message_get_body_buffer (msg, &body_buffer); \
  if (body_buffer) { \
    gst_util_dump_buffer (body_buffer); \
  } else { \
    gst_rtsp_message_get_body (msg, &data, &size); \
    if (data) \
      gst_util_dump_mem (data, size); \
  } \
} G_STMT_END;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  switch (msg->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      g_print ("RTSP request message %p\n", msg);
      g_print (" request line:\n");
      g_print ("   method: '%s'\n",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      g_print ("   uri:    '%s'\n", msg->type_data.request.uri);
      g_print ("   version: '%s'\n",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      g_print (" headers:\n");
      key_value_foreach (msg->hdr_fields, dump_key_value, NULL);
      g_print (" body:\n");
      PRINT_BODY;
      break;
    case GST_RTSP_MESSAGE_RESPONSE:
      g_print ("RTSP response message %p\n", msg);
      g_print (" status line:\n");
      g_print ("   code:   '%d'\n", msg->type_data.response.code);
      g_print ("   reason: '%s'\n", msg->type_data.response.reason);
      g_print ("   version: '%s'\n",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      g_print (" headers:\n");
      key_value_foreach (msg->hdr_fields, dump_key_value, NULL);
      PRINT_BODY;
      break;
    case GST_RTSP_MESSAGE_HTTP_REQUEST:
      g_print ("HTTP request message %p\n", msg);
      g_print (" request line:\n");
      g_print ("   method:  '%s'\n",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      g_print ("   uri:     '%s'\n", msg->type_data.request.uri);
      g_print ("   version: '%s'\n",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      g_print (" headers:\n");
      key_value_foreach (msg->hdr_fields, dump_key_value, NULL);
      g_print (" body:\n");
      PRINT_BODY;
      break;
    case GST_RTSP_MESSAGE_HTTP_RESPONSE:
      g_print ("HTTP response message %p\n", msg);
      g_print (" status line:\n");
      g_print ("   code:    '%d'\n", msg->type_data.response.code);
      g_print ("   reason:  '%s'\n", msg->type_data.response.reason);
      g_print ("   version: '%s'\n",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      g_print (" headers:\n");
      key_value_foreach (msg->hdr_fields, dump_key_value, NULL);
      PRINT_BODY;
      break;
    case GST_RTSP_MESSAGE_DATA:
      g_print ("RTSP data message %p\n", msg);
      g_print (" channel: '%d'\n", msg->type_data.data.channel);
      g_print (" size:    '%d'\n", msg->body_size);
      PRINT_BODY;
      break;
    default:
      g_print ("unsupported message type %d\n", msg->type);
      return GST_RTSP_EINVAL;
  }
  return GST_RTSP_OK;

#undef PRINT_BODY
}


static const gchar *
skip_lws (const gchar * s)
{
  while (g_ascii_isspace (*s))
    s++;
  return s;
}

static const gchar *
skip_commas (const gchar * s)
{
  /* The grammar allows for multiple commas */
  while (g_ascii_isspace (*s) || *s == ',')
    s++;
  return s;
}

static const gchar *
skip_scheme (const gchar * s)
{
  while (*s && !g_ascii_isspace (*s))
    s++;
  return s;
}

static const gchar *
skip_item (const gchar * s)
{
  gboolean quoted = FALSE;

  /* A list item ends at the last non-whitespace character
   * before a comma which is not inside a quoted-string. Or at
   * the end of the string.
   */
  while (*s) {
    if (*s == '"') {
      quoted = !quoted;
    } else if (quoted) {
      if (*s == '\\' && *(s + 1))
        s++;
    } else {
      if (*s == ',' || g_ascii_isspace (*s))
        break;
    }
    s++;
  }

  return s;
}

static void
decode_quoted_string (gchar * quoted_string)
{
  gchar *src, *dst;

  src = quoted_string + 1;
  dst = quoted_string;
  while (*src && *src != '"') {
    if (*src == '\\' && *(src + 1))
      src++;
    *dst++ = *src++;
  }
  *dst = '\0';
}

static void
parse_auth_credentials (GPtrArray * auth_credentials, const gchar * header,
    GstRTSPHeaderField field)
{
  while (header[0] != '\0') {
    const gchar *end;
    GstRTSPAuthCredential *auth_credential;

    /* Skip whitespace at the start of the string */
    header = skip_lws (header);
    if (header[0] == '\0')
      break;

    /* Skip until end of string or whitespace: end of scheme */
    end = skip_scheme (header);

    auth_credential = g_new0 (GstRTSPAuthCredential, 1);

    if (g_ascii_strncasecmp (header, "basic", 5) == 0) {
      auth_credential->scheme = GST_RTSP_AUTH_BASIC;
    } else if (g_ascii_strncasecmp (header, "digest", 6) == 0) {
      auth_credential->scheme = GST_RTSP_AUTH_DIGEST;
    } else {
      /* Not supported, skip */
      g_free (auth_credential);
      header = end;
      continue;
    }

    /* Basic Authorization request has only an unformatted blurb following, all
     * other variants have comma-separated name=value pairs */
    if (end[0] != '\0' && field == GST_RTSP_HDR_AUTHORIZATION
        && auth_credential->scheme == GST_RTSP_AUTH_BASIC) {
      auth_credential->authorization = g_strdup (end + 1);
      header = end;
    } else if (end[0] != '\0') {
      GPtrArray *params;

      params = g_ptr_array_new ();

      /* Space or start of param */
      header = end;

      /* Parse a header whose content is described by RFC2616 as
       * "#something", where "something" does not itself contain commas,
       * except as part of quoted-strings, into a list of allocated strings.
       */
      while (*header) {
        const gchar *item_end;
        const gchar *eq;

        header = skip_commas (header);
        item_end = skip_item (header);

        for (eq = header; *eq != '\0' && *eq != '=' && eq < item_end; eq++);
        if (eq[0] == '=') {
          GstRTSPAuthParam *auth_param = g_new0 (GstRTSPAuthParam, 1);
          const gchar *value;

          /* have an actual param */
          auth_param->name = g_strndup (header, eq - header);

          value = eq + 1;
          value = skip_lws (value);
          auth_param->value = g_strndup (value, item_end - value);
          if (value[0] == '"')
            decode_quoted_string (auth_param->value);

          g_ptr_array_add (params, auth_param);
          header = item_end;
        } else {
          /* at next scheme, header at start of it */
          break;
        }
      }
      if (params->len)
        g_ptr_array_add (params, NULL);
      auth_credential->params =
          (GstRTSPAuthParam **) g_ptr_array_free (params, FALSE);
    } else {
      header = end;
    }
    g_ptr_array_add (auth_credentials, auth_credential);

    /* WWW-Authenticate allows multiple, Authorization allows one */
    if (field == GST_RTSP_HDR_AUTHORIZATION)
      break;
  }
}

/**
 * gst_rtsp_message_parse_auth_credentials:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 *
 * Parses the credentials given in a WWW-Authenticate or Authorization header.
 *
 * Returns: (transfer full) (array zero-terminated=1):
 *     %NULL-terminated array of GstRTSPAuthCredential or %NULL.
 *
 * Since: 1.12
 */
GstRTSPAuthCredential **
gst_rtsp_message_parse_auth_credentials (GstRTSPMessage * msg,
    GstRTSPHeaderField field)
{
  gchar *header;
  GPtrArray *auth_credentials;
  gint i;

  g_return_val_if_fail (msg != NULL, NULL);

  auth_credentials = g_ptr_array_new ();

  i = 0;
  while (gst_rtsp_message_get_header (msg, field, &header, i) == GST_RTSP_OK) {
    parse_auth_credentials (auth_credentials, header, field);
    i++;
  }

  if (auth_credentials->len)
    g_ptr_array_add (auth_credentials, NULL);

  return (GstRTSPAuthCredential **) g_ptr_array_free (auth_credentials, FALSE);
}

GstRTSPAuthParam *
gst_rtsp_auth_param_copy (GstRTSPAuthParam * param)
{
  GstRTSPAuthParam *copy;

  if (param == NULL)
    return NULL;

  copy = g_new0 (GstRTSPAuthParam, 1);
  copy->name = g_strdup (param->name);
  copy->value = g_strdup (param->value);

  return copy;
}

void
gst_rtsp_auth_param_free (GstRTSPAuthParam * param)
{
  if (param != NULL) {
    g_free (param->name);
    g_free (param->value);
    g_free (param);
  }
}

G_DEFINE_BOXED_TYPE (GstRTSPAuthParam, gst_rtsp_auth_param,
    (GBoxedCopyFunc) gst_rtsp_auth_param_copy,
    (GBoxedFreeFunc) gst_rtsp_auth_param_free);

static void
gst_rtsp_auth_credential_free (GstRTSPAuthCredential * credential)
{
  GstRTSPAuthParam **p;

  if (credential == NULL)
    return;

  for (p = credential->params; p != NULL && *p != NULL; ++p)
    gst_rtsp_auth_param_free (*p);

  g_free (credential->params);
  g_free (credential->authorization);
  g_free (credential);
}

static GstRTSPAuthCredential *
gst_rtsp_auth_credential_copy (GstRTSPAuthCredential * cred)
{
  GstRTSPAuthCredential *copy;

  if (cred == NULL)
    return NULL;

  copy = g_new0 (GstRTSPAuthCredential, 1);
  copy->scheme = cred->scheme;
  if (cred->params) {
    guint i, n_params = g_strv_length ((gchar **) cred->params);

    copy->params = g_new0 (GstRTSPAuthParam *, n_params + 1);
    for (i = 0; i < n_params; ++i)
      copy->params[i] = gst_rtsp_auth_param_copy (cred->params[i]);
  }
  copy->authorization = g_strdup (cred->authorization);
  return copy;
}

/**
 * gst_rtsp_auth_credentials_free:
 * @credentials: a %NULL-terminated array of #GstRTSPAuthCredential
 *
 * Free a %NULL-terminated array of credentials returned from
 * gst_rtsp_message_parse_auth_credentials().
 *
 * Since: 1.12
 */
void
gst_rtsp_auth_credentials_free (GstRTSPAuthCredential ** credentials)
{
  GstRTSPAuthCredential **p;

  if (!credentials)
    return;

  for (p = credentials; p != NULL && *p != NULL; ++p)
    gst_rtsp_auth_credential_free (*p);

  g_free (credentials);
}

G_DEFINE_BOXED_TYPE (GstRTSPAuthCredential, gst_rtsp_auth_credential,
    (GBoxedCopyFunc) gst_rtsp_auth_credential_copy,
    (GBoxedFreeFunc) gst_rtsp_auth_credential_free);
