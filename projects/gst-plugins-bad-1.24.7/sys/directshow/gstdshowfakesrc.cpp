/* GStreamer
 * Copyright <2006, 2007, 2008, 2009, 2010> Fluendo <support@fluendo.com>
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowfakesrc.cpp:
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

#include "gstdshowfakesrc.h"

GST_DEBUG_CATEGORY_EXTERN (dshowdec_debug);
#define GST_CAT_DEFAULT dshowdec_debug

const GUID CLSID_DecodeFakeSrc =
{ 0x039527db, 0x6b48, 0x45a7, { 0xab, 0xcf, 0x21, 0xab, 0xc5, 0x44, 0xbb, 0xb6} };

static CCritSec g_pCriticSec;

/* output pin*/
FakeOutputPin::FakeOutputPin (CBaseFilter *pFilter, CCritSec *sec):
  CBaseOutputPin("FakeOutputPin", pFilter, sec, &m_hres, L"output")
{
}

FakeOutputPin::~FakeOutputPin()
{
}

HRESULT FakeOutputPin::GetMediaType(int iPosition,
                                    CMediaType *pMediaType)
{
  if(iPosition == 0) {
    *pMediaType = m_MediaType;
    return S_OK;
  }

  return VFW_S_NO_MORE_ITEMS;
}
#if 0
#define GUID_FORMAT "0x%.8x 0x%.4x 0x%.4x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x"
#define GUID_ARGS(g) g.Data1, g.Data2, g.Data3, \
  g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], \
  g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]

static void printMediaType (AM_MEDIA_TYPE *mt)
{
  GST_DEBUG (":: majortype: "GUID_FORMAT, GUID_ARGS(mt->majortype));
  GST_DEBUG (":: subtype: "GUID_FORMAT, GUID_ARGS(mt->subtype));

  GST_DEBUG (":: bFixedSizeSamples: %d", mt->bFixedSizeSamples);
  GST_DEBUG (":: bTemporalCompression: %d", mt->bTemporalCompression);
  GST_DEBUG (":: cbFormat: %d", mt->cbFormat);
  GST_DEBUG (":: formattype: %x", mt->formattype);
  GST_DEBUG (":: lSampleSize: %lu", mt->lSampleSize);
  GST_DEBUG (":: pbFormat: %p", mt->pbFormat);
}
#endif

HRESULT FakeOutputPin::CheckMediaType(const CMediaType *pmt)
{
  if (m_MediaType == *pmt) {
    return S_OK;
  }

  return S_FALSE;
}

HRESULT FakeOutputPin::DecideBufferSize (IMemAllocator *pAlloc,
                                         ALLOCATOR_PROPERTIES *ppropInputRequest)
{
  ALLOCATOR_PROPERTIES properties;
  ppropInputRequest->cbBuffer = m_SampleSize;
  ppropInputRequest->cBuffers = 1;
  HRESULT hres = pAlloc->SetProperties(ppropInputRequest, &properties);
  pAlloc->Commit();

  return S_OK;
}

STDMETHODIMP FakeOutputPin::SetMediaType (AM_MEDIA_TYPE *pmt)
{
  m_MediaType.Set (*pmt);
  m_SampleSize = m_MediaType.GetSampleSize();
  return S_OK;
}

STDMETHODIMP FakeOutputPin::PushBuffer(byte *buffer,
                                       __int64 start, __int64 stop,
                                       unsigned int size, bool discont)
{
  IMediaSample *pSample = NULL;

  if (start != -1) {
    start /= 100;
    stop /= 100;
  }

  HRESULT hres = GetDeliveryBuffer(&pSample, NULL, NULL, 0);
  if (hres == S_OK && pSample)
  {
    BYTE *sample_buffer;
    pSample->GetPointer(&sample_buffer);
    if(sample_buffer)
    {
      memcpy (sample_buffer, buffer, size);
      pSample->SetActualDataLength(size);
    }
    pSample->SetDiscontinuity(discont);

    pSample->SetSyncPoint(TRUE);
    pSample->SetPreroll(FALSE);

    if (start != -1)
      pSample->SetTime(&start, &stop);

    hres = Deliver(pSample);
    pSample->Release();
  }
  else {
    GST_WARNING ("unable to obtain a delivery buffer");
  }

  return S_OK;
}

STDMETHODIMP FakeOutputPin::Flush ()
{
  DeliverBeginFlush();
  DeliverEndFlush();
  return S_OK;
}

STDMETHODIMP FakeOutputPin::SetSampleSize (unsigned int size)
{
  m_SampleSize = size;
  return S_OK;
}

/* filter */
FakeSrc::FakeSrc() :
    CBaseFilter("DshowFakeSink", NULL, &g_pCriticSec, CLSID_DecodeFakeSrc)
{
  m_pOutputPin = new FakeOutputPin((CSource *)this, m_pLock);
}

FakeSrc::~FakeSrc()
{
  if (m_pOutputPin)
    delete m_pOutputPin;
}

int FakeSrc::GetPinCount()
{
  return 1;
}

CBasePin *FakeSrc::GetPin(int n)
{
  return (CBasePin *)m_pOutputPin;
}

FakeOutputPin *FakeSrc::GetOutputPin()
{
  return m_pOutputPin;
}
