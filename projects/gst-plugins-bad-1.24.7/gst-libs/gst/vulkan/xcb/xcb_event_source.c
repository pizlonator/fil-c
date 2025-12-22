/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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

#include <stdint.h>
#include <stdlib.h>

#include <gst/vulkan/gstvkapi.h>

#include <gst/vulkan/xcb/gstvkdisplay_xcb.h>
#include "gstvkwindow_xcb.h"

#include "xcb_event_source.h"

static gint
_compare_xcb_window (GstVulkanWindowXCB * window_xcb, xcb_window_t * window_id)
{
  gint ret;

  g_return_val_if_fail (GST_IS_VULKAN_WINDOW_XCB (window_xcb), -1);
  g_return_val_if_fail (window_id != 0, -1);

  ret = window_xcb->win_id - *window_id;

  return ret;
}

static GstVulkanWindowXCB *
_find_window_from_xcb_window (GstVulkanDisplayXCB * display_xcb,
    xcb_window_t window_id)
{
  GstVulkanDisplay *display = GST_VULKAN_DISPLAY (display_xcb);

  if (!window_id)
    return NULL;

  return (GstVulkanWindowXCB *) gst_vulkan_display_find_window (display,
      &window_id, (GCompareFunc) _compare_xcb_window);
}

static GstVulkanWindowXCB *
_window_from_event (GstVulkanDisplayXCB * display_xcb,
    xcb_generic_event_t * event)
{
  uint8_t event_code = event->response_type & 0x7f;

  switch (event_code) {
/* *INDENT-OFF* */
#define WIN_FROM_EVENT(case_val,event_type,window_field) \
    case case_val:{ \
      event_type * real_event = (event_type *) event; \
      return _find_window_from_xcb_window (display_xcb, real_event->window_field); \
    }
    WIN_FROM_EVENT (XCB_CLIENT_MESSAGE, xcb_client_message_event_t, window)
    WIN_FROM_EVENT (XCB_CONFIGURE_NOTIFY, xcb_configure_notify_event_t, window)
    WIN_FROM_EVENT (XCB_EXPOSE, xcb_expose_event_t, window)
    WIN_FROM_EVENT (XCB_KEY_PRESS, xcb_key_press_event_t, event)
    WIN_FROM_EVENT (XCB_KEY_RELEASE, xcb_key_release_event_t, event)
    WIN_FROM_EVENT (XCB_BUTTON_PRESS, xcb_button_press_event_t, event)
    WIN_FROM_EVENT (XCB_BUTTON_RELEASE, xcb_button_release_event_t, event)
    WIN_FROM_EVENT (XCB_MOTION_NOTIFY, xcb_motion_notify_event_t, event)
#undef WIN_FROM_EVENT
/* *INDENT-ON* */
    default:
      return NULL;
  }
}

G_GNUC_INTERNAL
    extern gboolean
gst_vulkan_window_xcb_handle_event (GstVulkanWindowXCB * window_xcb,
    xcb_generic_event_t * event);

static gboolean
_xcb_handle_event (GstVulkanDisplayXCB * display_xcb)
{
  xcb_connection_t *connection =
      GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  xcb_generic_event_t *event;
  gboolean ret = TRUE;

  while (ret && (event = xcb_poll_for_event (connection))) {
    GstVulkanWindowXCB *window_xcb;

    window_xcb = _window_from_event (display_xcb, event);
    if (window_xcb) {
      ret = gst_vulkan_window_xcb_handle_event (window_xcb, event);
      gst_object_unref (window_xcb);
    }

    g_free (event);
  }

  return ret;
}

typedef struct _XCBEventSource
{
  GSource source;
  GPollFD pfd;
  uint32_t mask;
  GstVulkanDisplayXCB *display_xcb;
} XCBEventSource;

static gboolean
xcb_event_source_prepare (GSource * base, gint * timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean
xcb_event_source_check (GSource * base)
{
  XCBEventSource *source = (XCBEventSource *) base;
  gboolean retval;

  retval = source->pfd.revents;

  return retval;
}

static gboolean
xcb_event_source_dispatch (GSource * base, GSourceFunc callback, gpointer data)
{
  XCBEventSource *source = (XCBEventSource *) base;

  gboolean ret = _xcb_handle_event (source->display_xcb);

  if (callback)
    callback (data);

  return ret;
}

static GSourceFuncs xcb_event_source_funcs = {
  xcb_event_source_prepare,
  xcb_event_source_check,
  xcb_event_source_dispatch,
  NULL
};

GSource *
xcb_event_source_new (GstVulkanDisplayXCB * display_xcb)
{
  xcb_connection_t *connection;
  XCBEventSource *source;

  connection = GST_VULKAN_DISPLAY_XCB_CONNECTION (display_xcb);
  g_return_val_if_fail (connection != NULL, NULL);

  source = (XCBEventSource *)
      g_source_new (&xcb_event_source_funcs, sizeof (XCBEventSource));
  source->display_xcb = display_xcb;
  source->pfd.fd = xcb_get_file_descriptor (connection);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}
