/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2023, Ratchanan Srirattanamet <peathot@hotmail.com>
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

#include "../gstamc-format.h"
#include "gstamc-ndk.h"
#include "gstamc-internal-ndk.h"

#include <media/NdkMediaFormat.h>

#include <dlfcn.h>
#include <stdbool.h>

static struct
{
  void *mediandk_handle;

  AMediaFormat *(*new) ();
    media_status_t (*delete) (AMediaFormat *);

  const char *(*to_string) (AMediaFormat *);

  bool (*get_int32) (AMediaFormat *, const char *name, int32_t * out);
  bool (*get_float) (AMediaFormat *, const char *name, float *out);
  bool (*get_buffer) (AMediaFormat *, const char *name, void **data,
      size_t *size);
  bool (*get_string) (AMediaFormat *, const char *name, const char **out);

  void (*set_int32) (AMediaFormat *, const char *name, int32_t value);
  void (*set_float) (AMediaFormat *, const char *name, float value);
  void (*set_string) (AMediaFormat *, const char *name, const char *value);
  void (*set_buffer) (AMediaFormat *, const char *name, const void *data,
      size_t size);
} a_media_format;

gboolean
gst_amc_format_ndk_static_init (void)
{
  a_media_format.mediandk_handle = dlopen ("libmediandk.so", RTLD_NOW);

  if (!a_media_format.mediandk_handle)
    return FALSE;

  a_media_format.new =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_new");
  a_media_format.delete =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_delete");
  a_media_format.to_string =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_toString");
  a_media_format.get_int32 =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_getInt32");
  a_media_format.get_float =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_getFloat");
  a_media_format.get_buffer =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_getBuffer");
  a_media_format.get_string =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_getString");
  a_media_format.set_int32 =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_setInt32");
  a_media_format.set_float =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_setFloat");
  a_media_format.set_string =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_setString");
  a_media_format.set_buffer =
      dlsym (a_media_format.mediandk_handle, "AMediaFormat_setBuffer");

  if (!a_media_format.new || !a_media_format.delete || !a_media_format.to_string
      || !a_media_format.get_int32 || !a_media_format.get_float
      || !a_media_format.get_buffer || !a_media_format.get_string
      || !a_media_format.set_int32 || !a_media_format.set_float
      || !a_media_format.set_string || !a_media_format.set_buffer) {
    GST_WARNING ("Failed to get AMediaFormat functions");
    dlclose (a_media_format.mediandk_handle);
    a_media_format.mediandk_handle = NULL;
    return FALSE;
  }

  return TRUE;
}

GstAmcFormat *
gst_amc_format_ndk_new (GError ** err)
{
  GstAmcFormat *ret = g_new0 (GstAmcFormat, 1);

  ret->ndk_media_format = a_media_format.new ();

  if (!ret->ndk_media_format) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to allocate AMediaFormat");

    g_free (ret);
    return NULL;
  }

  return ret;
}

#define MEDIAFORMAT_KEY_MIME "mime"
#define MEDIAFORMAT_SAMPLE_RATE "sample-rate"
#define MEDIAFORMAT_CHANNEL_COUNT "channel-count"

static GstAmcFormat *
gst_amc_format_ndk_new_audio (const gchar * mime, gint sample_rate,
    gint channels, GError ** err)
{
  GstAmcFormat *ret = gst_amc_format_ndk_new (err);

  if (!ret)
    return ret;

  a_media_format.set_string (ret->ndk_media_format, MEDIAFORMAT_KEY_MIME, mime);
  a_media_format.set_int32 (ret->ndk_media_format, MEDIAFORMAT_SAMPLE_RATE,
      sample_rate);
  a_media_format.set_int32 (ret->ndk_media_format, MEDIAFORMAT_CHANNEL_COUNT,
      channels);

  return ret;
}

#define MEDIAFORMAT_KEY_WIDTH "width"
#define MEDIAFORMAT_KEY_HEIGHT "height"

static GstAmcFormat *
gst_amc_format_ndk_new_video (const gchar * mime, gint width, gint height,
    GError ** err)
{
  GstAmcFormat *ret = gst_amc_format_ndk_new (err);

  if (!ret)
    return ret;

  a_media_format.set_string (ret->ndk_media_format, MEDIAFORMAT_KEY_MIME, mime);
  a_media_format.set_int32 (ret->ndk_media_format, MEDIAFORMAT_KEY_WIDTH,
      width);
  a_media_format.set_int32 (ret->ndk_media_format, MEDIAFORMAT_KEY_HEIGHT,
      height);

  return ret;
}

GstAmcFormat *
gst_amc_format_ndk_from_a_media_format (AMediaFormat * ndk_media_format)
{
  GstAmcFormat *ret = g_new0 (GstAmcFormat, 1);
  ret->ndk_media_format = ndk_media_format;
  return ret;
}

static void
gst_amc_format_ndk_free (GstAmcFormat * format)
{
  a_media_format.delete (format->ndk_media_format);
  g_free (format);
}

static gchar *
gst_amc_format_ndk_to_string (GstAmcFormat * format, GError ** err)
{
  const char *str = a_media_format.to_string (format->ndk_media_format);

  if (!str) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to call AMediaFormat_toString()");
    return NULL;
  }

  return g_strdup (str);
}

static gboolean
gst_amc_format_ndk_get_float (GstAmcFormat * format, const gchar * key,
    gfloat * value, GError ** err)
{
  float val;

  if (!a_media_format.get_float (format->ndk_media_format, key, &val)) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to call AMediaFormat_getFloat()");

    return FALSE;
  }

  *value = val;
  return TRUE;
}

static gboolean
gst_amc_format_ndk_set_float (GstAmcFormat * format, const gchar * key,
    gfloat value, GError ** err)
{
  a_media_format.set_float (format->ndk_media_format, key, value);
  return TRUE;
}

static gboolean
gst_amc_format_ndk_get_int (GstAmcFormat * format, const gchar * key,
    gint * value, GError ** err)
{
  int32_t val;

  if (!a_media_format.get_int32 (format->ndk_media_format, key, &val)) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to call AMediaFormat_getInt32()");

    return FALSE;
  }

  *value = val;
  return TRUE;
}

static gboolean
gst_amc_format_ndk_set_int (GstAmcFormat * format, const gchar * key,
    gint value, GError ** err)
{
  a_media_format.set_int32 (format->ndk_media_format, key, value);
  return TRUE;
}

static gboolean
gst_amc_format_ndk_get_string (GstAmcFormat * format, const gchar * key,
    gchar ** value, GError ** err)
{
  const char *val;

  if (!a_media_format.get_string (format->ndk_media_format, key, &val)) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to call AMediaFormat_getString()");

    return FALSE;
  }

  *value = g_strdup (val);
  return TRUE;
}

static gboolean
gst_amc_format_ndk_set_string (GstAmcFormat * format, const gchar * key,
    const gchar * value, GError ** err)
{
  a_media_format.set_string (format->ndk_media_format, key, value);
  return TRUE;
}

static gboolean
gst_amc_format_ndk_get_buffer (GstAmcFormat * format, const gchar * key,
    guint8 ** out_data, gsize * out_size, GError ** err)
{
  void *data;
  size_t size;

  if (!a_media_format.get_buffer (format->ndk_media_format, key, &data, &size)) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to call AMediaFormat_getBuffer()");

    return FALSE;
  }

  *out_size = size;
  *out_data = g_memdup2 (data, size);
  return TRUE;
}

static gboolean
gst_amc_format_ndk_set_buffer (GstAmcFormat * format, const gchar * key,
    guint8 * data, gsize size, GError ** err)
{
  a_media_format.set_buffer (format->ndk_media_format, key, data, size);
  return TRUE;
}

GstAmcFormatVTable gst_amc_format_ndk_vtable = {
  .new_audio = gst_amc_format_ndk_new_audio,
  .new_video = gst_amc_format_ndk_new_video,
  .free = gst_amc_format_ndk_free,

  .to_string = gst_amc_format_ndk_to_string,

  .get_float = gst_amc_format_ndk_get_float,
  .set_float = gst_amc_format_ndk_set_float,
  .get_int = gst_amc_format_ndk_get_int,
  .set_int = gst_amc_format_ndk_set_int,
  .get_string = gst_amc_format_ndk_get_string,
  .set_string = gst_amc_format_ndk_set_string,
  .get_buffer = gst_amc_format_ndk_get_buffer,
  .set_buffer = gst_amc_format_ndk_set_buffer,
};
