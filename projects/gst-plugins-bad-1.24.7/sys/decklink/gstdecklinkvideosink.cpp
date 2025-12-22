/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-decklinkvideosink
 * @short_description: Outputs Video to a BlackMagic DeckLink Device
 *
 * Playout Video to a BlackMagic DeckLink Device.
 *
 * ## Sample pipeline
 * |[
 * gst-launch-1.0 \
 *   videotestsrc ! \
 *   decklinkvideosink device-number=0 mode=1080p25
 * ]|
 * Playout a 1080p25 test-video to the SDI-Out of Card 0. Devices are numbered
 * starting with 0.
 *
 * ## Duplex-Mode
 * Certain DeckLink Cards like the Duo2 or the Quad2 contain two or four
 * independent SDI units with two connectors each. These units can operate either
 * in half- or in full-duplex mode.
 *
 * The Duplex-Mode of a Card can be configured using the `duplex-mode`-Property.
 * Cards that to not support Duplex-Modes are not influenced by the property.
 *
 * ### Half-Duplex-Mode (default)
 * By default decklinkvideosink will configure them into half-duplex mode, so that
 * each connector acts as if it were an independent DeckLink Card which can either
 * be used as an Input or as an Output. In this mode the Duo2 can be used as as 4 SDI
 * In-/Outputs and the Quad2 as 8 SDI In-/Outputs.
 *
 * |[
 *  gst-launch-1.0 \
 *    videotestsrc foreground-color=0x00ff0000 ! decklinkvideosink device-number=0 mode=1080p25 \
 *    videotestsrc foreground-color=0x0000ff00 ! decklinkvideosink device-number=1 mode=1080p25 \
 *    videotestsrc foreground-color=0x000000ff ! decklinkvideosink device-number=2 mode=1080p25 \
 *    videotestsrc foreground-color=0x00ffffff ! decklinkvideosink device-number=3 mode=1080p25
 * ]|
 * Playout four Test-Screen with colored Snow on the first four units in the System
 * (ie. the Connectors 1-4 of a Duo2 unit).
 *
 * |[
 *  gst-launch-1.0 \
 *    videotestsrc is-live=true foreground-color=0x0000ff00 ! decklinkvideosink device-number=0 mode=1080p25 \
 *    decklinkvideosrc device-number=1 mode=1080p25 ! autovideosink \
 *    decklinkvideosrc device-number=2 mode=1080p25 ! autovideosink \
 *    videotestsrc is-live=true foreground-color=0x00ff0000 ! decklinkvideosink device-number=3 mode=1080p25
 * ]|
 * Capture 1080p25 from the second and third unit in the System,
 * Playout a Test-Screen with colored Snow on the first and fourth unit
 * (ie. the Connectors 1-4 of a Duo2 unit).
 *
 * ### Device-Number-Mapping in Half-Duplex-Mode
 * The device-number to connector-mapping is as follows for the Duo2
 * - `device-number=0` SDI1
 * - `device-number=1` SDI3
 * - `device-number=2` SDI2
 * - `device-number=3` SDI4
 *
 * And for the Quad2
 * - `device-number=0` SDI1
 * - `device-number=1` SDI3
 * - `device-number=2` SDI5
 * - `device-number=3` SDI7
 * - `device-number=4` SDI2
 * - `device-number=5` SDI4
 * - `device-number=6` SDI6
 * - `device-number=7` SDI8
 *
 * ### Full-Duplex-Mode
 * When operating in full-duplex mode, two connectors of a unit are combined to
 * a single device, performing keying with the second connection.
 *
 * ### Device-Number-Mapping in Full-Duplex-Mode
 * The device-number to connector-mapping in full-duplex-mode is as follows for the Duo2
 * - `device-number=0` SDI1 primary, SDI2 secondary
 * - `device-number=1` SDI3 primaty, SDI4 secondary
 *
 * And for the Quad2
 * - `device-number=0` SDI1 primary, SDI2 secondary
 * - `device-number=1` SDI3 primaty, SDI4 secondary
 * - `device-number=2` SDI5 primary, SDI6 secondary
 * - `device-number=3` SDI7 primary, SDI8 secondary
 *
 * ## Keying
 * Keying is the process of overlaing Video with an Alpha-Channel on top of an
 * existing Video-Stream. The Duo2 and Quad2-Cards can perform two different
 * Keying-Modes when operated in full-duplex mode. Both modes expect Video with
 * an Alpha-Channel.
 *
 * ### Internal Keyer
 * In internal Keying-Mode the primary port becomes an Input and the secondary port
 * an Output. The unit overlays Video played back from the Computer onto the Input
 * and outputs the combined Video-Stream to the Output.
 *
 * |[
 * gst-launch-1.0 \
 *  videotestsrc foreground-color=0x00000000 background-color=0x00000000 ! \
 *  video/x-raw,format=BGRA,width=1920,height=1080 ! \
 *  decklinkvideosink device-number=0 duplex-mode=full keyer-mode=internal video-format=8bit-bgra mode=1080p25
 * ]|
 *
 * ### External Keyer
 * In external Keying-Mode the primary port outputs the alpha-chanel as the
 * luma-value (key-channel). Transparent pixels are black, opaque pixels are white.
 * The RGB-Component of the Video are output on the secondary channel.
 *
 * |[
 * gst-launch-1.0 \
 *  videotestsrc foreground-color=0x00000000 background-color=0x00000000 ! \
 *  video/x-raw,format=BGRA,width=1920,height=1080 ! \
 *  decklinkvideosink device-number=0 duplex-mode=full keyer-mode=external video-format=8bit-bgra mode=1080p25
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkvideosink.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_decklink_video_sink_debug);
#define GST_CAT_DEFAULT gst_decklink_video_sink_debug

#define DEFAULT_PERSISTENT_ID (-1)

class GStreamerVideoOutputCallback:public IDeckLinkVideoOutputCallback
{
public:
  GStreamerVideoOutputCallback (GstDecklinkVideoSink * sink)
  :IDeckLinkVideoOutputCallback (), m_refcount (1)
  {
    m_sink = GST_DECKLINK_VIDEO_SINK_CAST (gst_object_ref (sink));
    g_mutex_init (&m_mutex);
  }

  virtual HRESULT WINAPI QueryInterface (REFIID, LPVOID *)
  {
    return E_NOINTERFACE;
  }

  virtual ULONG WINAPI AddRef (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount++;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);

    return ret;
  }

  virtual ULONG WINAPI Release (void)
  {
    ULONG ret;

    g_mutex_lock (&m_mutex);
    m_refcount--;
    ret = m_refcount;
    g_mutex_unlock (&m_mutex);

    if (ret == 0) {
      delete this;
    }

    return ret;
  }

  virtual HRESULT WINAPI ScheduledFrameCompleted (IDeckLinkVideoFrame *
      completedFrame, BMDOutputFrameCompletionResult result)
  {
    switch (result) {
      case bmdOutputFrameCompleted:
        GST_LOG_OBJECT (m_sink, "Completed frame %p", completedFrame);
        break;
      case bmdOutputFrameDisplayedLate:
        GST_INFO_OBJECT (m_sink, "Late Frame %p", completedFrame);
        break;
      case bmdOutputFrameDropped:
        GST_INFO_OBJECT (m_sink, "Dropped Frame %p", completedFrame);
        break;
      case bmdOutputFrameFlushed:
        GST_DEBUG_OBJECT (m_sink, "Flushed Frame %p", completedFrame);
        break;
      default:
        GST_INFO_OBJECT (m_sink, "Unknown Frame %p: %d", completedFrame,
            (gint) result);
        break;
    }

    return S_OK;
  }

  virtual HRESULT WINAPI ScheduledPlaybackHasStopped (void)
  {
    GST_LOG_OBJECT (m_sink, "Scheduled playback stopped");

    if (m_sink->output) {
      g_mutex_lock (&m_sink->output->lock);
      g_cond_signal (&m_sink->output->cond);
      g_mutex_unlock (&m_sink->output->lock);
    }

    return S_OK;
  }

  virtual ~ GStreamerVideoOutputCallback () {
    gst_object_unref (m_sink);
    g_mutex_clear (&m_mutex);
  }

private:
  GstDecklinkVideoSink * m_sink;
  GMutex m_mutex;
  gint m_refcount;
};

class GstDecklinkTimecode:public IDeckLinkTimecode
{
public:
  GstDecklinkTimecode (GstVideoTimeCode *
      timecode):m_timecode (gst_video_time_code_copy (timecode)), m_refcount (1)
  {
  }

  virtual BMDTimecodeBCD STDMETHODCALLTYPE GetBCD (void)
  {
    BMDTimecodeBCD bcd = 0;

    bcd |= (m_timecode->frames % 10) << 0;
    bcd |= ((m_timecode->frames / 10) & 0x0f) << 4;
    bcd |= (m_timecode->seconds % 10) << 8;
    bcd |= ((m_timecode->seconds / 10) & 0x0f) << 12;
    bcd |= (m_timecode->minutes % 10) << 16;
    bcd |= ((m_timecode->minutes / 10) & 0x0f) << 20;
    bcd |= (m_timecode->hours % 10) << 24;
    bcd |= ((m_timecode->hours / 10) & 0x0f) << 28;

    if (m_timecode->config.fps_n == 24 && m_timecode->config.fps_d == 1)
      bcd |= 0x0 << 30;
    else if (m_timecode->config.fps_n == 25 && m_timecode->config.fps_d == 1)
      bcd |= 0x1 << 30;
    else if (m_timecode->config.fps_n == 30 && m_timecode->config.fps_d == 1001)
      bcd |= 0x2 << 30;
    else if (m_timecode->config.fps_n == 30 && m_timecode->config.fps_d == 1)
      bcd |= 0x3 << 30;

    return bcd;
  }

  virtual HRESULT STDMETHODCALLTYPE GetComponents (uint8_t * hours,
      uint8_t * minutes, uint8_t * seconds, uint8_t * frames)
  {
    *hours = m_timecode->hours;
    *minutes = m_timecode->minutes;
    *seconds = m_timecode->seconds;
    *frames = m_timecode->frames;

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetString (COMSTR_T * timecode)
  {
    COMSTR_T s = (COMSTR_T) gst_video_time_code_to_string (m_timecode);
    CONVERT_TO_COM_STRING (s);
    *timecode = s;
    return S_OK;
  }

  virtual BMDTimecodeFlags STDMETHODCALLTYPE GetFlags (void)
  {
    BMDTimecodeFlags flags = (BMDTimecodeFlags) 0;

    if (((GstVideoTimeCodeFlags) (m_timecode->
                config.flags)) & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME)
      flags = (BMDTimecodeFlags) (flags | bmdTimecodeIsDropFrame);
    else
      flags = (BMDTimecodeFlags) (flags | bmdTimecodeFlagDefault);
    if (m_timecode->field_count == 2)
      flags = (BMDTimecodeFlags) (flags | bmdTimecodeFieldMark);

    return flags;
  }

  virtual HRESULT STDMETHODCALLTYPE GetTimecodeUserBits (BMDTimecodeUserBits *
      userBits)
  {
    *userBits = 0;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID *)
  {
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef (void)
  {
    ULONG ret;

    ret = g_atomic_int_add (&m_refcount, 1) + 1;

    return ret;
  }

  virtual ULONG STDMETHODCALLTYPE Release (void)
  {
    ULONG ret;

    ret = g_atomic_int_add (&m_refcount, -1);
    if (ret == 1) {
      delete this;
    }

    return ret - 1;
  }

private:
  GstVideoTimeCode * m_timecode;
  int m_refcount;

  virtual ~ GstDecklinkTimecode () {
    if (m_timecode) {
      gst_video_time_code_free (m_timecode);
    }
  }
};

class GstDecklinkVideoFrame:public IDeckLinkVideoFrame
{
public:
  GstDecklinkVideoFrame (GstVideoFrame * frame):m_frame (0),
      m_dframe (0), m_ancillary (0), m_timecode (0), m_refcount (1)
  {
    m_frame = g_new0 (GstVideoFrame, 1);
    *m_frame = *frame;
  }

  GstDecklinkVideoFrame (IDeckLinkMutableVideoFrame * dframe):m_frame (0),
      m_dframe (dframe), m_ancillary (0), m_timecode (0), m_refcount (1)
  {
  }

  virtual long STDMETHODCALLTYPE GetWidth (void)
  {
    return m_frame ? GST_VIDEO_FRAME_WIDTH (m_frame) : m_dframe->GetWidth ();
  }
  virtual long STDMETHODCALLTYPE GetHeight (void)
  {
    return m_frame ? GST_VIDEO_FRAME_HEIGHT (m_frame) : m_dframe->GetHeight ();
  }
  virtual long STDMETHODCALLTYPE GetRowBytes (void)
  {
    return m_frame ? GST_VIDEO_FRAME_PLANE_STRIDE (m_frame,
        0) : m_dframe->GetRowBytes ();
  }
  virtual BMDPixelFormat STDMETHODCALLTYPE GetPixelFormat (void)
  {
    if (m_dframe)
      return m_dframe->GetPixelFormat ();

    switch (GST_VIDEO_FRAME_FORMAT (m_frame)) {
      case GST_VIDEO_FORMAT_UYVY:
        return bmdFormat8BitYUV;
      case GST_VIDEO_FORMAT_v210:
        return bmdFormat10BitYUV;
      case GST_VIDEO_FORMAT_ARGB:
        return bmdFormat8BitARGB;
      case GST_VIDEO_FORMAT_BGRA:
        return bmdFormat8BitBGRA;
      case GST_VIDEO_FORMAT_r210:
        return bmdFormat10BitRGB;
      default:
        g_assert_not_reached ();
    }
  }
  virtual BMDFrameFlags STDMETHODCALLTYPE GetFlags (void)
  {
    return m_dframe ? m_dframe->GetFlags () : bmdFrameFlagDefault;
  }
  virtual HRESULT STDMETHODCALLTYPE GetBytes (void **buffer)
  {
    if (m_dframe)
      return m_dframe->GetBytes (buffer);

    *buffer = GST_VIDEO_FRAME_PLANE_DATA (m_frame, 0);
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetTimecode (BMDTimecodeFormat format,
      IDeckLinkTimecode ** timecode)
  {
    *timecode = m_timecode;
    if (m_timecode) {
      m_timecode->AddRef ();
      return S_OK;
    } else {
      return S_FALSE;
    }
  }

  virtual HRESULT STDMETHODCALLTYPE SetTimecode (GstVideoTimeCode * timecode)
  {
    if (m_timecode) {
      m_timecode->Release ();
    }
    m_timecode = new GstDecklinkTimecode (timecode);

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
      GetAncillaryData (IDeckLinkVideoFrameAncillary ** ancillary)
  {
    *ancillary = m_ancillary;
    if (m_ancillary) {
      m_ancillary->AddRef ();
      return S_OK;
    } else {
      return S_FALSE;
    }
  }
  virtual HRESULT STDMETHODCALLTYPE
      SetAncillaryData (IDeckLinkVideoFrameAncillary * ancillary)
  {
    if (m_ancillary)
      m_ancillary->Release ();

    if (ancillary)
      ancillary->AddRef ();

    m_ancillary = ancillary;

    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID *)
  {
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef (void)
  {
    ULONG ret;

    ret = g_atomic_int_add (&m_refcount, 1) + 1;

    return ret;
  }

  virtual ULONG STDMETHODCALLTYPE Release (void)
  {
    ULONG ret;

    ret = g_atomic_int_add (&m_refcount, -1);
    if (ret == 1) {
      delete this;
    }

    return ret - 1;
  }

private:
  GstVideoFrame * m_frame;
  IDeckLinkMutableVideoFrame *m_dframe;
  IDeckLinkVideoFrameAncillary *m_ancillary;
  GstDecklinkTimecode *m_timecode;
  int m_refcount;

  virtual ~ GstDecklinkVideoFrame () {
    if (m_frame) {
      gst_video_frame_unmap (m_frame);
      g_free (m_frame);
    }
    if (m_dframe) {
      m_dframe->Release ();
    }
    if (m_ancillary) {
      m_ancillary->Release ();
    }
    if (m_timecode) {
      m_timecode->Release ();
    }
  }
};

/**
 * GstDecklinkMappingFormat:
 * @GST_DECKLINK_MAPPING_FORMAT_DEFAULT: Don't change the mapping format
 * @GST_DECKLINK_MAPPING_FORMAT_LEVEL_A: Level A
 * @GST_DECKLINK_MAPPING_FORMAT_LEVEL_B: Level B
 *
 * Since: 1.22
 */
enum
{
  PROP_0,
  PROP_MODE,
  PROP_DEVICE_NUMBER,
  PROP_VIDEO_FORMAT,
  PROP_PROFILE_ID,
  PROP_TIMECODE_FORMAT,
  PROP_KEYER_MODE,
  PROP_KEYER_LEVEL,
  PROP_HW_SERIAL_NUMBER,
  PROP_CC_LINE,
  PROP_AFD_BAR_LINE,
  PROP_MAPPING_FORMAT,
  PROP_PERSISTENT_ID
};

static void gst_decklink_video_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_decklink_video_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_decklink_video_sink_finalize (GObject * object);

static GstStateChangeReturn
gst_decklink_video_sink_change_state (GstElement * element,
    GstStateChange transition);
static GstClock *gst_decklink_video_sink_provide_clock (GstElement * element);

static GstCaps *gst_decklink_video_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_decklink_video_sink_set_caps (GstBaseSink * bsink,
    GstCaps * caps);
static GstFlowReturn gst_decklink_video_sink_prepare (GstBaseSink * bsink,
    GstBuffer * buffer);
static GstFlowReturn gst_decklink_video_sink_render (GstBaseSink * bsink,
    GstBuffer * buffer);
static gboolean gst_decklink_video_sink_open (GstBaseSink * bsink);
static gboolean gst_decklink_video_sink_close (GstBaseSink * bsink);
static gboolean gst_decklink_video_sink_stop (GstDecklinkVideoSink * self);
static gboolean gst_decklink_video_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);
static gboolean gst_decklink_video_sink_event (GstBaseSink * bsink,
    GstEvent * event);

static void
gst_decklink_video_sink_start_scheduled_playback (GstElement * element);

#define parent_class gst_decklink_video_sink_parent_class
G_DEFINE_TYPE (GstDecklinkVideoSink, gst_decklink_video_sink,
    GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (decklinkvideosink, "decklinkvideosink",
    GST_RANK_NONE, GST_TYPE_DECKLINK_VIDEO_SINK,
    decklink_element_init (plugin));

static gboolean
reset_framerate (GstCapsFeatures * features, GstStructure * structure,
    gpointer user_data)
{
  gst_structure_set (structure, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
      G_MAXINT, 1, NULL);

  return TRUE;
}

static void
gst_decklink_video_sink_class_init (GstDecklinkVideoSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstCaps *templ_caps;

  gobject_class->set_property = gst_decklink_video_sink_set_property;
  gobject_class->get_property = gst_decklink_video_sink_get_property;
  gobject_class->finalize = gst_decklink_video_sink_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_change_state);
  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_provide_clock);

  basesink_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_get_caps);
  basesink_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_set_caps);
  basesink_class->prepare = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_prepare);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_render);
  // FIXME: These are misnamed in basesink!
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_open);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_close);
  basesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_propose_allocation);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_event);

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Playback Mode",
          "Video Mode to use for playback",
          GST_TYPE_DECKLINK_MODE, GST_DECKLINK_MODE_NTSC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NUMBER,
      g_param_spec_int ("device-number", "Device number",
          "Output device instance to use", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));
  /**
   * GstDecklinkVideoSink:persistent-id
   *
   * Decklink device to use. Higher priority than "device-number".
   * BMDDeckLinkPersistentID is a device speciﬁc, 32-bit unique identiﬁer.
   * It is stable even when the device is plugged in a diﬀerent connector,
   * across reboots, and when plugged into diﬀerent computers.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PERSISTENT_ID,
      g_param_spec_int64 ("persistent-id", "Persistent id",
          "Output device instance to use. Higher priority than \"device-number\".",
          DEFAULT_PERSISTENT_ID, G_MAXINT64, DEFAULT_PERSISTENT_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_VIDEO_FORMAT,
      g_param_spec_enum ("video-format", "Video format",
          "Video format type to use for playback",
          GST_TYPE_DECKLINK_VIDEO_FORMAT, GST_DECKLINK_VIDEO_FORMAT_8BIT_YUV,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  /**
   * GstDecklinkVideoSink:profile
   *
   * Specifies decklink profile to use.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_PROFILE_ID,
      g_param_spec_enum ("profile", "Profile",
          "Certain DeckLink devices such as the DeckLink 8K Pro, the DeckLink "
          "Quad 2 and the DeckLink Duo 2 support multiple profiles to "
          "configure the capture and playback behavior of its sub-devices."
          "For the DeckLink Duo 2 and DeckLink Quad 2, a profile is shared "
          "between any 2 sub-devices that utilize the same connectors. For the "
          "DeckLink 8K Pro, a profile is shared between all 4 sub-devices. Any "
          "sub-devices that share a profile are considered to be part of the "
          "same profile group."
          "DeckLink Duo 2 support configuration of the duplex mode of "
          "individual sub-devices.",
          GST_TYPE_DECKLINK_PROFILE_ID, GST_DECKLINK_PROFILE_ID_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_TIMECODE_FORMAT,
      g_param_spec_enum ("timecode-format", "Timecode format",
          "Timecode format type to use for playback",
          GST_TYPE_DECKLINK_TIMECODE_FORMAT,
          GST_DECKLINK_TIMECODE_FORMAT_RP188ANY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_KEYER_MODE,
      g_param_spec_enum ("keyer-mode", "Keyer mode",
          "Keyer mode to be enabled",
          GST_TYPE_DECKLINK_KEYER_MODE,
          GST_DECKLINK_KEYER_MODE_OFF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_KEYER_LEVEL,
      g_param_spec_int ("keyer-level", "Keyer level",
          "Keyer level", 0, 255, 255,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_HW_SERIAL_NUMBER,
      g_param_spec_string ("hw-serial-number", "Hardware serial number",
          "The serial number (hardware ID) of the Decklink card",
          NULL, (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CC_LINE,
      g_param_spec_int ("cc-line", "CC Line",
          "Line number to use for inserting closed captions (0 = disabled)", 0,
          22, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_AFD_BAR_LINE,
      g_param_spec_int ("afd-bar-line", "AFD/Bar Line",
          "Line number to use for inserting AFD/Bar data (0 = disabled)", 0,
          10000, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  /**
   * GstDecklinkVideoSink:mapping-format
   *
   * Specifies the 3G-SDI mapping format to use (SMPTE ST 425-1:2017).
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_MAPPING_FORMAT,
      g_param_spec_enum ("mapping-format", "3G-SDI Mapping Format",
          "3G-SDI Mapping Format (Level A/B)",
          GST_TYPE_DECKLINK_MAPPING_FORMAT, GST_DECKLINK_MAPPING_FORMAT_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  templ_caps = gst_decklink_mode_get_template_caps (FALSE);
  templ_caps = gst_caps_make_writable (templ_caps);
  /* For output we support any framerate and only really care about timestamps */
  gst_caps_map_in_place (templ_caps, reset_framerate, NULL);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, templ_caps));
  gst_caps_unref (templ_caps);

  gst_element_class_set_static_metadata (element_class, "Decklink Video Sink",
      "Video/Sink/Hardware", "Decklink Sink",
      "David Schleef <ds@entropywave.com>, "
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (gst_decklink_video_sink_debug, "decklinkvideosink",
      0, "debug category for decklinkvideosink element");

  gst_type_mark_as_plugin_api (GST_TYPE_DECKLINK_MAPPING_FORMAT,
      (GstPluginAPIFlags) 0);
}

static void
gst_decklink_video_sink_init (GstDecklinkVideoSink * self)
{
  self->mode = GST_DECKLINK_MODE_NTSC;
  self->device_number = 0;
  self->persistent_id = DEFAULT_PERSISTENT_ID;
  self->video_format = GST_DECKLINK_VIDEO_FORMAT_8BIT_YUV;
  self->profile_id = GST_DECKLINK_PROFILE_ID_DEFAULT;
  /* VITC is legacy, we should expect RP188 in modern use cases */
  self->timecode_format = bmdTimecodeRP188Any;
  self->caption_line = 0;
  self->afd_bar_line = 0;
  self->mapping_format = GST_DECKLINK_MAPPING_FORMAT_DEFAULT;

  gst_base_sink_set_max_lateness (GST_BASE_SINK_CAST (self), 20 * GST_MSECOND);
  gst_base_sink_set_qos_enabled (GST_BASE_SINK_CAST (self), TRUE);
}

void
gst_decklink_video_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      self->mode = (GstDecklinkModeEnum) g_value_get_enum (value);
      break;
    case PROP_DEVICE_NUMBER:
      self->device_number = g_value_get_int (value);
      break;
    case PROP_VIDEO_FORMAT:
      self->video_format = (GstDecklinkVideoFormat) g_value_get_enum (value);
      switch (self->video_format) {
        case GST_DECKLINK_VIDEO_FORMAT_AUTO:
        case GST_DECKLINK_VIDEO_FORMAT_8BIT_YUV:
        case GST_DECKLINK_VIDEO_FORMAT_10BIT_YUV:
        case GST_DECKLINK_VIDEO_FORMAT_8BIT_ARGB:
        case GST_DECKLINK_VIDEO_FORMAT_8BIT_BGRA:
        case GST_DECKLINK_VIDEO_FORMAT_10BIT_RGB:
          break;
        default:
          GST_ELEMENT_WARNING (GST_ELEMENT (self), CORE, NOT_IMPLEMENTED,
              ("Format %d not supported", self->video_format), (NULL));
          break;
      }
      break;
    case PROP_PROFILE_ID:
      self->profile_id = (GstDecklinkProfileId) g_value_get_enum (value);
      break;
    case PROP_TIMECODE_FORMAT:
      self->timecode_format =
          gst_decklink_timecode_format_from_enum ((GstDecklinkTimecodeFormat)
          g_value_get_enum (value));
      break;
    case PROP_KEYER_MODE:
      self->keyer_mode =
          gst_decklink_keyer_mode_from_enum ((GstDecklinkKeyerMode)
          g_value_get_enum (value));
      break;
    case PROP_KEYER_LEVEL:
      self->keyer_level = g_value_get_int (value);
      break;
    case PROP_CC_LINE:
      self->caption_line = g_value_get_int (value);
      break;
    case PROP_AFD_BAR_LINE:
      self->afd_bar_line = g_value_get_int (value);
      break;
    case PROP_MAPPING_FORMAT:
      self->mapping_format =
          (GstDecklinkMappingFormat) g_value_get_enum (value);
      break;
    case PROP_PERSISTENT_ID:
      self->persistent_id = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;
    case PROP_DEVICE_NUMBER:
      g_value_set_int (value, self->device_number);
      break;
    case PROP_VIDEO_FORMAT:
      g_value_set_enum (value, self->video_format);
      break;
    case PROP_PROFILE_ID:
      g_value_set_enum (value, self->profile_id);
      break;
    case PROP_TIMECODE_FORMAT:
      g_value_set_enum (value,
          gst_decklink_timecode_format_to_enum (self->timecode_format));
      break;
    case PROP_KEYER_MODE:
      g_value_set_enum (value,
          gst_decklink_keyer_mode_to_enum (self->keyer_mode));
      break;
    case PROP_KEYER_LEVEL:
      g_value_set_int (value, self->keyer_level);
      break;
    case PROP_HW_SERIAL_NUMBER:
      if (self->output)
        g_value_set_string (value, self->output->hw_serial_number);
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_CC_LINE:
      g_value_set_int (value, self->caption_line);
      break;
    case PROP_AFD_BAR_LINE:
      g_value_set_int (value, self->afd_bar_line);
      break;
    case PROP_MAPPING_FORMAT:
      g_value_set_enum (value, self->mapping_format);
      break;
    case PROP_PERSISTENT_ID:
      g_value_set_int64 (value, self->persistent_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_sink_finalize (GObject * object)
{
  //GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_decklink_video_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  const GstDecklinkMode *mode;
  HRESULT ret;
  BMDVideoOutputFlags flags;
  GstVideoInfo info;

  GST_DEBUG_OBJECT (self, "Setting caps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;


  g_mutex_lock (&self->output->lock);
  if (self->output->video_enabled) {
    if (self->info.finfo->format == info.finfo->format &&
        self->info.width == info.width && self->info.height == info.height) {
      // FIXME: We should also consider the framerate as it is used
      // for mode selection below in auto mode
      GST_DEBUG_OBJECT (self, "Nothing relevant has changed");
      self->info = info;
      g_mutex_unlock (&self->output->lock);
      return TRUE;
    } else {
      GST_DEBUG_OBJECT (self, "Reconfiguration not supported at this point");
      g_mutex_unlock (&self->output->lock);
      return FALSE;
    }
  }
  g_mutex_unlock (&self->output->lock);

  self->output->output->SetScheduledFrameCompletionCallback (new
      GStreamerVideoOutputCallback (self));

  if (self->mode == GST_DECKLINK_MODE_AUTO) {
    BMDPixelFormat f;
    mode = gst_decklink_find_mode_and_format_for_caps (caps, &f);
    if (mode == NULL) {
      GST_WARNING_OBJECT (self,
          "Failed to find compatible mode for caps  %" GST_PTR_FORMAT, caps);
      return FALSE;
    }
    if (self->video_format != GST_DECKLINK_VIDEO_FORMAT_AUTO &&
        gst_decklink_pixel_format_from_type (self->video_format) != f) {
      GST_WARNING_OBJECT (self, "Failed to set pixel format to %d",
          self->video_format);
      return FALSE;
    }
  } else {
    /* We don't have to give the format in EnableVideoOutput. Therefore,
     * even if it's AUTO, we have it stored in self->info and set it in
     * gst_decklink_video_sink_prepare */
    mode = gst_decklink_get_mode (self->mode);
    g_assert (mode != NULL);
  };

  /* enable or disable keyer */
  if (self->output->keyer != NULL) {
    if (self->keyer_mode == bmdKeyerModeOff) {
      self->output->keyer->Disable ();
    } else if (self->keyer_mode == bmdKeyerModeInternal) {
      self->output->keyer->Enable (false);
      self->output->keyer->SetLevel (self->keyer_level);
    } else if (self->keyer_mode == bmdKeyerModeExternal) {
      self->output->keyer->Enable (true);
      self->output->keyer->SetLevel (self->keyer_level);
    } else {
      g_assert_not_reached ();
    }
  } else if (self->keyer_mode != bmdKeyerModeOff) {
    GST_WARNING_OBJECT (self, "Failed to set keyer to mode %d",
        self->keyer_mode);
  }

  /* The timecode_format itself is used when we embed the actual timecode data
   * into the frame. Now we only need to know which of the two standards the
   * timecode format will adhere to: VITC or RP188, and send the appropriate
   * flag to EnableVideoOutput. The exact format is specified later.
   *
   * Note that this flag will have no effect in practice if the video stream
   * does not contain timecode metadata.
   */
  if ((gint64) self->timecode_format ==
      (gint64) GST_DECKLINK_TIMECODE_FORMAT_VITC
      || (gint64) self->timecode_format ==
      (gint64) GST_DECKLINK_TIMECODE_FORMAT_VITCFIELD2)
    flags = bmdVideoOutputVITC;
  else
    flags = bmdVideoOutputRP188;

  if (self->caption_line > 0 || self->afd_bar_line > 0)
    flags = (BMDVideoOutputFlags) (flags | bmdVideoOutputVANC);

  ret = self->output->output->EnableVideoOutput (mode->mode, flags);
  if (ret != S_OK) {
    GST_WARNING_OBJECT (self, "Failed to enable video output: 0x%08lx",
        (unsigned long) ret);
    return FALSE;
  }

  self->info = info;
  g_mutex_lock (&self->output->lock);
  self->output->mode = mode;
  self->output->video_enabled = TRUE;
  if (self->output->start_scheduled_playback)
    self->output->start_scheduled_playback (self->output->videosink);
  g_mutex_unlock (&self->output->lock);

  if (self->vbiencoder) {
    gst_video_vbi_encoder_free (self->vbiencoder);
    self->vbiencoder = NULL;
    self->anc_vformat = GST_VIDEO_FORMAT_UNKNOWN;
  }

  return TRUE;
}

static GstCaps *
gst_decklink_video_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  GstCaps *mode_caps, *caps;

  if (self->mode == GST_DECKLINK_MODE_AUTO
      && self->video_format == GST_DECKLINK_VIDEO_FORMAT_AUTO)
    mode_caps = gst_decklink_mode_get_template_caps (FALSE);
  else if (self->video_format == GST_DECKLINK_VIDEO_FORMAT_AUTO)
    mode_caps = gst_decklink_mode_get_caps_all_formats (self->mode, FALSE);
  else if (self->mode == GST_DECKLINK_MODE_AUTO)
    mode_caps =
        gst_decklink_pixel_format_get_caps (gst_decklink_pixel_format_from_type
        (self->video_format), FALSE);
  else
    mode_caps =
        gst_decklink_mode_get_caps (self->mode,
        gst_decklink_pixel_format_from_type (self->video_format), FALSE);
  mode_caps = gst_caps_make_writable (mode_caps);
  /* For output we support any framerate and only really care about timestamps */
  gst_caps_map_in_place (mode_caps, reset_framerate, NULL);

  if (filter) {
    caps =
        gst_caps_intersect_full (filter, mode_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (mode_caps);
  } else {
    caps = mode_caps;
  }

  return caps;
}

static GstFlowReturn
gst_decklink_video_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  return GST_FLOW_OK;
}

void
gst_decklink_video_sink_convert_to_internal_clock (GstDecklinkVideoSink * self,
    GstClockTime * timestamp, GstClockTime * duration)
{
  GstClock *clock;
  GstClockTime internal_base, external_base, internal_offset;

  g_assert (timestamp != NULL);

  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  GST_OBJECT_LOCK (self);
  internal_base = self->internal_base_time;
  external_base = self->external_base_time;
  internal_offset = self->internal_time_offset;
  GST_OBJECT_UNLOCK (self);

  if (!clock || clock != self->output->clock) {
    GstClockTime internal, external, rate_n, rate_d;
    GstClockTime external_timestamp = *timestamp;
    GstClockTime base_time;

    gst_clock_get_calibration (self->output->clock, &internal, &external,
        &rate_n, &rate_d);

    // Convert to the running time corresponding to both clock times
    if (!GST_CLOCK_TIME_IS_VALID (internal_base) || internal < internal_base)
      internal = 0;
    else
      internal -= internal_base;

    if (!GST_CLOCK_TIME_IS_VALID (external_base) || external < external_base)
      external = 0;
    else
      external -= external_base;

    // Convert timestamp to the "running time" since we started scheduled
    // playback, that is the difference between the pipeline's base time
    // and our own base time.
    base_time = gst_element_get_base_time (GST_ELEMENT_CAST (self));
    if (base_time > external_base)
      base_time = 0;
    else
      base_time = external_base - base_time;

    if (external_timestamp < base_time)
      external_timestamp = 0;
    else
      external_timestamp = external_timestamp - base_time;

    // Get the difference in the external time, note
    // that the running time is external time.
    // Then scale this difference and offset it to
    // our internal time. Now we have the running time
    // according to our internal clock.
    //
    // For the duration we just scale
    *timestamp =
        gst_clock_unadjust_with_calibration (NULL, external_timestamp,
        internal, external, rate_n, rate_d);

    GST_LOG_OBJECT (self,
        "Converted %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT " (internal: %"
        GST_TIME_FORMAT " external %" GST_TIME_FORMAT " rate: %lf)",
        GST_TIME_ARGS (external_timestamp), GST_TIME_ARGS (*timestamp),
        GST_TIME_ARGS (internal), GST_TIME_ARGS (external),
        ((gdouble) rate_n) / ((gdouble) rate_d));

    if (duration) {
      GstClockTime external_duration = *duration;

      *duration = gst_util_uint64_scale (external_duration, rate_d, rate_n);

      GST_LOG_OBJECT (self,
          "Converted duration %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT
          " (internal: %" GST_TIME_FORMAT " external %" GST_TIME_FORMAT
          " rate: %lf)", GST_TIME_ARGS (external_duration),
          GST_TIME_ARGS (*duration), GST_TIME_ARGS (internal),
          GST_TIME_ARGS (external), ((gdouble) rate_n) / ((gdouble) rate_d));
    }
  } else {
    GST_LOG_OBJECT (self, "No clock conversion needed, same clocks: %"
        GST_TIME_FORMAT, GST_TIME_ARGS (*timestamp));
  }

  if (external_base != GST_CLOCK_TIME_NONE &&
      internal_base != GST_CLOCK_TIME_NONE)
    *timestamp += internal_offset;
  else
    *timestamp = gst_clock_get_internal_time (self->output->clock);

  GST_DEBUG_OBJECT (self, "Output timestamp %" GST_TIME_FORMAT
      " using clock epoch %" GST_TIME_FORMAT,
      GST_TIME_ARGS (*timestamp), GST_TIME_ARGS (self->output->clock_epoch));

  if (clock)
    gst_object_unref (clock);
}

/* Copied from ext/closedcaption/gstccconverter.c */
/* Converts raw CEA708 cc_data and an optional timecode into CDP */
static guint
convert_cea708_cc_data_cea708_cdp_internal (GstDecklinkVideoSink * self,
    const guint8 * cc_data, guint cc_data_len, guint8 * cdp, guint cdp_len,
    const GstVideoTimeCodeMeta * tc_meta)
{
  GstByteWriter bw;
  guint8 flags, checksum;
  guint i, len;
  const GstDecklinkMode *mode = gst_decklink_get_mode (self->mode);

  gst_byte_writer_init_with_data (&bw, cdp, cdp_len, FALSE);
  gst_byte_writer_put_uint16_be_unchecked (&bw, 0x9669);
  /* Write a length of 0 for now */
  gst_byte_writer_put_uint8_unchecked (&bw, 0);
  if (mode->fps_n == 24000 && mode->fps_d == 1001) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x1f);
  } else if (mode->fps_n == 24 && mode->fps_d == 1) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x2f);
  } else if (mode->fps_n == 25 && mode->fps_d == 1) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x3f);
  } else if (mode->fps_n == 30000 && mode->fps_d == 1001) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x4f);
  } else if (mode->fps_n == 30 && mode->fps_d == 1) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x5f);
  } else if (mode->fps_n == 50 && mode->fps_d == 1) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x6f);
  } else if (mode->fps_n == 60000 && mode->fps_d == 1001) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x7f);
  } else if (mode->fps_n == 60 && mode->fps_d == 1) {
    gst_byte_writer_put_uint8_unchecked (&bw, 0x8f);
  } else {
    g_assert_not_reached ();
  }

  /* ccdata_present | caption_service_active */
  flags = 0x42;

  /* time_code_present */
  if (tc_meta)
    flags |= 0x80;

  /* reserved */
  flags |= 0x01;

  gst_byte_writer_put_uint8_unchecked (&bw, flags);

  gst_byte_writer_put_uint16_be_unchecked (&bw, self->cdp_hdr_sequence_cntr);

  if (tc_meta) {
    const GstVideoTimeCode *tc = &tc_meta->tc;
    guint8 u8;

    gst_byte_writer_put_uint8_unchecked (&bw, 0x71);
    /* reserved 11 - 2 bits */
    u8 = 0xc0;
    /* tens of hours - 2 bits */
    u8 |= ((tc->hours / 10) & 0x3) << 4;
    /* units of hours - 4 bits */
    u8 |= (tc->hours % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* reserved 1 - 1 bit */
    u8 = 0x80;
    /* tens of minutes - 3 bits */
    u8 |= ((tc->minutes / 10) & 0x7) << 4;
    /* units of minutes - 4 bits */
    u8 |= (tc->minutes % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* field flag - 1 bit */
    u8 = tc->field_count < 2 ? 0x00 : 0x80;
    /* tens of seconds - 3 bits */
    u8 |= ((tc->seconds / 10) & 0x7) << 4;
    /* units of seconds - 4 bits */
    u8 |= (tc->seconds % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);

    /* drop frame flag - 1 bit */
    u8 = (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) ? 0x80 :
        0x00;
    /* reserved0 - 1 bit */
    /* tens of frames - 2 bits */
    u8 |= ((tc->frames / 10) & 0x3) << 4;
    /* units of frames 4 bits */
    u8 |= (tc->frames % 10) & 0xf;
    gst_byte_writer_put_uint8_unchecked (&bw, u8);
  }

  gst_byte_writer_put_uint8_unchecked (&bw, 0x72);
  gst_byte_writer_put_uint8_unchecked (&bw, 0xe0 | cc_data_len / 3);
  gst_byte_writer_put_data_unchecked (&bw, cc_data, cc_data_len);

  gst_byte_writer_put_uint8_unchecked (&bw, 0x74);
  gst_byte_writer_put_uint16_be_unchecked (&bw, self->cdp_hdr_sequence_cntr);
  self->cdp_hdr_sequence_cntr++;
  /* We calculate the checksum afterwards */
  gst_byte_writer_put_uint8_unchecked (&bw, 0);

  len = gst_byte_writer_get_pos (&bw);
  gst_byte_writer_set_pos (&bw, 2);
  gst_byte_writer_put_uint8_unchecked (&bw, len);

  checksum = 0;
  for (i = 0; i < len; i++) {
    checksum += cdp[i];
  }
  checksum &= 0xff;
  checksum = 256 - checksum;
  cdp[len - 1] = checksum;

  return len;
}

static void
write_vbi (GstDecklinkVideoSink * self, GstBuffer * buffer,
    BMDPixelFormat format, GstDecklinkVideoFrame * frame,
    GstVideoTimeCodeMeta * tc_meta)
{
  IDeckLinkVideoFrameAncillary *vanc_frame = NULL;
  gpointer iter = NULL;
  GstVideoCaptionMeta *cc_meta;
  guint8 *vancdata;
  gboolean got_captions = FALSE;

  if (self->caption_line == 0 && self->afd_bar_line == 0)
    return;

  if (self->vbiencoder == NULL) {
    self->vbiencoder =
        gst_video_vbi_encoder_new (GST_VIDEO_FORMAT_v210, self->info.width);
    self->anc_vformat = GST_VIDEO_FORMAT_v210;
  }

  /* Put any closed captions into the configured line */
  while ((cc_meta =
          (GstVideoCaptionMeta *) gst_buffer_iterate_meta_filtered (buffer,
              &iter, GST_VIDEO_CAPTION_META_API_TYPE))) {
    switch (cc_meta->caption_type) {
      case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:{
        guint8 data[138];
        guint i, n;

        n = cc_meta->size / 2;
        if (cc_meta->size > 46) {
          GST_WARNING_OBJECT (self, "Too big raw CEA608 buffer");
          break;
        }

        /* This is the offset from line 9 for 525-line fields and from line
         * 5 for 625-line fields.
         *
         * The highest bit is set for field 1 but not for field 0, but we
         * have no way of knowning the field here
         */
        for (i = 0; i < n; i++) {
          data[3 * i] = 0x80 | (self->info.height ==
              525 ? self->caption_line - 9 : self->caption_line - 5);
          data[3 * i + 1] = cc_meta->data[2 * i];
          data[3 * i + 2] = cc_meta->data[2 * i + 1];
        }

        if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
                FALSE,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_608 >> 8,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_608 & 0xff, data, 3))
          GST_WARNING_OBJECT (self, "Couldn't add meta to ancillary data");

        got_captions = TRUE;

        break;
      }
      case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:{
        if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
                FALSE,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_608 >> 8,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_608 & 0xff, cc_meta->data,
                cc_meta->size))
          GST_WARNING_OBJECT (self, "Couldn't add meta to ancillary data");

        got_captions = TRUE;

        break;
      }
      case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:{
        guint8 data[256];
        guint n;

        n = cc_meta->size / 3;
        if (cc_meta->size > 46) {
          GST_WARNING_OBJECT (self, "Too big raw CEA708 buffer");
          break;
        }

        n = convert_cea708_cc_data_cea708_cdp_internal (self, cc_meta->data,
            cc_meta->size, data, sizeof (data), tc_meta);
        if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder, FALSE,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_708 >> 8,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_708 & 0xff, data, n))
          GST_WARNING_OBJECT (self, "Couldn't add meta to ancillary data");

        got_captions = TRUE;

        break;
      }
      case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:{
        if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
                FALSE,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_708 >> 8,
                GST_VIDEO_ANCILLARY_DID16_S334_EIA_708 & 0xff, cc_meta->data,
                cc_meta->size))
          GST_WARNING_OBJECT (self, "Couldn't add meta to ancillary data");

        got_captions = TRUE;

        break;
      }
      default:{
        GST_FIXME_OBJECT (self, "Caption type %d not supported",
            cc_meta->caption_type);
        break;
      }
    }
  }

  if ((got_captions || self->afd_bar_line != 0)
      && self->output->output->CreateAncillaryData (bmdFormat10BitYUV,
          &vanc_frame) == S_OK) {
    GstVideoAFDMeta *afd_meta = NULL, *afd_meta2 = NULL;
    GstVideoBarMeta *bar_meta = NULL, *bar_meta2 = NULL;
    GstMeta *meta;
    gpointer meta_iter;
    guint8 afd_bar_data[8] = { 0, };
    guint8 afd_bar_data2[8] = { 0, };
    guint8 afd = 0;
    gboolean is_letterbox = 0;
    guint16 bar1 = 0, bar2 = 0;
    guint i;

    // Get any reasonable AFD/Bar metas for both fields
    meta_iter = NULL;
    while ((meta =
            gst_buffer_iterate_meta_filtered (buffer, &meta_iter,
                GST_VIDEO_AFD_META_API_TYPE))) {
      GstVideoAFDMeta *tmp_meta = (GstVideoAFDMeta *) meta;

      if (tmp_meta->field == 0 || !afd_meta || (afd_meta && afd_meta->field != 0
              && tmp_meta->field == 0))
        afd_meta = tmp_meta;
      if (tmp_meta->field == 1 || !afd_meta2 || (afd_meta2
              && afd_meta->field != 1 && tmp_meta->field == 1))
        afd_meta2 = tmp_meta;
    }

    meta_iter = NULL;
    while ((meta =
            gst_buffer_iterate_meta_filtered (buffer, &meta_iter,
                GST_VIDEO_BAR_META_API_TYPE))) {
      GstVideoBarMeta *tmp_meta = (GstVideoBarMeta *) meta;

      if (tmp_meta->field == 0 || !bar_meta || (bar_meta && bar_meta->field != 0
              && tmp_meta->field == 0))
        bar_meta = tmp_meta;
      if (tmp_meta->field == 1 || !bar_meta2 || (bar_meta2
              && bar_meta->field != 1 && tmp_meta->field == 1))
        bar_meta2 = tmp_meta;
    }

    for (i = 0; i < 2; i++) {
      guint8 *afd_bar_data_ptr;

      if (i == 0) {
        afd_bar_data_ptr = afd_bar_data;
        afd = afd_meta ? afd_meta->afd : 0;
        is_letterbox = bar_meta ? bar_meta->is_letterbox : FALSE;
        bar1 = bar_meta ? bar_meta->bar_data1 : 0;
        bar2 = bar_meta ? bar_meta->bar_data2 : 0;
      } else {
        afd_bar_data_ptr = afd_bar_data2;
        afd = afd_meta2 ? afd_meta2->afd : 0;
        is_letterbox = bar_meta2 ? bar_meta2->is_letterbox : FALSE;
        bar1 = bar_meta2 ? bar_meta2->bar_data1 : 0;
        bar2 = bar_meta2 ? bar_meta2->bar_data2 : 0;
      }

      /* See SMPTE 2016-3 Section 4 */
      /* AFD and AR */
      if (self->mode <= (gint) GST_DECKLINK_MODE_PAL_P) {
        afd_bar_data_ptr[0] = (afd << 3) | 0x0;
      } else {
        afd_bar_data_ptr[0] = (afd << 3) | 0x4;
      }

      /* Bar flags */
      afd_bar_data_ptr[3] = is_letterbox ? 0xc0 : 0x30;

      /* Bar value 1 and 2 */
      GST_WRITE_UINT16_BE (&afd_bar_data_ptr[4], bar1);
      GST_WRITE_UINT16_BE (&afd_bar_data_ptr[6], bar2);
    }

    /* AFD on the same line as the captions */
    if (self->caption_line == self->afd_bar_line) {
      if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
              FALSE, GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR >> 8,
              GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR & 0xff, afd_bar_data,
              sizeof (afd_bar_data)))
        GST_WARNING_OBJECT (self,
            "Couldn't add AFD/Bar data to ancillary data");
    }

    /* FIXME: Add captions to the correct field? Captions for the second
     * field should probably be inserted into the second field */

    if (got_captions || self->caption_line == self->afd_bar_line) {
      if (vanc_frame->GetBufferForVerticalBlankingLine (self->caption_line,
              (void **) &vancdata) == S_OK) {
        gst_video_vbi_encoder_write_line (self->vbiencoder, vancdata);
      } else {
        GST_WARNING_OBJECT (self,
            "Failed to get buffer for line %d ancillary data",
            self->caption_line);
      }
    }

    /* AFD on a different line than the captions */
    if (self->afd_bar_line != 0 && self->caption_line != self->afd_bar_line) {
      if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
              FALSE, GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR >> 8,
              GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR & 0xff, afd_bar_data,
              sizeof (afd_bar_data)))
        GST_WARNING_OBJECT (self,
            "Couldn't add AFD/Bar data to ancillary data");

      if (vanc_frame->GetBufferForVerticalBlankingLine (self->afd_bar_line,
              (void **) &vancdata) == S_OK) {
        gst_video_vbi_encoder_write_line (self->vbiencoder, vancdata);
      } else {
        GST_WARNING_OBJECT (self,
            "Failed to get buffer for line %d ancillary data",
            self->afd_bar_line);
      }
    }

    /* For interlaced video we need to also add AFD to the second field */
    if (GST_VIDEO_INFO_IS_INTERLACED (&self->info) && self->afd_bar_line != 0) {
      guint field2_offset;

      /* The VANC lines for the second field are at an offset, depending on
       * the format in use.
       */
      switch (self->info.height) {
        case 486:
          /* NTSC: 525 / 2 + 1 */
          field2_offset = 263;
          break;
        case 576:
          /* PAL: 625 / 2 + 1 */
          field2_offset = 313;
          break;
        case 1080:
          /* 1080i: 1125 / 2 + 1 */
          field2_offset = 563;
          break;
        default:
          g_assert_not_reached ();
      }

      if (!gst_video_vbi_encoder_add_ancillary (self->vbiencoder,
              FALSE, GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR >> 8,
              GST_VIDEO_ANCILLARY_DID16_S2016_3_AFD_BAR & 0xff, afd_bar_data2,
              sizeof (afd_bar_data)))
        GST_WARNING_OBJECT (self,
            "Couldn't add AFD/Bar data to ancillary data");

      if (vanc_frame->GetBufferForVerticalBlankingLine (self->afd_bar_line +
              field2_offset, (void **) &vancdata) == S_OK) {
        gst_video_vbi_encoder_write_line (self->vbiencoder, vancdata);
      } else {
        GST_WARNING_OBJECT (self,
            "Failed to get buffer for line %d ancillary data",
            self->afd_bar_line);
      }
    }

    if (frame->SetAncillaryData (vanc_frame) != S_OK) {
      GST_WARNING_OBJECT (self, "Failed to set ancillary data");
    }

    vanc_frame->Release ();
  } else if (got_captions || self->afd_bar_line != 0) {
    GST_WARNING_OBJECT (self, "Failed to allocate ancillary data frame");
  }
}

static gboolean
buffer_is_pbo_memory (GstBuffer * buffer)
{
  GstMemory *mem;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (mem->allocator
      && g_strcmp0 (mem->allocator->mem_type, "GLMemoryPBO") == 0)
    return TRUE;
  return FALSE;
}

static GstFlowReturn
gst_decklink_video_sink_prepare (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  GstVideoFrame vframe;
  GstDecklinkVideoFrame *frame = NULL;
  GstFlowReturn flow_ret;
  HRESULT ret;
  GstClockTime timestamp, duration;
  GstClockTime running_time, running_time_duration;
  GstClockTime latency, render_delay;
  GstClockTimeDiff ts_offset;
  GstDecklinkVideoFormat caps_format;
  BMDPixelFormat format;
  GstVideoTimeCodeMeta *tc_meta;

  GST_DEBUG_OBJECT (self, "Preparing buffer %" GST_PTR_FORMAT, buffer);

  // FIXME: Handle no timestamps
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    return GST_FLOW_ERROR;
  }

  caps_format = gst_decklink_type_from_video_format (self->info.finfo->format);
  format = gst_decklink_pixel_format_from_type (caps_format);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  if (duration == GST_CLOCK_TIME_NONE) {
    duration =
        gst_util_uint64_scale_int (GST_SECOND, self->info.fps_d,
        self->info.fps_n);
  }
  running_time =
      gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
      GST_FORMAT_TIME, timestamp);
  running_time_duration =
      gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
      GST_FORMAT_TIME, timestamp + duration) - running_time;

  /* See gst_base_sink_adjust_time() */
  latency = gst_base_sink_get_latency (bsink);
  render_delay = gst_base_sink_get_render_delay (bsink);
  ts_offset = gst_base_sink_get_ts_offset (bsink);

  running_time += latency;

  if (ts_offset < 0) {
    ts_offset = -ts_offset;
    if ((GstClockTime) ts_offset < running_time)
      running_time -= ts_offset;
    else
      running_time = 0;
  } else {
    running_time += ts_offset;
  }

  if (running_time > render_delay)
    running_time -= render_delay;
  else
    running_time = 0;

  if (!gst_video_frame_map (&vframe, &self->info, buffer, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map video frame");
    flow_ret = GST_FLOW_ERROR;
    goto out;
  }
  // If the video frame is stored in PBO memory then we need to copy anyway as
  // it might be stored in CPU-accessible GPU memory that can't be accessed
  // from the Decklink driver.
  if (buffer_is_pbo_memory (buffer)) {
    guint8 *outdata;
    const guint8 *indata;
    gint i, src_stride, dest_stride, stride;
    IDeckLinkMutableVideoFrame *dframe;

    ret = self->output->output->CreateVideoFrame (self->info.width,
        self->info.height, self->info.stride[0], format, bmdFrameFlagDefault,
        &dframe);

    if (ret != S_OK) {
      gst_video_frame_unmap (&vframe);
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          (NULL), ("Failed to create video frame: 0x%08lx",
              (unsigned long) ret));
      return GST_FLOW_ERROR;
    }

    dframe->GetBytes ((void **) &outdata);
    indata = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0);
    src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&vframe, 0);
    dest_stride = dframe->GetRowBytes ();
    stride = MIN (src_stride, dest_stride);
    for (i = 0; i < self->info.height; i++) {
      memcpy (outdata, indata, stride);
      indata += src_stride;
      outdata += dest_stride;
    }
    gst_video_frame_unmap (&vframe);

    // Takes ownership of the frame
    frame = new GstDecklinkVideoFrame (dframe);
  } else {
    // Takes ownership of the frame
    frame = new GstDecklinkVideoFrame (&vframe);
  }

  tc_meta = gst_buffer_get_video_time_code_meta (buffer);
  if (tc_meta) {
    gchar *tc_str;

    frame->SetTimecode (&tc_meta->tc);
    tc_str = gst_video_time_code_to_string (&tc_meta->tc);
    GST_DEBUG_OBJECT (self, "Set frame timecode to %s", tc_str);
    g_free (tc_str);
  }

  write_vbi (self, buffer, format, frame, tc_meta);

  gst_decklink_video_sink_convert_to_internal_clock (self, &running_time,
      &running_time_duration);

  GST_LOG_OBJECT (self, "Scheduling video frame %p at %" GST_TIME_FORMAT
      " with duration %" GST_TIME_FORMAT, frame, GST_TIME_ARGS (running_time),
      GST_TIME_ARGS (running_time_duration));

  ret = self->output->output->ScheduleVideoFrame (frame,
      running_time, running_time_duration, GST_SECOND);
  if (ret != S_OK) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        (NULL), ("Failed to schedule frame: 0x%08lx", (unsigned long) ret));
    flow_ret = GST_FLOW_ERROR;
    goto out;
  }

  flow_ret = GST_FLOW_OK;

out:

  if (frame)
    frame->Release ();

  return flow_ret;
}

static gboolean
gst_decklink_video_sink_open (GstBaseSink * bsink)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  const GstDecklinkMode *mode;

  GST_DEBUG_OBJECT (self, "Starting");

  self->output =
      gst_decklink_acquire_nth_output (self->device_number, self->persistent_id,
      GST_ELEMENT_CAST (self), FALSE);
  if (!self->output) {
    GST_ERROR_OBJECT (self, "Failed to acquire output");
    return FALSE;
  }

  g_object_notify (G_OBJECT (self), "hw-serial-number");

  mode = gst_decklink_get_mode (self->mode);
  g_assert (mode != NULL);

  g_mutex_lock (&self->output->lock);
  self->output->mode = mode;
  self->output->start_scheduled_playback =
      gst_decklink_video_sink_start_scheduled_playback;
  self->output->clock_start_time = GST_CLOCK_TIME_NONE;
  self->output->clock_epoch += self->output->clock_last_time;
  self->output->clock_last_time = 0;
  self->output->clock_offset = 0;
  GST_OBJECT_LOCK (self);
  self->internal_base_time = GST_CLOCK_TIME_NONE;
  self->external_base_time = GST_CLOCK_TIME_NONE;
  GST_OBJECT_UNLOCK (self);
  g_mutex_unlock (&self->output->lock);

  return TRUE;
}

static gboolean
gst_decklink_video_sink_close (GstBaseSink * bsink)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);

  GST_DEBUG_OBJECT (self, "Closing");

  if (self->output) {
    g_mutex_lock (&self->output->lock);
    self->output->mode = NULL;
    self->output->video_enabled = FALSE;
    if (self->output->start_scheduled_playback && self->output->videosink)
      self->output->start_scheduled_playback (self->output->videosink);
    g_mutex_unlock (&self->output->lock);

    self->output->output->DisableVideoOutput ();
    gst_decklink_release_nth_output (self->device_number, self->persistent_id,
        GST_ELEMENT_CAST (self), FALSE);
    self->output = NULL;
  }

  return TRUE;
}

static gboolean
gst_decklink_video_sink_stop (GstDecklinkVideoSink * self)
{
  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->output && self->output->video_enabled) {
    g_mutex_lock (&self->output->lock);
    self->output->video_enabled = FALSE;
    g_mutex_unlock (&self->output->lock);

    self->output->output->DisableVideoOutput ();
    self->output->output->SetScheduledFrameCompletionCallback (NULL);
  }

  if (self->vbiencoder) {
    gst_video_vbi_encoder_free (self->vbiencoder);
    self->vbiencoder = NULL;
    self->anc_vformat = GST_VIDEO_FORMAT_UNKNOWN;
  }

  return TRUE;
}

static void
_wait_for_stop_notify (GstDecklinkVideoSink * self)
{
  bool active = false;

  self->output->output->IsScheduledPlaybackRunning (&active);
  while (active) {
    /* cause sometimes decklink stops without notifying us... */
    guint64 wait_time = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    if (!g_cond_wait_until (&self->output->cond, &self->output->lock,
            wait_time))
      GST_WARNING_OBJECT (self, "Failed to wait for stop notification");
    self->output->output->IsScheduledPlaybackRunning (&active);
  }
}

static void
gst_decklink_video_sink_start_scheduled_playback (GstElement * element)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (element);
  GstClockTime start_time;
  HRESULT res;
  bool active;

  // Check if we're already started
  if (self->output->started) {
    GST_DEBUG_OBJECT (self, "Already started");
    return;
  }
  // Check if we're ready to start:
  // we need video and audio enabled, if there is audio
  // and both of the two elements need to be set to PLAYING already
  if (!self->output->video_enabled) {
    GST_DEBUG_OBJECT (self,
        "Not starting scheduled playback yet: video not enabled yet!");
    return;
  }

  if (self->output->audiosink && !self->output->audio_enabled) {
    GST_DEBUG_OBJECT (self,
        "Not starting scheduled playback yet: "
        "have audio but not enabled yet!");
    return;
  }

  if ((GST_STATE (self) < GST_STATE_PAUSED
          && GST_STATE_PENDING (self) < GST_STATE_PAUSED)
      || (self->output->audiosink &&
          GST_STATE (self->output->audiosink) < GST_STATE_PAUSED
          && GST_STATE_PENDING (self->output->audiosink) < GST_STATE_PAUSED)) {
    GST_DEBUG_OBJECT (self,
        "Not starting scheduled playback yet: "
        "Elements are not set to PAUSED yet");
    return;
  }
  // Need to unlock to get the clock time
  g_mutex_unlock (&self->output->lock);

  start_time = gst_clock_get_internal_time (self->output->clock);

  g_mutex_lock (&self->output->lock);
  // Check if someone else started in the meantime
  if (self->output->started) {
    return;
  }

  active = false;
  self->output->output->IsScheduledPlaybackRunning (&active);
  if (active) {
    GST_DEBUG_OBJECT (self, "Stopping scheduled playback");

    self->output->started = FALSE;

    res = self->output->output->StopScheduledPlayback (0, 0, 0);
    if (res != S_OK) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          (NULL), ("Failed to stop scheduled playback: 0x%08lx",
              (unsigned long) res));
      return;
    }
    // Wait until scheduled playback actually stopped
    _wait_for_stop_notify (self);
  }

  GST_INFO_OBJECT (self,
      "Starting scheduled playback at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (start_time));

  res =
      self->output->output->StartScheduledPlayback (start_time,
      GST_SECOND, 1.0);
  if (res != S_OK) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        (NULL), ("Failed to start scheduled playback: 0x%08lx",
            (unsigned long) res));
    return;
  }

  self->output->started = TRUE;
}

static GstStateChangeReturn
gst_decklink_video_sink_stop_scheduled_playback (GstDecklinkVideoSink * self)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstClockTime start_time;
  HRESULT res;

  if (!self->output->started)
    return ret;

  start_time = gst_clock_get_internal_time (self->output->clock);

  GST_INFO_OBJECT (self,
      "Stopping scheduled playback at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (start_time));

  g_mutex_lock (&self->output->lock);
  self->output->started = FALSE;
  res = self->output->output->StopScheduledPlayback (start_time, 0, GST_SECOND);
  if (res != S_OK) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        (NULL), ("Failed to stop scheduled playback: 0x%08lx", (unsigned long)
            res));
    ret = GST_STATE_CHANGE_FAILURE;
  } else {

    // Wait until scheduled playback actually stopped
    _wait_for_stop_notify (self);
  }
  g_mutex_unlock (&self->output->lock);
  GST_OBJECT_LOCK (self);
  self->internal_base_time = GST_CLOCK_TIME_NONE;
  self->external_base_time = GST_CLOCK_TIME_NONE;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static GstStateChangeReturn
gst_decklink_video_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->vbiencoder = NULL;
      self->anc_vformat = GST_VIDEO_FORMAT_UNKNOWN;
      self->cdp_hdr_sequence_cntr = 0;

      g_mutex_lock (&self->output->lock);
      self->output->clock_epoch += self->output->clock_last_time;
      self->output->clock_last_time = 0;
      self->output->clock_offset = 0;
      g_mutex_unlock (&self->output->lock);
      gst_element_post_message (element,
          gst_message_new_clock_provide (GST_OBJECT_CAST (element),
              self->output->clock, TRUE));
      g_mutex_lock (&self->output->lock);
      if (self->output->start_scheduled_playback)
        self->output->start_scheduled_playback (self->output->videosink);
      g_mutex_unlock (&self->output->lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      GstClock *clock;

      clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
      if (clock) {
        if (clock != self->output->clock) {
          gst_clock_set_master (self->output->clock, clock);
        }

        GST_OBJECT_LOCK (self);
        if (self->external_base_time == GST_CLOCK_TIME_NONE
            || self->internal_base_time == GST_CLOCK_TIME_NONE) {
          self->external_base_time = gst_clock_get_internal_time (clock);
          self->internal_base_time =
              gst_clock_get_internal_time (self->output->clock);
          self->internal_time_offset = self->internal_base_time;
        } else if (GST_CLOCK_TIME_IS_VALID (self->internal_pause_time)) {
          self->internal_time_offset +=
              gst_clock_get_internal_time (self->output->clock) -
              self->internal_pause_time;
        }

        GST_INFO_OBJECT (self, "clock has been set to %" GST_PTR_FORMAT
            ", updated base times - internal: %" GST_TIME_FORMAT
            " external: %" GST_TIME_FORMAT " internal offset %"
            GST_TIME_FORMAT, clock,
            GST_TIME_ARGS (self->internal_base_time),
            GST_TIME_ARGS (self->external_base_time),
            GST_TIME_ARGS (self->internal_time_offset));
        GST_OBJECT_UNLOCK (self);

        gst_object_unref (clock);
      } else {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL), ("Need a clock to go to PLAYING"));
        ret = GST_STATE_CHANGE_FAILURE;
      }
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (gst_decklink_video_sink_stop_scheduled_playback (self) ==
          GST_STATE_CHANGE_FAILURE)
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      gst_element_post_message (element,
          gst_message_new_clock_lost (GST_OBJECT_CAST (element),
              self->output->clock));
      gst_clock_set_master (self->output->clock, NULL);
      // Reset calibration to make the clock reusable next time we use it
      gst_clock_set_calibration (self->output->clock, 0, 0, 1, 1);
      g_mutex_lock (&self->output->lock);
      self->output->clock_epoch += self->output->clock_last_time;
      self->output->clock_last_time = 0;
      self->output->clock_offset = 0;
      g_mutex_unlock (&self->output->lock);
      gst_decklink_video_sink_stop (self);
      GST_OBJECT_LOCK (self);
      self->internal_base_time = GST_CLOCK_TIME_NONE;
      self->external_base_time = GST_CLOCK_TIME_NONE;
      self->internal_pause_time = GST_CLOCK_TIME_NONE;
      GST_OBJECT_UNLOCK (self);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      self->internal_pause_time =
          gst_clock_get_internal_time (self->output->clock);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_decklink_video_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    {
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      gboolean reset_time;

      gst_event_parse_flush_stop (event, &reset_time);
      if (reset_time) {
        GST_OBJECT_LOCK (self);
        /* force a recalculation of clock base times */
        self->external_base_time = GST_CLOCK_TIME_NONE;
        self->internal_base_time = GST_CLOCK_TIME_NONE;
        GST_OBJECT_UNLOCK (self);
      }
      break;
    }
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (bsink, event);
}

static GstClock *
gst_decklink_video_sink_provide_clock (GstElement * element)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (element);

  if (!self->output)
    return NULL;

  return GST_CLOCK_CAST (gst_object_ref (self->output->clock));
}

static gboolean
gst_decklink_video_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query)
{
  GstCaps *caps;
  GstVideoInfo info;
  GstBufferPool *pool;
  guint size;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstStructure *structure;
    GstAllocator *allocator = NULL;
    GstAllocationParams params = { (GstMemoryFlags) 0, 15, 0, 0 };

    if (gst_query_get_n_allocation_params (query) > 0)
      gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    else
      gst_query_add_allocation_param (query, allocator, &params);

    pool = gst_video_buffer_pool_new ();

    structure = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (structure, caps, size, 0, 0);
    gst_buffer_pool_config_set_allocator (structure, allocator, &params);

    if (allocator)
      gst_object_unref (allocator);

    if (!gst_buffer_pool_set_config (pool, structure))
      goto config_failed;

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
    gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  }

  return TRUE;
  // ERRORS
config_failed:
  {
    GST_ERROR_OBJECT (bsink, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}
