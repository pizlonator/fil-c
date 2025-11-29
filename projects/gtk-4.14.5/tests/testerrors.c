/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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


#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include "x11/gdkx.h"

static void
test_error_trapping (GdkDisplay *gdk_display)
{
  Display *d;
  int dummy;
  int error;

  d = GDK_DISPLAY_XDISPLAY (gdk_display);

  /* verify that we can catch errors */
  gdk_x11_display_error_trap_push (gdk_display);
  XListProperties (d, 0, &dummy); /* round trip */
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadWindow);

  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345); /* not a round trip */
  XSetCloseDownMode (d, DestroyAll);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);

  /* try the same without sync */
  gdk_x11_display_error_trap_push (gdk_display);
  XListProperties (d, 0, &dummy);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);

  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  XSetCloseDownMode (d, DestroyAll);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);

  XSync (d, TRUE);

  /* verify that we can catch with nested traps; inner-most
   * active trap gets the error */
  gdk_x11_display_error_trap_push (gdk_display);
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == Success);

  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_push (gdk_display);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == Success);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);

  /* try nested, without sync */
  gdk_x11_display_error_trap_push (gdk_display);
  gdk_x11_display_error_trap_push (gdk_display);
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);

  XSync (d, TRUE);

  /* try nested, without sync, with interleaved calls */
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);

  XSync (d, TRUE);

  /* don't want to get errors that weren't in our push range */
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  gdk_x11_display_error_trap_push (gdk_display);
  XSync (d, TRUE); /* not an error */
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == Success);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);

  /* non-roundtrip non-error request after error request, inside trap */
  gdk_x11_display_error_trap_push (gdk_display);
  XSetCloseDownMode (d, 12345);
  XMapWindow (d, DefaultRootWindow (d));
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);

  /* a non-roundtrip non-error request before error request, inside trap */
  gdk_x11_display_error_trap_push (gdk_display);
  XMapWindow (d, DefaultRootWindow (d));
  XSetCloseDownMode (d, 12345);
  error = gdk_x11_display_error_trap_pop (gdk_display);
  g_assert_true (error == BadValue);

  /* Not part of any test, just a double-check
   * that all errors have arrived
   */
  XSync (d, TRUE);
}

int
main (int argc, char *argv[])
{
  GdkDisplay *gdk_display;
  GdkDisplay *extra_display;

  gtk_init ();

  gdk_display = gdk_display_get_default ();
  test_error_trapping (gdk_display);

  extra_display = gdk_display_open (NULL);
  test_error_trapping (extra_display);
  gdk_display_close (extra_display);

  test_error_trapping (gdk_display_get_default ());

  /* open a display with a trap pushed and see if we
   * get confused
   */
  gdk_x11_display_error_trap_push (gdk_display);
  gdk_x11_display_error_trap_push (gdk_display);

  extra_display = gdk_display_open (NULL);
  test_error_trapping (extra_display);
  gdk_display_close (extra_display);

  gdk_x11_display_error_trap_pop_ignored (gdk_display);
  gdk_x11_display_error_trap_pop_ignored (gdk_display);

  test_error_trapping (gdk_display_get_default ());

  /* reassure us that the tests ran. */
  g_print("All errors properly trapped.\n");

  return 0;
}

