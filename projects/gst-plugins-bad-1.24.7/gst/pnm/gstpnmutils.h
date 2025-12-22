/* GStreamer PNM utility functions
 * Copyright (C) 2009 Lutz Mueller <lutz@users.sourceforge.net>
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

#ifndef __GST_PNM_UTILS_H__
#define __GST_PNM_UTILS_H__

#include <glib.h>
#include <glib-object.h>

#define MIME_BM "image/x-portable-bitmap"
#define MIME_GM "image/x-portable-graymap"
#define MIME_PM "image/x-portable-pixmap"
#define MIME_AM "image/x-portable-anymap"
#define MIME_ALL MIME_BM "; " MIME_GM "; " MIME_PM "; " MIME_AM

typedef enum
{
  GST_PNM_INFO_FIELDS_TYPE = 1 << 0,
  GST_PNM_INFO_FIELDS_WIDTH = 1 << 1,
  GST_PNM_INFO_FIELDS_HEIGHT = 1 << 2,
  GST_PNM_INFO_FIELDS_MAX = 1 << 3,
  GST_PNM_INFO_FIELDS_ENCODING = 1 << 4
} GstPnmInfoFields;

#define GST_PNM_INFO_FIELDS_ALL (GST_PNM_INFO_FIELDS_TYPE | GST_PNM_INFO_FIELDS_WIDTH | GST_PNM_INFO_FIELDS_HEIGHT | GST_PNM_INFO_FIELDS_MAX | GST_PNM_INFO_FIELDS_ENCODING)

typedef enum
{
  GST_PNM_TYPE_BITMAP = 1,
  GST_PNM_TYPE_GRAYMAP = 2,
  GST_PNM_TYPE_PIXMAP = 3
} GstPnmType;

typedef enum
{
  GST_PNM_ENCODING_RAW = 0,
  GST_PNM_ENCODING_ASCII = 1
} GstPnmEncoding;

typedef struct
{
  GstPnmInfoFields fields;
  GstPnmType type;
  GstPnmEncoding encoding;
  guint width, height, max;
} GstPnmInfo;

typedef enum
{
  GST_PNM_INFO_MNGR_STATE_NONE = 0,
  GST_PNM_INFO_MNGR_STATE_DATA_TYPE,
  GST_PNM_INFO_MNGR_STATE_DATA_WIDTH,
  GST_PNM_INFO_MNGR_STATE_DATA_HEIGHT,
  GST_PNM_INFO_MNGR_STATE_DATA_MAX,
  GST_PNM_INFO_MNGR_STATE_COMMENT,
  GST_PNM_INFO_MNGR_STATE_WHITE_SPACE
} GstPnmInfoMngrState;

typedef struct
{
  GstPnmInfoMngrState state;
  GstPnmInfo info;
  guint8 data_offset;
} GstPnmInfoMngr;

typedef enum
{
  GST_PNM_INFO_MNGR_RESULT_FAILED,
  GST_PNM_INFO_MNGR_RESULT_READING,
  GST_PNM_INFO_MNGR_RESULT_FINISHED
} GstPnmInfoMngrResult;

GstPnmInfoMngrResult gst_pnm_info_mngr_scan (GstPnmInfoMngr *, const guint8 *, guint);

#endif /* __GST_PNM_UTILS_H__ */
