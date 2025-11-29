/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkdropprivate.h"

#include "gdkprivate.h"
#include "gdkasync.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-x11.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkdragprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkselectioninputstream-x11.h"
#include "gdkselectionoutputstream-x11.h"
#include "gdktextlistconverter-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <string.h>

#define GDK_TYPE_X11_DROP (gdk_x11_drop_get_type())
G_DECLARE_FINAL_TYPE (GdkX11Drop, gdk_x11_drop, GDK, X11_DROP, GdkDrop)

struct _GdkX11Drop
{
  GdkDrop parent_instance;

  Window source_window;

  guint16 last_x;              /* Coordinates from last event */
  guint16 last_y;
  gulong timestamp;            /* Timestamp we claimed the DND selection with */
  guint version;               /* Xdnd protocol version */

  GdkDragAction xdnd_actions;  /* What is currently set in XdndActionList */
  GdkDragAction suggested_action;

  guint xdnd_targets_set  : 1; /* Whether we've already set XdndTypeList */
  guint xdnd_have_actions : 1; /* Whether an XdndActionList was provided */
  guint enter_emitted     : 1; /* Set after gdk_drop_emit_enter() */
};

struct _GdkX11DropClass
{
  GdkDropClass parent_class;
};

/* Forward declarations */

static gboolean        xdnd_source_surface_filter  (GdkDisplay   *display,
                                                    const XEvent *xevent,
                                                    gpointer      data);
static gboolean        xdnd_enter_filter    (GdkSurface   *surface,
                                             const XEvent *xevent);
static gboolean        xdnd_leave_filter    (GdkSurface   *surface,
                                             const XEvent *xevent);
static gboolean        xdnd_position_filter (GdkSurface   *surface,
                                             const XEvent *xevent);
static gboolean        xdnd_drop_filter     (GdkSurface   *surface,
                                             const XEvent *xevent);

static const struct {
  const char *atom_name;
  gboolean (* func) (GdkSurface *surface, const XEvent *event);
} xdnd_filters[] = {
  { "XdndEnter",                    xdnd_enter_filter },
  { "XdndLeave",                    xdnd_leave_filter },
  { "XdndPosition",                 xdnd_position_filter },
  { "XdndDrop",                     xdnd_drop_filter },
};

G_DEFINE_TYPE (GdkX11Drop, gdk_x11_drop, GDK_TYPE_DROP)

static GInputStream *
text_list_convert (GdkDisplay   *display,
                   GInputStream *stream,
                   const char   *encoding,
                   int           format)
{
  GInputStream *converter_stream;
  GConverter *converter;

  converter = gdk_x11_text_list_converter_to_utf8_new (display, encoding, format);
  converter_stream = g_converter_input_stream_new (stream, converter);

  g_object_unref (converter);
  g_object_unref (stream);

  return converter_stream;
}

static GInputStream *
no_convert (GdkDisplay   *display,
            GInputStream *stream,
            const char   *encoding,
            int           format)
{
  return stream;
}

static const struct {
  const char *x_target;
  const char *mime_type;
  GInputStream * (* convert) (GdkDisplay *, GInputStream *, const char *, int);
  const char *type;
  int format;
} special_targets[] = {
  { "UTF8_STRING",   "text/plain;charset=utf-8", no_convert,        "UTF8_STRING",   8 },
  { "COMPOUND_TEXT", "text/plain;charset=utf-8", text_list_convert, "COMPOUND_TEXT", 8 },
  { "TEXT",          "text/plain;charset=utf-8", text_list_convert, "STRING",        8 },
  { "STRING",        "text/plain;charset=utf-8", text_list_convert, "STRING",        8 },
  { "TARGETS",       NULL,                       NULL,              "ATOM",          32 },
  { "TIMESTAMP",     NULL,                       NULL,              "INTEGER",       32 },
  { "SAVE_TARGETS",  NULL,                       NULL,              "NULL",          32 }
};

static void
gdk_x11_drop_read_got_stream (GObject      *source,
                              GAsyncResult *res,
                              gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GInputStream *stream;
  const char *type;
  int format;
  
  stream = gdk_x11_selection_input_stream_new_finish (res, &type, &format, &error);
  if (stream == NULL)
    {
      GSList *targets, *next;
      
      targets = g_task_get_task_data (task);
      next = targets->next;
      if (next)
        {
          GdkDrop *drop = GDK_DROP (g_task_get_source_object (task));

          GDK_DISPLAY_DEBUG (gdk_drop_get_display (drop), DND,
                             "reading %s failed, trying %s next",
                             (char *) targets->data, (char *) next->data);
          targets->next = NULL;
          g_task_set_task_data (task, next, (GDestroyNotify) g_slist_free);
          gdk_x11_selection_input_stream_new_async (gdk_drop_get_display (drop),
                                                    "XdndSelection",
                                                    next->data,
                                                    CurrentTime,
                                                    g_task_get_priority (task),
                                                    g_task_get_cancellable (task),
                                                    gdk_x11_drop_read_got_stream,
                                                    task);
          g_error_free (error);
          return;
        }

      g_task_return_error (task, error);
    }
  else
    {
      gsize i;
      const char *mime_type = ((GSList *) g_task_get_task_data (task))->data;
      GdkDrop *drop = GDK_DROP (g_task_get_source_object (task));

      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (g_str_equal (mime_type, special_targets[i].x_target))
            {
              g_assert (special_targets[i].mime_type != NULL);

              mime_type = g_intern_string (special_targets[i].mime_type);
              g_task_set_task_data (task, g_slist_prepend (NULL, (gpointer) mime_type), (GDestroyNotify) g_slist_free);
              stream = special_targets[i].convert (gdk_drop_get_display (drop), stream, type, format);
              break;
            }
        }

      GDK_DISPLAY_DEBUG (gdk_drop_get_display (drop), DND,
                         "reading DND as %s now",
                         (const char *)((GSList *) g_task_get_task_data (task))->data);
      g_task_return_pointer (task, stream, g_object_unref);
    }

  g_object_unref (task);
}

static void
gdk_x11_drop_read_async (GdkDrop             *drop,
                         GdkContentFormats   *formats,
                         int                  io_priority,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GSList *targets;
  GTask *task;

  task = g_task_new (drop, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_x11_drop_read_async);

  targets = gdk_x11_clipboard_formats_to_targets (formats);
  g_task_set_task_data (task, targets, (GDestroyNotify) g_slist_free);
  if (targets == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      g_object_unref (task);
      return;
    }

  GDK_DISPLAY_DEBUG (gdk_drop_get_display (drop), DND,
                     "new read for %s (%u other options)",
                     (char *) targets->data, g_slist_length (targets->next));
  gdk_x11_selection_input_stream_new_async (gdk_drop_get_display (drop),
                                            "XdndSelection",
                                            targets->data,
                                            CurrentTime,
                                            io_priority,
                                            cancellable,
                                            gdk_x11_drop_read_got_stream,
                                            task);
}

static GInputStream *
gdk_x11_drop_read_finish (GdkDrop         *drop,
                          GAsyncResult    *result,
                          const char     **out_mime_type,
                          GError         **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (drop)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_x11_drop_read_async, NULL);

  if (out_mime_type)
    {
      GSList *targets;

      targets = g_task_get_task_data (task);
      *out_mime_type = targets ? targets->data : NULL;
    }

  return g_task_propagate_pointer (task, error);
}

static void
gdk_x11_drop_finalize (GObject *object)
{
  GdkX11Drop *drop_x11 = GDK_X11_DROP (object);

  if (gdk_drop_get_drag (GDK_DROP (drop_x11)) == NULL)
    {
      g_signal_handlers_disconnect_by_func (gdk_drop_get_display (GDK_DROP (drop_x11)),
                                            xdnd_source_surface_filter,
                                            drop_x11);
      /* Should we remove the GDK_PROPERTY_NOTIFY mask?
       * but we might want it for other reasons. (Like
       * INCR selection transactions).
       */
    }

  G_OBJECT_CLASS (gdk_x11_drop_parent_class)->finalize (object);
}

/* Utility functions */

static void
print_target_list (GdkContentFormats *formats)
{
  char *name = gdk_content_formats_to_string (formats);
  g_message ("DND formats: %s", name);
  g_free (name);
}

/*************************************************************
 ***************************** XDND **************************
 *************************************************************/

/* Utility functions */

static struct {
  const char *name;
  GdkDragAction action;
} xdnd_actions_table[] = {
    { "XdndActionCopy",    GDK_ACTION_COPY },
    { "XdndActionMove",    GDK_ACTION_MOVE },
    { "XdndActionLink",    GDK_ACTION_LINK },
    { "XdndActionAsk",     GDK_ACTION_ASK  },
    { "XdndActionPrivate", GDK_ACTION_COPY },
  };

static const int xdnd_n_actions = G_N_ELEMENTS (xdnd_actions_table);

static GdkDragAction
xdnd_action_from_atom (GdkDisplay *display,
                       Atom        xatom)
{
  const char *name;
  int i;

  if (xatom == None)
    return 0;

  name = gdk_x11_get_xatom_name_for_display (display, xatom);

  for (i = 0; i < xdnd_n_actions; i++)
    if (g_str_equal (name, xdnd_actions_table[i].name))
      return xdnd_actions_table[i].action;

  return 0;
}

static Atom
xdnd_action_to_atom (GdkDisplay    *display,
                     GdkDragAction  action)
{
  int i;

  for (i = 0; i < xdnd_n_actions; i++)
    if (action == xdnd_actions_table[i].action)
      return gdk_x11_get_xatom_by_name_for_display (display, xdnd_actions_table[i].name);

  return None;
}

/* Target side */

static void
gdk_x11_drop_update_actions (GdkX11Drop *drop_x11)
{
  GdkDragAction actions;

  if (!drop_x11->xdnd_have_actions)
    actions = drop_x11->suggested_action;
  else if (drop_x11->suggested_action & GDK_ACTION_ASK)
    actions = drop_x11->xdnd_actions | GDK_ACTION_ASK;
  else
    actions = drop_x11->xdnd_actions & GDK_ACTION_ALL;

  gdk_drop_set_actions (GDK_DROP (drop_x11), actions);
}

void
gdk_x11_drop_read_actions (GdkDrop *drop)
{
  GdkX11Drop *drop_x11 = GDK_X11_DROP (drop);
  GdkDisplay *display = gdk_drop_get_display (drop);
  GdkDrag *drag;
  GdkDragAction actions = GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK;
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  int i;

  drag = gdk_drop_get_drag (drop);

  drop_x11->xdnd_have_actions = FALSE;

  if (drag == NULL)
    {
      /* Get the XdndActionList, if set */

      gdk_x11_display_error_trap_push (display);
      if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                              drop_x11->source_window,
                              gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
                              0, 65536,
                              False, XA_ATOM, &type, &format, &nitems,
                              &after, &data) == Success &&
          type == XA_ATOM)
        {
          actions = 0;

          atoms = (Atom *)data;

          for (i = 0; i < nitems; i++)
            actions |= xdnd_action_from_atom (display, atoms[i]);

          drop_x11->xdnd_have_actions = TRUE;

          if (GDK_DISPLAY_DEBUG_CHECK (display, DND))
            {
              GString *action_str = g_string_new (NULL);
              GdkDragAction drop_actions = gdk_drop_get_actions (drop);
              if (drop_actions & GDK_ACTION_MOVE)
                g_string_append(action_str, "MOVE ");
              if (drop_actions & GDK_ACTION_COPY)
                g_string_append(action_str, "COPY ");
              if (drop_actions & GDK_ACTION_LINK)
                g_string_append(action_str, "LINK ");
              if (drop_actions & GDK_ACTION_ASK)
                g_string_append(action_str, "ASK ");

              g_message("Xdnd actions = %s", action_str->str);
              g_string_free (action_str, TRUE);
            }
        }

      if (data)
        XFree (data);

      gdk_x11_display_error_trap_pop_ignored (display);
    }
  else
    {
      actions = gdk_drag_get_actions (drag);
      drop_x11->xdnd_have_actions = TRUE;
    }

  drop_x11->xdnd_actions = actions;
  gdk_x11_drop_update_actions (drop_x11);
}

/* We have to make sure that the XdndActionList we keep internally
 * is up to date with the XdndActionList on the source window
 * because we get no notification, because Xdnd wasn’t meant
 * to continually send actions. So we select on PropertyChangeMask
 * and add this filter.
 */
static gboolean
xdnd_source_surface_filter (GdkDisplay   *display,
                            const XEvent *xevent,
                            gpointer      data)
{
  GdkX11Drop *drop_x11 = data;

  if ((xevent->xany.type == PropertyNotify) &&
      (xevent->xany.window == drop_x11->source_window) &&
      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList")))
    {
      gdk_x11_drop_read_actions (GDK_DROP (drop_x11));
    }

  return FALSE;
}

static gboolean
xdnd_enter_filter (GdkSurface   *surface,
                   const XEvent *xevent)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDrop *drop;
  GdkX11Drop *drop_x11;
  GdkDrag *drag;
  GdkSeat *seat;
  int i;
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;
  GdkContentFormats *content_formats;
  GPtrArray *formats;
  Window source_window;
  gboolean get_types;
  int version;

  source_window = xevent->xclient.data.l[0];
  get_types = ((xevent->xclient.data.l[1] & 1) != 0);
  version = (xevent->xclient.data.l[1] & 0xff000000) >> 24;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndEnter: source_window: %#lx, version: %#x",
                     source_window, version);

  if (version < 3)
    {
      /* Old source ignore */
      GDK_DISPLAY_DEBUG (display, DND, "Ignored old XdndEnter message");
      return TRUE;
    }

  if (display_x11->current_drop)
    {
      if (GDK_X11_DROP (display_x11->current_drop)->enter_emitted)
        gdk_drop_emit_leave_event (display_x11->current_drop, FALSE, GDK_CURRENT_TIME);
      g_clear_object (&display_x11->current_drop);
    }

  seat = gdk_display_get_default_seat (display);

  formats = g_ptr_array_new ();
  if (get_types)
    {
      gdk_x11_display_error_trap_push (display);
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                          source_window,
                          gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
                          0, 65536,
                          False, XA_ATOM, &type, &format, &nitems,
                          &after, &data);

      if (gdk_x11_display_error_trap_pop (display) || (format != 32) || (type != XA_ATOM))
        {
          if (data)
            XFree (data);

          return TRUE;
        }

      atoms = (Atom *)data;
      for (i = 0; i < nitems; i++)
        g_ptr_array_add (formats, 
                         (gpointer) gdk_x11_get_xatom_name_for_display (display, atoms[i]));

      XFree (atoms);
    }
  else
    {
      for (i = 0; i < 3; i++)
        if (xevent->xclient.data.l[2 + i])
          g_ptr_array_add (formats, 
                           (gpointer) gdk_x11_get_xatom_name_for_display (display,
                                                                          xevent->xclient.data.l[2 + i]));
    }
  content_formats = gdk_content_formats_new ((const char **) formats->pdata, formats->len);
  g_ptr_array_unref (formats);

  if (GDK_DISPLAY_DEBUG_CHECK (display, DND))
    print_target_list (content_formats);

  drag = gdk_x11_drag_find (display, source_window, GDK_SURFACE_XID (surface));

  drop_x11 = g_object_new (GDK_TYPE_X11_DROP,
                              "device", gdk_seat_get_pointer (seat),
                              "drag", drag,
                              "formats", content_formats,
                              "surface", surface,
                              NULL);
  drop = GDK_DROP (drop_x11);

  drop_x11->version = version;

  /* FIXME: Should extend DnD protocol to have device info */

  drop_x11->source_window = source_window;
  if (drag == NULL)
    {
      Display *xdisplay = gdk_x11_display_get_xdisplay (display);
      XWindowAttributes attrs;

      gdk_x11_display_error_trap_push (display);
      XGetWindowAttributes (xdisplay, source_window, &attrs);
      if (!(attrs.your_event_mask & PropertyChangeMask))
        {
          XSelectInput (xdisplay, source_window, attrs.your_event_mask | PropertyChangeMask);
        }
      gdk_x11_display_error_trap_pop_ignored (display);

      g_signal_connect (display, "xevent", G_CALLBACK (xdnd_source_surface_filter), drop);
    }
  gdk_x11_drop_read_actions (drop);

  display_x11->current_drop = drop;

  gdk_content_formats_unref (content_formats);

  return TRUE;
}

static gboolean
xdnd_leave_filter (GdkSurface   *surface,
                   const XEvent *xevent)
{
  Window source_window = xevent->xclient.data.l[0];
  GdkDisplay *display;
  GdkX11Display *display_x11;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndLeave: source_window: %#lx",
                     source_window);

  if ((display_x11->current_drop != NULL) &&
      (GDK_X11_DROP (display_x11->current_drop)->source_window == source_window))
    {
      if (GDK_X11_DROP (display_x11->current_drop)->enter_emitted)
        gdk_drop_emit_leave_event (display_x11->current_drop, FALSE, GDK_CURRENT_TIME);

      g_clear_object (&display_x11->current_drop);
    }

  return TRUE;
}

static gboolean
xdnd_position_filter (GdkSurface   *surface,
                      const XEvent *xevent)
{
  GdkX11Surface *impl;
  Window source_window = xevent->xclient.data.l[0];
  gint16 x_root = xevent->xclient.data.l[2] >> 16;
  gint16 y_root = xevent->xclient.data.l[2] & 0xffff;
  guint32 time = xevent->xclient.data.l[3];
  Atom action = xevent->xclient.data.l[4];
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDrop *drop;
  GdkX11Drop *drop_x11;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndPosition: source_window: %#lx position: (%d, %d)  time: %d  action: %ld",
                     source_window, x_root, y_root, time, action);

  drop = display_x11->current_drop;
  drop_x11 = GDK_X11_DROP (drop);

  if ((drop != NULL) &&
      (drop_x11->source_window == source_window))
    {
      surface = gdk_drop_get_surface (drop);
      impl = GDK_X11_SURFACE (surface);

      drop_x11->suggested_action = xdnd_action_from_atom (display, action);
      gdk_x11_drop_update_actions (drop_x11);

      drop_x11->last_x = x_root / impl->surface_scale;
      drop_x11->last_y = y_root / impl->surface_scale;

      if (drop_x11->enter_emitted)
        {
          gdk_drop_emit_motion_event (drop, FALSE, drop_x11->last_x - impl->abs_x, drop_x11->last_y - impl->abs_y, time);
        }
      else
        {
          gdk_drop_emit_enter_event (drop, FALSE, drop_x11->last_x - impl->abs_x, drop_x11->last_y - impl->abs_y, time);
          drop_x11->enter_emitted = TRUE;
        }
    }

  return TRUE;
}

static gboolean
xdnd_drop_filter (GdkSurface   *surface,
                  const XEvent *xevent)
{
  Window source_window = xevent->xclient.data.l[0];
  guint32 time = xevent->xclient.data.l[2];
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkDrop *drop;
  GdkX11Drop *drop_x11;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndDrop: source_window: %#lx  time: %d",
                     source_window, time);

  drop = display_x11->current_drop;
  drop_x11 = GDK_X11_DROP (drop);

  if ((drop != NULL) &&
      (drop_x11->source_window == source_window))
    {
      GdkSurface *s = gdk_drop_get_surface (drop);
      GdkX11Surface *si = GDK_X11_SURFACE (s);
      gdk_x11_surface_set_user_time (s, time);

      gdk_drop_emit_drop_event (drop, FALSE, drop_x11->last_x - si->abs_x, drop_x11->last_y - si->abs_y, time);
    }

  return TRUE;
}

gboolean
gdk_x11_drop_filter (GdkSurface   *surface,
                     const XEvent *xevent)
                     
{
  GdkDisplay *display;
  int i;

  if (!GDK_IS_X11_SURFACE (surface))
    return GDK_FILTER_CONTINUE;

  if (xevent->type != ClientMessage)
    return GDK_FILTER_CONTINUE;

  display = GDK_SURFACE_DISPLAY (surface);

  for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
    {
      if (xevent->xclient.message_type != gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name))
        continue;

      if (xdnd_filters[i].func (surface, xevent))
        return TRUE;
      else
        return FALSE;
    }

  return FALSE;
}

/* Destination side */

static void
gdk_x11_drop_do_nothing (Window   window,
                         gboolean success,
                         gpointer data)
{
  if (!success)
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (data), DND, "Send event to %lx failed", window);
    }
}

static void
gdk_x11_drop_status (GdkDrop       *drop,
                     GdkDragAction  actions,
                     GdkDragAction  preferred)
{
  GdkX11Drop *drop_x11 = GDK_X11_DROP (drop);
  GdkDragAction possible_actions, suggested_action;
  XEvent xev;
  GdkDisplay *display;

  display = gdk_drop_get_display (drop);

  possible_actions = actions & gdk_drop_get_actions (drop);

  if (preferred & possible_actions)
    suggested_action = preferred;
  else if (drop_x11->suggested_action & possible_actions)
    suggested_action = drop_x11->suggested_action;
  else if (possible_actions & GDK_ACTION_COPY)
    suggested_action = GDK_ACTION_COPY;
  else if (possible_actions & GDK_ACTION_MOVE)
    suggested_action = GDK_ACTION_MOVE;
  else if (possible_actions & GDK_ACTION_ASK)
    suggested_action = GDK_ACTION_ASK;
  else
    suggested_action = 0;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndStatus");
  xev.xclient.format = 32;
  xev.xclient.window = drop_x11->source_window;

  xev.xclient.data.l[0] = GDK_SURFACE_XID (gdk_drop_get_surface (drop));
  xev.xclient.data.l[1] = (possible_actions != 0) ? (2 | 1) : 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = xdnd_action_to_atom (display, suggested_action);

  if (gdk_drop_get_drag (drop))
    {
      gdk_x11_drag_handle_status (display, &xev);
    }
  else
    {
      _gdk_x11_send_client_message_async (display,
                                          drop_x11->source_window,
                                          FALSE, 0,
                                          &xev.xclient,
                                          gdk_x11_drop_do_nothing,
                                          display);
    }
}

static void
gdk_x11_drop_finish (GdkDrop       *drop,
                     GdkDragAction  action)
{
  GdkX11Drop *drop_x11 = GDK_X11_DROP (drop);
  GdkDisplay *display = gdk_drop_get_display (drop);
  XEvent xev;

  if (action == GDK_ACTION_MOVE)
    {
      XConvertSelection (GDK_DISPLAY_XDISPLAY (display),
                         gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection"),
                         gdk_x11_get_xatom_by_name_for_display (display, "DELETE"),
                         gdk_x11_get_xatom_by_name_for_display (display, "GDK_SELECTION"),
                         drop_x11->source_window,
                         GDK_X11_DROP (drop)->timestamp);
      /* XXX: Do we need to wait for a reply here before sending the next message? */
    }

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndFinished");
  xev.xclient.format = 32;
  xev.xclient.window = drop_x11->source_window;

  xev.xclient.data.l[0] = GDK_SURFACE_XID (gdk_drop_get_surface (drop));
  if (action != 0)
    {
      xev.xclient.data.l[1] = 1;
      xev.xclient.data.l[2] = xdnd_action_to_atom (display, action);
    }
  else
    {
      xev.xclient.data.l[1] = 0;
      xev.xclient.data.l[2] = None;
    }
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (gdk_drop_get_drag (drop))
    {
      gdk_x11_drag_handle_finished (display, &xev);
    }
  else
    {
      _gdk_x11_send_client_message_async (display,
                                          drop_x11->source_window,
                                          FALSE, 0,
                                          &xev.xclient,
                                          gdk_x11_drop_do_nothing,
                                          display);
    }
}

static void
gdk_x11_drop_class_init (GdkX11DropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);

  object_class->finalize = gdk_x11_drop_finalize;

  drop_class->status = gdk_x11_drop_status;
  drop_class->finish = gdk_x11_drop_finish;
  drop_class->read_async = gdk_x11_drop_read_async;
  drop_class->read_finish = gdk_x11_drop_read_finish;
}

static void
gdk_x11_drop_init (GdkX11Drop *drag)
{
}

