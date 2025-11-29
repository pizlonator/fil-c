/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkeventsource.h"

#include "gdk/gdkeventsprivate.h"

#include "gdksurface-x11.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "xsettings-client.h"


static gboolean gdk_event_source_prepare  (GSource     *source,
                                           int         *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

static GQuark quark_needs_enter = 0;

#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

struct _GdkEventSource
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
  GList *translators;
};

static GSourceFuncs event_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

static GdkSurface *
gdk_event_source_get_filter_surface (GdkEventSource      *event_source,
                                     const XEvent        *xevent,
                                     GdkEventTranslator **event_translator)
{
  GList *list = event_source->translators;
  GdkSurface *surface;

  *event_translator = NULL;

  while (list)
    {
      GdkEventTranslator *translator = list->data;

      list = list->next;
      surface = _gdk_x11_event_translator_get_surface (translator,
                                                     event_source->display,
                                                     xevent);
      if (surface)
        {
          *event_translator = translator;
          return surface;
        }
    }

  surface = gdk_x11_surface_lookup_for_display (event_source->display,
                                              xevent->xany.window);

  return surface;
}

static void
handle_focus_change (GdkEvent *event)
{
  GdkToplevelX11 *toplevel;
  GdkX11Screen *x11_screen;
  gboolean focus_in, had_focus;

  toplevel = _gdk_x11_surface_get_toplevel (gdk_event_get_surface (event));
  x11_screen = GDK_X11_SCREEN (GDK_SURFACE_SCREEN (gdk_event_get_surface (event)));
  focus_in = (gdk_event_get_event_type (event) == GDK_ENTER_NOTIFY);

  if (x11_screen->wmspec_check_window)
    return;

  if (!toplevel || gdk_crossing_event_get_detail (event) == GDK_NOTIFY_INFERIOR)
    return;

  toplevel->has_pointer = focus_in;

  if (!gdk_crossing_event_get_focus (event) || toplevel->has_focus_window)
    return;

  had_focus = HAS_FOCUS (toplevel);
  toplevel->has_pointer_focus = focus_in;

  if (HAS_FOCUS (toplevel) != had_focus)
    {
      GdkEvent *focus_event;

      focus_event = gdk_focus_event_new (gdk_event_get_surface (event),
                                         gdk_event_get_device (event),
                                         focus_in);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_display_put_event (gdk_event_get_display (event), focus_event);
G_GNUC_END_IGNORE_DEPRECATIONS
      gdk_event_unref (focus_event);
    }
}

static GdkEvent *
create_synth_crossing_event (GdkEventType     evtype,
                             GdkCrossingMode  mode,
                             GdkEvent        *real_event)
{
  GdkEvent *event;
  double x, y;

  g_assert (evtype == GDK_ENTER_NOTIFY || evtype == GDK_LEAVE_NOTIFY);

  gdk_event_get_position (real_event, &x, &y);
  event = gdk_crossing_event_new (evtype,
                                  gdk_event_get_surface (real_event),
                                  gdk_event_get_device (real_event),
                                  gdk_event_get_time (real_event),
                                  gdk_event_get_modifier_state (real_event),
                                  x, y,
                                  mode,
                                  GDK_NOTIFY_ANCESTOR);

  return event;
}

static void
handle_touch_synthetic_crossing (GdkEvent *event)
{
  GdkEventType evtype = gdk_event_get_event_type (event);
  GdkEvent *crossing = NULL;
  GdkSeat *seat = gdk_event_get_seat (event);
  gboolean needs_enter, set_needs_enter = FALSE;

  if (quark_needs_enter == 0)
    quark_needs_enter = g_quark_from_static_string ("gdk-x11-needs-enter-after-touch-end");

  needs_enter =
    GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (seat), quark_needs_enter));

  if (evtype == GDK_MOTION_NOTIFY && needs_enter)
    {
      set_needs_enter = FALSE;
      crossing = create_synth_crossing_event (GDK_ENTER_NOTIFY,
                                              GDK_CROSSING_DEVICE_SWITCH,
                                              event);
    }
  else if (evtype == GDK_TOUCH_BEGIN && needs_enter &&
           gdk_event_get_pointer_emulated (event))
    {
      set_needs_enter = FALSE;
      crossing = create_synth_crossing_event (GDK_ENTER_NOTIFY,
                                              GDK_CROSSING_TOUCH_BEGIN,
                                              event);
    }
  else if (evtype == GDK_TOUCH_END &&
           gdk_event_get_pointer_emulated (event))
    {
      set_needs_enter = TRUE;
      crossing = create_synth_crossing_event (GDK_LEAVE_NOTIFY,
                                              GDK_CROSSING_TOUCH_END,
                                              event);
    }
  else if (evtype == GDK_ENTER_NOTIFY ||
           evtype == GDK_LEAVE_NOTIFY)
    {
      /* We are receiving or shall receive a real crossing event,
       * turn this off.
       */
      set_needs_enter = FALSE;
    }
  else
    return;

  if (needs_enter != set_needs_enter)
    {
      if (!set_needs_enter)
        g_object_steal_qdata (G_OBJECT (seat), quark_needs_enter);
      else
        g_object_set_qdata (G_OBJECT (seat), quark_needs_enter,
                            GUINT_TO_POINTER (TRUE));
    }

  if (crossing)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_display_put_event (gdk_seat_get_display (seat), crossing);
G_GNUC_END_IGNORE_DEPRECATIONS
      gdk_event_unref (crossing);
    }
}

static GdkEvent *
gdk_event_source_translate_event (GdkX11Display  *x11_display,
                                  const XEvent   *xevent)
{
  GdkEventSource *event_source = (GdkEventSource *) x11_display->event_source;
  GdkEvent *event;
  GdkFilterReturn result = GDK_FILTER_CONTINUE;
  GdkDisplay *display = GDK_DISPLAY (x11_display);
  GdkEventTranslator *event_translator;
  GdkSurface *filter_surface;
  Display *dpy;
  GdkX11Screen *x11_screen;
  gpointer cache;

  x11_screen = GDK_X11_DISPLAY (display)->screen;
  dpy = GDK_DISPLAY_XDISPLAY (display);

  event = NULL;
  filter_surface = gdk_event_source_get_filter_surface (event_source, xevent,
                                                      &event_translator);

  /* apply XSettings filters */
  if (xevent->xany.window == XRootWindow (dpy, 0))
    result = gdk_xsettings_root_window_filter (xevent, x11_screen);

  if (result == GDK_FILTER_CONTINUE &&
      xevent->xany.window == x11_screen->xsettings_manager_window)
    result = gdk_xsettings_manager_window_filter (xevent, x11_screen);

  cache = gdk_surface_cache_get (display);
  if (cache)
    {
      if (result == GDK_FILTER_CONTINUE)
        result = gdk_surface_cache_shape_filter (xevent, cache);

      if (result == GDK_FILTER_CONTINUE &&
          xevent->xany.window == XRootWindow (dpy, 0))
        result = gdk_surface_cache_filter (xevent, cache);
    }

  if (result == GDK_FILTER_CONTINUE)
    result = _gdk_wm_protocols_filter (xevent, filter_surface, &event, NULL);

  if (result == GDK_FILTER_CONTINUE &&
      gdk_x11_drop_filter (filter_surface, xevent))
    result = GDK_FILTER_REMOVE;

  if (result != GDK_FILTER_CONTINUE)
    {
      if (result == GDK_FILTER_REMOVE)
        return NULL;
      else /* GDK_FILTER_TRANSLATE */
        return event;
    }

  if (event_translator)
    {
      /* Event translator was gotten before in get_filter_window() */
      event = _gdk_x11_event_translator_translate (event_translator,
                                                   display,
                                                   xevent);
    }
  else
    {
      GList *list = event_source->translators;

      while (list && !event)
        {
          GdkEventTranslator *translator = list->data;

          list = list->next;
          event = _gdk_x11_event_translator_translate (translator,
                                                       display,
                                                       xevent);
        }
    }

  if (event)
    {
      GdkEventType evtype = gdk_event_get_event_type (event);
  
      if ((evtype == GDK_ENTER_NOTIFY ||
           evtype == GDK_LEAVE_NOTIFY) &&
          gdk_event_get_surface (event) != NULL)
        {
          /* Handle focusing (in the case where no window manager is running */
          handle_focus_change (event);
        }

      if (evtype == GDK_TOUCH_BEGIN ||
          evtype == GDK_TOUCH_END ||
          evtype == GDK_MOTION_NOTIFY ||
          evtype == GDK_ENTER_NOTIFY ||
          evtype == GDK_LEAVE_NOTIFY)
        {
          handle_touch_synthetic_crossing (event);
        }
    }

  return event;
}

gboolean
gdk_event_source_xevent (GdkX11Display  *x11_display,
                         const XEvent   *xevent)
{
  GdkDisplay *display = GDK_DISPLAY (x11_display);
  GdkEvent *event;
  GList *node;

  event = gdk_event_source_translate_event (x11_display, xevent);
  if (event == NULL)
    return FALSE;

  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event, xevent->xany.serial);

  return TRUE;
}

static gboolean
gdk_check_xpending (GdkDisplay *display)
{
  return XPending (GDK_DISPLAY_XDISPLAY (display));
}

static gboolean
gdk_event_source_prepare (GSource *source,
                          int     *timeout)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  gboolean retval;

  *timeout = -1;

  if (display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (display) != NULL;
  else
    retval = (_gdk_event_queue_find_first (display) != NULL ||
              gdk_check_xpending (display));

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (event_source->display) != NULL;
  else if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              gdk_check_xpending (event_source->display));
  else
    retval = FALSE;

  return retval;
}

void
_gdk_x11_display_queue_events (GdkDisplay *display)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  XEvent xevent;
  gboolean unused;

  while (!_gdk_event_queue_find_first (display) && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      switch (xevent.type)
        {
        case KeyPress:
        case KeyRelease:
          break;
        default:
          if (XFilterEvent (&xevent, None))
            continue;
        }

#ifdef HAVE_XGENERICEVENTS
      /* Get cookie data here so it's available
       * to every event translator and event filter.
       */
      if (xevent.type == GenericEvent)
        XGetEventData (xdisplay, &xevent.xcookie);
#endif

      g_signal_emit_by_name (display, "xevent", &xevent, &unused);

#ifdef HAVE_XGENERICEVENTS
      if (xevent.type == GenericEvent)
        XFreeEventData (xdisplay, &xevent.xcookie);
#endif
    }
}

static gboolean
gdk_event_source_dispatch (GSource     *source,
                           GSourceFunc  callback,
                           gpointer     user_data)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  GdkEvent *event;

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_unref (event);
    }

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource *)source;

  g_list_free (event_source->translators);
  event_source->translators = NULL;
}

GSource *
gdk_x11_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  GdkX11Display *display_x11;
  int connection_number;
  char *name;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  name = g_strdup_printf ("GDK X11 Event source (%s)",
                          gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  display_x11 = GDK_X11_DISPLAY (display);
  connection_number = ConnectionNumber (display_x11->xdisplay);

  event_source->event_poll_fd.fd = connection_number;
  event_source->event_poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &event_source->event_poll_fd);

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

void
gdk_x11_event_source_add_translator (GdkEventSource     *source,
                                     GdkEventTranslator *translator)
{
  g_return_if_fail (GDK_IS_EVENT_TRANSLATOR (translator));

  source->translators = g_list_append (source->translators, translator);
}

void
gdk_x11_event_source_select_events (GdkEventSource *source,
                                    Window          window,
                                    GdkEventMask    event_mask,
                                    unsigned int    extra_x_mask)
{
  unsigned int xmask = extra_x_mask;
  GList *list;
  int i;

  list = source->translators;

  while (list)
    {
      GdkEventTranslator *translator = list->data;
      GdkEventMask translator_mask, mask;

      translator_mask = _gdk_x11_event_translator_get_handled_events (translator);
      mask = event_mask & translator_mask;

      if (mask != 0)
        {
          _gdk_x11_event_translator_select_surface_events (translator, window, mask);
          event_mask &= ~mask;
        }

      list = list->next;
    }

  for (i = 0; i < _gdk_x11_event_mask_table_size; i++)
    {
      if (event_mask & (1 << (i + 1)))
        xmask |= _gdk_x11_event_mask_table[i];
    }

  XSelectInput (GDK_DISPLAY_XDISPLAY (source->display), window, xmask);
}
