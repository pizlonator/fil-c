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

#include "gdkx11dnd.h"

#include "gdkprivate.h"
#include "gdkasync.h"
#include "gdkclipboardprivate.h"
#include "gdkclipboard-x11.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-xi2-private.h"
#include "gdkdisplay-x11.h"
#include "gdkdragprivate.h"
#include "gdkeventsprivate.h"
#include "gdksurfaceprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkseatprivate.h"
#include "gdkselectioninputstream-x11.h"
#include "gdkselectionoutputstream-x11.h"

#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <string.h>

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

/*
 * GdkDragProtocol:
 * @GDK_DRAG_PROTO_NONE: no protocol.
 * @GDK_DRAG_PROTO_XDND: The Xdnd protocol.
 * @GDK_DRAG_PROTO_ROOTWIN: An extension to the Xdnd protocol for
 *  unclaimed root window drops.
 *
 * Used in `GdkDrag` to indicate the protocol according to
 * which DND is done.
 */
typedef enum
{
  GDK_DRAG_PROTO_NONE = 0,
  GDK_DRAG_PROTO_XDND,
  GDK_DRAG_PROTO_ROOTWIN
} GdkDragProtocol;

typedef struct {
  guint32 xid;
  int x, y, width, height;
  gboolean mapped;
  gboolean shape_selected;
  gboolean shape_valid;
  cairo_region_t *shape;
} GdkCacheChild;

struct _GdkSurfaceCache {
  GList *children;
  GHashTable *child_hash;
  guint old_event_mask;
  GdkDisplay *display;
  int ref_count;
};


struct _GdkX11Drag
{
  GdkDrag drag;

  GdkDragProtocol protocol;

  int start_x;                /* Where the drag started */
  int start_y;
  guint16 last_x;              /* Coordinates from last event */
  guint16 last_y;
  gulong timestamp;            /* Timestamp we claimed the DND selection with */
  GdkDragAction xdnd_actions;  /* What is currently set in XdndActionList */
  guint version;               /* Xdnd protocol version */

  GdkSurfaceCache *cache;

  GdkSurface *drag_surface;

  GdkSurface *ipc_surface;
  GdkCursor *cursor;
  GdkSeat *grab_seat;
  GdkDragAction actions;
  GdkDragAction current_action;

  int hot_x;
  int hot_y;

  Window dest_xid;             /* The last window we looked up */
  Window proxy_xid;            /* The proxy window for dest_xid (or dest_xid if no proxying happens) */
  Window drop_xid;             /* The (non-proxied) window that is receiving drops */
  guint xdnd_targets_set  : 1; /* Whether we've already set XdndTypeList */
  guint xdnd_have_actions : 1; /* Whether an XdndActionList was provided */
  guint drag_status       : 4; /* current status of drag */
  guint drop_failed       : 1; /* Whether the drop was unsuccessful */
};

struct _GdkX11DragClass
{
  GdkDragClass parent_class;
};

/* Forward declarations */

static GdkSurfaceCache *gdk_surface_cache_ref   (GdkSurfaceCache *cache);
static void            gdk_surface_cache_unref (GdkSurfaceCache *cache);

gboolean gdk_x11_drag_handle_event   (GdkDrag        *drag,
                                      GdkEvent       *event);

static GList *drags;
static GSList *window_caches;

G_DEFINE_TYPE (GdkX11Drag, gdk_x11_drag, GDK_TYPE_DRAG)

static void
gdk_x11_drag_init (GdkX11Drag *drag)
{
  drags = g_list_prepend (drags, drag);
}

static void        gdk_x11_drag_finalize     (GObject          *object);
static Window      gdk_x11_drag_find_surface (GdkDrag          *drag,
                                              GdkSurface       *drag_surface,
                                              int              x_root,
                                              int              y_root,
                                              GdkDragProtocol *protocol);
static gboolean    gdk_x11_drag_drag_motion  (GdkDrag         *drag,
                                              Window           proxy_xid,
                                              GdkDragProtocol  protocol,
                                              int              x_root,
                                              int              y_root,
                                              GdkDragAction    suggested_action,
                                              GdkDragAction    possible_actions,
                                              guint32          time);
static void        gdk_x11_drag_drop         (GdkDrag         *drag,
                                              guint32          time_);
static GdkSurface * gdk_x11_drag_get_drag_surface (GdkDrag    *drag);
static void        gdk_x11_drag_set_hotspot  (GdkDrag         *drag,
                                              int              hot_x,
                                              int              hot_y);
static void        gdk_x11_drag_drop_done    (GdkDrag         *drag,
                                              gboolean         success);
static void        gdk_x11_drag_set_cursor   (GdkDrag *drag,
                                              GdkCursor      *cursor);
static void        gdk_x11_drag_cancel       (GdkDrag             *drag,
                                              GdkDragCancelReason  reason);
static void        gdk_x11_drag_drop_performed (GdkDrag           *drag,
                                                guint32            time);

static void
gdk_x11_drag_class_init (GdkX11DragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_x11_drag_finalize;

  drag_class->get_drag_surface = gdk_x11_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_x11_drag_set_hotspot;
  drag_class->drop_done = gdk_x11_drag_drop_done;
  drag_class->set_cursor = gdk_x11_drag_set_cursor;
  drag_class->cancel = gdk_x11_drag_cancel;
  drag_class->drop_performed = gdk_x11_drag_drop_performed;
  drag_class->handle_event = gdk_x11_drag_handle_event;
}

static void
gdk_x11_drag_finalize (GObject *object)
{
  GdkDrag *drag = GDK_DRAG (object);
  GdkX11Drag *x11_drag = GDK_X11_DRAG (object);
  GdkSurface *drag_surface, *ipc_surface;

  if (x11_drag->cache)
    gdk_surface_cache_unref (x11_drag->cache);

  drags = g_list_remove (drags, drag);

  drag_surface = x11_drag->drag_surface;
  ipc_surface = x11_drag->ipc_surface;

  G_OBJECT_CLASS (gdk_x11_drag_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (drag_surface);
  if (ipc_surface)
    gdk_surface_destroy (ipc_surface);
}

/* Drag Contexts */

GdkDrag *
gdk_x11_drag_find (GdkDisplay *display,
                   Window      source_xid,
                   Window      dest_xid)
{
  GList *tmp_list;
  GdkDrag *drag;
  GdkX11Drag *drag_x11;
  Window drag_dest_xid;
  GdkSurface *surface;
  Window surface_xid;

  for (tmp_list = drags; tmp_list; tmp_list = tmp_list->next)
    {
      drag = (GdkDrag *)tmp_list->data;
      drag_x11 = (GdkX11Drag *)drag;

      if (gdk_drag_get_display (drag) != display)
        continue;

      g_object_get (drag, "surface", &surface, NULL);
      surface_xid = surface ? GDK_SURFACE_XID (surface) : None;
      g_object_unref (surface);

      drag_dest_xid = drag_x11->proxy_xid
                            ? (drag_x11->drop_xid
                                  ? drag_x11->drop_xid
                                  : drag_x11->proxy_xid)
                            : None;

      if (((source_xid == None) || (surface && (surface_xid == source_xid))) &&
          ((dest_xid == None) || (drag_dest_xid == dest_xid)))
        return drag;
    }

  return NULL;
}

static void
precache_target_list (GdkDrag *drag)
{
  GdkContentFormats *formats;
  const char * const *atoms;
  gsize n_atoms;

  formats = gdk_content_formats_ref (gdk_drag_get_formats (drag));
  formats = gdk_content_formats_union_serialize_mime_types (formats);

  atoms = gdk_content_formats_get_mime_types (formats, &n_atoms);

  _gdk_x11_precache_atoms (gdk_drag_get_display (drag),
                           (const char **) atoms,
                           n_atoms);

  gdk_content_formats_unref (formats);
}

/* Utility functions */

static void
free_cache_child (GdkCacheChild *child,
                  GdkDisplay    *display)
{
  if (child->shape)
    cairo_region_destroy (child->shape);

  if (child->shape_selected && display)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, 0);
    }

  g_free (child);
}

static void
gdk_surface_cache_add (GdkSurfaceCache *cache,
                       guint32          xid,
                       int              x,
                       int              y,
                       int              width,
                       int              height,
                       gboolean         mapped)
{ 
  GdkCacheChild *child = g_new (GdkCacheChild, 1);

  child->xid = xid;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;
  child->mapped = mapped;
  child->shape_selected = FALSE;
  child->shape_valid = FALSE;
  child->shape = NULL;

  cache->children = g_list_prepend (cache->children, child);
  g_hash_table_insert (cache->child_hash, GUINT_TO_POINTER (xid),
                       cache->children);
}

GdkFilterReturn
gdk_surface_cache_shape_filter (const XEvent *xevent,
                                gpointer      data)
{
  GdkSurfaceCache *cache = data;

  GdkX11Display *display = GDK_X11_DISPLAY (cache->display);

  if (display->have_shapes &&
      xevent->type == display->shape_event_base + ShapeNotify)
    {
      XShapeEvent *xse = (XShapeEvent*)xevent;
      GList *node;

      node = g_hash_table_lookup (cache->child_hash,
                                  GUINT_TO_POINTER (xse->window));
      if (node)
        {
          GdkCacheChild *child = node->data;
          child->shape_valid = FALSE;
          if (child->shape)
            {
              cairo_region_destroy (child->shape);
              child->shape = NULL;
            }
        }

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

GdkFilterReturn
gdk_surface_cache_filter (const XEvent *xevent,
                          gpointer      data)
{
  GdkSurfaceCache *cache = data;

  switch (xevent->type)
    {
    case CirculateNotify:
      break;
    case ConfigureNotify:
      {
        const XConfigureEvent *xce = &xevent->xconfigure;
        GList *node;

        node = g_hash_table_lookup (cache->child_hash,
                                    GUINT_TO_POINTER (xce->window));
        if (node)
          {
            GdkCacheChild *child = node->data;
            child->x = xce->x;
            child->y = xce->y;
            child->width = xce->width;
            child->height = xce->height;
            if (xce->above == None && (node->next))
              {
                GList *last = g_list_last (cache->children);
                cache->children = g_list_remove_link (cache->children, node);
                last->next = node;
                node->next = NULL;
                node->prev = last;
              }
            else
              {
                GList *above_node = g_hash_table_lookup (cache->child_hash,
                                                         GUINT_TO_POINTER (xce->above));
                if (above_node && node->next != above_node)
                  {
                    /* Put the window above (before in the list) above_node */
                    cache->children = g_list_remove_link (cache->children, node);
                    node->prev = above_node->prev;
                    if (node->prev)
                      node->prev->next = node;
                    else
                      cache->children = node;
                    node->next = above_node;
                    above_node->prev = node;
                  }
              }
          }
        break;
      }
    case CreateNotify:
      {
        const XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

        if (!g_hash_table_lookup (cache->child_hash,
                                  GUINT_TO_POINTER (xcwe->window)))
          gdk_surface_cache_add (cache, xcwe->window,
                                xcwe->x, xcwe->y, xcwe->width, xcwe->height,
                                FALSE);
        break;
      }
    case DestroyNotify:
      {
        const XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;
        GList *node;

        node = g_hash_table_lookup (cache->child_hash,
                                    GUINT_TO_POINTER (xdwe->window));
        if (node)
          {
            GdkCacheChild *child = node->data;

            g_hash_table_remove (cache->child_hash,
                                 GUINT_TO_POINTER (xdwe->window));
            cache->children = g_list_remove_link (cache->children, node);
            /* window is destroyed, no need to disable ShapeNotify */
            free_cache_child (child, NULL);
            g_list_free_1 (node);
          }
        break;
      }
    case MapNotify:
      {
        const XMapEvent *xme = &xevent->xmap;
        GList *node;

        node = g_hash_table_lookup (cache->child_hash,
                                    GUINT_TO_POINTER (xme->window));
        if (node)
          {
            GdkCacheChild *child = node->data;
            child->mapped = TRUE;
          }
        break;
      }
    case ReparentNotify:
      break;
    case UnmapNotify:
      {
        const XMapEvent *xume = &xevent->xmap;
        GList *node;

        node = g_hash_table_lookup (cache->child_hash,
                                    GUINT_TO_POINTER (xume->window));
        if (node)
          {
            GdkCacheChild *child = node->data;
            child->mapped = FALSE;
          }
        break;
      }
    default:
      return GDK_FILTER_CONTINUE;
    }
  return GDK_FILTER_REMOVE;
}

static GdkSurfaceCache *
gdk_surface_cache_new (GdkDisplay *display)
{
  XWindowAttributes xwa;
  GdkX11Screen *screen = GDK_X11_DISPLAY (display)->screen;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  Window xroot_window = GDK_DISPLAY_XROOTWIN (display);
  GdkChildInfoX11 *children;
  guint nchildren, i;

  GdkSurfaceCache *result = g_new (GdkSurfaceCache, 1);

  result->children = NULL;
  result->child_hash = g_hash_table_new (g_direct_hash, NULL);
  result->display = display;
  result->ref_count = 1;

  XGetWindowAttributes (xdisplay, xroot_window, &xwa);
  result->old_event_mask = xwa.your_event_mask;

  if (G_UNLIKELY (!GDK_X11_DISPLAY (display)->trusted_client))
    {
      GList *toplevel_windows, *list;
      GdkSurface *surface;
      GdkX11Surface *impl;
      int x, y, width, height;

      toplevel_windows = gdk_x11_display_get_toplevel_windows (display);
      for (list = toplevel_windows; list; list = list->next)
        {
          surface = GDK_SURFACE (list->data);
	  impl = GDK_X11_SURFACE (surface);
          gdk_surface_get_geometry (surface, &x, &y, &width, &height);
          gdk_surface_cache_add (result, GDK_SURFACE_XID (surface),
                                x * impl->surface_scale, y * impl->surface_scale, 
				width * impl->surface_scale, 
				height * impl->surface_scale,
                                gdk_surface_get_mapped (surface));
        }
      return result;
    }

  XSelectInput (xdisplay, xroot_window, result->old_event_mask | SubstructureNotifyMask);

  if (!_gdk_x11_get_window_child_info (display,
                                       xroot_window,
                                       FALSE, NULL,
                                       &children, &nchildren))
    return result;

  for (i = 0; i < nchildren ; i++)
    {
      gdk_surface_cache_add (result, children[i].window,
                            children[i].x, children[i].y, children[i].width, children[i].height,
                            children[i].is_mapped);
    }

  g_free (children);

  return result;
}

static void
gdk_surface_cache_destroy (GdkSurfaceCache *cache)
{
  XSelectInput (GDK_DISPLAY_XDISPLAY (cache->display),
                GDK_DISPLAY_XROOTWIN (cache->display),
                cache->old_event_mask);

  gdk_x11_display_error_trap_push (cache->display);
  g_list_foreach (cache->children, (GFunc)free_cache_child, cache->display);
  gdk_x11_display_error_trap_pop_ignored (cache->display);

  g_list_free (cache->children);
  g_hash_table_destroy (cache->child_hash);

  g_free (cache);
}

static GdkSurfaceCache *
gdk_surface_cache_ref (GdkSurfaceCache *cache)
{
  cache->ref_count += 1;

  return cache;
}

static void
gdk_surface_cache_unref (GdkSurfaceCache *cache)
{
  g_assert (cache->ref_count > 0);

  cache->ref_count -= 1;

  if (cache->ref_count == 0)
    {
      window_caches = g_slist_remove (window_caches, cache);
      gdk_surface_cache_destroy (cache);
    }
}

GdkSurfaceCache *
gdk_surface_cache_get (GdkDisplay *display)
{
  GSList *list;
  GdkSurfaceCache *cache;

  for (list = window_caches; list; list = list->next)
    {
      cache = list->data;
      if (cache->display == display)
        return gdk_surface_cache_ref (cache);
    }

  cache = gdk_surface_cache_new (display);

  window_caches = g_slist_prepend (window_caches, cache);

  return cache;
}

static gboolean
is_pointer_within_shape (GdkDisplay    *display,
                         GdkCacheChild *child,
                         int            x_pos,
                         int            y_pos)
{
  if (!child->shape_selected)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, ShapeNotifyMask);
      child->shape_selected = TRUE;
    }
  if (!child->shape_valid)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
      cairo_region_t *input_shape;

      child->shape = NULL;
      if (display_x11->have_shapes)
        child->shape = _gdk_x11_xwindow_get_shape (display_x11->xdisplay,
                                                   child->xid, 1,  ShapeBounding);
#ifdef ShapeInput
      input_shape = NULL;
      if (display_x11->have_input_shapes)
        input_shape = _gdk_x11_xwindow_get_shape (display_x11->xdisplay,
                                                  child->xid, 1, ShapeInput);

      if (child->shape && input_shape)
        {
          cairo_region_intersect (child->shape, input_shape);
          cairo_region_destroy (input_shape);
        }
      else if (input_shape)
        {
          child->shape = input_shape;
        }
#endif

      child->shape_valid = TRUE;
    }

  return child->shape == NULL ||
         cairo_region_contains_point (child->shape, x_pos, y_pos);
}

static Window
get_client_window_at_coords_recurse (GdkDisplay *display,
                                     Window      win,
                                     gboolean    is_toplevel,
                                     int         x,
                                     int         y)
{
  GdkChildInfoX11 *children;
  unsigned int nchildren;
  int i;
  gboolean found_child = FALSE;
  GdkChildInfoX11 child = { 0, };
  gboolean has_wm_state = FALSE;

  if (!_gdk_x11_get_window_child_info (display, win, TRUE,
                                       is_toplevel? &has_wm_state : NULL,
                                       &children, &nchildren))
    return None;

  if (has_wm_state)
    {
      g_free (children);

      return win;
    }

  for (i = nchildren - 1; (i >= 0) && !found_child; i--)
    {
      GdkChildInfoX11 *cur_child = &children[i];

      if ((cur_child->is_mapped) && (cur_child->window_class == InputOutput) &&
          (x >= cur_child->x) && (x < cur_child->x + cur_child->width) &&
          (y >= cur_child->y) && (y < cur_child->y + cur_child->height))
        {
          x -= cur_child->x;
          y -= cur_child->y;
          child = *cur_child;
          found_child = TRUE;
        }
    }

  g_free (children);

  if (found_child)
    {
      if (child.has_wm_state)
        return child.window;
      else
        return get_client_window_at_coords_recurse (display, child.window, FALSE, x, y);
    }
  else
    return None;
}

static Window
get_client_window_at_coords (GdkSurfaceCache *cache,
                             Window          ignore,
                             int             x_root,
                             int             y_root)
{
  GList *tmp_list;
  Window retval = None;
  GdkDisplay *display;

  display = cache->display;

  gdk_x11_display_error_trap_push (display);

  tmp_list = cache->children;

  while (tmp_list && !retval)
    {
      GdkCacheChild *child = tmp_list->data;

      if ((child->xid != ignore) && (child->mapped))
        {
          if ((x_root >= child->x) && (x_root < child->x + child->width) &&
              (y_root >= child->y) && (y_root < child->y + child->height))
            {
              if (!is_pointer_within_shape (display, child,
                                            x_root - child->x,
                                            y_root - child->y))
                {
                  tmp_list = tmp_list->next;
                  continue;
                }

              retval = get_client_window_at_coords_recurse (display,
                  child->xid, TRUE,
                  x_root - child->x,
                  y_root - child->y);
              if (!retval)
                retval = child->xid;
            }
        }
      tmp_list = tmp_list->next;
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  if (retval)
    return retval;
  else
    return GDK_DISPLAY_XROOTWIN (display);
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

/* Source side */

void
gdk_x11_drag_handle_status (GdkDisplay   *display,
                            const XEvent *xevent)
{
  guint32 dest_surface = xevent->xclient.data.l[0];
  guint32 flags = xevent->xclient.data.l[1];
  Atom action = xevent->xclient.data.l[4];
  GdkDrag *drag;

  drag = gdk_x11_drag_find (display, xevent->xclient.window, dest_surface);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndStatus: dest_surface: %#x  action: %ld",
                     dest_surface, action);

  if (drag)
    {
      GdkX11Drag *drag_x11 = GDK_X11_DRAG (drag);
      if (drag_x11->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
        drag_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      if (!(action != 0) != !(flags & 1))
        {
          GDK_DISPLAY_DEBUG (display, DND,
                             "Received status event with flags not corresponding to action!");
          action = 0;
        }

      gdk_drag_set_selected_action (drag, xdnd_action_from_atom (display, action));
      drag_x11->current_action = action;
    }
}

void
gdk_x11_drag_handle_finished (GdkDisplay   *display,
                              const XEvent *xevent)
{
  guint32 dest_surface = xevent->xclient.data.l[0];
  GdkDrag *drag;
  GdkX11Drag *drag_x11;

  drag = gdk_x11_drag_find (display, xevent->xclient.window, dest_surface);

  GDK_DISPLAY_DEBUG (display, DND,
                     "XdndFinished: dest_surface: %#x", dest_surface);

  if (drag)
    {
      drag_x11 = GDK_X11_DRAG (drag);
      if (drag_x11->version == 5)
        drag_x11->drop_failed = xevent->xclient.data.l[1] == 0;

      g_object_ref (drag);
      g_signal_emit_by_name (drag, "dnd-finished");
      gdk_drag_drop_done (drag, !drag_x11->drop_failed);
      g_object_unref (drag);
    }
}

static void
xdnd_set_targets (GdkX11Drag *drag_x11)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  Atom *atomlist;
  const char * const *atoms;
  gsize i, n_atoms;
  GdkDisplay *display = gdk_drag_get_display (drag);
  GdkContentFormats *formats;

  formats = gdk_content_formats_ref (gdk_drag_get_formats (drag));
  formats = gdk_content_formats_union_serialize_mime_types (formats);

  atoms = gdk_content_formats_get_mime_types (formats, &n_atoms);
  atomlist = g_new (Atom, n_atoms);
  for (i = 0; i < n_atoms; i++)
    atomlist[i] = gdk_x11_get_xatom_by_name_for_display (display, atoms[i]);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (drag_x11->ipc_surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  drag_x11->xdnd_targets_set = 1;

  gdk_content_formats_unref (formats);
}

static void
xdnd_set_actions (GdkX11Drag *drag_x11)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  Atom *atomlist;
  int i;
  int n_atoms;
  guint actions;
  GdkDisplay *display = gdk_drag_get_display (drag);

  actions = gdk_drag_get_actions (drag);
  n_atoms = 0;
  for (i = 0; i < xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
        {
          actions &= ~xdnd_actions_table[i].action;
          n_atoms++;
        }
    }

  atomlist = g_new (Atom, n_atoms);

  actions = gdk_drag_get_actions (drag);
  n_atoms = 0;
  for (i = 0; i < xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
        {
          actions &= ~xdnd_actions_table[i].action;
          atomlist[n_atoms] = gdk_x11_get_xatom_by_name_for_display (display, xdnd_actions_table[i].name);
          n_atoms++;
        }
    }

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (drag_x11->ipc_surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  drag_x11->xdnd_actions = gdk_drag_get_actions (drag);
}

static void
send_client_message_async_cb (Window   window,
                              gboolean success,
                              gpointer data)
{
  GdkX11Drag *drag_x11 = data;
  GdkDrag *drag = data;

  GDK_DISPLAY_DEBUG (gdk_drag_get_display (drag), DND,
                     "Got async callback for #%lx, success = %d",
                     window, success);

  /* On failure, we immediately continue with the protocol
   * so we don't end up blocking for a timeout
   */
  if (!success &&
      window == drag_x11->proxy_xid)
    {
      drag_x11->proxy_xid = None;
      gdk_drag_set_selected_action (drag, 0);
      drag_x11->current_action = 0;
      drag_x11->drag_status = GDK_DRAG_STATUS_DRAG;
    }

  g_object_unref (drag);
}

static void
send_client_message_async (GdkDrag      *drag,
                           Window               window,
                           glong                event_mask,
                           XClientMessageEvent *event_send)
{
  GdkDisplay *display = gdk_drag_get_display (drag);

  g_object_ref (drag);

  _gdk_x11_send_client_message_async (display, window,
                                      FALSE, event_mask, event_send,
                                      send_client_message_async_cb, drag);
}

static void
xdnd_send_xevent (GdkX11Drag *drag_x11,
                  XEvent     *event_send)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  GdkDisplay *display = gdk_drag_get_display (drag);
  GdkSurface *surface;
  glong event_mask;

  g_assert (event_send->xany.type == ClientMessage);

  /* We short-circuit messages to ourselves */
  surface = gdk_x11_surface_lookup_for_display (display, drag_x11->proxy_xid);
  if (surface)
    {
      if (gdk_x11_drop_filter (surface, event_send))
        return;
    }

  if (_gdk_x11_display_is_root_window (display, drag_x11->proxy_xid))
    event_mask = ButtonPressMask;
  else
    event_mask = 0;

  send_client_message_async (drag, drag_x11->proxy_xid, event_mask,
                             &event_send->xclient);
}

static void
xdnd_send_enter (GdkX11Drag *drag_x11)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  GdkDisplay *display = gdk_drag_get_display (drag);
  GdkContentFormats *formats;
  const char * const *mime_types;
  gsize i, n_mime_types;
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndEnter");
  xev.xclient.format = 32;
  xev.xclient.window = drag_x11->drop_xid
                           ? drag_x11->drop_xid
                           : drag_x11->proxy_xid;
  xev.xclient.data.l[0] = GDK_SURFACE_XID (drag_x11->ipc_surface);
  xev.xclient.data.l[1] = (drag_x11->version << 24); /* version */
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  GDK_DISPLAY_DEBUG (display, DND,
                     "Sending enter source window %#lx XDND protocol version %d\n",
                     GDK_SURFACE_XID (drag_x11->ipc_surface), drag_x11->version);
  formats = gdk_content_formats_ref (gdk_drag_get_formats (drag));
  formats = gdk_content_formats_union_serialize_mime_types (formats);

  mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);

  if (n_mime_types > 3)
    {
      if (!drag_x11->xdnd_targets_set)
        xdnd_set_targets (drag_x11);
      xev.xclient.data.l[1] |= 1;
    }
  else
    {
      for (i = 0; i < n_mime_types; i++)
        {
          xev.xclient.data.l[i + 2] = gdk_x11_get_xatom_by_name_for_display (display, mime_types[i]);
        }
    }

  xdnd_send_xevent (drag_x11, &xev);

  gdk_content_formats_unref (formats);
}

static void
xdnd_send_leave (GdkX11Drag *drag_x11)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  GdkDisplay *display = gdk_drag_get_display (drag);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndLeave");
  xev.xclient.format = 32;
  xev.xclient.window = drag_x11->drop_xid
                           ? drag_x11->drop_xid
                           : drag_x11->proxy_xid;
  xev.xclient.data.l[0] = GDK_SURFACE_XID (drag_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  xdnd_send_xevent (drag_x11, &xev);
}

static void
xdnd_send_drop (GdkX11Drag *drag_x11,
                guint32            time)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  GdkDisplay *display = gdk_drag_get_display (drag);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndDrop");
  xev.xclient.format = 32;
  xev.xclient.window = drag_x11->drop_xid
                           ? drag_x11->drop_xid
                           : drag_x11->proxy_xid;
  xev.xclient.data.l[0] = GDK_SURFACE_XID (drag_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = time;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  xdnd_send_xevent (drag_x11, &xev);
}

static void
xdnd_send_motion (GdkX11Drag *drag_x11,
                  int                x_root,
                  int                y_root,
                  GdkDragAction      action,
                  guint32            time)
{
  GdkDrag *drag = GDK_DRAG (drag_x11);
  GdkDisplay *display = gdk_drag_get_display (drag);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndPosition");
  xev.xclient.format = 32;
  xev.xclient.window = drag_x11->drop_xid
                           ? drag_x11->drop_xid
                           : drag_x11->proxy_xid;
  xev.xclient.data.l[0] = GDK_SURFACE_XID (drag_x11->ipc_surface);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (x_root << 16) | y_root;
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);

  xdnd_send_xevent (drag_x11, &xev);
  drag_x11->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
}

static guint32
xdnd_check_dest (GdkDisplay *display,
                 Window      win,
                 guint      *xdnd_version)
{
  gboolean retval = FALSE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  guchar *data;
  Atom *version;
  Window *proxy_data;
  Window proxy;
  Atom xdnd_proxy_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndProxy");
  Atom xdnd_aware_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndAware");

  proxy = None;

  gdk_x11_display_error_trap_push (display);
  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), win,
                          xdnd_proxy_atom, 0,
                          1, False, AnyPropertyType,
                          &type, &format, &nitems, &after,
                          &data) == Success)
    {
      if (type != None)
        {
          proxy_data = (Window *)data;

          if ((format == 32) && (nitems == 1))
            {
              proxy = *proxy_data;
            }
          else
            {
              GDK_DISPLAY_DEBUG (display, DND,
                                 "Invalid XdndProxy property on window %ld", win);
            }

          XFree (proxy_data);
        }

      if ((XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), proxy ? proxy : win,
                               xdnd_aware_atom, 0,
                               1, False, AnyPropertyType,
                               &type, &format, &nitems, &after,
                               &data) == Success) &&
          type != None)
        {
          version = (Atom *)data;

          if ((format == 32) && (nitems == 1))
            {
              if (*version >= 3)
                retval = TRUE;
              if (xdnd_version)
                *xdnd_version = *version;
            }
          else
            {
              GDK_DISPLAY_DEBUG (display, DND,
                                 "Invalid XdndAware property on window %ld", win);
            }

          XFree (version);
        }
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  return retval ? (proxy ? proxy : win) : None;
}

/* Source side */

static void
gdk_drag_do_leave (GdkX11Drag *drag_x11)
{
  if (drag_x11->proxy_xid)
    {
      switch (drag_x11->protocol)
        {
        case GDK_DRAG_PROTO_XDND:
          xdnd_send_leave (drag_x11);
          break;
        case GDK_DRAG_PROTO_ROOTWIN:
        case GDK_DRAG_PROTO_NONE:
        default:
          break;
        }

      drag_x11->proxy_xid = None;
    }
}

static Window
_gdk_x11_display_get_drag_protocol (GdkDisplay      *display,
                                    Window           xid,
                                    GdkDragProtocol *protocol,
                                    guint           *version)

{
  GdkSurface *surface;
  Window retval;

  /* Check for a local drag */
  surface = gdk_x11_surface_lookup_for_display (display, xid);
  if (surface)
    {
      if (g_object_get_data (G_OBJECT (surface), "gdk-dnd-registered") != NULL)
        {
          *protocol = GDK_DRAG_PROTO_XDND;
          *version = 5;
          GDK_DISPLAY_DEBUG (display, DND, "Entering local Xdnd window %#x", (guint) xid);
          return xid;
        }
      else if (_gdk_x11_display_is_root_window (display, xid))
        {
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          GDK_DISPLAY_DEBUG (display, DND, "Entering root window");
          return xid;
        }
    }
  else if ((retval = xdnd_check_dest (display, xid, version)))
    {
      *protocol = GDK_DRAG_PROTO_XDND;
      GDK_DISPLAY_DEBUG (display, DND, "Entering Xdnd window %#x", (guint) xid);
      return retval;
    }
  else
    {
      /* Check if this is a root window */
      gboolean rootwin = FALSE;

      if (_gdk_x11_display_is_root_window (display, (Window) xid))
        rootwin = TRUE;

      if (rootwin)
        {
          GDK_DISPLAY_DEBUG (display, DND, "Entering root window");
          *protocol = GDK_DRAG_PROTO_ROOTWIN;
          return xid;
        }
    }

  *protocol = GDK_DRAG_PROTO_NONE;

  return 0; /* a.k.a. None */
}

static GdkSurfaceCache *
drag_find_window_cache (GdkX11Drag *drag_x11,
                        GdkDisplay *display)
{
  if (!drag_x11->cache)
    drag_x11->cache = gdk_surface_cache_get (display);

  return drag_x11->cache;
}

static Window
gdk_x11_drag_find_surface (GdkDrag         *drag,
                           GdkSurface      *drag_surface,
                           int              x_root,
                           int              y_root,
                           GdkDragProtocol *protocol)
{
  GdkX11Screen *screen_x11;
  GdkX11Drag *drag_x11 = GDK_X11_DRAG (drag);
  GdkSurfaceCache *window_cache;
  GdkDisplay *display;
  Window dest;
  Window proxy;

  display = gdk_drag_get_display (drag);
  screen_x11 = GDK_X11_SCREEN(GDK_X11_DISPLAY (display)->screen);

  window_cache = drag_find_window_cache (drag_x11, display);

  dest = get_client_window_at_coords (window_cache,
                                      drag_surface && GDK_IS_X11_SURFACE (drag_surface) ?
                                      GDK_SURFACE_XID (drag_surface) : None,
                                      x_root * screen_x11->surface_scale,
                                      y_root * screen_x11->surface_scale);

  if (drag_x11->dest_xid != dest)
    {
      drag_x11->dest_xid = dest;

      /* Check if new destination accepts drags, and which protocol */

      /* There is some ugliness here. We actually need to pass
       * _three_ pieces of information to drag_motion - dest_surface,
       * protocol, and the XID of the unproxied window. The first
       * two are passed explicitly, the third implicitly through
       * protocol->dest_xid.
       */
      proxy = _gdk_x11_display_get_drag_protocol (display,
                                                  dest,
                                                  protocol,
                                                  &drag_x11->version);
    }
  else
    {
      proxy = dest;
      *protocol = drag_x11->protocol;
    }

  return proxy;
}

static void
move_drag_surface (GdkDrag *drag,
                   guint    x_root,
                   guint    y_root)
{
  GdkX11Drag *drag_x11 = GDK_X11_DRAG (drag);

  gdk_x11_surface_move (drag_x11->drag_surface,
                        x_root - drag_x11->hot_x,
                        y_root - drag_x11->hot_y);
  gdk_x11_surface_raise (drag_x11->drag_surface);
}

static gboolean
gdk_x11_drag_drag_motion (GdkDrag         *drag,
                          Window           proxy_xid,
                          GdkDragProtocol  protocol,
                          int              x_root,
                          int              y_root,
                          GdkDragAction    suggested_action,
                          GdkDragAction    possible_actions,
                          guint32          time)
{
  GdkX11Drag *drag_x11 = GDK_X11_DRAG (drag);

  if (drag_x11->drag_surface)
    move_drag_surface (drag, x_root, y_root);

  gdk_drag_set_actions (drag, possible_actions);

  if (protocol == GDK_DRAG_PROTO_XDND && drag_x11->version == 0)
    {
      /* This ugly hack is necessary since GTK doesn't know about
       * the XDND protocol version, and in particular doesn't know
       * that gdk_drag_find_window() has the side-effect
       * of setting drag_x11->version, and therefore sometimes call
       * gdk_x11_drag_drag_motion() without a prior call to
       * gdk_drag_find_window(). This happens, e.g.
       * when GTK is proxying DND events to embedded windows.
       */
      if (proxy_xid)
        {
          GdkDisplay *display = gdk_drag_get_display (drag);

          xdnd_check_dest (display,
                           proxy_xid,
                           &drag_x11->version);
        }
    }

  if (drag_x11->proxy_xid != proxy_xid)
    {
      /* Send a leave to the last destination */
      gdk_drag_do_leave (drag_x11);
      drag_x11->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */

      if (proxy_xid)
        {
          drag_x11->proxy_xid = proxy_xid;
          drag_x11->drop_xid = drag_x11->dest_xid;
          drag_x11->protocol = protocol;

          switch (protocol)
            {
            case GDK_DRAG_PROTO_XDND:
              xdnd_send_enter (drag_x11);
              break;

            case GDK_DRAG_PROTO_ROOTWIN:
            case GDK_DRAG_PROTO_NONE:
            default:
              break;
            }
        }
      else
        {
          drag_x11->proxy_xid = None;
          drag_x11->drop_xid = None;
          gdk_drag_set_selected_action (drag, 0);
        }

      /* Push a status event, to let the client know that
       * the drag changed
       */
      drag_x11->current_action = gdk_drag_get_selected_action (drag);
    }

  /* When we have a Xdnd target, make sure our XdndActionList
   * matches the current actions;
   */
  if (protocol == GDK_DRAG_PROTO_XDND && drag_x11->xdnd_actions != gdk_drag_get_actions (drag))
    {
      if (proxy_xid)
        {
          GdkDisplay *display = gdk_drag_get_display (drag);
          GdkDrop *drop = GDK_X11_DISPLAY (display)->current_drop;

          if (drop && GDK_SURFACE_XID (gdk_drop_get_surface (drop)) == proxy_xid)
            gdk_x11_drop_read_actions (drop);
          else
            xdnd_set_actions (drag_x11);
        }
    }

  /* Send a drag-motion event */

  drag_x11->last_x = x_root;
  drag_x11->last_y = y_root;

  if (drag_x11->proxy_xid)
    {
      GdkDisplay *display = gdk_drag_get_display (drag);
      GdkX11Screen *screen_x11 = GDK_X11_SCREEN(GDK_X11_DISPLAY (display)->screen);

      if (drag_x11->drag_status == GDK_DRAG_STATUS_DRAG)
        {
          switch (drag_x11->protocol)
            {
            case GDK_DRAG_PROTO_XDND:
              xdnd_send_motion (drag_x11, x_root * screen_x11->surface_scale, y_root * screen_x11->surface_scale, suggested_action, time);
              break;

            case GDK_DRAG_PROTO_ROOTWIN:
              {
                GdkContentFormats *formats = gdk_drag_get_formats (drag);
                /* GTK traditionally has used application/x-rootwin-drop,
                 * but the XDND spec specifies x-rootwindow-drop.
                 */
                if (gdk_content_formats_contain_mime_type (formats, "application/x-rootwindow-drop") ||
                    gdk_content_formats_contain_mime_type (formats, "application/x-rootwin-drop"))
                  gdk_drag_set_selected_action (drag, suggested_action);
                else
                  gdk_drag_set_selected_action (drag, 0);

                drag_x11->current_action = gdk_drag_get_selected_action (drag);
              }
              break;
            case GDK_DRAG_PROTO_NONE:
              g_warning ("Invalid drag protocol %u in gdk_x11_drag_drag_motion()", drag_x11->protocol);
              break;
            default:
              break;
            }
        }
      else
        return TRUE;
    }

  return FALSE;
}

static void
gdk_x11_drag_drop (GdkDrag *drag,
                   guint32  time)
{
  GdkX11Drag *drag_x11 = GDK_X11_DRAG (drag);

  if (drag_x11->proxy_xid)
    {
      switch (drag_x11->protocol)
        {
        case GDK_DRAG_PROTO_XDND:
          xdnd_send_drop (drag_x11, time);
          break;

        case GDK_DRAG_PROTO_ROOTWIN:
          g_warning ("Drops for GDK_DRAG_PROTO_ROOTWIN must be handled internally");
          break;
        case GDK_DRAG_PROTO_NONE:
          g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
          break;
        default:
          g_warning ("Drag protocol %u is not valid in gdk_drag_drop()", drag_x11->protocol);
          break;
        }
    }
}

/* Destination side */

void
_gdk_x11_surface_register_dnd (GdkSurface *surface)
{
  static const gulong xdnd_version = 5;
  GdkDisplay *display = gdk_surface_get_display (surface);

  g_return_if_fail (surface != NULL);

  if (g_object_get_data (G_OBJECT (surface), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (surface), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  /* Set XdndAware */

  /* The property needs to be of type XA_ATOM, not XA_INTEGER. Blech */
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_SURFACE_XID (surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "XdndAware"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)&xdnd_version, 1);
}

static GdkSurface *
gdk_x11_drag_get_drag_surface (GdkDrag *drag)
{
  return GDK_X11_DRAG (drag)->drag_surface;
}

static void
gdk_x11_drag_set_hotspot (GdkDrag *drag,
                          int      hot_x,
                          int      hot_y)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

  x11_drag->hot_x = hot_x;
  x11_drag->hot_y = hot_y;

  if (x11_drag->grab_seat)
    {
      /* DnD is managed, update current position */
      move_drag_surface (drag, x11_drag->last_x, x11_drag->last_y);
    }
}

static void
gdk_x11_drag_default_output_closed (GObject      *stream,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GError *error = NULL;

  if (!g_output_stream_close_finish (G_OUTPUT_STREAM (stream), result, &error))
    {
      GDK_DEBUG (DND, "failed to close stream: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (stream);
}

static void
gdk_x11_drag_default_output_done (GObject      *drag,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GOutputStream *stream = user_data;
  GError *error = NULL;

  if (!gdk_drag_write_finish (GDK_DRAG (drag), result, &error))
    {
      GDK_DISPLAY_DEBUG (gdk_drag_get_display (GDK_DRAG (drag)), DND, "failed to write stream: %s", error->message);
      g_error_free (error);
    }

  g_output_stream_close_async (stream,
                               G_PRIORITY_DEFAULT,
                               NULL, 
                               gdk_x11_drag_default_output_closed,
                               NULL);
}

static void
gdk_x11_drag_default_output_handler (GOutputStream *stream,
                                     const char    *mime_type,
                                     gpointer       user_data)
{
  gdk_drag_write_async (GDK_DRAG (user_data),
                        mime_type,
                        stream,
                        G_PRIORITY_DEFAULT,
                        NULL,
                        gdk_x11_drag_default_output_done,
                        stream);
}

static gboolean
gdk_x11_drag_xevent (GdkDisplay   *display,
                     const XEvent *xevent,
                     gpointer      data)
{
  GdkDrag *drag = GDK_DRAG (data);
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);
  Window xwindow;
  Atom xselection;

  xwindow = GDK_SURFACE_XID (x11_drag->ipc_surface);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");

  if (xevent->xany.window != xwindow)
    return FALSE;

  switch (xevent->type)
  {
    case SelectionClear:
      if (xevent->xselectionclear.selection != xselection)
        return FALSE;

      if (xevent->xselectionclear.time < x11_drag->timestamp)
        {
          GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                             "ignoring SelectionClear with too old timestamp (%lu vs %lu)",
                             xevent->xselectionclear.time, x11_drag->timestamp);
          return FALSE;
        }

      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "got SelectionClear, aborting DND");
      gdk_drag_cancel (drag, GDK_DRAG_CANCEL_ERROR);
      return TRUE;

    case SelectionRequest:
      {
        GdkContentFormats *formats;
        const char *target, *property;

        if (xevent->xselectionrequest.selection != xselection)
          return FALSE;

        target = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.target);
        if (xevent->xselectionrequest.property == None)
          property = target;
        else
          property = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.property);

        if (xevent->xselectionrequest.requestor == None)
          {
            GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                               "got SelectionRequest for %s @ %s with NULL window, ignoring",
                               target, property);
            return TRUE;
          }

        GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                           "got SelectionRequest for %s @ %s",
                           target, property);

        formats = gdk_content_formats_ref (gdk_drag_get_formats (drag));
        formats = gdk_content_formats_union_serialize_mime_types (formats);

        gdk_x11_selection_output_streams_create (display,
                                                 formats,
                                                 xevent->xselectionrequest.requestor,
                                                 xevent->xselectionrequest.selection,
                                                 xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.property
                                                   ? xevent->xselectionrequest.property
                                                   : xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.time,
                                                 gdk_x11_drag_default_output_handler,
                                                 drag);

        gdk_content_formats_unref (formats);

        return TRUE;
      }

    case ClientMessage:
      if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "XdndStatus"))
        gdk_x11_drag_handle_status (display, xevent);
      else if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "XdndFinished"))
        gdk_x11_drag_handle_finished (display, xevent);
      else
        return FALSE;
      return TRUE;

    default:
      return FALSE;
  }
}

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}


#define ANIM_TIME 500000 /* half a second */

typedef struct _GdkDragAnim GdkDragAnim;
struct _GdkDragAnim {
  GdkX11Drag *drag;
  GdkFrameClock *frame_clock;
  gint64 start_time;
};

static void
gdk_drag_anim_destroy (GdkDragAnim *anim)
{
  gdk_surface_hide (anim->drag->drag_surface);
  g_object_unref (anim->drag);
  g_free (anim);
}

static gboolean
gdk_drag_anim_timeout (gpointer data)
{
  GdkDragAnim *anim = data;
  GdkX11Drag *drag = anim->drag;
  GdkFrameClock *frame_clock = anim->frame_clock;
  gint64 current_time;
  double f;
  double t;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);

  f = (current_time - anim->start_time) / (double) ANIM_TIME;

  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  gdk_x11_surface_show (drag->drag_surface, FALSE);
  gdk_x11_surface_move (drag->drag_surface,
                        (drag->last_x - drag->hot_x) +
                        (drag->start_x - drag->last_x) * t,
                        (drag->last_y - drag->hot_y) +
                        (drag->start_y - drag->last_y) * t);
  gdk_x11_surface_set_opacity (drag->drag_surface, 1.0 - f);

  return G_SOURCE_CONTINUE;
}

static void
gdk_x11_drag_release_selection (GdkDrag *drag)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Atom xselection;

  display = gdk_drag_get_display (drag);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");
  xwindow = GDK_SURFACE_XID (x11_drag->ipc_surface);

  if (XGetSelectionOwner (xdisplay, xselection) == xwindow)
    XSetSelectionOwner (xdisplay, xselection, None, CurrentTime);
}

static void
gdk_x11_drag_drop_done (GdkDrag  *drag,
                        gboolean  success)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);
  GdkDragAnim *anim;
  guint id;

  gdk_x11_drag_release_selection (drag);

  g_signal_handlers_disconnect_by_func (gdk_drag_get_display (drag),
                                        gdk_x11_drag_xevent,
                                        drag);
  if (success)
    {
      gdk_surface_hide (x11_drag->drag_surface);
      g_object_unref (drag);
      return;
    }

  anim = g_new0 (GdkDragAnim, 1);
  anim->drag = g_object_ref (x11_drag);
  anim->frame_clock = gdk_surface_get_frame_clock (x11_drag->drag_surface);
  anim->start_time = gdk_frame_clock_get_frame_time (anim->frame_clock);

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_drag_anim_timeout, anim,
                           (GDestroyNotify) gdk_drag_anim_destroy);
  gdk_source_set_static_name_by_id (id, "[gtk] gdk_drag_anim_timeout");
  g_object_unref (drag);
}

static gboolean
drag_grab (GdkDrag *drag)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);
  GdkSeatCapabilities capabilities;
  GdkSeat *seat;
  GdkCursor *cursor;

  if (!x11_drag->ipc_surface)
    return FALSE;

  seat = gdk_device_get_seat (gdk_drag_get_device (drag));

  capabilities = GDK_SEAT_CAPABILITY_ALL_POINTING;

  cursor = gdk_drag_get_cursor (drag, x11_drag->current_action);
  g_set_object (&x11_drag->cursor, cursor);

  if (gdk_seat_grab (seat, x11_drag->ipc_surface,
                     capabilities, FALSE,
                     x11_drag->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&x11_drag->grab_seat, seat);

  return TRUE;
}

static void
drag_ungrab (GdkDrag *drag)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

  if (!x11_drag->grab_seat)
    return;

  gdk_seat_ungrab (x11_drag->grab_seat);

  g_clear_object (&x11_drag->grab_seat);
}

GdkDrag *
_gdk_x11_surface_drag_begin (GdkSurface         *surface,
                             GdkDevice          *device,
                             GdkContentProvider *content,
                             GdkDragAction       actions,
                             double              dx,
                             double              dy)
{
  GdkX11Drag *x11_drag;
  GdkDrag *drag;
  GdkDisplay *display;
  double px, py;
  int x_root, y_root;
  Atom xselection;
  GdkSurface *ipc_surface;

  display = gdk_surface_get_display (surface);

  ipc_surface = gdk_x11_drag_surface_new (display);

  drag = (GdkDrag *) g_object_new (GDK_TYPE_X11_DRAG,
                                   "surface", ipc_surface,
                                   "device", device,
                                   "content", content,
                                   "actions", actions,
                                   NULL);
  x11_drag = GDK_X11_DRAG (drag);

  precache_target_list (drag);

  gdk_x11_device_xi2_query_state (device, surface, &px, &py, NULL);

  gdk_x11_surface_get_root_coords (surface,
                                   round (px + dx),
                                   round (py + dy),
                                   &x_root,
                                   &y_root);

  x11_drag->start_x = x_root;
  x11_drag->start_y = y_root;
  x11_drag->last_x = x_root;
  x11_drag->last_y = y_root;

  x11_drag->protocol = GDK_DRAG_PROTO_XDND;
  x11_drag->actions = actions;
  x11_drag->ipc_surface = ipc_surface;
  if (gdk_x11_surface_get_group (surface))
    gdk_x11_surface_set_group (x11_drag->ipc_surface, surface);

  gdk_surface_set_is_mapped (x11_drag->ipc_surface, TRUE);
  gdk_x11_surface_show (x11_drag->ipc_surface, FALSE);

  x11_drag->drag_surface = gdk_x11_drag_surface_new (display);

  if (!drag_grab (drag))
    {
      g_object_unref (drag);
      return NULL;
    }
 
  move_drag_surface (drag, x_root, y_root);

  x11_drag->timestamp = gdk_x11_get_server_time (GDK_X11_DISPLAY (display)->leader_gdk_surface);
  xselection = gdk_x11_get_xatom_by_name_for_display (display, "XdndSelection");
  XSetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                      xselection,
                      GDK_SURFACE_XID (x11_drag->ipc_surface),
                      x11_drag->timestamp);
  if (XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), xselection) != GDK_SURFACE_XID (x11_drag->ipc_surface))
    {
      GDK_DISPLAY_DEBUG (display, DND, "failed XSetSelectionOwner() on \"XdndSelection\", aborting DND");
      g_object_unref (drag);
      return NULL;
    }

  
  g_signal_connect_object (display, "xevent", G_CALLBACK (gdk_x11_drag_xevent), drag, 0);
  /* backend holds a ref until gdk_drag_drop_done is called */
  g_object_ref (drag);

  return drag;
}

static void
gdk_x11_drag_set_cursor (GdkDrag   *drag,
                         GdkCursor *cursor)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

  if (!g_set_object (&x11_drag->cursor, cursor))
    return;

  if (x11_drag->grab_seat)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_device_grab (gdk_seat_get_pointer (x11_drag->grab_seat),
                       x11_drag->ipc_surface,
                       FALSE,
                       GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                       cursor, GDK_CURRENT_TIME);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gdk_x11_drag_cancel (GdkDrag             *drag,
                     GdkDragCancelReason  reason)
{
  gdk_drag_do_leave (GDK_X11_DRAG (drag));
  drag_ungrab (drag);
  gdk_drag_drop_done (drag, FALSE);
}

static void
gdk_x11_drag_drop_performed (GdkDrag *drag,
                             guint32  time_)
{
  gdk_x11_drag_drop (drag, time_);
  drag_ungrab (drag);
}

#define BIG_STEP 20
#define SMALL_STEP 1

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              int              button,
                              GdkDragAction    actions,
                              GdkDragAction   *suggested_action,
                              GdkDragAction   *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
    {
      *suggested_action = GDK_ACTION_ASK;
      *possible_actions = actions;
    }
  else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    {
      if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
        {
          if (actions & GDK_ACTION_LINK)
            {
              *suggested_action = GDK_ACTION_LINK;
              *possible_actions = GDK_ACTION_LINK;
            }
        }
      else if (state & GDK_CONTROL_MASK)
        {
          if (actions & GDK_ACTION_COPY)
            {
              *suggested_action = GDK_ACTION_COPY;
              *possible_actions = GDK_ACTION_COPY;
            }
        }
      else
        {
          if (actions & GDK_ACTION_MOVE)
            {
              *suggested_action = GDK_ACTION_MOVE;
              *possible_actions = GDK_ACTION_MOVE;
            }
        }
    }
  else
    {
      *possible_actions = actions;

      if ((state & (GDK_ALT_MASK)) && (actions & GDK_ACTION_ASK))
        *suggested_action = GDK_ACTION_ASK;
      else if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static void
gdk_drag_update (GdkDrag         *drag,
                 double           x_root,
                 double           y_root,
                 GdkModifierType  mods,
                 guint32          evtime)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);
  GdkDragAction suggested_action;
  GdkDragAction possible_actions;
  GdkDragProtocol protocol;
  Window proxy;

  gdk_drag_get_current_actions (mods, GDK_BUTTON_PRIMARY, x11_drag->actions,
                                &suggested_action, &possible_actions);

  proxy = gdk_x11_drag_find_surface (drag,
                                     x11_drag->drag_surface,
                                     x_root, y_root, &protocol);

  gdk_x11_drag_drag_motion (drag, proxy, protocol, x_root, y_root,
                            suggested_action, possible_actions, evtime);
}

static gboolean
gdk_dnd_handle_motion_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  double x, y;
  int x_root, y_root;

  gdk_event_get_position (event, &x, &y);
  x_root = event->surface->x + x;
  y_root = event->surface->y + y;
  gdk_drag_update (drag, x_root, y_root,
                   gdk_event_get_modifier_state (event),
                   gdk_event_get_time (event));
  return TRUE;
}

static gboolean
gdk_dnd_handle_grab_broken_event (GdkDrag  *drag,
                                  GdkEvent *event)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

  gboolean is_implicit = gdk_grab_broken_event_get_implicit (event);
  GdkSurface *grab_surface = gdk_grab_broken_event_get_grab_surface (event);

  /* Don't cancel if we break the implicit grab from the initial button_press.
   * Also, don't cancel if we re-grab on the widget or on our IPC window, for
   * example, when changing the drag cursor.
   */
  if (is_implicit ||
      grab_surface == x11_drag->drag_surface ||
      grab_surface == x11_drag->ipc_surface)
    return FALSE;

  if (gdk_event_get_device (event) != gdk_drag_get_device (drag))
    return FALSE;

  gdk_drag_cancel (drag, GDK_DRAG_CANCEL_ERROR);

  return TRUE;
}

static gboolean
gdk_dnd_handle_button_event (GdkDrag  *drag,
                             GdkEvent *event)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

#if 0
  /* FIXME: Check the button matches */
  if (event->button != x11_drag->button)
    return FALSE;
#endif

  if ((gdk_drag_get_selected_action (drag) != 0) &&
      (x11_drag->proxy_xid != None))
    {
      g_signal_emit_by_name (drag, "drop-performed");
    }
  else
    gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

  return TRUE;
}

gboolean
gdk_x11_drag_handle_event (GdkDrag        *drag,
                           GdkEvent       *event)
{
  GdkX11Drag *x11_drag = GDK_X11_DRAG (drag);

  if (!x11_drag->grab_seat)
    return FALSE;

  switch ((guint) event->event_type)
    {
    case GDK_MOTION_NOTIFY:
      return gdk_dnd_handle_motion_event (drag, event);

    case GDK_BUTTON_RELEASE:
      return gdk_dnd_handle_button_event (drag, event);

    case GDK_GRAB_BROKEN:
      return gdk_dnd_handle_grab_broken_event (drag, event);

    default:
      break;
    }

  return FALSE;
}
