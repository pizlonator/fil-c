/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_AMC_FORMAT_H__
#define __GST_AMC_FORMAT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstAmcFormatVTable GstAmcFormatVTable;
typedef struct _GstAmcFormat GstAmcFormat;
typedef struct _GstAmcColorFormatInfo GstAmcColorFormatInfo;

struct _GstAmcFormatVTable {
  GstAmcFormat * (* new_audio)  (const gchar *mime,
                                 gint sample_rate,
                                 gint channels,
                                 GError **err);

  GstAmcFormat * (* new_video)  (const gchar *mime,
                                 gint width,
                                 gint height,
                                 GError **err);

  void           (* free)       (GstAmcFormat * format);

  gchar *        (* to_string)  (GstAmcFormat * format,
                                 GError **err);

  gboolean       (* get_float)  (GstAmcFormat *format,
                                 const gchar *key,
                                 gfloat *value,
                                 GError **err);

  gboolean       (* set_float)  (GstAmcFormat *format,
                                 const gchar *key,
                                 gfloat value,
                                 GError **err);

  gboolean       (* get_int)    (GstAmcFormat *format,
                                 const gchar *key,
                                 gint *value,
                                 GError **err);

  gboolean       (* set_int)    (GstAmcFormat *format,
                                 const gchar *key,
                                 gint value,
                                 GError **err);

  gboolean       (* get_string) (GstAmcFormat *format,
                                 const gchar *key,
                                 gchar **value,
                                 GError **err);

  gboolean       (* set_string) (GstAmcFormat *format,
                                 const gchar *key,
                                 const gchar *value,
                                GError **err);

  gboolean       (* get_buffer) (GstAmcFormat *format,
                                 const gchar *key,
                                 guint8 **data,
                                 gsize *size,
                                 GError **err);

  gboolean       (* set_buffer) (GstAmcFormat *format,
                                 const gchar *key,
                                 guint8 *data,
                                 gsize size,
                                 GError **err);
};

extern GstAmcFormatVTable *gst_amc_format_vtable;

GstAmcFormat * gst_amc_format_new_audio (const gchar *mime, gint sample_rate, gint channels, GError **err);
GstAmcFormat * gst_amc_format_new_video (const gchar *mime, gint width, gint height, GError **err);
void gst_amc_format_free (GstAmcFormat * format);

gchar * gst_amc_format_to_string (GstAmcFormat * format, GError **err);

gboolean gst_amc_format_get_float (GstAmcFormat *format, const gchar *key, gfloat *value, GError **err);
gboolean gst_amc_format_set_float (GstAmcFormat *format, const gchar *key, gfloat value, GError **err);
gboolean gst_amc_format_get_int (GstAmcFormat *format, const gchar *key, gint *value, GError **err);
gboolean gst_amc_format_set_int (GstAmcFormat *format, const gchar *key, gint value, GError **err);
gboolean gst_amc_format_get_string (GstAmcFormat *format, const gchar *key, gchar **value, GError **err);
gboolean gst_amc_format_set_string (GstAmcFormat *format, const gchar *key, const gchar *value, GError **err);
gboolean gst_amc_format_get_buffer (GstAmcFormat *format, const gchar *key, guint8 **data, gsize *size, GError **err);
gboolean gst_amc_format_set_buffer (GstAmcFormat *format, const gchar *key, guint8 *data, gsize size, GError **err);

G_END_DECLS

#endif /* __GST_AMC_FORMAT_H__ */
