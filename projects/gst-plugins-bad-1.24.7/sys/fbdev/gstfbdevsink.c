/* GStreamer fbdev plugin
 * Copyright (C) 2007 Sean D'Epagnier <sean@depagnier.com>
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

/* currently the driver does not switch modes, instead uses current mode.
   the video is centered and cropped if needed to fit onscreen.
   Whatever bitdepth is set is used, and tested to work for 16, 24, 32 bits
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "gstfbdevsink.h"

enum
{
  ARG_0,
  ARG_DEVICE
};

#if 0
static void gst_fbdevsink_get_times (GstBaseSink * basesink,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
#endif

static GstFlowReturn gst_fbdevsink_show_frame (GstVideoSink * videosink,
    GstBuffer * buff);

static gboolean gst_fbdevsink_start (GstBaseSink * bsink);
static gboolean gst_fbdevsink_stop (GstBaseSink * bsink);

static GstCaps *gst_fbdevsink_getcaps (GstBaseSink * bsink, GstCaps * filter);
static gboolean gst_fbdevsink_setcaps (GstBaseSink * bsink, GstCaps * caps);

static void gst_fbdevsink_finalize (GObject * object);
static void gst_fbdevsink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_fbdevsink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_fbdevsink_change_state (GstElement * element,
    GstStateChange transition);

#define VIDEO_CAPS "{ RGB, BGR, BGRx, xBGR, RGB, RGBx, xRGB, RGB15, RGB16 }"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_CAPS))
    );

#define parent_class gst_fbdevsink_parent_class
G_DEFINE_TYPE (GstFBDEVSink, gst_fbdevsink, GST_TYPE_VIDEO_SINK);
GST_ELEMENT_REGISTER_DEFINE (fbdevsink, "fbdevsink", GST_RANK_NONE,
    GST_TYPE_FBDEVSINK);

static void
gst_fbdevsink_init (GstFBDEVSink * fbdevsink)
{
  /* nothing to do here yet */
}

#if 0
static void
gst_fbdevsink_get_times (GstBaseSink * basesink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstFBDEVSink *fbdevsink;

  fbdevsink = GST_FBDEVSINK (basesink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    *start = GST_BUFFER_TIMESTAMP (buffer);
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      *end = *start + GST_BUFFER_DURATION (buffer);
    } else {
      if (fbdevsink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, fbdevsink->fps_d,
            fbdevsink->fps_n);
      }
    }
  }
}
#endif

static GstCaps *
gst_fbdevsink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstFBDEVSink *fbdevsink;
  GstVideoFormat format;
  GstCaps *caps;
  uint32_t rmask;
  uint32_t gmask;
  uint32_t bmask;
  uint32_t tmask;
  int endianness, depth, bpp;

  fbdevsink = GST_FBDEVSINK (bsink);

  caps = gst_static_pad_template_get_caps (&sink_template);

  /* FIXME: locking */
  if (!fbdevsink->framebuffer)
    goto done;

  bpp = fbdevsink->varinfo.bits_per_pixel;

  rmask = ((1 << fbdevsink->varinfo.red.length) - 1)
      << fbdevsink->varinfo.red.offset;
  gmask = ((1 << fbdevsink->varinfo.green.length) - 1)
      << fbdevsink->varinfo.green.offset;
  bmask = ((1 << fbdevsink->varinfo.blue.length) - 1)
      << fbdevsink->varinfo.blue.offset;
  tmask = ((1 << fbdevsink->varinfo.transp.length) - 1)
      << fbdevsink->varinfo.transp.offset;

  depth = fbdevsink->varinfo.red.length + fbdevsink->varinfo.green.length
      + fbdevsink->varinfo.blue.length;

  switch (fbdevsink->varinfo.bits_per_pixel) {
    case 32:
      /* swap endianness of masks */
      rmask = GUINT32_SWAP_LE_BE (rmask);
      gmask = GUINT32_SWAP_LE_BE (gmask);
      bmask = GUINT32_SWAP_LE_BE (bmask);
      tmask = GUINT32_SWAP_LE_BE (tmask);
      depth += fbdevsink->varinfo.transp.length;
      endianness = G_BIG_ENDIAN;
      break;
    case 24:{
      /* swap red and blue masks */
      tmask = rmask;
      rmask = bmask;
      bmask = tmask;
      tmask = 0;
      endianness = G_BIG_ENDIAN;
      break;
    }
    case 15:
    case 16:
      tmask = 0;
      endianness = G_LITTLE_ENDIAN;
      break;
    default:
      goto unsupported_bpp;
  }

  format = gst_video_format_from_masks (depth, bpp, endianness, rmask, gmask,
      bmask, tmask);

  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    goto unknown_format;

  caps = gst_caps_make_writable (caps);
  gst_caps_set_simple (caps, "format", G_TYPE_STRING,
      gst_video_format_to_string (format), NULL);

done:

  if (filter != NULL) {
    GstCaps *icaps;

    icaps = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = icaps;
  }

  return caps;

/* ERRORS */
unsupported_bpp:
  {
    GST_WARNING_OBJECT (bsink, "unsupported bit depth: %d", bpp);
    return NULL;
  }
unknown_format:
  {
    GST_WARNING_OBJECT (bsink, "could not map fbdev format to GstVideoFormat: "
        "depth=%u, bpp=%u, endianness=%u, rmask=0x%08x, gmask=0x%08x, "
        "bmask=0x%08x, tmask=0x%08x", depth, bpp, endianness, rmask, gmask,
        bmask, tmask);
    return NULL;
  }
}

static gboolean
gst_fbdevsink_setcaps (GstBaseSink * bsink, GstCaps * vscapslist)
{
  GstFBDEVSink *fbdevsink;
  GstStructure *structure;
  const GValue *fps;

  fbdevsink = GST_FBDEVSINK (bsink);

  structure = gst_caps_get_structure (vscapslist, 0);

  fps = gst_structure_get_value (structure, "framerate");
  fbdevsink->fps_n = gst_value_get_fraction_numerator (fps);
  fbdevsink->fps_d = gst_value_get_fraction_denominator (fps);

  gst_structure_get_int (structure, "width", &fbdevsink->width);
  gst_structure_get_int (structure, "height", &fbdevsink->height);

  /* calculate centering and scanlengths for the video */
  fbdevsink->bytespp =
      fbdevsink->fixinfo.line_length / fbdevsink->varinfo.xres_virtual;

  fbdevsink->cx = ((int) fbdevsink->varinfo.xres - fbdevsink->width) / 2;
  if (fbdevsink->cx < 0)
    fbdevsink->cx = 0;

  fbdevsink->cy = ((int) fbdevsink->varinfo.yres - fbdevsink->height) / 2;
  if (fbdevsink->cy < 0)
    fbdevsink->cy = 0;

  fbdevsink->linelen = fbdevsink->width * fbdevsink->bytespp;
  if (fbdevsink->linelen > fbdevsink->fixinfo.line_length)
    fbdevsink->linelen = fbdevsink->fixinfo.line_length;

  fbdevsink->lines = fbdevsink->height;
  if (fbdevsink->lines > fbdevsink->varinfo.yres)
    fbdevsink->lines = fbdevsink->varinfo.yres;

  return TRUE;
}


static GstFlowReturn
gst_fbdevsink_show_frame (GstVideoSink * videosink, GstBuffer * buf)
{

  GstFBDEVSink *fbdevsink;
  GstMapInfo map;
  int i;

  fbdevsink = GST_FBDEVSINK (videosink);

  /* optimization could remove this memcpy by allocating the buffer
     in framebuffer memory, but would only work when xres matches
     the video width */
  if (!gst_buffer_map (buf, &map, GST_MAP_READ))
    return GST_FLOW_ERROR;

  for (i = 0; i < fbdevsink->lines; i++) {
    memcpy (fbdevsink->framebuffer
        + (i + fbdevsink->cy) * fbdevsink->fixinfo.line_length
        + fbdevsink->cx * fbdevsink->bytespp,
        map.data + i * fbdevsink->width * fbdevsink->bytespp,
        fbdevsink->linelen);
  }

  gst_buffer_unmap (buf, &map);

  return GST_FLOW_OK;
}

static gboolean
gst_fbdevsink_start (GstBaseSink * bsink)
{
  GstFBDEVSink *fbdevsink;

  fbdevsink = GST_FBDEVSINK (bsink);

  if (!fbdevsink->device) {
    fbdevsink->device = g_strdup ("/dev/fb0");
  }

  fbdevsink->fd = open (fbdevsink->device, O_RDWR);

  if (fbdevsink->fd == -1)
    return FALSE;

  /* get the fixed screen info */
  if (ioctl (fbdevsink->fd, FBIOGET_FSCREENINFO, &fbdevsink->fixinfo))
    return FALSE;

  /* get the variable screen info */
  if (ioctl (fbdevsink->fd, FBIOGET_VSCREENINFO, &fbdevsink->varinfo))
    return FALSE;

  /* map the framebuffer */
  fbdevsink->framebuffer = mmap (0, fbdevsink->fixinfo.smem_len,
      PROT_WRITE, MAP_SHARED, fbdevsink->fd, 0);
  if (fbdevsink->framebuffer == MAP_FAILED)
    return FALSE;

  return TRUE;
}

static gboolean
gst_fbdevsink_stop (GstBaseSink * bsink)
{
  GstFBDEVSink *fbdevsink;

  fbdevsink = GST_FBDEVSINK (bsink);

  if (munmap (fbdevsink->framebuffer, fbdevsink->fixinfo.smem_len))
    return FALSE;

  if (close (fbdevsink->fd))
    return FALSE;


  return TRUE;
}

static void
gst_fbdevsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFBDEVSink *fbdevsink;

  fbdevsink = GST_FBDEVSINK (object);

  switch (prop_id) {
    case ARG_DEVICE:{
      g_free (fbdevsink->device);
      fbdevsink->device = g_value_dup_string (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_fbdevsink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFBDEVSink *fbdevsink;

  fbdevsink = GST_FBDEVSINK (object);

  switch (prop_id) {
    case ARG_DEVICE:{
      g_value_set_string (value, fbdevsink->device);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_fbdevsink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_FBDEVSINK (element), GST_STATE_CHANGE_FAILURE);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    default:
      break;
  }
  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (fbdevsink, plugin);
}

static void
gst_fbdevsink_class_init (GstFBDEVSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *basesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  basesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_fbdevsink_set_property;
  gobject_class->get_property = gst_fbdevsink_get_property;
  gobject_class->finalize = gst_fbdevsink_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_fbdevsink_change_state);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_DEVICE,
      g_param_spec_string ("device", "device",
          "The framebuffer device eg: /dev/fb0", NULL, G_PARAM_READWRITE));

  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_fbdevsink_setcaps);
  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_fbdevsink_getcaps);
#if 0
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_fbdevsink_get_times);
#endif
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_fbdevsink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_fbdevsink_stop);

  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_fbdevsink_show_frame);

  gst_element_class_set_static_metadata (gstelement_class, "fbdev video sink",
      "Sink/Video", "Linux framebuffer videosink",
      "Sean D'Epagnier <sean@depagnier.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
}

static void
gst_fbdevsink_finalize (GObject * object)
{
  GstFBDEVSink *fbdevsink = GST_FBDEVSINK (object);

  g_free (fbdevsink->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fbdevsink,
    "Linux framebuffer video sink",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
