/*
 * GStreamer DirectShow codecs wrapper
 * Copyright <2006, 2007, 2008, 2009, 2010> Fluendo <support@fluendo.com>
 * Copyright <2006, 2007, 2008> Pioneers of the Inevitable <songbird@songbirdnest.com>
 * Copyright <2007,2008> Sebastien Moutte <sebastien@moutte.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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


#ifndef __GST_DSHOWAUDIODEC_H__
#define __GST_DSHOWAUDIODEC_H__

#include <gst/gst.h>
#include "gstdshowutil.h"
#include "gstdshowfakesrc.h"

G_BEGIN_DECLS

typedef struct {
  gchar *element_name;      /* The gst element factory name */
  gchar *element_longname;  /* Description string for element */
  unsigned long format;     /* WAVEFORMATEX format */
  gchar *sinkcaps;          /* GStreamer caps of input format */
  PreferredFilter *preferred_filters; /* NULL-terminated list of preferred filters */
} AudioCodecEntry;

#define GST_TYPE_DSHOWAUDIODEC               (gst_dshowaudiodec_get_type())
#define GST_DSHOWAUDIODEC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DSHOWAUDIODEC,GstDshowAudioDec))
#define GST_DSHOWAUDIODEC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DSHOWAUDIODEC,GstDshowAudioDecClass))
#define GST_IS_DSHOWAUDIODEC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DSHOWAUDIODEC))
#define GST_IS_DSHOWAUDIODEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DSHOWAUDIODEC))

typedef struct _GstDshowAudioDec GstDshowAudioDec;
typedef struct _GstDshowAudioDecClass GstDshowAudioDecClass;

class AudioFakeSink;

struct _GstDshowAudioDec
{
  GstElement element;

  /* element pads */
  GstPad *sinkpad;
  GstPad *srcpad;

  GstFlowReturn last_ret;

  /* filters interfaces*/
  FakeSrc *fakesrc;
  AudioFakeSink *fakesink;

  IBaseFilterPtr decfilter;

  /* graph manager interfaces */
  IMediaFilterPtr mediafilter;
  IFilterGraphPtr filtergraph;

  /* true when dshow graph is setup */
  gboolean setup;

  /* audio settings */
  gint bitrate;
  gint block_align;
  gint depth;
  gint channels;
  gint rate;
  gint layer;
  GstBuffer *codec_data;

  /* current segment */
  GstSegment * segment;

  /* timestamp of the next buffer */
  GstClockTime timestamp;

  gboolean comInitialized;
  GMutex   com_init_lock;
  GMutex   com_deinit_lock;
  GCond    com_initialized;
  GCond    com_uninitialize;
  GCond    com_uninitialized;
};

struct _GstDshowAudioDecClass
{
  GstElementClass parent_class;
  const AudioCodecEntry *entry;
};

gboolean dshow_adec_register (GstPlugin * plugin);

const GUID CLSID_AudioFakeSink =
{ 0x3867f537, 0x3e3d, 0x44da,
  { 0xbb, 0xf2, 0x02, 0x48, 0x7b, 0xb0, 0xbc, 0xc4} };

class AudioFakeSink :  public CBaseRenderer
{
public:
  AudioFakeSink(GstDshowAudioDec *dec) :
      m_hres(S_OK),
      CBaseRenderer(CLSID_AudioFakeSink, _T("AudioFakeSink"), NULL, &m_hres),
      mDec(dec)
  {};
  virtual ~AudioFakeSink() {};

  HRESULT DoRenderSample(IMediaSample *pMediaSample);
  HRESULT CheckMediaType(const CMediaType *pmt);
  HRESULT SetMediaType (AM_MEDIA_TYPE *pmt)
  {
    m_MediaType.Set (*pmt);
    return S_OK;
  }
  int GetBufferSize();

protected:
  HRESULT m_hres;
  CMediaType m_MediaType;
  GstDshowAudioDec *mDec;
};

G_END_DECLS

#endif /* __GST_DSHOWAUDIODEC_H__ */
