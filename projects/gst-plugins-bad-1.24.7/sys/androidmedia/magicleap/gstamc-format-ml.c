/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstamc-internal-ml.h"
#include "../gstamc-format.h"

#include <ml_media_format.h>

struct _GstAmcFormat
{
  MLHandle handle;
};

static GstAmcFormat *
gst_amc_format_ml_new_audio (const gchar * mime, gint sample_rate,
    gint channels, GError ** err)
{
  GstAmcFormat *format = g_slice_new0 (GstAmcFormat);
  MLResult result;

  result =
      MLMediaFormatCreateAudio (mime, sample_rate, channels, &format->handle);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to create audio format");
    g_slice_free (GstAmcFormat, format);
    return NULL;
  }

  return format;
}

static GstAmcFormat *
gst_amc_format_ml_new_video (const gchar * mime, gint width, gint height,
    GError ** err)
{
  GstAmcFormat *format = g_slice_new0 (GstAmcFormat);
  MLResult result;

  result = MLMediaFormatCreateVideo (mime, width, height, &format->handle);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to create video format");
    g_slice_free (GstAmcFormat, format);
    return NULL;
  }

  return format;
}

GstAmcFormat *
gst_amc_format_ml_new_handle (MLHandle handle)
{
  GstAmcFormat *format = g_slice_new0 (GstAmcFormat);
  format->handle = handle;
  return format;
}

MLHandle
gst_amc_format_ml_get_handle (GstAmcFormat * format)
{
  return format->handle;
}

static void
gst_amc_format_ml_free (GstAmcFormat * format)
{
  g_return_if_fail (format != NULL);
  g_slice_free (GstAmcFormat, format);
}

static gchar *
gst_amc_format_ml_to_string (GstAmcFormat * format, GError ** err)
{
  MLResult result;
  gchar *str;

  str = g_new0 (gchar, MAX_FORMAT_STRING_SIZE);
  result = MLMediaFormatObjectToString (format->handle, str);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to convert format to string: %d", result);
    g_free (str);
    return NULL;
  }

  return str;
}

static gboolean
gst_amc_format_ml_get_float (GstAmcFormat * format, const gchar * key,
    gfloat * value, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  result = MLMediaFormatGetKeyValueFloat (format->handle, key, value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get float");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_format_ml_set_float (GstAmcFormat * format, const gchar * key,
    gfloat value, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  result = MLMediaFormatSetKeyFloat (format->handle, key, value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to set float");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_format_ml_get_int (GstAmcFormat * format, const gchar * key,
    gint * value, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  result = MLMediaFormatGetKeyValueInt32 (format->handle, key, value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get int");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_format_ml_set_int (GstAmcFormat * format, const gchar * key, gint value,
    GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  result = MLMediaFormatSetKeyInt32 (format->handle, key, value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to set int");
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_amc_format_ml_get_string (GstAmcFormat * format, const gchar * key,
    gchar ** value, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  *value = g_new0 (gchar, MAX_KEY_STRING_SIZE);
  result = MLMediaFormatGetKeyString (format->handle, key, *value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get string");
    g_clear_pointer (value, g_free);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_format_ml_set_string (GstAmcFormat * format, const gchar * key,
    const gchar * value, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  result = MLMediaFormatSetKeyString (format->handle, key, value);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to set string");
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_amc_format_ml_get_buffer (GstAmcFormat * format, const gchar * key,
    guint8 ** data, gsize * size, GError ** err)
{
  MLResult result;
  MLMediaFormatByteArray buffer;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (size != NULL, FALSE);

  result = MLMediaFormatGetKeyByteBuffer (format->handle, key, &buffer);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get buffer");
    return FALSE;
  }

  *size = buffer.length;
  *data = (guint8 *) g_memdup2 (buffer.ptr, buffer.length);
  MLMediaFormatKeyByteBufferRelease (format->handle, &buffer);

  return TRUE;
}

static gboolean
gst_amc_format_ml_set_buffer (GstAmcFormat * format, const gchar * key,
    guint8 * data, gsize size, GError ** err)
{
  MLResult result;
  MLMediaFormatByteArray buffer;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  buffer.ptr = data;
  buffer.length = size;
  result = MLMediaFormatSetKeyByteBuffer (format->handle, key, &buffer);
  if (result != MLResult_Ok) {
    g_set_error_literal (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to set buffer");
    return FALSE;
  }
  return TRUE;
}

GstAmcFormatVTable gst_amc_format_ml_vtable = {
  .new_audio = gst_amc_format_ml_new_audio,
  .new_video = gst_amc_format_ml_new_video,
  .free = gst_amc_format_ml_free,

  .to_string = gst_amc_format_ml_to_string,

  .get_float = gst_amc_format_ml_get_float,
  .set_float = gst_amc_format_ml_set_float,
  .get_int = gst_amc_format_ml_get_int,
  .set_int = gst_amc_format_ml_set_int,
  .get_string = gst_amc_format_ml_get_string,
  .set_string = gst_amc_format_ml_set_string,
  .get_buffer = gst_amc_format_ml_get_buffer,
  .set_buffer = gst_amc_format_ml_set_buffer,
};
