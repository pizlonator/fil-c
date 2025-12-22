/*
 * GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-d3d11screencapturesrc
 * @title: d3d11screencapturesrc
 *
 * A DXGI Desktop Duplication API based screen capture element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d11screencapturesrc ! queue ! d3d11videosink
 * ```
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11screencapturesrc.h"
#include "gstd3d11dxgicapture.h"
#ifdef HAVE_WINRT_CAPTURE
#include "gstd3d11winrtcapture.h"
#endif
#include "gstd3d11pluginutils.h"
#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_screen_capture_debug);
#define GST_CAT_DEFAULT gst_d3d11_screen_capture_debug

enum
{
  PROP_0,
  PROP_MONITOR_INDEX,
  PROP_MONITOR_HANDLE,
  PROP_SHOW_CURSOR,
  PROP_CROP_X,
  PROP_CROP_Y,
  PROP_CROP_WIDTH,
  PROP_CROP_HEIGHT,
  PROP_WINDOW_HANDLE,
  PROP_SHOW_BORDER,
  PROP_CAPTURE_API,
  PROP_ADAPTER,
  PROP_WINDOW_CAPTURE_MODE,
};

typedef enum
{
  GST_D3D11_SCREEN_CAPTURE_API_DXGI,
  GST_D3D11_SCREEN_CAPTURE_API_WGC,
} GstD3D11ScreenCaptureAPI;

typedef enum
{
  GST_D3D11_WINDOW_CAPTURE_DEFAULT,
  GST_D3D11_WINDOW_CAPTURE_CLIENT,
} GstD3D11WindowCaptureMode;

#ifdef HAVE_WINRT_CAPTURE
/**
 * GstD3D11ScreenCaptureAPI:
 *
 * Since: 1.22
 */
#define GST_TYPE_D3D11_SCREEN_CAPTURE_API (gst_d3d11_screen_capture_api_get_type())
static GType
gst_d3d11_screen_capture_api_get_type (void)
{
  static GType api_type = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    static const GEnumValue api_types[] = {
      /**
       * GstD3D11ScreenCaptureAPI::dxgi:
       *
       * Since: 1.22
       */
      {GST_D3D11_SCREEN_CAPTURE_API_DXGI, "DXGI Desktop Duplication", "dxgi"},

      /**
       * GstD3D11ScreenCaptureAPI::wgc:
       *
       * Since: 1.22
       */
      {GST_D3D11_SCREEN_CAPTURE_API_WGC, "Windows Graphics Capture", "wgc"},
      {0, nullptr, nullptr},
    };

    api_type = g_enum_register_static ("GstD3D11ScreenCaptureAPI", api_types);
  } GST_D3D11_CALL_ONCE_END;

  return api_type;
}

/**
 * GstD3D11WindowCaptureMode:
 *
 * Since: 1.24
 */
#define GST_TYPE_D3D11_WINDOW_CAPTURE_MODE (gst_d3d11_window_capture_mode_get_type())
static GType
gst_d3d11_window_capture_mode_get_type (void)
{
  static GType type = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    static const GEnumValue hwnd_modes[] = {
      /**
       * GstD3D11WindowCaptureMode::default:
       *
       * Since: 1.24
       */
      {GST_D3D11_WINDOW_CAPTURE_DEFAULT,
          "Capture entire window area", "default"},

      /**
       * GstD3D11WindowCaptureMode::client:
       *
       * Since: 1.24
       */
      {GST_D3D11_WINDOW_CAPTURE_CLIENT, "Capture client area", "client"},
      {0, nullptr, nullptr},
    };

    type = g_enum_register_static ("GstD3D11WindowCaptureMode", hwnd_modes);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}
#endif

#define DEFAULT_MONITOR_INDEX -1
#define DEFAULT_SHOW_CURSOR FALSE
#define DEFAULT_SHOW_BORDER FALSE
#define DEFAULT_CAPTURE_API GST_D3D11_SCREEN_CAPTURE_API_DXGI
#define DEFAULT_ADAPTER -1
#define DEFAULT_WINDOW_CAPTURE_MODE GST_D3D11_WINDOW_CAPTURE_DEFAULT

static GstStaticCaps template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
        "BGRA") ", pixel-aspect-ratio = 1/1, colorimetry = (string) sRGB; "
    GST_VIDEO_CAPS_MAKE ("BGRA") ", pixel-aspect-ratio = 1/1, "
    "colorimetry = (string) sRGB");

struct _GstD3D11ScreenCaptureSrc
{
  GstBaseSrc src;

  guint64 last_frame_no;
  GstClockID clock_id;
  GstVideoInfo video_info;

  GstD3D11Device *device;
  GstD3D11ScreenCapture *capture;

  GstBufferPool *pool;

  gint64 adapter_luid;
  gint monitor_index;
  HMONITOR monitor_handle;
  HWND window_handle;
  gboolean show_cursor;
  gboolean show_border;
  GstD3D11ScreenCaptureAPI capture_api;
  GstD3D11WindowCaptureMode hwnd_capture_mode;
  gint adapter;

  guint crop_x;
  guint crop_y;
  guint crop_w;
  guint crop_h;
  D3D11_BOX crop_box;

  gboolean flushing;
  GstClockTime min_latency;
  GstClockTime max_latency;

  gboolean downstream_supports_d3d11;

  ShaderResource resource;

  CRITICAL_SECTION lock;
};

static void gst_d3d11_screen_capture_src_dispose (GObject * object);
static void gst_d3d11_screen_capture_src_finalize (GObject * object);
static void gst_d3d11_screen_capture_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_screen_capture_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void gst_d3d11_screen_capture_src_set_context (GstElement * element,
    GstContext * context);

static GstCaps *gst_d3d11_screen_capture_src_get_caps (GstBaseSrc * bsrc,
    GstCaps * filter);
static GstCaps *gst_d3d11_screen_capture_src_fixate (GstBaseSrc * bsrc,
    GstCaps * caps);
static gboolean gst_d3d11_screen_capture_src_set_caps (GstBaseSrc * bsrc,
    GstCaps * caps);
static gboolean gst_d3d11_screen_capture_src_decide_allocation (GstBaseSrc *
    bsrc, GstQuery * query);
static gboolean gst_d3d11_screen_capture_src_start (GstBaseSrc * bsrc);
static gboolean gst_d3d11_screen_capture_src_stop (GstBaseSrc * bsrc);
static gboolean gst_d3d11_screen_capture_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_d3d11_screen_capture_src_unlock_stop (GstBaseSrc * bsrc);
static gboolean
gst_d3d11_screen_capture_src_src_query (GstBaseSrc * bsrc, GstQuery * query);

static GstFlowReturn gst_d3d11_screen_capture_src_create (GstBaseSrc * bsrc,
    guint64 offset, guint size, GstBuffer ** buf);

#define gst_d3d11_screen_capture_src_parent_class parent_class
G_DEFINE_TYPE (GstD3D11ScreenCaptureSrc, gst_d3d11_screen_capture_src,
    GST_TYPE_BASE_SRC);

static void
gst_d3d11_screen_capture_src_class_init (GstD3D11ScreenCaptureSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstCaps *caps;

  gobject_class->dispose = gst_d3d11_screen_capture_src_dispose;
  gobject_class->finalize = gst_d3d11_screen_capture_src_finalize;
  gobject_class->set_property = gst_d3d11_screen_capture_src_set_property;
  gobject_class->get_property = gst_d3d11_screen_capture_src_get_property;

  g_object_class_install_property (gobject_class, PROP_MONITOR_INDEX,
      g_param_spec_int ("monitor-index", "Monitor Index",
          "Zero-based index for monitor to capture (-1 = primary monitor)",
          -1, G_MAXINT, DEFAULT_MONITOR_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MONITOR_HANDLE,
      g_param_spec_uint64 ("monitor-handle", "Monitor Handle",
          "A HMONITOR handle of monitor to capture",
          0, G_MAXUINT64, 0,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SHOW_CURSOR,
      g_param_spec_boolean ("show-cursor",
          "Show Mouse Cursor", "Whether to show mouse cursor",
          DEFAULT_SHOW_CURSOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ScreenCaptureSrc:crop-x:
   *
   * Horizontal coordinate of top left corner for the screen capture area
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CROP_X,
      g_param_spec_uint ("crop-x", "Crop X",
          "Horizontal coordinate of top left corner for the screen capture area",
          0, G_MAXUINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ScreenCaptureSrc:crop-y:
   *
   * Vertical coordinate of top left corner for the screen capture area
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CROP_Y,
      g_param_spec_uint ("crop-y", "Crop Y",
          "Vertical coordinate of top left corner for the screen capture area",
          0, G_MAXUINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ScreenCaptureSrc:crop-width:
   *
   * Width of screen capture area (0 = maximum)
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CROP_WIDTH,
      g_param_spec_uint ("crop-width", "Crop Width",
          "Width of screen capture area (0 = maximum)",
          0, G_MAXUINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ScreenCaptureSrc:crop-height:
   *
   * Height of screen capture area (0 = maximum)
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CROP_HEIGHT,
      g_param_spec_uint ("crop-height", "Crop Height",
          "Height of screen capture area (0 = maximum)",
          0, G_MAXUINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

#ifdef HAVE_WINRT_CAPTURE
  if (gst_d3d11_winrt_capture_load_library ()) {
    /**
     * GstD3D11ScreenCaptureSrc:window-handle:
     *
     * HWND window handle to capture
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_WINDOW_HANDLE,
        g_param_spec_uint64 ("window-handle", "Window Handle",
            "A HWND handle of window to capture",
            0, G_MAXUINT64, 0,
            (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS)));

    /**
     * GstD3D11ScreenCaptureSrc:show-border:
     *
     * Show border lines to capture area when WGC mode is selected.
     * This feature requires Windows11 or newer
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_SHOW_BORDER,
        g_param_spec_boolean ("show-border", "Show Border",
            "Show border lines to capture area when WGC mode is selected",
            DEFAULT_SHOW_BORDER,
            (GParamFlags) (GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_READWRITE
                | G_PARAM_STATIC_STRINGS)));

    /**
     * GstD3D11ScreenCaptureSrc:capture-api:
     *
     * Capture API to use
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_CAPTURE_API,
        g_param_spec_enum ("capture-api", "Capture API", "Capture API to use",
            GST_TYPE_D3D11_SCREEN_CAPTURE_API,
            DEFAULT_CAPTURE_API,
            (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS)));
    gst_type_mark_as_plugin_api (GST_TYPE_D3D11_SCREEN_CAPTURE_API,
        (GstPluginAPIFlags) 0);

    /**
     * GstD3D11ScreenCaptureSrc:adapter:
     *
     * DXGI Adapter index for creating device when WGC mode is selected
     *
     * Since: 1.22
     */
    g_object_class_install_property (gobject_class, PROP_ADAPTER,
        g_param_spec_int ("adapter", "Adapter",
            "DXGI Adapter index for creating device when WGC mode is selected "
            "(-1 for default)",
            -1, G_MAXINT32, DEFAULT_ADAPTER,
            (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS)));

    /**
     * GstD3D11ScreenCaptureSrc:window-capture-mode:
     *
     * Window capture mode to use
     *
     * Since: 1.24
     */
    g_object_class_install_property (gobject_class, PROP_WINDOW_CAPTURE_MODE,
        g_param_spec_enum ("window-capture-mode", "Window Capture Mode",
            "Window capture mode to use if \"window-handle\" is set",
            GST_TYPE_D3D11_WINDOW_CAPTURE_MODE, DEFAULT_WINDOW_CAPTURE_MODE,
            (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS)));
    gst_type_mark_as_plugin_api (GST_TYPE_D3D11_WINDOW_CAPTURE_MODE,
        (GstPluginAPIFlags) 0);
  }
#endif

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_set_context);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Screen Capture Source", "Source/Video",
      "Captures desktop screen", "Seungha Yang <seungha@centricular.com>");

  caps = gst_d3d11_get_updated_template_caps (&template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  basesrc_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_get_caps);
  basesrc_class->fixate =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_fixate);
  basesrc_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_set_caps);
  basesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_decide_allocation);
  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_stop);
  basesrc_class->unlock =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_unlock);
  basesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_unlock_stop);
  basesrc_class->query =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_src_query);
  basesrc_class->create =
      GST_DEBUG_FUNCPTR (gst_d3d11_screen_capture_src_create);
}

static void
gst_d3d11_screen_capture_src_init (GstD3D11ScreenCaptureSrc * self)
{
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);

  self->monitor_index = DEFAULT_MONITOR_INDEX;
  self->show_cursor = DEFAULT_SHOW_CURSOR;
  self->show_border = DEFAULT_SHOW_BORDER;
  self->capture_api = DEFAULT_CAPTURE_API;
  self->hwnd_capture_mode = DEFAULT_WINDOW_CAPTURE_MODE;
  self->adapter = DEFAULT_ADAPTER;
  self->min_latency = GST_CLOCK_TIME_NONE;
  self->max_latency = GST_CLOCK_TIME_NONE;

  InitializeCriticalSection (&self->lock);
}

static void
gst_d3d11_screen_capture_src_dispose (GObject * object)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (object);

  gst_clear_object (&self->capture);
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_screen_capture_src_finalize (GObject * object)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (object);

  DeleteCriticalSection (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_screen_capture_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (object);
  GstD3D11CSLockGuard lk (&self->lock);

  switch (prop_id) {
    case PROP_MONITOR_INDEX:
      self->monitor_index = g_value_get_int (value);
      break;
    case PROP_MONITOR_HANDLE:
      self->monitor_handle = (HMONITOR) g_value_get_uint64 (value);
      break;
    case PROP_SHOW_CURSOR:
      self->show_cursor = g_value_get_boolean (value);
      break;
    case PROP_CROP_X:
      self->crop_x = g_value_get_uint (value);
      break;
    case PROP_CROP_Y:
      self->crop_y = g_value_get_uint (value);
      break;
    case PROP_CROP_WIDTH:
      self->crop_w = g_value_get_uint (value);
      break;
    case PROP_CROP_HEIGHT:
      self->crop_h = g_value_get_uint (value);
      break;
    case PROP_WINDOW_HANDLE:
      self->window_handle = (HWND) g_value_get_uint64 (value);
      break;
    case PROP_SHOW_BORDER:
      self->show_border = g_value_get_boolean (value);
      if (self->capture)
        gst_d3d11_screen_capture_show_border (self->capture, self->show_border);
      break;
    case PROP_CAPTURE_API:
      self->capture_api = (GstD3D11ScreenCaptureAPI) g_value_get_enum (value);
      break;
    case PROP_ADAPTER:
      self->adapter = g_value_get_int (value);
      break;
    case PROP_WINDOW_CAPTURE_MODE:
      self->hwnd_capture_mode =
          (GstD3D11WindowCaptureMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  };
}

static void
gst_d3d11_screen_capture_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (object);

  switch (prop_id) {
    case PROP_MONITOR_INDEX:
      g_value_set_int (value, self->monitor_index);
      break;
    case PROP_MONITOR_HANDLE:
      g_value_set_uint64 (value, (guint64) self->monitor_handle);
      break;
    case PROP_SHOW_CURSOR:
      g_value_set_boolean (value, self->show_cursor);
      break;
    case PROP_CROP_X:
      g_value_set_uint (value, self->crop_x);
      break;
    case PROP_CROP_Y:
      g_value_set_uint (value, self->crop_y);
      break;
    case PROP_CROP_WIDTH:
      g_value_set_uint (value, self->crop_w);
      break;
    case PROP_CROP_HEIGHT:
      g_value_set_uint (value, self->crop_h);
      break;
    case PROP_WINDOW_HANDLE:
      g_value_set_uint64 (value, (guint64) self->window_handle);
      break;
    case PROP_SHOW_BORDER:
      g_value_set_boolean (value, self->show_border);
      break;
    case PROP_CAPTURE_API:
      g_value_set_enum (value, self->capture_api);
      break;
    case PROP_ADAPTER:
      g_value_set_int (value, self->adapter);
      break;
    case PROP_WINDOW_CAPTURE_MODE:
      g_value_set_enum (value, self->hwnd_capture_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  };
}

static void
gst_d3d11_screen_capture_src_set_context (GstElement * element,
    GstContext * context)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (element);
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->capture_api == GST_D3D11_SCREEN_CAPTURE_API_DXGI) {
    gst_d3d11_handle_set_context_for_adapter_luid (element,
        context, self->adapter_luid, &self->device);
  } else {
    gst_d3d11_handle_set_context (element,
        context, self->adapter, &self->device);
  }

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static D3D11_BOX
gst_d3d11_screen_capture_src_get_crop_box (GstD3D11ScreenCaptureSrc * self)
{
  D3D11_BOX box;
  guint screen_width, screen_height;

  box.front = 0;
  box.back = 1;

  gst_d3d11_screen_capture_get_size (self->capture, &screen_width,
      &screen_height);

  if ((self->crop_x + self->crop_w) > screen_width ||
      (self->crop_y + self->crop_h) > screen_height) {
    GST_WARNING ("Capture region outside of the screen bounds; ignoring.");

    box.left = 0;
    box.top = 0;
    box.right = screen_width;
    box.bottom = screen_height;
  } else {
    box.left = self->crop_x;
    box.top = self->crop_y;
    box.right = self->crop_w ? (self->crop_x + self->crop_w) : screen_width;
    box.bottom = self->crop_h ? (self->crop_y + self->crop_h) : screen_height;
  }

  return box;
}

static GstCaps *
gst_d3d11_screen_capture_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  GstCaps *caps = NULL;
  guint width, height;

  if (!self->capture) {
    GST_DEBUG_OBJECT (self, "capture object is not configured yet");
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  }

  self->crop_box = gst_d3d11_screen_capture_src_get_crop_box (self);
  width = self->crop_box.right - self->crop_box.left;
  height = self->crop_box.bottom - self->crop_box.top;

  caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  caps = gst_caps_make_writable (caps);

  gst_caps_set_simple (caps, "width", G_TYPE_INT, width, "height",
      G_TYPE_INT, height, nullptr);

  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);

    caps = tmp;
  }

  return caps;
}

static GstCaps *
gst_d3d11_screen_capture_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  guint size;
  GstCaps *d3d11_caps = nullptr;

  caps = gst_caps_make_writable (caps);
  size = gst_caps_get_size (caps);

  for (guint i = 0; i < size; i++) {
    GstStructure *s;

    s = gst_caps_get_structure (caps, i);
    gst_structure_fixate_field_nearest_fraction (s, "framerate", 30, 1);

    if (!d3d11_caps) {
      GstCapsFeatures *features;
      features = gst_caps_get_features (caps, i);

      if (features && gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {

        d3d11_caps = gst_caps_new_empty ();
        gst_caps_append_structure (d3d11_caps, gst_structure_copy (s));

        gst_caps_set_features (d3d11_caps, 0,
            gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
                nullptr));

        break;
      }
    }
  }

  if (d3d11_caps) {
    gst_caps_unref (caps);
    caps = d3d11_caps;
  }

  return gst_caps_fixate (caps);
}

static gboolean
gst_d3d11_screen_capture_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  GstCapsFeatures *features;

  GST_DEBUG_OBJECT (self, "Set caps %" GST_PTR_FORMAT, caps);

  features = gst_caps_get_features (caps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    self->downstream_supports_d3d11 = TRUE;
  } else {
    self->downstream_supports_d3d11 = FALSE;
  }

  gst_video_info_from_caps (&self->video_info, caps);
  gst_base_src_set_blocksize (bsrc, GST_VIDEO_INFO_SIZE (&self->video_info));

  return TRUE;
}

static gboolean
gst_d3d11_screen_capture_src_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;
  GstVideoInfo vinfo;

  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_ERROR_OBJECT (self, "No output caps");
    return FALSE;
  }

  gst_video_info_from_caps (&vinfo, caps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    size = GST_VIDEO_INFO_SIZE (&vinfo);

    min = max = 0;
    update_pool = FALSE;
  }

  if (pool && self->downstream_supports_d3d11) {
    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != self->device)
        gst_clear_object (&pool);
    }
  }

  if (!pool) {
    if (self->downstream_supports_d3d11)
      pool = gst_d3d11_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (self->downstream_supports_d3d11) {
    d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
    if (!d3d11_params) {
      d3d11_params = gst_d3d11_allocation_params_new (self->device, &vinfo,
          GST_D3D11_ALLOCATION_FLAG_DEFAULT,
          D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);
    } else {
      gst_d3d11_allocation_params_set_bind_flags (d3d11_params,
          D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    }

    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set config");
    goto error;
  }

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (!self->downstream_supports_d3d11) {
    self->pool = gst_d3d11_buffer_pool_new (self->device);

    config = gst_buffer_pool_get_config (self->pool);

    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
    if (!d3d11_params) {
      d3d11_params = gst_d3d11_allocation_params_new (self->device, &vinfo,
          GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_RENDER_TARGET, 0);
    } else {
      gst_d3d11_allocation_params_set_bind_flags (d3d11_params,
          D3D11_BIND_RENDER_TARGET);
    }

    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);

    if (!gst_buffer_pool_set_config (self->pool, config)) {
      GST_ERROR_OBJECT (self, "Failed to set config for internal pool");
      goto error;
    }

    if (!gst_buffer_pool_set_active (self->pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to activate internal pool");
      goto error;
    }
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;

error:
  gst_clear_object (&self->pool);
  gst_clear_object (&pool);

  return FALSE;
}

static gboolean
gst_d3d11_screen_capture_prepare_shader (GstD3D11ScreenCaptureSrc * self)
{
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11SamplerState > sampler;
  ComPtr < ID3D11BlendState > blend;
  ComPtr < ID3D11RasterizerState > rs;
  D3D11_BLEND_DESC blend_desc;
  ID3D11Device *device_handle;
  HRESULT hr;
  ShaderResource *resource = &self->resource;

  device_handle = gst_d3d11_device_get_device_handle (self->device);

  hr = gst_d3d11_get_vertex_shader_coord (self->device, &vs, &layout);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create vertex shader");
    return FALSE;
  }

  hr = gst_d3d11_get_pixel_shader_sample (self->device, &ps);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create pixel shader");
    return FALSE;
  }

  hr = gst_d3d11_device_get_sampler (self->device,
      D3D11_FILTER_MIN_MAG_MIP_LINEAR, &sampler);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "Failed to create sampler state, hr 0x%x", (guint) hr);
    return FALSE;
  }

  blend_desc.AlphaToCoverageEnable = FALSE;
  blend_desc.IndependentBlendEnable = FALSE;
  blend_desc.RenderTarget[0].BlendEnable = TRUE;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  hr = device_handle->CreateBlendState (&blend_desc, &blend);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "Failed to create blend state, hr 0x%x", (guint) hr);
    return FALSE;
  }

  hr = gst_d3d11_device_get_rasterizer (self->device, &rs);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get rasterizer state");
    return FALSE;
  }

  resource->vs = vs.Detach ();
  resource->ps = ps.Detach ();
  resource->layout = layout.Detach ();
  resource->sampler = sampler.Detach ();
  resource->blend = blend.Detach ();
  resource->rs = rs.Detach ();

  return TRUE;
}

static gboolean
gst_d3d11_screen_capture_src_start (GstBaseSrc * bsrc)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  GstFlowReturn ret;
  HMONITOR monitor = self->monitor_handle;
  ComPtr < IDXGIAdapter1 > adapter;
  DXGI_ADAPTER_DESC desc;
  HRESULT hr;
  GstD3D11ScreenCapture *capture = nullptr;
  GstD3D11ScreenCaptureAPI capture_api = self->capture_api;

  EnterCriticalSection (&self->lock);
  if (self->window_handle) {
    self->capture_api = GST_D3D11_SCREEN_CAPTURE_API_WGC;
  } else {
    if (monitor) {
      hr = gst_d3d11_screen_capture_find_output_for_monitor (monitor,
          &adapter, nullptr);
    } else if (self->monitor_index < 0) {
      hr = gst_d3d11_screen_capture_find_primary_monitor (&monitor,
          &adapter, nullptr);
    } else {
      hr = gst_d3d11_screen_capture_find_nth_monitor (self->monitor_index,
          &monitor, &adapter, nullptr);
    }

    if (FAILED (hr))
      goto error;
  }

  if (self->capture_api == GST_D3D11_SCREEN_CAPTURE_API_DXGI) {
    hr = adapter->GetDesc (&desc);
    if (FAILED (hr))
      goto error;

    self->adapter_luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);
    gst_clear_object (&self->device);

    gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT_CAST (self),
        self->adapter_luid, &self->device);
  } else {
    gst_clear_object (&self->device);
    gst_d3d11_ensure_element_data (GST_ELEMENT_CAST (self),
        self->adapter, &self->device);
  }

  if (!self->device)
    goto no_device;

#ifdef HAVE_WINRT_CAPTURE
  if (self->window_handle) {
    capture = gst_d3d11_winrt_capture_new (self->device, nullptr,
        self->window_handle,
        self->hwnd_capture_mode == GST_D3D11_WINDOW_CAPTURE_CLIENT);
  } else if (self->capture_api == GST_D3D11_SCREEN_CAPTURE_API_WGC) {
    capture = gst_d3d11_winrt_capture_new (self->device,
        monitor, nullptr, FALSE);
  }
#endif

  if (!capture)
    capture = gst_d3d11_dxgi_capture_new (self->device, monitor);

  if (!capture)
    goto error;

  /* Check if we can open device */
  ret = gst_d3d11_screen_capture_prepare (capture);
  switch (ret) {
    case GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR:
    case GST_FLOW_OK:
      break;
    case GST_D3D11_SCREEN_CAPTURE_FLOW_UNSUPPORTED:
#ifdef HAVE_WINRT_CAPTURE
      /* Try WinRT capture if DXGI capture does not work */
      if (self->capture_api == GST_D3D11_SCREEN_CAPTURE_API_DXGI) {
        self->capture_api = GST_D3D11_SCREEN_CAPTURE_API_WGC;
        gst_clear_object (&capture);
        GST_WARNING_OBJECT (self, "DXGI capture is not available");
        capture = gst_d3d11_winrt_capture_new (self->device,
            monitor, nullptr, FALSE);
        if (capture
            && gst_d3d11_screen_capture_prepare (capture) == GST_FLOW_OK) {
          GST_INFO_OBJECT (self, "Fallback to Windows Graphics Capture");
          break;
        }
      }
#endif
      goto unsupported;
    default:
      goto error;
  }

  if (self->capture_api == GST_D3D11_SCREEN_CAPTURE_API_DXGI &&
      !gst_d3d11_screen_capture_prepare_shader (self)) {
    goto error;
  }

  self->last_frame_no = -1;
  self->min_latency = self->max_latency = GST_CLOCK_TIME_NONE;

  gst_d3d11_screen_capture_show_border (capture, self->show_border);
  self->capture = capture;

  LeaveCriticalSection (&self->lock);
  if (self->capture_api != capture_api) {
    GST_INFO_OBJECT (self, "Updated capture api: %d", self->capture_api);
    g_object_notify (G_OBJECT (self), "capture-api");
  }

  return TRUE;

no_device:
  {
    LeaveCriticalSection (&self->lock);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("D3D11 device is not available"), (nullptr));
    return FALSE;
  }

error:
  {
    gst_clear_object (&capture);
    LeaveCriticalSection (&self->lock);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Failed to prepare capture object with given configuration, "
            "monitor-index: %d, monitor-handle: %p, window-handle: %p",
            self->monitor_index, self->monitor_handle, self->window_handle),
        (nullptr));
    return FALSE;
  }

unsupported:
  {
    gst_clear_object (&capture);
    LeaveCriticalSection (&self->lock);
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Failed to prepare capture object with given configuration, "
            "monitor-index: %d, monitor-handle: %p",
            self->monitor_index, self->monitor_handle),
        ("Try run the application on the integrated GPU"));
    return FALSE;
  }
}

static gboolean
gst_d3d11_screen_capture_src_stop (GstBaseSrc * bsrc)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  ShaderResource *resource = &self->resource;
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  GST_D3D11_CLEAR_COM (resource->vs);
  GST_D3D11_CLEAR_COM (resource->ps);
  GST_D3D11_CLEAR_COM (resource->layout);
  GST_D3D11_CLEAR_COM (resource->sampler);
  GST_D3D11_CLEAR_COM (resource->blend);
  GST_D3D11_CLEAR_COM (resource->rs);

  gst_clear_object (&self->capture);
  gst_clear_object (&self->device);

  return TRUE;
}

static gboolean
gst_d3d11_screen_capture_src_unlock (GstBaseSrc * bsrc)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);

  GST_OBJECT_LOCK (self);
  if (self->capture)
    gst_d3d11_screen_capture_unlock (self->capture);

  if (self->clock_id) {
    GST_DEBUG_OBJECT (self, "Waking up waiting clock");
    gst_clock_id_unschedule (self->clock_id);
  }
  self->flushing = TRUE;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_d3d11_screen_capture_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);

  GST_OBJECT_LOCK (self);
  if (self->capture)
    gst_d3d11_screen_capture_unlock_stop (self->capture);

  self->flushing = FALSE;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_d3d11_screen_capture_src_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT_CAST (self), query,
              self->device)) {
        return TRUE;
      }
      break;
    case GST_QUERY_LATENCY:
      if (GST_CLOCK_TIME_IS_VALID (self->min_latency)) {
        gst_query_set_latency (query,
            TRUE, self->min_latency, self->max_latency);
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
}

static GstFlowReturn
gst_d3d11_screen_capture_src_create (GstBaseSrc * bsrc, guint64 offset,
    guint size, GstBuffer ** buf)
{
  GstD3D11ScreenCaptureSrc *self = GST_D3D11_SCREEN_CAPTURE_SRC (bsrc);
  ID3D11Texture2D *texture;
  ID3D11RenderTargetView *rtv = NULL;
  gint fps_n, fps_d;
  GstMapInfo info;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  GstFlowReturn ret = GST_FLOW_OK;
  GstClock *clock = NULL;
  GstClockTime base_time;
  GstClockTime next_capture_ts;
  GstClockTime before_capture;
  GstClockTime after_capture;
  GstClockTime latency;
  GstClockTime dur;
  gboolean update_latency = FALSE;
  guint64 next_frame_no;
  gboolean draw_mouse;
  /* Just magic number... */
  gint unsupported_retry_count = 100;
  GstBuffer *buffer = NULL;
  GstBuffer *sysmem_buf = NULL;
  D3D11_BOX crop_box;

  if (!self->capture) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Couldn't configure capture object"), (nullptr));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  fps_n = GST_VIDEO_INFO_FPS_N (&self->video_info);
  fps_d = GST_VIDEO_INFO_FPS_D (&self->video_info);

  if (fps_n <= 0 || fps_d <= 0)
    return GST_FLOW_NOT_NEGOTIATED;

  crop_box = gst_d3d11_screen_capture_src_get_crop_box (self);
  if (crop_box.left != self->crop_box.left ||
      crop_box.right != self->crop_box.right ||
      crop_box.top != self->crop_box.top ||
      crop_box.bottom != self->crop_box.bottom) {
    GST_INFO_OBJECT (self, "Capture area changed, need negotiation");
    if (!gst_base_src_negotiate (bsrc)) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with new capture area");
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

again:
  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  if (!clock) {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
        ("Cannot operate without a clock"), (nullptr));
    return GST_FLOW_ERROR;
  }

  /* Check flushing before waiting clock because we are might be doing
   * retry */
  GST_OBJECT_LOCK (self);
  if (self->flushing) {
    ret = GST_FLOW_FLUSHING;
    GST_OBJECT_UNLOCK (self);
    goto out;
  }

  base_time = GST_ELEMENT_CAST (self)->base_time;
  next_capture_ts = gst_clock_get_time (clock);
  next_capture_ts -= base_time;

  next_frame_no = gst_util_uint64_scale (next_capture_ts,
      fps_n, GST_SECOND * fps_d);

  if (next_frame_no == self->last_frame_no) {
    GstClockID id;
    GstClockReturn clock_ret;

    /* Need to wait for the next frame */
    next_frame_no += 1;

    /* Figure out what the next frame time is */
    next_capture_ts = gst_util_uint64_scale (next_frame_no,
        fps_d * GST_SECOND, fps_n);

    id = gst_clock_new_single_shot_id (GST_ELEMENT_CLOCK (self),
        next_capture_ts + base_time);
    self->clock_id = id;

    /* release the object lock while waiting */
    GST_OBJECT_UNLOCK (self);

    GST_LOG_OBJECT (self, "Waiting for next frame time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (next_capture_ts));
    clock_ret = gst_clock_id_wait (id, NULL);
    GST_OBJECT_LOCK (self);

    gst_clock_id_unref (id);
    self->clock_id = NULL;

    if (clock_ret == GST_CLOCK_UNSCHEDULED) {
      /* Got woken up by the unlock function */
      ret = GST_FLOW_FLUSHING;
      GST_OBJECT_UNLOCK (self);
      goto out;
    }
    /* Duration is a complete 1/fps frame duration */
    dur = gst_util_uint64_scale_int (GST_SECOND, fps_d, fps_n);
  } else {
    GstClockTime next_frame_ts;

    GST_LOG_OBJECT (self, "No need to wait for next frame time %"
        GST_TIME_FORMAT " next frame = %" G_GINT64_FORMAT " prev = %"
        G_GINT64_FORMAT, GST_TIME_ARGS (next_capture_ts), next_frame_no,
        self->last_frame_no);

    next_frame_ts = gst_util_uint64_scale (next_frame_no + 1,
        fps_d * GST_SECOND, fps_n);
    /* Frame duration is from now until the next expected capture time */
    dur = next_frame_ts - next_capture_ts;
  }

  self->last_frame_no = next_frame_no;
  GST_OBJECT_UNLOCK (self);

  if (!buffer) {
    if (self->downstream_supports_d3d11) {
      ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc,
          offset, size, &buffer);
    } else {
      if (!self->pool) {
        GST_ERROR_OBJECT (self, "Internal pool wasn't configured");
        goto error;
      }

      ret = gst_buffer_pool_acquire_buffer (self->pool, &buffer, nullptr);
    }

    if (ret != GST_FLOW_OK)
      goto out;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_ERROR_OBJECT (self, "Not a D3D11 memory");
    goto error;
  }

  dmem = (GstD3D11Memory *) mem;
  draw_mouse = self->show_cursor;
  rtv = gst_d3d11_memory_get_render_target_view (dmem, 0);
  if (draw_mouse && !rtv) {
    GST_ERROR_OBJECT (self, "Render target view is unavailable");
    goto error;
  }

  if (!gst_memory_map (mem, &info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map d3d11 memory");
    goto error;
  }

  texture = (ID3D11Texture2D *) info.data;
  before_capture = gst_clock_get_time (clock);
  ret = gst_d3d11_screen_capture_do_capture (self->capture, self->device,
      texture, rtv, &self->resource, &self->crop_box, draw_mouse);
  gst_memory_unmap (mem, &info);

  switch (ret) {
    case GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR:
      GST_WARNING_OBJECT (self, "Got expected error, try again");
      gst_clear_object (&clock);
      goto again;
    case GST_D3D11_SCREEN_CAPTURE_FLOW_UNSUPPORTED:
      GST_WARNING_OBJECT (self, "Got DXGI_ERROR_UNSUPPORTED error");
      unsupported_retry_count--;

      if (unsupported_retry_count < 0)
        goto error;

      gst_clear_object (&clock);
      goto again;
    case GST_D3D11_SCREEN_CAPTURE_FLOW_SIZE_CHANGED:
      GST_INFO_OBJECT (self, "Size was changed, need negotiation");
      gst_clear_buffer (&buffer);
      gst_clear_object (&clock);

      if (!gst_base_src_negotiate (bsrc)) {
        GST_ERROR_OBJECT (self, "Failed to negotiate with new size");
        ret = GST_FLOW_NOT_NEGOTIATED;
        goto out;
      }
      goto again;
    default:
      break;
  }

  if (ret != GST_FLOW_OK)
    goto out;

  if (!self->downstream_supports_d3d11) {
    ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc,
        offset, size, &sysmem_buf);
    if (ret != GST_FLOW_OK) {
      gst_clear_buffer (&buffer);
      goto out;
    }

    if (!gst_d3d11_buffer_copy_into (sysmem_buf, buffer, &self->video_info)) {
      GST_ERROR_OBJECT (self, "Failed to copy frame");
      goto error;
    }

    gst_buffer_unref (buffer);
    buffer = sysmem_buf;
    sysmem_buf = nullptr;
  }

  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_PTS (buffer) = next_capture_ts;
  GST_BUFFER_DURATION (buffer) = dur;

  after_capture = gst_clock_get_time (clock);
  latency = after_capture - before_capture;
  if (!GST_CLOCK_TIME_IS_VALID (self->min_latency)) {
    self->min_latency = self->max_latency = latency;
    update_latency = TRUE;
    GST_DEBUG_OBJECT (self, "Initial latency %" GST_TIME_FORMAT,
        GST_TIME_ARGS (latency));
  }

  if (latency > self->max_latency) {
    self->max_latency = latency;
    update_latency = TRUE;
    GST_DEBUG_OBJECT (self, "Updating max latency %" GST_TIME_FORMAT,
        GST_TIME_ARGS (latency));
  }

  if (update_latency) {
    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_latency (GST_OBJECT_CAST (self)));
  }

out:
  gst_clear_object (&clock);
  *buf = buffer;

  return ret;

error:
  gst_clear_buffer (&buffer);
  gst_clear_buffer (&sysmem_buf);
  gst_clear_object (&clock);

  return GST_FLOW_ERROR;
}
