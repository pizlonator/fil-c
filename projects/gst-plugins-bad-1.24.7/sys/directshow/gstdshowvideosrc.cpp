/* GStreamer
 * Copyright (C)  2007 Sebastien Moutte <sebastien@moutte.net>
 * Copyright (C)  2009 Julien Isorce <julien.isorce@gmail.com>
 *
 * gstdshowvideosrc.c:
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

#include "gstdshowvideosrc.h"

#include <gst/video/video.h>

GST_DEBUG_CATEGORY_EXTERN (dshowvideosrc_debug);
#define GST_CAT_DEFAULT dshowvideosrc_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE ("{ I420, BGR, YUY2, UYVY, BGRx, BGR16, BGR15, "
                             "GRAY8 }") "; "

        "video/x-dv, "
        "format= (string) DVSD, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ", "
        "systemstream = (boolean) { TRUE, FALSE }; "

        "image/jpeg, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE "; "

        "video/x-h264, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE
        )
    );

G_DEFINE_TYPE (GstDshowVideoSrc, gst_dshowvideosrc, GST_TYPE_PUSH_SRC)

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_DEVICE_INDEX
};

#define DEFAULT_PROP_DEVICE_INDEX 0


static void gst_dshowvideosrc_dispose (GObject * gobject);
static void gst_dshowvideosrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dshowvideosrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_dshowvideosrc_change_state (GstElement *
    element, GstStateChange transition);


static gboolean gst_dshowvideosrc_start (GstBaseSrc * bsrc);
static gboolean gst_dshowvideosrc_stop (GstBaseSrc * bsrc);
static gboolean gst_dshowvideosrc_unlock (GstBaseSrc * bsrc);
static gboolean gst_dshowvideosrc_unlock_stop (GstBaseSrc * bsrc);
static gboolean gst_dshowvideosrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static GstCaps *gst_dshowvideosrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static GstCaps *gst_dshowvideosrc_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static GstFlowReturn gst_dshowvideosrc_create (GstPushSrc * psrc,
    GstBuffer ** buf);

/*utils*/
GstCaps *gst_dshowvideosrc_getcaps_from_streamcaps (IPin * pin,
    GList ** pins_mediatypes);
GstCaps *gst_dshowvideosrc_getcaps_from_enum_mediatypes (IPin * pin,
    GList ** pins_mediatypes);

static gboolean gst_dshowvideosrc_push_buffer (guint8 * buffer, guint size,
    gpointer src_object, GstClockTime duration);

static void
gst_dshowvideosrc_class_init (GstDshowVideoSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_dshowvideosrc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_dshowvideosrc_get_property);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dshowvideosrc_change_state);

  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_set_caps);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_get_caps);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_src_fixate);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_dshowvideosrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_dshowvideosrc_create);

  g_object_class_install_property
      (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Directshow device path (@..classID/name)", NULL,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  g_object_class_install_property
      (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the video device", NULL,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  g_object_class_install_property
      (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Index of the enumerated video device", 0, G_MAXINT,
          DEFAULT_PROP_DEVICE_INDEX,
          static_cast < GParamFlags > (G_PARAM_READWRITE)));

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "DirectShow video capture source", "Source/Video",
      "Receive data from a directshow video capture graph",
      "Sebastien Moutte <sebastien@moutte.net>");
}

static void
gst_dshowvideosrc_init (GstDshowVideoSrc * src)
{
  src->device = NULL;
  src->device_name = NULL;
  src->device_index = DEFAULT_PROP_DEVICE_INDEX;
  src->video_cap_filter = NULL;
  src->dshow_fakesink = NULL;
  src->media_filter = NULL;
  src->filter_graph = NULL;
  src->caps = NULL;
  src->pins_mediatypes = NULL;
  src->is_rgb = FALSE;
  src->is_running = FALSE;

  /*added for analog input*/
  src->graph_builder = NULL;
  src->capture_builder = NULL;
  src->pVC = NULL;
  src->pVSC = NULL;

  g_cond_init(&src->buffer_cond);
  g_mutex_init(&src->buffer_mutex);
  src->buffer = NULL;
  src->stop_requested = FALSE;

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
}

static GstCaps *
gst_dshowvideosrc_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  /* If there is no desired video size, set default video size to device preferred video size */

  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  guint i = 0;
  gint res = -1;

  for (; i < gst_caps_get_size (src->caps) && res == -1; i++) {
    GstCaps *capstmp = gst_caps_copy_nth (src->caps, i);

    if (gst_caps_is_subset (caps, capstmp)) {
      res = i;
    }
    gst_caps_unref (capstmp);
  }

  if (res != -1) {
    GList *type_pin_mediatype = g_list_nth (src->pins_mediatypes, res);
    if (type_pin_mediatype) {
      GstCapturePinMediaType *pin_mediatype =
          (GstCapturePinMediaType *) type_pin_mediatype->data;
      gst_structure_fixate_field_nearest_int (structure, "width",
          pin_mediatype->defaultWidth);
      gst_structure_fixate_field_nearest_int (structure, "height",
          pin_mediatype->defaultHeight);
      gst_structure_fixate_field_nearest_fraction (structure, "framerate",
          pin_mediatype->defaultFPS, 1);
    }
  }

  caps = GST_BASE_SRC_CLASS (gst_dshowvideosrc_parent_class)->fixate(bsrc, caps);

  return caps;
}

static void
gst_dshowvideosrc_dispose (GObject * gobject)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (gobject);

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

  /* clean dshow */
  if (src->video_cap_filter) {
    src->video_cap_filter->Release ();
    src->video_cap_filter = NULL;
  }

  g_cond_clear(&src->buffer_cond);
  g_mutex_clear(&src->buffer_mutex);

  CoUninitialize ();

  G_OBJECT_CLASS (gst_dshowvideosrc_parent_class)->dispose (gobject);
}

static void
gst_dshowvideosrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
    {
      const gchar *device = g_value_get_string (value);
      g_free (src->device);
      src->device = NULL;
      if (device && strlen (device) != 0) {
        src->device = g_value_dup_string (value);
      }
      break;
    }
    case PROP_DEVICE_NAME:
    {
      const gchar *device_name = g_value_get_string (value);
      g_free (src->device_name);
      src->device_name = NULL;
      if (device_name && strlen (device_name) != 0) {
        src->device_name = g_value_dup_string (value);
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
gst_dshowvideosrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDshowVideoSrc *src;

  g_return_if_fail (GST_IS_DSHOWVIDEOSRC (object));
  src = GST_DSHOWVIDEOSRC (object);

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
gst_dshowvideosrc_get_caps (GstBaseSrc * basesrc, GstCaps * filter)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (basesrc);
  GstCaps *caps;

  if (src->caps) {
    caps = gst_caps_ref (src->caps);
  } else {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  }

  if (caps) {
    GstCaps *filtcaps;

    if (filter) {
      filtcaps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    } else {
      filtcaps = gst_caps_ref (caps);
    }
    gst_caps_unref (caps);

    return filtcaps;
  }

  return NULL;
}

GstCaps *
gst_dshowvideosrc_getcaps_from_capture_filter (IBaseFilter * filter,
    GList ** pins_mediatypes)
{
  IPin *capture_pin = NULL;
  IEnumPins *enumpins = NULL;
  HRESULT hres;
  GstCaps *caps;

  g_assert (filter);

  caps = gst_caps_new_empty ();

  /* get the capture pins supported types */
  hres = filter->EnumPins (&enumpins);
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
          GstCaps *caps2;
          caps2 = gst_dshowvideosrc_getcaps_from_streamcaps (capture_pin,
              pins_mediatypes);
          if (caps2) {
            gst_caps_append (caps, caps2);
          } else {
            caps2 = gst_dshowvideosrc_getcaps_from_enum_mediatypes (
                capture_pin, pins_mediatypes);
            if (caps2) {
              gst_caps_append (caps, caps2);
            }
          }
        }
        pKs->Release ();
      }
      capture_pin->Release ();
    }
    enumpins->Release ();
  }

  GST_DEBUG ("Device supports these caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstStateChangeReturn
gst_dshowvideosrc_change_state (GstElement * element, GstStateChange transition)
{
  HRESULT hres = S_FALSE;
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (src->media_filter) {
	/* Setting this to TRUE because set_caps may be invoked before
	   Run() returns. */
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
        GST_ERROR ("Can't STOP the directshow capture graph (error=%d)", hres);
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

  return GST_ELEMENT_CLASS(gst_dshowvideosrc_parent_class)->change_state(element, transition);
}

static gboolean
gst_dshowvideosrc_start (GstBaseSrc * bsrc)
{
  HRESULT hres = S_FALSE;
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);
  DshowDeviceEntry *device_entry;
  IMoniker *moniker = NULL;

  device_entry = gst_dshow_select_device (&CLSID_VideoInputDeviceCategory,
      src->device, src->device_name, src->device_index);
  if (device_entry == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Failed to find device"), (NULL));
    return FALSE;
  }

  g_free (src->device);
  g_free (src->device_name);
  src->device = g_strdup (device_entry->device);
  src->device_name = g_strdup (device_entry->device_name);
  src->device_index = device_entry->device_index;
  moniker = device_entry->moniker;
  device_entry->moniker = NULL;
  gst_dshow_device_entry_free (device_entry);

  src->video_cap_filter = gst_dshow_create_capture_filter (moniker);
  moniker->Release ();
  if (src->video_cap_filter == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to create capture filter for device"), (NULL));
    return FALSE;
  }

  src->caps = gst_dshowvideosrc_getcaps_from_capture_filter (
      src->video_cap_filter, (GList**)&src->pins_mediatypes);
  if (gst_caps_is_empty (src->caps)) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to get any caps from device"), (NULL));
    return FALSE;
  }

  /*
  The filter graph now is created via the IGraphBuilder Interface
  Code added to build upstream filters, needed for USB Analog TV Tuners / DVD Maker, based on AMCap code.
  by Fabrice Costa <fabricio.costa@moldeointeractive.com.ar>
  */

  hres =  CoCreateInstance(CLSID_FilterGraph, NULL,
    CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (LPVOID *) & src->graph_builder );
  if (hres != S_OK || !src->graph_builder ) {
    GST_ERROR
        ("Can't create an instance of the dshow graph builder (error=0x%x)",
        hres);
    goto error;
  } else {
	/*graph builder is derived from IFilterGraph so we can assign it to the old src->filter_graph*/
	src->filter_graph = (IFilterGraph*) src->graph_builder;
  }

  /*adding capture graph builder to correctly create upstream filters, Analog TV, TV Tuner */

  hres = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
        CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
        (LPVOID *) & src->capture_builder);
  if ( hres != S_OK || !src->capture_builder ) {
    GST_ERROR
        ("Can't create an instance of the dshow capture graph builder manager (error=0x%x)",
        hres);
	goto error;
  } else {
	src->capture_builder->SetFiltergraph(src->graph_builder);
  }

  hres = src->filter_graph->QueryInterface (IID_IMediaFilter,
      (LPVOID *) & src->media_filter);
  if (hres != S_OK || !src->media_filter) {
    GST_ERROR
        ("Can't get IMediacontrol interface from the graph manager (error=0x%x)",
        hres);
    goto error;
  }

  src->dshow_fakesink = new CDshowFakeSink;
  src->dshow_fakesink->AddRef ();

  hres = src->filter_graph->AddFilter (src->video_cap_filter, L"capture");
  if (hres != S_OK) {
    GST_ERROR ("Can't add video capture filter to the graph (error=0x%x)",
        hres);
    goto error;
  }

  /* Finding interfaces really creates the upstream filters */

  hres = src->capture_builder->FindInterface(&PIN_CATEGORY_CAPTURE,
                                      &MEDIATYPE_Interleaved, src->video_cap_filter,
                                      IID_IAMVideoCompression, (LPVOID *)&src->pVC);

  if(hres != S_OK)
  {
	hres = src->capture_builder->FindInterface(&PIN_CATEGORY_CAPTURE,
                                          &MEDIATYPE_Video, src->video_cap_filter,
                                          IID_IAMVideoCompression, (LPVOID *)&src->pVC);
  }

  hres = src->capture_builder->FindInterface(&PIN_CATEGORY_CAPTURE,
                                      &MEDIATYPE_Interleaved,
                                      src->video_cap_filter, IID_IAMStreamConfig, (LPVOID *)&src->pVSC);
  if(hres != S_OK)
  {
	  hres = src->capture_builder->FindInterface(&PIN_CATEGORY_CAPTURE,
											&MEDIATYPE_Video, src->video_cap_filter,
											IID_IAMStreamConfig, (LPVOID *)&src->pVSC);
	  if (hres != S_OK) {
                  /* this means we can't set frame rate (non-DV only) */
		  GST_ERROR ("Error %x: Cannot find VCapture:IAMStreamConfig",	hres);
			goto error;
	  }
  }

  hres = src->filter_graph->AddFilter (src->dshow_fakesink, L"sink");
  if (hres != S_OK) {
    GST_ERROR ("Can't add our fakesink filter to the graph (error=0x%x)", hres);
    goto error;
  }

  return TRUE;

error:
  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
     ("Failed to build filter graph"), (NULL));

  if (src->dshow_fakesink) {
    src->dshow_fakesink->Release ();
    src->dshow_fakesink = NULL;
  }

  if (src->media_filter) {
    src->media_filter->Release ();
    src->media_filter = NULL;
  }
  if (src->graph_builder) {
    src->graph_builder->Release ();
    src->graph_builder = NULL;
  }
  if (src->capture_builder) {
    src->capture_builder->Release ();
    src->capture_builder = NULL;
  }
  if (src->pVC) {
    src->pVC->Release ();
    src->pVC = NULL;
  }
  if (src->pVSC) {
    src->pVSC->Release ();
    src->pVSC = NULL;
  }

  return FALSE;
}

static gboolean
gst_dshowvideosrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  HRESULT hres;
  IPin *input_pin = NULL;
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  GstCaps *current_caps = gst_pad_get_current_caps (GST_BASE_SRC_PAD (bsrc));

  if (current_caps) {
    if (gst_caps_is_equal (caps, current_caps)) {
      gst_caps_unref (current_caps);
      return TRUE;
    }
    gst_caps_unref (current_caps);
  }

  /* Same remark as in gstdshowaudiosrc. */
  gboolean was_running = src->is_running;
  if (was_running) {
    HRESULT hres = src->media_filter->Stop ();
    if (hres != S_OK) {
      GST_ERROR ("Can't STOP the directshow capture graph (error=0x%x)", hres);
      return FALSE;
    }
    src->is_running = FALSE;
  }

  /* search the negotiated caps in our caps list to get its index and the corresponding mediatype */
  if (gst_caps_is_subset (caps, src->caps)) {
    guint i = 0;
    gint res = -1;

    for (; i < gst_caps_get_size (src->caps) && res == -1; i++) {
      GstCaps *capstmp = gst_caps_copy_nth (src->caps, i);

      if (gst_caps_is_subset (caps, capstmp)) {
        res = i;
      }
      gst_caps_unref (capstmp);
    }

    if (res != -1 && src->pins_mediatypes) {
      /* get the corresponding media type and build the dshow graph */
      GList *type_pin_mediatype = g_list_nth (src->pins_mediatypes, res);

      if (type_pin_mediatype) {
        GstCapturePinMediaType *pin_mediatype =
            (GstCapturePinMediaType *) type_pin_mediatype->data;
        gchar *caps_string = NULL;
        gchar *src_caps_string = NULL;

        GST_DEBUG_OBJECT (src, "Default: %dx%d@%d", pin_mediatype->defaultWidth, pin_mediatype->defaultHeight, pin_mediatype->defaultFPS);

        /* retrieve the desired video size */
        VIDEOINFOHEADER *video_info = NULL;
        gint width = 0;
        gint height = 0;
        gint numerator = 0;
        gint denominator = 0;
        gst_structure_get_int (s, "width", &width);
        gst_structure_get_int (s, "height", &height);
        gst_structure_get_fraction (s, "framerate", &numerator, &denominator);

        /* check if the desired video size is valid about granularity  */
        /* This check will be removed when GST_TYPE_INT_RANGE_STEP exits */
        /* See remarks in gst_dshow_new_video_caps function */
        if (pin_mediatype->granularityWidth != 0
            && width % pin_mediatype->granularityWidth != 0)
          g_warning ("your desired video size is not valid : %d mod %d !=0\n",
              width, pin_mediatype->granularityWidth);
        if (pin_mediatype->granularityHeight != 0
            && height % pin_mediatype->granularityHeight != 0)
          g_warning ("your desired video size is not valid : %d mod %d !=0\n",
              height, pin_mediatype->granularityHeight);

        /* update mediatype */
        video_info = (VIDEOINFOHEADER *) pin_mediatype->mediatype->pbFormat;
        video_info->bmiHeader.biWidth = width;
        video_info->bmiHeader.biHeight = height;
        video_info->AvgTimePerFrame =
            (LONGLONG) (10000000 * denominator / (double) numerator);
        video_info->bmiHeader.biSizeImage = DIBSIZE (video_info->bmiHeader);
        pin_mediatype->mediatype->lSampleSize = DIBSIZE (video_info->bmiHeader);

        src->dshow_fakesink->gst_set_media_type (pin_mediatype->mediatype);
        src->dshow_fakesink->gst_set_buffer_callback (
            (push_buffer_func) gst_dshowvideosrc_push_buffer, src);

        gst_dshow_get_pin_from_filter (src->dshow_fakesink, PINDIR_INPUT,
            &input_pin);
        if (!input_pin) {
          GST_ERROR ("Can't get input pin from our dshow fakesink");
          goto error;
        }

        if (gst_dshow_is_pin_connected (pin_mediatype->capture_pin)) {
          GST_DEBUG_OBJECT (src,
              "capture_pin already connected, disconnecting");
          src->filter_graph->Disconnect (pin_mediatype->capture_pin);
        }

        if (gst_dshow_is_pin_connected (input_pin)) {
          GST_DEBUG_OBJECT (src, "input_pin already connected, disconnecting");
          src->filter_graph->Disconnect (input_pin);
        }

        hres = src->pVSC->SetFormat(pin_mediatype->mediatype);
        if (FAILED (hres)) {
          GST_ERROR ("Failed to set capture pin format (error=0x%x)", hres);
          goto error;
        }

        hres = src->filter_graph->ConnectDirect (pin_mediatype->capture_pin,
            input_pin, pin_mediatype->mediatype);
        input_pin->Release ();

        if (hres != S_OK) {
          GST_ERROR
              ("Can't connect capture filter with fakesink filter (error=0x%x)",
              hres);
          goto error;
        }

        /* save width and height negotiated */
        gst_structure_get_int (s, "width", &src->width);
        gst_structure_get_int (s, "height", &src->height);

	GstVideoInfo info;
	gst_video_info_from_caps(&info, caps);
	switch (GST_VIDEO_INFO_FORMAT(&info)) {
          case GST_VIDEO_FORMAT_RGB:
          case GST_VIDEO_FORMAT_BGR:
	    src->is_rgb = TRUE;
	    break;
	default:
	  src->is_rgb = FALSE;
	  break;
	}
      }
    }
  }

  if (was_running) {
    HRESULT hres = src->media_filter->Run (0);
    if (hres != S_OK) {
      GST_ERROR ("Can't RUN the directshow capture graph (error=0x%x)", hres);
      return FALSE;
    }
    src->is_running = TRUE;
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_dshowvideosrc_stop (GstBaseSrc * bsrc)
{
  IPin *input_pin = NULL, *output_pin = NULL;
  HRESULT hres = S_FALSE;
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);

  if (!src->filter_graph)
    return TRUE;

  /* disconnect filters */
  gst_dshow_get_pin_from_filter (src->video_cap_filter, PINDIR_OUTPUT,
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

  /* remove filters from the graph */
  src->filter_graph->RemoveFilter (src->video_cap_filter);
  src->filter_graph->RemoveFilter (src->dshow_fakesink);

  /* release our gstreamer dshow sink */
  src->dshow_fakesink->Release ();
  src->dshow_fakesink = NULL;

  /* release media filter interface */
  src->media_filter->Release ();
  src->media_filter = NULL;

  /* release any upstream filter */
  if (src->pVC) {
      src->pVC->Release ();
      src->pVC = NULL;
  }

  if (src->pVSC) {
      src->pVSC->Release ();
      src->pVSC = NULL;
  }

/* release the graph builder */
  if (src->graph_builder) {
    src->graph_builder->Release ();
    src->graph_builder = NULL;
    src->filter_graph = NULL;
  }

/* release the capture builder */
  if (src->capture_builder) {
    src->capture_builder->Release ();
    src->capture_builder = NULL;
  }

  /* reset caps */
  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  /* reset device id */
  if (src->device) {
    g_free (src->device);
    src->device = NULL;
  }

  if (src->video_cap_filter) {
    src->video_cap_filter->Release ();
    src->video_cap_filter = NULL;
  }

  return TRUE;
}

static gboolean
gst_dshowvideosrc_unlock (GstBaseSrc * bsrc)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);

  g_mutex_lock (&src->buffer_mutex);
  src->stop_requested = TRUE;
  g_cond_signal (&src->buffer_cond);
  g_mutex_unlock (&src->buffer_mutex);

  return TRUE;
}

static gboolean
gst_dshowvideosrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (bsrc);

  src->stop_requested = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_dshowvideosrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (psrc);

  g_mutex_lock (&src->buffer_mutex);
  while (src->buffer == NULL && !src->stop_requested)
    g_cond_wait (&src->buffer_cond, &src->buffer_mutex);
  *buf = src->buffer;
  src->buffer = NULL;
  g_mutex_unlock (&src->buffer_mutex);

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  GST_DEBUG ("dshowvideosrc_create => pts %" GST_TIME_FORMAT " duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*buf)));

  return GST_FLOW_OK;
}

GstCaps *
gst_dshowvideosrc_getcaps_from_streamcaps (IPin * pin, GList ** pins_mediatypes)
{
  GstCaps *caps = NULL;
  HRESULT hres = S_OK;
  int icount = 0;
  int isize = 0;
  int i = 0;
  IAMStreamConfig *streamcaps = NULL;

  hres = pin->QueryInterface (IID_IAMStreamConfig, (LPVOID *) & streamcaps);
  if (FAILED (hres)) {
    GST_ERROR ("Failed to retrieve IAMStreamConfig (error=0x%x)", hres);
    return NULL;
  }

  streamcaps->GetNumberOfCapabilities (&icount, &isize);

  if (isize != sizeof (VIDEO_STREAM_CONFIG_CAPS)) {
    streamcaps->Release ();
    return NULL;
  }

  caps = gst_caps_new_empty ();

  for (i = 0; i < icount; i++) {

    GstCapturePinMediaType *pin_mediatype =
      gst_dshow_new_pin_mediatype_from_streamcaps (pin, i, streamcaps);

    if (pin_mediatype) {

      GstCaps *mediacaps = NULL;
      GstVideoFormat video_format =
        gst_dshow_guid_to_gst_video_format (pin_mediatype->mediatype);

      if (video_format != GST_VIDEO_FORMAT_UNKNOWN) {
        mediacaps = gst_dshow_new_video_caps (video_format, NULL,
            pin_mediatype);

      } else if (gst_dshow_check_mediatype (pin_mediatype->mediatype,
              MEDIASUBTYPE_dvsd, FORMAT_VideoInfo)) {
        mediacaps =
            gst_dshow_new_video_caps (GST_VIDEO_FORMAT_UNKNOWN,
            "video/x-dv, systemstream=FALSE", pin_mediatype);

      } else if (gst_dshow_check_mediatype (pin_mediatype->mediatype,
              MEDIASUBTYPE_dvsd, FORMAT_DvInfo)) {
        mediacaps =
            gst_dshow_new_video_caps (GST_VIDEO_FORMAT_UNKNOWN,
            "video/x-dv, systemstream=TRUE", pin_mediatype);

        pin_mediatype->granularityWidth = 0;
        pin_mediatype->granularityHeight = 0;

      } else if (gst_dshow_check_mediatype (pin_mediatype->mediatype,
              MEDIASUBTYPE_MJPG, FORMAT_VideoInfo)) {
        mediacaps =
            gst_dshow_new_video_caps (GST_VIDEO_FORMAT_ENCODED,
            "image/jpeg", pin_mediatype);

      } else if (gst_dshow_check_mediatype (pin_mediatype->mediatype,
              MEDIASUBTYPE_H264, FORMAT_VideoInfo)) {
        mediacaps =
            gst_dshow_new_video_caps (GST_VIDEO_FORMAT_ENCODED,
            "video/x-h264", pin_mediatype);
      }

      if (mediacaps) {
        if (pins_mediatypes != NULL) {
          *pins_mediatypes = g_list_append (*pins_mediatypes, pin_mediatype);
        }
        gst_caps_append (caps, mediacaps);
      } else {
        /* failed to convert dshow caps */
        gst_dshow_free_pin_mediatype (pin_mediatype);
      }
    }
  }

  streamcaps->Release ();

  if (caps && gst_caps_is_empty (caps)) {
    gst_caps_unref (caps);
    caps = NULL;
  }

  return caps;
}

GstCaps *
gst_dshowvideosrc_getcaps_from_enum_mediatypes (IPin * pin, GList ** pins_mediatypes)
{
  GstCaps *caps = NULL;
  IEnumMediaTypes *enum_mediatypes = NULL;
  HRESULT hres = S_OK;
  GstCapturePinMediaType *pin_mediatype = NULL;

  hres = pin->EnumMediaTypes (&enum_mediatypes);
  if (FAILED (hres)) {
    GST_ERROR ("Failed to retrieve IEnumMediaTypes (error=0x%x)", hres);
    return NULL;
  }

  caps = gst_caps_new_empty ();

  while ((pin_mediatype = gst_dshow_new_pin_mediatype_from_enum_mediatypes (pin, enum_mediatypes)) != NULL) {

    GstCaps *mediacaps = NULL;
    GstVideoFormat video_format = gst_dshow_guid_to_gst_video_format (pin_mediatype->mediatype);

	if (video_format != GST_VIDEO_FORMAT_UNKNOWN) {
		GstVideoInfo info;

		gst_video_info_init(&info);
		gst_video_info_set_format(&info, video_format, pin_mediatype->defaultWidth, pin_mediatype->defaultHeight);
		info.fps_n = pin_mediatype->defaultFPS;
		info.fps_d = 1;
		info.par_n = 1;
		info.par_d = 1;
		info.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE; /* XXX is this correct ? */
		mediacaps = gst_video_info_to_caps(&info);
	}

    if (mediacaps) {
      if (pins_mediatypes != NULL) {
        *pins_mediatypes = g_list_append (*pins_mediatypes, pin_mediatype);
      }
      gst_caps_append (caps, mediacaps);
    } else {
      /* failed to convert dshow caps */
      gst_dshow_free_pin_mediatype (pin_mediatype);
    }
  }

  enum_mediatypes->Release ();

  if (caps && gst_caps_is_empty (caps)) {
    gst_caps_unref (caps);
    caps = NULL;
  }

  return caps;
}

static gboolean
gst_dshowvideosrc_push_buffer (guint8 * buffer, guint size, gpointer src_object,
    GstClockTime duration)
{
  GstDshowVideoSrc *src = GST_DSHOWVIDEOSRC (src_object);
  GstBuffer *buf = NULL;
  IPin *pPin = NULL;
  HRESULT hres = S_FALSE;
  AM_MEDIA_TYPE *pMediaType = NULL;
  GstMapInfo info;

  if (!buffer || size == 0 || !src) {
    return FALSE;
  }

  /* create a new buffer assign to it the clock time as timestamp */
  buf = gst_buffer_new_and_alloc (size);

  gst_buffer_set_size(buf, size);

  GstClock *clock = gst_element_get_clock (GST_ELEMENT (src));
  GST_BUFFER_TIMESTAMP (buf) =
    GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)), gst_clock_get_time (clock));
  gst_object_unref (clock);

  GST_BUFFER_DURATION (buf) = duration;

  if (!gst_buffer_map(buf, &info, GST_MAP_WRITE)) {
	  gst_buffer_unref(buf);
	  GST_ERROR("Failed to map buffer");
	  return FALSE;
  }

  if (src->is_rgb) {
    /* FOR RGB directshow decoder will return bottom-up BITMAP
     * There is probably a way to get top-bottom video frames from
     * the decoder...
     */
    gint line = 0;
    gint stride = size / src->height;

    for (; line < src->height; line++) {
      memcpy (info.data + (line * stride),
          buffer + (size - ((line + 1) * (stride))), stride);
    }
  } else {
    memcpy (info.data, buffer, size);
  }

  gst_buffer_unmap(buf, &info);

  GST_DEBUG ("push_buffer => pts %" GST_TIME_FORMAT "duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (duration));

  g_mutex_lock (&src->buffer_mutex);
  if (src->buffer != NULL)
    gst_buffer_unref (src->buffer);
  src->buffer = buf;
  g_cond_signal (&src->buffer_cond);
  g_mutex_unlock (&src->buffer_mutex);

  return TRUE;
}
