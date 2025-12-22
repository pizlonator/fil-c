/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
 * SECTION:element-d3d11videosink
 * @title: d3d11videosink
 *
 * Direct3D11 based video render element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! d3d11upload ! d3d11videosink
 * ```
 * This pipeline will display test video stream on screen via d3d11videosink
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11videosink.h"
#include "gstd3d11pluginutils.h"
#include <string>

#if GST_D3D11_WINAPI_APP
#include "gstd3d11window_corewindow.h"
#include "gstd3d11window_swapchainpanel.h"
#endif
#if (!GST_D3D11_WINAPI_ONLY_APP)
#include "gstd3d11window_win32.h"
#endif
#include "gstd3d11window_dummy.h"

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_FORCE_ASPECT_RATIO,
  PROP_ENABLE_NAVIGATION_EVENTS,
  PROP_FULLSCREEN_TOGGLE_MODE,
  PROP_FULLSCREEN,
  PROP_DRAW_ON_SHARED_TEXTURE,
  PROP_ROTATE_METHOD,
  PROP_GAMMA_MODE,
  PROP_PRIMARIES_MODE,
  PROP_DISPLAY_FORMAT,
  PROP_EMIT_PRESENT,
  PROP_FOV,
  PROP_ORTHO,
  PROP_ROTATION_X,
  PROP_ROTATION_Y,
  PROP_ROTATION_Z,
  PROP_SCALE_X,
  PROP_SCALE_Y,
  PROP_MSAA,
  PROP_SAMPLING_METHOD,
  PROP_REDRAW_ON_UPDATE,
  PROP_RENDER_RECTANGE,
};

#define DEFAULT_ADAPTER                   -1
#define DEFAULT_FORCE_ASPECT_RATIO        TRUE
#define DEFAULT_ENABLE_NAVIGATION_EVENTS  TRUE
#define DEFAULT_FULLSCREEN_TOGGLE_MODE    GST_D3D11_WINDOW_FULLSCREEN_TOGGLE_MODE_NONE
#define DEFAULT_FULLSCREEN                FALSE
#define DEFAULT_DRAW_ON_SHARED_TEXTURE    FALSE
#define DEFAULT_GAMMA_MODE                GST_VIDEO_GAMMA_MODE_NONE
#define DEFAULT_PRIMARIES_MODE            GST_VIDEO_PRIMARIES_MODE_NONE
#define DEFAULT_DISPLAY_FORMAT            DXGI_FORMAT_UNKNOWN
#define DEFAULT_EMIT_PRESENT              FALSE
#define DEFAULT_ROTATION                  0.0f
#define DEFAULT_SCALE                     1.0f
#define DEFAULT_FOV                       90.0f
#define DEFAULT_ORTHO                     FALSE
#define DEFAULT_MSAA                      GST_D3D11_MSAA_DISABLED
#define DEFAULT_SAMPLING_METHOD           GST_D3D11_SAMPLING_METHOD_BILINEAR
#define DEFAULT_REDROW_ON_UPDATE          TRUE

/**
 * GstD3D11VideoSinkDisplayFormat:
 *
 * Swapchain's DXGI format
 *
 * Since: 1.22
 */
#define GST_TYPE_D3D11_VIDEO_SINK_DISPLAY_FORMAT (gst_d3d11_video_sink_display_format_type())
static GType
gst_d3d11_video_sink_display_format_type (void)
{
  static GType format_type = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    static const GEnumValue format_types[] = {
      /**
       * GstD3D11VideoSinkDisplayFormat::unknown:
       *
       * Since: 1.22
       */
      {DXGI_FORMAT_UNKNOWN, "DXGI_FORMAT_UNKNOWN", "unknown"},

      /**
       * GstD3D11VideoSinkDisplayFormat::r10g10b10a2-unorm:
       *
       * Since: 1.22
       */
      {DXGI_FORMAT_R10G10B10A2_UNORM,
          "DXGI_FORMAT_R10G10B10A2_UNORM", "r10g10b10a2-unorm"},

      /**
       * GstD3D11VideoSinkDisplayFormat::r8g8b8a8-unorm:
       *
       * Since: 1.22
       */
      {DXGI_FORMAT_R8G8B8A8_UNORM,
          "DXGI_FORMAT_R8G8B8A8_UNORM", "r8g8b8a8-unorm"},

      /**
       * GstD3D11VideoSinkDisplayFormat::b8g8r8a8-unorm:
       *
       * Since: 1.22
       */
      {DXGI_FORMAT_B8G8R8A8_UNORM,
          "DXGI_FORMAT_B8G8R8A8_UNORM", "b8g8r8a8-unorm"},
      {0, nullptr, nullptr},
    };

    format_type = g_enum_register_static ("GstD3D11VideoSinkDisplayFormat",
        format_types);
  } GST_D3D11_CALL_ONCE_END;

  return format_type;
}

enum
{
  /* signals */
  SIGNAL_BEGIN_DRAW,
  SIGNAL_PRESENT,

  /* actions */
  SIGNAL_DRAW,

  LAST_SIGNAL
};

static guint gst_d3d11_video_sink_signals[LAST_SIGNAL] = { 0, };

static GstStaticCaps pad_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SINK_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_SINK_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE (GST_D3D11_SINK_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_SINK_FORMATS));

GST_DEBUG_CATEGORY (d3d11_video_sink_debug);
#define GST_CAT_DEFAULT d3d11_video_sink_debug

struct _GstD3D11VideoSink
{
  GstVideoSink parent;
  GstD3D11Device *device;
  GstD3D11Window *window;
  gint video_width;
  gint video_height;
  GstVideoInfo info;
  guintptr window_id;
  gboolean caps_updated;
  GstBuffer *prepared_buffer;
  GstBufferPool *pool;

  /* properties */
  gint adapter;
  gboolean force_aspect_ratio;
  gboolean enable_navigation_events;
  GstD3D11WindowFullscreenToggleMode fullscreen_toggle_mode;
  gboolean fullscreen;
  gboolean draw_on_shared_texture;
  GstVideoGammaMode gamma_mode;
  GstVideoPrimariesMode primaries_mode;
  DXGI_FORMAT display_format;
  gboolean emit_present;
  gfloat fov;
  gboolean ortho;
  gfloat rotation_x;
  gfloat rotation_y;
  gfloat rotation_z;
  gfloat scale_x;
  gfloat scale_y;
  GstD3D11MSAAMode msaa;
  GstD3D11SamplingMethod sampling_method;
  gboolean redraw_on_update;

  /* saved render rectangle until we have a window */
  GstVideoRectangle render_rect;
  gboolean pending_render_rect;

  /* For drawing on user texture */
  gboolean drawing;
  CRITICAL_SECTION lock;

  gchar *title;

  /* method configured via property */
  GstVideoOrientationMethod method;
  /* method parsed from tag */
  GstVideoOrientationMethod tag_method;
  /* method currently selected based on "method" and "tag_method" */
  GstVideoOrientationMethod selected_method;
};

static void gst_d3d11_videosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_videosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d11_video_sink_finalize (GObject * object);
static gboolean
gst_d3d11_video_sink_draw_action (GstD3D11VideoSink * self,
    gpointer shared_handle, guint texture_misc_flags, guint64 acquire_key,
    guint64 release_key);

static void
gst_d3d11_video_sink_video_overlay_init (GstVideoOverlayInterface * iface);
static void
gst_d3d11_video_sink_navigation_init (GstNavigationInterface * iface);

static void gst_d3d11_video_sink_set_context (GstElement * element,
    GstContext * context);
static GstCaps *gst_d3d11_video_sink_get_caps (GstBaseSink * sink,
    GstCaps * filter);
static gboolean gst_d3d11_video_sink_set_caps (GstBaseSink * sink,
    GstCaps * caps);

static gboolean gst_d3d11_video_sink_start (GstBaseSink * sink);
static gboolean gst_d3d11_video_sink_stop (GstBaseSink * sink);
static gboolean gst_d3d11_video_sink_propose_allocation (GstBaseSink * sink,
    GstQuery * query);
static gboolean gst_d3d11_video_sink_query (GstBaseSink * sink,
    GstQuery * query);
static gboolean gst_d3d11_video_sink_unlock (GstBaseSink * sink);
static gboolean gst_d3d11_video_sink_unlock_stop (GstBaseSink * sink);
static gboolean gst_d3d11_video_sink_event (GstBaseSink * sink,
    GstEvent * event);
static GstFlowReturn gst_d3d11_video_sink_prepare (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn
gst_d3d11_video_sink_show_frame (GstVideoSink * sink, GstBuffer * buf);
static gboolean gst_d3d11_video_sink_prepare_window (GstD3D11VideoSink * self);
static void gst_d3d11_video_sink_set_orientation (GstD3D11VideoSink * self,
    GstVideoOrientationMethod method, gboolean from_tag);

#define gst_d3d11_video_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstD3D11VideoSink, gst_d3d11_video_sink,
    GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_d3d11_video_sink_video_overlay_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_d3d11_video_sink_navigation_init);
    GST_DEBUG_CATEGORY_INIT (d3d11_video_sink_debug,
        "d3d11videosink", 0, "Direct3D11 Video Sink"));

static void
gst_d3d11_video_sink_class_init (GstD3D11VideoSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS (klass);
  GstCaps *caps;

  gobject_class->set_property = gst_d3d11_videosink_set_property;
  gobject_class->get_property = gst_d3d11_videosink_get_property;
  gobject_class->finalize = gst_d3d11_video_sink_finalize;

  g_object_class_install_property (gobject_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "Adapter index for creating device (-1 for default)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ENABLE_NAVIGATION_EVENTS,
      g_param_spec_boolean ("enable-navigation-events",
          "Enable navigation events",
          "When enabled, navigation events are sent upstream",
          DEFAULT_ENABLE_NAVIGATION_EVENTS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FULLSCREEN_TOGGLE_MODE,
      g_param_spec_flags ("fullscreen-toggle-mode",
          "Full screen toggle mode",
          "Full screen toggle mode used to trigger fullscreen mode change",
          GST_D3D11_WINDOW_TOGGLE_MODE_GET_TYPE, DEFAULT_FULLSCREEN_TOGGLE_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FULLSCREEN,
      g_param_spec_boolean ("fullscreen",
          "fullscreen",
          "Ignored when \"fullscreen-toggle-mode\" does not include \"property\"",
          DEFAULT_FULLSCREEN,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:draw-on-shared-texture:
   *
   * Instruct the sink to draw on a shared texture provided by user.
   * User must watch #d3d11videosink::begin-draw signal and should call
   * #d3d11videosink::draw method on the #d3d11videosink::begin-draw
   * signal handler.
   *
   * Currently supported formats for user texture are:
   * - DXGI_FORMAT_R8G8B8A8_UNORM
   * - DXGI_FORMAT_B8G8R8A8_UNORM
   * - DXGI_FORMAT_R10G10B10A2_UNORM
   *
   * Since: 1.20
   *
   * Deprecated, Use appsink to access GStreamer produced D3D11 texture
   */
  g_object_class_install_property (gobject_class, PROP_DRAW_ON_SHARED_TEXTURE,
      g_param_spec_boolean ("draw-on-shared-texture",
          "Draw on shared texture",
          "Draw on user provided shared texture instead of window. "
          "When enabled, user can pass application's own texture to sink "
          "by using \"draw\" action signal on \"begin-draw\" signal handler, "
          "so that sink can draw video data on application's texture. "
          "Supported texture formats for user texture are "
          "DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, and "
          "DXGI_FORMAT_R10G10B10A2_UNORM.",
          DEFAULT_DRAW_ON_SHARED_TEXTURE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:rotate-method:
   *
   * Video rotation/flip method to use
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_ROTATE_METHOD,
      g_param_spec_enum ("rotate-method", "Rotate Method",
          "Rotate method to use",
          GST_TYPE_VIDEO_ORIENTATION_METHOD, GST_VIDEO_ORIENTATION_IDENTITY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:gamma-mode:
   *
   * Gamma conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_GAMMA_MODE,
      g_param_spec_enum ("gamma-mode", "Gamma mode",
          "Gamma conversion mode", GST_TYPE_VIDEO_GAMMA_MODE,
          DEFAULT_GAMMA_MODE, (GParamFlags) (GST_PARAM_MUTABLE_READY |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:primaries-mode:
   *
   * Primaries conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PRIMARIES_MODE,
      g_param_spec_enum ("primaries-mode", "Primaries Mode",
          "Primaries conversion mode", GST_TYPE_VIDEO_PRIMARIES_MODE,
          DEFAULT_PRIMARIES_MODE, (GParamFlags) (GST_PARAM_MUTABLE_READY |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:display-format:
   *
   * Swapchain display format
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_DISPLAY_FORMAT,
      g_param_spec_enum ("display-format", "Display Format",
          "Swapchain display format", GST_TYPE_D3D11_VIDEO_SINK_DISPLAY_FORMAT,
          DEFAULT_DISPLAY_FORMAT, (GParamFlags) (G_PARAM_READWRITE |
              GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:emit-present:
   *
   * Emits "present" signal
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_EMIT_PRESENT,
      g_param_spec_boolean ("emit-present", "Emit present",
          "Emits present signal", DEFAULT_EMIT_PRESENT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:fov:
   *
   * Field of view angle in degrees
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_FOV,
      g_param_spec_float ("fov", "Fov",
          "Field of view angle in degrees",
          0, G_MAXFLOAT, DEFAULT_FOV,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:ortho:
   *
   * Use orthographic projection
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ORTHO,
      g_param_spec_boolean ("ortho", "Orthographic",
          "Use orthographic projection", DEFAULT_ORTHO,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:rotation-x:
   *
   * x-axis rotation angle to be applied prior to "rotate-method"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ROTATION_X,
      g_param_spec_float ("rotation-x", "Rotation X",
          "x-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:rotation-y:
   *
   * y-axis rotation angle to be applied prior to "rotate-method"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ROTATION_Y,
      g_param_spec_float ("rotation-y", "Rotation Y",
          "y-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:rotation-z:
   *
   * z-axis rotation angle to be applied prior to "rotate-method"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ROTATION_Z,
      g_param_spec_float ("rotation-z", "Rotation Z",
          "z-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:scale-x:
   *
   * Scale multiplier for x-axis
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_SCALE_X,
      g_param_spec_float ("scale-x", "Scale X",
          "Scale multiplier for x-axis",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_SCALE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:scale-y:
   *
   * Scale multiplier for y-axis
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_SCALE_Y,
      g_param_spec_float ("scale-y", "Scale Y",
          "Scale multiplier for y-axis",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_SCALE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:msaa:
   *
   * MSAA (Multi-Sampling Anti-Aliasing) level
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MSAA,
      g_param_spec_enum ("msaa", "MSAA",
          "MSAA (Multi-Sampling Anti-Aliasing) level",
          GST_TYPE_D3D11_MSAA_MODE, DEFAULT_MSAA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:sampling-method:
   *
   * Sampling method
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_SAMPLING_METHOD,
      g_param_spec_enum ("sampling-method", "Sampling method",
          "Sampler filter type to use", GST_TYPE_D3D11_SAMPLING_METHOD,
          DEFAULT_SAMPLING_METHOD, (GParamFlags) (GST_PARAM_MUTABLE_READY |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:redraw-on-update:
   *
   * Immediately apply updated geometry related properties and redraw
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_REDRAW_ON_UPDATE,
      g_param_spec_boolean ("redraw-on-update",
          "redraw-on-update",
          "Immediately apply updated geometry related properties and redraw. "
          "If disabled, properties will be applied on the next frame or "
          "window resize", DEFAULT_REDROW_ON_UPDATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11VideoSink:render-rectangle:
   *
   * Since: 1.24
   */
  gst_video_overlay_install_properties (gobject_class, PROP_RENDER_RECTANGE);

  /**
   * GstD3D11VideoSink::begin-draw:
   * @videosink: the #d3d11videosink
   *
   * Emitted when sink has a texture to draw. Application needs to invoke
   * #d3d11videosink::draw action signal before returning from
   * #d3d11videosink::begin-draw signal handler.
   *
   * Since: 1.20
   *
   * Deprecated, Use appsink to access GStreamer produced D3D11 texture
   */
  gst_d3d11_video_sink_signals[SIGNAL_BEGIN_DRAW] =
      g_signal_new ("begin-draw", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstD3D11VideoSinkClass, begin_draw),
      NULL, NULL, NULL, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstD3D11VideoSink::draw:
   * @videosink: the #d3d11videosink
   * @shard_handle: a pointer to HANDLE
   * @texture_misc_flags: a D3D11_RESOURCE_MISC_FLAG value
   * @acquire_key: a key value used for IDXGIKeyedMutex::AcquireSync
   * @release_key: a key value used for IDXGIKeyedMutex::ReleaseSync
   *
   * Draws on a shared texture. @shard_handle must be a valid pointer to
   * a HANDLE which was obtained via IDXGIResource::GetSharedHandle or
   * IDXGIResource1::CreateSharedHandle.
   *
   * If the texture was created with D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX flag,
   * caller must specify valid @acquire_key and @release_key.
   * Otherwise (i.e., created with D3D11_RESOURCE_MISC_SHARED flag),
   * @acquire_key and @release_key will be ignored.
   *
   * Since: 1.20
   *
   * As of 1.24, @acquire_key and @release_key must be zero. Other values are
   * not supported.
   *
   * Deprecated, Use appsink to access GStreamer produced D3D11 texture
   */
  gst_d3d11_video_sink_signals[SIGNAL_DRAW] =
      g_signal_new ("draw", G_TYPE_FROM_CLASS (klass),
      (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
      G_STRUCT_OFFSET (GstD3D11VideoSinkClass, draw), NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT64,
      G_TYPE_UINT64);

  /**
   * GstD3D11VideoSink::present
   * @videosink: the #GstD3D11VideoSink
   * @device: a GstD3D11Device object
   * @render_target: a ID3D11RenderTargetView handle of swapchain's backbuffer
   *
   * Emitted just before presenting a texture via the IDXGISwapChain::Present.
   * The client can perform additional rendering on the given @render_target,
   * or can read the content already rendered on the swapchain's backbuffer.
   *
   * This signal will be emitted with gst_d3d11_device_lock taken and
   * client should perform GPU operation from the thread where this signal
   * emitted.
   *
   * Since: 1.22
   */
  gst_d3d11_video_sink_signals[SIGNAL_PRESENT] =
      g_signal_new ("present", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
      G_TYPE_NONE, 2, GST_TYPE_OBJECT, G_TYPE_POINTER);

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_set_context);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Video Sink", "Sink/Video",
      "A Direct3D11 based videosink",
      "Seungha Yang <seungha.yang@navercorp.com>");

  caps = gst_d3d11_get_updated_template_caps (&pad_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_get_caps);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_set_caps);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_stop);
  basesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_propose_allocation);
  basesink_class->query = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_query);
  basesink_class->unlock = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_unlock);
  basesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_unlock_stop);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_event);
  basesink_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_prepare);

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_d3d11_video_sink_show_frame);

  klass->draw = gst_d3d11_video_sink_draw_action;

  gst_type_mark_as_plugin_api (GST_D3D11_WINDOW_TOGGLE_MODE_GET_TYPE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_VIDEO_SINK_DISPLAY_FORMAT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_MSAA_MODE, (GstPluginAPIFlags) 0);
}

static void
gst_d3d11_video_sink_init (GstD3D11VideoSink * self)
{
  self->adapter = DEFAULT_ADAPTER;
  self->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  self->enable_navigation_events = DEFAULT_ENABLE_NAVIGATION_EVENTS;
  self->fullscreen_toggle_mode = DEFAULT_FULLSCREEN_TOGGLE_MODE;
  self->fullscreen = DEFAULT_FULLSCREEN;
  self->draw_on_shared_texture = DEFAULT_DRAW_ON_SHARED_TEXTURE;
  self->gamma_mode = DEFAULT_GAMMA_MODE;
  self->primaries_mode = DEFAULT_PRIMARIES_MODE;
  self->display_format = DEFAULT_DISPLAY_FORMAT;
  self->emit_present = DEFAULT_EMIT_PRESENT;
  self->fov = DEFAULT_FOV;
  self->ortho = DEFAULT_ORTHO;
  self->rotation_x = DEFAULT_ROTATION;
  self->rotation_y = DEFAULT_ROTATION;
  self->rotation_z = DEFAULT_ROTATION;
  self->scale_x = DEFAULT_SCALE;
  self->scale_y = DEFAULT_SCALE;
  self->msaa = DEFAULT_MSAA;
  self->sampling_method = DEFAULT_SAMPLING_METHOD;
  self->redraw_on_update = DEFAULT_REDROW_ON_UPDATE;

  InitializeCriticalSection (&self->lock);
}

static void
gst_d3d11_videosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (object);
  GstD3D11CSLockGuard lk (&self->lock);

  switch (prop_id) {
    case PROP_ADAPTER:
      self->adapter = g_value_get_int (value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      self->force_aspect_ratio = g_value_get_boolean (value);
      if (self->window)
        g_object_set (self->window,
            "force-aspect-ratio", self->force_aspect_ratio, NULL);
      break;
    case PROP_ENABLE_NAVIGATION_EVENTS:
      self->enable_navigation_events = g_value_get_boolean (value);
      if (self->window) {
        g_object_set (self->window,
            "enable-navigation-events", self->enable_navigation_events, NULL);
      }
      break;
    case PROP_FULLSCREEN_TOGGLE_MODE:
      self->fullscreen_toggle_mode =
          (GstD3D11WindowFullscreenToggleMode) g_value_get_flags (value);
      if (self->window) {
        g_object_set (self->window,
            "fullscreen-toggle-mode", self->fullscreen_toggle_mode, NULL);
      }
      break;
    case PROP_FULLSCREEN:
      self->fullscreen = g_value_get_boolean (value);
      if (self->window) {
        g_object_set (self->window, "fullscreen", self->fullscreen, NULL);
      }
      break;
    case PROP_DRAW_ON_SHARED_TEXTURE:
      self->draw_on_shared_texture = g_value_get_boolean (value);
      break;
    case PROP_ROTATE_METHOD:
      gst_d3d11_video_sink_set_orientation (self,
          (GstVideoOrientationMethod) g_value_get_enum (value), FALSE);
      break;
    case PROP_GAMMA_MODE:
      self->gamma_mode = (GstVideoGammaMode) g_value_get_enum (value);
      break;
    case PROP_PRIMARIES_MODE:
      self->primaries_mode = (GstVideoPrimariesMode) g_value_get_enum (value);
      break;
    case PROP_DISPLAY_FORMAT:
      self->display_format = (DXGI_FORMAT) g_value_get_enum (value);
      break;
    case PROP_EMIT_PRESENT:
      self->emit_present = g_value_get_boolean (value);
      break;
    case PROP_FOV:
      self->fov = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_ORTHO:
      self->ortho = g_value_get_boolean (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_ROTATION_X:
      self->rotation_x = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_ROTATION_Y:
      self->rotation_y = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_ROTATION_Z:
      self->rotation_z = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_SCALE_X:
      self->scale_x = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_SCALE_Y:
      self->scale_y = g_value_get_float (value);
      gst_d3d11_video_sink_set_orientation (self, self->method, FALSE);
      break;
    case PROP_MSAA:
      self->msaa = (GstD3D11MSAAMode) g_value_get_enum (value);
      if (self->window)
        gst_d3d11_window_set_msaa_mode (self->window, self->msaa);
      break;
    case PROP_SAMPLING_METHOD:
      self->sampling_method = (GstD3D11SamplingMethod) g_value_get_enum (value);
      break;
    case PROP_REDRAW_ON_UPDATE:
      self->redraw_on_update = g_value_get_boolean (value);
      break;
    case PROP_RENDER_RECTANGE:
      gst_video_overlay_set_property (object, PROP_RENDER_RECTANGE,
          PROP_RENDER_RECTANGE, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_videosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (object);
  GstD3D11CSLockGuard lk (&self->lock);

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, self->adapter);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, self->force_aspect_ratio);
      break;
    case PROP_ENABLE_NAVIGATION_EVENTS:
      g_value_set_boolean (value, self->enable_navigation_events);
      break;
    case PROP_FULLSCREEN_TOGGLE_MODE:
      g_value_set_flags (value, self->fullscreen_toggle_mode);
      break;
    case PROP_FULLSCREEN:
      if (self->window) {
        g_object_get_property (G_OBJECT (self->window), pspec->name, value);
      } else {
        g_value_set_boolean (value, self->fullscreen);
      }
      break;
    case PROP_DRAW_ON_SHARED_TEXTURE:
      g_value_set_boolean (value, self->draw_on_shared_texture);
      break;
    case PROP_ROTATE_METHOD:
      g_value_set_enum (value, self->method);
      break;
    case PROP_GAMMA_MODE:
      g_value_set_enum (value, self->gamma_mode);
      break;
    case PROP_PRIMARIES_MODE:
      g_value_set_enum (value, self->primaries_mode);
      break;
    case PROP_DISPLAY_FORMAT:
      g_value_set_enum (value, self->display_format);
      break;
    case PROP_EMIT_PRESENT:
      g_value_set_boolean (value, self->emit_present);
      break;
    case PROP_FOV:
      g_value_set_float (value, self->fov);
      break;
    case PROP_ORTHO:
      g_value_set_boolean (value, self->ortho);
      break;
    case PROP_ROTATION_X:
      g_value_set_float (value, self->rotation_x);
      break;
    case PROP_ROTATION_Y:
      g_value_set_float (value, self->rotation_x);
      break;
    case PROP_ROTATION_Z:
      g_value_set_float (value, self->rotation_z);
      break;
    case PROP_SCALE_X:
      g_value_set_float (value, self->scale_x);
      break;
    case PROP_SCALE_Y:
      g_value_set_float (value, self->scale_y);
      break;
    case PROP_MSAA:
      g_value_set_enum (value, self->msaa);
      break;
    case PROP_SAMPLING_METHOD:
      g_value_set_enum (value, self->sampling_method);
      break;
    case PROP_REDRAW_ON_UPDATE:
      g_value_set_boolean (value, self->redraw_on_update);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_video_sink_finalize (GObject * object)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (object);

  DeleteCriticalSection (&self->lock);
  g_free (self->title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_video_sink_set_context (GstElement * element, GstContext * context)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (element);

  gst_d3d11_handle_set_context (element, context, self->adapter, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static GstCaps *
gst_d3d11_video_sink_get_caps (GstBaseSink * sink, GstCaps * filter)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstCaps *caps = NULL;

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (sink));

  if (self->device) {
    gboolean is_hardware = FALSE;

    g_object_get (self->device, "hardware", &is_hardware, NULL);

    /* In case of WARP device, conversion via shader would be inefficient than
     * upstream videoconvert. Allow native formats in this case */
    if (!is_hardware) {
      GValue format_list = G_VALUE_INIT;
      GValue format = G_VALUE_INIT;

      g_value_init (&format_list, GST_TYPE_LIST);
      g_value_init (&format, G_TYPE_STRING);

      g_value_set_string (&format, "RGBA");
      gst_value_list_append_and_take_value (&format_list, &format);

      format = G_VALUE_INIT;
      g_value_init (&format, G_TYPE_STRING);
      g_value_set_string (&format, "BGRA");
      gst_value_list_append_and_take_value (&format_list, &format);

      caps = gst_caps_make_writable (caps);
      gst_caps_set_value (caps, "format", &format_list);
      g_value_unset (&format_list);
    }
  }

  if (filter) {
    GstCaps *isect;
    isect = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = isect;
  }

  return caps;
}

static gboolean
gst_d3d11_video_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (self, "set caps %" GST_PTR_FORMAT, caps);

  /* We will update window on show_frame() */
  self->caps_updated = TRUE;

  return TRUE;
}

static void
gst_d3d11_video_sink_release_window (GstD3D11VideoSink * self)
{
  if (self->window == NULL)
    return;

  g_signal_handlers_disconnect_by_data (self->window, self);
  gst_d3d11_window_unprepare (self->window);
  gst_clear_object (&self->window);
}

static GstFlowReturn
gst_d3d11_video_sink_update_window (GstD3D11VideoSink * self, GstCaps * caps)
{
  gint video_width, video_height;
  gint video_par_n, video_par_d;        /* video's PAR */
  gint display_par_n = 1, display_par_d = 1;    /* display's PAR */
  guint num, den;
  GError *error = NULL;
  GstStructure *config;
  GstD3D11Window *window;
  GstFlowReturn ret = GST_FLOW_OK;
  GstD3D11AllocationParams *params;
  guint bind_flags = D3D11_BIND_SHADER_RESOURCE;
  GstD3D11Format device_format;

  GST_DEBUG_OBJECT (self, "Updating window with caps %" GST_PTR_FORMAT, caps);

  self->caps_updated = FALSE;
  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  EnterCriticalSection (&self->lock);
  if (!gst_d3d11_video_sink_prepare_window (self)) {
    LeaveCriticalSection (&self->lock);

    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND, (nullptr),
        ("Failed to open window."));

    return GST_FLOW_ERROR;
  }

  if (!gst_video_info_from_caps (&self->info, caps)) {
    GST_DEBUG_OBJECT (self,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    LeaveCriticalSection (&self->lock);
    return GST_FLOW_ERROR;
  }

  video_width = GST_VIDEO_INFO_WIDTH (&self->info);
  video_height = GST_VIDEO_INFO_HEIGHT (&self->info);
  video_par_n = GST_VIDEO_INFO_PAR_N (&self->info);
  video_par_d = GST_VIDEO_INFO_PAR_D (&self->info);

  /* get aspect ratio from caps if it's present, and
   * convert video width and height to a display width and height
   * using wd / hd = wv / hv * PARv / PARd */

  if (!gst_video_calculate_display_ratio (&num, &den, video_width,
          video_height, video_par_n, video_par_d, display_par_n,
          display_par_d)) {
    LeaveCriticalSection (&self->lock);

    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (nullptr),
        ("Error calculating the output display ratio of the video."));
    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (self,
      "video width/height: %dx%d, calculated display ratio: %d/%d format: %s",
      video_width, video_height, num, den,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&self->info)));

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = num / den
   */

  /* start with same height, because of interlaced video
   * check hd / den is an integer scale factor, and scale wd with the PAR
   */
  if (video_height % den == 0) {
    GST_DEBUG_OBJECT (self, "keeping video height");
    GST_VIDEO_SINK_WIDTH (self) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (self) = video_height;
  } else if (video_width % num == 0) {
    GST_DEBUG_OBJECT (self, "keeping video width");
    GST_VIDEO_SINK_WIDTH (self) = video_width;
    GST_VIDEO_SINK_HEIGHT (self) = (guint)
        gst_util_uint64_scale_int (video_width, den, num);
  } else {
    GST_DEBUG_OBJECT (self, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (self) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (self) = video_height;
  }

  GST_DEBUG_OBJECT (self, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (self), GST_VIDEO_SINK_HEIGHT (self));
  self->video_width = video_width;
  self->video_height = video_height;

  if (GST_VIDEO_SINK_WIDTH (self) <= 0 || GST_VIDEO_SINK_HEIGHT (self) <= 0) {
    LeaveCriticalSection (&self->lock);

    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (nullptr),
        ("Error calculating the output display ratio of the video."));
    return GST_FLOW_ERROR;
  }

  if (self->pending_render_rect) {
    GstVideoRectangle rect = self->render_rect;

    self->pending_render_rect = FALSE;
    gst_d3d11_window_set_render_rectangle (self->window, &rect);
  }

  config = gst_structure_new ("convert-config",
      GST_D3D11_CONVERTER_OPT_GAMMA_MODE,
      GST_TYPE_VIDEO_GAMMA_MODE, self->gamma_mode,
      GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE,
      GST_TYPE_VIDEO_PRIMARIES_MODE, self->primaries_mode,
      GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER,
      GST_TYPE_D3D11_CONVERTER_SAMPLER_FILTER,
      gst_d3d11_sampling_method_to_native (self->sampling_method), nullptr);

  window = (GstD3D11Window *) gst_object_ref (self->window);
  LeaveCriticalSection (&self->lock);

  ret = gst_d3d11_window_prepare (window, GST_VIDEO_SINK_WIDTH (self),
      GST_VIDEO_SINK_HEIGHT (self), caps, config, self->display_format, &error);
  if (ret != GST_FLOW_OK) {
    GstMessage *error_msg;

    if (ret == GST_FLOW_FLUSHING) {
      GstD3D11CSLockGuard lk (&self->lock);
      GST_WARNING_OBJECT (self, "Couldn't prepare window but we are flushing");
      gst_d3d11_video_sink_release_window (self);
      gst_object_unref (window);

      return GST_FLOW_FLUSHING;
    }

    GST_ERROR_OBJECT (self, "cannot create swapchain");
    error_msg = gst_message_new_error (GST_OBJECT_CAST (self),
        error, "Failed to prepare d3d11window");
    g_clear_error (&error);
    gst_element_post_message (GST_ELEMENT (self), error_msg);
    gst_object_unref (window);

    return GST_FLOW_ERROR;
  }

  if (self->title) {
    gst_d3d11_window_set_title (window, self->title);
    g_clear_pointer (&self->title, g_free);
  }

  gst_object_unref (window);

  self->pool = gst_d3d11_buffer_pool_new (self->device);
  config = gst_buffer_pool_get_config (self->pool);

  if (gst_d3d11_device_get_format (self->device,
          GST_VIDEO_INFO_FORMAT (&self->info), &device_format) &&
      (device_format.format_support[0] &
          (guint) D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  }

  params = gst_d3d11_allocation_params_new (self->device, &self->info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  gst_buffer_pool_config_set_params (config, caps, self->info.size, 0, 0);
  if (!gst_buffer_pool_set_config (self->pool, config) ||
      !gst_buffer_pool_set_active (self->pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't setup buffer pool");
    gst_clear_object (&self->pool);

    GST_ELEMENT_ERROR (self, RESOURCE, FAILED, (nullptr),
        ("Couldn't setup buffer pool"));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
gst_d3d11_video_sink_key_event (GstD3D11Window * window, const gchar * event,
    const gchar * key, GstD3D11VideoSink * self)
{
  GstEvent *key_event;

  if (!self->enable_navigation_events || !event || !key)
    return;

  GST_LOG_OBJECT (self, "send key event %s, key %s", event, key);
  if (g_strcmp0 ("key-press", event) == 0) {
    key_event = gst_navigation_event_new_key_press (key,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (g_strcmp0 ("key-release", event) == 0) {
    key_event = gst_navigation_event_new_key_release (key,
        GST_NAVIGATION_MODIFIER_NONE);
  } else {
    return;
  }

  gst_navigation_send_event_simple (GST_NAVIGATION (self), key_event);
}

static void
gst_d3d11_video_mouse_key_event (GstD3D11Window * window, const gchar * event,
    gint button, gdouble x, gdouble y, GstD3D11VideoSink * self)
{
  GstEvent *mouse_event;

  if (!self->enable_navigation_events || !event)
    return;

  GST_LOG_OBJECT (self,
      "send mouse event %s, button %d (%.1f, %.1f)", event, button, x, y);
  if (g_strcmp0 ("mouse-button-press", event) == 0) {
    mouse_event = gst_navigation_event_new_mouse_button_press (button, x, y,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (g_strcmp0 ("mouse-button-release", event) == 0) {
    mouse_event = gst_navigation_event_new_mouse_button_release (button, x, y,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (g_strcmp0 ("mouse-move", event) == 0) {
    mouse_event = gst_navigation_event_new_mouse_move (x, y,
        GST_NAVIGATION_MODIFIER_NONE);
  } else {
    return;
  }

  gst_navigation_send_event_simple (GST_NAVIGATION (self), mouse_event);
}

static void
gst_d3d11_video_sink_present (GstD3D11Window * window, GstD3D11Device * device,
    ID3D11RenderTargetView * rtv, GstD3D11VideoSink * self)
{
  g_signal_emit (self, gst_d3d11_video_sink_signals[SIGNAL_PRESENT], 0,
      device, rtv);
}

static gboolean
gst_d3d11_video_sink_start (GstBaseSink * sink)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (self, "Start");

  if (!gst_d3d11_ensure_element_data (GST_ELEMENT_CAST (self), self->adapter,
          &self->device)) {
    GST_ERROR_OBJECT (sink, "Cannot create d3d11device");
    return FALSE;
  }

  return TRUE;
}

/* called with lock */
static gboolean
gst_d3d11_video_sink_prepare_window (GstD3D11VideoSink * self)
{
  GstD3D11WindowNativeType window_type = GST_D3D11_WINDOW_NATIVE_TYPE_HWND;

  if (self->window)
    return TRUE;

  if (self->draw_on_shared_texture) {
    GST_INFO_OBJECT (self,
        "Create dummy window for rendering on shared texture");
    self->window = gst_d3d11_window_dummy_new (self->device);
    goto done;
  }

  if (!self->window_id)
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (self));

  if (self->window_id) {
    window_type =
        gst_d3d11_window_get_native_type_from_handle (self->window_id);

    if (window_type != GST_D3D11_WINDOW_NATIVE_TYPE_NONE) {
      GST_DEBUG_OBJECT (self, "Have window handle %" G_GUINTPTR_FORMAT,
          self->window_id);
      gst_video_overlay_got_window_handle (GST_VIDEO_OVERLAY (self),
          self->window_id);
    }
  }

  GST_DEBUG_OBJECT (self, "Create window (type: %s)",
      gst_d3d11_window_get_native_type_to_string (window_type));

#if GST_D3D11_WINAPI_ONLY_APP
  if (window_type != GST_D3D11_WINDOW_NATIVE_TYPE_CORE_WINDOW &&
      window_type != GST_D3D11_WINDOW_NATIVE_TYPE_SWAP_CHAIN_PANEL) {
    GST_ERROR_OBJECT (self, "Overlay handle must be set before READY state");
    return FALSE;
  }
#endif

  switch (window_type) {
#if (!GST_D3D11_WINAPI_ONLY_APP)
    case GST_D3D11_WINDOW_NATIVE_TYPE_HWND:
      self->window = gst_d3d11_window_win32_new (self->device, self->window_id);
      if (!self->window_id) {
        HWND internal_hwnd =
            gst_d3d11_window_win32_get_internal_hwnd (self->window);
        GST_DEBUG_OBJECT (self, "Have window handle %" G_GUINTPTR_FORMAT,
            (guintptr) internal_hwnd);
        gst_video_overlay_got_window_handle (GST_VIDEO_OVERLAY (self),
            (guintptr) internal_hwnd);
      }
      break;
#endif
#if GST_D3D11_WINAPI_APP
    case GST_D3D11_WINDOW_NATIVE_TYPE_CORE_WINDOW:
      self->window = gst_d3d11_window_core_window_new (self->device,
          self->window_id);
      break;
    case GST_D3D11_WINDOW_NATIVE_TYPE_SWAP_CHAIN_PANEL:
      self->window = gst_d3d11_window_swap_chain_panel_new (self->device,
          self->window_id);
      break;
#endif
    default:
      break;
  }

done:
  if (!self->window) {
    GST_ERROR_OBJECT (self, "Cannot create d3d11window");
    return FALSE;
  }

  g_object_set (self->window,
      "force-aspect-ratio", self->force_aspect_ratio,
      "fullscreen-toggle-mode", self->fullscreen_toggle_mode,
      "fullscreen", self->fullscreen,
      "enable-navigation-events", self->enable_navigation_events,
      "emit-present", self->emit_present, nullptr);

  gst_d3d11_window_set_orientation (self->window, self->redraw_on_update,
      self->selected_method,
      self->fov, self->ortho, self->rotation_x, self->rotation_y,
      self->rotation_z, self->scale_x, self->scale_y);
  gst_d3d11_window_set_msaa_mode (self->window, self->msaa);

  g_signal_connect (self->window, "key-event",
      G_CALLBACK (gst_d3d11_video_sink_key_event), self);
  g_signal_connect (self->window, "mouse-event",
      G_CALLBACK (gst_d3d11_video_mouse_key_event), self);
  g_signal_connect (self->window, "present",
      G_CALLBACK (gst_d3d11_video_sink_present), self);

  GST_DEBUG_OBJECT (self,
      "Have prepared window %" GST_PTR_FORMAT, self->window);

  return TRUE;
}

static gboolean
gst_d3d11_video_sink_stop (GstBaseSink * sink)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstD3D11CSLockGuard lk (&self->lock);

  GST_DEBUG_OBJECT (self, "Stop");

  gst_clear_buffer (&self->prepared_buffer);
  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  gst_d3d11_video_sink_release_window (self);

  gst_clear_object (&self->device);
  g_clear_pointer (&self->title, g_free);

  return TRUE;
}

static gboolean
gst_d3d11_video_sink_propose_allocation (GstBaseSink * sink, GstQuery * query)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstCaps *caps;
  GstBufferPool *pool = NULL;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;

  if (!self->device)
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GstCapsFeatures *features;
    GstStructure *config;
    gboolean is_d3d11 = FALSE;

    features = gst_caps_get_features (caps, 0);
    if (features
        && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
      GST_DEBUG_OBJECT (self, "upstream support d3d11 memory");
      pool = gst_d3d11_buffer_pool_new (self->device);
      is_d3d11 = TRUE;
    } else {
      pool = gst_video_buffer_pool_new ();
    }

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!is_d3d11) {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    if (is_d3d11) {
      GstD3D11AllocationParams *d3d11_params;

      d3d11_params =
          gst_d3d11_allocation_params_new (self->device,
          &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_SHADER_RESOURCE,
          0);

      gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
      gst_d3d11_allocation_params_free (d3d11_params);
    }

    gst_buffer_pool_config_set_params (config, caps, (guint) size, 2, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (pool, "Couldn't set config");
      gst_object_unref (pool);

      return FALSE;
    }

    /* d3d11 buffer pool will update buffer size based on allocated texture,
     * get size from config again */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr,
        nullptr);
    gst_structure_free (config);

    if (is_d3d11) {
      /* In case of system memory, we will upload video frame to GPU memory,
       * (which is copy in any case), so crop meta support for system memory
       * is almost pointless */
      gst_query_add_allocation_meta (query,
          GST_VIDEO_CROP_META_API_TYPE, nullptr);
    }
  }

  /* We need at least 2 buffers because we hold on to the last one for redrawing
   * on window-resize event */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_WARNING_OBJECT (self, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_WARNING_OBJECT (self, "invalid caps specified");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_video_sink_query (GstBaseSink * sink, GstQuery * query)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT (self), query,
              self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->query (sink, query);
}

static gboolean
gst_d3d11_video_sink_unlock (GstBaseSink * sink)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->window)
    gst_d3d11_window_unlock (self->window);

  return TRUE;
}

static gboolean
gst_d3d11_video_sink_unlock_stop (GstBaseSink * sink)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->window)
    gst_d3d11_window_unlock_stop (self->window);

  return TRUE;
}

static gboolean
gst_d3d11_video_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:{
      GstTagList *taglist;
      gchar *title = nullptr;
      GstVideoOrientationMethod method = GST_VIDEO_ORIENTATION_IDENTITY;

      gst_event_parse_tag (event, &taglist);
      gst_tag_list_get_string (taglist, GST_TAG_TITLE, &title);

      if (title) {
        const gchar *app_name = g_get_application_name ();
        std::string title_string;
        GstD3D11CSLockGuard lk (&self->lock);

        if (app_name) {
          title_string = std::string (title) + " : " + std::string (app_name);
        } else {
          title_string = std::string (title);
        }

        if (self->window) {
          gst_d3d11_window_set_title (self->window, title_string.c_str ());
        } else {
          g_free (self->title);
          self->title = g_strdup (title_string.c_str ());
        }

        g_free (title);
      }

      if (gst_video_orientation_from_tag (taglist, &method)) {
        GstD3D11CSLockGuard lk (&self->lock);
        gst_d3d11_video_sink_set_orientation (self, method, TRUE);
      }
      break;
    }
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

/* called with lock */
static void
gst_d3d11_video_sink_set_orientation (GstD3D11VideoSink * self,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  if (method == GST_VIDEO_ORIENTATION_CUSTOM) {
    GST_WARNING_OBJECT (self, "Unsupported custom orientation");
    return;
  }

  if (from_tag)
    self->tag_method = method;
  else
    self->method = method;

  if (self->method == GST_VIDEO_ORIENTATION_AUTO) {
    self->selected_method = self->tag_method;
  } else {
    self->selected_method = self->method;
  }

  if (self->window) {
    gst_d3d11_window_set_orientation (self->window, self->redraw_on_update,
        self->selected_method,
        self->fov, self->ortho, self->rotation_x, self->rotation_y,
        self->rotation_z, self->scale_x, self->scale_y);
  }
}

static void
gst_d3d11_video_sink_check_device_update (GstD3D11VideoSink * self,
    GstBuffer * buf)
{
  GstMemory *mem;
  GstD3D11Memory *dmem;
  gboolean update_device = FALSE;

  /* We have configured window already, cannot update device */
  if (self->window)
    return;

  mem = gst_buffer_peek_memory (buf, 0);
  if (!gst_is_d3d11_memory (mem))
    return;

  dmem = GST_D3D11_MEMORY_CAST (mem);
  /* Same device, nothing to do */
  if (dmem->device == self->device)
    return;

  if (self->adapter < 0) {
    update_device = TRUE;
  } else {
    guint adapter = 0;

    g_object_get (dmem->device, "adapter", &adapter, NULL);
    /* The same GPU as what user wanted, update */
    if (adapter == (guint) self->adapter)
      update_device = TRUE;
  }

  if (!update_device)
    return;

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, dmem->device);

  gst_object_unref (self->device);
  self->device = (GstD3D11Device *) gst_object_ref (dmem->device);
}

static gboolean
gst_d3d11_video_sink_foreach_meta (GstBuffer * buffer, GstMeta ** meta,
    GstBuffer * uploaded)
{
  GstVideoOverlayCompositionMeta *cmeta;

  if ((*meta)->info->api != GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE)
    return TRUE;

  cmeta = (GstVideoOverlayCompositionMeta *) (*meta);
  if (!cmeta->overlay)
    return TRUE;

  if (gst_video_overlay_composition_n_rectangles (cmeta->overlay) == 0)
    return TRUE;

  gst_buffer_add_video_overlay_composition_meta (uploaded, cmeta->overlay);

  return TRUE;
}

static GstFlowReturn
gst_d3d11_video_sink_prepare (GstBaseSink * sink, GstBuffer * buffer)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstFlowReturn ret;

  gst_clear_buffer (&self->prepared_buffer);

  gst_d3d11_video_sink_check_device_update (self, buffer);
  if (self->caps_updated || !self->window) {
    GstCaps *caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (sink));

    /* shouldn't happen */
    if (!caps)
      return GST_FLOW_NOT_NEGOTIATED;

    ret = gst_d3d11_video_sink_update_window (self, caps);
    gst_caps_unref (caps);

    if (ret != GST_FLOW_OK)
      return ret;
  }

  if (!gst_is_d3d11_buffer (buffer)) {
    ret = gst_buffer_pool_acquire_buffer (self->pool, &self->prepared_buffer,
        nullptr);
    if (ret != GST_FLOW_OK)
      return ret;

    gst_d3d11_buffer_copy_into (self->prepared_buffer, buffer, &self->info);
    /* Upload to default texture */
    for (guint i = 0; i < gst_buffer_n_memory (self->prepared_buffer); i++) {
      GstMemory *mem = gst_buffer_peek_memory (self->prepared_buffer, i);
      GstMapInfo info;
      if (!gst_memory_map (mem,
              &info, (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
        GST_ERROR_OBJECT (self, "Couldn't map fallback buffer");
        gst_clear_buffer (&self->prepared_buffer);
        return GST_FLOW_ERROR;
      }

      gst_memory_unmap (mem, &info);
    }

    gst_buffer_foreach_meta (buffer,
        (GstBufferForeachMetaFunc) gst_d3d11_video_sink_foreach_meta,
        self->prepared_buffer);
  } else {
    self->prepared_buffer = gst_buffer_ref (buffer);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d11_video_sink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (sink);
  GstFlowReturn ret = GST_FLOW_OK;

  if (!self->prepared_buffer) {
    GST_ERROR_OBJECT (self, "No prepared buffer");
    return GST_FLOW_ERROR;
  }

  if (self->draw_on_shared_texture) {
    GstD3D11CSLockGuard lk (&self->lock);

    self->drawing = TRUE;

    GST_LOG_OBJECT (self, "Begin drawing");

    /* Application should call draw method on this callback */
    g_signal_emit (self, gst_d3d11_video_sink_signals[SIGNAL_BEGIN_DRAW], 0,
        NULL);

    GST_LOG_OBJECT (self, "End drawing");
    self->drawing = FALSE;
  } else {
    gst_d3d11_window_show (self->window);
    ret = gst_d3d11_window_render (self->window, self->prepared_buffer);
  }

  if (ret == GST_D3D11_WINDOW_FLOW_CLOSED) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Output window was closed"), (NULL));

    ret = GST_FLOW_ERROR;
  }

  return ret;
}

/* VideoOverlay interface */
static void
gst_d3d11_video_sink_set_window_handle (GstVideoOverlay * overlay,
    guintptr window_id)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (overlay);

  GST_DEBUG ("set window handle %" G_GUINTPTR_FORMAT, window_id);

  self->window_id = window_id;
}

static void
gst_d3d11_video_sink_set_render_rectangle (GstVideoOverlay * overlay, gint x,
    gint y, gint width, gint height)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (overlay);
  GstD3D11CSLockGuard lk (&self->lock);

  GST_DEBUG_OBJECT (self,
      "render rect x: %d, y: %d, width: %d, height %d", x, y, width, height);

  if (self->window) {
    GstVideoRectangle rect;

    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;

    self->render_rect = rect;

    gst_d3d11_window_set_render_rectangle (self->window, &rect);
  } else {
    self->render_rect.x = x;
    self->render_rect.y = y;
    self->render_rect.w = width;
    self->render_rect.h = height;
    self->pending_render_rect = TRUE;
  }
}

static void
gst_d3d11_video_sink_expose (GstVideoOverlay * overlay)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (overlay);
  GstD3D11CSLockGuard lk (&self->lock);

  if (self->window && self->window->swap_chain)
    gst_d3d11_window_render (self->window, nullptr);
}

static void
gst_d3d11_video_sink_video_overlay_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_d3d11_video_sink_set_window_handle;
  iface->set_render_rectangle = gst_d3d11_video_sink_set_render_rectangle;
  iface->expose = gst_d3d11_video_sink_expose;
}

/* Navigation interface */
static void
gst_d3d11_video_sink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstD3D11VideoSink *self = GST_D3D11_VIDEO_SINK (navigation);

  /* TODO: add support for translating native coordinate and video coordinate
   * when force-aspect-ratio is set */
  if (event) {
    gboolean handled;

    gst_event_ref (event);
    handled = gst_pad_push_event (GST_VIDEO_SINK_PAD (self), event);

    if (!handled)
      gst_element_post_message (GST_ELEMENT_CAST (self),
          gst_navigation_message_new_event (GST_OBJECT_CAST (self), event));

    gst_event_unref (event);
  }
}

static void
gst_d3d11_video_sink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = gst_d3d11_video_sink_navigation_send_event;
}

static gboolean
gst_d3d11_video_sink_draw_action (GstD3D11VideoSink * self,
    gpointer shared_handle, guint texture_misc_flags,
    guint64 acquire_key, guint64 release_key)
{
  GstFlowReturn ret;
  g_return_val_if_fail (shared_handle != NULL, FALSE);

  if (!self->draw_on_shared_texture) {
    GST_ERROR_OBJECT (self, "Invalid draw call, we are drawing on window");
    return FALSE;
  }

  if (!shared_handle) {
    GST_ERROR_OBJECT (self, "Invalid handle");
    return FALSE;
  }

  if (acquire_key != 0 || release_key != 0) {
    GST_ERROR_OBJECT (self, "Non zero mutex key value is not supported");
    return FALSE;
  }

  GstD3D11CSLockGuard lk (&self->lock);
  if (!self->drawing || !self->prepared_buffer) {
    GST_WARNING_OBJECT (self, "Nothing to draw");
    return FALSE;
  }

  GST_LOG_OBJECT (self, "Drawing on shared handle %p, MiscFlags: 0x%x"
      ", acquire key: %" G_GUINT64_FORMAT ", release key: %"
      G_GUINT64_FORMAT, shared_handle, texture_misc_flags, acquire_key,
      release_key);

  ret = gst_d3d11_window_render_on_shared_handle (self->window,
      self->prepared_buffer, shared_handle, texture_misc_flags, acquire_key,
      release_key);

  return ret == GST_FLOW_OK;
}
