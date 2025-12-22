/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Copyright (C) 2023 Pengutronix e.K. - www.pengutronix.de
 *
 */

/**
 * SECTION:plugin-uvcgadget
 * @title: uvcgadget
 *
 * Since: 1.24
 */

/**
 * SECTION:element-uvcsink
 * @title: uvcsink
 *
 * uvcsink can be used to push frames to the Linux UVC Gadget
 * driver.
 *
 * ## Example launch lines
 * |[
 * gst-launch videotestsrc ! uvcsink v4l2sink::device=/dev/video1
 * ]|
 * This pipeline streams a test pattern on UVC gadget /dev/video1.
 *
 * Before starting the pipeline, the linux system needs an uvc gadget
 * configured on the udc (usb device controller)
 *
 * Either by using the legacy g_webcam gadget or by preconfiguring it
 * with configfs. One way to configure is to use the example script from
 * uvc-gadget:
 *
 * https://git.ideasonboard.org/uvc-gadget.git/blob/HEAD:/scripts/uvc-gadget.sh
 *
 * A modern way of configuring the gadget with an scheme file that gets
 * loaded by gadget-tool (gt) using libusbgx.
 *
 * https://github.com/linux-usb-gadgets/libusbgx
 * https://github.com/linux-usb-gadgets/gt
 *
 * Since: 1.24
 */
#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "gstuvcsink.h"

GST_DEBUG_CATEGORY (uvcsink_debug);
#define GST_CAT_DEFAULT uvcsink_debug

enum
{
  ARG_0,
  PROP_STREAMING,
  PROP_LAST
};

#define gst_uvc_sink_parent_class parent_class
G_DEFINE_TYPE (GstUvcSink, gst_uvc_sink, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (uvcsink,
    "uvcsink", GST_RANK_NONE, GST_TYPE_UVCSINK);

/* GstElement methods: */
static GstStateChangeReturn gst_uvc_sink_change_state (GstElement *
    element, GstStateChange transition);

static void gst_uvc_sink_dispose (GObject * object);
static gboolean gst_uvc_sink_prepare_configfs (GstUvcSink * self);

static void
gst_uvc_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUvcSink *self = GST_UVCSINK (object);

  switch (prop_id) {
    case PROP_STREAMING:
      g_value_set_boolean (value, g_atomic_int_get (&self->streaming));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStructure *
gst_v4l2uvc_fourcc_to_bare_struct (guint32 fourcc)
{
  GstStructure *structure = NULL;

  /* Since MJPEG and YUY2 are currently the only one supported
   * we limit the function to parse only these fourccs
   */
  switch (fourcc) {
    case V4L2_PIX_FMT_MJPEG:   /* Motion-JPEG */
    case V4L2_PIX_FMT_JPEG:    /* JFIF JPEG */
      structure = gst_structure_new_empty ("image/jpeg");
      break;
    case V4L2_PIX_FMT_YUYV:{
      GstVideoFormat format = GST_VIDEO_FORMAT_YUY2;
      if (format != GST_VIDEO_FORMAT_UNKNOWN)
        structure = gst_structure_new ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (format), NULL);
      break;
    }
      break;
    default:
      GST_DEBUG ("Unsupported fourcc 0x%08x %" GST_FOURCC_FORMAT,
          fourcc, GST_FOURCC_ARGS (fourcc));
      break;
  }

  return structure;
}

/* The UVC EVENT_DATA from the host, which is committing the currently
 * selected configuration (format+resolution+framerate) is only selected
 * by some index values (except the framerate). We have to transform
 * those values to an valid caps string that we can return on the caps
 * query.
 */
static GstCaps *
gst_uvc_sink_get_configured_caps (GstUvcSink * self)
{
  struct v4l2_fmtdesc format;
  struct v4l2_frmsizeenum size;
  struct v4l2_frmivalenum ival;
  guint32 width, height;
  gint device_fd;
  GstStructure *s;
  gint numerator, denominator;
  GValue framerate = { 0, };

  g_object_get (G_OBJECT (self->v4l2sink), "device-fd", &device_fd, NULL);

  format.index = self->cur.bFormatIndex - 1;
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if (ioctl (device_fd, VIDIOC_ENUM_FMT, &format) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Linux kernel error"),
        ("VIDIOC_ENUM_FMT failed: %s (%d)", g_strerror (errno), errno));
    return NULL;
  }

  s = gst_v4l2uvc_fourcc_to_bare_struct (format.pixelformat);

  memset (&size, 0, sizeof (struct v4l2_frmsizeenum));
  size.index = self->cur.bFrameIndex - 1;
  size.pixel_format = format.pixelformat;

  if (ioctl (device_fd, VIDIOC_ENUM_FRAMESIZES, &size) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Linux kernel error"),
        ("VIDIOC_ENUM_FRAMESIZES failed: %s (%d)", g_strerror (errno), errno));
    return NULL;
  }

  GST_LOG_OBJECT (self, "got discrete frame size %dx%d",
      size.discrete.width, size.discrete.height);

  width = MIN (size.discrete.width, G_MAXINT);
  height = MIN (size.discrete.height, G_MAXINT);

  if (!width || !height)
    return NULL;

  g_value_init (&framerate, GST_TYPE_FRACTION);

  memset (&ival, 0, sizeof (struct v4l2_frmivalenum));

  ival.index = 0;
  ival.pixel_format = format.pixelformat;
  ival.width = width;
  ival.height = height;

#define SIMPLIFY_FRACTION_N_TERMS   8
#define SIMPLIFY_FRACTION_THRESHOLD 333

  numerator = self->cur.dwFrameInterval;
  denominator = 10000000;
  gst_util_simplify_fraction (&numerator, &denominator,
      SIMPLIFY_FRACTION_N_TERMS, SIMPLIFY_FRACTION_THRESHOLD);

  if (ioctl (device_fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Linux kernel error"),
        ("VIDIOC_ENUM_FRAMEINTERVALS failed: %s (%d)",
            g_strerror (errno), errno));
    return NULL;
  }

  do {
    gint inumerator = ival.discrete.numerator;
    gint idenominator = ival.discrete.denominator;

    gst_util_simplify_fraction (&inumerator, &idenominator,
        SIMPLIFY_FRACTION_N_TERMS, SIMPLIFY_FRACTION_THRESHOLD);

    if (numerator == inumerator && denominator == idenominator)
      /* swap to get the framerate */
      gst_value_set_fraction (&framerate, denominator, numerator);

    ival.index++;
  } while (ioctl (device_fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) >= 0);

  gst_structure_set (s, "width", G_TYPE_INT, (gint) width,
      "height", G_TYPE_INT, (gint) height, NULL);

  gst_structure_take_value (s, "framerate", &framerate);

  return gst_caps_new_full (s, NULL);
}

static gboolean gst_uvc_sink_to_fakesink (GstUvcSink * self);
static gboolean gst_uvc_sink_to_v4l2sink (GstUvcSink * self);

static void
gst_uvc_sink_update_streaming (GstUvcSink * self)
{
  if (self->streamon && !self->streaming)
    GST_ERROR_OBJECT (self, "Unexpected STREAMON");
  if (self->streamoff && self->streaming)
    GST_ERROR_OBJECT (self, "Unexpected STREAMOFF");

  if (self->streamon)
    gst_uvc_sink_to_v4l2sink (self);

  g_atomic_int_set (&self->streamon, FALSE);
  g_atomic_int_set (&self->streamoff, FALSE);
}

static gboolean
gst_uvc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstUvcSink *self = GST_UVCSINK (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      GST_DEBUG_OBJECT (self, "Handling %" GST_PTR_FORMAT, event);

      /* EVENT CAPS signals that the buffers after the event will use new caps.
       * If the UVC host requested a new format, we now must start the stream.
       */
      if (self->caps_changed) {
        if (self->streamon || self->streamoff)
          g_atomic_int_set (&self->caps_changed, FALSE);

        gst_uvc_sink_update_streaming (self);
      }
      break;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_uvc_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstUvcSink *self = GST_UVCSINK (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      if (gst_caps_is_empty (self->cur_caps))
        return gst_pad_query_default (pad, parent, query);

      GST_DEBUG_OBJECT (self, "caps %" GST_PTR_FORMAT, self->cur_caps);
      gst_query_set_caps_result (query, self->cur_caps);

      if (!self->caps_changed)
        gst_uvc_sink_update_streaming (self);

      return TRUE;
    }
    case GST_QUERY_ALLOCATION:
      return TRUE;
    default:
      return gst_pad_query_default (pad, parent, query);
  }
  return TRUE;
}

static void
gst_uvc_sink_class_init (GstUvcSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (klass);

  element_class->change_state = gst_uvc_sink_change_state;

  gst_element_class_set_metadata (element_class,
      "UVC Sink", "Sink/Video",
      "Streams Video via UVC Gadget", "Michael Grzeschik <mgr@pengutronix.de>");

  GST_DEBUG_CATEGORY_INIT (uvcsink_debug, "uvcsink", 0, "uvc sink element");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gst_uvc_sink_dispose;
  gobject_class->get_property = gst_uvc_sink_get_property;

  /**
   * uvcsink:streaming:
   *
   * The stream status of the host.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_STREAMING,
      g_param_spec_boolean ("streaming", "streaming",
          "The stream status of the host", 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static gboolean
gst_uvc_sink_to_fakesink (GstUvcSink * self)
{
  if (gst_pad_is_linked (self->fakesinkpad)) {
    GST_DEBUG_OBJECT (self, "v4l2sink already disabled");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "switching to fakesink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->sinkpad), self->fakesinkpad);
  gst_element_set_state (GST_ELEMENT (self->fakesink), GST_STATE_PLAYING);

  /* going to state READY makes v4l2sink lose its reference to the clock */
  self->v4l2_clock = gst_element_get_clock (self->v4l2sink);

  gst_pad_query (self->v4l2sinkpad, gst_query_new_drain ());

  gst_element_set_state (GST_ELEMENT (self->v4l2sink), GST_STATE_READY);

  return TRUE;
}

static gboolean
gst_uvc_sink_to_v4l2sink (GstUvcSink * self)
{
  if (gst_pad_is_linked (self->v4l2sinkpad)) {
    GST_DEBUG_OBJECT (self, "fakesink already disabled");
    return FALSE;
  }

  if (self->v4l2_clock) {
    gst_element_set_clock (self->v4l2sink, self->v4l2_clock);
    gst_object_unref (self->v4l2_clock);
  }

  GST_DEBUG_OBJECT (self, "switching to v4l2sink");

  gst_ghost_pad_set_target (GST_GHOST_PAD (self->sinkpad), self->v4l2sinkpad);
  gst_element_set_state (GST_ELEMENT (self->v4l2sink), GST_STATE_PLAYING);

  gst_pad_query (self->fakesinkpad, gst_query_new_drain ());

  gst_element_set_state (GST_ELEMENT (self->fakesink), GST_STATE_READY);

  return TRUE;
}

static GstPadProbeReturn
gst_uvc_sink_sinkpad_buffer_peer_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data)
{
  GstUvcSink *self = user_data;

  if (self->streamon || self->streamoff)
    return GST_PAD_PROBE_DROP;

  self->buffer_peer_probe_id = 0;

  return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
gst_uvc_sink_sinkpad_idle_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data)
{
  GstUvcSink *self = user_data;

  if (self->streamon || self->streamoff) {
    /* Drop all incoming buffers until the streamoff or streamon is done. */
    self->buffer_peer_probe_id =
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        gst_uvc_sink_sinkpad_buffer_peer_probe, self, NULL);

    GST_DEBUG_OBJECT (self, "Send reconfigure");
    gst_pad_push_event (self->sinkpad, gst_event_new_reconfigure ());
  }

  if (self->streamoff)
    gst_uvc_sink_to_fakesink (self);

  return GST_PAD_PROBE_PASS;
}

static void
gst_uvc_sink_remove_buffer_peer_probe (GstUvcSink * self)
{
  GstPad *peerpad = gst_pad_get_peer (self->sinkpad);
  if (peerpad && self->buffer_peer_probe_id) {
    gst_pad_remove_probe (peerpad, self->buffer_peer_probe_id);
    gst_object_unref (peerpad);
    self->buffer_peer_probe_id = 0;
  }
}

static void
gst_uvc_sink_create_idle_probe (GstUvcSink * self)
{
  GstPad *peerpad = gst_pad_get_peer (self->sinkpad);
  if (peerpad) {
    self->idle_probe_id =
        gst_pad_add_probe (peerpad, GST_PAD_PROBE_TYPE_IDLE,
        gst_uvc_sink_sinkpad_idle_probe, self, NULL);
    gst_object_unref (peerpad);
  }
}

static void
gst_uvc_sink_remove_idle_probe (GstUvcSink * self)
{
  GstPad *peerpad = gst_pad_get_peer (self->sinkpad);
  if (peerpad && self->idle_probe_id) {
    gst_pad_remove_probe (peerpad, self->idle_probe_id);
    gst_object_unref (peerpad);
    self->idle_probe_id = 0;
  }
}

static gboolean
gst_uvc_sink_open (GstUvcSink * self)
{
  if (!self->v4l2sink) {
    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_missing_element_message_new (GST_ELEMENT_CAST (self), "v4l2sink"));

    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("No v4l2sink element found"), ("Check your plugin installation"));
    return FALSE;
  }

  if (!self->fakesink) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("No fakesink element found"), ("Check your plugin installation"));
    return FALSE;
  }

  return TRUE;
}

static void
gst_uvc_sink_init (GstUvcSink * self)
{
  /* add the v4l2sink element */
  self->v4l2sink = gst_element_factory_make ("v4l2sink", "v4l2sink");
  if (!self->v4l2sink)
    return;

  g_object_set (G_OBJECT (self->v4l2sink), "async", FALSE, NULL);
  gst_bin_add (GST_BIN (self), self->v4l2sink);

  /* add the fakesink element */
  self->fakesink = gst_element_factory_make ("fakesink", "fakesink");
  if (!self->fakesink)
    return;

  g_object_set (G_OBJECT (self->fakesink), "sync", TRUE, NULL);
  gst_bin_add (GST_BIN (self), self->fakesink);

  self->v4l2sinkpad = gst_element_get_static_pad (self->v4l2sink, "sink");
  g_return_if_fail (self->v4l2sinkpad != NULL);

  self->fakesinkpad = gst_element_get_static_pad (self->fakesink, "sink");
  g_return_if_fail (self->fakesinkpad != NULL);

  /* create ghost pad sink */
  self->sinkpad = gst_ghost_pad_new ("sink", self->fakesinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  g_atomic_int_set (&self->streaming, FALSE);

  g_atomic_int_set (&self->streamon, FALSE);
  g_atomic_int_set (&self->streamoff, FALSE);

  gst_pad_set_query_function (self->sinkpad, gst_uvc_sink_query);
  gst_pad_set_event_function (self->sinkpad, gst_uvc_sink_event);

  self->cur_caps = gst_caps_new_empty ();
}

static void
gst_uvc_sink_dispose (GObject * object)
{
  GstUvcSink *self = GST_UVCSINK (object);

  if (self->sinkpad) {
    gst_uvc_sink_remove_buffer_peer_probe (self);

    gst_pad_set_active (self->sinkpad, FALSE);
    gst_element_remove_pad (GST_ELEMENT (self), self->sinkpad);
    self->sinkpad = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean gst_uvc_sink_unwatch (GstUvcSink * self);

/* the thread where everything happens */
static void
gst_uvc_sink_task (gpointer data)
{
  GstUvcSink *self = GST_UVCSINK (data);
  GstClockTime timeout = GST_CLOCK_TIME_NONE;
  struct uvc_request_data resp;
  gboolean ret = TRUE;

  /* Since the plugin needs to be able to start immidiatly in PLAYING
     state we ensure the pipeline is not blocked while we poll for
     events.
   */
  GST_PAD_STREAM_UNLOCK (self->sinkpad);

  ret = gst_poll_wait (self->poll, timeout);
  if (G_UNLIKELY (ret < 0))
    return;

  timeout = GST_CLOCK_TIME_NONE;

  if (gst_poll_fd_has_closed (self->poll, &self->pollfd)) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("videofd was closed"), NULL);
    gst_uvc_sink_unwatch (self);
    gst_element_set_state (GST_ELEMENT (self), GST_STATE_NULL);
    return;
  }

  if (gst_poll_fd_has_error (self->poll, &self->pollfd)) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("videofd has error"), NULL);
    gst_uvc_sink_unwatch (self);
    gst_element_set_state (GST_ELEMENT (self), GST_STATE_NULL);
    return;
  }

  /* PRI is used to signal that events are available */
  if (gst_poll_fd_has_pri (self->poll, &self->pollfd)) {
    struct v4l2_event event = { 0, };
    struct uvc_event *uvc_event = (void *) &event.u.data;

    if (ioctl (self->pollfd.fd, VIDIOC_DQEVENT, &event) < 0) {
      GST_ELEMENT_ERROR (self, RESOURCE, READ,
          ("could not dequeue event"), NULL);
      gst_uvc_sink_unwatch (self);
      gst_element_set_state (GST_ELEMENT (self), GST_STATE_NULL);
      return;
    }

    switch (event.type) {
      case UVC_EVENT_STREAMON:
        GST_DEBUG_OBJECT (self, "UVC_EVENT_STREAMON");
        GST_STATE_LOCK (GST_ELEMENT (self));
        g_atomic_int_set (&self->streaming, TRUE);
        g_atomic_int_set (&self->streamon, TRUE);
        GST_STATE_UNLOCK (GST_ELEMENT (self));
        g_object_notify (G_OBJECT (self), "streaming");
        break;
      case UVC_EVENT_STREAMOFF:
      case UVC_EVENT_DISCONNECT:
        GST_DEBUG_OBJECT (self, "UVC_EVENT_STREAMOFF");
        GST_STATE_LOCK (GST_ELEMENT (self));
        g_atomic_int_set (&self->streaming, FALSE);
        g_atomic_int_set (&self->streamoff, TRUE);
        GST_STATE_UNLOCK (GST_ELEMENT (self));
        g_object_notify (G_OBJECT (self), "streaming");
        break;
      case UVC_EVENT_SETUP:
        GST_DEBUG_OBJECT (self, "UVC_EVENT_SETUP");

        memset (&resp, 0, sizeof (resp));
        resp.length = -EL2HLT;

        uvc_events_process_setup (self, &uvc_event->req, &resp);

        if (ioctl (self->pollfd.fd, UVCIOC_SEND_RESPONSE, &resp) < 0) {
          GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
              ("UVCIOC_SEND_RESPONSE failed"),
              ("UVCIOC_SEND_RESPONSE on UVC_EVENT_SETUP failed"));
          gst_uvc_sink_unwatch (self);
          gst_element_set_state (GST_ELEMENT (self), GST_STATE_NULL);
          return;
        }
        break;
      case UVC_EVENT_DATA:
        GST_DEBUG_OBJECT (self, "UVC_EVENT_DATA");
        uvc_events_process_data (self, &uvc_event->data);
        if (self->control == UVC_VS_COMMIT_CONTROL) {
          GstCaps *configured_caps;
          GstCaps *prev_caps;

          /* The configured caps are not sufficient for negotiation. Select caps
           * from the probed caps that match the configured caps.
           */
          configured_caps = gst_uvc_sink_get_configured_caps (self);
          gst_clear_caps (&self->cur_caps);
          self->cur_caps =
              gst_caps_intersect_full (self->probed_caps, configured_caps,
              GST_CAPS_INTERSECT_FIRST);
          GST_INFO_OBJECT (self, "UVC host selected %" GST_PTR_FORMAT,
              self->cur_caps);
          gst_caps_unref (configured_caps);

          prev_caps = gst_pad_get_current_caps (self->sinkpad);
          if (!gst_caps_is_subset (prev_caps, self->cur_caps)) {
            self->caps_changed = TRUE;
            GST_DEBUG_OBJECT (self,
                "caps changed from %" GST_PTR_FORMAT, prev_caps);
          }
          gst_caps_unref (prev_caps);
        }
        break;
      default:
        break;
    }
  }
}

static gboolean
gst_uvc_sink_watch (GstUvcSink * self)
{
  struct v4l2_event_subscription sub = {.type = UVC_EVENT_STREAMON };
  gboolean ret = TRUE;
  gint device_fd;
  gint fd;

  ret = gst_uvc_sink_prepare_configfs (self);
  if (!ret) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("could not parse configfs"), ("Check your configfs setup"));
    return FALSE;
  }

  g_object_get (G_OBJECT (self->v4l2sink), "device-fd", &device_fd, NULL);

  fd = dup3 (device_fd, device_fd + 1, O_CLOEXEC);
  if (fd < 0)
    return FALSE;

  self->poll = gst_poll_new (TRUE);
  gst_poll_fd_init (&self->pollfd);
  self->pollfd.fd = fd;
  gst_poll_add_fd (self->poll, &self->pollfd);
  gst_poll_fd_ctl_pri (self->poll, &self->pollfd, TRUE);

  if (ioctl (device_fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to subscribe event"),
        ("UVC_EVENT_STREAMON could not be subscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_STREAMOFF;
  if (ioctl (device_fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to subscribe event"),
        ("UVC_EVENT_STREAMOFF could not be subscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_DISCONNECT;
  if (ioctl (device_fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to subscribe event"),
        ("UVC_EVENT_DISCONNECT could not be subscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_SETUP;
  if (ioctl (device_fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to subscribe event"),
        ("UVC_EVENT_SETUP could not be subscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_DATA;
  if (ioctl (device_fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to subscribe event"),
        ("UVC_EVENT_DATA could not be subscribed"));
    return FALSE;
  }

  if (!gst_pad_start_task (GST_PAD (self->sinkpad),
          (GstTaskFunction) gst_uvc_sink_task, self, NULL)) {
    GST_ELEMENT_ERROR (self, CORE, THREAD, ("Could not start pad task"),
        ("Could not start pad task"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_uvc_sink_unwatch (GstUvcSink * self)
{
  struct v4l2_event_subscription sub = {.type = UVC_EVENT_DATA };
  gint device_fd;

  gst_poll_set_flushing (self->poll, TRUE);
  gst_pad_stop_task (GST_PAD (self->sinkpad));

  g_object_get (G_OBJECT (self->v4l2sink), "device-fd", &device_fd, NULL);

  if (ioctl (device_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to unsubscribe event"),
        ("UVC_EVENT_DATA could not be unsubscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_SETUP;
  if (ioctl (device_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to unsubscribe event"),
        ("UVC_EVENT_SETUP could not be unsubscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_STREAMON;
  if (ioctl (device_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to unsubscribe event"),
        ("UVC_EVENT_STREAMON could not be unsubscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_STREAMOFF;
  if (ioctl (device_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to unsubscribe event"),
        ("UVC_EVENT_STREAMOFF could not be unsubscribed"));
    return FALSE;
  }

  sub.type = UVC_EVENT_DISCONNECT;
  if (ioctl (device_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to unsubscribe event"),
        ("UVC_EVENT_DISCONNECT could not be unsubscribed"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_uvc_sink_prepare_configfs (GstUvcSink * self)
{
  gint device_fd;
  GValue device = G_VALUE_INIT;

  g_object_get (G_OBJECT (self->v4l2sink), "device-fd", &device_fd, NULL);
  g_object_get_property (G_OBJECT (self->v4l2sink), "device", &device);

  self->fc = configfs_parse_uvc_videodev (device_fd,
      g_value_get_string (&device));
  if (!self->fc) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to identify function configuration"),
        ("Check your configfs setup"));
    return FALSE;
  }

  uvc_fill_streaming_control (self, &self->probe, self->cur.bFrameIndex,
      self->cur.bFormatIndex, self->cur.dwFrameInterval);
  uvc_fill_streaming_control (self, &self->commit, self->cur.bFrameIndex,
      self->cur.bFormatIndex, self->cur.dwFrameInterval);

  return TRUE;
}

static gboolean
gst_uvc_sink_query_probed_caps (GstUvcSink * self)
{
  GstQuery *caps_query = gst_query_new_caps (NULL);
  GstCaps *query_caps;

  gst_clear_caps (&self->probed_caps);
  if (!gst_pad_query (self->v4l2sinkpad, caps_query))
    return FALSE;

  gst_query_parse_caps_result (caps_query, &query_caps);
  gst_query_unref (caps_query);

  self->probed_caps = gst_caps_copy (query_caps);

  gst_caps_replace (&self->cur_caps, self->probed_caps);

  return TRUE;
}

static GstStateChangeReturn
gst_uvc_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstUvcSink *self = GST_UVCSINK (element);
  int bret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "%s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_uvc_sink_open (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_uvc_sink_create_idle_probe (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_element_sync_state_with_parent (GST_ELEMENT (self->fakesink));
      gst_uvc_sink_remove_idle_probe (self);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (!gst_uvc_sink_unwatch (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  bret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_uvc_sink_watch (self))
        return GST_STATE_CHANGE_FAILURE;
      if (!gst_uvc_sink_query_probed_caps (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return bret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (uvcsink, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    uvcgadget,
    "gstuvcgadget plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
