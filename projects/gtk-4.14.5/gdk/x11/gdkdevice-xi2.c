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

#include "gdkx11device-xi2.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-xi2-private.h"

#include <glib/gi18n-lib.h>
#include "gdkasync.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

#include "gdkprivate.h"

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include <math.h>

typedef struct _ScrollValuator ScrollValuator;

struct _ScrollValuator
{
  guint n_valuator       : 4;
  guint direction        : 4;
  guint last_value_valid : 1;
  double last_value;
  double increment;
};

struct _GdkX11DeviceXI2
{
  GdkDevice parent_instance;

  int device_id;
  GArray *scroll_valuators;
  double *last_axes;
  GdkX11DeviceType device_type;
};

struct _GdkX11DeviceXI2Class
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkX11DeviceXI2, gdk_x11_device_xi2, GDK_TYPE_DEVICE)


static void gdk_x11_device_xi2_finalize     (GObject      *object);
static void gdk_x11_device_xi2_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void gdk_x11_device_xi2_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void gdk_x11_device_xi2_set_surface_cursor (GdkDevice *device,
                                                  GdkSurface *surface,
                                                  GdkCursor *cursor);

static GdkGrabStatus gdk_x11_device_xi2_grab   (GdkDevice     *device,
                                                GdkSurface     *surface,
                                                gboolean       owner_events,
                                                GdkEventMask   event_mask,
                                                GdkSurface     *confine_to,
                                                GdkCursor     *cursor,
                                                guint32        time_);
static void          gdk_x11_device_xi2_ungrab (GdkDevice     *device,
                                                guint32        time_);

static GdkSurface * gdk_x11_device_xi2_surface_at_position (GdkDevice       *device,
                                                            double          *win_x,
                                                            double          *win_y,
                                                            GdkModifierType *mask);


enum {
  PROP_0,
  PROP_DEVICE_ID
};

static void
gdk_x11_device_xi2_class_init (GdkX11DeviceXI2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  object_class->finalize = gdk_x11_device_xi2_finalize;
  object_class->get_property = gdk_x11_device_xi2_get_property;
  object_class->set_property = gdk_x11_device_xi2_set_property;

  device_class->set_surface_cursor = gdk_x11_device_xi2_set_surface_cursor;
  device_class->grab = gdk_x11_device_xi2_grab;
  device_class->ungrab = gdk_x11_device_xi2_ungrab;
  device_class->surface_at_position = gdk_x11_device_xi2_surface_at_position;

  g_object_class_install_property (object_class,
                                   PROP_DEVICE_ID,
                                   g_param_spec_int ("device-id", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
}

static void
gdk_x11_device_xi2_init (GdkX11DeviceXI2 *device)
{
  device->scroll_valuators = g_array_new (FALSE, FALSE, sizeof (ScrollValuator));
}

static void
gdk_x11_device_xi2_finalize (GObject *object)
{
  GdkX11DeviceXI2 *device = GDK_X11_DEVICE_XI2 (object);

  g_array_free (device->scroll_valuators, TRUE);
  g_free (device->last_axes);

  G_OBJECT_CLASS (gdk_x11_device_xi2_parent_class)->finalize (object);
}

static void
gdk_x11_device_xi2_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_int (value, device_xi2->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_xi2_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      device_xi2->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_xi2_set_surface_cursor (GdkDevice *device,
                                      GdkSurface *surface,
                                      GdkCursor *cursor)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);

  /* Non-logical devices don't have a cursor */
  if (device_xi2->device_type != GDK_X11_DEVICE_TYPE_LOGICAL)
    return;

  if (cursor)
    XIDefineCursor (GDK_SURFACE_XDISPLAY (surface),
                    device_xi2->device_id,
                    GDK_SURFACE_XID (surface),
                    gdk_x11_display_get_xcursor (GDK_SURFACE_DISPLAY (surface), cursor));
  else
    XIUndefineCursor (GDK_SURFACE_XDISPLAY (surface),
                      device_xi2->device_id,
                      GDK_SURFACE_XID (surface));
}

void
gdk_x11_device_xi2_query_state (GdkDevice        *device,
                                GdkSurface        *surface,
                                double           *win_x,
                                double           *win_y,
                                GdkModifierType  *mask)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  GdkX11Screen *default_screen;
  Window xroot_window, xchild_window, xwindow;
  double xroot_x, xroot_y, xwin_x, xwin_y;
  XIButtonState button_state;
  XIModifierState mod_state;
  XIGroupState group_state;
  int scale;

  display = gdk_device_get_display (device);
  default_screen = GDK_X11_DISPLAY (display)->screen;
  if (surface == NULL)
    {
      xwindow = GDK_DISPLAY_XROOTWIN (display);
      scale = default_screen->surface_scale;
    }
  else
    {
      xwindow = GDK_SURFACE_XID (surface);
      scale = GDK_X11_SURFACE (surface)->surface_scale;
    }

  if (!GDK_X11_DISPLAY (display)->trusted_client ||
      !XIQueryPointer (GDK_DISPLAY_XDISPLAY (display),
                       device_xi2->device_id,
                       xwindow,
                       &xroot_window,
                       &xchild_window,
                       &xroot_x, &xroot_y,
                       &xwin_x, &xwin_y,
                       &button_state,
                       &mod_state,
                       &group_state))
    {
      XSetWindowAttributes attributes;
      Display *xdisplay;
      Window w;

      /* FIXME: untrusted clients not multidevice-safe */
      xdisplay = GDK_SCREEN_XDISPLAY (default_screen);
      xwindow = GDK_SCREEN_XROOTWIN (default_screen);

      w = XCreateWindow (xdisplay, xwindow, 0, 0, 1, 1, 0,
                         CopyFromParent, InputOnly, CopyFromParent,
                         0, &attributes);
      XIQueryPointer (xdisplay, device_xi2->device_id,
                      w,
                      &xroot_window,
                      &xchild_window,
                      &xroot_x, &xroot_y,
                      &xwin_x, &xwin_y,
                      &button_state,
                      &mod_state,
                      &group_state);
      XDestroyWindow (xdisplay, w);
    }

  if (win_x)
    *win_x = xwin_x / scale;

  if (win_y)
    *win_y = xwin_y / scale;

  if (mask)
    *mask = _gdk_x11_device_xi2_translate_state (&mod_state, &button_state, &group_state);

  free (button_state.mask);
}

static GdkGrabStatus
gdk_x11_convert_grab_status (int status)
{
  switch (status)
    {
    case GrabSuccess:
      return GDK_GRAB_SUCCESS;
    case AlreadyGrabbed:
      return GDK_GRAB_ALREADY_GRABBED;
    case GrabInvalidTime:
      return GDK_GRAB_INVALID_TIME;
    case GrabNotViewable:
      return GDK_GRAB_NOT_VIEWABLE;
    case GrabFrozen:
      return GDK_GRAB_FROZEN;
    default:
      g_assert_not_reached();
      return 0;
    }
}

static GdkGrabStatus
gdk_x11_device_xi2_grab (GdkDevice    *device,
                         GdkSurface    *surface,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkSurface    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkX11DeviceManagerXI2 *device_manager_xi2;
  GdkDisplay *display;
  XIEventMask mask;
  Window xwindow;
  Cursor xcursor;
  int status;

  display = gdk_device_get_display (device);
  device_manager_xi2 = GDK_X11_DEVICE_MANAGER_XI2 (GDK_X11_DISPLAY (display)->device_manager);

  /* FIXME: confine_to is actually unused */

  xwindow = GDK_SURFACE_XID (surface);

  if (!cursor)
    xcursor = None;
  else
    {
      xcursor = gdk_x11_display_get_xcursor (display, cursor);
    }

  mask.deviceid = device_xi2->device_id;
  mask.mask = _gdk_x11_device_xi2_translate_event_mask (device_manager_xi2,
                                                        event_mask,
                                                        &mask.mask_len);

  status = XIGrabDevice (GDK_DISPLAY_XDISPLAY (display),
                         device_xi2->device_id,
                         xwindow,
                         time_,
                         xcursor,
                         GrabModeAsync, GrabModeAsync,
                         owner_events,
                         &mask);

  g_free (mask.mask);

  _gdk_x11_display_update_grab_info (display, device, status);

  return gdk_x11_convert_grab_status (status);
}

static void
gdk_x11_device_xi2_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  gulong serial;

  display = gdk_device_get_display (device);
  serial = NextRequest (GDK_DISPLAY_XDISPLAY (display));

  XIUngrabDevice (GDK_DISPLAY_XDISPLAY (display), device_xi2->device_id, time_);

  _gdk_x11_display_update_grab_info_ungrab (display, device, time_, serial);
}

static GdkSurface *
gdk_x11_device_xi2_surface_at_position (GdkDevice       *device,
                                        double          *win_x,
                                        double          *win_y,
                                        GdkModifierType *mask)
{
  GdkX11Surface *impl;
  GdkX11DeviceXI2 *device_xi2 = GDK_X11_DEVICE_XI2 (device);
  GdkDisplay *display;
  GdkX11Screen *screen;
  Display *xdisplay;
  GdkSurface *surface;
  Window xwindow, root, child, last = None;
  double xroot_x, xroot_y, xwin_x, xwin_y;
  XIButtonState button_state = { 0 };
  XIModifierState mod_state;
  XIGroupState group_state;
  Bool retval;

  display = gdk_device_get_display (device);
  screen = GDK_X11_DISPLAY (display)->screen;

  gdk_x11_display_error_trap_push (display);

  /* This function really only works if the mouse pointer is held still
   * during its operation. If it moves from one leaf window to another
   * than we'll end up with inaccurate values for win_x, win_y
   * and the result.
   */
  gdk_x11_display_grab (display);

  xdisplay = GDK_SCREEN_XDISPLAY (screen);
  xwindow = GDK_SCREEN_XROOTWIN (screen);

  if (G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    {
      XIQueryPointer (xdisplay,
                      device_xi2->device_id,
                      xwindow,
                      &root, &child,
                      &xroot_x, &xroot_y,
                      &xwin_x, &xwin_y,
                      &button_state,
                      &mod_state,
                      &group_state);

      if (root == xwindow)
        xwindow = child;
      else
        xwindow = root;
    }
  else
    {
      int width, height;
      GList *toplevels, *list;
      Window pointer_window;

      /* FIXME: untrusted clients case not multidevice-safe */
      pointer_window = None;

      toplevels = gdk_x11_display_get_toplevel_windows (display);
      for (list = toplevels; list != NULL; list = list->next)
        {
          surface = GDK_SURFACE (list->data);
          xwindow = GDK_SURFACE_XID (surface);

          /* Free previous button mask, if any */
          g_free (button_state.mask);

          retval = XIQueryPointer (xdisplay,
                                   device_xi2->device_id,
                                   xwindow,
                                   &root, &child,
                                   &xroot_x, &xroot_y,
                                   &xwin_x, &xwin_y,
                                   &button_state,
                                   &mod_state,
                                   &group_state);
          if (!retval)
            continue;

          if (child != None)
            {
              pointer_window = child;
              break;
            }
          gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
          if (xwin_x >= 0 && xwin_y >= 0 && xwin_x < width && xwin_y < height)
            {
              /* A childless toplevel, or below another window? */
              XSetWindowAttributes attributes;
              Window w;

              free (button_state.mask);

              w = XCreateWindow (xdisplay, xwindow, (int)xwin_x, (int)xwin_y, 1, 1, 0,
                                 CopyFromParent, InputOnly, CopyFromParent,
                                 0, &attributes);
              XMapWindow (xdisplay, w);
              XIQueryPointer (xdisplay,
                              device_xi2->device_id,
                              xwindow,
                              &root, &child,
                              &xroot_x, &xroot_y,
                              &xwin_x, &xwin_y,
                              &button_state,
                              &mod_state,
                              &group_state);
              XDestroyWindow (xdisplay, w);
              if (child == w)
                {
                  pointer_window = xwindow;
                  break;
                }
            }

          if (pointer_window != None)
            break;
        }

      xwindow = pointer_window;
    }

  while (xwindow)
    {
      last = xwindow;
      free (button_state.mask);

      retval = XIQueryPointer (xdisplay,
                               device_xi2->device_id,
                               xwindow,
                               &root, &xwindow,
                               &xroot_x, &xroot_y,
                               &xwin_x, &xwin_y,
                               &button_state,
                               &mod_state,
                               &group_state);
      if (!retval)
        break;

      if (last != root &&
          (surface = gdk_x11_surface_lookup_for_display (display, last)) != NULL)
        {
          xwindow = last;
          break;
        }
    }

  gdk_x11_display_ungrab (display);

  if (gdk_x11_display_error_trap_pop (display) == 0)
    {
      surface = gdk_x11_surface_lookup_for_display (display, last);
      impl = NULL;
      if (surface)
        impl = GDK_X11_SURFACE (surface);

      if (mask)
        *mask = _gdk_x11_device_xi2_translate_state (&mod_state, &button_state, &group_state);

      free (button_state.mask);
    }
  else
    {
      surface = NULL;

      if (mask)
        *mask = 0;
    }

  if (win_x)
    *win_x = (surface) ? (xwin_x / impl->surface_scale) : -1;

  if (win_y)
    *win_y = (surface) ? (xwin_y / impl->surface_scale) : -1;


  return surface;
}

guchar *
_gdk_x11_device_xi2_translate_event_mask (GdkX11DeviceManagerXI2 *device_manager_xi2,
                                          GdkEventMask            event_mask,
                                          int                    *len)
{
  guchar *mask;
  int minor;

  g_object_get (device_manager_xi2, "minor", &minor, NULL);

  *len = XIMaskLen (XI_LASTEVENT);
  mask = g_new0 (guchar, *len);

  if (event_mask & GDK_POINTER_MOTION_MASK)
    XISetMask (mask, XI_Motion);

  if (event_mask & GDK_BUTTON_MOTION_MASK ||
      event_mask & GDK_BUTTON1_MOTION_MASK ||
      event_mask & GDK_BUTTON2_MOTION_MASK ||
      event_mask & GDK_BUTTON3_MOTION_MASK)
    {
      XISetMask (mask, XI_ButtonPress);
      XISetMask (mask, XI_ButtonRelease);
      XISetMask (mask, XI_Motion);
    }

  if (event_mask & GDK_SCROLL_MASK)
    {
      XISetMask (mask, XI_ButtonPress);
      XISetMask (mask, XI_ButtonRelease);
    }

  if (event_mask & GDK_BUTTON_PRESS_MASK)
    XISetMask (mask, XI_ButtonPress);

  if (event_mask & GDK_BUTTON_RELEASE_MASK)
    XISetMask (mask, XI_ButtonRelease);

  if (event_mask & GDK_KEY_PRESS_MASK)
    XISetMask (mask, XI_KeyPress);

  if (event_mask & GDK_KEY_RELEASE_MASK)
    XISetMask (mask, XI_KeyRelease);

  if (event_mask & GDK_ENTER_NOTIFY_MASK)
    XISetMask (mask, XI_Enter);

  if (event_mask & GDK_LEAVE_NOTIFY_MASK)
    XISetMask (mask, XI_Leave);

  if (event_mask & GDK_FOCUS_CHANGE_MASK)
    {
      XISetMask (mask, XI_FocusIn);
      XISetMask (mask, XI_FocusOut);
    }

#ifdef XINPUT_2_2
  /* XInput 2.2 includes multitouch support */
  if (minor >= 2 &&
      event_mask & GDK_TOUCH_MASK)
    {
      XISetMask (mask, XI_TouchBegin);
      XISetMask (mask, XI_TouchUpdate);
      XISetMask (mask, XI_TouchEnd);
    }
#endif /* XINPUT_2_2 */

#ifdef XINPUT_2_4
  /* XInput 2.4 includes touchpad gesture support */
  if (minor >= 4 &&
      event_mask & GDK_TOUCHPAD_GESTURE_MASK)
    {
      XISetMask (mask, XI_GesturePinchBegin);
      XISetMask (mask, XI_GesturePinchUpdate);
      XISetMask (mask, XI_GesturePinchEnd);
      XISetMask (mask, XI_GestureSwipeBegin);
      XISetMask (mask, XI_GestureSwipeUpdate);
      XISetMask (mask, XI_GestureSwipeEnd);
    }
#endif

  return mask;
}

guint
_gdk_x11_device_xi2_translate_state (XIModifierState *mods_state,
                                     XIButtonState   *buttons_state,
                                     XIGroupState    *group_state)
{
  guint state = 0;

  if (mods_state)
    state = mods_state->effective;

  if (buttons_state)
    {
      int len, i;

      /* We're only interested in the first 3 buttons */
      len = MIN (3, buttons_state->mask_len * 8);

      for (i = 1; i <= len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          switch (i)
            {
            case 1:
              state |= GDK_BUTTON1_MASK;
              break;
            case 2:
              state |= GDK_BUTTON2_MASK;
              break;
            case 3:
              state |= GDK_BUTTON3_MASK;
              break;
            default:
              break;
            }
        }
    }

  if (group_state)
    state |= (group_state->effective) << 13;

  return state;
}

#ifdef XINPUT_2_4
guint
_gdk_x11_device_xi2_gesture_type_to_phase (int evtype, int flags)
{
  switch (evtype)
    {
    case XI_GesturePinchBegin:
    case XI_GestureSwipeBegin:
      return GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;

    case XI_GesturePinchUpdate:
    case XI_GestureSwipeUpdate:
      return GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;

    case XI_GesturePinchEnd:
      if (flags & XIGesturePinchEventCancelled)
        return GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      return GDK_TOUCHPAD_GESTURE_PHASE_END;

    case XI_GestureSwipeEnd:
      if (flags & XIGestureSwipeEventCancelled)
        return GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      return GDK_TOUCHPAD_GESTURE_PHASE_END;
    default:
      g_assert_not_reached ();
      return GDK_TOUCHPAD_GESTURE_PHASE_END;
    }
}
#endif /* XINPUT_2_4 */

void
_gdk_x11_device_xi2_add_scroll_valuator (GdkX11DeviceXI2    *device,
                                         guint               n_valuator,
                                         GdkScrollDirection  direction,
                                         double              increment)
{
  ScrollValuator scroll;

  g_return_if_fail (GDK_IS_X11_DEVICE_XI2 (device));
  g_return_if_fail (n_valuator < gdk_device_get_n_axes (GDK_DEVICE (device)));

  scroll.n_valuator = n_valuator;
  scroll.direction = direction;
  scroll.last_value_valid = FALSE;
  scroll.increment = increment;

  g_array_append_val (device->scroll_valuators, scroll);
}

gboolean
_gdk_x11_device_xi2_get_scroll_delta (GdkX11DeviceXI2    *device,
                                      guint               n_valuator,
                                      double              valuator_value,
                                      GdkScrollDirection *direction_ret,
                                      double             *delta_ret)
{
  guint i;

  g_return_val_if_fail (GDK_IS_X11_DEVICE_XI2 (device), FALSE);
  g_return_val_if_fail (n_valuator < gdk_device_get_n_axes (GDK_DEVICE (device)), FALSE);

  for (i = 0; i < device->scroll_valuators->len; i++)
    {
      ScrollValuator *scroll;

      scroll = &g_array_index (device->scroll_valuators, ScrollValuator, i);

      if (scroll->n_valuator == n_valuator)
        {
          if (direction_ret)
            *direction_ret = scroll->direction;

          if (delta_ret)
            *delta_ret = 0;

          if (scroll->last_value_valid)
            {
              if (delta_ret)
                *delta_ret = (valuator_value - scroll->last_value) / scroll->increment;

              scroll->last_value = valuator_value;
            }
          else
            {
              scroll->last_value = valuator_value;
              scroll->last_value_valid = TRUE;
            }

          return TRUE;
        }
    }

  return FALSE;
}

void
_gdk_device_xi2_reset_scroll_valuators (GdkX11DeviceXI2 *device)
{
  guint i;

  for (i = 0; i < device->scroll_valuators->len; i++)
    {
      ScrollValuator *scroll;

      scroll = &g_array_index (device->scroll_valuators, ScrollValuator, i);
      scroll->last_value_valid = FALSE;
    }
}

void
_gdk_device_xi2_unset_scroll_valuators (GdkX11DeviceXI2 *device)
{
  if (device->scroll_valuators->len > 0)
    g_array_remove_range (device->scroll_valuators, 0,
                          device->scroll_valuators->len);
}

int
_gdk_x11_device_xi2_get_id (GdkX11DeviceXI2 *device)
{
  g_return_val_if_fail (GDK_IS_X11_DEVICE_XI2 (device), 0);

  return device->device_id;
}

double
gdk_x11_device_xi2_get_last_axis_value (GdkX11DeviceXI2 *device,
                                        int              n_axis)
{
  if (n_axis >= gdk_device_get_n_axes (GDK_DEVICE (device)))
    return 0;

  if (!device->last_axes)
    return 0;

  return device->last_axes[n_axis];
}

void
gdk_x11_device_xi2_store_axes (GdkX11DeviceXI2 *device,
                               double          *axes,
                               int              n_axes)
{
  g_free (device->last_axes);

  if (axes && n_axes)
    device->last_axes = g_memdup2 (axes, sizeof (double) * n_axes);
  else
    device->last_axes = NULL;
}

GdkX11DeviceType
gdk_x11_device_xi2_get_device_type (GdkX11DeviceXI2 *device)
{
  return device->device_type;
}

void
gdk_x11_device_xi2_set_device_type (GdkX11DeviceXI2  *device,
                                    GdkX11DeviceType  type)
{
  device->device_type = type;
}
