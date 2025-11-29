/* GDK - The GIMP Drawing Kit
 * gdkdisplay-x11.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define VK_USE_PLATFORM_XLIB_KHR

#include "gdkdisplay-x11.h"
#include "gdkdisplayprivate.h"

#include "gdkasync.h"
#include "gdkdisplay.h"
#include "gdkeventsprivate.h"
#include "gdkeventsource.h"
#include "gdkeventtranslator.h"
#include "gdkframeclockprivate.h"
#include "gdkdeviceprivate.h"
#include "gdksurfaceprivate.h"
#include "gdkkeysprivate.h"
#include "gdkmarshalers.h"
#include "xsettings-client.h"

#include "gdkcairocontext-x11.h"
#include "gdkclipboard-x11.h"
#include "gdkglcontext-x11.h"
#include "gdkkeys-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkvulkancontext-x11.h"

#include "gdkprivate.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <epoxy/egl.h>

#include <X11/Xatom.h>
#include <X11/Xlibint.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <X11/extensions/shape.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

enum {
  XEVENT,
  LAST_SIGNAL
};

typedef struct _GdkErrorTrap  GdkErrorTrap;

struct _GdkErrorTrap
{
  /* Next sequence when trap was pushed, i.e. first sequence to
   * ignore
   */
  gulong start_sequence;

  /* Next sequence when trap was popped, i.e. first sequence
   * to not ignore. 0 if trap is still active.
   */
  gulong end_sequence;

  /* Most recent error code within the sequence */
  int error_code;
};

static void   gdk_x11_display_dispose            (GObject            *object);
static void   gdk_x11_display_finalize           (GObject            *object);

static void     gdk_x11_display_event_translator_init (GdkEventTranslatorIface *iface);

static GdkEvent * gdk_x11_display_translate_event (GdkEventTranslator *translator,
                                                   GdkDisplay         *display,
                                                   const XEvent       *xevent);

static void gdk_internal_connection_watch (Display  *display,
					   XPointer  arg,
					   int       fd,
					   gboolean  opening,
					   XPointer *watch_data);

typedef struct _GdkEventTypeX11 GdkEventTypeX11;

struct _GdkEventTypeX11
{
  int base;
  int n_events;
};

/* Note that we never *directly* use WM_LOCALE_NAME, WM_PROTOCOLS,
 * but including them here has the side-effect of getting them
 * into the internal Xlib cache
 */
static const char *const precache_atoms[] = {
  "UTF8_STRING",
  "WM_CLIENT_LEADER",
  "WM_DELETE_WINDOW",
  "WM_ICON_NAME",
  "WM_LOCALE_NAME",
  "WM_NAME",
  "WM_PROTOCOLS",
  "WM_TAKE_FOCUS",
  "WM_WINDOW_ROLE",
  "WM_STATE",
  "_NET_ACTIVE_WINDOW",
  "_NET_CURRENT_DESKTOP",
  "_NET_FRAME_EXTENTS",
  "_NET_STARTUP_ID",
  "_NET_WM_CM_S0",
  "_NET_WM_DESKTOP",
  "_NET_WM_ICON",
  "_NET_WM_ICON_NAME",
  "_NET_WM_NAME",
  "_NET_WM_PID",
  "_NET_WM_PING",
  "_NET_WM_STATE",
  "_NET_WM_STATE_ABOVE",
  "_NET_WM_STATE_BELOW",
  "_NET_WM_STATE_FULLSCREEN",
  "_NET_WM_STATE_HIDDEN",
  "_NET_WM_STATE_MODAL",
  "_NET_WM_STATE_MAXIMIZED_VERT",
  "_NET_WM_STATE_MAXIMIZED_HORZ",
  "_NET_WM_STATE_SKIP_TASKBAR",
  "_NET_WM_STATE_SKIP_PAGER",
  "_NET_WM_STATE_STICKY",
  "_NET_WM_SYNC_REQUEST",
  "_NET_WM_SYNC_REQUEST_COUNTER",
  "_NET_WM_WINDOW_TYPE",
  "_NET_WM_WINDOW_TYPE_COMBO",
  "_NET_WM_WINDOW_TYPE_DIALOG",
  "_NET_WM_WINDOW_TYPE_DND",
  "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
  "_NET_WM_WINDOW_TYPE_MENU",
  "_NET_WM_WINDOW_TYPE_NORMAL",
  "_NET_WM_WINDOW_TYPE_POPUP_MENU",
  "_NET_WM_WINDOW_TYPE_TOOLTIP",
  "_NET_WM_WINDOW_TYPE_UTILITY",
  "_NET_WM_USER_TIME",
  "_NET_WM_USER_TIME_WINDOW",
  "_NET_VIRTUAL_ROOTS",
  "GDK_SELECTION",
  "_NET_WM_STATE_FOCUSED",
  "GDK_VISUALS",
  "XdndAware",
  "XdndProxy",
  "XdndActionAsk",
  "XdndActionCopy",
  "XdndActionLink",
  "XdndActionList",
  "XdndActionMove",
  "XdndActionPrivate",
  "XdndDrop",
  "XdndEnter",
  "XdndFinished",
  "XdndLeave",
  "XdndPosition",
  "XdndSelection",
  "XdndStatus",
  "XdndTypeList"
};

static char *gdk_sm_client_id;

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GdkX11Display, gdk_x11_display, GDK_TYPE_DISPLAY,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_display_event_translator_init))

static void
gdk_x11_display_init (GdkX11Display *self)
{
  self->monitors = g_list_store_new (GDK_TYPE_MONITOR);
}

static void
gdk_x11_display_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_x11_display_translate_event;
}

#define ANY_EDGE_TILED (GDK_TOPLEVEL_STATE_LEFT_TILED | \
                        GDK_TOPLEVEL_STATE_RIGHT_TILED | \
                        GDK_TOPLEVEL_STATE_TOP_TILED | \
                        GDK_TOPLEVEL_STATE_BOTTOM_TILED)

static void
do_edge_constraint_state_check (GdkSurface      *surface,
                                GdkToplevelState  old_state,
                                GdkToplevelState *set,
                                GdkToplevelState *unset)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkToplevelState local_set, local_unset;
  guint edge_constraints;

  local_set = *set;
  local_unset = *unset;
  edge_constraints = toplevel->edge_constraints;

  /* If the WM doesn't support _GTK_EDGE_CONSTRAINTS, rely on the fallback
   * implementation. If it supports _GTK_EDGE_CONSTRAINTS, arrange for
   * GDK_TOPLEVEL_STATE_TILED to be set if any edge is tiled, and cleared
   * if no edge is tiled.
   */
  if (!gdk_x11_surface_supports_edge_constraints (surface))
    {
      /* FIXME: we rely on implementation details of mutter here:
       * mutter only tiles horizontally, and sets maxvert when it does
       * and if it tiles, it always affects all edges
       */
      if (old_state & GDK_TOPLEVEL_STATE_TILED)
        {
          if (!toplevel->have_maxvert)
            local_unset |= GDK_TOPLEVEL_STATE_TILED;
        }
      else
        {
          if (toplevel->have_maxvert && !toplevel->have_maxhorz)
            local_set |= GDK_TOPLEVEL_STATE_TILED;
        }
    }
  else
    {
      if (old_state & GDK_TOPLEVEL_STATE_TILED)
        {
          if (!(edge_constraints & ANY_EDGE_TILED))
            local_unset |= GDK_TOPLEVEL_STATE_TILED;
        }
      else
        {
          if (edge_constraints & ANY_EDGE_TILED)
            local_set |= GDK_TOPLEVEL_STATE_TILED;
        }
    }

  /* Top edge */
  if (old_state & GDK_TOPLEVEL_STATE_TOP_TILED)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_TOP_TILED) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_TOP_TILED;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_TOP_TILED)
        local_set |= GDK_TOPLEVEL_STATE_TOP_TILED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_TOP_RESIZABLE)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_TOP_RESIZABLE) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_TOP_RESIZABLE)
        local_set |= GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
    }

  /* Right edge */
  if (old_state & GDK_TOPLEVEL_STATE_RIGHT_TILED)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_RIGHT_TILED) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_RIGHT_TILED;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_RIGHT_TILED)
        local_set |= GDK_TOPLEVEL_STATE_RIGHT_TILED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE)
        local_set |= GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
    }

  /* Bottom edge */
  if (old_state & GDK_TOPLEVEL_STATE_BOTTOM_TILED)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_BOTTOM_TILED) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_BOTTOM_TILED;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_BOTTOM_TILED)
        local_set |= GDK_TOPLEVEL_STATE_BOTTOM_TILED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE)
        local_set |= GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
    }

  /* Left edge */
  if (old_state & GDK_TOPLEVEL_STATE_LEFT_TILED)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_LEFT_TILED) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_LEFT_TILED;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_LEFT_TILED)
        local_set |= GDK_TOPLEVEL_STATE_LEFT_TILED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_LEFT_RESIZABLE)
    {
      if ((edge_constraints & GDK_TOPLEVEL_STATE_LEFT_RESIZABLE) == 0)
        local_unset |= GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;
    }
  else
    {
      if (edge_constraints & GDK_TOPLEVEL_STATE_LEFT_RESIZABLE)
        local_set |= GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;
    }

  *set = local_set;
  *unset = local_unset;
}

static void
do_net_wm_state_changes (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkToplevelState old_state, set, unset;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !GDK_IS_TOPLEVEL (surface))
    return;

  old_state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));

  set = unset = 0;

  if (old_state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    {
      if (!toplevel->have_fullscreen)
        unset |= GDK_TOPLEVEL_STATE_FULLSCREEN;
    }
  else
    {
      if (toplevel->have_fullscreen)
        set |= GDK_TOPLEVEL_STATE_FULLSCREEN;
    }

  /* Our "maximized" means both vertical and horizontal; if only one,
   * we don't expose that via GDK
   */
  if (old_state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    {
      if (!(toplevel->have_maxvert && toplevel->have_maxhorz))
        unset |= GDK_TOPLEVEL_STATE_MAXIMIZED;
    }
  else
    {
      if (toplevel->have_maxvert && toplevel->have_maxhorz)
        set |= GDK_TOPLEVEL_STATE_MAXIMIZED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_FOCUSED)
    {
      if (!toplevel->have_focused)
        unset |= GDK_TOPLEVEL_STATE_FOCUSED;
    }
  else
    {
      if (toplevel->have_focused)
        set |= GDK_TOPLEVEL_STATE_FOCUSED;
    }

  if (old_state & GDK_TOPLEVEL_STATE_MINIMIZED)
    {
      if (!toplevel->have_hidden)
        {
          unset |= (GDK_TOPLEVEL_STATE_MINIMIZED |
                    GDK_TOPLEVEL_STATE_SUSPENDED);
        }
    }
  else
    {
      if (toplevel->have_hidden)
        {
          set |= (GDK_TOPLEVEL_STATE_MINIMIZED |
                  GDK_TOPLEVEL_STATE_SUSPENDED);
        }
    }

  /* Update edge constraints and tiling */
  do_edge_constraint_state_check (surface, old_state, &set, &unset);

  gdk_synthesize_surface_state (surface, unset, set);
}

static void
gdk_check_wm_desktop_changed (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);

  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  gulong *desktop;

  type = None;
  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                      GDK_SURFACE_XID (surface),
                      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
                      0, G_MAXLONG, False, XA_CARDINAL, &type,
                      &format, &nitems,
                      &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (type != None)
    {
      desktop = (gulong *)data;
      toplevel->on_all_desktops = ((*desktop & 0xFFFFFFFF) == 0xFFFFFFFF);
      XFree (desktop);
    }
  else
    toplevel->on_all_desktops = FALSE;

  do_net_wm_state_changes (surface);
}

static void
gdk_check_wm_state_changed (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);
  GdkX11Screen *screen = GDK_SURFACE_SCREEN (surface);

  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Atom *atoms = NULL;
  gulong i;

  toplevel->have_maxvert = FALSE;
  toplevel->have_maxhorz = FALSE;
  toplevel->have_fullscreen = FALSE;
  toplevel->have_focused = FALSE;
  toplevel->have_hidden = FALSE;

  type = None;
  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_SURFACE_XID (surface),
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
		      0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
		      &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (type != None)
    {
      Atom maxvert_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_VERT");
      Atom maxhorz_atom	= gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_HORZ");
      Atom fullscreen_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_FULLSCREEN");
      Atom focused_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_FOCUSED");
      Atom hidden_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_HIDDEN");

      atoms = (Atom *)data;

      i = 0;
      while (i < nitems)
        {
          if (atoms[i] == maxvert_atom)
            toplevel->have_maxvert = TRUE;
          else if (atoms[i] == maxhorz_atom)
            toplevel->have_maxhorz = TRUE;
          else if (atoms[i] == fullscreen_atom)
            toplevel->have_fullscreen = TRUE;
          else if (atoms[i] == focused_atom)
            toplevel->have_focused = TRUE;
          else if (atoms[i] == hidden_atom)
            toplevel->have_hidden = TRUE;

          ++i;
        }

      XFree (atoms);
    }

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            g_intern_static_string ("_NET_WM_STATE_FOCUSED")))
    toplevel->have_focused = TRUE;

  do_net_wm_state_changes (surface);
}

static void
gdk_check_edge_constraints_changed (GdkSurface *surface)
{
  GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (surface);
  GdkDisplay *display = GDK_SURFACE_DISPLAY (surface);

  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  gulong *constraints;

  type = None;
  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                      GDK_SURFACE_XID (surface),
                      gdk_x11_get_xatom_by_name_for_display (display, "_GTK_EDGE_CONSTRAINTS"),
                      0, G_MAXLONG, False, XA_CARDINAL, &type,
                      &format, &nitems,
                      &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (type != None)
    {
      constraints = (gulong *)data;

      /* The GDK enum for these states does not begin at zero so, to avoid
       * messing around with shifts, just make the passed value and GDK's
       * enum values match by shifting to the first tiled state.
       */
      toplevel->edge_constraints = constraints[0] << 8;

      XFree (constraints);
    }
  else
    {
      toplevel->edge_constraints = 0;
    }

  do_net_wm_state_changes (surface);
}

static Window
get_event_xwindow (const XEvent *xevent)
{
  Window xwindow;

  switch (xevent->type)
    {
    case DestroyNotify:
      xwindow = xevent->xdestroywindow.window;
      break;
    case UnmapNotify:
      xwindow = xevent->xunmap.window;
      break;
    case MapNotify:
      xwindow = xevent->xmap.window;
      break;
    case ConfigureNotify:
      xwindow = xevent->xconfigure.window;
      break;
    case ReparentNotify:
      xwindow = xevent->xreparent.window;
      break;
    case GravityNotify:
      xwindow = xevent->xgravity.window;
      break;
    case CirculateNotify:
      xwindow = xevent->xcirculate.window;
      break;
    default:
      xwindow = xevent->xany.window;
    }

  return xwindow;
}

static GdkEvent *
gdk_x11_display_translate_event (GdkEventTranslator *translator,
                                 GdkDisplay         *display,
                                 const XEvent       *xevent)
{
  Window xwindow;
  GdkSurface *surface;
  GdkX11Surface *surface_impl = NULL;
  GdkX11Screen *x11_screen = NULL;
  GdkToplevelX11 *toplevel = NULL;
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkEvent *event;

  event = NULL;

  xwindow = get_event_xwindow (xevent);
  if (xwindow != xevent->xany.window)
    return NULL;

  surface = gdk_x11_surface_lookup_for_display (display, xwindow);
  if (surface)
    {
      /* We may receive events such as NoExpose/GraphicsExpose
       * and ShmCompletion for pixmaps
       */
      if (!GDK_IS_SURFACE (surface))
        return NULL;

      x11_screen = GDK_SURFACE_SCREEN (surface);
      toplevel = _gdk_x11_surface_get_toplevel (surface);
      surface_impl = GDK_X11_SURFACE (surface);

      g_object_ref (surface);
    }

  if (surface && GDK_SURFACE_DESTROYED (surface))
    {
      if (xevent->type != DestroyNotify)
        goto done;
    }

  if (xevent->type == DestroyNotify)
    {
      x11_screen = GDK_X11_DISPLAY (display)->screen;

      if (x11_screen->wmspec_check_window == xevent->xdestroywindow.window)
        {
          x11_screen->wmspec_check_window = None;
          g_free (x11_screen->window_manager_name);
          x11_screen->window_manager_name = g_strdup ("unknown");

          /* careful, reentrancy */
          _gdk_x11_screen_window_manager_changed (x11_screen);

          goto done;
        }
    }

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  switch (xevent->type)
    {
    case KeymapNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS, "keymap notify");

      /* Not currently handled */
      break;

    case Expose:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "expose:\t\twindow: %ld  %d x,y: %d %d  w,h: %d %d",
                         xevent->xexpose.window, xevent->xexpose.count,
                         xevent->xexpose.x, xevent->xexpose.y,
                         xevent->xexpose.width, xevent->xexpose.height);

      if (surface == NULL)
        break;

      {
	GdkRectangle expose_rect;
        int x2, y2;

        expose_rect.x = xevent->xexpose.x / surface_impl->surface_scale;
        expose_rect.y = xevent->xexpose.y / surface_impl->surface_scale;

        x2 = (xevent->xexpose.x + xevent->xexpose.width + surface_impl->surface_scale -1) / surface_impl->surface_scale;
        expose_rect.width = x2 - expose_rect.x;

        y2 = (xevent->xexpose.y + xevent->xexpose.height + surface_impl->surface_scale -1) / surface_impl->surface_scale;
        expose_rect.height = y2 - expose_rect.y;

        gdk_surface_invalidate_rect (surface, &expose_rect);
      }

      break;

    case GraphicsExpose:
      {
	GdkRectangle expose_rect;
        int x2, y2;

        GDK_DISPLAY_DEBUG (display, EVENTS,
                           "graphics expose:\tdrawable: %ld",
                           xevent->xgraphicsexpose.drawable);

        if (surface == NULL)
          break;

        expose_rect.x = xevent->xgraphicsexpose.x / surface_impl->surface_scale;
        expose_rect.y = xevent->xgraphicsexpose.y / surface_impl->surface_scale;

        x2 = (xevent->xgraphicsexpose.x + xevent->xgraphicsexpose.width + surface_impl->surface_scale -1) / surface_impl->surface_scale;
        expose_rect.width = x2 - expose_rect.x;

        y2 = (xevent->xgraphicsexpose.y + xevent->xgraphicsexpose.height + surface_impl->surface_scale -1) / surface_impl->surface_scale;
        expose_rect.height = y2 - expose_rect.y;

        gdk_surface_invalidate_rect (surface, &expose_rect);
      }
      break;

    case VisibilityNotify:
      if (GDK_DISPLAY_DEBUG_CHECK (display, EVENTS))
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_message ("visibility notify:\twindow: %ld	 none",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityPartiallyObscured:
	    g_message ("visibility notify:\twindow: %ld	 partial",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityUnobscured:
	    g_message ("visibility notify:\twindow: %ld	 full",
		       xevent->xvisibility.window);
	    break;
          default:
            break;
	  }
      /* not handled */
      break;

    case CreateNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "create notify:\twindow: %ld  x,y: %d %d     w,h: %d %d  b-w: %d  parent: %ld         ovr: %d",
                         xevent->xcreatewindow.window,
                         xevent->xcreatewindow.x,
                         xevent->xcreatewindow.y,
                         xevent->xcreatewindow.width,
                         xevent->xcreatewindow.height,
                         xevent->xcreatewindow.border_width,
                         xevent->xcreatewindow.parent,
                         xevent->xcreatewindow.override_redirect);
      /* not really handled */
      break;

    case DestroyNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "destroy notify:\twindow: %ld",
                         xevent->xdestroywindow.window);

      if (surface)
        event = gdk_delete_event_new (surface);

      if (surface && GDK_SURFACE_XID (surface) != x11_screen->xroot_window)
        gdk_surface_destroy_notify (surface);

      break;

    case UnmapNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "unmap notify:\t\twindow: %ld",
                         xevent->xmap.window);

      if (surface)
	{
          /* If the WM supports the _NET_WM_STATE_HIDDEN hint, we do not want to
           * interpret UnmapNotify events as implying iconic state.
           * http://bugzilla.gnome.org/show_bug.cgi?id=590726.
           */
          if (x11_screen &&
              !gdk_x11_screen_supports_net_wm_hint (x11_screen,
                                                    g_intern_static_string ("_NET_WM_STATE_HIDDEN")))
            {
              /* If we are shown (not withdrawn) and get an unmap, it means we were
               * iconified in the X sense. If we are withdrawn, and get an unmap, it
               * means we hid the window ourselves, so we will have already flipped
               * the minimized bit off.
               */
              if (GDK_SURFACE_IS_MAPPED (surface))
                {
                  gdk_synthesize_surface_state (surface,
                                                0,
                                                GDK_TOPLEVEL_STATE_MINIMIZED |
                                                GDK_TOPLEVEL_STATE_SUSPENDED);
                }
            }

          if (surface_impl->toplevel &&
              surface_impl->toplevel->frame_pending)
            {
              surface_impl->toplevel->frame_pending = FALSE;
              gdk_surface_thaw_updates (surface);
            }

	  if (toplevel)
            gdk_surface_freeze_updates (surface);

          _gdk_x11_surface_grab_check_unmap (surface, xevent->xany.serial);

          gdk_profiler_add_markf (GDK_PROFILER_CURRENT_TIME, 0, "unmapped window", "0x%lx", GDK_SURFACE_XID (surface));
        }

      break;

    case MapNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "map notify:\t\twindow: %ld",
                         xevent->xmap.window);

      if (surface)
	{
	  /* Unset minimized if it was set */
	  if (surface->state & GDK_TOPLEVEL_STATE_MINIMIZED)
            {
              gdk_synthesize_surface_state (surface,
                                            GDK_TOPLEVEL_STATE_MINIMIZED |
                                            GDK_TOPLEVEL_STATE_SUSPENDED,
                                            0);
            }

	  if (toplevel)
	    gdk_surface_thaw_updates (surface);

          if (GDK_PROFILER_IS_RUNNING)
            {
              gdk_profiler_end_markf (surface_impl->map_time, "mapped window", "0x%lx", GDK_SURFACE_XID (surface));
              surface_impl->map_time = 0;
            }
	}

      break;

    case ReparentNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "reparent notify:\twindow: %ld  x,y: %d %d  parent: %ld      ovr: %d",
                         xevent->xreparent.window,
                         xevent->xreparent.x,
                         xevent->xreparent.y,
                         xevent->xreparent.parent,
                         xevent->xreparent.override_redirect);

      /* Not currently handled */
      break;

    case ConfigureNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "configure notify:\twindow: %ld  x,y: %d %d  w,h: %d %d  b-w: %d  above: %ld  ovr: %d%s",
                         xevent->xconfigure.window,
                         xevent->xconfigure.x,
                         xevent->xconfigure.y,
                         xevent->xconfigure.width,
                         xevent->xconfigure.height,
                         xevent->xconfigure.border_width,
                         xevent->xconfigure.above,
                         xevent->xconfigure.override_redirect,
                         !surface ? " (discarding)" : "");
      if (_gdk_x11_display_is_root_window (display, xevent->xconfigure.window))
        {
	  _gdk_x11_screen_size_changed (x11_screen, xevent);
        }

#ifdef HAVE_XSYNC
      if (toplevel && display_x11->use_sync && toplevel->pending_counter_value != 0)
	{
	  toplevel->configure_counter_value = toplevel->pending_counter_value;
	  toplevel->configure_counter_value_is_extended = toplevel->pending_counter_value_is_extended;
	  toplevel->pending_counter_value = 0;
	}
#endif

    if (surface && 
	xevent->xconfigure.event == xevent->xconfigure.window)
        {
          int x, y;
          int configured_width;
          int configured_height;
          int new_abs_x, new_abs_y;

          configured_width =
            (xevent->xconfigure.width + surface_impl->surface_scale - 1) /
            surface_impl->surface_scale;
          configured_height =
            (xevent->xconfigure.height + surface_impl->surface_scale - 1) /
            surface_impl->surface_scale;

	  if (!xevent->xconfigure.send_event &&
	      !xevent->xconfigure.override_redirect &&
	      !GDK_SURFACE_DESTROYED (surface))
	    {
	      int tx = 0;
	      int ty = 0;
	      Window child_window = 0;

              x = y = 0;
	      gdk_x11_display_error_trap_push (display);
	      if (XTranslateCoordinates (GDK_SURFACE_XDISPLAY (surface),
					 GDK_SURFACE_XID (surface),
					 x11_screen->xroot_window,
					 0, 0,
					 &tx, &ty,
					 &child_window))
		{
		  x = tx / surface_impl->surface_scale;
		  y = ty / surface_impl->surface_scale;
		}
	      gdk_x11_display_error_trap_pop_ignored (display);
	    }
	  else
	    {
	      x = xevent->xconfigure.x / surface_impl->surface_scale;
	      y = xevent->xconfigure.y / surface_impl->surface_scale;
	    }

          new_abs_x = x;
          new_abs_y = y;

          surface_impl->abs_x = new_abs_x;
          surface_impl->abs_y = new_abs_y;

          if (surface->parent)
            {
              GdkX11Surface *parent_impl =
                GDK_X11_SURFACE (surface->parent);

              surface->x = new_abs_x - parent_impl->abs_x;
              surface->y = new_abs_y - parent_impl->abs_y;
            }

          if (surface_impl->unscaled_width != xevent->xconfigure.width ||
              surface_impl->unscaled_height != xevent->xconfigure.height)
            {
              surface_impl->unscaled_width = xevent->xconfigure.width;
              surface_impl->unscaled_height = xevent->xconfigure.height;

              surface_impl->next_layout.configured_width = configured_width;
              surface_impl->next_layout.configured_height = configured_height;
              surface_impl->next_layout.surface_geometry_dirty = TRUE;
              surface_impl->next_layout.configure_pending = TRUE;
              gdk_surface_request_layout (surface);
            }

          if (surface->resize_count >= 1)
            {
              surface->resize_count -= 1;

              if (surface->resize_count == 0)
                _gdk_x11_moveresize_configure_done (display, surface);
            }

          gdk_x11_surface_update_popups (surface);
          gdk_x11_surface_enter_leave_monitors (surface);
        }
      break;

    case PropertyNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "property notify:\twindow: %ld, atom(%ld): %s%s%s",
                         xevent->xproperty.window,
                         xevent->xproperty.atom,
                         "\"",
                         gdk_x11_get_xatom_name_for_display (display, xevent->xproperty.atom),
                         "\"");

      if (surface == NULL)
        break;

      /* We compare with the serial of the last time we mapped the
       * window to avoid refetching properties that we set ourselves
       */
      if (toplevel &&
	  xevent->xproperty.serial >= toplevel->map_serial)
	{
	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"))
	    gdk_check_wm_state_changed (surface);

	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"))
	    gdk_check_wm_desktop_changed (surface);

	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_GTK_EDGE_CONSTRAINTS"))
	    gdk_check_edge_constraints_changed (surface);
	}
      break;

    case ColormapNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "colormap notify:\twindow: %ld",
                         xevent->xcolormap.window);

      /* Not currently handled */
      break;

    case ClientMessage:
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "client message:\twindow: %ld",
                         xevent->xclient.window);

      /* Not currently handled */
      break;

    case MappingNotify:
      GDK_DISPLAY_DEBUG (display, EVENTS, "mapping notify");

      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping ((XMappingEvent *) xevent);
      _gdk_x11_keymap_keys_changed (display);
      break;

    default:
#ifdef HAVE_RANDR
      if (xevent->type - display_x11->xrandr_event_base == RRScreenChangeNotify ||
          xevent->type - display_x11->xrandr_event_base == RRNotify)
	{
          if (display_x11->screen)
            _gdk_x11_screen_size_changed (display_x11->screen, xevent);
	}
      else
#endif
#ifdef HAVE_XKB
      if (xevent->type == display_x11->xkb_event_type)
	{
	  XkbEvent *xkb_event = (XkbEvent *) xevent;

	  switch (xkb_event->any.xkb_type)
	    {
	    case XkbNewKeyboardNotify:
	    case XkbMapNotify:
	      _gdk_x11_keymap_keys_changed (display);
	      break;

	    case XkbStateNotify:
	      _gdk_x11_keymap_state_changed (display, xevent);
	      break;
            default:
              break;
	    }
	}
#endif
    }

 done:
  if (surface)
    g_object_unref (surface);

  return event;
}

static GdkFrameTimings *
find_frame_timings (GdkFrameClock *clock,
                    guint64        serial)
{
  gint64 start_frame, end_frame, i;

  start_frame = gdk_frame_clock_get_history_start (clock);
  end_frame = gdk_frame_clock_get_frame_counter (clock);
  for (i = end_frame; i >= start_frame; i--)
    {
      GdkFrameTimings *timings = gdk_frame_clock_get_timings (clock, i);

      if (timings->cookie == serial)
        return timings;
    }

  return NULL;
}

/* _NET_WM_FRAME_DRAWN and _NET_WM_FRAME_TIMINGS messages represent time
 * as a "high resolution server time" - this is the server time interpolated
 * to microsecond resolution. The advantage of this time representation
 * is that if  X server is running on the same computer as a client, and
 * the Xserver uses 'clock_gettime(CLOCK_MONOTONIC, ...)' for the server
 * time, the client can detect this, and all such clients will share a
 * a time representation with high accuracy. If there is not a common
 * time source, then the time synchronization will be less accurate.
 */
static gint64
server_time_to_monotonic_time (GdkX11Display *display_x11,
                               gint64         server_time)
{
  if (display_x11->server_time_query_time == 0 ||
      (!display_x11->server_time_is_monotonic_time &&
       server_time > display_x11->server_time_query_time + 10*1000*1000)) /* 10 seconds */
    {
      gint64 current_server_time = gdk_x11_get_server_time (display_x11->leader_gdk_surface);
      gint64 current_server_time_usec = (gint64)current_server_time * 1000;
      gint64 current_monotonic_time = g_get_monotonic_time ();
      display_x11->server_time_query_time = current_monotonic_time;

      /* If the server time is within a second of the monotonic time,
       * we assume that they are identical. This seems like a big margin,
       * but we want to be as robust as possible even if the system
       * is under load and our processing of the server response is
       * delayed.
       */
      if (current_server_time_usec > current_monotonic_time - 1000*1000 &&
          current_server_time_usec < current_monotonic_time + 1000*1000)
        display_x11->server_time_is_monotonic_time = TRUE;

      display_x11->server_time_offset = current_server_time_usec - current_monotonic_time;
    }

  if (display_x11->server_time_is_monotonic_time)
    return server_time;
  else
    return server_time - display_x11->server_time_offset;
}

GdkFilterReturn
_gdk_wm_protocols_filter (const XEvent  *xevent,
                          GdkSurface    *win,
                          GdkEvent     **event,
                          gpointer       data)
{
  GdkDisplay *display;
  Atom atom;

  if (!GDK_IS_X11_SURFACE (win) || GDK_SURFACE_DESTROYED (win))
    return GDK_FILTER_CONTINUE;

  if (xevent->type != ClientMessage)
    return GDK_FILTER_CONTINUE;

  display = GDK_SURFACE_DISPLAY (win);

  /* This isn't actually WM_PROTOCOLS because that wouldn't leave enough space
   * in the message for everything that gets stuffed in */
  if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_FRAME_DRAWN"))
    {
      GdkX11Surface *surface_impl;
      surface_impl = GDK_X11_SURFACE (win);
      if (surface_impl->toplevel)
        {
          guint32 d0 = xevent->xclient.data.l[0];
          guint32 d1 = xevent->xclient.data.l[1];
          guint32 d2 = xevent->xclient.data.l[2];
          guint32 d3 = xevent->xclient.data.l[3];

          guint64 serial = ((guint64)d1 << 32) | d0;
          gint64 frame_drawn_time = server_time_to_monotonic_time (GDK_X11_DISPLAY (display), ((guint64)d3 << 32) | d2);
          gint64 refresh_interval, presentation_time;

          GdkFrameClock *clock = gdk_surface_get_frame_clock (win);
          GdkFrameTimings *timings = find_frame_timings (clock, serial);

          if (timings)
            timings->drawn_time = frame_drawn_time;

          if (!surface_impl->toplevel->frame_still_painting && surface_impl->toplevel->frame_pending)
            {
              surface_impl->toplevel->frame_pending = FALSE;
              gdk_surface_thaw_updates (win);
            }

          gdk_frame_clock_get_refresh_info (clock,
                                            frame_drawn_time,
                                            &refresh_interval,
                                            &presentation_time);
          if (presentation_time != 0)
            surface_impl->toplevel->throttled_presentation_time = presentation_time + refresh_interval;
        }

      return GDK_FILTER_REMOVE;
    }

  if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_FRAME_TIMINGS"))
    {
      GdkX11Surface *surface_impl;
      surface_impl = GDK_X11_SURFACE (win);
      if (surface_impl->toplevel)
        {
          guint32 d0 = xevent->xclient.data.l[0];
          guint32 d1 = xevent->xclient.data.l[1];
          guint32 d2 = xevent->xclient.data.l[2];
          guint32 d3 = xevent->xclient.data.l[3];

          guint64 serial = ((guint64)d1 << 32) | d0;

          GdkFrameClock *clock = gdk_surface_get_frame_clock (win);
          GdkFrameTimings *timings = find_frame_timings (clock, serial);

          if (timings)
            {
              gint32 presentation_time_offset = (gint32)d2;
              gint32 refresh_interval = d3;

              if (timings->drawn_time && presentation_time_offset)
                timings->presentation_time = timings->drawn_time + presentation_time_offset;

              if (refresh_interval)
                timings->refresh_interval = refresh_interval;

              timings->complete = TRUE;
              if (GDK_DISPLAY_DEBUG_CHECK (display, FRAMES))
                _gdk_frame_clock_debug_print_timings (clock, timings);

              if (GDK_PROFILER_IS_RUNNING)
                _gdk_frame_clock_add_timings_to_profiler (clock, timings);
            }
        }
    }

  if (xevent->xclient.message_type != gdk_x11_get_xatom_by_name_for_display (display, "WM_PROTOCOLS"))
    return GDK_FILTER_CONTINUE;

  atom = (Atom) xevent->xclient.data.l[0];

  if (atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW"))
    {
      /* The delete window request specifies a window
       *  to delete. We don't actually destroy the
       *  window because "it is only a request". (The
       *  window might contain vital data that the
       *  program does not want destroyed). Instead
       *  the event is passed along to the program,
       *  which should then destroy the window.
       */
      GDK_DISPLAY_DEBUG (display, EVENTS,
                         "delete window:\t\twindow: %ld",
                         xevent->xclient.window);

      *event = gdk_delete_event_new (win);

      gdk_x11_surface_set_user_time (win, xevent->xclient.data.l[1]);

      return GDK_FILTER_TRANSLATE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS"))
    {
      GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (win);

      /* There is no way of knowing reliably whether we are viewable;
       * so trap errors asynchronously around the XSetInputFocus call
       */
      if (toplevel)
        {
          gdk_x11_display_error_trap_push (display);
          XSetInputFocus (GDK_DISPLAY_XDISPLAY (display),
                          toplevel->focus_window,
                          RevertToParent,
                          xevent->xclient.data.l[1]);
          gdk_x11_display_error_trap_pop_ignored (display);
       }

      return GDK_FILTER_REMOVE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING") &&
	   !_gdk_x11_display_is_root_window (display, xevent->xclient.window))
    {
      XClientMessageEvent xclient = xevent->xclient;

      xclient.window = GDK_SURFACE_XROOTWIN (win);
      XSendEvent (GDK_SURFACE_XDISPLAY (win),
		  xclient.window,
		  False,
		  SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xclient);

      return GDK_FILTER_REMOVE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_SYNC_REQUEST") &&
	   GDK_X11_DISPLAY (display)->use_sync)
    {
      GdkToplevelX11 *toplevel = _gdk_x11_surface_get_toplevel (win);
      if (toplevel)
	{
#ifdef HAVE_XSYNC
	  toplevel->pending_counter_value = xevent->xclient.data.l[2] + ((gint64)xevent->xclient.data.l[3] << 32);
	  toplevel->pending_counter_value_is_extended = xevent->xclient.data.l[4] != 0;
#endif
	}
      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkX11Display *display_x11;

  display_x11 = GDK_X11_DISPLAY (display);
  display_x11->event_source = gdk_x11_event_source_new (display);

  gdk_x11_event_source_add_translator ((GdkEventSource *) display_x11->event_source,
                                       GDK_EVENT_TRANSLATOR (display));

  gdk_x11_event_source_add_translator ((GdkEventSource *) display_x11->event_source,
                                        GDK_EVENT_TRANSLATOR (display_x11->device_manager));
}

static void
set_sm_client_id (GdkDisplay  *display,
                  const char *sm_client_id)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (gdk_display_is_closed (display))
    return;

  if (sm_client_id && strcmp (sm_client_id, ""))
    XChangeProperty (display_x11->xdisplay, display_x11->leader_window,
                     gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"),
                     XA_STRING, 8, PropModeReplace, (guchar *)sm_client_id,
                     strlen (sm_client_id));
  else
    XDeleteProperty (display_x11->xdisplay, display_x11->leader_window,
                     gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"));
}

static void
gdk_x11_display_query_default_visual (GdkX11Display  *self,
                                      Visual        **out_visual,
                                      int            *out_depth)
{
  XVisualInfo template, *visinfo;
  int n_visuals;
  Display *dpy;

  dpy = gdk_x11_display_get_xdisplay (GDK_DISPLAY (self));

  template.screen = self->screen->screen_num;
  template.depth = 32;
  template.red_mask  = 0xff0000;
  template.green_mask = 0x00ff00;
  template.blue_mask = 0x0000ff;

  visinfo = XGetVisualInfo (dpy,
                            VisualScreenMask | VisualDepthMask
                            | VisualRedMaskMask | VisualGreenMaskMask | VisualBlueMaskMask,
                            &template,
                            &n_visuals);
  if (visinfo != NULL)
    {
      *out_visual = visinfo[0].visual;
      *out_depth = visinfo[0].depth;
      XFree (visinfo);
      return;
    }

  *out_visual = DefaultVisual (dpy, self->screen->screen_num);
  *out_depth = DefaultDepth (dpy, self->screen->screen_num);
}

static void
gdk_x11_display_init_leader_surface (GdkX11Display *self)
{
  GdkDisplay *display = GDK_DISPLAY (self);
  Display *xdisplay = gdk_x11_display_get_xdisplay (display);

  self->window_colormap = XCreateColormap (xdisplay,
                                           DefaultRootWindow (xdisplay),
                                           self->window_visual,
                                           AllocNone);
  gdk_display_set_rgba (display, self->window_depth == 32);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_x11_xsettings_init (GDK_X11_SCREEN (self->screen));

  self->device_manager = _gdk_x11_device_manager_new (display);

  gdk_event_init (display);

  self->leader_gdk_surface = gdk_x11_drag_surface_new (display);

  (_gdk_x11_surface_get_toplevel (self->leader_gdk_surface))->is_leader = TRUE;
  self->leader_window = GDK_SURFACE_XID (self->leader_gdk_surface);
  self->leader_window_title_set = FALSE;
}

/**
 * gdk_x11_display_open:
 * @display_name: (nullable): name of the X display.
 *   See the XOpenDisplay() for details.
 *
 * Tries to open a new display to the X server given by
 * @display_name. If opening the display fails, %NULL is
 * returned.
 *
 * Returns: (nullable) (transfer full): The new display
 */
GdkDisplay *
gdk_x11_display_open (const char *display_name)
{
  Display *xdisplay;
  GdkDisplay *display;
  GdkX11Display *display_x11;
  int argc;
  char *argv[1];
  XClassHint *class_hint;
  int ignore;
  int maj, min;
  char *cm_name;
  gboolean frame_extents;
  gboolean rgba;
  gboolean composited;

  XInitThreads ();

  xdisplay = XOpenDisplay (display_name);
  if (!xdisplay)
    return NULL;

  display = g_object_new (GDK_TYPE_X11_DISPLAY, NULL);
  display_x11 = GDK_X11_DISPLAY (display);

  display_x11->xdisplay = xdisplay;

  /* Set up handlers for Xlib internal connections */
  XAddConnectionWatch (xdisplay, gdk_internal_connection_watch, NULL);

  _gdk_x11_precache_atoms (display, precache_atoms, G_N_ELEMENTS (precache_atoms));

  /* RandR must be initialized before we initialize the screens */
  display_x11->have_randr12 = FALSE;
  display_x11->have_randr13 = FALSE;
  display_x11->have_randr15 = FALSE;
#ifdef HAVE_RANDR
  if (XRRQueryExtension (display_x11->xdisplay,
			 &display_x11->xrandr_event_base, &ignore))
  {
      int major, minor;

      XRRQueryVersion (display_x11->xdisplay, &major, &minor);

      if ((major == 1 && minor >= 2) || major > 1) {
	  display_x11->have_randr12 = TRUE;
	  if (minor >= 3 || major > 1)
	      display_x11->have_randr13 = TRUE;
#ifdef HAVE_RANDR15
	  if (minor >= 5 || major > 1)
	      display_x11->have_randr15 = TRUE;
#endif
      }
  }
#endif

  /* initialize the display's screens */ 
  display_x11->screen = _gdk_x11_screen_new (display, DefaultScreen (display_x11->xdisplay));

  /* If GL is available we want to pick better default/rgba visuals,
   * as we care about GLX details such as alpha/depth/stencil depth,
   * stereo and double buffering
   *
   * Note that this also sets up the leader surface while creating the initial
   * GL context.
   */
  if (!gdk_display_prepare_gl (display, NULL))
    {
      gdk_x11_display_query_default_visual (display_x11, &display_x11->window_visual, &display_x11->window_depth);
      gdk_x11_display_init_leader_surface (display_x11);
    }

#ifdef HAVE_XFIXES
  if (XFixesQueryExtension (display_x11->xdisplay, 
			    &display_x11->xfixes_event_base, 
			    &ignore))
    {
      display_x11->have_xfixes = TRUE;
    }
  else
#endif
    display_x11->have_xfixes = FALSE;

  display_x11->have_shapes = FALSE;
  display_x11->have_input_shapes = FALSE;

  if (XShapeQueryExtension (GDK_DISPLAY_XDISPLAY (display), &display_x11->shape_event_base, &ignore))
    {
      display_x11->have_shapes = TRUE;
#ifdef ShapeInput
      if (XShapeQueryVersion (GDK_DISPLAY_XDISPLAY (display), &maj, &min))
	display_x11->have_input_shapes = (maj == 1 && min >= 1);
#endif
    }

  gdk_display_set_input_shapes (display, display_x11->have_input_shapes);

  display_x11->trusted_client = TRUE;
  {
    Window root, child;
    int rootx, rooty, winx, winy;
    unsigned int xmask;

    gdk_x11_display_error_trap_push (display);
    XQueryPointer (display_x11->xdisplay,
		   GDK_X11_SCREEN (display_x11->screen)->xroot_window,
		   &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
    if (G_UNLIKELY (gdk_x11_display_error_trap_pop (display) == BadWindow))
      {
	g_warning ("Connection to display %s appears to be untrusted. Pointer and keyboard grabs and inter-client communication may not work as expected.", gdk_display_get_name (display));
	display_x11->trusted_client = FALSE;
      }
  }

  if (g_getenv ("GDK_SYNCHRONIZE"))
    XSynchronize (display_x11->xdisplay, True);

  class_hint = XAllocClassHint();
  class_hint->res_name = (char *) g_get_prgname ();
  class_hint->res_class = (char *) g_get_prgname ();

  /* XmbSetWMProperties sets the RESOURCE_NAME environment variable
   * from argv[0], so we just synthesize an argument array here.
   */
  argc = 1;
  argv[0] = (char *) g_get_prgname ();

  XmbSetWMProperties (display_x11->xdisplay,
		      display_x11->leader_window,
		      NULL, NULL, argv, argc, NULL, NULL,
		      class_hint);
  XFree (class_hint);

  if (gdk_sm_client_id)
    set_sm_client_id (display, gdk_sm_client_id);

  if (!gdk_running_in_sandbox ())
    {
      /* if sandboxed, we're likely in a pid namespace and would only confuse the wm with this */
      long pid = getpid ();
      XChangeProperty (display_x11->xdisplay,
                       display_x11->leader_window,
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PID"),
                       XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);
    }

  /* We don't yet know a valid time. */
  display_x11->user_time = 0;
  
#ifdef HAVE_XKB
  {
    int xkb_major = XkbMajorVersion;
    int xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;
	    
        if (XkbQueryExtension (display_x11->xdisplay, 
			       NULL, &display_x11->xkb_event_type, NULL,
                               &xkb_major, &xkb_minor))
          {
	    Bool detectable_autorepeat_supported;
	    
	    display_x11->use_xkb = TRUE;

            XkbSelectEvents (display_x11->xdisplay,
                             XkbUseCoreKbd,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);

	    /* keep this in sync with _gdk_x11_keymap_state_changed() */
	    XkbSelectEventDetails (display_x11->xdisplay,
				   XkbUseCoreKbd, XkbStateNotify,
				   XkbAllStateComponentsMask,
                                   XkbModifierStateMask|XkbGroupStateMask);

	    XkbSetDetectableAutoRepeat (display_x11->xdisplay,
					True,
					&detectable_autorepeat_supported);

            GDK_DEBUG (MISC, "Detectable autorepeat %s.",
                             detectable_autorepeat_supported
                               ? "supported"
                               : "not supported");

            display_x11->have_xkb_autorepeat = detectable_autorepeat_supported;
          }
      }
  }
#endif

  display_x11->use_sync = FALSE;
#ifdef HAVE_XSYNC
  {
    int major, minor;
    int error_base, event_base;
    
    if (XSyncQueryExtension (display_x11->xdisplay,
			     &event_base, &error_base) &&
        XSyncInitialize (display_x11->xdisplay,
                         &major, &minor))
      display_x11->use_sync = TRUE;
  }
#endif

#ifdef HAVE_XDAMAGE
  display_x11->have_damage = FALSE;
  if (XDamageQueryExtension (display_x11->xdisplay,
                             &display_x11->damage_event_base,
                             &display_x11->damage_error_base))
    display_x11->have_damage = TRUE;
#endif

  display->clipboard = gdk_x11_clipboard_new (display, "CLIPBOARD");
  display->primary_clipboard = gdk_x11_clipboard_new (display, "PRIMARY");

  /*
   * It is important that we first request the selection
   * notification, and then setup the initial state of
   * is_composited to avoid a race condition here.
   */
  cm_name = g_strdup_printf ("_NET_WM_CM_S%d", DefaultScreen (GDK_DISPLAY_XDISPLAY (display)));
  gdk_x11_display_request_selection_notification (display, cm_name);
  gdk_display_set_composited (GDK_DISPLAY (display),
                              XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                                                  gdk_x11_get_xatom_by_name_for_display (display, cm_name)) != None);
  g_free (cm_name);

  frame_extents = gdk_x11_screen_supports_net_wm_hint (gdk_x11_display_get_screen (display),
                                                       g_intern_static_string ("_GTK_FRAME_EXTENTS"));
  rgba = gdk_display_is_rgba (display);
  composited = gdk_display_is_composited (display);

  gdk_display_set_shadow_width (display, frame_extents && rgba && composited);

  gdk_display_emit_opened (display);

  return display;
}

/**
 * gdk_x11_display_set_program_class:
 * @display: a `GdkDisplay`
 * @program_class: a string
 *
 * Sets the program class.
 *
 * The X11 backend uses the program class to set the class name part
 * of the `WM_CLASS` property on toplevel windows; see the ICCCM.
 */
void
gdk_x11_display_set_program_class (GdkDisplay *display,
                                   const char *program_class)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  XClassHint *class_hint;

  g_free (display_x11->program_class);
  display_x11->program_class = g_strdup (program_class);

  class_hint = XAllocClassHint();
  class_hint->res_name = (char *) g_get_prgname ();
  class_hint->res_class = (char *) program_class;
  XSetClassHint (display_x11->xdisplay, display_x11->leader_window, class_hint);
  XFree (class_hint);
}

/*
 * XLib internal connection handling
 */
typedef struct _GdkInternalConnection GdkInternalConnection;

struct _GdkInternalConnection
{
  int	         fd;
  GSource	*source;
  Display	*display;
};

static gboolean
process_internal_connection (GIOChannel  *gioc,
			     GIOCondition cond,
			     gpointer     data)
{
  GdkInternalConnection *connection = (GdkInternalConnection *)data;

  XProcessInternalConnection ((Display*)connection->display, connection->fd);

  return TRUE;
}

static gulong
gdk_x11_display_get_next_serial (GdkDisplay *display)
{
  return NextRequest (GDK_DISPLAY_XDISPLAY (display));
}


static GdkInternalConnection *
gdk_add_connection_handler (Display *display,
			    guint    fd)
{
  GIOChannel *io_channel;
  GdkInternalConnection *connection;

  connection = g_new (GdkInternalConnection, 1);

  connection->fd = fd;
  connection->display = display;
  
  io_channel = g_io_channel_unix_new (fd);
  
  connection->source = g_io_create_watch (io_channel, G_IO_IN);
  g_source_set_callback (connection->source,
			 (GSourceFunc)process_internal_connection, connection, NULL);
  g_source_attach (connection->source, NULL);
  
  g_io_channel_unref (io_channel);
  
  return connection;
}

static void
gdk_remove_connection_handler (GdkInternalConnection *connection)
{
  g_source_destroy (connection->source);
  g_free (connection);
}

static void
gdk_internal_connection_watch (Display  *display,
			       XPointer  arg,
			       int       fd,
			       gboolean  opening,
			       XPointer *watch_data)
{
  if (opening)
    *watch_data = (XPointer)gdk_add_connection_handler (display, fd);
  else
    gdk_remove_connection_handler ((GdkInternalConnection *)*watch_data);
}

static const char *
gdk_x11_display_get_name (GdkDisplay *display)
{
  return (char *) DisplayString (GDK_X11_DISPLAY (display)->xdisplay);
}

gboolean
_gdk_x11_display_is_root_window (GdkDisplay *display,
				 Window      xroot_window)
{
  GdkX11Display *display_x11;

  display_x11 = GDK_X11_DISPLAY (display);

  return GDK_SCREEN_XROOTWIN (display_x11->screen) == xroot_window;
}

static void
device_grab_update_callback (GdkDisplay *display,
                             gpointer    data,
                             gulong      serial)
{
  GdkDevice *device = data;

  _gdk_display_device_grab_update (display, device, serial);
}

#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

void
_gdk_x11_display_update_grab_info (GdkDisplay *display,
                                   GdkDevice  *device,
                                   int         status)
{
  if (status == GrabSuccess)
    _gdk_x11_roundtrip_async (display, device_grab_update_callback, device);
}

void
_gdk_x11_display_update_grab_info_ungrab (GdkDisplay *display,
                                          GdkDevice  *device,
                                          guint32     time,
                                          gulong      serial)
{
  GdkDeviceGrabInfo *grab;

  XFlush (GDK_DISPLAY_XDISPLAY (display));

  grab = _gdk_display_get_last_device_grab (display, device);
  if (grab &&
      (time == GDK_CURRENT_TIME ||
       grab->time == GDK_CURRENT_TIME ||
       !XSERVER_TIME_IS_LATER (grab->time, time)))
    {
      grab->serial_end = serial;
      _gdk_x11_roundtrip_async (display, device_grab_update_callback, device);
    }
}

static void
gdk_x11_display_beep (GdkDisplay *display)
{
  if (!GDK_X11_DISPLAY (display)->trusted_client)
    return;

#ifdef HAVE_XKB
  XkbBell (GDK_DISPLAY_XDISPLAY (display), None, 0, None);
#else
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
#endif
}

static void
gdk_x11_display_sync (GdkDisplay *display)
{
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

static void
gdk_x11_display_flush (GdkDisplay *display)
{
  if (!display->closed)
    XFlush (GDK_DISPLAY_XDISPLAY (display));
}

static gboolean
gdk_x11_display_has_pending (GdkDisplay *display)
{
  return XPending (GDK_DISPLAY_XDISPLAY (display));
}

/**
 * gdk_x11_display_get_default_group:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Returns the default group leader surface for all toplevel surfaces
 * on @display. This surface is implicitly created by GDK.
 * See gdk_x11_surface_set_group().
 *
 * Returns: (transfer none): The default group leader surface
 * for @display
 */
GdkSurface *
gdk_x11_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_X11_DISPLAY (display)->leader_gdk_surface;
}

/**
 * gdk_x11_display_grab:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Call XGrabServer() on @display.
 * To ungrab the display again, use gdk_x11_display_ungrab().
 *
 * gdk_x11_display_grab()/gdk_x11_display_ungrab() calls can be nested.
 **/
void
gdk_x11_display_grab (GdkDisplay *display)
{
  GdkX11Display *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_X11_DISPLAY (display);
  
  if (display_x11->grab_count == 0)
    XGrabServer (display_x11->xdisplay);
  display_x11->grab_count++;
}

/**
 * gdk_x11_display_ungrab:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Ungrab @display after it has been grabbed with
 * gdk_x11_display_grab().
 **/
void
gdk_x11_display_ungrab (GdkDisplay *display)
{
  GdkX11Display *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_X11_DISPLAY (display);;
  g_return_if_fail (display_x11->grab_count > 0);
  
  display_x11->grab_count--;
  if (display_x11->grab_count == 0)
    {
      XUngrabServer (display_x11->xdisplay);
      XFlush (display_x11->xdisplay);
    }
}

static void
gdk_x11_display_dispose (GObject *object)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (object);

  if (display_x11->event_source)
    {
      g_source_destroy (display_x11->event_source);
      g_source_unref (display_x11->event_source);
      display_x11->event_source = NULL;
    }

  G_OBJECT_CLASS (gdk_x11_display_parent_class)->dispose (object);
}

static void
gdk_x11_display_finalize (GObject *object)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (object);

  /* Keymap */
  if (display_x11->keymap)
    g_object_unref (display_x11->keymap);

  _gdk_x11_cursor_display_finalize (GDK_DISPLAY (display_x11));

  /* Get rid of pending streams */
  g_slist_free_full (display_x11->streams, g_object_unref);

  /* Atom Hashtable */
  g_hash_table_destroy (display_x11->atom_from_string);
  g_hash_table_destroy (display_x11->atom_to_string);

  /* Leader Window */
  XDestroyWindow (display_x11->xdisplay, display_x11->leader_window);

  /* Free all GdkX11Screens */
  g_object_unref (display_x11->screen);

  g_list_store_remove_all (display_x11->monitors);
  g_object_unref (display_x11->monitors);

  g_free (display_x11->startup_notification_id);

  /* X ID hashtable */
  g_hash_table_destroy (display_x11->xid_ht);

  XCloseDisplay (display_x11->xdisplay);

  g_clear_error (&display_x11->gl_error);

  /* error traps */
  while (display_x11->error_traps != NULL)
    {
      GdkErrorTrap *trap = display_x11->error_traps->data;

      display_x11->error_traps =
        g_slist_delete_link (display_x11->error_traps,
                             display_x11->error_traps);

      if (trap->end_sequence == 0)
        g_warning ("Display finalized with an unpopped error trap");

      g_free (trap);
    }

  g_free (display_x11->program_class);

  G_OBJECT_CLASS (gdk_x11_display_parent_class)->finalize (object);
}

/**
 * gdk_x11_lookup_xdisplay:
 * @xdisplay: a pointer to an X Display
 *
 * Find the `GdkDisplay` corresponding to @xdisplay, if any exists.
*
 * Returns: (transfer none) (type GdkX11Display): the `GdkDisplay`, if found, otherwise %NULL.
 **/
GdkDisplay *
gdk_x11_lookup_xdisplay (Display *xdisplay)
{
  GSList *list, *l;
  GdkDisplay *display;

  display = NULL;

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());

  for (l = list; l; l = l->next)
    {
      if (GDK_IS_X11_DISPLAY (l->data) &&
          GDK_DISPLAY_XDISPLAY (l->data) == xdisplay)
        {
          display = l->data;
          break;
        }
    }

  g_slist_free (list);

  return display;
}

/**
 * gdk_x11_display_get_xdisplay:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Returns the X display of a `GdkDisplay`.
 *
 * Returns: (transfer none): an X display
 */
Display *
gdk_x11_display_get_xdisplay (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_X11_DISPLAY (display)->xdisplay;
}

/**
 * gdk_x11_display_get_xscreen:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Returns the X Screen used by `GdkDisplay`.
 *
 * Returns: (transfer none): an X Screen
 */
Screen *
gdk_x11_display_get_xscreen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_X11_SCREEN (GDK_X11_DISPLAY (display)->screen)->xscreen;
}

/**
 * gdk_x11_display_get_xrootwindow:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Returns the root X window used by `GdkDisplay`.
 *
 * Returns: an X Window
 */
Window
gdk_x11_display_get_xrootwindow (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  return GDK_SCREEN_XROOTWIN (GDK_X11_DISPLAY (display)->screen);
}

static void
gdk_x11_display_make_default (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  const char *startup_id;

  g_free (display_x11->startup_notification_id);
  display_x11->startup_notification_id = NULL;

  startup_id = gdk_get_startup_notification_id ();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (startup_id)
    gdk_x11_display_set_startup_notification_id (display, startup_id);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
broadcast_xmessage (GdkDisplay *display,
		    const char *message_type,
		    const char *message_type_begin,
		    const char *message)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Window xroot_window = GDK_DISPLAY_XROOTWIN (display);
  
  Atom type_atom;
  Atom type_atom_begin;
  Window xwindow;

  if (!G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    return;

  {
    XSetWindowAttributes attrs;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     xroot_window,
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     (Visual *)CopyFromParent,
                     CWOverrideRedirect | CWEventMask,
                     &attrs);
  }

  type_atom = gdk_x11_get_xatom_by_name_for_display (display, message_type);
  type_atom_begin = gdk_x11_get_xatom_by_name_for_display (display, message_type_begin);
  
  {
    XClientMessageEvent xclient;
    const char *src;
    const char *src_end;
    char *dest;
    char *dest_end;
    
    memset(&xclient, 0, sizeof (xclient));
    xclient.type = ClientMessage;
    xclient.message_type = type_atom_begin;
    xclient.display =xdisplay;
    xclient.window = xwindow;
    xclient.format = 8;

    src = message;
    src_end = message + strlen (message) + 1; /* +1 to include nul byte */
    
    while (src != src_end)
      {
        dest = &xclient.data.b[0];
        dest_end = dest + 20;
        
        while (dest != dest_end &&
               src != src_end)
          {
            *dest = *src;
            ++dest;
            ++src;
          }

	while (dest != dest_end)
	  {
	    *dest = 0;
	    ++dest;
	  }
        
        XSendEvent (xdisplay,
                    xroot_window,
                    False,
                    PropertyChangeMask,
                    (XEvent *)&xclient);

        xclient.message_type = type_atom;
      }
  }

  XDestroyWindow (xdisplay, xwindow);
  XFlush (xdisplay);
}

/**
 * gdk_x11_display_broadcast_startup_message:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @message_type: startup notification message type ("new", "change",
 * or "remove")
 * @...: a list of key/value pairs (as strings), terminated by a
 * %NULL key. (A %NULL value for a key will cause that key to be
 * skipped in the output.)
 *
 * Sends a startup notification message of type @message_type to
 * @display. 
 *
 * This is a convenience function for use by code that implements the
 * freedesktop startup notification specification. Applications should
 * not normally need to call it directly. See the
 * [Startup Notification Protocol specification](http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt)
 * for definitions of the message types and keys that can be used.
 **/
void
gdk_x11_display_broadcast_startup_message (GdkDisplay *display,
					   const char *message_type,
					   ...)
{
  GString *message;
  va_list ap;
  const char *key, *value, *p;

  message = g_string_new (message_type);
  g_string_append_c (message, ':');

  va_start (ap, message_type);
  while ((key = va_arg (ap, const char *)))
    {
      value = va_arg (ap, const char *);
      if (!value)
	continue;

      g_string_append_printf (message, " %s=\"", key);
      for (p = value; *p; p++)
	{
	  switch (*p)
	    {
	    case ' ':
	    case '"':
	    case '\\':
	      g_string_append_c (message, '\\');
	      break;
            default:
              break;
	    }

	  g_string_append_c (message, *p);
	}
      g_string_append_c (message, '\"');
    }
  va_end (ap);

  broadcast_xmessage (display,
                      "_NET_STARTUP_INFO",
                      "_NET_STARTUP_INFO_BEGIN",
                      message->str);

  g_string_free (message, TRUE);
}

static void
gdk_x11_display_notify_startup_complete (GdkDisplay  *display,
                                         const char *startup_id)
{
  char *free_this = NULL;

  if (startup_id == NULL)
    {
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

      startup_id = free_this = display_x11->startup_notification_id;
      display_x11->startup_notification_id = NULL;

      if (startup_id == NULL)
        return;
    }

  gdk_x11_display_broadcast_startup_message (display, "remove",
                                             "ID", startup_id,
                                             NULL);

  g_free (free_this);
}

gboolean
gdk_x11_display_request_selection_notification (GdkDisplay *display,
						const char *selection)

{
#ifdef HAVE_XFIXES
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  Atom atom;

  if (display_x11->have_xfixes)
    {
      atom = gdk_x11_get_xatom_by_name_for_display (display, selection);
      XFixesSelectSelectionInput (display_x11->xdisplay, 
				  display_x11->leader_window,
				  atom,
				  XFixesSetSelectionOwnerNotifyMask |
				  XFixesSelectionWindowDestroyNotifyMask |
				  XFixesSelectionClientCloseNotifyMask);
      return TRUE;
    }
  else
#endif
    return FALSE;
}

/**
 * gdk_x11_display_get_user_time:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Returns the timestamp of the last user interaction on
 * @display. The timestamp is taken from events caused
 * by user interaction such as key presses or pointer
 * movements. See gdk_x11_surface_set_user_time().
 *
 * Returns: the timestamp of the last user interaction
 */
guint32
gdk_x11_display_get_user_time (GdkDisplay *display)
{
  return GDK_X11_DISPLAY (display)->user_time;
}

/**
 * gdk_x11_display_get_startup_notification_id:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Gets the startup notification ID for a display.
 * 
 * Returns: the startup notification ID for @display
 *
 * Deprecated: 4.10
 */
const char *
gdk_x11_display_get_startup_notification_id (GdkDisplay *display)
{
  return GDK_X11_DISPLAY (display)->startup_notification_id;
}

/**
 * gdk_x11_display_set_startup_notification_id:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @startup_id: the startup notification ID (must be valid utf8)
 *
 * Sets the startup notification ID for a display.
 *
 * This is usually taken from the value of the DESKTOP_STARTUP_ID
 * environment variable, but in some cases (such as the application not
 * being launched using exec()) it can come from other sources.
 *
 * If the ID contains the string "_TIME" then the portion following that
 * string is taken to be the X11 timestamp of the event that triggered
 * the application to be launched and the GDK current event time is set
 * accordingly.
 *
 * The startup ID is also what is used to signal that the startup is
 * complete (for example, when opening a window or when calling
 * gdk_display_notify_startup_complete()).
 *
 * Deprecated: 4.10: Using [method@Gdk.Toplevel.set_startup_id] is sufficient
 */
void
gdk_x11_display_set_startup_notification_id (GdkDisplay  *display,
                                             const char *startup_id)
{
  GdkX11Display *display_x11;
  char *time_str;

  display_x11 = GDK_X11_DISPLAY (display);

  g_free (display_x11->startup_notification_id);
  display_x11->startup_notification_id = g_strdup (startup_id);

  if (startup_id != NULL)
    {
      /* Find the launch time from the startup_id, if it's there.  Newer spec
       * states that the startup_id is of the form <unique>_TIME<timestamp>
       */
      time_str = g_strrstr (startup_id, "_TIME");
      if (time_str != NULL)
        {
          gulong retval;
          char *end;
          errno = 0;

          /* Skip past the "_TIME" part */
          time_str += 5;

          retval = strtoul (time_str, &end, 0);
          if (end != time_str && errno == 0)
            display_x11->user_time = retval;
        }
      else
        display_x11->user_time = 0;

      /* Set the startup id on the leader window so it
       * applies to all windows we create on this display
       */
      XChangeProperty (display_x11->xdisplay,
                       display_x11->leader_window,
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"),
                       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                       PropModeReplace,
                       (guchar *)startup_id, strlen (startup_id));
    }
  else
    {
      XDeleteProperty (display_x11->xdisplay, display_x11->leader_window,
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"));
      display_x11->user_time = 0;
    }
}

/* look up the extension name for a given major opcode.  grubs around in
 * xlib to do it since a) it’s already cached there b) XQueryExtension
 * emits protocol so we can’t use it in an error handler.
 */
static const char *
_gdk_x11_decode_request_code(Display *dpy, int code)
{
  _XExtension *ext;

  if (code < 128)
    return "core protocol";

  for (ext = dpy->ext_procs; ext; ext = ext->next)
    {
      if (ext->codes.major_opcode == code)
        return ext->name;
    }

  return "unknown";
}

/* compare X sequence numbers handling wraparound */
#define SEQUENCE_COMPARE(a,op,b) (((long) (a) - (long) (b)) op 0)

/* delivers an error event from the error handler in gdkmain-x11.c */
void
_gdk_x11_display_error_event (GdkDisplay  *display,
                              XErrorEvent *error)
{
  GdkX11Display *display_x11;
  GSList *tmp_list;
  gboolean ignore;

  display_x11 = GDK_X11_DISPLAY (display);

  ignore = FALSE;
  for (tmp_list = display_x11->error_traps;
       tmp_list != NULL;
       tmp_list = tmp_list->next)
    {
      GdkErrorTrap *trap;

      trap = tmp_list->data;

      if (SEQUENCE_COMPARE (trap->start_sequence, <=, error->serial) &&
          (trap->end_sequence == 0 ||
           SEQUENCE_COMPARE (trap->end_sequence, >, error->serial)))
        {
          ignore = TRUE;
          trap->error_code = error->error_code;
          break; /* only innermost trap gets the error code */
        }
    }

  if (!ignore)
    {
      char buf[64];
      char *msg;

      XGetErrorText (display_x11->xdisplay, error->error_code, buf, 63);

      msg =
        g_strdup_printf ("The program '%s' received an X Window System error.\n"
                         "This probably reflects a bug in the program.\n"
                         "The error was '%s'.\n"
                         "  (Details: serial %ld error_code %d request_code %d (%s) minor_code %d)\n"
                         "  (Note to programmers: normally, X errors are reported asynchronously;\n"
                         "   that is, you will receive the error a while after causing it.\n"
                         "   To debug your program, run it with the GDK_SYNCHRONIZE environment\n"
                         "   variable to change this behavior. You can then get a meaningful\n"
                         "   backtrace from your debugger if you break on the gdk_x_error() function.)",
                         g_get_prgname (),
                         buf,
                         error->serial,
                         error->error_code,
                         error->request_code,
                         _gdk_x11_decode_request_code(display_x11->xdisplay,
                                                      error->request_code),
                         error->minor_code);

#ifdef G_ENABLE_DEBUG
      g_error ("%s", msg);
#else /* !G_ENABLE_DEBUG */
      g_warning ("%s", msg);

      _exit (1);
#endif /* G_ENABLE_DEBUG */
    }
}

static void
delete_outdated_error_traps (GdkX11Display *display_x11)
{
  GSList *tmp_list;
  gulong processed_sequence;

  processed_sequence = XLastKnownRequestProcessed (display_x11->xdisplay);

  tmp_list = display_x11->error_traps;
  while (tmp_list != NULL)
    {
      GdkErrorTrap *trap = tmp_list->data;

      if (trap->end_sequence != 0 &&
          SEQUENCE_COMPARE (trap->end_sequence, <=, processed_sequence))
        {
          GSList *free_me = tmp_list;

          tmp_list = tmp_list->next;
          display_x11->error_traps =
            g_slist_delete_link (display_x11->error_traps, free_me);
          g_free (trap);
        }
      else
        {
          tmp_list = tmp_list->next;
        }
    }
}

/**
 * gdk_x11_display_error_trap_push:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Begins a range of X requests on @display for which X error events
 * will be ignored. Unignored errors (when no trap is pushed) will abort
 * the application. Use gdk_x11_display_error_trap_pop() or
 * gdk_x11_display_error_trap_pop_ignored()to lift a trap pushed
 * with this function.
 */
void
gdk_x11_display_error_trap_push (GdkDisplay *display)
{
  GdkX11Display *display_x11;
  GdkErrorTrap *trap;

  display_x11 = GDK_X11_DISPLAY (display);

  delete_outdated_error_traps (display_x11);

  /* set up the Xlib callback to tell us about errors */
  _gdk_x11_error_handler_push ();

  trap = g_new0 (GdkErrorTrap, 1);

  trap->start_sequence = XNextRequest (display_x11->xdisplay);
  trap->error_code = Success;

  display_x11->error_traps =
    g_slist_prepend (display_x11->error_traps, trap);
}

static int
gdk_x11_display_error_trap_pop_internal (GdkDisplay *display,
                                         gboolean    need_code)
{
  GdkX11Display *display_x11;
  GdkErrorTrap *trap;
  GSList *tmp_list;
  int result;

  display_x11 = GDK_X11_DISPLAY (display);

  g_return_val_if_fail (display_x11->error_traps != NULL, Success);

  /* Find the first trap that hasn't been popped already */
  trap = NULL; /* quiet gcc */
  for (tmp_list = display_x11->error_traps;
       tmp_list != NULL;
       tmp_list = tmp_list->next)
    {
      trap = tmp_list->data;

      if (trap->end_sequence == 0)
        break;
    }

  if (trap == NULL)
    {
      g_critical ("gdk_x11_display_error_trap_pop() called without gdk_x11_display_error_trap_push()");
      return Success;
    }
  g_assert (trap->end_sequence == 0);

  /* May need to sync to fill in trap->error_code if we care about
   * getting an error code.
   */
  if (need_code)
    {
      gulong processed_sequence;
      gulong next_sequence;

      next_sequence = XNextRequest (display_x11->xdisplay);
      processed_sequence = XLastKnownRequestProcessed (display_x11->xdisplay);

      /* If our last request was already processed, there is no point
       * in syncing. i.e. if last request was a round trip (or even if
       * we got an event with the serial of a non-round-trip)
       */
      if ((next_sequence - 1) != processed_sequence)
        {
          XSync (display_x11->xdisplay, False);
        }

      result = trap->error_code;
    }
  else
    {
      result = Success;
    }

  /* record end of trap, giving us a range of
   * error sequences we'll ignore.
   */
  trap->end_sequence = XNextRequest (display_x11->xdisplay);

  /* remove the Xlib callback */
  _gdk_x11_error_handler_pop ();

  /* we may already be outdated */
  delete_outdated_error_traps (display_x11);

  return result;
}

/**
 * gdk_x11_display_set_surface_scale:
 * @display: (type GdkX11Display): the display
 * @scale: The new scale value
 *
 * Forces a specific window scale for all windows on this display,
 * instead of using the default or user configured scale. This
 * is can be used to disable scaling support by setting @scale to
 * 1, or to programmatically set the window scale.
 *
 * Once the scale is set by this call it will not change in response
 * to later user configuration changes.
 */
void
gdk_x11_display_set_surface_scale (GdkDisplay *display,
                                  int         scale)
{
  GdkX11Screen *x11_screen;
  gboolean need_reread_settings = FALSE;

  g_return_if_fail (GDK_IS_X11_DISPLAY (display));

  scale = MAX (scale, 1);

  x11_screen = GDK_X11_SCREEN (GDK_X11_DISPLAY (display)->screen);

  if (!x11_screen->fixed_surface_scale)
    {
      x11_screen->fixed_surface_scale = TRUE;

      /* We treat screens with a window scale set differently when
       * reading xsettings, so we need to reread
       */
      need_reread_settings = TRUE;
    }

  _gdk_x11_screen_set_surface_scale (x11_screen, scale);

  if (need_reread_settings)
    _gdk_x11_settings_force_reread (x11_screen);
}


/**
 * gdk_x11_display_error_trap_pop:
 * @display: (type GdkX11Display): the display
 *
 * Pops the error trap pushed by gdk_x11_display_error_trap_push().
 * Will XSync() if necessary and will always block until
 * the error is known to have occurred or not occurred,
 * so the error code can be returned.
 *
 * If you don’t need to use the return value,
 * gdk_x11_display_error_trap_pop_ignored() would be more efficient.
 *
 * Returns: X error code or 0 on success
 */
int
gdk_x11_display_error_trap_pop (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_X11_DISPLAY (display), Success);

  return gdk_x11_display_error_trap_pop_internal (display, TRUE);
}

/**
 * gdk_x11_display_error_trap_pop_ignored:
 * @display: (type GdkX11Display): the display
 *
 * Pops the error trap pushed by gdk_x11_display_error_trap_push().
 * Does not block to see if an error occurred; merely records the
 * range of requests to ignore errors for, and ignores those errors
 * if they arrive asynchronously.
 */
void
gdk_x11_display_error_trap_pop_ignored (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_X11_DISPLAY (display));

  gdk_x11_display_error_trap_pop_internal (display, FALSE);
}

/**
 * gdk_x11_set_sm_client_id:
 * @sm_client_id: (nullable): the client id assigned by the session manager
 *    when the connection was opened, or %NULL to remove the property.
 *
 * Sets the `SM_CLIENT_ID` property on the application’s leader window so that
 * the window manager can save the application’s state using the X11R6 ICCCM
 * session management protocol.
 *
 * See the X Session Management Library documentation for more information on
 * session management and the Inter-Client Communication Conventions Manual
 */
void
gdk_x11_set_sm_client_id (const char *sm_client_id)
{
 GSList *displays, *l;

  g_free (gdk_sm_client_id);
  gdk_sm_client_id = g_strdup (sm_client_id);

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = displays; l; l = l->next)
    {
      if (GDK_IS_X11_DISPLAY (l->data))
        set_sm_client_id (l->data, sm_client_id);
    }

  g_slist_free (displays);
}
 
gsize
gdk_x11_display_get_max_request_size (GdkDisplay *display)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  gsize size;

  size = XExtendedMaxRequestSize (xdisplay);
  if (size <= 0)
    size = XMaxRequestSize (xdisplay);
  
  size = MIN (262144, size - 100);
  return size;
}

/**
 * gdk_x11_display_get_screen:
 * @display: (type GdkX11Display): a `GdkX11Display`
 *
 * Retrieves the `GdkX11Screen` of the @display.
 *
 * Returns: (transfer none): the `GdkX11Screen`
 */
GdkX11Screen *
gdk_x11_display_get_screen (GdkDisplay *display)
{
  return GDK_X11_DISPLAY (display)->screen;
}

static GdkKeymap *
gdk_x11_display_get_keymap (GdkDisplay *display)
{
  GdkX11Display *display_x11;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_x11 = GDK_X11_DISPLAY (display);

  if (!display_x11->keymap)
    {
      display_x11->keymap = g_object_new (GDK_TYPE_X11_KEYMAP, NULL);
      display_x11->keymap->display = display; /* beware of ref cycle */
    }

  return display_x11->keymap;
}

static GdkSeat *
gdk_x11_display_get_default_seat (GdkDisplay *display)
{
  GList *seats, *l;
  int device_id;
  gboolean result = FALSE;

  seats = gdk_display_list_seats (display);

  gdk_x11_display_error_trap_push (display);
  result = XIGetClientPointer (GDK_DISPLAY_XDISPLAY (display),
                               None, &device_id);
  gdk_x11_display_error_trap_pop_ignored (display);

  for (l = seats; l; l = l->next)
    {
      GdkDevice *pointer;

      pointer = gdk_seat_get_pointer (l->data);

      if (gdk_x11_device_get_id (pointer) == device_id || !result)
        {
          GdkSeat *seat = l->data;
          g_list_free (seats);

          return seat;
        }
    }

  g_list_free (seats);

  return NULL;
}

static GListModel *
gdk_x11_display_get_monitors (GdkDisplay *display)
{
  GdkX11Display *self = GDK_X11_DISPLAY (display);

  return G_LIST_MODEL (self->monitors);
}

/**
 * gdk_x11_display_get_primary_monitor:
 * @display: (type GdkX11Display): a `GdkDisplay`
 *
 * Gets the primary monitor for the display.
 *
 * The primary monitor is considered the monitor where the “main desktop”
 * lives. While normal application surfaces typically allow the window
 * manager to place the surfaces, specialized desktop applications
 * such as panels should place themselves on the primary monitor.
 *
 * If no monitor is the designated primary monitor, any monitor
 * (usually the first) may be returned.
 *
 * Returns: (transfer none): the primary monitor, or any monitor if no
 *   primary monitor is configured by the user
 */
GdkMonitor *
gdk_x11_display_get_primary_monitor (GdkDisplay *display)
{
  GdkX11Display *self = GDK_X11_DISPLAY (display);
  GdkMonitor *monitor;

  monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), self->primary_monitor);
  if (monitor == NULL)
    monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), 0);

  /* because g_list_model_get_item() returns a ref */
  if (monitor)
    g_object_unref (monitor);

  return monitor;
}

int
gdk_x11_display_get_window_depth (GdkX11Display *display)
{
  return display->window_depth;
}

Visual *
gdk_x11_display_get_window_visual (GdkX11Display *display)
{
  return display->window_visual;
}

Colormap
gdk_x11_display_get_window_colormap (GdkX11Display *display)
{
  return display->window_colormap;
}

static gboolean
gdk_x11_display_get_setting (GdkDisplay  *display,
                             const char *name,
                             GValue      *value)
{
  return gdk_x11_screen_get_setting (GDK_X11_DISPLAY (display)->screen, name, value);
}

GList *
gdk_x11_display_get_toplevel_windows (GdkDisplay *display)
{
  return GDK_X11_DISPLAY (display)->toplevels;
}

static gboolean
gdk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                 GValue                *return_accu,
                                 const GValue          *handler_return,
                                 gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

static XVisualInfo *
gdk_x11_display_get_visual_info_for_visual (GdkX11Display  *self,
                                            VisualID        visualid)
{
  XVisualInfo template, *visinfo;
  int nvisuals;

  template.screen = self->screen->screen_num;
  template.visualid = visualid;

  visinfo = XGetVisualInfo (gdk_x11_display_get_xdisplay (GDK_DISPLAY (self)),
                            VisualScreenMask | VisualIDMask,
                            &template,
                            &nvisuals);
  g_warn_if_fail (nvisuals == 1);
  
  return visinfo;
}

static gboolean
visual_is_rgba (XVisualInfo *visinfo)
{
  return
    visinfo->depth == 32 &&
    visinfo->visual->red_mask   == 0xff0000 &&
    visinfo->visual->green_mask == 0x00ff00 &&
    visinfo->visual->blue_mask  == 0x0000ff;
}

static guint
gdk_x11_display_rate_egl_config (GdkDisplay *display,
                                 gpointer    egl_display,
                                 gpointer    config)
{
  GdkX11Display *self = GDK_X11_DISPLAY (display);
  XVisualInfo *visinfo;
  guint distance;
  int visualid;

  if (!eglGetConfigAttrib (egl_display, config, EGL_NATIVE_VISUAL_ID, &visualid))
    return G_MAXUINT;

  visinfo = gdk_x11_display_get_visual_info_for_visual (self, visualid);
  if (visinfo == NULL)
    return G_MAXUINT;

  distance = GDK_DISPLAY_CLASS (gdk_x11_display_parent_class)->rate_egl_config (display, egl_display, config);

  if (!visual_is_rgba (visinfo))
    distance += 0x100;

  XFree (visinfo);

  return distance;
}

static gboolean
gdk_x11_display_init_gl_backend (GdkX11Display  *self,
                                 Visual        **out_visual,
                                 int            *out_depth,
                                 GError        **error)
{
  GdkDisplay *display = GDK_DISPLAY (self);
  Display *dpy = gdk_x11_display_get_xdisplay (GDK_DISPLAY (self));
  EGLDisplay egl_display;
  XVisualInfo *visinfo;
  int visualid;

  /* No env vars set, do the regular GL initialization.
   * 
   * We try EGL first, but are very picky about what we accept.
   * If that fails, we try to go with GLX instead.
   * And if that also fails, we try EGL again, but this time accept anything.
   *
   * The idea here is that EGL is the preferred method going forward, but GLX is
   * the tried and tested method that we know works. So if we detect issues with
   * EGL, we want to avoid using it in favor of GLX.
   */

  if (!gdk_display_init_egl (display, EGL_PLATFORM_X11_KHR, dpy, FALSE, error))
    {
      g_clear_error (error);

      if (gdk_x11_display_init_glx (self, out_visual, out_depth, error))
        return TRUE;
      
      g_clear_error (error);
      if (!gdk_display_init_egl (display, EGL_PLATFORM_X11_KHR, dpy, TRUE, error))
        return FALSE;
    }

  if (!eglGetConfigAttrib (gdk_display_get_egl_display (display),
                           gdk_display_get_egl_config (display),
                           EGL_NATIVE_VISUAL_ID,
                           &visualid))
    {
      /* We guarantee this when rating configs */
      g_assert_not_reached ();
    }
  visinfo = gdk_x11_display_get_visual_info_for_visual (self, visualid);
  g_assert (visinfo);
  *out_visual = visinfo->visual;
  *out_depth = visinfo->depth;

  egl_display = gdk_display_get_egl_display (display);

  self->egl_version = epoxy_egl_version (egl_display);

  XFree (visinfo);

  return TRUE;
}

static GdkGLContext *
gdk_x11_display_init_gl (GdkDisplay  *display,
                         GError     **error)
{
  GdkX11Display *self = GDK_X11_DISPLAY (display);

  if (!gdk_x11_display_init_gl_backend (self, &self->window_visual, &self->window_depth, error))
    return FALSE;

  gdk_x11_display_init_leader_surface (self);

  if (self->glx_config != NULL)
    return g_object_new (GDK_TYPE_X11_GL_CONTEXT_GLX, "surface", self->leader_gdk_surface, NULL);
  else if (gdk_display_get_egl_display (display))
    return g_object_new (GDK_TYPE_X11_GL_CONTEXT_EGL, "surface", self->leader_gdk_surface, NULL);
  else
    g_return_val_if_reached (NULL);
}

static void
gdk_x11_display_class_init (GdkX11DisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_x11_display_dispose;
  object_class->finalize = gdk_x11_display_finalize;

  display_class->toplevel_type = GDK_TYPE_X11_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_X11_POPUP;
  display_class->cairo_context_type = GDK_TYPE_X11_CAIRO_CONTEXT;
#ifdef GDK_RENDERING_VULKAN
  display_class->vk_context_type = GDK_TYPE_X11_VULKAN_CONTEXT;
  display_class->vk_extension_name = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#endif

  display_class->get_name = gdk_x11_display_get_name;
  display_class->beep = gdk_x11_display_beep;
  display_class->sync = gdk_x11_display_sync;
  display_class->flush = gdk_x11_display_flush;
  display_class->make_default = gdk_x11_display_make_default;
  display_class->has_pending = gdk_x11_display_has_pending;
  display_class->queue_events = _gdk_x11_display_queue_events;
  display_class->get_app_launch_context = _gdk_x11_display_get_app_launch_context;

  display_class->get_next_serial = gdk_x11_display_get_next_serial;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  display_class->get_startup_notification_id = gdk_x11_display_get_startup_notification_id;
G_GNUC_END_IGNORE_DEPRECATIONS
  display_class->notify_startup_complete = gdk_x11_display_notify_startup_complete;
  display_class->get_keymap = gdk_x11_display_get_keymap;

  display_class->init_gl = gdk_x11_display_init_gl;
  display_class->rate_egl_config = gdk_x11_display_rate_egl_config;

  display_class->get_default_seat = gdk_x11_display_get_default_seat;

  display_class->get_monitors = gdk_x11_display_get_monitors;
  display_class->get_setting = gdk_x11_display_get_setting;
  display_class->set_cursor_theme = gdk_x11_display_set_cursor_theme;

  class->xevent = gdk_event_source_xevent;

  /**
   * GdkX11Display::xevent:
   * @display: (type GdkX11Display): the object on which the signal is emitted
   * @xevent: a pointer to the XEvent to process
   *
   * The ::xevent signal is a low level signal that is emitted
   * whenever an XEvent has been received.
   *
   * When handlers to this signal return %TRUE, no other handlers will be
   * invoked. In particular, the default handler for this function is
   * GDK's own event handling mechanism, so by returning %TRUE for an event
   * that GDK expects to translate, you may break GDK and/or GTK+ in
   * interesting ways. You have been warned.
   *
   * If you want this signal handler to queue a `GdkEvent`, you can use
   * gdk_display_put_event().
   *
   * If you are interested in X GenericEvents, bear in mind that
   * XGetEventData() has been already called on the event, and
   * XFreeEventData() will be called afterwards.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  signals[XEVENT] =
    g_signal_new (g_intern_static_string ("xevent"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkX11DisplayClass, xevent),
                  gdk_boolean_handled_accumulator, NULL,
                  _gdk_marshal_BOOLEAN__POINTER,
                  G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  g_signal_set_va_marshaller (signals[XEVENT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_BOOLEAN__POINTERv);

  _gdk_x11_surfaceing_init ();
}
