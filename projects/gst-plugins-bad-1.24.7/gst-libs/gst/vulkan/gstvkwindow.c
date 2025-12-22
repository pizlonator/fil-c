/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * SECTION:vkwindow
 * @title: GstVulkanWindow
 * @short_description: window/surface abstraction
 * @see_also: #GstVulkanDisplay, #GstVulkanSwapper
 *
 * GstVulkanWindow represents a window that elements can render into.  A window can
 * either be a user visible window (onscreen) or hidden (offscreen).
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <gmodule.h>
#include <stdio.h>

#include "gstvkwindow.h"

#if GST_VULKAN_HAVE_WINDOW_XCB
#include "xcb/gstvkwindow_xcb.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_WAYLAND
#include "wayland/gstvkwindow_wayland.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_COCOA
#include "cocoa/gstvkwindow_cocoa.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_IOS
#include "ios/gstvkwindow_ios.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_WIN32
#include "win32/gstvkwindow_win32.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_ANDROID
#include "android/gstvkwindow_android.h"
#endif

#define GST_CAT_DEFAULT gst_vulkan_window_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

struct _GstVulkanWindowPrivate
{
  guint surface_width;
  guint surface_height;
};

#define GET_PRIV(window) gst_vulkan_window_get_instance_private (window)

#define gst_vulkan_window_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstVulkanWindow, gst_vulkan_window,
    GST_TYPE_OBJECT);

static void gst_vulkan_window_finalize (GObject * object);

typedef struct _GstVulkanDummyWindow
{
  GstVulkanWindow parent;

  guintptr handle;
} GstVulkanDummyWindow;

typedef struct _GstVulkanDummyWindowCass
{
  GstVulkanWindowClass parent;
} GstVulkanDummyWindowClass;

GstVulkanDummyWindow *gst_vulkan_dummy_window_new (void);

enum
{
  PROP_0,
  PROP_DISPLAY,
};

enum
{
  SIGNAL_0,
  SIGNAL_CLOSE,
  SIGNAL_DRAW,
  SIGNAL_RESIZE,
  SIGNAL_MOUSE_EVENT,
  SIGNAL_KEY_EVENT,
  LAST_SIGNAL
};

static guint gst_vulkan_window_signals[LAST_SIGNAL] = { 0 };

static gboolean
_accum_logical_and (GSignalInvocationHint * ihint, GValue * return_accu,
    const GValue * handler_return, gpointer data)
{
  gboolean val = g_value_get_boolean (handler_return);
  gboolean val2 = g_value_get_boolean (return_accu);

  g_value_set_boolean (return_accu, val && val2);

  return TRUE;
}

GQuark
gst_vulkan_window_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-window-error-quark");
}

static gboolean
gst_vulkan_window_default_open (GstVulkanWindow * window, GError ** error)
{
  return TRUE;
}

static void
gst_vulkan_window_default_close (GstVulkanWindow * window)
{
}

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanwindow", 0,
        "Vulkan Window");
    g_once_init_leave (&_init, 1);
  }
}

static void
gst_vulkan_window_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanWindow *window = GST_VULKAN_WINDOW (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      window->display = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_window_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanWindow *window = GST_VULKAN_WINDOW (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      g_value_set_object (value, window->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_window_init (GstVulkanWindow * window)
{
}

static void
gst_vulkan_window_class_init (GstVulkanWindowClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  klass->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_default_open);
  klass->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_default_close);

  gst_vulkan_window_signals[SIGNAL_CLOSE] =
      g_signal_new ("close", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
      (GSignalAccumulator) _accum_logical_and, NULL, NULL, G_TYPE_BOOLEAN, 0);

  gst_vulkan_window_signals[SIGNAL_DRAW] =
      g_signal_new ("draw", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 0);

  gst_vulkan_window_signals[SIGNAL_RESIZE] =
      g_signal_new ("resize", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * GstVulkanWindow::mouse-event:
   * @object: the #GstVulkanWindow
   * @id: the name of the event
   * @button: the id of the button
   * @x: the x coordinate of the mouse event
   * @y: the y coordinate of the mouse event
   *
   * Will be emitted when a mouse event is received by the #GstVulkanWindow.
   *
   * Since: 1.18
   */
  gst_vulkan_window_signals[SIGNAL_MOUSE_EVENT] =
      g_signal_new ("mouse-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 4, G_TYPE_STRING,
      G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GstVulkanWindow::key-event:
   * @object: the #GstVulkanWindow
   * @id: the name of the event
   * @key: the id of the key pressed
   *
   * Will be emitted when a key event is received by the #GstVulkanWindow.
   *
   * Since: 1.18
   */
  gst_vulkan_window_signals[SIGNAL_KEY_EVENT] =
      g_signal_new ("key-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING,
      G_TYPE_STRING);

  gobject_class->set_property = gst_vulkan_window_set_property;
  gobject_class->get_property = gst_vulkan_window_get_property;
  gobject_class->finalize = gst_vulkan_window_finalize;

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_object ("display", "Display",
          "Associated Vulkan Display",
          GST_TYPE_VULKAN_DISPLAY, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  _init_debug ();
}

/**
 * gst_vulkan_window_new:
 * @display: a #GstVulkanDisplay
 *
 * Returns: (transfer full): a new #GstVulkanWindow using @display's connection
 *
 * Since: 1.18
 */
GstVulkanWindow *
gst_vulkan_window_new (GstVulkanDisplay * display)
{
  GstVulkanWindow *window = NULL;
  const gchar *user_choice;

  g_return_val_if_fail (display != NULL, NULL);

  _init_debug ();

  user_choice = g_getenv ("GST_VULKAN_WINDOW");
  GST_INFO ("creating a window, user choice:%s", user_choice);
#if GST_VULKAN_HAVE_WINDOW_XCB
  if (!window && (!user_choice || g_strstr_len (user_choice, 3, "xcb")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_xcb_new (display));
#endif
#if GST_VULKAN_HAVE_WINDOW_WAYLAND
  if (!window && (!user_choice || g_strstr_len (user_choice, 7, "wayland")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_wayland_new (display));
#endif
#if GST_VULKAN_HAVE_WINDOW_COCOA
  if (!window && (!user_choice || g_strstr_len (user_choice, 5, "cocoa")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_cocoa_new (display));
#endif
#if GST_VULKAN_HAVE_WINDOW_IOS
  if (!window && (!user_choice || g_strstr_len (user_choice, 3, "ios")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_ios_new (display));
#endif
#if GST_VULKAN_HAVE_WINDOW_WIN32
  if (!window && (!user_choice || g_strstr_len (user_choice, 5, "win32")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_win32_new (display));
#endif
#if GST_VULKAN_HAVE_WINDOW_ANDROID
  if (!window && (!user_choice || g_strstr_len (user_choice, 7, "android")))
    window = GST_VULKAN_WINDOW (gst_vulkan_window_android_new (display));
#endif
  if (!window) {
    /* subclass returned a NULL window */
    GST_WARNING ("Could not create window. user specified %s, creating dummy"
        " window", user_choice ? user_choice : "(null)");

    window = GST_VULKAN_WINDOW (gst_vulkan_dummy_window_new ());
  }

  window->display = gst_object_ref (display);

  return window;
}

static void
gst_vulkan_window_finalize (GObject * object)
{
  GstVulkanWindow *window = GST_VULKAN_WINDOW (object);

  gst_object_unref (window->display);

  G_OBJECT_CLASS (gst_vulkan_window_parent_class)->finalize (object);
}

/**
 * gst_vulkan_window_get_display:
 * @window: a #GstVulkanWindow
 *
 * Returns: (transfer full): the #GstVulkanDisplay for @window
 *
 * Since: 1.18
 */
GstVulkanDisplay *
gst_vulkan_window_get_display (GstVulkanWindow * window)
{
  g_return_val_if_fail (GST_IS_VULKAN_WINDOW (window), NULL);

  return gst_object_ref (window->display);
}

/**
 * gst_vulkan_window_get_surface: (skip)
 * @window: a #GstVulkanWindow
 * @error: a #GError
 *
 * Returns: the VkSurface for displaying into.  The caller is responsible for
 *     calling `VkDestroySurface` on the returned surface.
 *
 * Since: 1.18
 */
VkSurfaceKHR
gst_vulkan_window_get_surface (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_WINDOW (window), (VkSurfaceKHR) 0);
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);
  g_return_val_if_fail (klass->get_surface != NULL, (VkSurfaceKHR) 0);

  return klass->get_surface (window, error);
}

/**
 * gst_vulkan_window_get_presentation_support:
 * @window: a #GstVulkanWindow
 * @device: a #GstVulkanDevice
 * @queue_family_idx: the queue family
 *
 * Returns: whether the given combination of @window, @device and
 *          @queue_family_idx supports presentation
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_window_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  GstVulkanWindowClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_WINDOW (window), FALSE);
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);
  g_return_val_if_fail (klass->get_presentation_support != NULL, FALSE);

  return klass->get_presentation_support (window, device, queue_family_idx);
}

/**
 * gst_vulkan_window_open:
 * @window: a #GstVulkanWindow
 * @error: a #GError
 *
 * Returns: whether @window could be successfully opened
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_window_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_WINDOW (window), FALSE);
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);
  g_return_val_if_fail (klass->open != NULL, FALSE);

  return klass->open (window, error);
}

/**
 * gst_vulkan_window_close:
 * @window: a #GstVulkanWindow
 *
 * Attempt to close the window.
 *
 * Since: 1.18
 */
void
gst_vulkan_window_close (GstVulkanWindow * window)
{
  GstVulkanWindowClass *klass;
  gboolean to_close;

  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);
  g_return_if_fail (klass->close != NULL);

  g_signal_emit (window, gst_vulkan_window_signals[SIGNAL_CLOSE], 0, &to_close);

  if (to_close)
    klass->close (window);
}

/**
 * gst_vulkan_window_resize:
 * @window: a #GstVulkanWindow
 * @width: the new width
 * @height: the new height
 *
 * Resize the output surface.
 *
 * Currently intended for subclasses to update internal state.
 *
 * Since: 1.18
 */
void
gst_vulkan_window_resize (GstVulkanWindow * window, gint width, gint height)
{
  GstVulkanWindowPrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));

  priv = GET_PRIV (window);

  priv->surface_width = width;
  priv->surface_height = height;

  g_signal_emit (window, gst_vulkan_window_signals[SIGNAL_RESIZE], 0, width,
      height);
}

/**
 * gst_vulkan_window_redraw:
 * @window: a #GstVulkanWindow
 *
 * Ask the @window to redraw its contents
 *
 * Since: 1.18
 */
void
gst_vulkan_window_redraw (GstVulkanWindow * window)
{
  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));

  g_signal_emit (window, gst_vulkan_window_signals[SIGNAL_DRAW], 0);
}

void
gst_vulkan_window_set_window_handle (GstVulkanWindow * window, guintptr handle)
{
  GstVulkanWindowClass *klass;

  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);

  if (!klass->set_window_handle) {
    if (handle)
      g_warning ("%s does not implement the set_window_handle vfunc. "
          "Output will not be embedded into the specified surface.",
          GST_OBJECT_NAME (window));
  } else {
    klass->set_window_handle (window, handle);
  }
}

/**
 * gst_vulkan_window_get_surface_dimensions:
 * @window: a #GstVulkanWindow
 * @width: (out): Current width of @window
 * @height: (out): Current height of @window
 *
 * Since: 1.18
 */
void
gst_vulkan_window_get_surface_dimensions (GstVulkanWindow * window,
    guint * width, guint * height)
{
  GstVulkanWindowPrivate *priv;
  GstVulkanWindowClass *klass;

  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));
  klass = GST_VULKAN_WINDOW_GET_CLASS (window);
  priv = GET_PRIV (window);

  if (klass->get_surface_dimensions) {
    klass->get_surface_dimensions (window, width, height);
  } else {
    GST_DEBUG_OBJECT (window, "Returning size %ix%i",
        priv->surface_width, priv->surface_height);
    *width = priv->surface_width;
    *height = priv->surface_height;
  }
}

void
gst_vulkan_window_send_key_event (GstVulkanWindow * window,
    const char *event_type, const char *key_str)
{
  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));

  g_signal_emit (window, gst_vulkan_window_signals[SIGNAL_KEY_EVENT], 0,
      event_type, key_str);
}

void
gst_vulkan_window_send_mouse_event (GstVulkanWindow * window,
    const char *event_type, int button, double posx, double posy)
{
  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));

  g_signal_emit (window, gst_vulkan_window_signals[SIGNAL_MOUSE_EVENT], 0,
      event_type, button, posx, posy);
}

/**
 * gst_vulkan_window_handle_events:
 * @window: a #GstVulkanWindow
 * @handle_events: a #gboolean indicating if events should be handled or not.
 *
 * Tell a @window that it should handle events from the window system. These
 * events are forwarded upstream as navigation events. In some window systems
 * events are not propagated in the window hierarchy if a client is listening
 * for them. This method allows you to disable events handling completely
 * from the @window.
 *
 * Since: 1.18
 */
void
gst_vulkan_window_handle_events (GstVulkanWindow * window,
    gboolean handle_events)
{
  GstVulkanWindowClass *window_class;

  g_return_if_fail (GST_IS_VULKAN_WINDOW (window));
  window_class = GST_VULKAN_WINDOW_GET_CLASS (window);

  if (window_class->handle_events)
    window_class->handle_events (window, handle_events);
}

GType gst_vulkan_dummy_window_get_type (void);
G_DEFINE_TYPE (GstVulkanDummyWindow, gst_vulkan_dummy_window,
    GST_TYPE_VULKAN_WINDOW);

static void
gst_vulkan_dummy_window_class_init (GstVulkanDummyWindowClass * klass)
{
}

static void
gst_vulkan_dummy_window_init (GstVulkanDummyWindow * dummy)
{
  dummy->handle = 0;
}

GstVulkanDummyWindow *
gst_vulkan_dummy_window_new (void)
{
  GstVulkanDummyWindow *window;

  window = g_object_new (gst_vulkan_dummy_window_get_type (), NULL);
  gst_object_ref_sink (window);

  return window;
}
