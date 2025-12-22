/* GStreamer
 * Copyright (C) 2007 Haakon Sporsheim <hakon.sporsheim@tandberg.com>
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
 * SECTION:element-gdiscreencapsrc
 * @title: gdiscreencapsrc
 *
 * This element uses GDI to capture the desktop or a portion of it.
 * The default is capturing the whole desktop, but #GstGDIScreenCapSrc:x,
 * #GstGDIScreenCapSrc:y, #GstGDIScreenCapSrc:width and
 * #GstGDIScreenCapSrc:height can be used to select a particular region.
 * Use #GstGDIScreenCapSrc:monitor for changing which monitor to capture
 * from.
 *
 * Set #GstGDIScreenCapSrc:cursor to TRUE to include the mouse cursor.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 gdiscreencapsrc ! videoconvert ! dshowvideosink
 * ]| Capture the desktop and display it.
 * |[
 * gst-launch-1.0 gdiscreencapsrc x=100 y=100 width=320 height=240 cursor=TRUE
 * ! videoconvert ! dshowvideosink
 * ]| Capture a portion of the desktop, including the mouse cursor, and
 * display it.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgdiscreencapsrc.h"
#include <gst/video/video.h>
#include <versionhelpers.h>

GST_DEBUG_CATEGORY_STATIC (gdiscreencapsrc_debug);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("BGR")));

#define gst_gdiscreencapsrc_parent_class parent_class
G_DEFINE_TYPE (GstGDIScreenCapSrc, gst_gdiscreencapsrc, GST_TYPE_PUSH_SRC);

enum
{
  PROP_0,
  PROP_MONITOR,
  PROP_SHOW_CURSOR,
  PROP_X_POS,
  PROP_Y_POS,
  PROP_WIDTH,
  PROP_HEIGHT
};

/* Fwd. decl. */
static void gst_gdiscreencapsrc_dispose (GObject * object);
static void gst_gdiscreencapsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gdiscreencapsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_gdiscreencapsrc_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_gdiscreencapsrc_set_caps (GstBaseSrc * bsrc,
    GstCaps * caps);
static GstCaps *gst_gdiscreencapsrc_get_caps (GstBaseSrc * bsrc,
    GstCaps * filter);
static gboolean gst_gdiscreencapsrc_start (GstBaseSrc * bsrc);
static gboolean gst_gdiscreencapsrc_unlock (GstBaseSrc * bsrc);

static GstFlowReturn gst_gdiscreencapsrc_create (GstPushSrc * src,
    GstBuffer ** buf);

static gboolean gst_gdiscreencapsrc_screen_capture (GstGDIScreenCapSrc * src,
    GstBuffer * buf);

/* Implementation. */
static void
gst_gdiscreencapsrc_class_init (GstGDIScreenCapSrcClass * klass)
{
  GObjectClass *go_class;
  GstElementClass *e_class;
  GstBaseSrcClass *bs_class;
  GstPushSrcClass *ps_class;

  go_class = (GObjectClass *) klass;
  e_class = (GstElementClass *) klass;
  bs_class = (GstBaseSrcClass *) klass;
  ps_class = (GstPushSrcClass *) klass;

  go_class->dispose = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_dispose);
  go_class->set_property = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_set_property);
  go_class->get_property = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_get_property);

  bs_class->get_caps = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_get_caps);
  bs_class->set_caps = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_set_caps);
  bs_class->start = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_start);
  bs_class->unlock = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_unlock);
  bs_class->fixate = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_fixate);

  ps_class->create = GST_DEBUG_FUNCPTR (gst_gdiscreencapsrc_create);

  g_object_class_install_property (go_class, PROP_MONITOR,
      g_param_spec_int ("monitor",
          "Monitor", "Which monitor to use (0 = 1st monitor and default)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (go_class, PROP_SHOW_CURSOR,
      g_param_spec_boolean ("cursor", "Show mouse cursor",
          "Whether to show mouse cursor (default off)",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (go_class, PROP_X_POS,
      g_param_spec_int ("x", "X",
          "Horizontal coordinate of top left corner for the screen capture "
          "area", 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (go_class, PROP_Y_POS,
      g_param_spec_int ("y", "Y",
          "Vertical coordinate of top left corner for the screen capture "
          "area", 0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (go_class, PROP_WIDTH,
      g_param_spec_int ("width", "Width",
          "Width of screen capture area (0 = maximum)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (go_class, PROP_HEIGHT,
      g_param_spec_int ("height", "Height",
          "Height of screen capture area (0 = maximum)",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  gst_element_class_add_static_pad_template (e_class, &src_template);
  gst_element_class_set_static_metadata (e_class,
      "GDI screen capture source", "Source/Video", "Captures screen",
      "Haakon Sporsheim <hakon.sporsheim@tandberg.com>");

  GST_DEBUG_CATEGORY_INIT (gdiscreencapsrc_debug,
      "gdiscreencapsrc", 0, "GDI screen capture source");
}

static void
gst_gdiscreencapsrc_init (GstGDIScreenCapSrc * src)
{
  static gsize deprecated_warn = 0;

  /* Set src element initial values... */
  src->dibMem = NULL;
  src->hBitmap = (HBITMAP) INVALID_HANDLE_VALUE;
  src->memDC = (HDC) INVALID_HANDLE_VALUE;
  src->capture_x = 0;
  src->capture_y = 0;
  src->capture_w = 0;
  src->capture_h = 0;

  src->monitor = 0;
  src->show_cursor = FALSE;

  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  if (g_once_init_enter (&deprecated_warn)) {
    if (IsWindows8OrGreater ()) {
      g_warning ("\"gdiscreencapsrc\" is deprecated and will be removed"
          "in the future. Use \"d3d11screencapturesrc\" element instead");
    }
    g_once_init_leave (&deprecated_warn, 1);
  }
}

static void
gst_gdiscreencapsrc_dispose (GObject * object)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (object);

  if (src->hBitmap != INVALID_HANDLE_VALUE)
    DeleteObject (src->hBitmap);

  if (src->memDC != INVALID_HANDLE_VALUE)
    DeleteDC (src->memDC);

  src->hBitmap = (HBITMAP) INVALID_HANDLE_VALUE;
  src->memDC = (HDC) INVALID_HANDLE_VALUE;
  src->dibMem = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_gdiscreencapsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (object);

  switch (prop_id) {
    case PROP_MONITOR:
      if (g_value_get_int (value) >= GetSystemMetrics (SM_CMONITORS)) {
        G_OBJECT_WARN_INVALID_PSPEC (object, "Monitor", prop_id, pspec);
        break;
      }
      src->monitor = g_value_get_int (value);
      break;
    case PROP_SHOW_CURSOR:
      src->show_cursor = g_value_get_boolean (value);
      break;
    case PROP_X_POS:
      src->capture_x = g_value_get_int (value);
      break;
    case PROP_Y_POS:
      src->capture_y = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      src->capture_w = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      src->capture_h = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  };
}

static void
gst_gdiscreencapsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (object);

  switch (prop_id) {
    case PROP_MONITOR:
      g_value_set_int (value, src->monitor);
      break;
    case PROP_SHOW_CURSOR:
      g_value_set_boolean (value, src->show_cursor);
      break;
    case PROP_X_POS:
      g_value_set_int (value, src->capture_x);
      break;
    case PROP_Y_POS:
      g_value_set_int (value, src->capture_y);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->capture_w);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, src->capture_h);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  };
}

static GstCaps *
gst_gdiscreencapsrc_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 640);
  gst_structure_fixate_field_nearest_int (structure, "height", 480);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_gdiscreencapsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (bsrc);
  HWND capture;
  HDC device;
  GstStructure *structure;
  const GValue *framerate;

  structure = gst_caps_get_structure (caps, 0);

  src->src_rect = src->screen_rect;
  if (src->capture_w && src->capture_h) {
    src->src_rect.left += src->capture_x;
    src->src_rect.top += src->capture_y;
    src->src_rect.right = src->src_rect.left + src->capture_w;
    src->src_rect.bottom = src->src_rect.top + src->capture_h;
  }

  framerate = gst_structure_get_value (structure, "framerate");
  if (framerate) {
    src->rate_numerator = gst_value_get_fraction_numerator (framerate);
    src->rate_denominator = gst_value_get_fraction_denominator (framerate);
  }

  src->info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  src->info.bmiHeader.biWidth = src->src_rect.right - src->src_rect.left;
  src->info.bmiHeader.biHeight = src->src_rect.top - src->src_rect.bottom;
  src->info.bmiHeader.biPlanes = 1;
  src->info.bmiHeader.biBitCount = 24;
  src->info.bmiHeader.biCompression = BI_RGB;
  src->info.bmiHeader.biSizeImage = 0;
  src->info.bmiHeader.biXPelsPerMeter = 0;
  src->info.bmiHeader.biYPelsPerMeter = 0;
  src->info.bmiHeader.biClrUsed = 0;
  src->info.bmiHeader.biClrImportant = 0;

  /* Cleanup first */
  if (src->hBitmap != INVALID_HANDLE_VALUE)
    DeleteObject (src->hBitmap);

  if (src->memDC != INVALID_HANDLE_VALUE)
    DeleteDC (src->memDC);

  /* Allocate */
  capture = GetDesktopWindow ();
  device = GetDC (capture);
  src->hBitmap = CreateDIBSection (device, &(src->info), DIB_RGB_COLORS,
      (void **) &(src->dibMem), 0, 0);
  src->memDC = CreateCompatibleDC (device);
  SelectObject (src->memDC, src->hBitmap);
  ReleaseDC (capture, device);

  GST_DEBUG_OBJECT (src, "size %dx%d, %d/%d fps",
      (gint) src->info.bmiHeader.biWidth,
      (gint) (-src->info.bmiHeader.biHeight),
      src->rate_numerator, src->rate_denominator);

  return TRUE;
}

static GstCaps *
gst_gdiscreencapsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (bsrc);
  RECT rect_dst;
  GstCaps *caps;

  src->screen_rect = rect_dst = gst_win32_get_monitor_rect (src->monitor);

  if (src->capture_w && src->capture_h &&
      src->capture_x + src->capture_w < rect_dst.right - rect_dst.left &&
      src->capture_y + src->capture_h < rect_dst.bottom - rect_dst.top) {
    rect_dst.left = src->capture_x;
    rect_dst.top = src->capture_y;
    rect_dst.right = src->capture_x + src->capture_w;
    rect_dst.bottom = src->capture_y + src->capture_h;
  } else {
    /* Default values. */
    src->capture_x = src->capture_y = 0;
    src->capture_w = src->capture_h = 0;
  }

  GST_DEBUG ("width = %d, height=%d",
      (gint) (rect_dst.right - rect_dst.left),
      (gint) (rect_dst.bottom - rect_dst.top));

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "BGR",
      "width", G_TYPE_INT, rect_dst.right - rect_dst.left,
      "height", G_TYPE_INT, rect_dst.bottom - rect_dst.top,
      "framerate", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = tmp;
  }

  return caps;
}

static gboolean
gst_gdiscreencapsrc_start (GstBaseSrc * bsrc)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (bsrc);

  src->frame_number = -1;

  return TRUE;
}

static gboolean
gst_gdiscreencapsrc_unlock (GstBaseSrc * bsrc)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (bsrc);

  GST_OBJECT_LOCK (src);
  if (src->clock_id) {
    GST_DEBUG_OBJECT (src, "Waking up waiting clock");
    gst_clock_id_unschedule (src->clock_id);
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static GstFlowReturn
gst_gdiscreencapsrc_create (GstPushSrc * push_src, GstBuffer ** buf)
{
  GstGDIScreenCapSrc *src = GST_GDISCREENCAPSRC (push_src);
  GstBuffer *new_buf;
  gint new_buf_size;
  GstClock *clock;
  GstClockTime buf_time, buf_dur;
  guint64 frame_number;

  if (G_UNLIKELY (!src->info.bmiHeader.biWidth ||
          !src->info.bmiHeader.biHeight)) {
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        ("format wasn't negotiated before create function"));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  new_buf_size = GST_ROUND_UP_4 (src->info.bmiHeader.biWidth * 3) *
      (-src->info.bmiHeader.biHeight);

  GST_LOG_OBJECT (src,
      "creating buffer of %d bytes with %dx%d image",
      new_buf_size, (gint) src->info.bmiHeader.biWidth,
      (gint) (-src->info.bmiHeader.biHeight));

  new_buf = gst_buffer_new_and_alloc (new_buf_size);

  clock = gst_element_get_clock (GST_ELEMENT (src));
  if (clock != NULL) {
    GstClockTime time, base_time;

    /* Calculate sync time. */

    time = gst_clock_get_time (clock);
    base_time = gst_element_get_base_time (GST_ELEMENT (src));
    buf_time = time - base_time;

    if (src->rate_numerator) {
      frame_number = gst_util_uint64_scale (buf_time,
          src->rate_numerator, GST_SECOND * src->rate_denominator);
    } else {
      frame_number = -1;
    }
  } else {
    buf_time = GST_CLOCK_TIME_NONE;
    frame_number = -1;
  }

  if (frame_number != -1 && frame_number == src->frame_number) {
    GstClockID id;
    GstClockReturn ret;

    /* Need to wait for the next frame */
    frame_number += 1;

    /* Figure out what the next frame time is */
    buf_time = gst_util_uint64_scale (frame_number,
        src->rate_denominator * GST_SECOND, src->rate_numerator);

    id = gst_clock_new_single_shot_id (clock,
        buf_time + gst_element_get_base_time (GST_ELEMENT (src)));
    GST_OBJECT_LOCK (src);
    src->clock_id = id;
    GST_OBJECT_UNLOCK (src);

    GST_DEBUG_OBJECT (src, "Waiting for next frame time %" G_GUINT64_FORMAT,
        buf_time);
    ret = gst_clock_id_wait (id, NULL);
    GST_OBJECT_LOCK (src);

    gst_clock_id_unref (id);
    src->clock_id = NULL;
    if (ret == GST_CLOCK_UNSCHEDULED) {
      /* Got woken up by the unlock function */
      GST_OBJECT_UNLOCK (src);
      return GST_FLOW_FLUSHING;
    }
    GST_OBJECT_UNLOCK (src);

    /* Duration is a complete 1/fps frame duration */
    buf_dur =
        gst_util_uint64_scale_int (GST_SECOND, src->rate_denominator,
        src->rate_numerator);
  } else if (frame_number != -1) {
    GstClockTime next_buf_time;

    GST_DEBUG_OBJECT (src, "No need to wait for next frame time %"
        G_GUINT64_FORMAT " next frame = %" G_GINT64_FORMAT " prev = %"
        G_GINT64_FORMAT, buf_time, frame_number, src->frame_number);
    next_buf_time = gst_util_uint64_scale (frame_number + 1,
        src->rate_denominator * GST_SECOND, src->rate_numerator);
    /* Frame duration is from now until the next expected capture time */
    buf_dur = next_buf_time - buf_time;
  } else {
    buf_dur = GST_CLOCK_TIME_NONE;
  }
  src->frame_number = frame_number;

  GST_BUFFER_TIMESTAMP (new_buf) = buf_time;
  GST_BUFFER_DURATION (new_buf) = buf_dur;

  /* Do screen capture and put it into buffer... */
  gst_gdiscreencapsrc_screen_capture (src, new_buf);

  gst_object_unref (clock);

  *buf = new_buf;
  return GST_FLOW_OK;
}

static gboolean
gst_gdiscreencapsrc_screen_capture (GstGDIScreenCapSrc * src, GstBuffer * buf)
{
  HWND capture;
  HDC winDC;
  gint height, width;
  GstMapInfo map;

  if (G_UNLIKELY (!src->hBitmap || !src->dibMem))
    return FALSE;

  width = src->info.bmiHeader.biWidth;
  height = -src->info.bmiHeader.biHeight;

  /* Capture screen */
  capture = GetDesktopWindow ();
  winDC = GetWindowDC (capture);

  BitBlt (src->memDC, 0, 0, width, height,
      winDC, src->src_rect.left, src->src_rect.top, SRCCOPY);

  ReleaseDC (capture, winDC);

  /* Capture mouse cursor */
  if (src->show_cursor) {
    CURSORINFO ci;

    ci.cbSize = sizeof (CURSORINFO);
    GetCursorInfo (&ci);
    if (ci.flags & CURSOR_SHOWING) {
      ICONINFO ii;

      GetIconInfo (ci.hCursor, &ii);

      DrawIconEx (src->memDC,
          ci.ptScreenPos.x - src->src_rect.left - ii.xHotspot,
          ci.ptScreenPos.y - src->src_rect.top - ii.yHotspot, ci.hCursor, 0, 0,
          0, NULL, DI_DEFAULTSIZE | DI_NORMAL | DI_COMPAT);

      DeleteObject (ii.hbmColor);
      DeleteObject (ii.hbmMask);
    }
  }

  /* Copy DC bits to GST buffer */
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  memcpy (map.data, src->dibMem, map.size);
  gst_buffer_unmap (buf, &map);

  return TRUE;
}
