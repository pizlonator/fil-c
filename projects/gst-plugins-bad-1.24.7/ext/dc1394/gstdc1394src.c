/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
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
 * SECTION:element-dc1394src
 * @title: dc1394src
 *
 * Source for IIDC (Instrumentation & Industrial Digital Camera) firewire
 * cameras. If several cameras are connected to the system, the desired one
 * can be selected by its GUID and an optional unit number (most cameras are
 * single unit and do not require it). The frame size, rate and format are set
 * from capabilities. Although the IIDC specification includes a raw video
 * mode, many cameras use mono video modes to capture in Bayer format.
 * Thus, for each mono video mode supported by a camera, both gray raw and Bayer
 * corresponding video formats are exposed in the capabilities.
 * The Bayer pattern is left unspecified.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v dc1394src ! videoconvert ! autovideosink
 * ]| Capture and display frames from the first camera available in the system.
 * |[
 * gst-launch-1.0 dc1394src guid=00074813004DF937 \
 *     ! "video/x-bayer,format=gbrg,width=1280,height=960,framerate=15/2" \
 *     ! bayer2rgb ! videoconvert ! autovideosink
 * ]| Capture and display frames from a specific camera in the desired format.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstdc1394src.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (dc1394_debug);
#define GST_CAT_DEFAULT dc1394_debug


enum
{
  PROP_0,
  PROP_CAMERA_GUID,
  PROP_CAMERA_UNIT,
  PROP_ISO_SPEED,
  PROP_DMA_BUFFER_SIZE
};


#define GST_TYPE_DC1394_ISO_SPEED (gst_dc1394_iso_speed_get_type ())
static GType
gst_dc1394_iso_speed_get_type (void)
{
  static const GEnumValue iso_speeds[] = {
    {100, "DC1394 ISO speed 100", "100"},
    {200, "DC1394 ISO speed 200", "200"},
    {400, "DC1394 ISO speed 400", "400"},
    {800, "DC1394 ISO speed 800", "800"},
    {1600, "DC1394 ISO speed 1600", "1600"},
    {3200, "DC1394 ISO speed 3200", "3200"},
    {0, NULL, NULL}
  };
  static GType type = 0;

  if (!type) {
    type = g_enum_register_static ("GstDC1394ISOSpeed", iso_speeds);
  }
  return type;
}


#define gst_dc1394_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDC1394Src, gst_dc1394_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (dc1394_debug, "dc1394", 0, "DC1394 interface");
    );
GST_ELEMENT_REGISTER_DEFINE (dc1394src, "dc1394src", GST_RANK_NONE,
    GST_TYPE_DC1394_SRC);

static void gst_dc1394_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dc1394_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_dc1394_src_start (GstBaseSrc * bsrc);
static gboolean gst_dc1394_src_stop (GstBaseSrc * bsrc);
static gboolean gst_dc1394_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static GstCaps *gst_dc1394_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static GstFlowReturn gst_dc1394_src_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

static void gst_dc1394_src_set_prop_camera_guid (GstDC1394Src * src,
    const gchar * guid);
static gchar *gst_dc1394_src_get_prop_camera_guid (GstDC1394Src * src);
static void gst_dc1394_src_set_prop_camera_unit (GstDC1394Src * src, gint unit);
static gint gst_dc1394_src_get_prop_camera_unit (GstDC1394Src * src);
static void gst_dc1394_src_set_prop_iso_speed (GstDC1394Src * src, guint speed);
static guint gst_dc1394_src_get_prop_iso_speed (GstDC1394Src * src);
static void gst_dc1394_src_set_prop_dma_buffer_size (GstDC1394Src * src,
    guint size);
static guint gst_dc1394_src_get_prop_dma_buffer_size (GstDC1394Src * src);
static gboolean gst_dc1394_src_open_cam (GstDC1394Src * src);
static void gst_dc1394_src_close_cam (GstDC1394Src * src);
static gboolean gst_dc1394_src_start_cam (GstDC1394Src * src);
static gboolean gst_dc1394_src_stop_cam (GstDC1394Src * src);
static gboolean gst_dc1394_src_set_cam_caps (GstDC1394Src * src,
    GstCaps * caps);
static GstCaps *gst_dc1394_src_get_cam_caps (GstDC1394Src * src);
static GstCaps *gst_dc1394_src_get_all_caps (void);

static GstCaps *gst_dc1394_src_build_caps (const dc1394color_codings_t *
    supported_codings, const dc1394framerates_t * supported_rates,
    guint width_min, guint width_max, guint width_step, guint height_min,
    guint height_max, guint height_step);
static gboolean gst_dc1394_src_parse_caps (const GstCaps * caps,
    dc1394color_codings_t * color_codings, dc1394framerate_t * rate,
    gdouble * rate_decimal, guint * width, guint * height);

static void
gst_dc1394_src_class_init (GstDC1394SrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSrcClass *basesrc_class;
  GstPushSrcClass *pushsrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesrc_class = GST_BASE_SRC_CLASS (klass);
  pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_dc1394_src_set_property;
  gobject_class->get_property = gst_dc1394_src_get_property;
  g_object_class_install_property (gobject_class, PROP_CAMERA_GUID,
      g_param_spec_string ("guid", "Camera GUID",
          "The hexadecimal representation of the GUID of the camera"
          " (use first camera available if null)",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_CAMERA_UNIT,
      g_param_spec_int ("unit", "Camera unit",
          "The unit number of the camera (-1 if no unit number is used)",
          -1, G_MAXINT, -1,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_ISO_SPEED,
      g_param_spec_enum ("iso", "ISO bandwidth",
          "The ISO bandwidth in Mbps",
          GST_TYPE_DC1394_ISO_SPEED, 400,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_DMA_BUFFER_SIZE,
      g_param_spec_uint ("dma", "DMA ring buffer size",
          "The number of frames in the Direct Memory Access ring buffer",
          1, G_MAXUINT, 10,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));

  gst_element_class_set_static_metadata (element_class,
      "1394 IIDC Video Source", "Source/Video",
      "libdc1394 based source for IIDC cameras",
      "Antoine Tremblay <hexa00@gmail.com>");
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_dc1394_src_get_all_caps ()));

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_dc1394_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_dc1394_src_stop);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_dc1394_src_set_caps);
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_dc1394_src_get_caps);

  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_dc1394_src_create);

  gst_type_mark_as_plugin_api (GST_TYPE_DC1394_ISO_SPEED, 0);
}


static void
gst_dc1394_src_init (GstDC1394Src * src)
{
  src->guid = -1;
  src->unit = -1;
  src->iso_speed = DC1394_ISO_SPEED_400;
  src->dma_buffer_size = 10;
  src->dc1394 = NULL;
  src->camera = NULL;
  src->caps = NULL;

  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
}


static void
gst_dc1394_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDC1394Src *src;

  src = GST_DC1394_SRC (object);
  switch (prop_id) {
    case PROP_CAMERA_GUID:
      g_value_take_string (value, gst_dc1394_src_get_prop_camera_guid (src));
      break;
    case PROP_CAMERA_UNIT:
      g_value_set_int (value, gst_dc1394_src_get_prop_camera_unit (src));
      break;
    case PROP_ISO_SPEED:
      g_value_set_enum (value, gst_dc1394_src_get_prop_iso_speed (src));
      break;
    case PROP_DMA_BUFFER_SIZE:
      g_value_set_uint (value, gst_dc1394_src_get_prop_dma_buffer_size (src));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_dc1394_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDC1394Src *src;

  src = GST_DC1394_SRC (object);
  switch (prop_id) {
    case PROP_CAMERA_GUID:
      gst_dc1394_src_set_prop_camera_guid (src, g_value_get_string (value));
      break;
    case PROP_CAMERA_UNIT:
      gst_dc1394_src_set_prop_camera_unit (src, g_value_get_int (value));
      break;
    case PROP_ISO_SPEED:
      gst_dc1394_src_set_prop_iso_speed (src, g_value_get_enum (value));
      break;
    case PROP_DMA_BUFFER_SIZE:
      gst_dc1394_src_set_prop_dma_buffer_size (src, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_dc1394_src_start (GstBaseSrc * bsrc)
{
  GstDC1394Src *src;

  src = GST_DC1394_SRC (bsrc);
  return gst_dc1394_src_open_cam (src);
}


static gboolean
gst_dc1394_src_stop (GstBaseSrc * bsrc)
{
  GstDC1394Src *src;

  src = GST_DC1394_SRC (bsrc);
  if (!gst_dc1394_src_stop_cam (src))
    return FALSE;
  gst_dc1394_src_close_cam (src);
  return TRUE;
}


static GstCaps *
gst_dc1394_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstDC1394Src *src;
  GstCaps *caps, *ret;

  src = GST_DC1394_SRC (bsrc);
  if (src->camera) {
    caps = gst_dc1394_src_get_cam_caps (src);
  } else {
    caps = gst_dc1394_src_get_all_caps ();
  }
  if (caps && filter) {
    ret = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
  } else {
    ret = caps;
  }
  return ret;
}


static gboolean
gst_dc1394_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstDC1394Src *src;

  src = GST_DC1394_SRC (bsrc);
  return gst_dc1394_src_stop_cam (src)
      && gst_dc1394_src_set_cam_caps (src, caps)
      && gst_dc1394_src_start_cam (src);
}


static GstFlowReturn
gst_dc1394_src_create (GstPushSrc * psrc, GstBuffer ** obuf)
{
  GstDC1394Src *src;
  GstBuffer *buffer = NULL;
  dc1394video_frame_t *frame;
  dc1394error_t ret;

  src = GST_DC1394_SRC (psrc);
  ret = dc1394_capture_dequeue (src->camera, DC1394_CAPTURE_POLICY_WAIT,
      &frame);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not dequeue frame: %s.", dc1394_error_get_string (ret)));
    return GST_FLOW_ERROR;
  }
  /*
   * TODO: We could create the buffer by wrapping the image bytes in the frame
   * (enqueing the frame in the notify function) to save the copy operation.
   * It will only work if all the buffers are disposed before closing the camera
   * when state changes from PAUSED to READY.
   */
  buffer = gst_buffer_new_allocate (NULL, frame->image_bytes, NULL);
  gst_buffer_fill (buffer, 0, frame->image, frame->image_bytes);
  /*
   * TODO: There is a field timestamp in the frame structure,
   * It is not sure if it could be used as PTS or DTS:
   * we are not sure if it comes from a monotonic clock,
   * and it seems to be left undefined under MS Windows.
   */
  ret = dc1394_capture_enqueue (src->camera, frame);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("Could not enqueue frame: %s.", dc1394_error_get_string (ret)));
  }
  *obuf = buffer;
  return GST_FLOW_OK;
}


static void
gst_dc1394_src_set_prop_camera_guid (GstDC1394Src * src, const gchar * guid)
{
  gchar *end;

  if (!guid) {
    GST_DEBUG_OBJECT (src, "Null camera GUID value: %s.",
        "first camera available will be used");
    src->guid = -1;
    return;
  }
  errno = 0;
  src->guid = g_ascii_strtoull (guid, &end, 16);
  if (errno == ERANGE || end == guid || *end != '\0') {
    GST_ERROR_OBJECT (src, "Invalid camera GUID value: %s.", guid);
    return;
  }
}


static gchar *
gst_dc1394_src_get_prop_camera_guid (GstDC1394Src * src)
{
  if (src->guid == -1) {
    return NULL;
  }
  return g_strdup_printf ("%016" G_GINT64_MODIFIER "X", src->guid);
}


static void
gst_dc1394_src_set_prop_camera_unit (GstDC1394Src * src, gint unit)
{
  src->unit = unit;
}


static gint
gst_dc1394_src_get_prop_camera_unit (GstDC1394Src * src)
{
  return src->unit;
}


static void
gst_dc1394_src_set_prop_iso_speed (GstDC1394Src * src, guint speed)
{
  switch (speed) {
    case 100:
      src->iso_speed = DC1394_ISO_SPEED_100;
      break;
    case 200:
      src->iso_speed = DC1394_ISO_SPEED_200;
      break;
    case 400:
      src->iso_speed = DC1394_ISO_SPEED_400;
      break;
    case 800:
      src->iso_speed = DC1394_ISO_SPEED_800;
      break;
    case 1600:
      src->iso_speed = DC1394_ISO_SPEED_1600;
      break;
    case 3200:
      src->iso_speed = DC1394_ISO_SPEED_3200;
      break;
    default:
      GST_ERROR_OBJECT (src, "Invalid ISO speed value: %d.", speed);
  }
}


static guint
gst_dc1394_src_get_prop_iso_speed (GstDC1394Src * src)
{
  switch (src->iso_speed) {
    case DC1394_ISO_SPEED_100:
      return 100;
    case DC1394_ISO_SPEED_200:
      return 200;
    case DC1394_ISO_SPEED_400:
      return 400;
    case DC1394_ISO_SPEED_800:
      return 800;
    case DC1394_ISO_SPEED_1600:
      return 1600;
    case DC1394_ISO_SPEED_3200:
      return 3200;
    default:                   /* never reached */
      return DC1394_ISO_SPEED_MIN - 1;
  }
}


static void
gst_dc1394_src_set_prop_dma_buffer_size (GstDC1394Src * src, guint size)
{
  src->dma_buffer_size = size;
}


static guint
gst_dc1394_src_get_prop_dma_buffer_size (GstDC1394Src * src)
{
  return src->dma_buffer_size;
}


static gboolean
gst_dc1394_src_open_cam (GstDC1394Src * src)
{
  dc1394camera_list_t *cameras;
  dc1394error_t ret;
  int number;
  uint64_t guid;
  int unit, i;

  src->dc1394 = dc1394_new ();
  if (!src->dc1394) {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not initialize dc1394 library."));
    goto error;
  }

  number = -1;
  guid = -1;
  unit = -1;
  ret = dc1394_camera_enumerate (src->dc1394, &cameras);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, LIBRARY, FAILED, (NULL),
        ("Could not enumerate cameras: %s.", dc1394_error_get_string (ret)));
    goto error;
  }
  for (i = 0; i < cameras->num; i++) {
    GST_DEBUG_OBJECT (src, "Camera %2d is %016" G_GINT64_MODIFIER "X %d.",
        i, cameras->ids[i].guid, cameras->ids[i].unit);
    if ((src->guid == -1 || src->guid == cameras->ids[i].guid) &&
        (src->unit == -1 || src->unit == cameras->ids[i].unit)) {
      number = i;
      guid = cameras->ids[i].guid;
      unit = cameras->ids[i].unit;
    }
  }
  dc1394_camera_free_list (cameras);
  if (number < 0) {
    if (src->guid == -1) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
          ("No cameras found."));
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
          ("Camera %016" G_GINT64_MODIFIER "X %d not found.",
              src->guid, src->unit));
    }
    goto error;
  }

  GST_DEBUG_OBJECT (src, "Open camera %016" G_GINT64_MODIFIER "X %d.",
      guid, unit);
  src->camera = dc1394_camera_new_unit (src->dc1394, guid, unit);
  if (!src->camera) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not open camera %016" G_GINT64_MODIFIER "X %d.", guid, unit));
    goto error;
  }
  GST_DEBUG_OBJECT (src,
      "Camera %016" G_GINT64_MODIFIER "X %d opened: \"%s %s\".",
      src->camera->guid, src->camera->unit,
      src->camera->vendor, src->camera->model);

  if (src->iso_speed > DC1394_ISO_SPEED_400) {
    ret = dc1394_video_set_operation_mode (src->camera,
        DC1394_OPERATION_MODE_1394B);
    if (ret != DC1394_SUCCESS) {
      GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
          ("Could not set 1394B operation mode: %s.",
              dc1394_error_get_string (ret)));
      goto error;
    }
  }
  ret = dc1394_video_set_iso_speed (src->camera, src->iso_speed);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not set ISO speed %d: %s.", src->iso_speed,
            dc1394_error_get_string (ret)));
    goto error;
  }

  return TRUE;

error:
  if (src->camera) {
    dc1394_camera_free (src->camera);
    src->camera = NULL;
  }
  if (src->dc1394) {
    dc1394_free (src->dc1394);
    src->dc1394 = NULL;
  }
  return FALSE;
}


static void
gst_dc1394_src_close_cam (GstDC1394Src * src)
{
  GST_DEBUG_OBJECT (src,
      "Close camera %016" G_GINT64_MODIFIER "X %d: \"%s %s\".",
      src->camera->guid, src->camera->unit,
      src->camera->vendor, src->camera->model);
  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  dc1394_camera_free (src->camera);
  src->camera = NULL;
  dc1394_free (src->dc1394);
  src->dc1394 = NULL;
  GST_DEBUG_OBJECT (src, "Camera closed.");
}


static gboolean
gst_dc1394_src_start_cam (GstDC1394Src * src)
{
  dc1394error_t ret;
  dc1394switch_t status;
  guint trials;

  GST_DEBUG_OBJECT (src, "Setup capture with a DMA buffer of %d frames",
      src->dma_buffer_size);
  ret = dc1394_capture_setup (src->camera, src->dma_buffer_size,
      DC1394_CAPTURE_FLAGS_DEFAULT);
  if (ret == DC1394_NO_BANDWIDTH) {
    GST_DEBUG_OBJECT (src,
        "Could not setup capture with available ISO bandwidth,"
        "releasing channels and bandwidth and retrying...");
    ret = dc1394_iso_release_all (src->camera);
    if (ret != DC1394_SUCCESS) {
      GST_ELEMENT_WARNING (src, RESOURCE, FAILED, (NULL),
          ("Could not release ISO channels and bandwidth: %s",
              dc1394_error_get_string (ret)));
    }
    ret = dc1394_capture_setup (src->camera, src->dma_buffer_size,
        DC1394_CAPTURE_FLAGS_DEFAULT);
  }
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, (NULL),
        ("Could not setup capture: %s", dc1394_error_get_string (ret)));
    goto error_capture;
  }

  /*
   * TODO: dc1394_capture_setup/stop can start/stop the transmission
   * when called with DC1394_CAPTURE_FLAGS_AUTO_ISO in the flags.
   * The repeated trials check is a leftover of the original code,
   * and might not be needed.
   */
  GST_DEBUG_OBJECT (src, "Enable camera transmission.");
  ret = dc1394_video_set_transmission (src->camera, DC1394_ON);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, (NULL),
        ("Could not set transmission status: %s.",
            dc1394_error_get_string (ret)));
    goto error_transmission;
  }
  ret = dc1394_video_get_transmission (src->camera, &status);
  for (trials = 10;
      (trials > 0) && !(ret == DC1394_SUCCESS && status == DC1394_ON);
      trials--) {
    GST_DEBUG_OBJECT (src,
        "Wait for camera to start transmission (%d trials left).", trials);
    g_usleep (50000);
    ret = dc1394_video_get_transmission (src->camera, &status);
  }
  if (!(ret == DC1394_SUCCESS && status == DC1394_ON)) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, (NULL),
        ("Could not get positive transmission status: %s.",
            dc1394_error_get_string (ret)));
    goto error_transmission;
  }

  GST_DEBUG_OBJECT (src, "Capture successfully started.");
  return TRUE;

error_transmission:
  ret = dc1394_capture_stop (src->camera);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_WARNING (src, RESOURCE, FAILED, (NULL),
        ("Could not stop capture: %s.", dc1394_error_get_string (ret)));
  }
error_capture:
  return FALSE;
}


static gboolean
gst_dc1394_src_stop_cam (GstDC1394Src * src)
{
  dc1394error_t ret;
  dc1394switch_t status;
  guint trials;

  /*
   * TODO: dc1394_capture_setup/stop can start/stop the transmission
   * when called with DC1394_CAPTURE_FLAGS_AUTO_ISO in the flags.
   * The repeated trials check is a leftover of the original code,
   * and might not be needed.
   */
  GST_DEBUG_OBJECT (src, "Disable camera transmission.");
  ret = dc1394_video_set_transmission (src->camera, DC1394_OFF);
  if (ret != DC1394_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, (NULL),
        ("Could not set transmission status: %s.",
            dc1394_error_get_string (ret)));
    return FALSE;
  }
  ret = dc1394_video_get_transmission (src->camera, &status);
  for (trials = 10;
      (trials > 0) && !(ret == DC1394_SUCCESS && status == DC1394_OFF);
      trials--) {
    GST_DEBUG_OBJECT (src,
        "Wait for camera to stop transmission (%d trials left).", trials);
    g_usleep (50000);
    ret = dc1394_video_get_transmission (src->camera, &status);
  }
  if (!(ret == DC1394_SUCCESS && status == DC1394_OFF)) {
    GST_WARNING_OBJECT (src,
        "Could not get negative transmission status: %s.",
        dc1394_error_get_string (ret));
  }

  GST_DEBUG_OBJECT (src, "Clear capture resources.");
  ret = dc1394_capture_stop (src->camera);
  if (ret != DC1394_SUCCESS && ret != DC1394_CAPTURE_IS_NOT_SET) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, (NULL),
        ("Could not clear capture: %s.", dc1394_error_get_string (ret)));
    return FALSE;
  }

  switch (ret) {
    case DC1394_CAPTURE_IS_NOT_SET:
      GST_DEBUG_OBJECT (src, "Capture was not set up.");
      break;
    case DC1394_SUCCESS:
      GST_DEBUG_OBJECT (src, "Capture successfully stopped.");
      break;
    default:
      break;
  }

  return TRUE;
}


static gboolean
gst_dc1394_src_set_cam_caps (GstDC1394Src * src, GstCaps * caps)
{
  GstCaps *mode_caps;
  gboolean ok, supported;
  dc1394video_modes_t supported_modes;
  dc1394video_mode_t mode;
  dc1394color_codings_t supported_codings;
  dc1394color_coding_t coding;
  dc1394framerates_t supported_rates;
  dc1394framerate_t rate;
  double rate_decimal;
  uint64_t total_bytes;
  uint32_t width, width_step, height, height_step;
  guint m, c;

  ok = dc1394_video_get_supported_modes (src->camera,
      &supported_modes) == DC1394_SUCCESS;
  if (!ok) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not get supported modes."));
    goto error;
  }
  supported = FALSE;
  for (m = 0; m < supported_modes.num && !supported; m++) {
    mode = supported_modes.modes[m];
    mode_caps = gst_caps_new_empty ();
    if (dc1394_is_video_mode_scalable (mode)) {
      ok &= dc1394_format7_get_color_codings (src->camera, mode,
          &supported_codings) == DC1394_SUCCESS;
      ok &= dc1394_format7_get_max_image_size (src->camera, mode,
          &width, &height) == DC1394_SUCCESS;
      ok &= dc1394_format7_get_unit_size (src->camera, mode,
          &width_step, &height_step) == DC1394_SUCCESS;
    } else {
      ok &= dc1394_get_color_coding_from_video_mode (src->camera, mode,
          &coding) == DC1394_SUCCESS;
      ok &= dc1394_get_image_size_from_video_mode (src->camera, mode,
          &width, &height) == DC1394_SUCCESS;
      ok &= dc1394_video_get_supported_framerates (src->camera, mode,
          &supported_rates) == DC1394_SUCCESS;
    }
    if (!ok) {
      GST_ELEMENT_WARNING (src, RESOURCE, SETTINGS, (NULL),
          ("Could not get video mode %d parameters.", mode));
    } else if (dc1394_is_video_mode_scalable (mode)) {
      gst_caps_append (mode_caps,
          gst_dc1394_src_build_caps (&supported_codings, NULL,
              width_step, width, width_step, height_step, height, height_step));
    } else {
      supported_codings.num = 1;
      supported_codings.codings[0] = coding;
      gst_caps_append (mode_caps,
          gst_dc1394_src_build_caps (&supported_codings, &supported_rates,
              width, width, 1, height, height, 1));
    }
    supported = gst_caps_can_intersect (caps, mode_caps);
    gst_caps_unref (mode_caps);
  }
  ok = supported && gst_dc1394_src_parse_caps (caps, &supported_codings, &rate,
      &rate_decimal, &width, &height);
  if (!ok) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Unsupported caps %" GST_PTR_FORMAT, caps));
    goto error;
  }
  GST_DEBUG_OBJECT (src, "Set video mode %d.", mode);
  ok = dc1394_video_set_mode (src->camera, mode) == DC1394_SUCCESS;
  if (!ok) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not set video mode %d.", mode));
    goto error;
  }
  if (dc1394_is_video_mode_scalable (mode)) {
    ok = FALSE;
    for (c = 0; c < supported_codings.num && !ok; c++) {
      coding = supported_codings.codings[c];
      GST_DEBUG_OBJECT (src,
          "Try format7 video mode %d with coding %d, size %d %d, and rate %.4f Hz.",
          mode, coding, width, height, rate_decimal);
      ok = (dc1394_format7_set_color_coding (src->camera, mode,
              coding) == DC1394_SUCCESS)
          && (dc1394_format7_set_image_size (src->camera, mode,
              width, height) == DC1394_SUCCESS)
          && (dc1394_format7_get_total_bytes (src->camera, mode,
              &total_bytes) == DC1394_SUCCESS)
          && (dc1394_format7_set_packet_size (src->camera, mode,
              total_bytes * rate_decimal * 0.000125) == DC1394_SUCCESS);
    }
  } else {
    GST_DEBUG_OBJECT (src, "Set fixed video mode %d rate %.4f Hz (%d).",
        mode, rate_decimal, rate);
    ok = dc1394_video_set_framerate (src->camera, rate) == DC1394_SUCCESS;
  }
  /* TODO: check feature framerate */
  if (!ok) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not set video mode %d parameters.", mode));
    goto error;
  }
  return TRUE;

error:
  return FALSE;
}


GstCaps *
gst_dc1394_src_get_cam_caps (GstDC1394Src * src)
{
  gboolean ok;
  dc1394video_modes_t supported_modes;
  dc1394video_mode_t mode;
  dc1394color_codings_t supported_codings;
  dc1394color_coding_t coding;
  dc1394framerates_t supported_rates;
  uint32_t width, width_step, height, height_step;
  guint m;

  if (src->caps)
    return gst_caps_ref (src->caps);

  ok = dc1394_video_get_supported_modes (src->camera,
      &supported_modes) == DC1394_SUCCESS;
  if (!ok) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not get supported modes."));
    return NULL;
  }

  src->caps = gst_caps_new_empty ();
  for (m = 0; m < supported_modes.num; m++) {
    mode = supported_modes.modes[m];
    if (dc1394_is_video_mode_scalable (mode)) {
      ok &= dc1394_format7_get_color_codings (src->camera, mode,
          &supported_codings) == DC1394_SUCCESS;
      ok &= dc1394_format7_get_max_image_size (src->camera, mode,
          &width, &height) == DC1394_SUCCESS;
      ok &= dc1394_format7_get_unit_size (src->camera, mode,
          &width_step, &height_step) == DC1394_SUCCESS;
      if (!ok) {
        GST_ELEMENT_WARNING (src, RESOURCE, SETTINGS, (NULL),
            ("Could not get format7 video mode %d parameters.", mode));
      } else {
        gst_caps_append (src->caps,
            gst_dc1394_src_build_caps (&supported_codings, NULL,
                width_step, width, width_step, height_step, height,
                height_step));
      }
    } else {
      ok &= dc1394_get_image_size_from_video_mode (src->camera, mode,
          &width, &height) == DC1394_SUCCESS;
      ok &= dc1394_video_get_supported_framerates (src->camera, mode,
          &supported_rates) == DC1394_SUCCESS;
      ok &= dc1394_get_color_coding_from_video_mode (src->camera, mode,
          &coding) == DC1394_SUCCESS;
      if (!ok) {
        GST_ELEMENT_WARNING (src, RESOURCE, SETTINGS, (NULL),
            ("Could not get fixed video mode %d parameters.", mode));
      } else {
        supported_codings.num = 1;
        supported_codings.codings[0] = coding;
        gst_caps_append (src->caps,
            gst_dc1394_src_build_caps (&supported_codings, &supported_rates,
                width, width, 1, height, height, 1));
      }
    }
  }
  GST_DEBUG_OBJECT (src, "Camera capabilities: \"%" GST_PTR_FORMAT "\".",
      src->caps);
  return gst_caps_ref (src->caps);
}


static GstCaps *
gst_dc1394_src_get_all_caps (void)
{
  GstCaps *caps;
  dc1394color_coding_t coding;
  dc1394color_codings_t video_codings;
  uint32_t width, height;

  const dc1394color_codings_t supported_codings = { 7, {
          /* DC1394_COLOR_CODING_RGB16S, DC1394_COLOR_CODING_RGB16, */
          DC1394_COLOR_CODING_RGB8, DC1394_COLOR_CODING_YUV444,
          DC1394_COLOR_CODING_YUV422, DC1394_COLOR_CODING_YUV411,
          /* DC1394_COLOR_CODING_RAW16, DC1394_COLOR_CODING_MONO16S */
          DC1394_COLOR_CODING_MONO16, DC1394_COLOR_CODING_RAW8,
      DC1394_COLOR_CODING_MONO8}
  };
  const dc1394framerates_t all_rates = { 8, {
          DC1394_FRAMERATE_1_875, DC1394_FRAMERATE_3_75, DC1394_FRAMERATE_7_5,
          DC1394_FRAMERATE_15, DC1394_FRAMERATE_30, DC1394_FRAMERATE_60,
      DC1394_FRAMERATE_120, DC1394_FRAMERATE_240}
  };
  dc1394video_mode_t mode;

  caps = gst_caps_new_empty ();
  /* First caps for fixed video modes */
  for (mode = DC1394_VIDEO_MODE_MIN; mode < DC1394_VIDEO_MODE_EXIF; mode++) {
    dc1394_get_image_size_from_video_mode (NULL, mode, &width, &height);
    dc1394_get_color_coding_from_video_mode (NULL, mode, &coding);
    video_codings.codings[0] = coding;
    video_codings.num = 1;
    gst_caps_append (caps,
        gst_dc1394_src_build_caps (&video_codings, &all_rates,
            width, width, 1, height, height, 1));
  }
  /* Then caps for Format 7 modes */
  gst_caps_append (caps,
      gst_dc1394_src_build_caps (&supported_codings, NULL,
          1, G_MAXINT, 1, 1, G_MAXINT, 1));
  return caps;
}


static GstCaps *
gst_dc1394_src_build_caps (const dc1394color_codings_t * supported_codings,
    const dc1394framerates_t * supported_rates,
    uint32_t width_min, uint32_t width_max, uint32_t width_step,
    uint32_t height_min, uint32_t height_max, uint32_t height_step)
{
  GstCaps *caps;
  GstStructure *structure;
  GstVideoFormat video_format;
  dc1394color_coding_t coding;
  dc1394framerate_t rate;
  GValue format = { 0 };
  GValue formats = { 0 };
  GValue width = { 0 };
  GValue widths = { 0 };
  GValue height = { 0 };
  GValue heights = { 0 };
  GValue framerate = { 0 };
  GValue framerates = { 0 };
  guint c, w, h, r;

  caps = gst_caps_new_empty ();
  for (c = 0; c < supported_codings->num; c++) {
    coding = supported_codings->codings[c];
    switch (coding) {
      case DC1394_COLOR_CODING_MONO8:
        video_format = GST_VIDEO_FORMAT_GRAY8;
        break;
      case DC1394_COLOR_CODING_YUV411:
        video_format = GST_VIDEO_FORMAT_IYU1;
        break;
      case DC1394_COLOR_CODING_YUV422:
        video_format = GST_VIDEO_FORMAT_UYVY;
        break;
      case DC1394_COLOR_CODING_YUV444:
        video_format = GST_VIDEO_FORMAT_IYU2;
        break;
      case DC1394_COLOR_CODING_RGB8:
        video_format = GST_VIDEO_FORMAT_RGB;
        break;
      case DC1394_COLOR_CODING_RAW8:
        video_format = GST_VIDEO_FORMAT_UNKNOWN;        /* GST_BAYER_FORMAT_XXXX8 */
        break;
      case DC1394_COLOR_CODING_MONO16:
        video_format = GST_VIDEO_FORMAT_GRAY16_BE;
        break;
        /*
         * The following formats do not exist in Gstreamer:
         *case DC1394_COLOR_CODING_RGB16: // Unsigned RGB 16 bits per channel
         *  video_format = GST_VIDEO_FORMAT_RGB48;
         *  break;
         *case DC1394_COLOR_CODING_MONO16S: // Signed grayscale 16 bits
         *  video_format = GST_VIDEO_FORMAT_GRAY16_BE_SIGNED;
         *  break;
         *case DC1394_COLOR_CODING_RGB16S: // Signed RGB 16 bits per channel
         *  video_format = GST_VIDEO_FORMAT_RGB48_SIGNED;
         *  break;
         *case DC1394_COLOR_CODING_RAW16: // Raw sensor output (bayer) 16 bits
         *  video_format = GST_VIDEO_FORMAT_UNKNOWN; // GST_BAYER_FORMAT_XXXX16_BE
         *  break;
         */
      default:
        video_format = GST_VIDEO_FORMAT_UNKNOWN;
        GST_DEBUG ("unsupported dc1394 video coding %d", coding);
    }
    if (video_format != GST_VIDEO_FORMAT_UNKNOWN) {
      g_value_init (&formats, G_TYPE_STRING);
      g_value_set_string (&formats, gst_video_format_to_string (video_format));
      structure = gst_structure_new_empty ("video/x-raw");
      gst_structure_set_value (structure, "format", &formats);
      gst_caps_append_structure (caps, structure);
      g_value_unset (&formats);
    }
    if (coding == DC1394_COLOR_CODING_MONO8 ||
        coding == DC1394_COLOR_CODING_RAW8) {
      g_value_init (&formats, GST_TYPE_LIST);
      g_value_init (&format, G_TYPE_STRING);
      g_value_set_static_string (&format, "bggr");
      gst_value_list_append_value (&formats, &format);
      g_value_set_static_string (&format, "rggb");
      gst_value_list_append_value (&formats, &format);
      g_value_set_static_string (&format, "grbg");
      gst_value_list_append_value (&formats, &format);
      g_value_set_static_string (&format, "gbrg");
      gst_value_list_append_value (&formats, &format);
      structure = gst_structure_new_empty ("video/x-bayer");
      gst_structure_set_value (structure, "format", &formats);
      gst_caps_append_structure (caps, structure);
      g_value_unset (&format);
      g_value_unset (&formats);
    }
  }

  if (width_min == width_max) {
    g_value_init (&widths, G_TYPE_INT);
    g_value_set_int (&widths, width_min);
  } else if (width_step == 1) {
    g_value_init (&widths, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (&widths, width_min, width_max);
  } else {
    g_value_init (&widths, GST_TYPE_LIST);
    g_value_init (&width, G_TYPE_INT);
    for (w = width_min; w <= width_max; w += width_step) {
      g_value_set_int (&width, w);
      gst_value_list_append_value (&widths, &width);
    }
    g_value_unset (&width);
  }
  if (height_min == height_max) {
    g_value_init (&heights, G_TYPE_INT);
    g_value_set_int (&heights, height_min);
  } else if (height_step == 1) {
    g_value_init (&heights, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (&heights, height_min, height_max);
  } else {
    g_value_init (&heights, GST_TYPE_LIST);
    g_value_init (&height, G_TYPE_INT);
    for (h = height_min; h <= height_max; h += height_step) {
      g_value_set_int (&height, h);
      gst_value_list_append_value (&heights, &height);
    }
    g_value_unset (&height);
  }
  gst_caps_set_value (caps, "width", &widths);
  gst_caps_set_value (caps, "height", &heights);
  g_value_unset (&widths);
  g_value_unset (&heights);

  if (supported_rates) {
    g_value_init (&framerates, GST_TYPE_LIST);
    g_value_init (&framerate, GST_TYPE_FRACTION);
    for (r = 0; r < supported_rates->num; r++) {
      rate = supported_rates->framerates[r];
      switch (rate) {
        case DC1394_FRAMERATE_1_875:
          gst_value_set_fraction (&framerate, 240, 128);
          break;
        case DC1394_FRAMERATE_3_75:
          gst_value_set_fraction (&framerate, 240, 64);
          break;
        case DC1394_FRAMERATE_7_5:
          gst_value_set_fraction (&framerate, 240, 32);
          break;
        case DC1394_FRAMERATE_15:
          gst_value_set_fraction (&framerate, 240, 16);
          break;
        case DC1394_FRAMERATE_30:
          gst_value_set_fraction (&framerate, 240, 8);
          break;
        case DC1394_FRAMERATE_60:
          gst_value_set_fraction (&framerate, 240, 4);
          break;
        case DC1394_FRAMERATE_120:
          gst_value_set_fraction (&framerate, 240, 2);
          break;
        case DC1394_FRAMERATE_240:
          gst_value_set_fraction (&framerate, 240, 1);
          break;
      }
      gst_value_list_append_value (&framerates, &framerate);
    }
    g_value_unset (&framerate);
  } else {
    g_value_init (&framerates, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full (&framerates, 1, G_MAXINT, G_MAXINT, 1);
  }
  gst_caps_set_value (caps, "framerate", &framerates);
  g_value_unset (&framerates);
  return caps;
}


static gboolean
gst_dc1394_src_parse_caps (const GstCaps * caps,
    dc1394color_codings_t * color_codings,
    dc1394framerate_t * rate, double *rate_decimal,
    uint32_t * width, uint32_t * height)
{
  const GstStructure *structure;
  const gchar *format;
  gint w, h, num, den;
  gdouble dec;

  structure = gst_caps_get_structure (caps, 0);
  if (!structure)
    goto error;

  if (!gst_structure_get_int (structure, "width", &w)
      || !gst_structure_get_int (structure, "height", &h))
    goto error;

  *width = w;
  *height = h;

  if (!gst_structure_get_fraction (structure, "framerate", &num, &den))
    goto error;

  if (gst_util_fraction_compare (num, den, 240, 128) <= 0) {
    *rate = DC1394_FRAMERATE_1_875;
  } else if (gst_util_fraction_compare (num, den, 240, 64) <= 0) {
    *rate = DC1394_FRAMERATE_3_75;
  } else if (gst_util_fraction_compare (num, den, 240, 32) <= 0) {
    *rate = DC1394_FRAMERATE_7_5;
  } else if (gst_util_fraction_compare (num, den, 240, 16) <= 0) {
    *rate = DC1394_FRAMERATE_15;
  } else if (gst_util_fraction_compare (num, den, 240, 8) <= 0) {
    *rate = DC1394_FRAMERATE_30;
  } else if (gst_util_fraction_compare (num, den, 240, 4) <= 0) {
    *rate = DC1394_FRAMERATE_60;
  } else if (gst_util_fraction_compare (num, den, 240, 2) <= 0) {
    *rate = DC1394_FRAMERATE_120;
  } else if (gst_util_fraction_compare (num, den, 240, 1) <= 0) {
    *rate = DC1394_FRAMERATE_240;
  } else {
    *rate = DC1394_FRAMERATE_240;
  }

  gst_util_fraction_to_double (num, den, &dec);
  *rate_decimal = dec;

  if (gst_structure_has_name (structure, "video/x-raw")) {
    format = gst_structure_get_string (structure, "format");
    switch (gst_video_format_from_string (format)) {
      case GST_VIDEO_FORMAT_GRAY8:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_MONO8;
        break;
      case GST_VIDEO_FORMAT_IYU1:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_YUV411;
        break;
      case GST_VIDEO_FORMAT_UYVY:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_YUV422;
        break;
      case GST_VIDEO_FORMAT_IYU2:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_YUV444;
        break;
      case GST_VIDEO_FORMAT_RGB:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_RGB8;
        break;
      case GST_VIDEO_FORMAT_GRAY16_BE:
        color_codings->num = 1;
        color_codings->codings[0] = DC1394_COLOR_CODING_MONO16;
        break;
        /*
         * The following formats do not exist in Gstreamer:
         *case GST_VIDEO_FORMAT_RGB48: // Unsigned RGB format 16 bits per channel
         *  color_codings->num = 1
         *  color_codings->codings[0] = DC1394_COLOR_CODING_RGB16;
         *  break;
         *case GST_VIDEO_FORMAT_GRAY16_BE_SIGNED: // Signed grayscale format 16 bits
         *  color_codings->num = 1
         *  color_codings->codings[0] = DC1394_COLOR_CODING_MONO16S;
         *  break;
         *case GST_VIDEO_FORMAT_RGB48_SIGNED: // Signed RGB format 16 bits per channel
         *  color_codings->num = 1
         *  color_codings->codings[0] = DC1394_COLOR_CODING_RGB16S;
         *  break;
         */
      default:
        GST_ERROR ("unsupported raw video format %s", format);
        goto error;
    }
  } else if (gst_structure_has_name (structure, "video/x-bayer")) {
    /*
     * The following formats do not exist in Gstreamer:
     *switch (gst_bayer_format_from_string(format)) {
     *  case GST_BAYER_FORMAT_BGGR8:
     *  case GST_BAYER_FORMAT_GBRG8:
     *  case GST_BAYER_FORMAT_GRBG8:
     *  case GST_BAYER_FORMAT_BGGR8:
     *    *coding = DC1394_COLOR_CODING_RAW8;
     *    break;
     *  case GST_BAYER_FORMAT_BGGR16_BE:
     *  case GST_BAYER_FORMAT_GBRG16_BE:
     *  case GST_BAYER_FORMAT_GRBG16_BE:
     *  case GST_BAYER_FORMAT_BGGR16_BE:
     *    *coding = DC1394_COLOR_CODING_RAW16;
     *    break;
     *  default:
     *    GST_ERROR("unsupported raw video format %s", format);
     *    goto error;
     *}
     */
    color_codings->num = 2;
    color_codings->codings[0] = DC1394_COLOR_CODING_RAW8;
    color_codings->codings[1] = DC1394_COLOR_CODING_MONO8;
  } else {
    goto error;
  }

  return TRUE;

error:
  return FALSE;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (dc1394src, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dc1394,
    "1394 IIDC video source",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
