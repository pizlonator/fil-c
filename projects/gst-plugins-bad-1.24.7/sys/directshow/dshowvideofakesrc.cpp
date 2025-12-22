/* GStreamer
 * Copyright (C) 2008 Pioneers of the Inevitable <songbird@songbirdnest.com>
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

#include "dshowvideofakesrc.h"

GST_DEBUG_CATEGORY_EXTERN (dshowvideosink_debug);
#define GST_CAT_DEFAULT dshowvideosink_debug

// {A0A5CF33-BD0C-4158-9A56-3011DEE3AF6B}
const GUID CLSID_VideoFakeSrc =
{ 0xa0a5cf33, 0xbd0c, 0x4158, { 0x9a, 0x56, 0x30, 0x11, 0xde, 0xe3, 0xaf, 0x6b } };

/* output pin*/
VideoFakeSrcPin::VideoFakeSrcPin (CBaseFilter *pFilter, CCritSec *sec, HRESULT *hres):
  CDynamicOutputPin("VideoFakeSrcPin", pFilter, sec, hres, L"output")
{
}

VideoFakeSrcPin::~VideoFakeSrcPin()
{
}

HRESULT VideoFakeSrcPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  GST_DEBUG ("GetMediaType(%d) called", iPosition);
  if(iPosition == 0) {
    *pMediaType = m_MediaType;
    return S_OK;
  }

  return VFW_S_NO_MORE_ITEMS;
}

/* This seems to be called to notify us of the actual media type being used,
 * even though SetMediaType isn't called. How bizarre! */
HRESULT VideoFakeSrcPin::CheckMediaType(const CMediaType *pmt)
{
    GST_DEBUG ("CheckMediaType called: %p", pmt);

    /* The video renderer will request a different stride, which we must accept.
     * So, we accept arbitrary strides (and do memcpy() to convert if needed),
     * and require the rest of the media type to match
     */
    if (IsEqualGUID(pmt->majortype,m_MediaType.majortype) &&
        IsEqualGUID(pmt->subtype,m_MediaType.subtype) &&
        IsEqualGUID(pmt->formattype,m_MediaType.formattype) &&
        pmt->cbFormat >= m_MediaType.cbFormat)
    {
      if (IsEqualGUID(pmt->formattype, FORMAT_VideoInfo)) {
        VIDEOINFOHEADER *newvh = (VIDEOINFOHEADER *)pmt->pbFormat;
        VIDEOINFOHEADER *curvh = (VIDEOINFOHEADER *)m_MediaType.pbFormat;

        if ((memcmp ((void *)&newvh->rcSource, (void *)&curvh->rcSource, sizeof (RECT)) == 0) &&
            (memcmp ((void *)&newvh->rcTarget, (void *)&curvh->rcTarget, sizeof (RECT)) == 0) &&
            (newvh->bmiHeader.biCompression == curvh->bmiHeader.biCompression) &&
            (newvh->bmiHeader.biHeight == curvh->bmiHeader.biHeight) &&
            (newvh->bmiHeader.biWidth >= curvh->bmiHeader.biWidth))
        {
          GST_DEBUG ("CheckMediaType has same media type, width %d (%d image)", newvh->bmiHeader.biWidth, curvh->bmiHeader.biWidth);

          /* OK, compatible! */
          return S_OK;
        }
        else {
          GST_WARNING ("Looked similar, but aren't...");
        }
      }

    }
    GST_WARNING ("Different media types, FAILING!");
    return S_FALSE;
}

HRESULT VideoFakeSrcPin::DecideBufferSize (IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest)
{
  ALLOCATOR_PROPERTIES properties;
  GST_DEBUG ("Required allocator properties: %d, %d, %d, %d",
        ppropInputRequest->cbAlign, ppropInputRequest->cbBuffer,
        ppropInputRequest->cbPrefix, ppropInputRequest->cBuffers);

  ppropInputRequest->cbBuffer = m_SampleSize;
  ppropInputRequest->cBuffers = 1;

  /* First set the buffer descriptions we're interested in */
  HRESULT hres = pAlloc->SetProperties(ppropInputRequest, &properties);
  GST_DEBUG ("Actual Allocator properties: %d, %d, %d, %d",
        properties.cbAlign, properties.cbBuffer,
        properties.cbPrefix, properties.cBuffers);

  return S_OK;
}

STDMETHODIMP
VideoFakeSrcPin::Notify(IBaseFilter * pSender, Quality q)
{
  /* Implementing this usefully is not required, but the base class
   * has an assertion here... */
  /* TODO: Map this to GStreamer QOS events? */
  return E_NOTIMPL;
}

STDMETHODIMP VideoFakeSrcPin::SetMediaType (AM_MEDIA_TYPE *pmt)
{
  m_MediaType.Set (*pmt);
  m_SampleSize = m_MediaType.GetSampleSize();

  GST_DEBUG ("SetMediaType called. SampleSize is %d", m_SampleSize);

  return S_OK;
}

/* If the destination buffer is a different shape (strides, etc.) from the source
 * buffer, we have to copy. Do that here, for supported video formats.
 *
 * TODO: When possible (when these things DON'T differ), we should buffer-alloc the
 * final output buffer, and not do this copy */
STDMETHODIMP VideoFakeSrcPin::CopyToDestinationBuffer (byte *srcbuf, byte *dstbuf)
{
  VIDEOINFOHEADER *vh = (VIDEOINFOHEADER *)m_MediaType.pbFormat;
  GST_DEBUG ("Rendering a frame");

  byte *src, *dst;
  int dststride, srcstride, rows;
  guint32 fourcc = vh->bmiHeader.biCompression;

  /* biHeight is always negative; we don't want that. */
  int height = ABS (vh->bmiHeader.biHeight);
  int width = vh->bmiHeader.biWidth;

  /* YUY2 is the preferred layout for DirectShow, so we will probably get this
   * most of the time */
  if ((fourcc == GST_MAKE_FOURCC ('Y', 'U', 'Y', '2')) ||
      (fourcc == GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V')) ||
      (fourcc == GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y')))
  {
    /* Nice and simple */
    int srcstride = GST_ROUND_UP_4 (vh->rcSource.right * 2);
    int dststride = width * 2;

    for (int i = 0; i < height; i++) {
      memcpy (dstbuf + dststride * i, srcbuf + srcstride * i, srcstride);
    }
  }
  else if (fourcc == GST_MAKE_FOURCC ('Y', 'V', '1', '2')) {
    for (int component = 0; component < 3; component++) {
      // TODO: Get format properly rather than hard-coding it. Use gst_video_* APIs *?
      if (component == 0) {
        srcstride = GST_ROUND_UP_4 (vh->rcSource.right);
        src = srcbuf;
      }
      else {
        srcstride = GST_ROUND_UP_4 ( GST_ROUND_UP_2 (vh->rcSource.right) / 2);
        if (component == 1)
          src = srcbuf + GST_ROUND_UP_4 (vh->rcSource.right) * GST_ROUND_UP_2 (vh->rcSource.bottom);
        else
          src = srcbuf + GST_ROUND_UP_4 (vh->rcSource.right) * GST_ROUND_UP_2 (vh->rcSource.bottom) +
                  srcstride * (GST_ROUND_UP_2 (vh->rcSource.bottom) / 2);
      }

      /* Is there a better way to do this? This is ICK! */
      if (component == 0) {
        dststride = width;
        dst = dstbuf;
        rows = height;
      } else if (component == 1) {
        dststride = width / 2;
        dst = dstbuf + width * height;
        rows = height/2;
      }
      else {
        dststride = width / 2;
        dst = dstbuf + width * height +
                       width/2 * height/2;
        rows = height/2;
      }

      for (int i = 0; i < rows; i++) {
        memcpy (dst + i * dststride, src + i * srcstride, srcstride);
      }
    }
  }

  return S_OK;
}

STDMETHODIMP VideoFakeSrcPin::Disconnect ()
{
  GST_DEBUG_OBJECT (this, "Disconnecting pin");
  HRESULT hr = CDynamicOutputPin::Disconnect();
  GST_DEBUG_OBJECT (this, "Pin disconnected");
  return hr;
}

HRESULT VideoFakeSrcPin::Inactive ()
{
  GST_DEBUG_OBJECT (this, "Pin going inactive");
  HRESULT hr = CDynamicOutputPin::Inactive();
  GST_DEBUG_OBJECT (this, "Pin inactivated");
  return hr;
}

HRESULT VideoFakeSrcPin::BreakConnect ()
{
  GST_DEBUG_OBJECT (this, "Breaking connection");
  HRESULT hr = CDynamicOutputPin::BreakConnect();
  GST_DEBUG_OBJECT (this, "Connection broken");
  return hr;
}

HRESULT VideoFakeSrcPin::CompleteConnect (IPin *pReceivePin)
{
  GST_DEBUG_OBJECT (this, "Completing connection");
  HRESULT hr = CDynamicOutputPin::CompleteConnect(pReceivePin);
  GST_DEBUG_OBJECT (this, "Completed connection: %x", hr);
  return hr;
}

STDMETHODIMP VideoFakeSrcPin::Block(DWORD dwBlockFlags, HANDLE hEvent)
{
  GST_DEBUG_OBJECT (this, "Calling Block()");
  HRESULT hr = CDynamicOutputPin::Block (dwBlockFlags, hEvent);
  GST_DEBUG_OBJECT (this, "Called Block()");
  return hr;
}

/* When moving the video to a different monitor, directshow stops and restarts the playback pipeline.
 * Unfortunately, it doesn't properly block pins or do anything special, so we racily just fail
 * at this point.
 * So, we try multiple times in a loop, hoping that it'll have finished (we get no notifications at all!)
 * at some point.
 */
#define MAX_ATTEMPTS 10

GstFlowReturn VideoFakeSrcPin::PushBuffer(GstBuffer *buffer)
{
  IMediaSample *pSample = NULL;
  byte *data;
  GstMapInfo map;
  int attempts = 0;
  HRESULT hres;
  BYTE *sample_buffer;
  AM_MEDIA_TYPE *mediatype;

  /* FIXME: check return value. */
  gst_buffer_map (buffer, &map, GST_MAP_READ);
  data = map.data;
  StartUsingOutputPin();

  while (attempts < MAX_ATTEMPTS)
  {
    hres = GetDeliveryBuffer(&pSample, NULL, NULL, 0);
    if (SUCCEEDED (hres))
      break;
    attempts++;
    Sleep(100);
  }

  if (FAILED (hres))
  {
    StopUsingOutputPin();
    GST_WARNING ("Could not get sample for delivery to sink: %x", hres);
    return GST_FLOW_ERROR;
  }

  pSample->GetPointer(&sample_buffer);
  pSample->GetMediaType(&mediatype);
  if (mediatype)
    SetMediaType (mediatype);

  if(sample_buffer)
  {
    /* Copy to the destination stride.
     * This is not just a simple memcpy because of the different strides.
     * TODO: optimise for the same-stride case and avoid the copy entirely.
     */
    CopyToDestinationBuffer (data, sample_buffer);
  }
  gst_buffer_unmap (buffer, &map);

  pSample->SetDiscontinuity(FALSE); /* Decoded frame; unimportant */
  pSample->SetSyncPoint(TRUE); /* Decoded frame; always a valid syncpoint */
  pSample->SetPreroll(FALSE); /* For non-displayed frames.
                                 Not used in GStreamer */

  /* Disable synchronising on this sample. We instead let GStreamer handle
   * this at a higher level, inside BaseSink. */
  pSample->SetTime(NULL, NULL);

  while (attempts < MAX_ATTEMPTS)
  {
    hres = Deliver(pSample);
    if (SUCCEEDED (hres))
      break;
    attempts++;
    Sleep(100);
  }

  pSample->Release();

  StopUsingOutputPin();

  if (SUCCEEDED (hres))
    return GST_FLOW_OK;
  else {
    GST_WARNING_OBJECT (this, "Failed to deliver sample: %x", hres);
    if (hres == VFW_E_NOT_CONNECTED)
      return GST_FLOW_NOT_LINKED;
    else
      return GST_FLOW_ERROR;
  }
}

STDMETHODIMP VideoFakeSrcPin::Flush ()
{
  DeliverBeginFlush();
  DeliverEndFlush();
  return S_OK;
}

VideoFakeSrc::VideoFakeSrc() : CBaseFilter("VideoFakeSrc", NULL, &m_critsec, CLSID_VideoFakeSrc),
	m_evFilterStoppingEvent(TRUE)
{
  HRESULT hr = S_OK;
  m_pOutputPin = new VideoFakeSrcPin ((CSource *)this, &m_critsec, &hr);
}

int VideoFakeSrc::GetPinCount()
{
  return 1;
}

CBasePin *VideoFakeSrc::GetPin(int n)
{
  return (CBasePin *)m_pOutputPin;
}

VideoFakeSrcPin *VideoFakeSrc::GetOutputPin()
{
  return m_pOutputPin;
}

STDMETHODIMP VideoFakeSrc::Stop(void)
{
  GST_DEBUG_OBJECT (this, "Stop()");
  m_evFilterStoppingEvent.Set();

  return CBaseFilter::Stop();
}

STDMETHODIMP VideoFakeSrc::Pause(void)
{
  GST_DEBUG_OBJECT (this, "Pause()");

  m_evFilterStoppingEvent.Reset();

  return CBaseFilter::Pause();
}

STDMETHODIMP VideoFakeSrc::Run(REFERENCE_TIME tStart)
{
  GST_DEBUG_OBJECT (this, "Run()");

  return CBaseFilter::Run(tStart);
}

STDMETHODIMP VideoFakeSrc::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
  HRESULT hr;

  // The filter is joining the filter graph.
  if(NULL != pGraph)
  {
    IGraphConfig* pGraphConfig = NULL;
    hr = pGraph->QueryInterface(IID_IGraphConfig, (void**)&pGraphConfig);
    if(FAILED(hr))
      return hr;

    hr = CBaseFilter::JoinFilterGraph(pGraph, pName);
    if(FAILED(hr))
    {
      pGraphConfig->Release();
      return hr;
    }

    m_pOutputPin->SetConfigInfo(pGraphConfig, m_evFilterStoppingEvent);
    pGraphConfig->Release();
  }
  else
  {
    hr = CBaseFilter::JoinFilterGraph(pGraph, pName);
    if(FAILED(hr))
      return hr;

    m_pOutputPin->SetConfigInfo(NULL, NULL);
  }

  return S_OK;
}

