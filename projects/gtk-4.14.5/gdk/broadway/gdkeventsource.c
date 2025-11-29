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

#include "gdkeventsprivate.h"
#include "gdkframeclockprivate.h"
#include "gdksurfaceprivate.h"

#include <stdlib.h>

static gboolean gdk_event_source_prepare  (GSource     *source,
                                           int         *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

#define HAS_FOCUS(toplevel)                                     \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

struct _GdkEventSource
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
};

static GSourceFuncs event_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

static GList *event_sources = NULL;

static gboolean
gdk_event_source_prepare (GSource *source,
                          int     *timeout)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  gboolean retval;

  *timeout = -1;

  retval = (_gdk_event_queue_find_first (display) != NULL);

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0 ||
      event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL);
  else
    retval = FALSE;

  return retval;
}

static void
handle_focus_change (GdkEvent *event)
{
  GdkEvent *focus_event;
  gboolean focus_in = (gdk_event_get_event_type (event) == GDK_ENTER_NOTIFY);

  if (gdk_crossing_event_get_detail (event) == GDK_NOTIFY_INFERIOR)
    return;

  if (!gdk_crossing_event_get_focus (event) )
    return;

  focus_event = gdk_focus_event_new (gdk_event_get_surface (event),
				     gdk_event_get_device (event),
				     focus_in);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_display_put_event (gdk_event_get_display (event), focus_event);
G_GNUC_END_IGNORE_DEPRECATIONS
  gdk_event_unref (focus_event);
}

void
_gdk_broadway_events_got_input (GdkDisplay *display,
                                BroadwayInputMsg *message)
{
  GdkBroadwayDisplay *display_broadway;
  GdkSurface *surface;
  GdkEvent *event = NULL;
  GList *node;

  display_broadway = GDK_BROADWAY_DISPLAY (display);

  switch (message->base.type) {
  case BROADWAY_EVENT_ENTER:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_crossing_event_new (GDK_ENTER_NOTIFY,
                                        surface,
                                        display_broadway->core_pointer,
                                        message->base.time,
                                        message->pointer.state,
                                        message->pointer.win_x,
                                        message->pointer.win_y,
                                        message->crossing.mode,
                                        GDK_NOTIFY_ANCESTOR);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);

	handle_focus_change (event);
      }
    break;
  case BROADWAY_EVENT_LEAVE:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_crossing_event_new (GDK_LEAVE_NOTIFY,
                                        surface,
                                        display_broadway->core_pointer,
                                        message->base.time,
                                        message->pointer.state,
                                        message->pointer.win_x,
                                        message->pointer.win_y,
                                        message->crossing.mode,
                                        GDK_NOTIFY_ANCESTOR);

	handle_focus_change (event);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    break;
  case BROADWAY_EVENT_POINTER_MOVE:
    if (_gdk_broadway_moveresize_handle_event (display, message))
      break;

    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_motion_event_new (surface,
                                      display_broadway->core_pointer,
                                      NULL,
                                      message->base.time,
                                      message->pointer.state,
                                      message->pointer.win_x,
                                      message->pointer.win_y,
                                      NULL);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    if (message->base.type != BROADWAY_EVENT_BUTTON_PRESS &&
        _gdk_broadway_moveresize_handle_event (display, message))
      break;

    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_button_event_new (message->base.type == BROADWAY_EVENT_BUTTON_PRESS
                                        ? GDK_BUTTON_PRESS
                                        : GDK_BUTTON_RELEASE,
                                      surface,
                                      display_broadway->core_pointer,
                                      NULL,
                                      message->base.time,
                                      message->pointer.state,
                                      message->button.button,
                                      message->pointer.win_x,
                                      message->pointer.win_y,
                                      NULL);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_SCROLL:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_scroll_event_new_discrete (surface,
                                               display_broadway->core_pointer,
                                               NULL,
                                               message->base.time,
                                               message->pointer.state,
                                               message->scroll.dir == 0
                                                 ? GDK_SCROLL_UP
                                                 : GDK_SCROLL_DOWN);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_TOUCH:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->touch.event_surface_id));
    if (surface)
      {
        GdkEventType event_type = 0;
        GdkModifierType state;

        switch (message->touch.touch_type) {
        case 0:
          event_type = GDK_TOUCH_BEGIN;
          break;
        case 1:
          event_type = GDK_TOUCH_UPDATE;
          break;
        case 2:
          event_type = GDK_TOUCH_END;
          break;
        default:
          g_printerr ("_gdk_broadway_events_got_input - Unknown touch type %d\n", message->touch.touch_type);
        }

        if (event_type != GDK_TOUCH_BEGIN &&
            message->touch.is_emulated && _gdk_broadway_moveresize_handle_event (display, message))
          break;

        state = message->touch.state;
        if (event_type == GDK_TOUCH_BEGIN || event_type == GDK_TOUCH_UPDATE)
          state |= GDK_BUTTON1_MASK;

        event = gdk_touch_event_new (event_type,
                                     GUINT_TO_POINTER (message->touch.sequence_id),
                                     surface,
                                     display_broadway->core_pointer,
                                     message->base.time,
                                     state,
                                     message->touch.win_x,
                                     message->touch.win_y,
                                     NULL,
                                     message->touch.is_emulated);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    surface = g_hash_table_lookup (display_broadway->id_ht,
                                  GINT_TO_POINTER (message->key.surface_id));
    if (surface)
      {
        GdkTranslatedKey translated;
        translated.keyval = message->key.key;
        translated.consumed = 0;
        translated.layout = 0;
        translated.level = 0;
        event = gdk_key_event_new (message->base.type == BROADWAY_EVENT_KEY_PRESS
                                     ? GDK_KEY_PRESS
                                     : GDK_KEY_RELEASE,
                                   surface,
                                   display_broadway->core_keyboard,
                                   message->base.time,
                                   message->key.key,
                                   message->key.state,
                                   FALSE,
                                   &translated,
                                   &translated,
                                   NULL);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    _gdk_display_device_grab_update (display,
                                     display_broadway->core_pointer,
                                     message->base.serial);
    break;

  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->configure_notify.id));
    if (surface)
      {
        gdk_surface_request_layout (surface);

        if (surface->resize_count >= 1)
          {
            surface->resize_count -= 1;

            if (surface->resize_count == 0)
              _gdk_broadway_moveresize_configure_done (display, surface);
          }
      }
    break;

  case BROADWAY_EVENT_ROUNDTRIP_NOTIFY:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->roundtrip_notify.id));
    if (surface)
      _gdk_broadway_roundtrip_notify (surface, message->roundtrip_notify.tag, message->roundtrip_notify.local);
    break;

  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    _gdk_broadway_display_size_changed (display, &message->screen_resize_notify);
    break;

  case BROADWAY_EVENT_FOCUS:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->focus.old_id));
    if (surface)
      {
        event = gdk_focus_event_new (surface,
                                     display_broadway->core_keyboard,
                                     FALSE);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->focus.new_id));
    if (surface)
      {
        event = gdk_focus_event_new (surface,
                                     display_broadway->core_keyboard,
                                     TRUE);

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    break;

  default:
    g_printerr ("_gdk_broadway_events_got_input - Unknown input command %c\n", message->base.type);
    break;
  }
}

void
_gdk_broadway_display_queue_events (GdkDisplay *display)
{
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

  event_sources = g_list_remove (event_sources, event_source);
}

GSource *
_gdk_broadway_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  char *name;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  name = g_strdup_printf ("GDK Broadway Event source (%s)",
                          gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  event_sources = g_list_prepend (event_sources, source);

  return source;
}
