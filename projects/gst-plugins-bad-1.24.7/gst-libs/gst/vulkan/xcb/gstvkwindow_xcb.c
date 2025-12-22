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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <locale.h>

#include "gstvkwindow_xcb.h"
#include "gstvkdisplay_xcb.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#define GET_PRIV(o) gst_vulkan_window_xcb_get_instance_private (o)

#define GST_CAT_DEFAULT gst_vulkan_window_xcb_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanwindowxcb", 0,
        "Vulkan XCB Window");
    g_once_init_leave (&_init, 1);
  }
}

enum
{
  PROP_0,
};

struct _GstVulkanWindowXCBPrivate
{
  gboolean activate;
  gboolean activate_result;

  gint preferred_width;
  gint preferred_height;

  xcb_intern_atom_reply_t *atom_wm_delete_window;
  gboolean handle_events;

  guint8 first_xkb_event;
  gint32 kbd_device_id;
  struct xkb_context *xkb_ctx;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
};

#define gst_vulkan_window_xcb_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanWindowXCB, gst_vulkan_window_xcb,
    GST_TYPE_VULKAN_WINDOW, G_ADD_PRIVATE (GstVulkanWindowXCB) _init_debug ());

static VkSurfaceKHR gst_vulkan_window_xcb_get_surface (GstVulkanWindow * window,
    GError ** error);
static gboolean gst_vulkan_window_xcb_get_presentation_support (GstVulkanWindow
    * window, GstVulkanDevice * device, guint32 queue_family_idx);
static gboolean gst_vulkan_window_xcb_open (GstVulkanWindow * window,
    GError ** error);
static void gst_vulkan_window_xcb_close (GstVulkanWindow * window);
static void gst_vulkan_window_xcb_handle_events (GstVulkanWindow * window,
    gboolean handle_events);

static void
gst_vulkan_window_xcb_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vulkan_window_xcb_class_init (GstVulkanWindowXCBClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  GstVulkanWindowClass *window_class = (GstVulkanWindowClass *) klass;

  obj_class->finalize = gst_vulkan_window_xcb_finalize;

  window_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_xcb_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_xcb_close);
  window_class->get_surface = gst_vulkan_window_xcb_get_surface;
  window_class->handle_events = gst_vulkan_window_xcb_handle_events;
  window_class->get_presentation_support =
      gst_vulkan_window_xcb_get_presentation_support;
}

static void
gst_vulkan_window_xcb_init (GstVulkanWindowXCB * window)
{
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window);

  priv->handle_events = TRUE;
}

/* Must be called in the gl thread */
GstVulkanWindowXCB *
gst_vulkan_window_xcb_new (GstVulkanDisplay * display)
{
  GstVulkanWindowXCB *window;

  _init_debug ();

  if ((gst_vulkan_display_get_handle_type (display) &
          GST_VULKAN_DISPLAY_TYPE_XCB)
      == GST_VULKAN_DISPLAY_TYPE_NONE) {
    GST_INFO ("Wrong display type %u for this window type %u", display->type,
        GST_VULKAN_DISPLAY_TYPE_XCB);
    return NULL;
  }

  window = g_object_new (GST_TYPE_VULKAN_WINDOW_XCB, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
gst_vulkan_window_xcb_show (GstVulkanWindow * window)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  GstVulkanDisplayXCB *display_xcb = GST_VULKAN_DISPLAY_XCB (window->display);
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);

  if (!window_xcb->visible) {
    xcb_map_window (connection, window_xcb->win_id);
    xcb_flush (connection);
    window_xcb->visible = TRUE;
  }
}

static void
gst_vulkan_window_xcb_hide (GstVulkanWindow * window)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  GstVulkanDisplayXCB *display_xcb = GST_VULKAN_DISPLAY_XCB (window->display);
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);

  if (window_xcb->visible) {
    xcb_unmap_window (connection, window_xcb->win_id);
    window_xcb->visible = FALSE;
  }
}

static gboolean
init_keyboard (GstVulkanWindowXCB * window_xcb)
{
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window_xcb);
  GstVulkanWindow *window = GST_VULKAN_WINDOW (window_xcb);
  GstVulkanDisplayXCB *display_xcb = GST_VULKAN_DISPLAY_XCB (window->display);
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  int ret;

  ret = xkb_x11_setup_xkb_extension (connection,
      XKB_X11_MIN_MAJOR_XKB_VERSION,
      XKB_X11_MIN_MINOR_XKB_VERSION,
      XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
      NULL, NULL, &priv->first_xkb_event, NULL);
  if (!ret) {
    GST_ERROR_OBJECT (window_xcb, "Couldn't setup XKB extension\n");
    return FALSE;;
  }

  priv->xkb_ctx = xkb_context_new (0);
  if (!priv->xkb_ctx)
    return FALSE;

  priv->kbd_device_id = xkb_x11_get_core_keyboard_device_id (connection);

  priv->xkb_keymap = xkb_x11_keymap_new_from_device (priv->xkb_ctx,
      connection, priv->kbd_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!priv->xkb_keymap)
    return FALSE;
  priv->xkb_state = xkb_x11_state_new_from_device (priv->xkb_keymap,
      connection, priv->kbd_device_id);
  if (!priv->xkb_state)
    return FALSE;

  return TRUE;
}

gboolean
gst_vulkan_window_xcb_create_window (GstVulkanWindowXCB * window_xcb)
{
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window_xcb);
  GstVulkanDisplayXCB *display_xcb;
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t root_window;
  uint32_t value_mask, value_list[32];
  xcb_intern_atom_cookie_t cookie, cookie2;
  xcb_intern_atom_reply_t *reply, *reply2;
//  const gchar *title = "OpenGL renderer";
  gint x = 0, y = 0, width = 320, height = 240;

  display_xcb =
      GST_VULKAN_DISPLAY_XCB (GST_VULKAN_WINDOW (window_xcb)->display);
  connection = GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  root_window = GST_VULKAN_DISPLAY_XCB_ROOT_WINDOW (display_xcb);
  screen = GST_VULKAN_DISPLAY_XCB_SCREEN (display_xcb);

  window_xcb->win_id = xcb_generate_id (connection);

  value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  value_list[0] = screen->black_pixel;
  value_list[1] =
      XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY;

  xcb_create_window (connection, XCB_COPY_FROM_PARENT, window_xcb->win_id,
      root_window, x, y, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual, value_mask, value_list);

  gst_vulkan_window_xcb_handle_events (GST_VULKAN_WINDOW (window_xcb),
      priv->handle_events);

  GST_LOG_OBJECT (window_xcb, "vulkan window id: %p",
      (gpointer) (guintptr) window_xcb->win_id);
  GST_LOG_OBJECT (window_xcb, "vulkan window props: x:%d y:%d", x, y);

  /* Magic code that will send notification when window is destroyed */
  cookie = xcb_intern_atom (connection, 1, 12, "WM_PROTOCOLS");
  reply = xcb_intern_atom_reply (connection, cookie, 0);

  cookie2 = xcb_intern_atom (connection, 0, 16, "WM_DELETE_WINDOW");
  reply2 = xcb_intern_atom_reply (connection, cookie2, 0);

  xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window_xcb->win_id,
      reply->atom, 4, 32, 1, &reply2->atom);
  g_free (reply);
  g_free (reply2);

  init_keyboard (window_xcb);

  gst_vulkan_window_xcb_show (GST_VULKAN_WINDOW (window_xcb));

  return TRUE;
}

static VkSurfaceKHR
gst_vulkan_window_xcb_get_surface (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  VkXcbSurfaceCreateInfoKHR info = { 0, };
  VkSurfaceKHR ret;
  VkResult err;

  info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.connection = GST_VULKAN_DISPLAY_XCB_CONNECTION (window->display);
  info.window = GST_VULKAN_WINDOW_XCB (window)->win_id;

  if (!window_xcb->CreateXcbSurface)
    window_xcb->CreateXcbSurface =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkCreateXcbSurfaceKHR");
  if (!window_xcb->CreateXcbSurface) {
    g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_FEATURE_NOT_PRESENT,
        "Could not retrieve \"vkCreateXcbSurfaceKHR\" function pointer");
    return VK_NULL_HANDLE;
  }

  err =
      window_xcb->CreateXcbSurface (window->display->instance->instance, &info,
      NULL, &ret);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateXcbSurfaceKHR") < 0)
    return VK_NULL_HANDLE;

  return ret;
}

static gboolean
gst_vulkan_window_xcb_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  VkPhysicalDevice gpu;
  xcb_screen_t *screen;

  screen = GST_VULKAN_DISPLAY_XCB_SCREEN (window->display);

  if (!window_xcb->GetPhysicalDeviceXcbPresentationSupport)
    window_xcb->GetPhysicalDeviceXcbPresentationSupport =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkGetPhysicalDeviceXcbPresentationSupportKHR");
  if (!window_xcb->GetPhysicalDeviceXcbPresentationSupport) {
    GST_WARNING_OBJECT (window, "Could not retrieve "
        "\"vkGetPhysicalDeviceXcbPresentationSupportKHR\" " "function pointer");
    return FALSE;
  }

  gpu = gst_vulkan_device_get_physical_device (device);
  if (window_xcb->GetPhysicalDeviceXcbPresentationSupport (gpu,
          queue_family_idx, GST_VULKAN_DISPLAY_XCB_CONNECTION (window->display),
          screen->root_visual))
    return TRUE;
  return FALSE;
}

static gboolean
gst_vulkan_window_xcb_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  GstVulkanDisplayXCB *display_xcb;
  xcb_connection_t *connection;

  if (!GST_IS_VULKAN_DISPLAY_XCB (window->display)) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Cannot create an XCB window from a non-XCB display");
    goto failure;
  }

  display_xcb = GST_VULKAN_DISPLAY_XCB (window->display);
  connection = GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  if (connection == NULL) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to connect to X display server with XCB");
    goto failure;
  }

  if (!GST_VULKAN_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  return gst_vulkan_window_xcb_create_window (window_xcb);

failure:
  return FALSE;
}

static void
gst_vulkan_window_xcb_close (GstVulkanWindow * window)
{
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window_xcb);
  GstVulkanDisplayXCB *display_xcb = (GstVulkanDisplayXCB *) window->display;
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);

  if (connection) {
    gst_vulkan_window_xcb_hide (window);

    g_free (priv->atom_wm_delete_window);
    priv->atom_wm_delete_window = NULL;
    GST_DEBUG ("display receiver closed");
  }

  if (priv->xkb_state)
    xkb_state_unref (priv->xkb_state);
  priv->xkb_state = NULL;
  if (priv->xkb_keymap)
    xkb_keymap_unref (priv->xkb_keymap);
  priv->xkb_keymap = NULL;
  if (priv->xkb_ctx)
    xkb_context_unref (priv->xkb_ctx);
  priv->xkb_ctx = NULL;

  GST_VULKAN_WINDOW_CLASS (parent_class)->close (window);
}

G_GNUC_INTERNAL
    gboolean
gst_vulkan_window_xcb_handle_event (GstVulkanWindowXCB * window_xcb,
    xcb_generic_event_t * event);

gboolean
gst_vulkan_window_xcb_handle_event (GstVulkanWindowXCB * window_xcb,
    xcb_generic_event_t * event)
{
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window_xcb);
  GstVulkanDisplayXCB *display_xcb =
      GST_VULKAN_DISPLAY_XCB (window_xcb->parent.display);
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  uint8_t event_code = event->response_type & 0x7f;

  switch (event_code) {
    case XCB_CLIENT_MESSAGE:{
      xcb_client_message_event_t *client_event;
      xcb_intern_atom_cookie_t cookie;
      xcb_intern_atom_reply_t *reply;

      client_event = (xcb_client_message_event_t *) event;
      cookie = xcb_intern_atom (connection, 0, 16, "WM_DELETE_WINDOW");
      reply = xcb_intern_atom_reply (connection, cookie, 0);

      if (client_event->data.data32[0] == reply->atom) {
        GST_INFO_OBJECT (window_xcb, "Close requested");

        gst_vulkan_window_close (GST_VULKAN_WINDOW (window_xcb));
        gst_vulkan_display_remove_window (GST_VULKAN_DISPLAY (display_xcb),
            GST_VULKAN_WINDOW (window_xcb));
      }

      g_free (reply);
      break;
    }
    case XCB_CONFIGURE_NOTIFY:{
      xcb_configure_notify_event_t *configure_event;

      configure_event = (xcb_configure_notify_event_t *) event;

      gst_vulkan_window_resize (GST_VULKAN_WINDOW (window_xcb),
          configure_event->width, configure_event->height);
      break;
    }
    case XCB_EXPOSE:{
      xcb_expose_event_t *expose_event = (xcb_expose_event_t *) event;

      /* non-zero means that other Expose follows
       * so just wait for the last one
       * in theory we should not receive non-zero because
       * we have no sub areas here but just in case */
      if (expose_event->count != 0)
        break;

      gst_vulkan_window_redraw (GST_VULKAN_WINDOW (window_xcb));
      break;
    }
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:{
      xcb_key_press_event_t *kp = (xcb_key_press_event_t *) event;
      int nsyms;
      const xkb_keysym_t *syms;
      const char *event_type_str;

      if (!priv->xkb_state)
        /* no xkb support. What year is this?! */
        break;

      nsyms = xkb_state_key_get_syms (priv->xkb_state, kp->detail, &syms);

      if (event_code == XCB_KEY_PRESS)
        event_type_str = "key-press";
      else
        event_type_str = "key-release";

      /* TODO: compose states, keymap changes */
      if (nsyms <= 0)
        break;

      for (int i = 0; i < nsyms; i++) {
        char s[64];
        xkb_keysym_get_name (syms[i], s, sizeof (s));
        gst_vulkan_window_send_key_event (GST_VULKAN_WINDOW (window_xcb),
            event_type_str, s);
      }
      break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:{
      xcb_button_press_event_t *bp = (xcb_button_press_event_t *) event;
      const gchar *event_type_str;

      if (event_code == XCB_BUTTON_PRESS)
        event_type_str = "mouse-button-press";
      else
        event_type_str = "mouse-button-release";

      gst_vulkan_window_send_mouse_event (GST_VULKAN_WINDOW (window_xcb),
          event_type_str, bp->detail, (double) bp->event_x,
          (double) bp->event_y);
      break;
    }
    case XCB_MOTION_NOTIFY:{
      xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *) event;

      gst_vulkan_window_send_mouse_event (GST_VULKAN_WINDOW (window_xcb),
          "mouse-move", 0, (double) motion->event_x, (double) motion->event_y);
      break;
    }
    default:
      GST_DEBUG ("unhandled XCB type: %u", event_code);
      break;
  }

  return TRUE;
}

static void
gst_vulkan_window_xcb_handle_events (GstVulkanWindow * window,
    gboolean handle_events)
{
  GstVulkanDisplayXCB *display_xcb = GST_VULKAN_DISPLAY_XCB (window->display);
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  GstVulkanWindowXCB *window_xcb = GST_VULKAN_WINDOW_XCB (window);
  GstVulkanWindowXCBPrivate *priv = GET_PRIV (window_xcb);

  priv->handle_events = handle_events;

  if (window_xcb->win_id) {
    guint32 events;

    events = XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE
        | XCB_EVENT_MASK_VISIBILITY_CHANGE;
    if (handle_events) {
      events |= XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE
          | XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_POINTER_MOTION
          | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
          | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;
    }
    xcb_change_window_attributes (connection,
        window_xcb->win_id, XCB_CW_EVENT_MASK, &events);
  }
}
