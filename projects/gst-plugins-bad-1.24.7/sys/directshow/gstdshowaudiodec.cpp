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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdshowaudiodec.h"
#include <mmreg.h>
#include <dmoreg.h>
#include <wmcodecdsp.h>
#include <gst/audio/audio.h>

GST_DEBUG_CATEGORY_STATIC (dshowaudiodec_debug);
#define GST_CAT_DEFAULT dshowaudiodec_debug

#define gst_dshowaudiodec_parent_class parent_class
G_DEFINE_TYPE(GstDshowAudioDec, gst_dshowaudiodec, GST_TYPE_ELEMENT)

static void gst_dshowaudiodec_finalize (GObject * object);
static GstStateChangeReturn gst_dshowaudiodec_change_state
    (GstElement * element, GstStateChange transition);

/* sink pad overwrites */
static gboolean gst_dshowaudiodec_sink_setcaps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_dshowaudiodec_chain (GstPad * pad, GstObject *parent, GstBuffer * buffer);
static gboolean gst_dshowaudiodec_sink_event (GstPad * pad, GstObject *parent, GstEvent * event);

/* utils */
static gboolean gst_dshowaudiodec_create_graph_and_filters (GstDshowAudioDec *
    adec);
static gboolean gst_dshowaudiodec_destroy_graph_and_filters (GstDshowAudioDec *
    adec);
static gboolean gst_dshowaudiodec_flush (GstDshowAudioDec * adec);
static gboolean gst_dshowaudiodec_get_filter_settings (GstDshowAudioDec * adec);
static gboolean gst_dshowaudiodec_setup_graph (GstDshowAudioDec * adec, GstCaps *caps);

/* All the GUIDs we want are generated from the FOURCC like this */
#define GUID_MEDIASUBTYPE_FROM_FOURCC(fourcc) \
    { fourcc , 0x0000, 0x0010, \
    { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}

/* WMA we should always use the DMO */
static PreferredFilter preferred_wma_filters[] = {
  {&CLSID_CWMADecMediaObject, &DMOCATEGORY_AUDIO_DECODER},
  {0}
};

/* Prefer the Vista (DMO) decoder if present, otherwise the XP
 * decoder (not a DMO), otherwise fallback to highest-merit */
static const GUID CLSID_XP_MP3_DECODER = {0x38BE3000, 0xDBF4, 0x11D0,
   {0x86,0x0E,0x00,0xA0,0x24,0xCF,0xEF,0x6D}};
static PreferredFilter preferred_mp3_filters[] = {
  {&CLSID_CMP3DecMediaObject, &DMOCATEGORY_AUDIO_DECODER},
  {&CLSID_XP_MP3_DECODER},
  {0}
};

/* MPEG 1/2: use the MPEG Audio Decoder filter */
static const GUID CLSID_WINDOWS_MPEG_AUDIO_DECODER =
  {0x4A2286E0, 0x7BEF, 0x11CE,
   {0x9B, 0xD9, 0x00, 0x00, 0xE2, 0x02, 0x59, 0x9C}};
static PreferredFilter preferred_mpegaudio_filters[] = {
  {&CLSID_WINDOWS_MPEG_AUDIO_DECODER},
  {0}
};

static const AudioCodecEntry audio_dec_codecs[] = {
  {"dshowadec_wma1", "Windows Media Audio 7",
   WAVE_FORMAT_MSAUDIO1,
   "audio/x-wma, wmaversion = (int) 1",
   preferred_wma_filters},

  {"dshowadec_wma2", "Windows Media Audio 8",
   WAVE_FORMAT_WMAUDIO2,
   "audio/x-wma, wmaversion = (int) 2",
   preferred_wma_filters},

  {"dshowadec_wma3", "Windows Media Audio 9 Professional",
   WAVE_FORMAT_WMAUDIO3,
   "audio/x-wma, wmaversion = (int) 3",
   preferred_wma_filters},

  {"dshowadec_wma4", "Windows Media Audio 9 Lossless",
   WAVE_FORMAT_WMAUDIO_LOSSLESS,
   "audio/x-wma, wmaversion = (int) 4",
   preferred_wma_filters},

  {"dshowadec_wms", "Windows Media Audio Voice v9",
   WAVE_FORMAT_WMAVOICE9,
   "audio/x-wms",
   preferred_wma_filters},

  {"dshowadec_mp3", "MPEG Layer 3 Audio",
   WAVE_FORMAT_MPEGLAYER3,
   "audio/mpeg, "
       "mpegversion = (int) 1, "
       "layer = (int)3, "
       "rate = (int) [ 8000, 48000 ], "
       "channels = (int) [ 1, 2 ], "
       "parsed= (boolean) true",
   preferred_mp3_filters},

  {"dshowadec_mpeg_1_2", "MPEG Layer 1,2 Audio",
   WAVE_FORMAT_MPEG,
   "audio/mpeg, "
       "mpegversion = (int) 1, "
       "layer = (int) [ 1, 2 ], "
       "rate = (int) [ 8000, 48000 ], "
       "channels = (int) [ 1, 2 ], "
       "parsed= (boolean) true",
   preferred_mpegaudio_filters},
};

HRESULT AudioFakeSink::DoRenderSample(IMediaSample *pMediaSample)
{
  GstBuffer *out_buf = NULL;
  gboolean in_seg = FALSE;
  GstClockTime buf_start, buf_stop;
  guint64 clip_start = 0, clip_stop = 0;
  guint start_offset = 0, stop_offset;
  GstClockTime duration;

  if(pMediaSample)
  {
    BYTE *pBuffer = NULL;
    LONGLONG lStart = 0, lStop = 0;
    long size = pMediaSample->GetActualDataLength();

    pMediaSample->GetPointer(&pBuffer);
    pMediaSample->GetTime(&lStart, &lStop);

    if (!GST_CLOCK_TIME_IS_VALID (mDec->timestamp)) {
      // Convert REFERENCE_TIME to GST_CLOCK_TIME
      mDec->timestamp = (GstClockTime)lStart * 100;
    }
    duration = (lStop - lStart) * 100;

    buf_start = mDec->timestamp;
    buf_stop = mDec->timestamp + duration;

    /* save stop position to start next buffer with it */
    mDec->timestamp = buf_stop;

    /* check if this buffer is in our current segment */
    in_seg = gst_segment_clip (mDec->segment, GST_FORMAT_TIME,
        buf_start, buf_stop, &clip_start, &clip_stop);

    /* if the buffer is out of segment do not push it downstream */
    if (!in_seg) {
      GST_DEBUG_OBJECT (mDec,
          "buffer is out of segment, start %" GST_TIME_FORMAT " stop %"
          GST_TIME_FORMAT, GST_TIME_ARGS (buf_start), GST_TIME_ARGS (buf_stop));
      goto done;
    }

    /* buffer is entirely or partially in-segment, so allocate a
     * GstBuffer for output, and clip if required */

    /* allocate a new buffer for raw audio */
    out_buf = gst_buffer_new_and_alloc(size);
    if (!out_buf) {
      GST_WARNING_OBJECT (mDec, "cannot allocate a new GstBuffer");
      goto done;
    }

    /* set buffer properties */
    GST_BUFFER_TIMESTAMP (out_buf) = buf_start;
    GST_BUFFER_DURATION (out_buf) = duration;

    if (gst_buffer_fill(out_buf, 0, pBuffer, size) != size) {
      gst_buffer_unref (out_buf);
      GST_WARNING_OBJECT (mDec, "unable to fill output buffer");
      goto done;
    }

    /* we have to remove some heading samples */
    if ((GstClockTime) clip_start > buf_start) {
      start_offset = (guint)gst_util_uint64_scale_int (clip_start - buf_start,
          mDec->rate, GST_SECOND) * mDec->depth / 8 * mDec->channels;
    }
    else
      start_offset = 0;
    /* we have to remove some trailing samples */
    if ((GstClockTime) clip_stop < buf_stop) {
      stop_offset = (guint)gst_util_uint64_scale_int (buf_stop - clip_stop,
          mDec->rate, GST_SECOND) * mDec->depth / 8 * mDec->channels;
    }
    else
      stop_offset = size;

    /* truncating */
    if ((start_offset != 0) || (stop_offset != (size_t) size)) {

      GstBuffer *subbuf = gst_buffer_copy_region (out_buf, GST_BUFFER_COPY_ALL,
        start_offset, stop_offset - start_offset);

      if (subbuf) {
        gst_buffer_unref (out_buf);
        out_buf = subbuf;
      }
    }

    GST_BUFFER_TIMESTAMP (out_buf) = clip_start;
    GST_BUFFER_DURATION (out_buf) = clip_stop - clip_start;

    /* replace the saved stop position by the clipped one */
    mDec->timestamp = clip_stop;

    GST_DEBUG_OBJECT (mDec,
        "push_buffer (size %d)=> pts %" GST_TIME_FORMAT " stop %" GST_TIME_FORMAT
        " duration %" GST_TIME_FORMAT, size,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (out_buf)),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (out_buf) +
            GST_BUFFER_DURATION (out_buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (out_buf)));

    mDec->last_ret = gst_pad_push (mDec->srcpad, out_buf);
  }

done:
  return S_OK;
}

HRESULT AudioFakeSink::CheckMediaType(const CMediaType *pmt)
{
  if(pmt != NULL)
  {
    /* The Vista MP3 decoder (and possibly others?) outputs an
     * AM_MEDIA_TYPE with the wrong cbFormat. So, rather than using
     * CMediaType.operator==, we implement a sufficient check ourselves.
     * I think this is a bug in the MP3 decoder.
     */
    if (IsEqualGUID (pmt->majortype, m_MediaType.majortype) &&
        IsEqualGUID (pmt->subtype, m_MediaType.subtype) &&
        IsEqualGUID (pmt->formattype, m_MediaType.formattype))
    {
      /* Types are the same at the top-level. Now, we need to compare
       * the format blocks.
       * We special case WAVEFORMATEX to not check that
       * pmt->cbFormat == m_MediaType.cbFormat, though the actual format
       * blocks must still be the same.
       */
      if (pmt->formattype == FORMAT_WaveFormatEx) {
        if (pmt->cbFormat >= sizeof (WAVEFORMATEX) &&
            m_MediaType.cbFormat >= sizeof (WAVEFORMATEX))
        {
          WAVEFORMATEX *wf1 = (WAVEFORMATEX *)pmt->pbFormat;
          WAVEFORMATEX *wf2 = (WAVEFORMATEX *)m_MediaType.pbFormat;
          if (wf1->cbSize == wf2->cbSize &&
              memcmp (wf1, wf2, sizeof(WAVEFORMATEX) + wf1->cbSize) == 0)
            return S_OK;
        }
      }
      else {
        if (pmt->cbFormat == m_MediaType.cbFormat &&
             pmt->cbFormat == 0 ||
             (pmt->pbFormat != NULL && m_MediaType.pbFormat != NULL &&
                 memcmp (pmt->pbFormat, m_MediaType.pbFormat, pmt->cbFormat) == 0))
          return S_OK;
      }
    }
  }

  return S_FALSE;
}

int AudioFakeSink::GetBufferSize()
{
  IMemAllocator *allocator = NULL;
  if (m_pInputPin) {
    allocator = m_pInputPin->Allocator();
    if(allocator) {
      ALLOCATOR_PROPERTIES props;
      allocator->GetProperties(&props);
      return props.cbBuffer;
    }
  }

  return 0;
}

static void
gst_dshowaudiodec_base_init (gpointer klass)
{
  GstDshowAudioDecClass *audiodec_class = (GstDshowAudioDecClass *) klass;
  GstPadTemplate *src, *sink;
  GstCaps *srccaps, *sinkcaps;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  const AudioCodecEntry *tmp;
  gpointer qdata;
  gchar *longname, *description;

  qdata = g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass), DSHOW_CODEC_QDATA);

  /* element details */
  tmp = audiodec_class->entry = (AudioCodecEntry *) qdata;

  longname = g_strdup_printf ("DirectShow %s Decoder Wrapper",
      tmp->element_longname);
  description = g_strdup_printf ("DirectShow %s Decoder Wrapper",
      tmp->element_longname);

  gst_element_class_set_metadata(element_class, longname, "Codec/Decoder/Audio", description,
    "Sebastien Moutte <sebastien@moutte.net>");

  g_free (longname);
  g_free (description);

  sinkcaps = gst_caps_from_string (tmp->sinkcaps);

  srccaps = gst_caps_from_string (
      "audio/x-raw,"
      "format = (string)" GST_AUDIO_FORMATS_ALL ","
      "rate = (int)[1, MAX],"
      "channels = (int)[1, MAX],"
      "layout = (string)interleaved");

  sink = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sinkcaps);
  src = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, srccaps);

  /* register */
  gst_element_class_add_pad_template (element_class, src);
  gst_element_class_add_pad_template (element_class, sink);

  if (sinkcaps)
    gst_caps_unref(sinkcaps);

  if (srccaps)
    gst_caps_unref(srccaps);
}

static void
gst_dshowaudiodec_class_init (GstDshowAudioDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_dshowaudiodec_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dshowaudiodec_change_state);

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);
}

static void
gst_dshowaudiodec_com_thread (GstDshowAudioDec * adec)
{
  HRESULT res;

  g_mutex_lock (&adec->com_init_lock);

  /* Initialize COM with a MTA for this process. This thread will
   * be the first one to enter the apartement and the last one to leave
   * it, unitializing COM properly */

  res = CoInitializeEx (0, COINIT_MULTITHREADED);
  if (res == S_FALSE)
    GST_WARNING_OBJECT (adec, "COM has been already initialized in the same process");
  else if (res == RPC_E_CHANGED_MODE)
    GST_WARNING_OBJECT (adec, "The concurrency model of COM has changed.");
  else
    GST_INFO_OBJECT (adec, "COM initialized successfully");

  adec->comInitialized = TRUE;

  /* Signal other threads waiting on this condition that COM was initialized */
  g_cond_signal (&adec->com_initialized);

  g_mutex_unlock (&adec->com_init_lock);

  /* Wait until the uninitialize condition is met to leave the COM apartement */
  g_mutex_lock (&adec->com_deinit_lock);
  g_cond_wait (&adec->com_uninitialize, &adec->com_deinit_lock);

  CoUninitialize ();
  GST_INFO_OBJECT (adec, "COM uninitialized successfully");
  adec->comInitialized = FALSE;
  g_cond_signal (&adec->com_uninitialized);
  g_mutex_unlock (&adec->com_deinit_lock);
}

static void
gst_dshowaudiodec_init (GstDshowAudioDec * adec)
{
  GstElementClass *element_class = GST_ELEMENT_GET_CLASS (adec);

  /* setup pads */
  adec->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (element_class, "sink"), "sink");

  gst_pad_set_event_function (adec->sinkpad, gst_dshowaudiodec_sink_event);
  gst_pad_set_chain_function (adec->sinkpad, gst_dshowaudiodec_chain);
  gst_element_add_pad (GST_ELEMENT (adec), adec->sinkpad);

  adec->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (element_class, "src"), "src");
  gst_element_add_pad (GST_ELEMENT (adec), adec->srcpad);

  adec->fakesrc = NULL;
  adec->fakesink = NULL;

  adec->decfilter = 0;
  adec->filtergraph = 0;
  adec->mediafilter = 0;

  adec->timestamp = GST_CLOCK_TIME_NONE;
  adec->segment = gst_segment_new ();
  adec->setup = FALSE;
  adec->depth = 0;
  adec->bitrate = 0;
  adec->block_align = 0;
  adec->channels = 0;
  adec->rate = 0;
  adec->layer = 0;
  adec->codec_data = NULL;

  adec->last_ret = GST_FLOW_OK;

  g_mutex_init(&adec->com_init_lock);
  g_mutex_init(&adec->com_deinit_lock);
  g_cond_init(&adec->com_initialized);
  g_cond_init(&adec->com_uninitialize);
  g_cond_init(&adec->com_uninitialized);

  g_mutex_lock (&adec->com_init_lock);

  /* create the COM initialization thread */
  g_thread_new ("COM init thread", (GThreadFunc)gst_dshowaudiodec_com_thread,
    adec);

  /* wait until the COM thread signals that COM has been initialized */
  g_cond_wait (&adec->com_initialized, &adec->com_init_lock);
  g_mutex_unlock (&adec->com_init_lock);
}

static void
gst_dshowaudiodec_finalize (GObject * object)
{
  GstDshowAudioDec *adec = (GstDshowAudioDec *) (object);

  if (adec->segment) {
    gst_segment_free (adec->segment);
    adec->segment = NULL;
  }

  if (adec->codec_data) {
    gst_buffer_unref (adec->codec_data);
    adec->codec_data = NULL;
  }

  /* signal the COM thread that it sould uninitialize COM */
  if (adec->comInitialized) {
    g_mutex_lock (&adec->com_deinit_lock);
    g_cond_signal (&adec->com_uninitialize);
    g_cond_wait (&adec->com_uninitialized, &adec->com_deinit_lock);
    g_mutex_unlock (&adec->com_deinit_lock);
  }

  g_mutex_clear (&adec->com_init_lock);
  g_mutex_clear (&adec->com_deinit_lock);
  g_cond_clear (&adec->com_initialized);
  g_cond_clear (&adec->com_uninitialize);
  g_cond_clear (&adec->com_uninitialized);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstStateChangeReturn
gst_dshowaudiodec_change_state (GstElement * element, GstStateChange transition)
{
  GstDshowAudioDec *adec = (GstDshowAudioDec *) (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_dshowaudiodec_create_graph_and_filters (adec))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      adec->depth = 0;
      adec->bitrate = 0;
      adec->block_align = 0;
      adec->channels = 0;
      adec->rate = 0;
      adec->layer = 0;
      if (adec->codec_data) {
        gst_buffer_unref (adec->codec_data);
        adec->codec_data = NULL;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (!gst_dshowaudiodec_destroy_graph_and_filters (adec))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS(parent_class)->change_state (element, transition);
}

static gboolean
gst_dshowaudiodec_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = FALSE;
  GstDshowAudioDec *adec = (GstDshowAudioDec *) gst_pad_get_parent (pad);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const GValue *v = NULL;

  adec->timestamp = GST_CLOCK_TIME_NONE;

  /* read data, only rate and channels are needed */
  if (!gst_structure_get_int (s, "rate", &adec->rate) ||
      !gst_structure_get_int (s, "channels", &adec->channels)) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("error getting audio specs from caps"), (NULL));
    goto end;
  }

  gst_structure_get_int (s, "depth", &adec->depth);
  gst_structure_get_int (s, "bitrate", &adec->bitrate);
  gst_structure_get_int (s, "block_align", &adec->block_align);
  gst_structure_get_int (s, "layer", &adec->layer);

  if (adec->codec_data) {
    gst_buffer_unref (adec->codec_data);
    adec->codec_data = NULL;
  }

  if ((v = gst_structure_get_value (s, "codec_data")))
    adec->codec_data = gst_buffer_ref (gst_value_get_buffer (v));

  ret = gst_dshowaudiodec_setup_graph (adec, caps);
end:
  gst_object_unref (adec);

  return ret;
}

static GstFlowReturn
gst_dshowaudiodec_chain (GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  GstDshowAudioDec *adec = (GstDshowAudioDec *) gst_pad_get_parent (pad);
  GstMapInfo map;
  bool discont = FALSE;

  if (!adec->setup) {
    /* we are not set up */
    GST_WARNING_OBJECT (adec, "Decoder not set up, failing");
    adec->last_ret = GST_FLOW_FLUSHING;
    goto beach;
  }

  if (adec->last_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (adec, "last decoding iteration generated a fatal error "
        "%s", gst_flow_get_name (adec->last_ret));
    goto beach;
  }

  GST_CAT_DEBUG_OBJECT (dshowaudiodec_debug, adec, "chain (size %d)=> pts %"
      GST_TIME_FORMAT " stop %" GST_TIME_FORMAT,
      gst_buffer_get_size(buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  /* if the incoming buffer has discont flag set => flush decoder data */
  if (buffer && GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
    GST_CAT_DEBUG_OBJECT (dshowaudiodec_debug, adec,
        "this buffer has a DISCONT flag (%" GST_TIME_FORMAT "), flushing",
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
    gst_dshowaudiodec_flush (adec);
    discont = TRUE;
  }

  /* push the buffer to the directshow decoder */
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  adec->fakesrc->GetOutputPin()->PushBuffer (
      map.data, GST_BUFFER_TIMESTAMP (buffer),
      GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer),
      map.size, (bool)discont);
  gst_buffer_unmap(buffer, &map);

beach:
  gst_buffer_unref (buffer);
  gst_object_unref (adec);
  return adec->last_ret;
}

static gboolean
gst_dshowaudiodec_sink_event (GstPad * pad, GstObject *parent, GstEvent * event)
{
  gboolean ret = TRUE;
  GstDshowAudioDec *adec = (GstDshowAudioDec *) parent;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      gst_event_parse_caps(event, &caps);
      ret = gst_dshowaudiodec_sink_setcaps(pad, caps);
      break;
    }

    case GST_EVENT_FLUSH_STOP:{
      gst_dshowaudiodec_flush (adec);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }

    case GST_EVENT_SEGMENT:{
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);

      GST_CAT_DEBUG_OBJECT (dshowaudiodec_debug, adec,
          "received new segment from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (segment->start), GST_TIME_ARGS (segment->stop));

      /* save the new segment in our local current segment */
      gst_segment_copy_into(segment, adec->segment);

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }

    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_dshowaudiodec_flush (GstDshowAudioDec * adec)
{
  if (!adec->fakesrc)
    return FALSE;

  /* flush dshow decoder and reset timestamp */
  adec->fakesrc->GetOutputPin()->Flush();

  adec->timestamp = GST_CLOCK_TIME_NONE;
  adec->last_ret = GST_FLOW_OK;

  return TRUE;
}

static AM_MEDIA_TYPE *
dshowaudiodec_set_input_format (GstDshowAudioDec *adec, GstCaps *caps)
{
  AM_MEDIA_TYPE *mediatype;
  WAVEFORMATEX *format;
  GstDshowAudioDecClass *klass =
      (GstDshowAudioDecClass *) G_OBJECT_GET_CLASS (adec);
  const AudioCodecEntry *codec_entry = klass->entry;
  int size;

  mediatype = (AM_MEDIA_TYPE *)g_malloc0 (sizeof(AM_MEDIA_TYPE));
  mediatype->majortype = MEDIATYPE_Audio;
  GUID subtype = GUID_MEDIASUBTYPE_FROM_FOURCC (0x00000000);
  subtype.Data1 = codec_entry->format;
  mediatype->subtype = subtype;
  mediatype->bFixedSizeSamples = TRUE;
  mediatype->bTemporalCompression = FALSE;
  if (adec->block_align)
    mediatype->lSampleSize = adec->block_align;
  else
    mediatype->lSampleSize = 8192; /* need to evaluate it dynamically */
  mediatype->formattype = FORMAT_WaveFormatEx;

  /* We need this special behaviour for layers 1 and 2 (layer 3 uses a different
   * decoder which doesn't need this */
  if (adec->layer == 1 || adec->layer == 2) {
    MPEG1WAVEFORMAT *mpeg1_format;
    int samples, version;
    GstStructure *structure = gst_caps_get_structure (caps, 0);

    size = sizeof (MPEG1WAVEFORMAT);
    format = (WAVEFORMATEX *)g_malloc0 (size);
    format->cbSize = sizeof (MPEG1WAVEFORMAT) - sizeof (WAVEFORMATEX);
    format->wFormatTag = WAVE_FORMAT_MPEG;

    mpeg1_format = (MPEG1WAVEFORMAT *) format;

    mpeg1_format->wfx.nChannels = adec->channels;
    if (adec->channels == 2)
      mpeg1_format->fwHeadMode = ACM_MPEG_STEREO;
    else
      mpeg1_format->fwHeadMode = ACM_MPEG_SINGLECHANNEL;

    mpeg1_format->fwHeadModeExt = 0;
    mpeg1_format->wHeadEmphasis = 0;
    mpeg1_format->fwHeadFlags = 0;

    switch (adec->layer) {
      case 1:
        mpeg1_format->fwHeadLayer = ACM_MPEG_LAYER3;
        break;
      case 2:
        mpeg1_format->fwHeadLayer = ACM_MPEG_LAYER2;
        break;
      case 3:
        mpeg1_format->fwHeadLayer = ACM_MPEG_LAYER1;
        break;
    };

    gst_structure_get_int (structure, "mpegaudioversion", &version);
    if (adec->layer == 1) {
      samples = 384;
    } else {
      if (version == 1) {
        samples = 576;
      } else {
        samples = 1152;
      }
    }
    mpeg1_format->wfx.nBlockAlign = (WORD) samples;
    mpeg1_format->wfx.nSamplesPerSec = adec->rate;
    mpeg1_format->dwHeadBitrate = 128000; /* This doesn't seem to matter */
    mpeg1_format->wfx.nAvgBytesPerSec = mpeg1_format->dwHeadBitrate / 8;
  }
  else
  {
    size = sizeof (WAVEFORMATEX) +
        (adec->codec_data ? gst_buffer_get_size(adec->codec_data) : 0);

    if (adec->layer == 3) {
      MPEGLAYER3WAVEFORMAT *mp3format;

      /* The WinXP mp3 decoder doesn't actually check the size of this structure,
       * but requires that this be allocated and filled out (or we get obscure
       * random crashes)
       */
      size = sizeof (MPEGLAYER3WAVEFORMAT);
      mp3format = (MPEGLAYER3WAVEFORMAT *)g_malloc0 (size);
      format = (WAVEFORMATEX *)mp3format;
      format->cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;

      mp3format->wID = MPEGLAYER3_ID_MPEG;
      mp3format->fdwFlags = MPEGLAYER3_FLAG_PADDING_ISO; /* No idea what this means for a decoder */

      /* The XP decoder divides by nBlockSize, so we must set this to a
         non-zero value, but it doesn't matter what - this is meaningless
         for VBR mp3 anyway */
      mp3format->nBlockSize = 1;
      mp3format->nFramesPerBlock = 1;
      mp3format->nCodecDelay = 0;
    }
    else {
      format = (WAVEFORMATEX *)g_malloc0 (size);

      if (adec->codec_data) {     /* Codec data is appended after our header */
        gsize codec_size = gst_buffer_get_size(adec->codec_data);
        gst_buffer_extract(adec->codec_data, 0, ((guchar *) format) + sizeof (WAVEFORMATEX),
          codec_size);
        format->cbSize = codec_size;
      }
    }

    format->wFormatTag = codec_entry->format;
    format->nChannels = adec->channels;
    format->nSamplesPerSec = adec->rate;
    format->nAvgBytesPerSec = adec->bitrate / 8;
    format->nBlockAlign = adec->block_align;
    format->wBitsPerSample = adec->depth;
  }

  mediatype->cbFormat = size;
  mediatype->pbFormat = (BYTE *) format;

  return mediatype;
}

static AM_MEDIA_TYPE *
dshowaudiodec_set_output_format (GstDshowAudioDec *adec)
{
  AM_MEDIA_TYPE *mediatype;
  WAVEFORMATEX *format;
  GstDshowAudioDecClass *klass =
      (GstDshowAudioDecClass *) G_OBJECT_GET_CLASS (adec);
  const AudioCodecEntry *codec_entry = klass->entry;

  if (!gst_dshowaudiodec_get_filter_settings (adec)) {
    return NULL;
  }

  format = (WAVEFORMATEX *)g_malloc0(sizeof (WAVEFORMATEX));
  format->wFormatTag = WAVE_FORMAT_PCM;
  format->wBitsPerSample = adec->depth;
  format->nChannels = adec->channels;
  format->nBlockAlign = adec->channels * (adec->depth / 8);
  format->nSamplesPerSec = adec->rate;
  format->nAvgBytesPerSec = format->nBlockAlign * adec->rate;

  mediatype = (AM_MEDIA_TYPE *)g_malloc0(sizeof (AM_MEDIA_TYPE));
  mediatype->majortype = MEDIATYPE_Audio;
  GUID subtype = GUID_MEDIASUBTYPE_FROM_FOURCC (WAVE_FORMAT_PCM);
  mediatype->subtype = subtype;
  mediatype->bFixedSizeSamples = TRUE;
  mediatype->bTemporalCompression = FALSE;
  mediatype->lSampleSize = format->nBlockAlign;
  mediatype->formattype = FORMAT_WaveFormatEx;
  mediatype->cbFormat = sizeof (WAVEFORMATEX);
  mediatype->pbFormat = (BYTE *)format;

  return mediatype;
}

static void
dshowadec_free_mediatype (AM_MEDIA_TYPE *mediatype)
{
  g_free (mediatype->pbFormat);
  g_free (mediatype);
}

static gboolean
gst_dshowaudiodec_setup_graph (GstDshowAudioDec * adec, GstCaps *caps)
{
  gboolean ret = FALSE;
  GstDshowAudioDecClass *klass =
      (GstDshowAudioDecClass *) G_OBJECT_GET_CLASS (adec);
  HRESULT hres;
  GstCaps *outcaps = NULL;
  AM_MEDIA_TYPE *output_mediatype = NULL;
  AM_MEDIA_TYPE *input_mediatype = NULL;
  IPinPtr output_pin = NULL;
  IPinPtr input_pin = NULL;
  const AudioCodecEntry *codec_entry = klass->entry;
  IBaseFilterPtr srcfilter;
  IBaseFilterPtr sinkfilter;
  GstAudioInfo audio_info;

  input_mediatype = dshowaudiodec_set_input_format (adec, caps);

  adec->fakesrc->GetOutputPin()->SetMediaType (input_mediatype);

  srcfilter = adec->fakesrc;

  /* connect our fake source to decoder */
  output_pin = gst_dshow_util_get_pin_from_filter (srcfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't get output pin from our directshow fakesrc filter"), (NULL));
    goto end;
  }
  input_pin = gst_dshow_util_get_pin_from_filter (adec->decfilter, PINDIR_INPUT);
  if (!input_pin) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't get input pin from decoder filter"), (NULL));
    goto end;
  }

  hres = adec->filtergraph->ConnectDirect (output_pin, input_pin,
      NULL);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't connect fakesrc with decoder (error=%x)", hres), (NULL));
    goto end;
  }

  output_mediatype = dshowaudiodec_set_output_format (adec);
  if (!output_mediatype) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't get audio output format from decoder"), (NULL));
    goto end;
  }

  adec->fakesink->SetMediaType(output_mediatype);

  gst_audio_info_init(&audio_info);
  gst_audio_info_set_format(&audio_info,
    gst_audio_format_build_integer(TRUE, G_BYTE_ORDER, adec->depth, adec->depth),
    adec->rate, adec->channels, NULL);

  outcaps = gst_audio_info_to_caps(&audio_info);

  if (!gst_pad_set_caps (adec->srcpad, outcaps)) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Failed to negotiate output"), (NULL));
    goto end;
  }

  /* connect the decoder to our fake sink */
  output_pin = gst_dshow_util_get_pin_from_filter (adec->decfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't get output pin from our decoder filter"), (NULL));
    goto end;
  }

  sinkfilter = adec->fakesink;
  input_pin = gst_dshow_util_get_pin_from_filter (sinkfilter, PINDIR_INPUT);
  if (!input_pin) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't get input pin from our directshow fakesink filter"), (NULL));
    goto end;
  }

  hres = adec->filtergraph->ConnectDirect(output_pin, input_pin, NULL);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't connect decoder with fakesink (error=%x)", hres), (NULL));
    goto end;
  }

  hres = adec->mediafilter->Run (-1);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("Can't run the directshow graph (error=%x)", hres), (NULL));
    goto end;
  }

  ret = TRUE;
  adec->setup = TRUE;
end:
  if (outcaps)
    gst_caps_unref(outcaps);
  if (input_mediatype)
    dshowadec_free_mediatype (input_mediatype);
  if (output_mediatype)
    dshowadec_free_mediatype (output_mediatype);

  return ret;
}

static gboolean
gst_dshowaudiodec_get_filter_settings (GstDshowAudioDec * adec)
{
  IPinPtr output_pin;
  IEnumMediaTypesPtr enum_mediatypes;
  HRESULT hres;
  ULONG fetched;
  BOOL ret = FALSE;

  if (adec->decfilter == 0)
    return FALSE;

  output_pin = gst_dshow_util_get_pin_from_filter (adec->decfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (adec, CORE, NEGOTIATION,
        ("failed getting output pin from the decoder"), (NULL));
    return FALSE;
  }

  hres = output_pin->EnumMediaTypes (&enum_mediatypes);
  if (hres == S_OK && enum_mediatypes) {
    AM_MEDIA_TYPE *mediatype = NULL;

    enum_mediatypes->Reset();
    while (!ret && enum_mediatypes->Next(1, &mediatype, &fetched) == S_OK)
    {
      if (IsEqualGUID (mediatype->subtype, MEDIASUBTYPE_PCM) &&
          IsEqualGUID (mediatype->formattype, FORMAT_WaveFormatEx))
      {
        WAVEFORMATEX *audio_info = (WAVEFORMATEX *) mediatype->pbFormat;

        adec->channels = audio_info->nChannels;
        adec->depth = audio_info->wBitsPerSample;
        adec->rate = audio_info->nSamplesPerSec;
        ret = TRUE;
      }
      DeleteMediaType (mediatype);
    }
  }

  return ret;
}

static gboolean
gst_dshowaudiodec_create_graph_and_filters (GstDshowAudioDec * adec)
{
  HRESULT hres;
  GstDshowAudioDecClass *klass =
      (GstDshowAudioDecClass *) G_OBJECT_GET_CLASS (adec);
  IBaseFilterPtr srcfilter;
  IBaseFilterPtr sinkfilter;
  GUID insubtype = GUID_MEDIASUBTYPE_FROM_FOURCC (klass->entry->format);
  GUID outsubtype = GUID_MEDIASUBTYPE_FROM_FOURCC (WAVE_FORMAT_PCM);

  /* create the filter graph manager object */
  hres = adec->filtergraph.CreateInstance (
      CLSID_FilterGraph, NULL, CLSCTX_INPROC);
  if (FAILED (hres)) {
    GST_ELEMENT_ERROR (adec, STREAM, FAILED,
        ("Can't create an instance of the directshow graph manager (error=%d)",
            hres), (NULL));
    goto error;
  }

  hres = adec->filtergraph->QueryInterface (&adec->mediafilter);
  if (FAILED (hres)) {
    GST_WARNING_OBJECT (adec, "Can't QI filtergraph to mediafilter");
    goto error;
  }

  /* create fake src filter */
  adec->fakesrc = new FakeSrc();
  /* Created with a refcount of zero, so increment that */
  adec->fakesrc->AddRef();

  /* create decoder filter */
  adec->decfilter = gst_dshow_util_find_filter (MEDIATYPE_Audio,
          insubtype,
          MEDIATYPE_Audio,
          outsubtype,
          klass->entry->preferred_filters);
  if (adec->decfilter == NULL) {
    GST_ELEMENT_ERROR (adec, STREAM, FAILED,
        ("Can't create an instance of the decoder filter"), (NULL));
    goto error;
  }

  /* create fake sink filter */
  adec->fakesink = new AudioFakeSink(adec);
  /* Created with a refcount of zero, so increment that */
  adec->fakesink->AddRef();

  /* add filters to the graph */
  srcfilter = adec->fakesrc;
  hres = adec->filtergraph->AddFilter (srcfilter, L"src");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, STREAM, FAILED,
        ("Can't add fakesrc filter to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  hres = adec->filtergraph->AddFilter(adec->decfilter, L"decoder");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, STREAM, FAILED,
        ("Can't add decoder filter to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  sinkfilter = adec->fakesink;
  hres = adec->filtergraph->AddFilter(sinkfilter, L"sink");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (adec, STREAM, FAILED,
        ("Can't add fakesink filter to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  return TRUE;

error:
  if (adec->fakesrc) {
    adec->fakesrc->Release();
    adec->fakesrc = NULL;
  }
  if (adec->fakesink) {
    adec->fakesink->Release();
    adec->fakesink = NULL;
  }
  adec->decfilter = 0;
  adec->mediafilter = 0;
  adec->filtergraph = 0;

  return FALSE;
}

static gboolean
gst_dshowaudiodec_destroy_graph_and_filters (GstDshowAudioDec * adec)
{
  if (adec->mediafilter) {
    adec->mediafilter->Stop();
  }

  if (adec->fakesrc) {
    if (adec->filtergraph) {
      IBaseFilterPtr filter = adec->fakesrc;
      adec->filtergraph->RemoveFilter(filter);
    }
    adec->fakesrc->Release();
    adec->fakesrc = NULL;
  }
  if (adec->decfilter) {
    if (adec->filtergraph)
      adec->filtergraph->RemoveFilter(adec->decfilter);
    adec->decfilter = 0;
  }
  if (adec->fakesink) {
    if (adec->filtergraph) {
      IBaseFilterPtr filter = adec->fakesink;
      adec->filtergraph->RemoveFilter(filter);
    }

    adec->fakesink->Release();
    adec->fakesink = NULL;
  }
  adec->mediafilter = 0;
  adec->filtergraph = 0;

  adec->setup = FALSE;

  return TRUE;
}

gboolean
dshow_adec_register (GstPlugin * plugin)
{
  GTypeInfo info = {
    sizeof (GstDshowAudioDecClass),
    (GBaseInitFunc) gst_dshowaudiodec_base_init,
    NULL,
    (GClassInitFunc) gst_dshowaudiodec_class_init,
    NULL,
    NULL,
    sizeof (GstDshowAudioDec),
    0,
    (GInstanceInitFunc) gst_dshowaudiodec_init,
  };
  gint i;
  HRESULT hr;

  GST_DEBUG_CATEGORY_INIT (dshowaudiodec_debug, "dshowaudiodec", 0,
      "Directshow filter audio decoder");

  hr = CoInitialize(0);
  for (i = 0; i < sizeof (audio_dec_codecs) / sizeof (AudioCodecEntry); i++) {
    GType type;
    IBaseFilterPtr filter;
    GUID insubtype = GUID_MEDIASUBTYPE_FROM_FOURCC (audio_dec_codecs[i].format);
    GUID outsubtype = GUID_MEDIASUBTYPE_FROM_FOURCC (WAVE_FORMAT_PCM);

    filter = gst_dshow_util_find_filter (MEDIATYPE_Audio,
            insubtype,
            MEDIATYPE_Audio,
            outsubtype,
            audio_dec_codecs[i].preferred_filters);

    if (filter)
    {
      GST_DEBUG ("Registering %s", audio_dec_codecs[i].element_name);

      type = g_type_register_static (GST_TYPE_ELEMENT,
          audio_dec_codecs[i].element_name, &info, (GTypeFlags)0);
      g_type_set_qdata (type, DSHOW_CODEC_QDATA, (gpointer) (audio_dec_codecs + i));
      if (!gst_element_register (plugin, audio_dec_codecs[i].element_name,
              GST_RANK_MARGINAL, type)) {
        return FALSE;
      }
      GST_CAT_DEBUG (dshowaudiodec_debug, "Registered %s",
          audio_dec_codecs[i].element_name);
    }
    else {
      GST_DEBUG ("Element %s not registered "
                 "(the format is not supported by the system)",
                 audio_dec_codecs[i].element_name);
    }
  }

  if (SUCCEEDED(hr))
    CoUninitialize ();

  return TRUE;
}
