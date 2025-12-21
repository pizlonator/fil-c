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

#include "xcb_event_source.h"
#include "gstgldisplay_x11.h"
#include "gstglwindow_x11.h"

G_GNUC_INTERNAL
    extern gboolean gst_gl_display_x11_handle_event (GstGLDisplayX11 *
    display_x11);

typedef struct _XCBEventSource
{
  GSource source;
  GPollFD pfd;
  uint32_t mask;
  GstGLDisplayX11 *display_x11;
} XCBEventSource;

static gboolean
xcb_event_source_prepare (GSource * base, gint * timeout)
{
  XCBEventSource *source = (XCBEventSource *) base;

  xcb_flush (source->display_x11->xcb_connection);

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

  gboolean ret = gst_gl_display_x11_handle_event (source->display_x11);

  source->pfd.revents = 0;

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
xcb_event_source_new (GstGLDisplayX11 * display_x11)
{
  xcb_connection_t *connection;
  XCBEventSource *source;

  connection = display_x11->xcb_connection;
  g_return_val_if_fail (connection != NULL, NULL);

  source = (XCBEventSource *)
      g_source_new (&xcb_event_source_funcs, sizeof (XCBEventSource));
  source->display_x11 = display_x11;
  source->pfd.fd = xcb_get_file_descriptor (connection);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}
