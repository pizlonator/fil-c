/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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

#include "gstdsdformat.h"

/**
 * gst_dsd_format_from_string:
 * @str: a DSD format string
 *
 * Convert the DSD format string @str to its #GstDsdFormat.
 *
 * Returns: the #GstDsdFormat for @format or GST_DSD_FORMAT_UNKNOWN when the
 * string is not a known format.
 *
 * Since: 1.24
 */
GstDsdFormat
gst_dsd_format_from_string (const gchar * str)
{
  if (g_strcmp0 (str, "DSDU8") == 0)
    return GST_DSD_FORMAT_U8;
  else if (g_strcmp0 (str, "DSDU16LE") == 0)
    return GST_DSD_FORMAT_U16LE;
  else if (g_strcmp0 (str, "DSDU16BE") == 0)
    return GST_DSD_FORMAT_U16BE;
  else if (g_strcmp0 (str, "DSDU32LE") == 0)
    return GST_DSD_FORMAT_U32LE;
  else if (g_strcmp0 (str, "DSDU32BE") == 0)
    return GST_DSD_FORMAT_U32BE;
  else
    return GST_DSD_FORMAT_UNKNOWN;
}

/**
 * gst_dsd_format_to_string:
 * @format: a #GstDsdFormat
 *
 * Returns a string containing a descriptive name for
 * the #GstDsdFormat if there is one, or NULL otherwise.
 *
 * Returns: the name corresponding to @format
 *
 * Since: 1.24
 */
const gchar *
gst_dsd_format_to_string (GstDsdFormat format)
{
  switch (format) {
    case GST_DSD_FORMAT_U8:
      return "DSDU8";
    case GST_DSD_FORMAT_U16LE:
      return "DSDU16LE";
    case GST_DSD_FORMAT_U16BE:
      return "DSDU16BE";
    case GST_DSD_FORMAT_U32LE:
      return "DSDU32LE";
    case GST_DSD_FORMAT_U32BE:
      return "DSDU32BE";
    default:
      return NULL;
  }
}

/**
 * gst_dsd_format_get_width:
 * @format: a #GstDsdFormat
 *
 * Returns: Number of bytes in this DSD grouping format.
 *
 * Since: 1.24
 */
guint
gst_dsd_format_get_width (GstDsdFormat format)
{
  switch (format) {
    case GST_DSD_FORMAT_U8:
      return 1;
    case GST_DSD_FORMAT_U16LE:
      return 2;
    case GST_DSD_FORMAT_U16BE:
      return 2;
    case GST_DSD_FORMAT_U32LE:
      return 4;
    case GST_DSD_FORMAT_U32BE:
      return 4;
    default:
      return 0;
  }
}
