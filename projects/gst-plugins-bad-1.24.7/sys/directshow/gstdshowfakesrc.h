/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowfakesrc.h:
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
#ifndef _DSHOWDECWRAPPER_FAKESRC_H_
#define _DSHOWDECWRAPPER_FAKESRC_H_

#include <gst/gst.h>
#include "gstdshowutil.h"

class FakeOutputPin : public CBaseOutputPin
{
protected:
/* members */
  HRESULT m_hres;
  CMediaType m_MediaType;
  unsigned int m_SampleSize;

public:
/* methods */
  FakeOutputPin (CBaseFilter *pFilter, CCritSec *sec);
  ~FakeOutputPin ();

  virtual HRESULT CheckMediaType(const CMediaType *pmt);
  HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
  virtual HRESULT DecideBufferSize (IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest);
  STDMETHOD (SetMediaType) (AM_MEDIA_TYPE *pmt);
  STDMETHOD (PushBuffer) (byte *buffer, __int64 start, __int64 stop, unsigned int size, bool discont);
  STDMETHOD (Flush) ();
  STDMETHOD (SetSampleSize) (unsigned int size);
};

class FakeSrc : public CBaseFilter
{
public:
/* members */
  FakeOutputPin *m_pOutputPin;

/* methods */
  FakeSrc ();
  virtual ~FakeSrc ();

  virtual int GetPinCount();
  virtual CBasePin *GetPin(int n);

  FakeOutputPin *GetOutputPin();
};

#endif // _DSHOWDECWRAPPER_FAKESRC_H_
