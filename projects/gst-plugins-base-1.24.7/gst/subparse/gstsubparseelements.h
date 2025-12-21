/* GStreamer
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GST_SUBPARSE_ELEMENT_H__
#define __GST_SUBPARSE_ELEMENT_H__

#include <gst/gst.h>

/* format enum */
typedef enum
{
  GST_SUB_PARSE_FORMAT_UNKNOWN = 0,
  GST_SUB_PARSE_FORMAT_MDVDSUB = 1,
  GST_SUB_PARSE_FORMAT_SUBRIP = 2,
  GST_SUB_PARSE_FORMAT_MPSUB = 3,
  GST_SUB_PARSE_FORMAT_SAMI = 4,
  GST_SUB_PARSE_FORMAT_TMPLAYER = 5,
  GST_SUB_PARSE_FORMAT_MPL2 = 6,
  GST_SUB_PARSE_FORMAT_SUBVIEWER = 7,
  GST_SUB_PARSE_FORMAT_DKS = 8,
  GST_SUB_PARSE_FORMAT_QTTEXT = 9,
  GST_SUB_PARSE_FORMAT_LRC = 10,
  GST_SUB_PARSE_FORMAT_VTT = 11
} GstSubParseFormat;


G_GNUC_INTERNAL GstSubParseFormat gst_sub_parse_data_format_autodetect (gchar * match_str);
G_GNUC_INTERNAL gchar * gst_sub_parse_detect_encoding (const gchar * str, gsize len);
G_GNUC_INTERNAL gchar * gst_sub_parse_gst_convert_to_utf8 (const gchar * str, gsize len, const gchar * encoding,
    gsize * consumed, GError ** err);
G_GNUC_INTERNAL gboolean sub_parse_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (subparse);
GST_ELEMENT_REGISTER_DECLARE (ssaparse);

GST_TYPE_FIND_REGISTER_DECLARE (subparse);

GST_DEBUG_CATEGORY_EXTERN (sub_parse_debug);
#define GST_CAT_DEFAULT sub_parse_debug

#endif /* __GST_SUBPARSE_ELEMENT_H__ */
