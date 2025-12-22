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

#include <dmoreg.h>
#include <wmcodecdsp.h>

#include "gstdshowvideodec.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (dshowvideodec_debug);
#define GST_CAT_DEFAULT dshowvideodec_debug

#define gst_dshowvideodec_parent_class parent_class
G_DEFINE_TYPE(GstDshowVideoDec, gst_dshowvideodec, GST_TYPE_ELEMENT)

static void gst_dshowvideodec_finalize (GObject * object);
static GstStateChangeReturn gst_dshowvideodec_change_state
    (GstElement * element, GstStateChange transition);

/* sink pad overwrites */
static gboolean gst_dshowvideodec_sink_setcaps (GstPad * pad, GstCaps * caps);
static gboolean gst_dshowvideodec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_dshowvideodec_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer);

/* src pad overwrites */
static GstCaps *gst_dshowvideodec_src_getcaps (GstPad * pad);
static gboolean gst_dshowvideodec_src_setcaps (GstPad * pad, GstCaps * caps);

/* utils */
static gboolean gst_dshowvideodec_create_graph_and_filters (GstDshowVideoDec *
    vdec);
static gboolean gst_dshowvideodec_destroy_graph_and_filters (GstDshowVideoDec *
    vdec);
static gboolean gst_dshowvideodec_flush (GstDshowVideoDec * adec);
static gboolean gst_dshowvideodec_get_filter_output_format (GstDshowVideoDec *
    vdec, const GUID subtype, VIDEOINFOHEADER ** format, guint * size);


#define GUID_MEDIATYPE_VIDEO    {0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WMVV1 {0x31564d57, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WMVV2 {0x32564d57, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WMVV3 {0x33564d57, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WMVP  {0x50564d57, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WMVA  {0x41564d57, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_WVC1  {0x31435657, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_CVID  {0x64697663, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_MP4S  {0x5334504d, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_MP42  {0x3234504d, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_MP43  {0x3334504d, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_M4S2  {0x3253344d, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_XVID  {0x44495658, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_DX50  {0x30355844, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_DIVX  {0x58564944, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_DIV3  {0x33564944, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}

#define GUID_MEDIASUBTYPE_MPG4          {0x3447504d, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_MPEG1Payload  {0xe436eb81, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}}


/* output types */
#define GUID_MEDIASUBTYPE_YUY2    {0x32595559, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_YV12    {0x32315659, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }}
#define GUID_MEDIASUBTYPE_RGB32   {0xe436eb7e, 0x524f, 0x11ce, { 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 }}
#define GUID_MEDIASUBTYPE_RGB565  {0xe436eb7b, 0x524f, 0x11ce, { 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 }}

/* WMV always uses the WMV DMO */
static PreferredFilter preferred_wmv_filters[] = {
  {&CLSID_CWMVDecMediaObject, &DMOCATEGORY_VIDEO_DECODER}, {0}
};

static const GUID CLSID_AVI_DECOMPRESSOR =
  {0xCF49D4E0, 0x1115, 0x11CE,
   {0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};
static PreferredFilter preferred_cinepack_filters[] = {
  {&CLSID_AVI_DECOMPRESSOR}, {0}
};

/* Various MPEG-4 video variants */
// MPG4, mpg4, MP42, mp42
static PreferredFilter preferred_mpeg4_filters[] = {
  {&CLSID_CMpeg4DecMediaObject, &DMOCATEGORY_VIDEO_DECODER}, {0}};
// MP4S, mp4s, M4S2, m4s2
static PreferredFilter preferred_mp4s_filters[] = {
  {&CLSID_CMpeg4sDecMediaObject, &DMOCATEGORY_VIDEO_DECODER}, {0}};
// MP43, mp43
static PreferredFilter preferred_mp43_filters[] = {
  {&CLSID_CMpeg43DecMediaObject, &DMOCATEGORY_VIDEO_DECODER}, {0}};

static const GUID CLSID_MPEG_VIDEO_DECODER =
  {0xFEB50740, 0x7BEF, 0x11CE,
   {0x9B, 0xD9, 0x00, 0x00, 0xE2, 0x02, 0x59, 0x9C}};
static PreferredFilter preferred_mpeg1_filters[] = {
  {&CLSID_MPEG_VIDEO_DECODER}, {0}
};

/* video codecs array */
static const VideoCodecEntry video_dec_codecs[] = {
  {"dshowvdec_wmv1", "Windows Media Video 7",
   GST_MAKE_FOURCC ('W', 'M', 'V', '1'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WMVV1,
   "video/x-wmv, wmvversion = (int) 1",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

  {"dshowvdec_wmv2", "Windows Media Video 8",
   GST_MAKE_FOURCC ('W', 'M', 'V', '2'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WMVV2,
   "video/x-wmv, wmvversion = (int) 2",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

  {"dshowvdec_wmv3", "Windows Media Video 9",
   GST_MAKE_FOURCC ('W', 'M', 'V', '3'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WMVV3,
   "video/x-wmv, wmvversion = (int) 3, " "format = (string) WMV3",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

  {"dshowvdec_wmvp", "Windows Media Video 9 Image",
   GST_MAKE_FOURCC ('W', 'M', 'V', 'P'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WMVP,
   "video/x-wmv, wmvversion = (int) 3, " "format = (string) { WMVP, MSS1 }",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

  {"dshowvdec_wmva", "Windows Media Video 9 Advanced",
   GST_MAKE_FOURCC ('W', 'M', 'V', 'A'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WMVA,
   "video/x-wmv, wmvversion = (int) 3, " "format = (string) WMVA",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

   {"dshowvdec_wvc1", "Windows Media VC1 video",
   GST_MAKE_FOURCC ('W', 'V', 'C', '1'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_WVC1,
   "video/x-wmv, wmvversion = (int) 3, " "format = (string) WVC1",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_wmv_filters},

  {"dshowvdec_cinepak", "Cinepack",
   0x64697663,
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_CVID,
   "video/x-cinepak",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_RGB32,
   "video/x-raw, format=(string)RGB, bpp=(int)32, depth=(int)24, "
       "endianness=(int)4321, red_mask=(int)65280, "
       "green_mask=(int)16711680, blue_mask=(int)-16777216",
   preferred_cinepack_filters},

  {"dshowvdec_msmpeg41", "Microsoft ISO MPEG-4 version 1",
   GST_MAKE_FOURCC ('M', 'P', '4', 'S'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MP4S,
   "video/x-msmpeg, msmpegversion=(int)41",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mp4s_filters},

  {"dshowvdec_msmpeg42", "Microsoft ISO MPEG-4 version 2",
   GST_MAKE_FOURCC ('M', 'P', '4', '2'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MP42,
   "video/x-msmpeg, msmpegversion=(int)42",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mpeg4_filters},

  {"dshowvdec_msmpeg43", "Microsoft ISO MPEG-4 version 3",
   GST_MAKE_FOURCC ('M', 'P', '4', '3'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MP43,
   "video/x-msmpeg, msmpegversion=(int)43",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mp43_filters},

  {"dshowvdec_msmpeg4", "Microsoft ISO MPEG-4 version 1.1",
   GST_MAKE_FOURCC ('M', '4', 'S', '2'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_M4S2,
   "video/x-msmpeg, msmpegversion=(int)4",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mp4s_filters},

  {"dshowvdec_mpeg1",
   "MPEG-1 Video",
   GST_MAKE_FOURCC ('M', 'P', 'E', 'G'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MPEG1Payload,
   "video/mpeg, mpegversion= (int) 1, "
       "parsed= (boolean) true, " "systemstream= (boolean) false",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mpeg1_filters},

  {"dshowvdec_mpeg4", "MPEG-4 Video",
   GST_MAKE_FOURCC ('M', 'P', 'G', '4'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MPG4,
   "video/mpeg, msmpegversion=(int)4",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2"),
   preferred_mpeg4_filters},

  /* The rest of these have no preferred filter; windows doesn't come
   * with anything appropriate */
  {"dshowvdec_xvid", "XVID Video",
   GST_MAKE_FOURCC ('X', 'V', 'I', 'D'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_XVID,
   "video/x-xvid",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2")},

  {"dshowvdec_divx5", "DIVX 5.0 Video",
   GST_MAKE_FOURCC ('D', 'X', '5', '0'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_DX50,
   "video/x-divx, divxversion=(int)5",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2")},

  {"dshowvdec_divx4", "DIVX 4.0 Video",
   GST_MAKE_FOURCC ('D', 'I', 'V', 'X'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_DIVX,
   "video/x-divx, divxversion=(int)4",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2")},

  {"dshowvdec_divx3", "DIVX 3.0 Video",
   GST_MAKE_FOURCC ('D', 'I', 'V', '3'),
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_MP43,
   "video/x-divx, divxversion=(int)3",
   GUID_MEDIATYPE_VIDEO, GUID_MEDIASUBTYPE_YUY2,
   GST_VIDEO_CAPS_MAKE("YUY2")}
};

HRESULT VideoFakeSink::DoRenderSample(IMediaSample *pMediaSample)
{
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;
  GstDshowVideoDecClass *klass =
      (GstDshowVideoDecClass *) G_OBJECT_GET_CLASS (mDec);
  GstBuffer *buf = NULL;
  GstClockTime start, stop;
  GstMapInfo map;

  if(pMediaSample)
  {
    BYTE *pBuffer = NULL;
    LONGLONG lStart = 0, lStop = 0;
    long size = pMediaSample->GetActualDataLength();

    pMediaSample->GetPointer(&pBuffer);
    pMediaSample->GetTime(&lStart, &lStop);

    start = lStart * 100;
    stop = lStop * 100;
    /* check if this buffer is in our current segment */
    in_seg = gst_segment_clip (mDec->segment, GST_FORMAT_TIME,
        start, stop, &clip_start, &clip_stop);

    /* if the buffer is out of segment do not push it downstream */
    if (!in_seg) {
      GST_DEBUG_OBJECT (mDec,
        "buffer is out of segment, start %" GST_TIME_FORMAT " stop %"
        GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
      goto done;
    }

    /* buffer is in our segment, allocate a new out buffer and clip its
     * timestamps */
    gst_buffer_pool_acquire_buffer(mDec->buffer_pool, &buf, NULL);
    if (!buf) {
      GST_WARNING_OBJECT (mDec,
          "cannot allocate a new GstBuffer");
      goto done;
    }

    /* set buffer properties */
    GST_BUFFER_TIMESTAMP (buf) = clip_start;
    GST_BUFFER_DURATION (buf) = clip_stop - clip_start;

    gst_buffer_map(buf, &map, GST_MAP_WRITE);
    if (strstr (klass->entry->srccaps, "rgb")) {
      /* FOR RGB directshow decoder will return bottom-up BITMAP
       * There is probably a way to get top-bottom video frames from
       * the decoder...
       */
      gint line = 0;
      guint stride = mDec->width * 4;

      for (; line < mDec->height; line++) {
        memcpy (map.data + (line * stride),
            pBuffer + (size - ((line + 1) * (stride))), stride);
      }
    } else {
      memcpy (map.data, pBuffer, MIN ((unsigned int)size, map.size));
    }
    gst_buffer_unmap(buf, &map);

    GST_LOG_OBJECT (mDec,
        "push_buffer (size %d)=> pts %" GST_TIME_FORMAT " stop %" GST_TIME_FORMAT
        " duration %" GST_TIME_FORMAT, size,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf) + GST_BUFFER_DURATION (buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

    /* push the buffer downstream */
    mDec->last_ret = gst_pad_push (mDec->srcpad, buf);
  }
done:

  return S_OK;
}

HRESULT VideoFakeSink::CheckMediaType(const CMediaType *pmt)
{
  if (pmt != NULL) {
    if (*pmt == m_MediaType)
      return S_OK;
  }

  return S_FALSE;
}

static void
gst_dshowvideodec_base_init (gpointer klass)
{
  GstDshowVideoDecClass *videodec_class = (GstDshowVideoDecClass *) klass;
  GstPadTemplate *src, *sink;
  GstCaps *srccaps, *sinkcaps;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  const VideoCodecEntry *tmp;
  gpointer qdata;
  gchar *longname, *description;

  qdata = g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass), DSHOW_CODEC_QDATA);

  /* element details */
  tmp = videodec_class->entry = (VideoCodecEntry *) qdata;

  longname = g_strdup_printf ("DirectShow %s Decoder Wrapper",
      tmp->element_longname);
  description = g_strdup_printf ("DirectShow %s Decoder Wrapper",
      tmp->element_longname);

  gst_element_class_set_metadata(element_class, longname, "Codec/Decoder/Video", description,
    "Sebastien Moutte <sebastien@moutte.net>");

  g_free (longname);
  g_free (description);

  sinkcaps = gst_caps_from_string (tmp->sinkcaps);
  gst_caps_set_simple (sinkcaps,
      "width", GST_TYPE_INT_RANGE, 16, 4096,
      "height", GST_TYPE_INT_RANGE, 16, 4096,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  srccaps = gst_caps_from_string (tmp->srccaps);

  sink = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sinkcaps);
  src = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, srccaps);

  gst_element_class_add_pad_template (element_class, src);
  gst_element_class_add_pad_template (element_class, sink);

  if (sinkcaps)
    gst_caps_unref(sinkcaps);

  if (srccaps)
    gst_caps_unref(srccaps);
}

static void
gst_dshowvideodec_class_init (GstDshowVideoDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_dshowvideodec_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dshowvideodec_change_state);

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);
}

static void
gst_dshowvideodec_com_thread (GstDshowVideoDec * vdec)
{
  HRESULT res;

  g_mutex_lock (&vdec->com_init_lock);

  /* Initialize COM with a MTA for this process. This thread will
   * be the first one to enter the apartement and the last one to leave
   * it, unitializing COM properly */

  res = CoInitializeEx (0, COINIT_MULTITHREADED);
  if (res == S_FALSE)
    GST_WARNING_OBJECT (vdec, "COM has been already initialized in the same process");
  else if (res == RPC_E_CHANGED_MODE)
    GST_WARNING_OBJECT (vdec, "The concurrency model of COM has changed.");
  else
    GST_INFO_OBJECT (vdec, "COM initialized successfully");

  vdec->comInitialized = TRUE;

  /* Signal other threads waiting on this condition that COM was initialized */
  g_cond_signal (&vdec->com_initialized);

  g_mutex_unlock (&vdec->com_init_lock);

  /* Wait until the uninitialize condition is met to leave the COM apartement */
  g_mutex_lock (&vdec->com_deinit_lock);
  g_cond_wait (&vdec->com_uninitialize, &vdec->com_deinit_lock);

  CoUninitialize ();
  GST_INFO_OBJECT (vdec, "COM uninitialized successfully");
  vdec->comInitialized = FALSE;
  g_cond_signal (&vdec->com_uninitialized);
  g_mutex_unlock (&vdec->com_deinit_lock);
}

static void
gst_dshowvideodec_init (GstDshowVideoDec * vdec)
{
  GstElementClass *element_class = GST_ELEMENT_GET_CLASS (vdec);

  /* setup pads */
  vdec->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (element_class, "sink"), "sink");

  gst_pad_set_event_function (vdec->sinkpad, gst_dshowvideodec_sink_event);
  gst_pad_set_chain_function (vdec->sinkpad, gst_dshowvideodec_chain);
  gst_element_add_pad (GST_ELEMENT (vdec), vdec->sinkpad);

  vdec->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (element_class, "src"), "src");
/* needed to implement caps negotiation on our src pad */
/*  gst_pad_set_getcaps_function (vdec->srcpad, gst_dshowvideodec_src_getcaps);
  gst_pad_set_setcaps_function (vdec->srcpad, gst_dshowvideodec_src_setcaps);*/
  gst_element_add_pad (GST_ELEMENT (vdec), vdec->srcpad);

  vdec->fakesrc = NULL;
  vdec->fakesink = NULL;
  vdec->decfilter = NULL;

  vdec->last_ret = GST_FLOW_OK;

  vdec->filtergraph = NULL;
  vdec->mediafilter = NULL;
  vdec->srccaps = NULL;
  vdec->segment = gst_segment_new ();

  vdec->setup = FALSE;
  vdec->buffer_pool = NULL;

  g_mutex_init (&vdec->com_init_lock);
  g_mutex_init (&vdec->com_deinit_lock);
  g_cond_init (&vdec->com_initialized);
  g_cond_init (&vdec->com_uninitialize);
  g_cond_init (&vdec->com_uninitialized);

  g_mutex_lock (&vdec->com_init_lock);

  /* create the COM initialization thread */
  g_thread_new ("COM Init Thread", (GThreadFunc)gst_dshowvideodec_com_thread,
    vdec);

  /* wait until the COM thread signals that COM has been initialized */
  g_cond_wait (&vdec->com_initialized, &vdec->com_init_lock);
  g_mutex_unlock (&vdec->com_init_lock);
}

static void
gst_dshowvideodec_finalize (GObject * object)
{
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) (object);

  if (vdec->segment) {
    gst_segment_free (vdec->segment);
    vdec->segment = NULL;
  }

  if(vdec->buffer_pool) {
    gst_object_unref(vdec->buffer_pool);
    vdec->buffer_pool = NULL;
  }

  /* signal the COM thread that it sould uninitialize COM */
  if (vdec->comInitialized) {
    g_mutex_lock (&vdec->com_deinit_lock);
    g_cond_signal (&vdec->com_uninitialize);
    g_cond_wait (&vdec->com_uninitialized, &vdec->com_deinit_lock);
    g_mutex_unlock (&vdec->com_deinit_lock);
  }

  g_mutex_clear (&vdec->com_init_lock);
  g_mutex_clear (&vdec->com_deinit_lock);
  g_cond_clear (&vdec->com_initialized);
  g_cond_clear (&vdec->com_uninitialize);
  g_cond_clear (&vdec->com_uninitialized);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_dshowvideodec_change_state (GstElement * element, GstStateChange transition)
{
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_dshowvideodec_create_graph_and_filters (vdec))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (!gst_dshowvideodec_destroy_graph_and_filters (vdec))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static gboolean
gst_dshowvideodec_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = FALSE;
  HRESULT hres;
  GstStructure *s = gst_caps_get_structure (caps, 0);
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) gst_pad_get_parent (pad);
  GstDshowVideoDecClass *klass =
      (GstDshowVideoDecClass *) G_OBJECT_GET_CLASS (vdec);
  GstBuffer *extradata = NULL;
  gsize extra_size;
  const GValue *v = NULL;
  guint size = 0;
  GstCaps *caps_out = NULL;
  AM_MEDIA_TYPE output_mediatype, input_mediatype;
  VIDEOINFOHEADER *input_vheader = NULL, *output_vheader = NULL;
  IPinPtr output_pin;
  IPinPtr input_pin;
  IBaseFilter *srcfilter = NULL;
  IBaseFilter *sinkfilter = NULL;
  const GValue *fps, *par;
  GstQuery *query = NULL;
  GstBufferPool *pool = NULL;
  GstStructure *pool_config = NULL;
  guint pool_size, pool_min, pool_max;

  /* read data */
  if (!gst_structure_get_int (s, "width", &vdec->width) ||
      !gst_structure_get_int (s, "height", &vdec->height)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("error getting video width or height from caps"), (NULL));
    goto end;
  }
  fps = gst_structure_get_value (s, "framerate");
  if (fps) {
    vdec->fps_n = gst_value_get_fraction_numerator (fps);
    vdec->fps_d = gst_value_get_fraction_denominator (fps);
  }
  else {
    /* Invent a sane default framerate; the timestamps matter
     * more anyway. */
    vdec->fps_n = 25;
    vdec->fps_d = 1;
  }

  par = gst_structure_get_value (s, "pixel-aspect-ratio");
  if (par) {
    vdec->par_n = gst_value_get_fraction_numerator (par);
    vdec->par_d = gst_value_get_fraction_denominator (par);
  }
  else {
    vdec->par_n = vdec->par_d = 1;
  }

  if ((v = gst_structure_get_value (s, "codec_data"))) {
    extradata = gst_value_get_buffer (v);
    extra_size = gst_buffer_get_size(extradata);
  }

  /* define the input type format */
  memset (&input_mediatype, 0, sizeof (AM_MEDIA_TYPE));
  input_mediatype.majortype = klass->entry->input_majortype;
  input_mediatype.subtype = klass->entry->input_subtype;
  input_mediatype.bFixedSizeSamples = FALSE;
  input_mediatype.bTemporalCompression = TRUE;

  if (strstr (klass->entry->sinkcaps, "video/mpeg, mpegversion= (int) 1")) {
    size =
        sizeof (MPEG1VIDEOINFO) + (extradata ? extra_size - 1 : 0);
    input_vheader = (VIDEOINFOHEADER *)g_malloc0 (size);

    input_vheader->bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    if (extradata) {
      MPEG1VIDEOINFO *mpeg_info = (MPEG1VIDEOINFO *) input_vheader;

      gst_buffer_extract(extradata, 0, mpeg_info->bSequenceHeader, extra_size);
      mpeg_info->cbSequenceHeader = extra_size;
    }
    input_mediatype.formattype = FORMAT_MPEGVideo;
  } else {
    size =
        sizeof (VIDEOINFOHEADER) + (extradata ? extra_size : 0);
    input_vheader = (VIDEOINFOHEADER *)g_malloc0 (size);
    input_vheader->bmiHeader.biSize = sizeof (BITMAPINFOHEADER);

    if (extradata) {            /* Codec data is appended after our header */
      gst_buffer_extract(extradata, 0,
        ((guchar *) input_vheader) + sizeof (VIDEOINFOHEADER), extra_size);
      input_vheader->bmiHeader.biSize += extra_size;
    }
    input_mediatype.formattype = FORMAT_VideoInfo;
  }

  input_vheader->rcSource.top = input_vheader->rcSource.left = 0;
  input_vheader->rcSource.right = vdec->width;
  input_vheader->rcSource.bottom = vdec->height;
  input_vheader->rcTarget = input_vheader->rcSource;
  input_vheader->bmiHeader.biWidth = vdec->width;
  input_vheader->bmiHeader.biHeight = vdec->height;
  input_vheader->bmiHeader.biPlanes = 1;
  input_vheader->bmiHeader.biBitCount = 16;
  input_vheader->bmiHeader.biCompression = klass->entry->format;
  input_vheader->bmiHeader.biSizeImage =
      (vdec->width * vdec->height) * (input_vheader->bmiHeader.biBitCount / 8);

  input_mediatype.cbFormat = size;
  input_mediatype.pbFormat = (BYTE *) input_vheader;
  input_mediatype.lSampleSize = input_vheader->bmiHeader.biSizeImage;

  vdec->fakesrc->GetOutputPin()->SetMediaType(&input_mediatype);

  /* set the sample size for fakesrc filter to the output buffer size */
  vdec->fakesrc->GetOutputPin()->SetSampleSize(input_mediatype.lSampleSize);

  /* connect our fake src to decoder */
  hres = vdec->fakesrc->QueryInterface(IID_IBaseFilter,
      (void **) &srcfilter);
  if (FAILED (hres)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
      ("Can't QT fakesrc to IBaseFilter: %x", hres), (NULL));
    goto end;
  }

  output_pin = gst_dshow_util_get_pin_from_filter (srcfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't get output pin from our directshow fakesrc filter"), (NULL));
    goto end;
  }
  input_pin = gst_dshow_util_get_pin_from_filter (vdec->decfilter, PINDIR_INPUT);
  if (!input_pin) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't get input pin from decoder filter"), (NULL));
    goto end;
  }

  hres = vdec->filtergraph->ConnectDirect (output_pin, input_pin, NULL);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't connect fakesrc with decoder (error=%x)", hres), (NULL));
    goto end;
  }

  /* get decoder output video format */
  if (!gst_dshowvideodec_get_filter_output_format (vdec,
          klass->entry->output_subtype, &output_vheader, &size)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't get decoder output video format"), (NULL));
    goto end;
  }

  memset (&output_mediatype, 0, sizeof (AM_MEDIA_TYPE));
  output_mediatype.majortype = klass->entry->output_majortype;
  output_mediatype.subtype = klass->entry->output_subtype;
  output_mediatype.bFixedSizeSamples = TRUE;
  output_mediatype.bTemporalCompression = FALSE;
  output_mediatype.lSampleSize = output_vheader->bmiHeader.biSizeImage;
  output_mediatype.formattype = FORMAT_VideoInfo;
  output_mediatype.cbFormat = size;
  output_mediatype.pbFormat = (BYTE *) output_vheader;

  vdec->fakesink->SetMediaType (&output_mediatype);

  /* connect decoder to our fake sink */
  output_pin = gst_dshow_util_get_pin_from_filter (vdec->decfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't get output pin from our decoder filter"), (NULL));
    goto end;
  }

  hres = vdec->fakesink->QueryInterface(IID_IBaseFilter,
      (void **) &sinkfilter);
  if (FAILED (hres)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
      ("Can't QT fakesink to IBaseFilter: %x", hres), (NULL));
    goto end;
  }

  input_pin = gst_dshow_util_get_pin_from_filter (sinkfilter, PINDIR_INPUT);
  if (!input_pin) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't get input pin from our directshow fakesink filter"), (NULL));
    goto end;
  }

  hres = vdec->filtergraph->ConnectDirect(output_pin, input_pin,
      &output_mediatype);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't connect decoder with fakesink (error=%x)", hres), (NULL));
    goto end;
  }

  /* negotiate output */
  caps_out = gst_caps_from_string (klass->entry->srccaps);
  gst_caps_set_simple (caps_out,
      "width", G_TYPE_INT, vdec->width,
      "height", G_TYPE_INT, vdec->height, NULL);

  if (vdec->fps_n && vdec->fps_d) {
      gst_caps_set_simple (caps_out,
          "framerate", GST_TYPE_FRACTION, vdec->fps_n, vdec->fps_d, NULL);
  }

  gst_caps_set_simple (caps_out,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, vdec->par_n, vdec->par_d, NULL);

  if (!gst_pad_set_caps (vdec->srcpad, caps_out)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Failed to negotiate output"), (NULL));
    goto end;
  }

  /* request or create a buffer pool */
  if (vdec->buffer_pool) {
    gst_object_unref (vdec->buffer_pool);
  }

  query = gst_query_new_allocation(caps_out, TRUE);
  gst_pad_peer_query(vdec->srcpad, query);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &pool_size, &pool_min,
      &pool_max);
  }
  else {
    pool = NULL;
    pool_size = output_mediatype.lSampleSize;
    pool_min = 1;
    pool_max = 0;
  }

  if (pool == NULL) {
    pool = gst_video_buffer_pool_new ();
  }

  if (!pool) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Could not create buffer bool"), (NULL));
    goto end;
  }

  pool_config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (pool_config, caps_out, pool_size,
    pool_min, pool_max);
  gst_buffer_pool_set_config (pool, pool_config);

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
      ("Failed set buffer pool active"), (NULL));
    goto end;
  }

  vdec->buffer_pool = pool;

  hres = vdec->mediafilter->Run (-1);
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("Can't run the directshow graph (error=%d)", hres), (NULL));
    goto end;
  }

  ret = TRUE;
end:
  if (caps_out)
    gst_caps_unref (caps_out);
  gst_object_unref (vdec);
  g_free (input_vheader);
  if (srcfilter)
    srcfilter->Release();
  if (sinkfilter)
    sinkfilter->Release();
  if (query)
    gst_query_unref(query);
  return ret;
}

static gboolean
gst_dshowvideodec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = TRUE;
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) gst_pad_get_parent (pad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      GstCaps *caps;
      gst_event_parse_caps(event, &caps);
      ret = gst_dshowvideodec_sink_setcaps(pad, caps);
      break;

    case GST_EVENT_FLUSH_STOP:
      gst_dshowvideodec_flush (vdec);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);

      /* save the new segment in our local current segment */
      gst_segment_copy_into(segment, vdec->segment);

      GST_CAT_DEBUG_OBJECT (dshowvideodec_debug, vdec,
          "new segment received => start=%" GST_TIME_FORMAT " stop=%"
          GST_TIME_FORMAT, GST_TIME_ARGS (vdec->segment->start),
          GST_TIME_ARGS (vdec->segment->stop));

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  gst_object_unref (vdec);

  return ret;
}

static GstFlowReturn
gst_dshowvideodec_chain (GstPad * pad, GstObject *parent, GstBuffer * buffer)
{
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) gst_pad_get_parent (pad);
  bool discont = FALSE;
  GstClockTime stop;
  GstMapInfo map;

  if (!vdec->setup) {
    /* we are not setup */
    GST_WARNING_OBJECT (vdec, "Decoder not set up, failing");
    vdec->last_ret = GST_FLOW_FLUSHING;
    goto beach;
  }

  if (vdec->last_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (vdec, "last decoding iteration generated a fatal error "
        "%s", gst_flow_get_name (vdec->last_ret));
    goto beach;
  }

  /* check if duration is valid and use duration only when it's valid
     /* because dshow is not decoding frames having stop smaller than start */
  if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
  } else {
    stop = GST_BUFFER_TIMESTAMP (buffer);
  }

  GST_CAT_LOG_OBJECT (dshowvideodec_debug, vdec,
      "chain (size %d)=> pts %" GST_TIME_FORMAT " stop %" GST_TIME_FORMAT,
      gst_buffer_get_size (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (stop));

  /* if the incoming buffer has discont flag set => flush decoder data */
  if (buffer && GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
    GST_CAT_DEBUG_OBJECT (dshowvideodec_debug, vdec,
        "this buffer has a DISCONT flag (%" GST_TIME_FORMAT "), flushing",
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
    gst_dshowvideodec_flush (vdec);
    discont = TRUE;
  }

  gst_buffer_map(buffer, &map, GST_MAP_READ);
  /* push the buffer to the directshow decoder */
  vdec->fakesrc->GetOutputPin()->PushBuffer(
      map.data, GST_BUFFER_TIMESTAMP (buffer), stop,
      map.size, discont);
  gst_buffer_unmap(buffer, &map);

beach:
  gst_buffer_unref (buffer);
  gst_object_unref (vdec);

  return vdec->last_ret;
}

static GstCaps *
gst_dshowvideodec_src_getcaps (GstPad * pad)
{
  GstDshowVideoDec *vdec = (GstDshowVideoDec *) gst_pad_get_parent (pad);
  GstCaps *caps = NULL;

  if (!vdec->srccaps)
    vdec->srccaps = gst_caps_new_empty ();

  if (vdec->decfilter) {
    IPinPtr output_pin;
    IEnumMediaTypesPtr enum_mediatypes;
    HRESULT hres;
    ULONG fetched;

    output_pin = gst_dshow_util_get_pin_from_filter (vdec->decfilter, PINDIR_OUTPUT);
    if (!output_pin) {
      GST_ELEMENT_ERROR (vdec, STREAM, FAILED,
          ("failed getting output pin from the decoder"), (NULL));
      goto beach;
    }

    hres = output_pin->EnumMediaTypes (&enum_mediatypes);
    if (hres == S_OK && enum_mediatypes) {
      AM_MEDIA_TYPE *mediatype = NULL;

      enum_mediatypes->Reset();
      while (hres =
          enum_mediatypes->Next(1, &mediatype, &fetched),
          hres == S_OK)
      {
        VIDEOINFOHEADER *video_info;
        GstCaps *mediacaps = NULL;

        /* RGB24 */
        if (IsEqualGUID (mediatype->subtype, MEDIASUBTYPE_RGB24) &&
            IsEqualGUID (mediatype->formattype, FORMAT_VideoInfo))
        {
          gint fps_n;

          video_info = (VIDEOINFOHEADER *) mediatype->pbFormat;

          if (video_info->AvgTimePerFrame > 0) {
            fps_n = 10000000 / video_info->AvgTimePerFrame;
          } else {
            fps_n = 30;
          }

          /* ffmpegcolorspace handles RGB24 in BIG_ENDIAN */
          mediacaps = gst_caps_new_simple ("video/x-raw-rgb",
              "bpp", G_TYPE_INT, 24,
              "depth", G_TYPE_INT, 24,
              "width", G_TYPE_INT, video_info->bmiHeader.biWidth,
              "height", G_TYPE_INT, video_info->bmiHeader.biHeight,
              "framerate", GST_TYPE_FRACTION, fps_n, 1, "endianness",
              G_TYPE_INT, G_BIG_ENDIAN, "red_mask", G_TYPE_INT, 255,
              "green_mask", G_TYPE_INT, 65280, "blue_mask", G_TYPE_INT,
              16711680, NULL);

          if (mediacaps) {
            vdec->mediatypes = g_list_append (vdec->mediatypes, mediatype);
            gst_caps_append (vdec->srccaps, mediacaps);
          } else {
            DeleteMediaType (mediatype);
          }
        } else {
          DeleteMediaType (mediatype);
        }

      }
    }
  }

  if (vdec->srccaps)
    caps = gst_caps_ref (vdec->srccaps);

beach:
  gst_object_unref (vdec);

  return caps;
}

static gboolean
gst_dshowvideodec_src_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = FALSE;

  return ret;
}

static gboolean
gst_dshowvideodec_flush (GstDshowVideoDec * vdec)
{
  if (!vdec->fakesrc)
    return FALSE;

  /* flush dshow decoder and reset timestamp */
  vdec->fakesrc->GetOutputPin()->Flush();
  vdec->last_ret = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_dshowvideodec_get_filter_output_format (GstDshowVideoDec * vdec,
    const GUID subtype, VIDEOINFOHEADER ** format, guint * size)
{
  IPinPtr output_pin;
  IEnumMediaTypesPtr enum_mediatypes;
  HRESULT hres;
  ULONG fetched;
  BOOL ret = FALSE;

  if (!vdec->decfilter)
    return FALSE;

  output_pin = gst_dshow_util_get_pin_from_filter (vdec->decfilter, PINDIR_OUTPUT);
  if (!output_pin) {
    GST_ELEMENT_ERROR (vdec, CORE, NEGOTIATION,
        ("failed getting output pin from the decoder"), (NULL));
    return FALSE;
  }

  hres = output_pin->EnumMediaTypes (&enum_mediatypes);
  if (hres == S_OK && enum_mediatypes) {
    AM_MEDIA_TYPE *mediatype = NULL;

    enum_mediatypes->Reset();
    while (hres =
        enum_mediatypes->Next(1, &mediatype, &fetched),
        hres == S_OK)
    {
      if (IsEqualGUID (mediatype->subtype, subtype) &&
          IsEqualGUID (mediatype->formattype, FORMAT_VideoInfo))
      {
        *size = mediatype->cbFormat;
        *format = (VIDEOINFOHEADER *)g_malloc0 (*size);
        memcpy (*format, mediatype->pbFormat, *size);
        ret = TRUE;
      }
      DeleteMediaType (mediatype);
      if (ret)
        break;
    }
  }

  return ret;
}

static gboolean
gst_dshowvideodec_create_graph_and_filters (GstDshowVideoDec * vdec)
{
  HRESULT hres = S_FALSE;
  GstDshowVideoDecClass *klass =
      (GstDshowVideoDecClass *) G_OBJECT_GET_CLASS (vdec);
  IBaseFilter *srcfilter = NULL;
  IBaseFilter *sinkfilter = NULL;
  gboolean ret = FALSE;

  /* create the filter graph manager object */
  hres = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_INPROC,
      IID_IFilterGraph, (LPVOID *) & vdec->filtergraph);
  if (hres != S_OK || !vdec->filtergraph) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED, ("Can't create an instance "
            "of the directshow graph manager (error=%d)", hres), (NULL));
    goto error;
  }

  hres = vdec->filtergraph->QueryInterface(IID_IMediaFilter,
      (void **) &vdec->mediafilter);
  if (hres != S_OK || !vdec->mediafilter) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED,
        ("Can't get IMediacontrol interface "
            "from the graph manager (error=%d)", hres), (NULL));
    goto error;
  }

  /* create fake src filter */
  vdec->fakesrc = new FakeSrc();
  /* Created with a refcount of zero, so increment that */
  vdec->fakesrc->AddRef();

  hres = vdec->fakesrc->QueryInterface(IID_IBaseFilter,
      (void **) &srcfilter);
  if (FAILED (hres)) {
    GST_WARNING_OBJECT (vdec, "Failed to QI fakesrc to IBaseFilter");
    goto error;
  }

  /* search a decoder filter and create it */
  vdec->decfilter = gst_dshow_util_find_filter (
          klass->entry->input_majortype,
          klass->entry->input_subtype,
          klass->entry->output_majortype,
          klass->entry->output_subtype,
          klass->entry->preferred_filters);
  if (vdec->decfilter == NULL) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED, ("Can't create an instance "
            "of the decoder filter"), (NULL));
    goto error;
  }

  /* create fake sink filter */
  vdec->fakesink = new VideoFakeSink(vdec);
  /* Created with a refcount of zero, so increment that */
  vdec->fakesink->AddRef();

  hres = vdec->fakesink->QueryInterface(IID_IBaseFilter,
      (void **) &sinkfilter);
  if (FAILED (hres)) {
    GST_WARNING_OBJECT (vdec, "Failed to QI fakesink to IBaseFilter");
    goto error;
  }

  /* add filters to the graph */
  hres = vdec->filtergraph->AddFilter (srcfilter, L"src");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED, ("Can't add fakesrc filter "
            "to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  hres = vdec->filtergraph->AddFilter(vdec->decfilter, L"decoder");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED, ("Can't add decoder filter "
            "to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  hres = vdec->filtergraph->AddFilter(sinkfilter, L"sink");
  if (hres != S_OK) {
    GST_ELEMENT_ERROR (vdec, STREAM, FAILED, ("Can't add fakesink filter "
            "to the graph (error=%d)", hres), (NULL));
    goto error;
  }

  vdec->setup = TRUE;

  ret = TRUE;

done:
  if (srcfilter)
    srcfilter->Release();
  if (sinkfilter)
    sinkfilter->Release();
  return ret;

error:
  if (vdec->fakesrc) {
    vdec->fakesrc->Release();
    vdec->fakesrc = NULL;
  }
  if (vdec->decfilter) {
    vdec->decfilter->Release();
    vdec->decfilter = NULL;
  }
  if (vdec->fakesink) {
    vdec->fakesink->Release();
    vdec->fakesink = NULL;
  }
  if (vdec->mediafilter) {
    vdec->mediafilter->Release();
    vdec->mediafilter = NULL;
  }
  if (vdec->filtergraph) {
    vdec->filtergraph->Release();
    vdec->filtergraph = NULL;
  }

  goto done;
}

static gboolean
gst_dshowvideodec_destroy_graph_and_filters (GstDshowVideoDec * vdec)
{
  HRESULT hres;

  if (vdec->mediafilter) {
    vdec->mediafilter->Stop();
  }

  if (vdec->fakesrc) {
    if (vdec->filtergraph) {
      IBaseFilter *filter;
      hres = vdec->fakesrc->QueryInterface(IID_IBaseFilter,
          (void **) &filter);
      if (SUCCEEDED (hres)) {
        vdec->filtergraph->RemoveFilter(filter);
        filter->Release();
      }
    }

    vdec->fakesrc->Release();
    vdec->fakesrc = NULL;
  }
  if (vdec->decfilter) {
    if (vdec->filtergraph)
      vdec->filtergraph->RemoveFilter(vdec->decfilter);
    vdec->decfilter->Release();
    vdec->decfilter = NULL;
  }
  if (vdec->fakesink) {
    if (vdec->filtergraph) {
      IBaseFilter *filter;
      hres = vdec->fakesink->QueryInterface(IID_IBaseFilter,
          (void **) &filter);
      if (SUCCEEDED (hres)) {
        vdec->filtergraph->RemoveFilter(filter);
        filter->Release();
      }
    }

    vdec->fakesink->Release();
    vdec->fakesink = NULL;
  }
  if (vdec->mediafilter) {
    vdec->mediafilter->Release();
    vdec->mediafilter = NULL;
  }
  if (vdec->filtergraph) {
    vdec->filtergraph->Release();
    vdec->filtergraph = NULL;
  }

  vdec->setup = FALSE;

  return TRUE;
}

gboolean
dshow_vdec_register (GstPlugin * plugin)
{
  GTypeInfo info = {
    sizeof (GstDshowVideoDecClass),
    (GBaseInitFunc) gst_dshowvideodec_base_init,
    NULL,
    (GClassInitFunc) gst_dshowvideodec_class_init,
    NULL,
    NULL,
    sizeof (GstDshowVideoDec),
    0,
    (GInstanceInitFunc) gst_dshowvideodec_init,
  };
  gint i;
  HRESULT hr;

  GST_DEBUG_CATEGORY_INIT (dshowvideodec_debug, "dshowvideodec", 0,
      "Directshow filter video decoder");

  hr = CoInitialize (0);

  for (i = 0; i < sizeof (video_dec_codecs) / sizeof (VideoCodecEntry); i++) {
    GType type;
    IBaseFilterPtr filter;
    guint rank = GST_RANK_MARGINAL;

    filter = gst_dshow_util_find_filter (
            video_dec_codecs[i].input_majortype,
            video_dec_codecs[i].input_subtype,
            video_dec_codecs[i].output_majortype,
            video_dec_codecs[i].output_subtype,
            video_dec_codecs[i].preferred_filters);
    if (filter != NULL) {

      if (video_dec_codecs[i].format == GST_MAKE_FOURCC ('W', 'V', 'C', '1')) {
        /* FFMPEG WVC1 decoder sucks, get higher priority for ours */
        rank = GST_RANK_MARGINAL + 2;
      }
      GST_DEBUG ("Registering %s with rank %u", video_dec_codecs[i].element_name, rank);

      type = g_type_register_static (GST_TYPE_ELEMENT,
          video_dec_codecs[i].element_name, &info, (GTypeFlags)0);
      g_type_set_qdata (type, DSHOW_CODEC_QDATA, (gpointer) (video_dec_codecs + i));
      if (!gst_element_register (plugin, video_dec_codecs[i].element_name, rank, type)) {
        return FALSE;
      }
      GST_DEBUG ("Registered %s", video_dec_codecs[i].element_name);
    } else {
      GST_DEBUG ("Element %s not registered "
        "(the format is not supported by the system)",
        video_dec_codecs[i].element_name);
    }
  }

  if (SUCCEEDED(hr))
    CoUninitialize ();

  return TRUE;
}
