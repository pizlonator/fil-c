/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowfakesink.cpp:
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

#include "gstdshowfakesink.h"

CDshowFakeSink::CDshowFakeSink ():
m_hres (S_OK),
m_callback (NULL),
m_data (NULL),
CBaseRenderer (CLSID_DshowFakeSink, TEXT("DshowFakeSink"), NULL, &m_hres)
{
}

STDMETHODIMP CDshowFakeSink::gst_set_media_type (AM_MEDIA_TYPE * pmt)
{
  m_MediaType.Set (*pmt);
  return S_OK;
}

STDMETHODIMP
    CDshowFakeSink::gst_set_buffer_callback (push_buffer_func push,
    gpointer data)
{
  m_callback = push;
  m_data = data;
  return S_OK;
}

HRESULT CDshowFakeSink::CheckMediaType (const CMediaType * pmt)
{
  if (!IsEqualGUID(pmt->majortype, m_MediaType.majortype) ||
      !IsEqualGUID(pmt->subtype, m_MediaType.subtype) ||
      !IsEqualGUID(pmt->formattype, m_MediaType.formattype) ||
      (pmt->cbFormat != m_MediaType.cbFormat))
    return S_FALSE;

  VIDEOINFOHEADER *info1 = (VIDEOINFOHEADER*)pmt->pbFormat;
  VIDEOINFOHEADER *info2 = (VIDEOINFOHEADER*)m_MediaType.pbFormat;

  if (memcmp(&info1->bmiHeader, &info2->bmiHeader, sizeof(BITMAPINFOHEADER)))
    return S_FALSE;

  return S_OK;
}

HRESULT CDshowFakeSink::DoRenderSample (IMediaSample * pMediaSample)
{
  if (pMediaSample && m_callback) {
    guint8 *pBuffer = NULL;
    pMediaSample->GetPointer (&pBuffer);

    guint size = pMediaSample->GetActualDataLength ();

    GstClockTimeDiff lStart = 0;
    GstClockTimeDiff lStop = 0;
    pMediaSample->GetTime (&lStart, &lStop);

    GstClockTime duration = (lStop - lStart) * 100;
    m_callback (pBuffer, size, m_data, duration);
  }

  return S_OK;
}
