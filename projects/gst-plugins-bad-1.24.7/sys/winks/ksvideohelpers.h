/*
 * Copyright (C) 2007 Haakon Sporsheim <hakon.sporsheim@tandberg.com>
 *               2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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

#ifndef __KSVIDEOHELPERS_H__
#define __KSVIDEOHELPERS_H__

#include <gst/gst.h>
#include <windows.h>
#include <ks.h>
#include <ksmedia.h>

G_BEGIN_DECLS

DEFINE_GUID(MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

typedef struct _KsVideoMediaType KsVideoMediaType;

/**
 * A structure that contain metadata about capabilities
 * for both KS and GStreamer for video only.
 */
struct _KsVideoMediaType
{
  guint pin_id;

  const KSDATARANGE * range;
  const KS_VIDEO_STREAM_CONFIG_CAPS vscc;

  guint8 * format;
  guint format_size;

  guint sample_size;

  GstCaps * translated_caps;
  gboolean is_rgb;
};

typedef struct DVINFO {
  DWORD dwDVAAuxSrc;
  DWORD dwDVAAuxCtl;
  DWORD dwDVAAuxSrc1;
  DWORD dwDVAAuxCtl1;
  DWORD dwDVVAuxSrc;
  DWORD dwDVVAuxCtl;
  DWORD dwDVReserved[2];
} DVINFO;

typedef struct KS_DATARANGE_DVVIDEO {
  KSDATARANGE DataRange;
  DVINFO DVVideoInfo;
} KS_DATARANGE_DVVIDEO,*PKS_DATARANGE_DVVIDEO;


GList * ks_video_device_list_sort_cameras_first (GList * devices);

KsVideoMediaType * ks_video_media_type_dup (KsVideoMediaType * media_type);
void ks_video_media_type_free (KsVideoMediaType * media_type);
GList * ks_video_probe_filter_for_caps (HANDLE filter_handle);
KSPIN_CONNECT * ks_video_create_pin_conn_from_media_type (KsVideoMediaType * media_type);
gboolean ks_video_fixate_media_type (const KSDATARANGE * range, guint8 * format, gint width, gint height, gint fps_n, gint fps_d);

GstCaps * ks_video_get_all_caps (void);

G_END_DECLS

#endif /* __KSVIDEOHELPERS_H__ */
