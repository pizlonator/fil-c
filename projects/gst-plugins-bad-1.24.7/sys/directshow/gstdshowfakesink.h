/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowfakesink.h:
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

#ifndef __GST_DHOW_FAKESINK_H__
#define __GST_DHOW_FAKESINK_H__

#include "gstdshow.h"

//{6A780808-9725-4d0b-8695-A4DD8D210773}
static const GUID CLSID_DshowFakeSink =
    { 0x6a780808, 0x9725, 0x4d0b, {0x86, 0x95, 0xa4, 0xdd, 0x8d, 0x21, 0x7,
    0x73}
};

typedef bool (*push_buffer_func) (guint8 * buffer, guint size, gpointer src_object,
    GstClockTime duration);

class CDshowFakeSink:public CBaseRenderer
{
public:
  CDshowFakeSink ();
  virtual ~ CDshowFakeSink ()
  {
  }

  virtual HRESULT CheckMediaType (const CMediaType * pmt);
  virtual HRESULT DoRenderSample (IMediaSample * pMediaSample);

  STDMETHOD (gst_set_media_type) (AM_MEDIA_TYPE * pmt);
  STDMETHOD (gst_set_buffer_callback) (push_buffer_func push, gpointer data);

protected:
  HRESULT m_hres;
  CMediaType m_MediaType;
  push_buffer_func m_callback;
  gpointer m_data;
};

#endif /* __GST_DSHOW_FAKESINK_H__ */
