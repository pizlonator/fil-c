/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "gdkdeviceprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkasync.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"

#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

/* non-GDK previous error handler */
typedef int (*GdkXErrorHandler) (Display *, XErrorEvent *);
static GdkXErrorHandler _gdk_old_error_handler;
/* number of times we've pushed the GDK error handler */
static int _gdk_error_handler_push_count = 0;

/*
 * Private function declarations
 */

static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);

void
_gdk_x11_surfaceing_init (void)
{
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);
}

/*
 * _gdk_x11_surface_grab_check_unmap:
 * @surface: a `GdkSurface`
 * @serial: serial from Unmap event (or from NextRequest(display)
 *   if the unmap is being done by this client.)
 *
 * Checks to see if an unmap request or event causes the current
 * grab surface to become not viewable, and if so, clear the
 * the pointer we keep to it.
 **/
void
_gdk_x11_surface_grab_check_unmap (GdkSurface *surface,
                                  gulong     serial)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkSeat *seat;
  GList *devices, *d;

  seat = gdk_display_get_default_seat (display);

  devices = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);
  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
  devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

  /* End all grabs on the newly hidden surface */
  for (d = devices; d; d = d->next)
    _gdk_display_end_device_grab (display, d->data, serial, surface, TRUE);

  g_list_free (devices);
}

/*
 * _gdk_x11_surface_grab_check_destroy:
 * @surface: a `GdkSurface`
 * 
 * Checks to see if surface is the current grab surface, and if
 * so, clear the current grab surface.
 **/
void
_gdk_x11_surface_grab_check_destroy (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkSeat *seat;
  GdkDeviceGrabInfo *grab;
  GList *devices, *d;

  seat = gdk_display_get_default_seat (display);

  devices = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);
  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
  devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

  for (d = devices; d; d = d->next)
    {
      /* Make sure there is no lasting grab in this native surface */
      grab = _gdk_display_get_last_device_grab (display, d->data);

      if (grab && grab->surface == surface)
        {
          /* We don't know the actual serial to end, but it
             doesn't really matter as this only happens
             after we get told of the destroy from the
             server so we know its ended in the server,
             just make sure its ended. */
          grab->serial_end = grab->serial_start;
          grab->implicit_ungrab = TRUE;
        }
    }

  g_list_free (devices);
}

/*
 *--------------------------------------------------------------
 * gdk_x_io_error
 *
 *   The X I/O error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error originated from.
 *
 * Results:
 *   An X I/O error basically means we lost our connection
 *   to the X server. There is not much we can do to
 *   continue, so simply print an error message and exit.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_io_error (Display *display)
{
  /* This is basically modelled after the code in XLib. We need
   * an explicit error handler here, so we can disable our atexit()
   * which would otherwise cause a nice segfault.
   * We g_debug() instead of g_warning(), because g_warning()
   * could possibly be redirected to the log
   */
  g_debug ("%s: Fatal IO error %d (%s) on X server %s.\n",
           g_get_prgname (),
           errno, g_strerror (errno),
           display ? DisplayString (display) : "");

  _exit (1);
}

/* X error handler. Keep the name the same because people are used to
 * breaking on it in the debugger.
 */
static int
gdk_x_error (Display	 *xdisplay,
	     XErrorEvent *error)
{
  if (error->error_code)
    {
      GdkDisplay *error_display;
      GdkDisplayManager *manager;
      GSList *displays;

      /* Figure out which GdkDisplay if any got the error. */
      error_display = NULL;
      manager = gdk_display_manager_get ();
      displays = gdk_display_manager_list_displays (manager);
      while (displays != NULL)
        {
          GdkX11Display *gdk_display = displays->data;

          if (GDK_IS_X11_DISPLAY (gdk_display) &&
              xdisplay == gdk_display->xdisplay)
            {
              error_display = GDK_DISPLAY (gdk_display);
              g_slist_free (displays);
              displays = NULL;
            }
          else
            {
              displays = g_slist_delete_link (displays, displays);
            }
        }

      if (error_display == NULL)
        {
          /* Error on an X display not opened by GDK. Ignore. */

          return 0;
        }
      else
        {
          _gdk_x11_display_error_event (error_display, error);
        }
    }

  return 0;
}

void
_gdk_x11_error_handler_push (void)
{
  GdkXErrorHandler previous;

  previous = XSetErrorHandler (gdk_x_error);

  if (_gdk_error_handler_push_count > 0)
    {
      if (previous != gdk_x_error)
        g_warning ("XSetErrorHandler() called with a GDK error trap pushed. Don't do that.");
    }
  else
    {
      _gdk_old_error_handler = previous;
    }

  _gdk_error_handler_push_count += 1;
}

void
_gdk_x11_error_handler_pop  (void)
{
  g_return_if_fail (_gdk_error_handler_push_count > 0);

  _gdk_error_handler_push_count -= 1;

  if (_gdk_error_handler_push_count == 0)
    {
      XSetErrorHandler (_gdk_old_error_handler);
      _gdk_old_error_handler = NULL;
    }
}

int
_gdk_x11_display_send_xevent (GdkDisplay *display,
                              Window      window,
                              gboolean    propagate,
                              glong       event_mask,
                              XEvent     *event_send)
{
  gboolean result;

  if (gdk_display_is_closed (display))
    return FALSE;

  gdk_x11_display_error_trap_push (display);
  result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window,
                       propagate, event_mask, event_send);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);

  if (gdk_x11_display_error_trap_pop (display))
    return FALSE;

  return result;
}

void
_gdk_x11_region_get_xrectangles (const cairo_region_t *region,
                                 int              x_offset,
                                 int              y_offset,
                                 int              scale,
                                 XRectangle     **rects,
                                 int             *n_rects)
{
  XRectangle *rectangles;
  cairo_rectangle_int_t box;
  int i, n;
  
  n = cairo_region_num_rectangles (region);
  rectangles = g_new (XRectangle, n);

  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &box);
      rectangles[i].x = CLAMP ((box.x + x_offset) * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].y = CLAMP ((box.y + y_offset) * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].width = CLAMP (box.width * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].height = CLAMP (box.height * scale, G_MINSHORT, G_MAXSHORT);
    }

  *n_rects = n;
  *rects = rectangles;
}
