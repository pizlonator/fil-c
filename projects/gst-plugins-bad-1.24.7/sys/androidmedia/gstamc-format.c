/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2023 Ratchanan Srirattanamet <peathot@hotmail.com>
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

#include <gst/gst.h>
#include "gstamc-format.h"

GstAmcFormatVTable *gst_amc_format_vtable = NULL;

GstAmcFormat *
gst_amc_format_new_audio (const gchar * mime, gint sample_rate, gint channels,
    GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->new_audio (mime, sample_rate, channels, err);
}

GstAmcFormat *
gst_amc_format_new_video (const gchar * mime, gint width, gint height,
    GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->new_video (mime, width, height, err);
}

void
gst_amc_format_free (GstAmcFormat * format)
{
  g_assert (gst_amc_format_vtable != NULL);
  gst_amc_format_vtable->free (format);
}

gchar *
gst_amc_format_to_string (GstAmcFormat * format, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->to_string (format, err);
}

gboolean
gst_amc_format_get_float (GstAmcFormat * format, const gchar * key,
    gfloat * value, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->get_float (format, key, value, err);
}

gboolean
gst_amc_format_set_float (GstAmcFormat * format, const gchar * key,
    gfloat value, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->set_float (format, key, value, err);
}

gboolean
gst_amc_format_get_int (GstAmcFormat * format, const gchar * key, gint * value,
    GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->get_int (format, key, value, err);
}

gboolean
gst_amc_format_set_int (GstAmcFormat * format, const gchar * key, gint value,
    GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->set_int (format, key, value, err);
}

gboolean
gst_amc_format_get_string (GstAmcFormat * format, const gchar * key,
    gchar ** value, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->get_string (format, key, value, err);
}

gboolean
gst_amc_format_set_string (GstAmcFormat * format, const gchar * key,
    const gchar * value, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->set_string (format, key, value, err);
}

gboolean
gst_amc_format_get_buffer (GstAmcFormat * format, const gchar * key,
    guint8 ** data, gsize * size, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->get_buffer (format, key, data, size, err);
}

gboolean
gst_amc_format_set_buffer (GstAmcFormat * format, const gchar * key,
    guint8 * data, gsize size, GError ** err)
{
  g_assert (gst_amc_format_vtable != NULL);
  return gst_amc_format_vtable->set_buffer (format, key, data, size, err);
}
