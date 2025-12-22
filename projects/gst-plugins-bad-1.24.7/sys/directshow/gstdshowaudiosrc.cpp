/* GStreamer
 * Copyright (C)  2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowaudiosrc.c:
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

#include "gstdshowaudiosrc.h"

GST_DEBUG_CATEGORY_STATIC (dshowaudiosrc_debug);
#define GST_CAT_DEFAULT dshowaudiosrc_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string){ "
	GST_AUDIO_NE (S16) ", "
	GST_AUDIO_NE (U16) ", "
	GST_AUDIO_NE (S8)  ", "
	GST_AUDIO_NE (U8)
        " }, "
        "rate = " GST_AUDIO_RATE_RANGE ", "
        "channels = (int) [ 1, 2 ]")
    );

G_DEFINE_TYPE(GstDshowAudioSrc, gst_dshowaudiosrc, GST_TYPE_AUDIO_SRC);

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_DEVICE_INDEX
};

#define DEFAULT_PROP_DEVICE_INDEX 0


static void gst_dshowaudiosrc_dispose (GObject * gobject);
static void gst_dshowaudiosrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dshowaudiosrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_dshowaudiosrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static GstStateChangeReturn gst_dshowaudiosrc_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_dshowaudiosrc_open (GstAudioSrc * asrc);
static gboolean gst_dshowaudiosrc_prepare (GstAudioSrc * asrc,
    GstAudioRingBufferSpec * spec);
static gboolean gst_dshowaudiosrc_unprepare (GstAudioSrc * asrc);
static gboolean gst_dshowaudiosrc_close (GstAudioSrc * asrc);
static guint gst_dshowaudiosrc_read (GstAudioSrc * asrc, gpointer data,
    guint length, GstClockTime *timestamp);
static guint gst_dshowaudiosrc_delay (GstAudioSrc * asrc);
static void gst_dshowaudiosrc_reset (GstAudioSrc * asrc);

/* utils */
static GstCaps *gst_dshowaudiosrc_getcaps_from_streamcaps (GstDshowAudioSrc *
    src, IPin * pin, IAMStreamConfig * streamcaps);
static gboolean gst_dshowaudiosrc_push_buffer (guint8 * buffer, guint size,
    gpointer src_object, GstClockTime duration);

static void
gst_dshowaudiosrc_class_init (GstDshowAudioSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstAudioSrcClass *gstaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstaudiosrc_class = (GstAudioSrcClass *) klass;

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_get_property);

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_get_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_change_state);

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_open);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_prepare);
  gstaudiosrc_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_unprepare);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_read);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_dshowaudiosrc_reset);

  g_object_class_install_property
      (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Directshow device reference (classID/name)", NULL,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  g_object_class_install_property
      (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the sound device", NULL,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  g_object_class_install_property
      (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Index of the enumerated audio device", 0, G_MAXINT,
          DEFAULT_PROP_DEVICE_INDEX,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "Directshow audio capture source", "Source/Audio",
      "Receive data from a directshow audio capture graph",
      "Sebastien Moutte <sebastien@moutte.net>");

  GST_DEBUG_CATEGORY_INIT (dshowaudiosrc_debug, "dshowaudiosrc", 0,
      "Directshow audio source");
}

static void
gst_dshowaudiosrc_init (GstDshowAudioSrc * src)
{
  src->device = NULL;
  src->device_name = NULL;
  src->device_index = DEFAULT_PROP_DEVICE_INDEX;
  src->audio_cap_filter = NULL;
  src->dshow_fakesink = NULL;
  src->media_filter = NULL;
  src->filter_graph = NULL;
  src->caps = NULL;
  src->pins_mediatypes = NULL;

  src->gbarray = g_byte_array_new ();
  g_mutex_init(&src->gbarray_lock);

  src->is_running = FALSE;

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
}

static void
gst_dshowaudiosrc_dispose (GObject * gobject)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (gobject);

  if (src->device) {
    g_free (src->device);
    src->device = NULL;
  }

  if (src->device_name) {
    g_free (src->device_name);
    src->device_name = NULL;
  }

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->pins_mediatypes) {
    gst_dshow_free_pins_mediatypes (src->pins_mediatypes);
    src->pins_mediatypes = NULL;
  }

  if (src->gbarray) {
    g_byte_array_free (src->gbarray, TRUE);
    src->gbarray = NULL;
  }

  g_mutex_clear(&src->gbarray_lock);

  /* clean dshow */
  if (src->audio_cap_filter)
    src->audio_cap_filter->Release ();

  CoUninitialize ();

  G_OBJECT_CLASS (gst_dshowaudiosrc_parent_class)->dispose (gobject);
}


static void
gst_dshowaudiosrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
    {
      if (src->device) {
        g_free (src->device);
        src->device = NULL;
      }
      if (g_value_get_string (value)) {
        src->device = g_value_dup_string (value);;
      }
      break;
    }
    case PROP_DEVICE_NAME:
    {
      if (src->device_name) {
        g_free (src->device_name);
        src->device_name = NULL;
      }
      if (g_value_get_string (value)) {
        src->device_name = g_value_dup_string (value);;
      }
      break;
    }
    case PROP_DEVICE_INDEX:
    {
      src->device_index = g_value_get_int (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dshowaudiosrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDshowAudioSrc *src;

  g_return_if_fail (GST_IS_DSHOWAUDIOSRC (object));
  src = GST_DSHOWAUDIOSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->device);
      break;
    case PROP_DEVICE_NAME:
      g_value_set_string (value, src->device_name);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, src->device_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_dshowaudiosrc_get_caps (GstBaseSrc * basesrc, GstCaps * filter)
{
  HRESULT hres = S_OK;
  IBindCtx *lpbc = NULL;
  IMoniker *audiom = NULL;
  DWORD dwEaten;
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (basesrc);
  gunichar2 *unidevice = NULL;

  if (src->device) {
    g_free (src->device);
    src->device = NULL;
  }

  src->device =
      gst_dshow_getdevice_from_devicename (&CLSID_AudioInputDeviceCategory,
      &src->device_name, &src->device_index);
  if (!src->device) {
    GST_ERROR ("No audio device found.");
    return NULL;
  }
  unidevice =
      g_utf8_to_utf16 (src->device, strlen (src->device), NULL, NULL, NULL);

  if (!src->audio_cap_filter) {
    hres = CreateBindCtx (0, &lpbc);
    if (SUCCEEDED (hres)) {
      hres =
          MkParseDisplayName (lpbc, (LPCOLESTR) unidevice, &dwEaten, &audiom);
      if (SUCCEEDED (hres)) {
        hres = audiom->BindToObject (lpbc, NULL, IID_IBaseFilter,
            (LPVOID *) & src->audio_cap_filter);
        audiom->Release ();
      }
      lpbc->Release ();
    }
  }

  if (src->audio_cap_filter && !src->caps) {
    /* get the capture pins supported types */
    IPin *capture_pin = NULL;
    IEnumPins *enumpins = NULL;
    HRESULT hres;

    hres = src->audio_cap_filter->EnumPins (&enumpins);
    if (SUCCEEDED (hres)) {
      while (enumpins->Next (1, &capture_pin, NULL) == S_OK) {
        IKsPropertySet *pKs = NULL;

        hres =
            capture_pin->QueryInterface (IID_IKsPropertySet, (LPVOID *) & pKs);
        if (SUCCEEDED (hres) && pKs) {
          DWORD cbReturned;
          GUID pin_category;
          RPC_STATUS rpcstatus;

          hres =
              pKs->Get (AMPROPSETID_Pin,
              AMPROPERTY_PIN_CATEGORY, NULL, 0, &pin_category, sizeof (GUID),
              &cbReturned);

          /* we only want capture pins */
          if (UuidCompare (&pin_category, (UUID *) & PIN_CATEGORY_CAPTURE,
                  &rpcstatus) == 0) {
            IAMStreamConfig *streamcaps = NULL;

            if (SUCCEEDED (capture_pin->QueryInterface (IID_IAMStreamConfig,
                        (LPVOID *) & streamcaps))) {
              src->caps =
                  gst_dshowaudiosrc_getcaps_from_streamcaps (src, capture_pin,
                  streamcaps);
              streamcaps->Release ();
            }
          }
          pKs->Release ();
        }
        capture_pin->Release ();
      }
      enumpins->Release ();
    }
  }

  if (unidevice) {
    g_free (unidevice);
  }

  if (src->caps) {
    GstCaps *caps;

    if (filter) {
      caps = gst_caps_intersect_full (filter, src->caps, GST_CAPS_INTERSECT_FIRST);
    } else {
      caps = gst_caps_ref (src->caps);
    }

    return caps;
  }

  return NULL;
}

static GstStateChangeReturn
gst_dshowaudiosrc_change_state (GstElement * element, GstStateChange transition)
{
  HRESULT hres = S_FALSE;
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (src->media_filter) {
        src->is_running = TRUE;
        hres = src->media_filter->Run (0);
      }
      if (hres != S_OK) {
        GST_ERROR ("Can't RUN the directshow capture graph (error=0x%x)", hres);
        src->is_running = FALSE;
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      if (src->media_filter)
        hres = src->media_filter->Stop ();
      if (hres != S_OK) {
        GST_ERROR ("Can't STOP the directshow capture graph (error=0x%x)",
            hres);
        return GST_STATE_CHANGE_FAILURE;
      }
      src->is_running = FALSE;

      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS(gst_dshowaudiosrc_parent_class)->change_state(element, transition);
}

static gboolean
gst_dshowaudiosrc_open (GstAudioSrc * asrc)
{
  HRESULT hres = S_FALSE;
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);

  hres = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_INPROC,
      IID_IFilterGraph, (LPVOID *) & src->filter_graph);
  if (hres != S_OK || !src->filter_graph) {
    GST_ERROR
        ("Can't create an instance of the directshow graph manager (error=0x%x)",
        hres);
    goto error;
  }

  hres =
      src->filter_graph->QueryInterface (IID_IMediaFilter,
      (LPVOID *) & src->media_filter);
  if (hres != S_OK || !src->media_filter) {
    GST_ERROR
        ("Can't get IMediacontrol interface from the graph manager (error=0x%x)",
        hres);
    goto error;
  }

  src->dshow_fakesink = new CDshowFakeSink;
  src->dshow_fakesink->AddRef ();

  hres = src->filter_graph->AddFilter (src->audio_cap_filter, L"capture");
  if (hres != S_OK) {
    GST_ERROR
        ("Can't add the directshow capture filter to the graph (error=0x%x)",
        hres);
    goto error;
  }

  hres = src->filter_graph->AddFilter (src->dshow_fakesink, L"fakesink");
  if (hres != S_OK) {
    GST_ERROR ("Can't add our fakesink filter to the graph (error=0x%x)", hres);
    goto error;
  }

  return TRUE;

error:
  if (src->dshow_fakesink) {
    src->dshow_fakesink->Release ();
    src->dshow_fakesink = NULL;
  }

  if (src->media_filter) {
    src->media_filter->Release ();
    src->media_filter = NULL;
  }
  if (src->filter_graph) {
    src->filter_graph->Release ();
    src->filter_graph = NULL;
  }

  return FALSE;
}

static gboolean
gst_dshowaudiosrc_prepare (GstAudioSrc * asrc, GstAudioRingBufferSpec * spec)
{
  HRESULT hres;
  IPin *input_pin = NULL;
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);
  GstCaps *current_caps = gst_pad_get_current_caps (GST_BASE_SRC_PAD (asrc));

  if (current_caps) {
    if (gst_caps_is_equal (spec->caps, current_caps)) {
      gst_caps_unref (current_caps);
      return TRUE;
    }
    gst_caps_unref (current_caps);
  }
  /* In 1.0, prepare() seems to be called in the PLAYING state. Most
     of the time you can't do much on a running graph. */

  gboolean was_running = src->is_running;
  if (was_running) {
    HRESULT hres = src->media_filter->Stop ();
    if (hres != S_OK) {
      GST_ERROR("Can't STOP the directshow capture graph for preparing (error=0x%x)", hres);
      return FALSE;
    }
    src->is_running = FALSE;
  }

  /* search the negotiated caps in our caps list to get its index and the corresponding mediatype */
  if (gst_caps_is_subset (spec->caps, src->caps)) {
    guint i = 0;
    gint res = -1;

    for (; i < gst_caps_get_size (src->caps) && res == -1; i++) {
      GstCaps *capstmp = gst_caps_copy_nth (src->caps, i);

      if (gst_caps_is_subset (spec->caps, capstmp)) {
        res = i;
      }
      gst_caps_unref (capstmp);
    }

    if (res != -1 && src->pins_mediatypes) {
      /*get the corresponding media type and build the dshow graph */
      GstCapturePinMediaType *pin_mediatype = NULL;
      GList *type = g_list_nth (src->pins_mediatypes, res);

      if (type) {
        pin_mediatype = (GstCapturePinMediaType *) type->data;

        src->dshow_fakesink->gst_set_media_type (pin_mediatype->mediatype);
        src->dshow_fakesink->gst_set_buffer_callback (
            (push_buffer_func) gst_dshowaudiosrc_push_buffer, src);

        gst_dshow_get_pin_from_filter (src->dshow_fakesink, PINDIR_INPUT,
            &input_pin);
        if (!input_pin) {
          GST_ERROR ("Can't get input pin from our directshow fakesink filter");
          goto error;
        }

        spec->segsize = (gint) (spec->info.bpf * spec->info.rate * spec->latency_time /
            GST_MSECOND);
        spec->segtotal = (gint) ((gfloat) spec->buffer_time /
            (gfloat) spec->latency_time + 0.5);
        if (!gst_dshow_configure_latency (pin_mediatype->capture_pin,
            spec->segsize))
        {
          GST_WARNING ("Could not change capture latency");
          spec->segsize = spec->info.rate * spec->info.channels;
          spec->segtotal = 2;
        };
        GST_INFO ("Configuring with segsize:%d segtotal:%d", spec->segsize, spec->segtotal);

        if (gst_dshow_is_pin_connected (pin_mediatype->capture_pin)) {
          GST_DEBUG_OBJECT (src,
              "capture_pin already connected, disconnecting");
          src->filter_graph->Disconnect (pin_mediatype->capture_pin);
        }

        if (gst_dshow_is_pin_connected (input_pin)) {
          GST_DEBUG_OBJECT (src, "input_pin already connected, disconnecting");
          src->filter_graph->Disconnect (input_pin);
        }

        hres = src->filter_graph->ConnectDirect (pin_mediatype->capture_pin,
            input_pin, NULL);
        input_pin->Release ();

        if (hres != S_OK) {
          GST_ERROR
              ("Can't connect capture filter with fakesink filter (error=0x%x)",
              hres);
          goto error;
        }

      }
    }
  }

  if (was_running) {
    HRESULT hres = src->media_filter->Run (0);
    if (hres != S_OK) {
      GST_ERROR("Can't RUN the directshow capture graph after prepare (error=0x%x)", hres);
      return FALSE;
    }

    src->is_running = TRUE;
  }

  return TRUE;

error:
  /* Don't restart the graph, we're out anyway. */
  return FALSE;
}

static gboolean
gst_dshowaudiosrc_unprepare (GstAudioSrc * asrc)
{
  IPin *input_pin = NULL, *output_pin = NULL;
  HRESULT hres = S_FALSE;
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);

  /* disconnect filters */
  gst_dshow_get_pin_from_filter (src->audio_cap_filter, PINDIR_OUTPUT,
      &output_pin);
  if (output_pin) {
    hres = src->filter_graph->Disconnect (output_pin);
    output_pin->Release ();
  }

  gst_dshow_get_pin_from_filter (src->dshow_fakesink, PINDIR_INPUT, &input_pin);
  if (input_pin) {
    hres = src->filter_graph->Disconnect (input_pin);
    input_pin->Release ();
  }

  return TRUE;
}

static gboolean
gst_dshowaudiosrc_close (GstAudioSrc * asrc)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);

  if (!src->filter_graph)
    return TRUE;

  /*remove filters from the graph */
  src->filter_graph->RemoveFilter (src->audio_cap_filter);
  src->filter_graph->RemoveFilter (src->dshow_fakesink);

  /*release our gstreamer dshow sink */
  src->dshow_fakesink->Release ();
  src->dshow_fakesink = NULL;

  /*release media filter interface */
  src->media_filter->Release ();
  src->media_filter = NULL;

  /*release the filter graph manager */
  src->filter_graph->Release ();
  src->filter_graph = NULL;

  return TRUE;
}

static guint
gst_dshowaudiosrc_read (GstAudioSrc * asrc, gpointer data, guint length, GstClockTime *timestamp)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);
  guint ret = 0;

  if (!src->is_running)
    return -1;

  if (src->gbarray) {
  test:
    if (src->gbarray->len >= length) {
      g_mutex_lock (&src->gbarray_lock);
      memcpy (data, src->gbarray->data + (src->gbarray->len - length), length);
      g_byte_array_remove_range (src->gbarray, src->gbarray->len - length,
          length);
      ret = length;
      g_mutex_unlock (&src->gbarray_lock);
    } else {
      if (src->is_running) {
        Sleep (GST_AUDIO_BASE_SRC(src)->ringbuffer->spec.latency_time /
            GST_MSECOND / 10);
        goto test;
      }
    }
  }

  return ret;
}

static guint
gst_dshowaudiosrc_delay (GstAudioSrc * asrc)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);
  guint ret = 0;

  if (src->gbarray) {
    g_mutex_lock (&src->gbarray_lock);
    if (src->gbarray->len) {
      ret = src->gbarray->len / 4;
    }
    g_mutex_unlock (&src->gbarray_lock);
  }

  return ret;
}

static void
gst_dshowaudiosrc_reset (GstAudioSrc * asrc)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (asrc);

  g_mutex_lock (&src->gbarray_lock);
  GST_DEBUG ("byte array size= %d", src->gbarray->len);
  if (src->gbarray->len > 0)
    g_byte_array_remove_range (src->gbarray, 0, src->gbarray->len);
  g_mutex_unlock (&src->gbarray_lock);
}

static GstCaps *
gst_dshowaudiosrc_getcaps_from_streamcaps (GstDshowAudioSrc * src, IPin * pin,
    IAMStreamConfig * streamcaps)
{
  GstCaps *caps = NULL;
  HRESULT hres = S_OK;
  int icount = 0;
  int isize = 0;
  AUDIO_STREAM_CONFIG_CAPS ascc;
  int i = 0;

  if (!streamcaps)
    return NULL;

  streamcaps->GetNumberOfCapabilities (&icount, &isize);

  if (isize != sizeof (ascc))
    return NULL;

  for (; i < icount; i++) {
    GstCapturePinMediaType *pin_mediatype = g_new0 (GstCapturePinMediaType, 1);

    pin->AddRef ();
    pin_mediatype->capture_pin = pin;

    hres = streamcaps->GetStreamCaps (i, &pin_mediatype->mediatype,
        (BYTE *) & ascc);
    if (hres == S_OK && pin_mediatype->mediatype) {
      GstCaps *mediacaps = NULL;

      if (!caps)
        caps = gst_caps_new_empty ();

      if (gst_dshow_check_mediatype (pin_mediatype->mediatype, MEDIASUBTYPE_PCM,
              FORMAT_WaveFormatEx)) {
	GstAudioFormat format = GST_AUDIO_FORMAT_UNKNOWN;
        WAVEFORMATEX *wavformat =
            (WAVEFORMATEX *) pin_mediatype->mediatype->pbFormat;

	switch (wavformat->wFormatTag) {
            case WAVE_FORMAT_PCM:
	      format = gst_audio_format_build_integer (TRUE, G_BYTE_ORDER, wavformat->wBitsPerSample, wavformat->wBitsPerSample);
	      break;
            default:
	      break;
	}

	if (format != GST_AUDIO_FORMAT_UNKNOWN) {
	  GstAudioInfo info;

	  gst_audio_info_init(&info);
	  gst_audio_info_set_format(&info,
				    format,
				    wavformat->nSamplesPerSec,
				    wavformat->nChannels,
				    NULL);
	  mediacaps = gst_audio_info_to_caps(&info);
	}

        if (mediacaps) {
          src->pins_mediatypes =
              g_list_append (src->pins_mediatypes, pin_mediatype);
          gst_caps_append (caps, mediacaps);
        } else {
          gst_dshow_free_pin_mediatype (pin_mediatype);
        }
      } else {
        gst_dshow_free_pin_mediatype (pin_mediatype);
      }
    } else {
      gst_dshow_free_pin_mediatype (pin_mediatype);
    }
  }

  if (caps && gst_caps_is_empty (caps)) {
    gst_caps_unref (caps);
    caps = NULL;
  }

  return caps;
}

static gboolean
gst_dshowaudiosrc_push_buffer (guint8 * buffer, guint size, gpointer src_object,
    GstClockTime duration)
{
  GstDshowAudioSrc *src = GST_DSHOWAUDIOSRC (src_object);

  if (!buffer || size == 0 || !src) {
    return FALSE;
  }

  g_mutex_lock (&src->gbarray_lock);
  g_byte_array_prepend (src->gbarray, buffer, size);
  g_mutex_unlock (&src->gbarray_lock);

  return TRUE;
}
