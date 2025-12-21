/* GStreamer
 * Copyright (C) 2021 Fluendo S.A. <support@fluendo.com>
 *   Authors: Andoni Morales Alastruey <amorales@fluendo.com>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <gst/base/gstbytereader.h>
#include "video-sei.h"

/**
 * SECTION:gstvideosei
 * @title: GstVideo SEI Unregistered User Data
 * @short_description: Utilities for SEI User Data Unregistered
 *
 * A collection of objects and methods to assist with SEI User Data Unregistered
 * metadata in H.264 and H.265 streams.
 *
 * Since: 1.22
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-sei", 0,
        "H.264 / H.265 SEI messages utilities");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

/* SEI User Data Unregistered implementation */

/**
 * gst_video_sei_user_data_unregistered_meta_api_get_type:
 *
 * Returns: #GType for the #GstVideoSEIUserDataUnregisteredMeta structure.
 *
 * Since: 1.22
 */
GType
gst_video_sei_user_data_unregistered_meta_api_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    static const gchar *tags[] = {
      GST_META_TAG_VIDEO_STR,
      NULL
    };
    GType _type =
        gst_meta_api_type_register ("GstVideoSEIUserDataUnregisteredMetaAPI",
        tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_sei_user_data_unregistered_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoSEIUserDataUnregisteredMeta *emeta =
      (GstVideoSEIUserDataUnregisteredMeta *) meta;

  emeta->data = NULL;
  emeta->size = 0;

  return TRUE;
}

static gboolean
gst_video_sei_user_data_unregistered_meta_transform (GstBuffer * dest,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoSEIUserDataUnregisteredMeta *smeta =
      (GstVideoSEIUserDataUnregisteredMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GST_DEBUG ("copy SEI User Data Unregistered metadata");
    gst_buffer_add_video_sei_user_data_unregistered_meta (dest,
        smeta->uuid, smeta->data, smeta->size);
    return TRUE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
}

static void
gst_video_sei_user_data_unregistered_meta_free (GstMeta * meta, GstBuffer * buf)
{
  GstVideoSEIUserDataUnregisteredMeta *smeta =
      (GstVideoSEIUserDataUnregisteredMeta *) meta;

  g_free (smeta->data);
  smeta->data = NULL;
}

/**
 * gst_video_sei_user_data_unregistered_meta_get_info:
 *
 * Returns: #GstMetaInfo pointer that describes #GstVideoSEIUserDataUnregisteredMeta.
 *
 * Since: 1.22
 */
const GstMetaInfo *
gst_video_sei_user_data_unregistered_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi =
        gst_meta_register (GST_VIDEO_SEI_USER_DATA_UNREGISTERED_META_API_TYPE,
        "GstVideoSEIUserDataUnregisteredMeta",
        sizeof (GstVideoSEIUserDataUnregisteredMeta),
        gst_video_sei_user_data_unregistered_meta_init,
        gst_video_sei_user_data_unregistered_meta_free,
        gst_video_sei_user_data_unregistered_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_video_sei_user_data_unregistered_meta:
 * @buffer: a #GstBuffer
 * @uuid: User Data Unregistered UUID
 * @data: (transfer none): SEI User Data Unregistered buffer
 * @size: size of the data buffer
 *
 * Attaches #GstVideoSEIUserDataUnregisteredMeta metadata to @buffer with the given
 * parameters.
 *
 * Returns: (transfer none): the #GstVideoSEIUserDataUnregisteredMeta on @buffer.
 *
 * Since: 1.22
 */
GstVideoSEIUserDataUnregisteredMeta *
gst_buffer_add_video_sei_user_data_unregistered_meta (GstBuffer * buffer,
    guint8 uuid[16], guint8 * data, gsize size)
{
  GstVideoSEIUserDataUnregisteredMeta *meta;
  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (data != NULL, NULL);

  meta = (GstVideoSEIUserDataUnregisteredMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_SEI_USER_DATA_UNREGISTERED_META_INFO, NULL);
  g_assert (meta != NULL);
  memcpy (meta->uuid, uuid, 16);
  meta->data = g_malloc (size);
  memcpy (meta->data, data, size);
  meta->size = size;

  return meta;
}

/**
 * gst_video_sei_user_data_unregistered_parse_precision_time_stamp:
 * @user_data: (transfer none): a #GstVideoSEIUserDataUnregisteredMeta
 * @status: (out): User Data Unregistered UUID
 * @precision_time_stamp: (out): The parsed Precision Time Stamp SEI
 *
 * Parses and returns the Precision Time Stamp (ST 0604) from the SEI User Data Unregistered buffer
 *
 * Returns: True if data is a Precision Time Stamp and it was parsed correctly
 *
 * Since: 1.22
 */
gboolean
    gst_video_sei_user_data_unregistered_parse_precision_time_stamp
    (GstVideoSEIUserDataUnregisteredMeta * user_data, guint8 * status,
    guint64 * precision_time_stamp) {
  guint8 *data = user_data->data;

  if (memcmp (user_data->uuid, &H264_MISP_MICROSECTIME, 16) != 0 &&
      memcmp (user_data->uuid, &H265_MISP_MICROSECONDS, 16) != 0 &&
      memcmp (user_data->uuid, &H265_MISP_NANOSECONDS, 16) != 0) {
    GST_WARNING
        ("User Data Unregistered UUID is not a known MISP Timestamp UUID");
    return FALSE;
  }

  if (user_data->size < 12) {
    GST_WARNING ("MISP Precision Time Stamp data size is too short, ignoring");
    return FALSE;
  }

  /* Status */
  *status = data[0];

  *precision_time_stamp =
      /* Two MS bytes of Time Stamp (microseconds) */
      _GST_GET (data, 1, 64, 56) | _GST_GET (data, 2, 64, 48) |
      /* Start Code Emulation Prevention Byte (0xFF) */
      /* Two next MS bytes of Time Stamp (microseconds) */
      _GST_GET (data, 4, 64, 40) | _GST_GET (data, 5, 64, 32) |
      /* Start Code Emulation Prevention Byte (0xFF) */
      /* Two LS bytes of Time Stamp (microseconds) */
      _GST_GET (data, 7, 64, 24) | _GST_GET (data, 8, 64, 16) |
      /* Start Code Emulation Prevention Byte (0xFF) */
      /* Two next LS bytes of Time Stamp (microseconds) */
      _GST_GET (data, 10, 64, 8) | _GST_GET (data, 11, 64, 0);

  return TRUE;
}
