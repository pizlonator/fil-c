/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <feborges@redhat.com>
 */

/* Fil-C: the pizlonated gtk4 is built without the X11 backend
 * (-D x11-backend=false; pizlix is Wayland-only), so gdk/x11/gdkx.h and the
 * XInput2 helpers do not exist.  The X11 capability checks are compiled only
 * when GDK_WINDOWING_X11 is defined (i.e. when gtk4 has the X11 backend);
 * on Wayland the touchpad knobs are shown unconditionally, which is what
 * the non-X11 fallback below already did. */
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#endif

#include "cc-mouse-caps-helper.h"

#ifdef GDK_WINDOWING_X11
static gboolean
touchpad_check_capabilities_x11 (gboolean *have_two_finger_scrolling,
                                 gboolean *have_edge_scrolling,
                                 gboolean *have_tap_to_click)
{
        GdkDisplay *gdisplay;
        Display *display;
	g_autoptr(GList) devicelist = NULL;
	GList *l;
	Atom realtype, prop_scroll_methods, prop_tapping_enabled;
	int realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data;

        gdisplay = gdk_display_get_default ();
        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	prop_scroll_methods = XInternAtom (display, "libinput Scroll Methods Available", False);
	prop_tapping_enabled = XInternAtom (display, "libinput Tapping Enabled", False);
	if (!prop_scroll_methods || !prop_tapping_enabled)
		return FALSE;

	*have_two_finger_scrolling = FALSE;
	*have_edge_scrolling = FALSE;
	*have_tap_to_click = FALSE;

        gdk_x11_display_error_trap_push (gdisplay);

	devicelist = gdk_seat_get_devices (gdk_display_get_default_seat (gdk_display_get_default ()),
                                           GDK_SEAT_CAPABILITY_ALL_POINTING);
	for (l = devicelist; l != NULL; l = l->next) {
                GdkDevice *device = l->data;
                if (gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
			continue;

		/* xorg-x11-drv-libinput */
		if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop_scroll_methods,
                                    0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for two-finger, edge, on-button scroll available. */

			if (data[0])
				*have_two_finger_scrolling = TRUE;

			if (data[1])
				*have_edge_scrolling = TRUE;

			XFree (data);
		}

		if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop_tapping_enabled,
                                    0, 1, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is boolean for tapping enabled. */
			*have_tap_to_click = TRUE;

			XFree (data);
		}
	}

        gdk_x11_display_error_trap_pop_ignored (gdisplay);

	return TRUE;
}
#endif /* GDK_WINDOWING_X11 */

gboolean
cc_touchpad_check_capabilities (gboolean *have_two_finger_scrolling,
                                gboolean *have_edge_scrolling,
                                gboolean *have_tap_to_click)
{
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		return touchpad_check_capabilities_x11 (have_two_finger_scrolling,
                                                        have_edge_scrolling,
                                                        have_tap_to_click);
#endif /* GDK_WINDOWING_X11 */
	/* else we unconditionally show all touchpad knobs */
        *have_two_finger_scrolling = TRUE;
        *have_edge_scrolling = TRUE;
        *have_tap_to_click = TRUE;
	return FALSE;
}

gboolean
cc_synaptics_check (void)
{
        gboolean have_synaptics = FALSE;

#ifndef GDK_WINDOWING_X11
        return FALSE;
#else
        GdkDisplay *gdisplay;
        Display *display;
        g_autoptr(GList) devicelist = NULL;
        GList *l;
        Atom prop, realtype;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
                return FALSE;

        gdisplay = gdk_display_get_default ();
        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        prop = XInternAtom (display, "Synaptics Capabilities", False);

        gdk_x11_display_error_trap_push (gdisplay);

        devicelist = gdk_seat_get_devices (gdk_display_get_default_seat (gdk_display_get_default ()),
                                           GDK_SEAT_CAPABILITY_ALL_POINTING);
        for (l = devicelist; l != NULL; l = l->next) {
                GdkDevice *device = l->data;

                if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop,
                                    0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
                        have_synaptics = TRUE;
                        XFree (data);
                }

                if (have_synaptics)
                        break;
        }

        gdk_x11_display_error_trap_pop_ignored (gdisplay);

        return have_synaptics;
#endif /* GDK_WINDOWING_X11 */
}
