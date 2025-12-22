/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

/**
 * SECTION:element-d3d11deinterlaceelement
 * @title: d3d11deinterlaceelement
 *
 * Deinterlacing interlaced video frames to progressive video frames by using
 * ID3D11VideoProcessor API. Depending on the hardware it runs on,
 * this element will only support a very limited set of video formats.
 * Use #d3d11deinterlace instead, which will take care of conversion.
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>

#include "gstd3d11deinterlace.h"
#include "gstd3d11pluginutils.h"
#include <wrl.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_deinterlace_debug);
#define GST_CAT_DEFAULT gst_d3d11_deinterlace_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

/* Deinterlacing Methods:
 * Direct3D11 provides Blend, Bob, Adaptive, Motion Compensation, and
 * Inverse Telecine methods. But depending on video processor device,
 * some of method might not be supported.
 * - Blend: the two fields of a interlaced frame are blended into a single
 *   progressive frame. Output rate will be half of input (e.g., 60i -> 30p)
 *   but due to the way of framerate signalling of GStreamer, that is, it uses
 *   frame rate, not field rate for interlaced stream, in/output framerate
 *   of caps will be identical.
 * - Bob: missing field lines are interpolated from the lines above and below.
 *   Output rate will be the same as that of input (e.g., 60i -> 60p).
 *   In order words, video processor will generate two frames from two field
 *   of a intelaced frame.
 * - Adaptive, Motion Compensation: future and past frames are used for
 *   reference frame for deinterlacing process. User should provide sufficent
 *   number of reference frames, otherwise processor device will fallback to
 *   Bob method.
 *
 * Direct3D11 doesn't provide a method for explicit deinterlacing method
 * selection. Instead, it could be done indirectly.
 * - Blend: sets output rate as half via VideoProcessorSetStreamOutputRate().
 * - Bob: sets output rate as normal. And performs VideoProcessorBlt() twice per
 *   a interlaced frame. D3D11_VIDEO_PROCESSOR_STREAM::OutputIndex needs to be
 *   incremented per field (e.g., OutputIndex = 0 for the first field,
 *   and 1 for the second field).
 * - Adaptive, Motion Compensation: in addition to the requirement of Bob,
 *   user should provide reference frames via
 *   D3D11_VIDEO_PROCESSOR_STREAM::ppPastSurfaces and
 *   D3D11_VIDEO_PROCESSOR_STREAM::ppFutureSurfaces
 */

typedef enum
{
  GST_D3D11_DEINTERLACE_METHOD_BLEND =
      D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND,
  GST_D3D11_DEINTERLACE_METHOD_BOB =
      D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB,
  GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE =
      D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE,
  GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION =
      D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION,

  /* TODO: INVERSE_TELECINE */
} GstD3D11DeinterlaceMethod;

DEFINE_ENUM_FLAG_OPERATORS (GstD3D11DeinterlaceMethod);

#define DEINTERLACE_METHOD_ALL \
    ((GstD3D11DeinterlaceMethod) (GST_D3D11_DEINTERLACE_METHOD_BLEND | \
        GST_D3D11_DEINTERLACE_METHOD_BOB | \
        GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE | \
        GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION))

/**
 * GstD3D11DeinterlaceMethod:
 *
 * Deinterlacing method
 *
 * Since: 1.20
 */
#define GST_TYPE_D3D11_DEINTERLACE_METHOD (gst_d3d11_deinterlace_method_type())

static GType
gst_d3d11_deinterlace_method_type (void)
{
  static GType method_type = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    static const GFlagsValue method_types[] = {
      {GST_D3D11_DEINTERLACE_METHOD_BLEND,
          "Blend: Blending top/bottom field pictures into one frame. "
            "Framerate will be preserved (e.g., 60i -> 30p)", "blend"},
      {GST_D3D11_DEINTERLACE_METHOD_BOB,
          "Bob: Interpolating missing lines by using the adjacent lines. "
            "Framerate will be doubled (e,g, 60i -> 60p)", "bob"},
      {GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE,
            "Adaptive: Interpolating missing lines by using spatial/temporal references. "
            "Framerate will be doubled (e,g, 60i -> 60p)",
          "adaptive"},
      {GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION,
          "Motion Compensation: Recreating missing lines by using motion vector. "
            "Framerate will be doubled (e,g, 60i -> 60p)", "mocomp"},
      {0, nullptr, nullptr},
    };

    method_type = g_flags_register_static ("GstD3D11DeinterlaceMethod",
        method_types);
  } GST_D3D11_CALL_ONCE_END;

  return method_type;
}

typedef struct
{
  GstD3D11DeinterlaceMethod supported_methods;
  GstD3D11DeinterlaceMethod default_method;

  guint max_past_frames;
  guint max_future_frames;
} GstD3D11DeinterlaceDeviceCaps;

typedef struct
{
  GType deinterlace_type;

  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint adapter;
  guint device_id;
  guint vendor_id;
  gchar *description;

  GstD3D11DeinterlaceDeviceCaps device_caps;

  guint ref_count;
} GstD3D11DeinterlaceClassData;

static GstD3D11DeinterlaceClassData *
gst_d3d11_deinterlace_class_data_new (void)
{
  GstD3D11DeinterlaceClassData *self = g_new0 (GstD3D11DeinterlaceClassData, 1);

  self->ref_count = 1;

  return self;
}

static GstD3D11DeinterlaceClassData *
gst_d3d11_deinterlace_class_data_ref (GstD3D11DeinterlaceClassData * data)
{
  g_assert (data != NULL);

  g_atomic_int_add (&data->ref_count, 1);

  return data;
}

static void
gst_d3d11_deinterlace_class_data_unref (GstD3D11DeinterlaceClassData * data)
{
  g_assert (data != NULL);

  if (g_atomic_int_dec_and_test (&data->ref_count)) {
    gst_clear_caps (&data->sink_caps);
    gst_clear_caps (&data->src_caps);
    g_free (data->description);
    g_free (data);
  }
}

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_DEVICE_ID,
  PROP_VENDOR_ID,
  PROP_METHOD,
  PROP_SUPPORTED_METHODS,
};

/* hardcoded maximum queue size for each past/future frame queue */
#define MAX_NUM_REFERENCES 2

#define DOC_CAPS \
    "video/x-raw(memory:D3D11Memory), format = (string) { NV12, P010_10LE}, " \
    "width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ]; " \
    "video/x-raw(memory:D3D11Memory, meta:GstVideoOverlayComposition), " \
    "format = (string) { NV12, P010_10LE}, " \
    "width = (int) [ 1, 16384 ], height = (int) [ 1, 16384 ]"

typedef struct _GstD3D11Deinterlace
{
  GstBaseTransform parent;

  GstVideoInfo in_info;
  GstVideoInfo out_info;
  /* Calculated buffer duration by using upstream framerate */
  GstClockTime default_buffer_duration;

  GstD3D11Device *device;

  ID3D11VideoDevice *video_device;
  ID3D11VideoContext *video_context;
  ID3D11VideoProcessorEnumerator *video_enum;
  ID3D11VideoProcessor *video_proc;

  GstD3D11DeinterlaceMethod method;

  CRITICAL_SECTION lock;
  GQueue past_frame_queue;
  GQueue future_frame_queue;
  GstBuffer *to_process;

  guint max_past_frames;
  guint max_future_frames;

  /* D3D11_VIDEO_PROCESSOR_STREAM::InputFrameOrField */
  guint input_index;

  /* Clear/Update per submit_input_buffer() */
  guint num_output_per_input;
  guint num_transformed;
  gboolean first_output;

  GstBufferPool *fallback_in_pool;
  GstBufferPool *fallback_out_pool;
} GstD3D11Deinterlace;

typedef struct _GstD3D11DeinterlaceClass
{
  GstBaseTransformClass parent_class;

  guint adapter;
  guint device_id;
  guint vendor_id;

  GstD3D11DeinterlaceDeviceCaps device_caps;
} GstD3D11DeinterlaceClass;

static GstElementClass *parent_class = NULL;

#define GST_D3D11_DEINTERLACE(object) ((GstD3D11Deinterlace *) (object))
#define GST_D3D11_DEINTERLACE_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object), \
    GstD3D11DeinterlaceClass))

static gboolean
gst_d3d11_deinterlace_update_method (GstD3D11Deinterlace * self);
static void gst_d3d11_deinterlace_reset (GstD3D11Deinterlace * self);
static GstFlowReturn gst_d3d11_deinterlace_drain (GstD3D11Deinterlace * self);

/* GObjectClass vfunc */
static void gst_d3d11_deinterlace_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_d3d11_deinterlace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_deinterlace_finalize (GObject * object);

/* GstElementClass vfunc */
static void gst_d3d11_deinterlace_set_context (GstElement * element,
    GstContext * context);

/* GstBaseTransformClass vfunc */
static gboolean gst_d3d11_deinterlace_start (GstBaseTransform * trans);
static gboolean gst_d3d11_deinterlace_stop (GstBaseTransform * trans);
static gboolean gst_d3d11_deinterlace_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static GstCaps *gst_d3d11_deinterlace_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_d3d11_deinterlace_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean
gst_d3d11_deinterlace_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean
gst_d3d11_deinterlace_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_d3d11_deinterlace_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn
gst_d3d11_deinterlace_submit_input_buffer (GstBaseTransform * trans,
    gboolean is_discont, GstBuffer * input);
static GstFlowReturn
gst_d3d11_deinterlace_generate_output (GstBaseTransform * trans,
    GstBuffer ** outbuf);
static GstFlowReturn
gst_d3d11_deinterlace_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf);
static gboolean gst_d3d11_deinterlace_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static void gst_d3d11_deinterlace_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);

static void
gst_d3d11_deinterlace_class_init (GstD3D11DeinterlaceClass * klass,
    gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstD3D11DeinterlaceClassData *cdata = (GstD3D11DeinterlaceClassData *) data;
  gchar *long_name;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_d3d11_deinterlace_get_property;
  gobject_class->set_property = gst_d3d11_deinterlace_set_property;
  gobject_class->finalize = gst_d3d11_deinterlace_finalize;

  g_object_class_install_property (gobject_class, PROP_ADAPTER,
      g_param_spec_uint ("adapter", "Adapter",
          "DXGI Adapter index for creating device",
          0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_flags ("method", "Method",
          "Deinterlace Method. Use can set multiple methods as a flagset "
          "and element will select one of method automatically. "
          "If deinterlacing device failed to deinterlace with given mode, "
          "fallback might happen by the device",
          GST_TYPE_D3D11_DEINTERLACE_METHOD, DEINTERLACE_METHOD_ALL,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_SUPPORTED_METHODS,
      g_param_spec_flags ("supported-methods", "Supported Methods",
          "Set of supported deinterlace methods by device",
          GST_TYPE_D3D11_DEINTERLACE_METHOD, DEINTERLACE_METHOD_ALL,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_set_context);

  long_name = g_strdup_printf ("Direct3D11 %s Deinterlacer",
      cdata->description);
  gst_element_class_set_metadata (element_class, long_name,
      "Filter/Effect/Video/Deinterlace/Hardware",
      "A Direct3D11 based deinterlacer",
      "Seungha Yang <seungha@centricular.com>");
  g_free (long_name);

  doc_caps = gst_caps_from_string (DOC_CAPS);
  pad_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  pad_templ = gst_pad_template_new ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);
  gst_caps_unref (doc_caps);

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_stop);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_query);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_fixate_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_decide_allocation);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_set_caps);
  trans_class->submit_input_buffer =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_submit_input_buffer);
  trans_class->generate_output =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_generate_output);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_transform);
  trans_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_sink_event);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_d3d11_deinterlace_before_transform);

  klass->adapter = cdata->adapter;
  klass->device_id = cdata->device_id;
  klass->vendor_id = cdata->vendor_id;
  klass->device_caps = cdata->device_caps;

  gst_d3d11_deinterlace_class_data_unref (cdata);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_DEINTERLACE_METHOD,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d11_deinterlace_init (GstD3D11Deinterlace * self)
{
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (self);

  self->method = klass->device_caps.default_method;
  self->default_buffer_duration = GST_CLOCK_TIME_NONE;
  gst_d3d11_deinterlace_update_method (self);

  g_queue_init (&self->past_frame_queue);
  g_queue_init (&self->future_frame_queue);
  InitializeCriticalSection (&self->lock);
}

static void
gst_d3d11_deinterlace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (object);
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_uint (value, klass->adapter);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, klass->device_id);
      break;
    case PROP_VENDOR_ID:
      g_value_set_uint (value, klass->vendor_id);
      break;
    case PROP_METHOD:
      g_value_set_flags (value, self->method);
      break;
    case PROP_SUPPORTED_METHODS:
      g_value_set_flags (value, klass->device_caps.supported_methods);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_d3d11_deinterlace_update_method (GstD3D11Deinterlace * self)
{
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (self);
  GstD3D11DeinterlaceMethod requested_method = self->method;
  gboolean updated = TRUE;

  /* Verify whether requested method is supported */
  if ((self->method & klass->device_caps.supported_methods) == 0) {
#ifndef GST_DISABLE_GST_DEBUG
    gchar *supported, *requested;

    supported = g_flags_to_string (GST_TYPE_D3D11_DEINTERLACE_METHOD,
        klass->device_caps.supported_methods);
    requested = g_flags_to_string (GST_TYPE_D3D11_DEINTERLACE_METHOD,
        klass->device_caps.supported_methods);

    GST_WARNING_OBJECT (self,
        "Requested method %s is not supported (supported: %s)",
        requested, supported);

    g_free (supported);
    g_free (requested);
#endif

    self->method = klass->device_caps.default_method;

    goto done;
  }

  /* Drop not supported methods */
  self->method = (GstD3D11DeinterlaceMethod)
      (klass->device_caps.supported_methods & self->method);

  /* Single method was requested? */
  if (self->method == GST_D3D11_DEINTERLACE_METHOD_BLEND ||
      self->method == GST_D3D11_DEINTERLACE_METHOD_BOB ||
      self->method == GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE ||
      self->method == GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION) {
    if (self->method == requested_method)
      updated = FALSE;
  } else {
    /* Pick single method from requested */
    if ((self->method & GST_D3D11_DEINTERLACE_METHOD_BOB) ==
        GST_D3D11_DEINTERLACE_METHOD_BOB) {
      self->method = GST_D3D11_DEINTERLACE_METHOD_BOB;
    } else if ((self->method & GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE) ==
        GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE) {
      self->method = GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE;
    } else if ((self->method & GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION)
        == GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION) {
      self->method = GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION;
    } else if ((self->method & GST_D3D11_DEINTERLACE_METHOD_BLEND) ==
        GST_D3D11_DEINTERLACE_METHOD_BLEND) {
      self->method = GST_D3D11_DEINTERLACE_METHOD_BLEND;
    } else {
      self->method = klass->device_caps.default_method;
      g_assert_not_reached ();
    }
  }

done:
  if (self->method == GST_D3D11_DEINTERLACE_METHOD_BLEND) {
    /* Both methods don't use reference frame for deinterlacing */
    self->max_past_frames = self->max_future_frames = 0;
  } else if (self->method == GST_D3D11_DEINTERLACE_METHOD_BOB) {
    /* To calculate timestamp and duration of output fraems, we will hold one
     * future frame even though processor device will not use reference */
    self->max_past_frames = 0;
    self->max_future_frames = 1;
  } else {
    /* FIXME: how many frames should be allowed? also, this needs to be
     * configurable */
    self->max_past_frames = MIN (klass->device_caps.max_past_frames,
        MAX_NUM_REFERENCES);

    /* Likewise Bob, we need at least one future frame for timestamp/duration
     * calculation */
    self->max_future_frames =
        MAX (MIN (klass->device_caps.max_future_frames, MAX_NUM_REFERENCES), 1);
  }

  return updated;
}

static void
gst_d3d11_deinterlace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (object);

  switch (prop_id) {
    case PROP_METHOD:{
      gboolean notify_update = FALSE;

      GST_OBJECT_LOCK (self);
      self->method = (GstD3D11DeinterlaceMethod) g_value_get_flags (value);
      notify_update = gst_d3d11_deinterlace_update_method (self);
      GST_OBJECT_UNLOCK (self);

      if (notify_update)
        g_object_notify (object, "method");
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_deinterlace_finalize (GObject * object)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (object);

  DeleteCriticalSection (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_deinterlace_set_context (GstElement * element, GstContext * context)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (element);
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (self);

  gst_d3d11_handle_set_context (element, context, klass->adapter,
      &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d11_deinterlace_open (GstD3D11Deinterlace * self)
{
  ID3D11VideoDevice *video_device;
  ID3D11VideoContext *video_context;

  video_device = gst_d3d11_device_get_video_device_handle (self->device);
  if (!video_device) {
    GST_ERROR_OBJECT (self, "ID3D11VideoDevice is not availale");
    return FALSE;
  }

  video_context = gst_d3d11_device_get_video_context_handle (self->device);
  if (!video_context) {
    GST_ERROR_OBJECT (self, "ID3D11VideoContext is not available");
    return FALSE;
  }

  self->video_device = video_device;
  video_device->AddRef ();

  self->video_context = video_context;
  video_context->AddRef ();

  return TRUE;
}

/* Must be called with lock taken */
static void
gst_d3d11_deinterlace_reset_history (GstD3D11Deinterlace * self)
{
  self->input_index = 0;
  self->num_output_per_input = 1;
  self->num_transformed = 0;
  self->first_output = TRUE;

  g_queue_clear_full (&self->past_frame_queue,
      (GDestroyNotify) gst_buffer_unref);
  g_queue_clear_full (&self->future_frame_queue,
      (GDestroyNotify) gst_buffer_unref);
  gst_clear_buffer (&self->to_process);
}

static void
gst_d3d11_deinterlace_reset (GstD3D11Deinterlace * self)
{
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->fallback_in_pool) {
    gst_buffer_pool_set_active (self->fallback_in_pool, FALSE);
    gst_object_unref (self->fallback_in_pool);
    self->fallback_in_pool = NULL;
  }

  if (self->fallback_out_pool) {
    gst_buffer_pool_set_active (self->fallback_out_pool, FALSE);
    gst_object_unref (self->fallback_out_pool);
    self->fallback_out_pool = NULL;
  }

  GST_D3D11_CLEAR_COM (self->video_enum);
  GST_D3D11_CLEAR_COM (self->video_proc);

  gst_d3d11_deinterlace_reset_history (self);
  self->default_buffer_duration = GST_CLOCK_TIME_NONE;
}

static void
gst_d3d11_deinterlace_close (GstD3D11Deinterlace * self)
{
  gst_d3d11_deinterlace_reset (self);

  GST_D3D11_CLEAR_COM (self->video_device);
  GST_D3D11_CLEAR_COM (self->video_context);

  gst_clear_object (&self->device);
}

static gboolean
gst_d3d11_deinterlace_start (GstBaseTransform * trans)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (self);

  if (!gst_d3d11_ensure_element_data (GST_ELEMENT_CAST (self), klass->adapter,
          &self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create d3d11device");
    return FALSE;
  }

  if (!gst_d3d11_deinterlace_open (self)) {
    GST_ERROR_OBJECT (self, "Couldn't open video device");
    gst_d3d11_deinterlace_close (self);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_deinterlace_stop (GstBaseTransform * trans)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);

  gst_d3d11_deinterlace_close (self);

  return TRUE;
}

static gboolean
gst_d3d11_deinterlace_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT_CAST (self),
              query, self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static GstCaps *
gst_d3d11_deinterlace_remove_interlace_info (GstCaps * caps,
    gboolean remove_framerate)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f, feature)) {
      if (remove_framerate) {
        gst_structure_remove_fields (st, "interlace-mode", "field-order",
            "framerate", NULL);
      } else {
        gst_structure_remove_fields (st, "interlace-mode", "field-order", NULL);
      }
    }

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  gst_caps_features_free (feature);

  return res;
}

static GstCaps *
gst_d3d11_deinterlace_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_d3d11_deinterlace_remove_interlace_info (caps,
      /* Non-blend mode will double framerate */
      self->method != GST_D3D11_DEINTERLACE_METHOD_BLEND);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static GstCaps *
gst_d3d11_deinterlace_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstStructure *s;
  GstCaps *tmp;
  gint fps_n, fps_d;
  GstVideoInfo info;
  const gchar *interlace_mode;

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);

  if (direction == GST_PAD_SRC)
    return gst_caps_fixate (othercaps);

  tmp = gst_caps_copy (caps);
  tmp = gst_caps_fixate (tmp);

  if (!gst_video_info_from_caps (&info, tmp)) {
    GST_WARNING_OBJECT (self, "Invalid caps %" GST_PTR_FORMAT, caps);
    gst_caps_unref (tmp);

    return gst_caps_fixate (othercaps);
  }

  s = gst_caps_get_structure (tmp, 0);
  if (gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d)) {
    /* for non-blend method, output framerate will be doubled */
    if (self->method != GST_D3D11_DEINTERLACE_METHOD_BLEND &&
        GST_VIDEO_INFO_IS_INTERLACED (&info)) {
      fps_n *= 2;
    }

    gst_caps_set_simple (othercaps,
        "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  }

  interlace_mode = gst_structure_get_string (s, "interlace-mode");
  if (g_strcmp0 ("progressive", interlace_mode) == 0) {
    /* Just forward interlace-mode=progressive.
     * By this way, basetransform will enable passthrough for non-interlaced
     * stream*/
    gst_caps_set_simple (othercaps,
        "interlace-mode", G_TYPE_STRING, "progressive", NULL);
  }

  gst_caps_unref (tmp);

  return gst_caps_fixate (othercaps);
}

static gboolean
gst_d3d11_deinterlace_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  guint n_pools, i;
  GstStructure *config;
  guint size;
  GstD3D11AllocationParams *d3d11_params;
  guint min_buffers = 0;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (decide_query == NULL)
    return TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  n_pools = gst_query_get_n_allocation_pools (query);
  for (i = 0; i < n_pools; i++) {
    gst_query_parse_nth_allocation_pool (query, i, &pool, NULL, NULL, NULL);
    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != self->device)
          gst_clear_object (&pool);
      }
    }
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (self->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (self->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_RENDER_TARGET, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (d3d11_params,
        D3D11_BIND_RENDER_TARGET);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  if (self->method == GST_D3D11_DEINTERLACE_METHOD_BOB) {
    /* For non-blend methods, we will produce two progressive frames from
     * a single interlaced frame. To determine timestamp and duration,
     * we might need to hold one past frame if buffer duration is unknown */
    min_buffers = 2;
  } else if (self->method == GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE ||
      self->method == GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION) {
    /* For advanced deinterlacing methods, we will hold more frame so that
     * device can use them as reference frames */

    min_buffers += self->max_past_frames;
    min_buffers += self->max_future_frames;
    /* And one for current frame */
    min_buffers++;

    /* we will hold at least one frame for timestamp/duration calculation */
    min_buffers = MAX (min_buffers, 2);
  }

  /* size will be updated by d3d11 buffer pool */
  gst_buffer_pool_config_set_params (config, caps, 0, min_buffers, 0);

  if (!gst_buffer_pool_set_config (pool, config))
    goto config_failed;

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, min_buffers, 0);

  gst_object_unref (pool);

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (self, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static gboolean
gst_d3d11_deinterlace_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min = 0, max = 0;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  gboolean update_pool = FALSE;
  GstVideoInfo info;

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  if (!gst_video_info_from_caps (&info, outcaps))
    return FALSE;

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != self->device)
          gst_clear_object (&pool);
      }
    }

    update_pool = TRUE;
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (self->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (self->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_RENDER_TARGET, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (d3d11_params,
        D3D11_BIND_RENDER_TARGET);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_set_config (pool, config);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_d3d11_deinterlace_prepare_fallback_pool (GstD3D11Deinterlace * self,
    GstCaps * in_caps, GstVideoInfo * in_info, GstCaps * out_caps,
    GstVideoInfo * out_info)
{
  GstD3D11AllocationParams *d3d11_params;

  /* Clearing potentially remaining resource here would be redundant.
   * Just to be safe enough */
  g_queue_clear_full (&self->past_frame_queue,
      (GDestroyNotify) gst_buffer_unref);
  g_queue_clear_full (&self->future_frame_queue,
      (GDestroyNotify) gst_buffer_unref);

  if (self->fallback_in_pool) {
    gst_buffer_pool_set_active (self->fallback_in_pool, FALSE);
    gst_object_unref (self->fallback_in_pool);
    self->fallback_in_pool = NULL;
  }

  if (self->fallback_out_pool) {
    gst_buffer_pool_set_active (self->fallback_out_pool, FALSE);
    gst_object_unref (self->fallback_out_pool);
    self->fallback_out_pool = NULL;
  }

  /* Empty bind flag is allowed for video processor input */
  d3d11_params = gst_d3d11_allocation_params_new (self->device, in_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);
  self->fallback_in_pool = gst_d3d11_buffer_pool_new_with_options (self->device,
      in_caps, d3d11_params, 0, 0);
  gst_d3d11_allocation_params_free (d3d11_params);

  if (!self->fallback_in_pool) {
    GST_ERROR_OBJECT (self, "Failed to create input fallback buffer pool");
    return FALSE;
  }

  /* For processor output, render target bind flag is required */
  d3d11_params = gst_d3d11_allocation_params_new (self->device, out_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_RENDER_TARGET, 0);
  self->fallback_out_pool =
      gst_d3d11_buffer_pool_new_with_options (self->device,
      out_caps, d3d11_params, 0, 0);
  gst_d3d11_allocation_params_free (d3d11_params);

  if (!self->fallback_out_pool) {
    GST_ERROR_OBJECT (self, "Failed to create output fallback buffer pool");
    gst_clear_object (&self->fallback_out_pool);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_deinterlace_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstVideoInfo in_info, out_info;
  /* *INDENT-OFF* */
  ComPtr<ID3D11VideoProcessorEnumerator> video_enum;
  ComPtr<ID3D11VideoProcessor> video_proc;
  /* *INDENT-ON* */
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  D3D11_VIDEO_PROCESSOR_CAPS proc_caps;
  D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rate_conv_caps;
  D3D11_VIDEO_PROCESSOR_OUTPUT_RATE output_rate =
      D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL;
  HRESULT hr;
  RECT rect;
  guint i;

  if (gst_base_transform_is_passthrough (trans))
    return TRUE;

  if (!gst_video_info_from_caps (&in_info, incaps)) {
    GST_ERROR_OBJECT (self, "Invalid input caps %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }

  if (!gst_video_info_from_caps (&out_info, outcaps)) {
    GST_ERROR_OBJECT (self, "Invalid output caps %" GST_PTR_FORMAT, outcaps);
    return FALSE;
  }

  self->in_info = in_info;
  self->out_info = out_info;

  /* Calculate expected buffer duration. We might need to reference this value
   * when buffer duration is unknown */
  if (GST_VIDEO_INFO_FPS_N (&in_info) > 0 &&
      GST_VIDEO_INFO_FPS_D (&in_info) > 0) {
    self->default_buffer_duration =
        gst_util_uint64_scale_int (GST_SECOND, GST_VIDEO_INFO_FPS_D (&in_info),
        GST_VIDEO_INFO_FPS_N (&in_info));
  } else {
    /* Assume 25 fps. We need this for reporting latency at least  */
    self->default_buffer_duration =
        gst_util_uint64_scale_int (GST_SECOND, 1, 25);
  }

  gst_d3d11_deinterlace_reset (self);

  /* Nothing to do */
  if (!GST_VIDEO_INFO_IS_INTERLACED (&in_info)) {
    gst_base_transform_set_passthrough (trans, TRUE);

    return TRUE;
  }

  /* TFF or BFF is not important here, this is just for enumerating
   * available deinterlace devices */
  memset (&desc, 0, sizeof (D3D11_VIDEO_PROCESSOR_CONTENT_DESC));

  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
  if (GST_VIDEO_INFO_FIELD_ORDER (&in_info) ==
      GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST)
    desc.InputFrameFormat =
        D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
  desc.InputWidth = GST_VIDEO_INFO_WIDTH (&in_info);
  desc.InputHeight = GST_VIDEO_INFO_HEIGHT (&in_info);
  desc.OutputWidth = GST_VIDEO_INFO_WIDTH (&out_info);
  desc.OutputHeight = GST_VIDEO_INFO_HEIGHT (&out_info);
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = self->video_device->CreateVideoProcessorEnumerator (&desc, &video_enum);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create VideoProcessorEnumerator");
    return FALSE;
  }

  hr = video_enum->GetVideoProcessorCaps (&proc_caps);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't query processor caps");
    return FALSE;
  }

  /* Shouldn't happen, we checked this already during plugin_init */
  if (proc_caps.RateConversionCapsCount == 0) {
    GST_ERROR_OBJECT (self, "Deinterlacing is not supported");
    return FALSE;
  }

  for (i = 0; i < proc_caps.RateConversionCapsCount; i++) {
    hr = video_enum->GetVideoProcessorRateConversionCaps (i, &rate_conv_caps);
    if (FAILED (hr))
      continue;

    if ((rate_conv_caps.ProcessorCaps & self->method) == self->method)
      break;
  }

  if (i >= proc_caps.RateConversionCapsCount) {
    GST_ERROR_OBJECT (self, "Deinterlacing method 0x%x is not supported",
        self->method);
    return FALSE;
  }

  hr = self->video_device->CreateVideoProcessor (video_enum.Get (),
      i, &video_proc);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create processor");
    return FALSE;
  }

  if (!gst_d3d11_deinterlace_prepare_fallback_pool (self, incaps, &in_info,
          outcaps, &out_info)) {
    GST_ERROR_OBJECT (self, "Couldn't prepare fallback buffer pool");
    return FALSE;
  }

  self->video_enum = video_enum.Detach ();
  self->video_proc = video_proc.Detach ();

  rect.left = 0;
  rect.top = 0;
  rect.right = GST_VIDEO_INFO_WIDTH (&self->in_info);
  rect.bottom = GST_VIDEO_INFO_HEIGHT (&self->in_info);

  /* Blending seems to be considered as half rate. See also
   * https://docs.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_process_input_stream_rate */
  if (self->method == GST_D3D11_DEINTERLACE_METHOD_BLEND)
    output_rate = D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF;

  GstD3D11DeviceLockGuard lk (self->device);
  self->video_context->VideoProcessorSetStreamSourceRect (self->video_proc,
      0, TRUE, &rect);
  self->video_context->VideoProcessorSetStreamDestRect (self->video_proc,
      0, TRUE, &rect);
  self->video_context->VideoProcessorSetOutputTargetRect (self->video_proc,
      TRUE, &rect);
  self->video_context->
      VideoProcessorSetStreamAutoProcessingMode (self->video_proc, 0, FALSE);
  self->video_context->VideoProcessorSetStreamOutputRate (self->video_proc, 0,
      output_rate, TRUE, NULL);

  return TRUE;
}

static ID3D11VideoProcessorInputView *
gst_d3d11_deinterace_get_piv_from_buffer (GstD3D11Deinterlace * self,
    GstBuffer * buffer)
{
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ID3D11VideoProcessorInputView *piv;

  if (gst_buffer_n_memory (buffer) != 1) {
    GST_WARNING_OBJECT (self, "Input buffer has more than one memory");
    return NULL;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_WARNING_OBJECT (self, "Input buffer is holding non-D3D11 memory");
    return NULL;
  }

  dmem = (GstD3D11Memory *) mem;
  if (dmem->device != self->device) {
    GST_WARNING_OBJECT (self,
        "Input D3D11 memory was allocated by other device");
    return NULL;
  }

  piv = gst_d3d11_memory_get_processor_input_view (dmem,
      self->video_device, self->video_enum);
  if (!piv) {
    GST_WARNING_OBJECT (self, "ID3D11VideoProcessorInputView is unavailable");
    return NULL;
  }

  return piv;
}

static GstBuffer *
gst_d3d11_deinterlace_ensure_input_buffer (GstD3D11Deinterlace * self,
    GstBuffer * input)
{
  GstD3D11Memory *dmem;
  ID3D11VideoProcessorInputView *piv;
  GstBuffer *new_buf = NULL;

  if (!input)
    return NULL;

  piv = gst_d3d11_deinterace_get_piv_from_buffer (self, input);
  if (piv)
    return input;

  if (!self->fallback_in_pool ||
      !gst_buffer_pool_set_active (self->fallback_in_pool, TRUE) ||
      gst_buffer_pool_acquire_buffer (self->fallback_in_pool, &new_buf,
          NULL) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Fallback input buffer is unavailable");
    gst_buffer_unref (input);

    return NULL;
  }

  if (!gst_d3d11_buffer_copy_into (new_buf, input, &self->in_info)) {
    GST_ERROR_OBJECT (self, "Couldn't copy input buffer to fallback buffer");
    gst_buffer_unref (new_buf);
    gst_buffer_unref (input);

    return NULL;
  }

  dmem = (GstD3D11Memory *) gst_buffer_peek_memory (new_buf, 0);
  piv = gst_d3d11_memory_get_processor_input_view (dmem,
      self->video_device, self->video_enum);
  if (!piv) {
    GST_ERROR_OBJECT (self, "ID3D11VideoProcessorInputView is unavailable");
    gst_buffer_unref (new_buf);
    gst_buffer_unref (input);

    return NULL;
  }

  /* copy metadata, default implementation of baseclass will copy everything
   * what we need */
  GST_BASE_TRANSFORM_CLASS (parent_class)->copy_metadata
      (GST_BASE_TRANSFORM_CAST (self), input, new_buf);

  gst_buffer_unref (input);

  return new_buf;
}

static GstFlowReturn
gst_d3d11_deinterlace_submit_future_frame (GstD3D11Deinterlace * self,
    GstBuffer * buffer)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (self);
  guint len;

  /* push tail and pop head, so that head frame can be the nearest frame
   * of current frame */
  if (buffer)
    g_queue_push_tail (&self->future_frame_queue, buffer);

  len = g_queue_get_length (&self->future_frame_queue);

  g_assert (len <= self->max_future_frames + 1);

  if (self->to_process) {
    GST_WARNING_OBJECT (self, "Found uncleared processing buffer");
    gst_clear_buffer (&self->to_process);
  }

  if (len > self->max_future_frames ||
      /* NULL means drain */
      (buffer == NULL && len > 0)) {
    GstClockTime cur_timestmap = GST_CLOCK_TIME_NONE;
    GstClockTime duration = GST_CLOCK_TIME_NONE;
    GstBuffer *next_buf;

    self->to_process =
        (GstBuffer *) g_queue_pop_head (&self->future_frame_queue);

    /* For non-blend methods, we will produce two frames from a single
     * interlaced frame. So, sufficiently correct buffer duration is required
     * to set timestamp for the second output frame */
    if (self->method != GST_D3D11_DEINTERLACE_METHOD_BLEND) {
      if (GST_BUFFER_PTS_IS_VALID (self->to_process)) {
        cur_timestmap = GST_BUFFER_PTS (self->to_process);
      } else {
        cur_timestmap = GST_BUFFER_DTS (self->to_process);
      }

      /* Ensure buffer duration */
      next_buf = (GstBuffer *) g_queue_peek_head (&self->future_frame_queue);
      if (next_buf && GST_CLOCK_STIME_IS_VALID (cur_timestmap)) {
        GstClockTime next_timestamp;

        if (GST_BUFFER_PTS_IS_VALID (next_buf)) {
          next_timestamp = GST_BUFFER_PTS (next_buf);
        } else {
          next_timestamp = GST_BUFFER_DTS (next_buf);
        }

        if (GST_CLOCK_STIME_IS_VALID (next_timestamp)) {
          if (trans->segment.rate >= 0.0 && next_timestamp > cur_timestmap) {
            duration = next_timestamp - cur_timestmap;
          } else if (trans->segment.rate < 0.0
              && next_timestamp < cur_timestmap) {
            duration = cur_timestmap - next_timestamp;
          }
        }
      }

      /* Make sure that we can update buffer duration safely */
      self->to_process = gst_buffer_make_writable (self->to_process);
      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        GST_BUFFER_DURATION (self->to_process) = duration;
      } else {
        GST_BUFFER_DURATION (self->to_process) = self->default_buffer_duration;
      }

      /* Bonus points, DTS doesn't make sense for raw video frame */
      GST_BUFFER_PTS (self->to_process) = cur_timestmap;
      GST_BUFFER_DTS (self->to_process) = GST_CLOCK_TIME_NONE;

      /* And mark the number of output frames for this input frame */
      self->num_output_per_input = 2;
    } else {
      self->num_output_per_input = 1;
    }

    self->first_output = TRUE;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d11_deinterlace_submit_input_buffer (GstBaseTransform * trans,
    gboolean is_discont, GstBuffer * input)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstFlowReturn ret;
  GstBuffer *buf;

  /* Let baseclass handle QoS first */
  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->submit_input_buffer (trans,
      is_discont, input);
  if (ret != GST_FLOW_OK)
    return ret;

  if (gst_base_transform_is_passthrough (trans))
    return ret;

  /* at this moment, baseclass must hold queued_buf */
  g_assert (trans->queued_buf != NULL);

  /* Check if we can use this buffer directly. If not, copy this into
   * our fallback buffer */
  buf = trans->queued_buf;
  trans->queued_buf = NULL;

  buf = gst_d3d11_deinterlace_ensure_input_buffer (self, buf);
  if (!buf) {
    GST_ERROR_OBJECT (self, "Invalid input buffer");
    return GST_FLOW_ERROR;
  }

  return gst_d3d11_deinterlace_submit_future_frame (self, buf);
}

static ID3D11VideoProcessorOutputView *
gst_d3d11_deinterace_get_pov_from_buffer (GstD3D11Deinterlace * self,
    GstBuffer * buffer)
{
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ID3D11VideoProcessorOutputView *pov;

  if (gst_buffer_n_memory (buffer) != 1) {
    GST_WARNING_OBJECT (self, "Output buffer has more than one memory");
    return NULL;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_WARNING_OBJECT (self, "Output buffer is holding non-D3D11 memory");
    return NULL;
  }

  dmem = (GstD3D11Memory *) mem;
  if (dmem->device != self->device) {
    GST_WARNING_OBJECT (self,
        "Output D3D11 memory was allocated by other device");
    return NULL;
  }

  pov = gst_d3d11_memory_get_processor_output_view (dmem,
      self->video_device, self->video_enum);
  if (!pov) {
    GST_WARNING_OBJECT (self, "ID3D11VideoProcessorOutputView is unavailable");
    return NULL;
  }

  return pov;
}

static GstBuffer *
gst_d3d11_deinterlace_ensure_output_buffer (GstD3D11Deinterlace * self,
    GstBuffer * output)
{
  GstD3D11Memory *dmem;
  ID3D11VideoProcessorOutputView *pov;
  GstBuffer *new_buf = NULL;

  pov = gst_d3d11_deinterace_get_pov_from_buffer (self, output);
  if (pov)
    return output;

  if (!self->fallback_out_pool ||
      !gst_buffer_pool_set_active (self->fallback_out_pool, TRUE) ||
      gst_buffer_pool_acquire_buffer (self->fallback_out_pool, &new_buf,
          NULL) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Fallback output buffer is unavailable");
    gst_buffer_unref (output);

    return NULL;
  }

  dmem = (GstD3D11Memory *) gst_buffer_peek_memory (new_buf, 0);
  pov = gst_d3d11_memory_get_processor_output_view (dmem,
      self->video_device, self->video_enum);
  if (!pov) {
    GST_ERROR_OBJECT (self, "ID3D11VideoProcessorOutputView is unavailable");
    gst_buffer_unref (new_buf);
    gst_buffer_unref (output);

    return NULL;
  }

  /* copy metadata, default implementation of baseclass will copy everything
   * what we need */
  GST_BASE_TRANSFORM_CLASS (parent_class)->copy_metadata
      (GST_BASE_TRANSFORM_CAST (self), output, new_buf);

  gst_buffer_unref (output);

  return new_buf;
}

static GstFlowReturn
gst_d3d11_deinterlace_submit_past_frame (GstD3D11Deinterlace * self,
    GstBuffer * buffer)
{
  /* push head and pop tail, so that head frame can be the nearest frame
   * of current frame */
  g_queue_push_head (&self->past_frame_queue, buffer);
  while (g_queue_get_length (&self->past_frame_queue) > self->max_past_frames) {
    GstBuffer *to_drop =
        (GstBuffer *) g_queue_pop_tail (&self->past_frame_queue);

    if (to_drop)
      gst_buffer_unref (to_drop);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d11_deinterlace_generate_output (GstBaseTransform * trans,
    GstBuffer ** outbuf)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *inbuf;
  GstBuffer *buf = NULL;

  if (gst_base_transform_is_passthrough (trans)) {
    return GST_BASE_TRANSFORM_CLASS (parent_class)->generate_output (trans,
        outbuf);
  }

  *outbuf = NULL;
  inbuf = self->to_process;
  if (inbuf == NULL)
    return GST_FLOW_OK;

  ret =
      GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      inbuf, &buf);

  if (ret != GST_FLOW_OK || !buf) {
    GST_WARNING_OBJECT (trans, "could not get buffer from pool: %s",
        gst_flow_get_name (ret));

    return ret;
  }

  g_assert (inbuf != buf);

  buf = gst_d3d11_deinterlace_ensure_output_buffer (self, buf);
  if (!buf) {
    GST_ERROR_OBJECT (self, "Failed to allocate output buffer to process");

    return GST_FLOW_ERROR;
  }

  ret = gst_d3d11_deinterlace_transform (trans, inbuf, buf);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (buf);
    return ret;
  }

  g_assert (self->num_output_per_input == 1 || self->num_output_per_input == 2);

  /* Update timestamp and buffer duration.
   * Here, PTS and duration of inbuf must be valid,
   * unless there's programing error, since we updated timestamp and duration
   * already around submit_input_buffer()  */
  if (self->num_output_per_input == 2) {
    if (!GST_BUFFER_DURATION_IS_VALID (inbuf)) {
      GST_LOG_OBJECT (self, "Input buffer duration is unknown");
    } else if (!GST_BUFFER_PTS_IS_VALID (inbuf)) {
      GST_LOG_OBJECT (self, "Input buffer timestamp is unknown");
    } else {
      GstClockTime duration = GST_BUFFER_DURATION (inbuf) / 2;
      gboolean second_field = FALSE;

      if (self->first_output) {
        /* For reverse playback, first output is the second field */
        if (trans->segment.rate < 0)
          second_field = TRUE;
        else
          second_field = FALSE;
      } else {
        if (trans->segment.rate < 0)
          second_field = FALSE;
        else
          second_field = TRUE;
      }

      GST_BUFFER_DURATION (buf) = duration;
      if (second_field) {
        GST_BUFFER_PTS (buf) = GST_BUFFER_PTS (buf) + duration;
      }
    }
  }

  *outbuf = buf;
  self->first_output = FALSE;
  self->num_transformed++;
  /* https://docs.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_process_input_stream_rate */
  if (self->method == GST_D3D11_DEINTERLACE_METHOD_BLEND) {
    self->input_index += 2;
  } else {
    self->input_index++;
  }

  if (self->num_output_per_input <= self->num_transformed) {
    /* Move processed frame to past_frame queue */
    gst_d3d11_deinterlace_submit_past_frame (self, self->to_process);
    self->to_process = NULL;
  }

  return ret;
}

static GstFlowReturn
gst_d3d11_deinterlace_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  ID3D11VideoProcessorInputView *piv;
  ID3D11VideoProcessorOutputView *pov;
  D3D11_VIDEO_FRAME_FORMAT frame_foramt = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  D3D11_VIDEO_PROCESSOR_STREAM proc_stream = { 0, };
  ID3D11VideoProcessorInputView *future_surfaces[MAX_NUM_REFERENCES] =
      { NULL, };
  ID3D11VideoProcessorInputView *past_surfaces[MAX_NUM_REFERENCES] = { NULL, };
  guint future_frames = 0;
  guint past_frames = 0;
  HRESULT hr;
  guint i;

  /* Input/output buffer must be holding valid D3D11 memory here,
   * as we checked it already in submit_input_buffer() and generate_output() */
  piv = gst_d3d11_deinterace_get_piv_from_buffer (self, inbuf);
  if (!piv) {
    GST_ERROR_OBJECT (self, "ID3D11VideoProcessorInputView is unavailable");
    return GST_FLOW_ERROR;
  }

  pov = gst_d3d11_deinterace_get_pov_from_buffer (self, outbuf);
  if (!pov) {
    GST_ERROR_OBJECT (self, "ID3D11VideoProcessorOutputView is unavailable");
    return GST_FLOW_ERROR;
  }

  /* Check field order */
  if (GST_VIDEO_INFO_INTERLACE_MODE (&self->in_info) ==
      GST_VIDEO_INTERLACE_MODE_MIXED ||
      (GST_VIDEO_INFO_INTERLACE_MODE (&self->in_info) ==
          GST_VIDEO_INTERLACE_MODE_INTERLEAVED &&
          GST_VIDEO_INFO_FIELD_ORDER (&self->in_info) ==
          GST_VIDEO_FIELD_ORDER_UNKNOWN)) {
    if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
      frame_foramt = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    } else if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_VIDEO_BUFFER_FLAG_TFF)) {
      frame_foramt = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
    } else {
      frame_foramt = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    }
  } else if (GST_VIDEO_INFO_FIELD_ORDER (&self->in_info) ==
      GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST) {
    frame_foramt = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
  } else if (GST_VIDEO_INFO_FIELD_ORDER (&self->in_info) ==
      GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST) {
    frame_foramt = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
  }

  if (frame_foramt == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE) {
    /* Progressive stream will produce only one frame per frame */
    self->num_output_per_input = 1;
  } else if (self->method != GST_D3D11_DEINTERLACE_METHOD_BLEND &&
      self->method != GST_D3D11_DEINTERLACE_METHOD_BOB) {
    /* Fill reference frames */
    for (i = 0; i < g_queue_get_length (&self->future_frame_queue) &&
        i < G_N_ELEMENTS (future_surfaces); i++) {
      GstBuffer *future_buf;
      ID3D11VideoProcessorInputView *future_piv;

      future_buf =
          (GstBuffer *) g_queue_peek_nth (&self->future_frame_queue, i);
      future_piv = gst_d3d11_deinterace_get_piv_from_buffer (self, future_buf);
      if (!future_piv) {
        GST_WARNING_OBJECT (self,
            "Couldn't get ID3D11VideoProcessorInputView from future "
            "reference %d", i);
        break;
      }

      future_surfaces[i] = future_piv;
      future_frames++;
    }

    for (i = 0; i < g_queue_get_length (&self->past_frame_queue) &&
        i < G_N_ELEMENTS (past_surfaces); i++) {
      GstBuffer *past_buf;
      ID3D11VideoProcessorInputView *past_piv;

      past_buf = (GstBuffer *) g_queue_peek_nth (&self->past_frame_queue, i);
      past_piv = gst_d3d11_deinterace_get_piv_from_buffer (self, past_buf);
      if (!past_piv) {
        GST_WARNING_OBJECT (self,
            "Couldn't get ID3D11VideoProcessorInputView from past "
            "reference %d", i);
        break;
      }

      past_surfaces[i] = past_piv;
      past_frames++;
    }
  }

  proc_stream.Enable = TRUE;
  proc_stream.pInputSurface = piv;
  proc_stream.InputFrameOrField = self->input_index;
  /* FIXME: This is wrong for inverse telechin case */
  /* OutputIndex == 0 for the first field, and 1 for the second field */
  if (self->num_output_per_input == 2) {
    if (trans->segment.rate < 0.0) {
      /* Process the second frame first in case of reverse playback */
      proc_stream.OutputIndex = self->first_output ? 1 : 0;
    } else {
      proc_stream.OutputIndex = self->first_output ? 0 : 1;
    }
  } else {
    proc_stream.OutputIndex = 0;
  }

  if (future_frames) {
    proc_stream.FutureFrames = future_frames;
    proc_stream.ppFutureSurfaces = future_surfaces;
  }

  if (past_frames) {
    proc_stream.PastFrames = past_frames;
    proc_stream.ppPastSurfaces = past_surfaces;
  }

  GstD3D11DeviceLockGuard lk (self->device);
  self->video_context->VideoProcessorSetStreamFrameFormat (self->video_proc, 0,
      frame_foramt);

  hr = self->video_context->VideoProcessorBlt (self->video_proc, pov, 0,
      1, &proc_stream);

  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to perform deinterlacing");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_deinterlace_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      /* stream-start means discont stream from previous one. Drain pending
       * frame if any */
      GST_DEBUG_OBJECT (self, "Have stream-start, drain frames if any");
      gst_d3d11_deinterlace_drain (self);
      break;
    case GST_EVENT_CAPS:{
      GstPad *sinkpad = GST_BASE_TRANSFORM_SINK_PAD (trans);
      GstCaps *prev_caps;

      prev_caps = gst_pad_get_current_caps (sinkpad);
      if (prev_caps) {
        GstCaps *caps;
        gst_event_parse_caps (event, &caps);
        /* If caps is updated, drain pending frames */
        if (!gst_caps_is_equal (prev_caps, caps)) {
          GST_DEBUG_OBJECT (self, "Caps updated from %" GST_PTR_FORMAT " to %"
              GST_PTR_FORMAT, prev_caps, caps);
          gst_d3d11_deinterlace_drain (self);
        }

        gst_caps_unref (prev_caps);
      }
      break;
    }
    case GST_EVENT_SEGMENT:
      /* new segment would mean that temporal discontinuity */
    case GST_EVENT_SEGMENT_DONE:
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (self, "Have event %s, drain frames if any",
          GST_EVENT_TYPE_NAME (event));
      gst_d3d11_deinterlace_drain (self);
      break;
    case GST_EVENT_FLUSH_STOP:
      EnterCriticalSection (&self->lock);
      gst_d3d11_deinterlace_reset_history (self);
      LeaveCriticalSection (&self->lock);
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static void
gst_d3d11_deinterlace_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstD3D11Deinterlace *self = GST_D3D11_DEINTERLACE (trans);
  GstD3D11DeinterlaceClass *klass = GST_D3D11_DEINTERLACE_GET_CLASS (self);
  GstD3D11Memory *dmem;
  GstMemory *mem;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  guint adapter = 0;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_ELEMENT_ERROR (self, CORE, FAILED, (NULL), ("Invalid memory"));
    return;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  /* Same device, nothing to do */
  if (dmem->device == self->device)
    return;

  g_object_get (dmem->device, "adapter", &adapter, NULL);
  /* We have per-GPU deinterlace elements because of different capability
   * per GPU. so, cannot accept other GPU at the moment */
  if (adapter != klass->adapter)
    return;

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, dmem->device);

  /* Drain buffers before updating device */
  gst_d3d11_deinterlace_drain (self);

  gst_object_unref (self->device);
  self->device = (GstD3D11Device *) gst_object_ref (dmem->device);

  in_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SINK_PAD (trans));
  if (!in_caps) {
    GST_WARNING_OBJECT (self, "sinkpad has null caps");
    goto out;
  }

  out_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SRC_PAD (trans));
  if (!out_caps) {
    GST_WARNING_OBJECT (self, "Has no configured output caps");
    goto out;
  }

  gst_d3d11_deinterlace_set_caps (trans, in_caps, out_caps);

  /* Mark reconfigure so that we can update pool */
  gst_base_transform_reconfigure_src (trans);

out:
  gst_clear_caps (&in_caps);
  gst_clear_caps (&out_caps);

  return;
}

/* FIXME: might be job of basetransform */
static GstFlowReturn
gst_d3d11_deinterlace_drain (GstD3D11Deinterlace * self)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (self);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *outbuf = NULL;

  EnterCriticalSection (&self->lock);

  if (gst_base_transform_is_passthrough (trans)) {
    /* If we were passthrough, nothing to do */
    goto done;
  } else if (!g_queue_get_length (&self->future_frame_queue)) {
    /* No pending data, nothing to do */
    goto done;
  }

  while (g_queue_get_length (&self->future_frame_queue)) {
    gst_d3d11_deinterlace_submit_future_frame (self, NULL);
    if (!self->to_process)
      break;

    do {
      outbuf = NULL;

      ret = gst_d3d11_deinterlace_generate_output (trans, &outbuf);
      if (outbuf != NULL) {
        /* Release lock during push buffer */
        LeaveCriticalSection (&self->lock);
        ret = gst_pad_push (trans->srcpad, outbuf);
        EnterCriticalSection (&self->lock);
      }
    } while (ret == GST_FLOW_OK && outbuf != NULL);
  }

done:
  gst_d3d11_deinterlace_reset_history (self);
  LeaveCriticalSection (&self->lock);

  return ret;
}

/**
 * SECTION:element-d3d11deinterlace
 * @title: d3d11deinterlace
 * @short_description: A Direct3D11 based deinterlace element
 *
 * Deinterlacing interlaced video frames to progressive video frames by using
 * ID3D11VideoProcessor API.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h264/file ! parsebin ! d3d11h264dec ! d3d11deinterlace ! d3d11videosink
 * ```
 *
 * Since: 1.20
 *
 */

/* GstD3D11DeinterlaceBin */
enum
{
  PROP_BIN_0,
  /* basetransform */
  PROP_BIN_QOS,
  /* deinterlace */
  PROP_BIN_ADAPTER,
  PROP_BIN_DEVICE_ID,
  PROP_BIN_VENDOR_ID,
  PROP_BIN_METHOD,
  PROP_BIN_SUPPORTED_METHODS,
};

typedef struct _GstD3D11DeinterlaceBin
{
  GstBin parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  GstElement *deinterlace;
  GstElement *in_convert;
  GstElement *out_convert;
  GstElement *upload;
  GstElement *download;
} GstD3D11DeinterlaceBin;

typedef struct _GstD3D11DeinterlaceBinClass
{
  GstBinClass parent_class;

  guint adapter;
  GType child_type;
} GstD3D11DeinterlaceBinClass;

static GstElementClass *bin_parent_class = NULL;
#define GST_D3D11_DEINTERLACE_BIN(object) ((GstD3D11DeinterlaceBin *) (object))
#define GST_D3D11_DEINTERLACE_BIN_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object), \
    GstD3D11DeinterlaceBinClass))

#define GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE(format) \
    "video/x-raw, " \
    "format = (string) " format ", "  \
    "width = (int) [1, 16384], " \
    "height = (int) [1, 16384] "

#define GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES(features,format) \
    "video/x-raw(" features "), " \
    "format = (string) " format ", "  \
    "width = (int) [1, 16384], " \
    "height = (int) [1, 16384] "

static GstStaticPadTemplate bin_sink_template_caps =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SINK_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D11_SINK_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE (GST_D3D11_SINK_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D11_SINK_FORMATS)
    ));

static GstStaticPadTemplate bin_src_template_caps =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SRC_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D11_SRC_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE (GST_D3D11_SRC_FORMATS) "; "
        GST_D3D11_DEINTERLACE_BIN_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D11_SRC_FORMATS)
    ));

static void gst_d3d11_deinterlace_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_deinterlace_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void
gst_d3d11_deinterlace_bin_class_init (GstD3D11DeinterlaceBinClass * klass,
    gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstD3D11DeinterlaceClassData *cdata = (GstD3D11DeinterlaceClassData *) data;
  gchar *long_name;

  bin_parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_d3d11_deinterlace_bin_get_property;
  gobject_class->set_property = gst_d3d11_deinterlace_bin_set_property;

  /* basetransform */
  g_object_class_install_property (gobject_class, PROP_BIN_QOS,
      g_param_spec_boolean ("qos", "QoS", "Handle Quality-of-Service events",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* deinterlace */
  g_object_class_install_property (gobject_class, PROP_BIN_ADAPTER,
      g_param_spec_uint ("adapter", "Adapter",
          "DXGI Adapter index for creating device",
          0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BIN_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BIN_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BIN_METHOD,
      g_param_spec_flags ("method", "Method",
          "Deinterlace Method. Use can set multiple methods as a flagset "
          "and element will select one of method automatically. "
          "If deinterlacing device failed to deinterlace with given mode, "
          "fallback might happen by the device",
          GST_TYPE_D3D11_DEINTERLACE_METHOD, DEINTERLACE_METHOD_ALL,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_BIN_SUPPORTED_METHODS,
      g_param_spec_flags ("supported-methods", "Supported Methods",
          "Set of supported deinterlace methods by device",
          GST_TYPE_D3D11_DEINTERLACE_METHOD, DEINTERLACE_METHOD_ALL,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  long_name = g_strdup_printf ("Direct3D11 %s Deinterlacer Bin",
      cdata->description);
  gst_element_class_set_metadata (element_class, long_name,
      "Filter/Effect/Video/Deinterlace/Hardware",
      "A Direct3D11 based deinterlacer bin",
      "Seungha Yang <seungha@centricular.com>");
  g_free (long_name);

  gst_element_class_add_static_pad_template (element_class,
      &bin_sink_template_caps);
  gst_element_class_add_static_pad_template (element_class,
      &bin_src_template_caps);

  klass->adapter = cdata->adapter;
  klass->child_type = cdata->deinterlace_type;

  gst_d3d11_deinterlace_class_data_unref (cdata);
}

static void
gst_d3d11_deinterlace_bin_init (GstD3D11DeinterlaceBin * self)
{
  GstD3D11DeinterlaceBinClass *klass =
      GST_D3D11_DEINTERLACE_BIN_GET_CLASS (self);
  GstPad *pad;

  self->deinterlace = (GstElement *) g_object_new (klass->child_type,
      "name", "deinterlace", NULL);
  self->in_convert = gst_element_factory_make ("d3d11colorconvert", NULL);
  self->out_convert = gst_element_factory_make ("d3d11colorconvert", NULL);
  self->upload = gst_element_factory_make ("d3d11upload", NULL);
  self->download = gst_element_factory_make ("d3d11download", NULL);

  /* Specify DXGI adapter index to use */
  g_object_set (G_OBJECT (self->in_convert), "adapter", klass->adapter, NULL);
  g_object_set (G_OBJECT (self->out_convert), "adapter", klass->adapter, NULL);
  g_object_set (G_OBJECT (self->upload), "adapter", klass->adapter, NULL);
  g_object_set (G_OBJECT (self->download), "adapter", klass->adapter, NULL);

  gst_bin_add_many (GST_BIN_CAST (self), self->upload, self->in_convert,
      self->deinterlace, self->out_convert, self->download, NULL);
  gst_element_link_many (self->upload, self->in_convert, self->deinterlace,
      self->out_convert, self->download, NULL);

  pad = gst_element_get_static_pad (self->upload, "sink");
  self->sinkpad = gst_ghost_pad_new ("sink", pad);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->sinkpad);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (self->download, "src");
  self->srcpad = gst_ghost_pad_new ("src", pad);
  gst_element_add_pad (GST_ELEMENT_CAST (self), self->srcpad);
  gst_object_unref (pad);
}

static void
gst_d3d11_deinterlace_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11DeinterlaceBin *self = GST_D3D11_DEINTERLACE_BIN (object);

  g_object_set_property (G_OBJECT (self->deinterlace), pspec->name, value);
}

static void
gst_d3d11_deinterlace_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11DeinterlaceBin *self = GST_D3D11_DEINTERLACE_BIN (object);

  g_object_get_property (G_OBJECT (self->deinterlace), pspec->name, value);
}

void
gst_d3d11_deinterlace_register (GstPlugin * plugin, GstD3D11Device * device,
    guint rank)
{
  GType type;
  GType bin_type;
  gchar *type_name;
  gchar *feature_name;
  guint index = 0;
  GTypeInfo type_info = {
    sizeof (GstD3D11DeinterlaceClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_d3d11_deinterlace_class_init,
    NULL,
    NULL,
    sizeof (GstD3D11Deinterlace),
    0,
    (GInstanceInitFunc) gst_d3d11_deinterlace_init,
  };
  GTypeInfo bin_type_info = {
    sizeof (GstD3D11DeinterlaceBinClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_d3d11_deinterlace_bin_class_init,
    NULL,
    NULL,
    sizeof (GstD3D11DeinterlaceBin),
    0,
    (GInstanceInitFunc) gst_d3d11_deinterlace_bin_init,
  };
  GstCaps *sink_caps = NULL;
  GstCaps *src_caps = NULL;
  GstCaps *caps = NULL;
  GstCapsFeatures *caps_features;
  ID3D11Device *device_handle;
  ID3D11DeviceContext *context_handle;
  /* *INDENT-OFF* */
  ComPtr<ID3D11VideoDevice> video_device;
  ComPtr<ID3D11VideoContext> video_context;
  ComPtr<ID3D11VideoProcessorEnumerator> video_proc_enum;
  ComPtr<ID3D11VideoProcessorEnumerator1> video_proc_enum1;
  /* *INDENT-ON* */
  HRESULT hr;
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  D3D11_VIDEO_PROCESSOR_CAPS proc_caps = { 0, };
  UINT supported_methods = 0;
  GstD3D11DeinterlaceMethod default_method;
  gboolean blend;
  gboolean bob;
  gboolean adaptive;
  gboolean mocomp;
  /* NOTE: processor might be able to handle other formats.
   * However, not all YUV formats can be used for render target.
   * For instance, DXGI_FORMAT_Y210 and DXGI_FORMAT_Y410 formats cannot be
   * render target. In practice, interlaced stream would output of video
   * decoders, so NV12/P010/P016 can cover most of real-world use case.
   */
  DXGI_FORMAT formats_to_check[] = {
    DXGI_FORMAT_NV12,           /* NV12 */
    DXGI_FORMAT_P010,           /* P010_10LE */
    DXGI_FORMAT_P016,           /* P016_LE */
  };
  GValue *supported_formats = NULL;
  GstD3D11DeinterlaceClassData *cdata;
  guint max_past_frames = 0;
  guint max_future_frames = 0;
  guint i;

  device_handle = gst_d3d11_device_get_device_handle (device);
  context_handle = gst_d3d11_device_get_device_context_handle (device);

  hr = device_handle->QueryInterface (IID_PPV_ARGS (&video_device));
  if (!gst_d3d11_result (hr, device))
    return;

  hr = context_handle->QueryInterface (IID_PPV_ARGS (&video_context));
  if (!gst_d3d11_result (hr, device))
    return;

  memset (&desc, 0, sizeof (D3D11_VIDEO_PROCESSOR_CONTENT_DESC));
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
  desc.InputWidth = 320;
  desc.InputHeight = 240;
  desc.OutputWidth = 320;
  desc.OutputHeight = 240;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = video_device->CreateVideoProcessorEnumerator (&desc, &video_proc_enum);
  if (!gst_d3d11_result (hr, device))
    return;

  /* We need ID3D11VideoProcessorEnumerator1 interface to check conversion
   * capability of device via CheckVideoProcessorFormatConversion()  */
  hr = video_proc_enum.As (&video_proc_enum1);
  if (!gst_d3d11_result (hr, device))
    return;

  hr = video_proc_enum->GetVideoProcessorCaps (&proc_caps);
  if (!gst_d3d11_result (hr, device))
    return;

  for (i = 0; i < proc_caps.RateConversionCapsCount; i++) {
    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rate_conv_caps = { 0, };

    hr = video_proc_enum->GetVideoProcessorRateConversionCaps (i,
        &rate_conv_caps);
    if (FAILED (hr))
      continue;

    supported_methods |= rate_conv_caps.ProcessorCaps;
    max_past_frames = MAX (max_past_frames, rate_conv_caps.PastFrames);
    max_future_frames = MAX (max_future_frames, rate_conv_caps.FutureFrames);
  }

  if (supported_methods == 0)
    return;

#define IS_SUPPORTED_METHOD(flags,val) (flags & val) == val
  blend = IS_SUPPORTED_METHOD (supported_methods,
      GST_D3D11_DEINTERLACE_METHOD_BLEND);
  bob = IS_SUPPORTED_METHOD (supported_methods,
      GST_D3D11_DEINTERLACE_METHOD_BOB);
  adaptive = IS_SUPPORTED_METHOD (supported_methods,
      GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE);
  mocomp = IS_SUPPORTED_METHOD (supported_methods,
      GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION);
#undef IS_SUPPORTED_METHOD

  if (!blend && !bob && !adaptive && !mocomp)
    return;

  /* Drop all not supported methods from flags */
  supported_methods = supported_methods &
      (GST_D3D11_DEINTERLACE_METHOD_BLEND | GST_D3D11_DEINTERLACE_METHOD_BOB |
      GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE |
      GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION);

  /* Prefer bob, it's equivalent to "linear" which is default mode of
   * software deinterlace element, also it's fallback mode
   * for our "adaptive" and "mocomp" modes. Note that since Direct3D12, "blend"
   * mode is no more supported, instead "bob" and "custom" mode are suported
   * by Direct3D12 */
  if (bob) {
    default_method = GST_D3D11_DEINTERLACE_METHOD_BOB;
  } else if (adaptive) {
    default_method = GST_D3D11_DEINTERLACE_METHOD_ADAPTIVE;
  } else if (mocomp) {
    default_method = GST_D3D11_DEINTERLACE_METHOD_MOTION_COMPENSATION;
  } else if (blend) {
    default_method = GST_D3D11_DEINTERLACE_METHOD_BLEND;
  } else {
    /* Programming error */
    g_return_if_reached ();
  }

  for (i = 0; i < G_N_ELEMENTS (formats_to_check); i++) {
    UINT flags = 0;
    GValue val = G_VALUE_INIT;
    GstVideoFormat format;
    BOOL supported = FALSE;

    hr = video_proc_enum->CheckVideoProcessorFormat (formats_to_check[i],
        &flags);
    if (FAILED (hr))
      continue;

    /* D3D11 video processor can support other conversion at once,
     * including color format conversion.
     * But not all combinations of in/out pairs can be supported.
     * To make things simple, this element will do only deinterlacing
     * (might not be optimal in terms of processing power/resource though) */

    /* D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT = 0x1,
     * D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT = 0x2,
     * MinGW header might not be defining the above enum values */
    if ((flags & 0x3) != 0x3)
      continue;

    format = gst_d3d11_dxgi_format_to_gst (formats_to_check[i]);
    /* This is programming error! */
    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      GST_ERROR ("Couldn't convert DXGI format %d to video format",
          formats_to_check[i]);
      continue;
    }

    hr = video_proc_enum1->CheckVideoProcessorFormatConversion
        (formats_to_check[i], DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
        formats_to_check[i], DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
        &supported);
    if (FAILED (hr) || !supported)
      continue;

    if (!supported_formats) {
      supported_formats = g_new0 (GValue, 1);
      g_value_init (supported_formats, GST_TYPE_LIST);
    }

    if (formats_to_check[i] == DXGI_FORMAT_P016) {
      /* This is used for P012 as well */
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_static_string (&val,
          gst_video_format_to_string (GST_VIDEO_FORMAT_P012_LE));
      gst_value_list_append_and_take_value (supported_formats, &val);
    }

    g_value_init (&val, G_TYPE_STRING);
    g_value_set_static_string (&val, gst_video_format_to_string (format));
    gst_value_list_append_and_take_value (supported_formats, &val);
  }

  if (!supported_formats)
    return;

  caps = gst_caps_new_empty_simple ("video/x-raw");
  /* FIXME: Check supported resolution, it would be different from
   * supported max texture dimension */
  gst_caps_set_simple (caps,
      "width", GST_TYPE_INT_RANGE, 1, 16384,
      "height", GST_TYPE_INT_RANGE, 1, 16384, NULL);
  gst_caps_set_value (caps, "format", supported_formats);
  g_value_unset (supported_formats);
  g_free (supported_formats);

  /* TODO: Add alternating deinterlace */
  src_caps = gst_caps_copy (caps);
  caps_features = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
      NULL);
  gst_caps_set_features_simple (src_caps, caps_features);

  caps_features = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
      GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, NULL);
  gst_caps_set_features_simple (caps, caps_features);
  gst_caps_append (src_caps, caps);

  sink_caps = gst_caps_copy (src_caps);

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  cdata = gst_d3d11_deinterlace_class_data_new ();
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->device_caps.supported_methods =
      (GstD3D11DeinterlaceMethod) supported_methods;
  cdata->device_caps.default_method = default_method;
  cdata->device_caps.max_past_frames = max_past_frames;
  cdata->device_caps.max_future_frames = max_future_frames;

  g_object_get (device, "adapter", &cdata->adapter,
      "device-id", &cdata->device_id, "vendor-id", &cdata->vendor_id,
      "description", &cdata->description, NULL);
  type_info.class_data = cdata;
  bin_type_info.class_data = gst_d3d11_deinterlace_class_data_ref (cdata);

  type_name = g_strdup ("GstD3D11Deinterlace");
  feature_name = g_strdup ("d3d11deinterlaceelement");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstD3D11Device%dDeinterlace", index);
    feature_name = g_strdup_printf ("d3d11device%ddeinterlaceelement", index);
  }

  type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
      type_name, &type_info, (GTypeFlags) 0);
  cdata->deinterlace_type = type;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, GST_RANK_NONE, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);

  /* Register wrapper bin */
  index = 0;
  type_name = g_strdup ("GstD3D11DeinterlaceBin");
  feature_name = g_strdup ("d3d11deinterlace");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstD3D11Device%dDeinterlaceBin", index);
    feature_name = g_strdup_printf ("d3d11device%ddeinterlace", index);
  }

  bin_type = g_type_register_static (GST_TYPE_BIN,
      type_name, &bin_type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (bin_type);

  if (!gst_element_register (plugin, feature_name, rank, bin_type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
