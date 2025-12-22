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
 * SECTION:element-dx9screencapsrc
 * @title: dx9screencapsrc
 *
 * This element uses DirectX to capture the desktop or a portion of it.
 * The default is capturing the whole desktop, but #GstDX9ScreenCapSrc:x,
 * #GstDX9ScreenCapSrc:y, #GstDX9ScreenCapSrc:width and
 * #GstDX9ScreenCapSrc:height can be used to select a particular region.
 * Use #GstDX9ScreenCapSrc:monitor for changing which monitor to capture
 * from.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 dx9screencapsrc ! videoconvert ! dshowvideosink
 * ]| Capture the desktop and display it.
 * |[
 * gst-launch-1.0 dx9screencapsrc x=100 y=100 width=320 height=240 !
 * videoconvert ! dshowvideosink
 * ]| Capture a portion of the desktop and display it.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdx9screencapsrc.h"
#include <gst/video/video.h>
#include <versionhelpers.h>

GST_DEBUG_CATEGORY_STATIC (dx9screencapsrc_debug);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("BGR"))
    );

#define gst_dx9screencapsrc_parent_class parent_class
G_DEFINE_TYPE (GstDX9ScreenCapSrc, gst_dx9screencapsrc, GST_TYPE_PUSH_SRC);

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

static IDirect3D9 *g_d3d9 = NULL;

/* Fwd. decl. */
static void gst_dx9screencapsrc_dispose (GObject * object);
static void gst_dx9screencapsrc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dx9screencapsrc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstCaps *gst_dx9screencapsrc_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_dx9screencapsrc_set_caps (GstBaseSrc * bsrc,
    GstCaps * caps);
static GstCaps *gst_dx9screencapsrc_get_caps (GstBaseSrc * bsrc,
    GstCaps * filter);
static gboolean gst_dx9screencapsrc_start (GstBaseSrc * bsrc);
static gboolean gst_dx9screencapsrc_stop (GstBaseSrc * bsrc);

static gboolean gst_dx9screencapsrc_unlock (GstBaseSrc * bsrc);

static GstFlowReturn gst_dx9screencapsrc_create (GstPushSrc * src,
    GstBuffer ** buf);

/* Implementation. */
static void
gst_dx9screencapsrc_class_init (GstDX9ScreenCapSrcClass * klass)
{
  GObjectClass *go_class;
  GstElementClass *e_class;
  GstBaseSrcClass *bs_class;
  GstPushSrcClass *ps_class;

  go_class = (GObjectClass *) klass;
  e_class = (GstElementClass *) klass;
  bs_class = (GstBaseSrcClass *) klass;
  ps_class = (GstPushSrcClass *) klass;

  go_class->dispose = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_dispose);
  go_class->set_property = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_set_property);
  go_class->get_property = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_get_property);

  bs_class->get_caps = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_get_caps);
  bs_class->set_caps = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_set_caps);
  bs_class->start = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_start);
  bs_class->stop = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_stop);
  bs_class->unlock = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_unlock);
  bs_class->fixate = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_fixate);

  ps_class->create = GST_DEBUG_FUNCPTR (gst_dx9screencapsrc_create);

  g_object_class_install_property (go_class, PROP_MONITOR,
      g_param_spec_int ("monitor", "Monitor",
          "Which monitor to use (0 = 1st monitor and default)",
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
      "DirectX 9 screen capture source", "Source/Video", "Captures screen",
      "Haakon Sporsheim <hakon.sporsheim@tandberg.com>");

  GST_DEBUG_CATEGORY_INIT (dx9screencapsrc_debug, "dx9screencapsrc", 0,
      "DirectX 9 screen capture source");
}

static void
gst_dx9screencapsrc_init (GstDX9ScreenCapSrc * src)
{
  static gsize deprecated_warn = 0;

  /* Set src element initial values... */
  src->surface = NULL;
  src->d3d9_device = NULL;
  src->capture_x = 0;
  src->capture_y = 0;
  src->capture_w = 0;
  src->capture_h = 0;

  src->monitor = 0;
  src->show_cursor = FALSE;
  src->monitor_info.cbSize = sizeof (MONITORINFO);

  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  if (!g_d3d9)
    g_d3d9 = Direct3DCreate9 (D3D_SDK_VERSION);
  else
    IDirect3D9_AddRef (g_d3d9);

  if (g_once_init_enter (&deprecated_warn)) {
    if (IsWindows8OrGreater ()) {
      g_warning ("\"dx9screencapsrc\" is deprecated and will be removed"
          "in the future. Use \"d3d11screencapturesrc\" element instead");
    }
    g_once_init_leave (&deprecated_warn, 1);
  }
}

static void
gst_dx9screencapsrc_dispose (GObject * object)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (object);

  if (src->surface) {
    GST_ERROR_OBJECT (object,
        "DX9 surface was not freed in _stop, freeing in _dispose!");
    IDirect3DSurface9_Release (src->surface);
    src->surface = NULL;
  }

  if (src->d3d9_device) {
    IDirect3DDevice9_Release (src->d3d9_device);
    src->d3d9_device = NULL;
  }

  if (!IDirect3D9_Release (g_d3d9))
    g_d3d9 = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_dx9screencapsrc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (object);

  switch (prop_id) {
    case PROP_MONITOR:
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
gst_dx9screencapsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (object);

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
gst_dx9screencapsrc_fixate (GstBaseSrc * bsrc, GstCaps * caps)
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
gst_dx9screencapsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (bsrc);
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

  GST_DEBUG_OBJECT (src, "size %dx%d, %d/%d fps",
      (gint) (src->src_rect.right - src->src_rect.left),
      (gint) (src->src_rect.bottom - src->src_rect.top),
      src->rate_numerator, src->rate_denominator);

  return TRUE;
}

static GstCaps *
gst_dx9screencapsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (bsrc);
  RECT rect_dst;
  GstCaps *caps;

  if (src->monitor >= IDirect3D9_GetAdapterCount (g_d3d9)) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Specified monitor with index %d not found", src->monitor), (NULL));
    return NULL;
  }

  if (FAILED (IDirect3D9_GetAdapterDisplayMode (g_d3d9, src->monitor,
              &src->disp_mode))) {
    return NULL;
  }

  SetRect (&rect_dst, 0, 0, src->disp_mode.Width, src->disp_mode.Height);
  src->screen_rect = rect_dst;

  if (src->capture_w && src->capture_h &&
      src->capture_x + src->capture_w < rect_dst.right - rect_dst.left &&
      src->capture_y + src->capture_h < rect_dst.bottom - rect_dst.top) {
    rect_dst.left = src->capture_x;
    rect_dst.top = src->capture_y;
    rect_dst.right = src->capture_x + src->capture_w;
    rect_dst.bottom = src->capture_y + src->capture_h;
  } else {
    /* Default values */
    src->capture_x = src->capture_y = 0;
    src->capture_w = src->capture_h = 0;
  }

  /* Note:
   *    Expose as xRGB even though the Surface is allocated as ARGB!
   *    This is due to IDirect3DDevice9_GetFrontBufferData which only takes
   *    ARGB surface, but the A channel is in reality never used.
   *    I should add that I had problems specifying ARGB. It might be a bug
   *    in ffmpegcolorspace which I used for testing.
   *    Another interesting thing is that directdrawsink did not support ARGB,
   *    but only xRGB. (On my system, using 32b color depth) And according to
   *    the DirectX documentation ARGB is NOT a valid display buffer format,
   *    but xRGB is.
   */
  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "BGRx",
      "width", G_TYPE_INT, rect_dst.right - rect_dst.left,
      "height", G_TYPE_INT, rect_dst.bottom - rect_dst.top,
      "framerate", GST_TYPE_FRACTION_RANGE, 1, 1, G_MAXINT, 1,
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
gst_dx9screencapsrc_start (GstBaseSrc * bsrc)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (bsrc);
  D3DPRESENT_PARAMETERS d3dpp;
  HMONITOR monitor;
  HRESULT res;

  src->frame_number = -1;

  ZeroMemory (&d3dpp, sizeof (D3DPRESENT_PARAMETERS));
  d3dpp.Windowed = TRUE;
  d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
  d3dpp.BackBufferFormat = src->disp_mode.Format;
  d3dpp.BackBufferHeight = src->disp_mode.Height;
  d3dpp.BackBufferWidth = src->disp_mode.Width;
  d3dpp.BackBufferCount = 1;
  d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
  d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  d3dpp.hDeviceWindow = GetDesktopWindow ();
  d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

  if (src->monitor >= IDirect3D9_GetAdapterCount (g_d3d9)) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Specified monitor with index %d not found", src->monitor), (NULL));
    return FALSE;
  }

  res = IDirect3D9_CreateDevice (g_d3d9, src->monitor, D3DDEVTYPE_HAL,
      GetDesktopWindow (), D3DCREATE_SOFTWARE_VERTEXPROCESSING,
      &d3dpp, &src->d3d9_device);
  if (FAILED (res))
    return FALSE;

  monitor = IDirect3D9_GetAdapterMonitor (g_d3d9, src->monitor);
  GetMonitorInfo (monitor, &src->monitor_info);

  return
      SUCCEEDED (IDirect3DDevice9_CreateOffscreenPlainSurface (src->d3d9_device,
          src->disp_mode.Width, src->disp_mode.Height, D3DFMT_A8R8G8B8,
          D3DPOOL_SYSTEMMEM, &src->surface, NULL));
}

static gboolean
gst_dx9screencapsrc_stop (GstBaseSrc * bsrc)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (bsrc);

  if (src->surface) {
    IDirect3DSurface9_Release (src->surface);
    src->surface = NULL;
  }

  return TRUE;
}

static gboolean
gst_dx9screencapsrc_unlock (GstBaseSrc * bsrc)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (bsrc);

  GST_OBJECT_LOCK (src);
  if (src->clock_id) {
    GST_DEBUG_OBJECT (src, "Waking up waiting clock");
    gst_clock_id_unschedule (src->clock_id);
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static GstFlowReturn
gst_dx9screencapsrc_create (GstPushSrc * push_src, GstBuffer ** buf)
{
  GstDX9ScreenCapSrc *src = GST_DX9SCREENCAPSRC (push_src);
  GstBuffer *new_buf;
  gint new_buf_size, i;
  gint width, height, stride;
  GstClock *clock;
  GstClockTime buf_time, buf_dur;
  D3DLOCKED_RECT locked_rect;
  LPBYTE p_dst, p_src;
  HRESULT hres;
  GstMapInfo map;
  guint64 frame_number;

  if (G_UNLIKELY (!src->d3d9_device)) {
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        ("format wasn't negotiated before create function"));
    return GST_FLOW_NOT_NEGOTIATED;
  }

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
    GST_OBJECT_UNLOCK (src);

    if (ret == GST_CLOCK_UNSCHEDULED) {
      /* Got woken up by the unlock function */
      gst_object_unref (clock);
      return GST_FLOW_FLUSHING;
    }

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

  height = (src->src_rect.bottom - src->src_rect.top);
  width = (src->src_rect.right - src->src_rect.left);
  new_buf_size = width * 4 * height;

  GST_LOG_OBJECT (src,
      "creating buffer of %d bytes with %dx%d image",
      new_buf_size, width, height);

  /* Do screen capture and put it into buffer...
   * Acquire front buffer, and lock it
   */
  hres =
      IDirect3DDevice9_GetFrontBufferData (src->d3d9_device, 0, src->surface);
  if (FAILED (hres)) {
    GST_DEBUG_OBJECT (src, "DirectX::GetBackBuffer failed.");
    if (clock != NULL)
      gst_object_unref (clock);
    return GST_FLOW_ERROR;
  }

  if (src->show_cursor) {
    CURSORINFO ci;

    ci.cbSize = sizeof (CURSORINFO);
    GetCursorInfo (&ci);
    if (ci.flags & CURSOR_SHOWING) {
      ICONINFO ii;
      HDC memDC;

      GetIconInfo (ci.hCursor, &ii);

      if (SUCCEEDED (IDirect3DSurface9_GetDC (src->surface, &memDC))) {
        HCURSOR cursor = CopyImage (ci.hCursor, IMAGE_CURSOR, 0, 0,
            LR_MONOCHROME | LR_DEFAULTSIZE);

        DrawIcon (memDC,
            ci.ptScreenPos.x - ii.xHotspot - src->monitor_info.rcMonitor.left,
            ci.ptScreenPos.y - ii.yHotspot - src->monitor_info.rcMonitor.top,
            cursor);

        DestroyCursor (cursor);
        IDirect3DSurface9_ReleaseDC (src->surface, memDC);
      }

      DeleteObject (ii.hbmColor);
      DeleteObject (ii.hbmMask);
    }
  }

  hres =
      IDirect3DSurface9_LockRect (src->surface, &locked_rect, &(src->src_rect),
      D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
  if (FAILED (hres)) {
    GST_DEBUG_OBJECT (src, "DirectX::LockRect failed.");
    if (clock != NULL)
      gst_object_unref (clock);
    return GST_FLOW_ERROR;
  }

  new_buf = gst_buffer_new_and_alloc (new_buf_size);
  gst_buffer_map (new_buf, &map, GST_MAP_WRITE);
  p_dst = (LPBYTE) map.data;
  p_src = (LPBYTE) locked_rect.pBits;
  stride = width * 4;
  for (i = 0; i < height; ++i) {
    memcpy (p_dst, p_src, stride);
    p_dst += stride;
    p_src += locked_rect.Pitch;
  }
  gst_buffer_unmap (new_buf, &map);

  /* Unlock copy of front buffer */
  IDirect3DSurface9_UnlockRect (src->surface);

  GST_BUFFER_TIMESTAMP (new_buf) = buf_time;
  GST_BUFFER_DURATION (new_buf) = buf_dur;

  if (clock != NULL)
    gst_object_unref (clock);

  *buf = new_buf;
  return GST_FLOW_OK;
}
