 /*
 * gdkscreen-x11.c
 *
 * Copyright 2001 Sun Microsystems Inc.
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

#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "xsettings-client.h"
#include "gdkmonitor-x11.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>

#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

static void gdk_x11_screen_dispose  (GObject      *object);
static void gdk_x11_screen_finalize (GObject      *object);
static void init_randr_support	    (GdkX11Screen *screen);
static void process_monitors_change (GdkX11Screen *screen);

enum
{
  WINDOW_MANAGER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkX11Screen, gdk_x11_screen, G_TYPE_OBJECT)

typedef struct _NetWmSupportedAtoms NetWmSupportedAtoms;

struct _NetWmSupportedAtoms
{
  Atom *atoms;
  gulong n_atoms;
};

static void
gdk_x11_screen_init (GdkX11Screen *screen)
{
}

static void
gdk_x11_screen_dispose (GObject *object)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (object);
  int i;

  for (i = 0; i < 32; ++i)
    {
      if (x11_screen->subwindow_gcs[i])
        {
          XFreeGC (x11_screen->xdisplay, x11_screen->subwindow_gcs[i]);
          x11_screen->subwindow_gcs[i] = 0;
        }
    }

  _gdk_x11_xsettings_finish (x11_screen);

  G_OBJECT_CLASS (gdk_x11_screen_parent_class)->dispose (object);

  x11_screen->xdisplay = NULL;
  x11_screen->xscreen = NULL;
  x11_screen->xroot_window = None;
  x11_screen->wmspec_check_window = None;
}

static void
gdk_x11_screen_finalize (GObject *object)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (object);

  g_free (x11_screen->window_manager_name);

  G_OBJECT_CLASS (gdk_x11_screen_parent_class)->finalize (object);
}

/**
 * gdk_x11_screen_get_monitor_output:
 * @screen: a `GdkX11Screen`
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Gets the XID of the specified output/monitor.
 * If the X server does not support version 1.2 of the RANDR
 * extension, 0 is returned.
 *
 * Returns: the XID of the monitor
 */
XID
gdk_x11_screen_get_monitor_output (GdkX11Screen *x11_screen,
                                   int           monitor_num)
{
  GdkX11Display *x11_display = GDK_X11_DISPLAY (x11_screen->display);
  GdkX11Monitor *monitor;
  XID output;

  g_return_val_if_fail (monitor_num >= 0, None);
  g_return_val_if_fail (monitor_num < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)), None);

  monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), monitor_num);
  output = monitor->output;
  g_object_unref (monitor);

  return output;
}

static int
get_current_desktop (GdkX11Screen *screen)
{
  Display *display;
  Window win;
  Atom current_desktop, type;
  int format;
  unsigned long n_items, bytes_after;
  unsigned char *data_return = NULL;
  int workspace = 0;

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            g_intern_static_string ("_NET_CURRENT_DESKTOP")))
    return workspace;

  display = GDK_DISPLAY_XDISPLAY (GDK_SCREEN_DISPLAY (screen));
  win = XRootWindow (display, gdk_x11_screen_get_screen_number (screen));

  current_desktop = XInternAtom (display, "_NET_CURRENT_DESKTOP", True);

  XGetWindowProperty (display,
                      win,
                      current_desktop,
                      0, G_MAXLONG,
                      False, XA_CARDINAL,
                      &type, &format, &n_items, &bytes_after,
                      &data_return);

  if (type == XA_CARDINAL && format == 32 && n_items > 0)
    workspace = ((long *) data_return)[0];

  if (data_return)
    XFree (data_return);

  return workspace;
}

gboolean
_gdk_x11_screen_get_monitor_work_area (GdkX11Screen *x11_screen,
                                       GdkMonitor   *monitor,
                                       GdkRectangle *area)
{
  Display *xdisplay;
  Atom net_workareas;
  int current_desktop;
  char *workareas_dn_name;
  Atom workareas_dn;
  int screen_number;
  Window xroot;
  int result;
  Atom type;
  int format;
  gulong num;
  gulong leftovers;
  guchar *ret_workarea;
  long *workareas;
  GdkRectangle geometry;
  int i;

  if (!gdk_x11_screen_supports_net_wm_hint (x11_screen,
                                            g_intern_static_string ("_GTK_WORKAREAS")))
    return FALSE;

  xdisplay = gdk_x11_display_get_xdisplay (x11_screen->display);
  net_workareas = XInternAtom (xdisplay, "_GTK_WORKAREAS", False);

  if (net_workareas == None)
    return FALSE;

  current_desktop = get_current_desktop (x11_screen);
  workareas_dn_name = g_strdup_printf ("_GTK_WORKAREAS_D%d", current_desktop);
  workareas_dn = XInternAtom (xdisplay, workareas_dn_name, True);
  g_free (workareas_dn_name);

  if (workareas_dn == None)
    return FALSE;

  screen_number = gdk_x11_screen_get_screen_number (x11_screen);
  xroot = XRootWindow (xdisplay, screen_number);

  gdk_x11_display_error_trap_push (x11_screen->display);

  ret_workarea = NULL;
  result = XGetWindowProperty (xdisplay,
                               xroot,
                               workareas_dn,
                               0,
                               G_MAXLONG,
                               False,
                               AnyPropertyType,
                               &type,
                               &format,
                               &num,
                               &leftovers,
                               &ret_workarea);

  gdk_x11_display_error_trap_pop_ignored (x11_screen->display);

  if (result != Success ||
      type == None ||
      format == 0 ||
      leftovers ||
      num % 4 != 0)
    {
      XFree (ret_workarea);

      return FALSE;
    }

  workareas = (long *) ret_workarea;

  gdk_monitor_get_geometry (monitor, &geometry);
  *area = geometry;

  for (i = 0; i < num / 4; i++)
    {
      GdkRectangle work_area;

      work_area = (GdkRectangle) {
        .x = workareas[0] / x11_screen->surface_scale,
        .y = workareas[1] / x11_screen->surface_scale,
        .width = workareas[2] / x11_screen->surface_scale,
        .height = workareas[3] / x11_screen->surface_scale,
      };

      if (gdk_rectangle_intersect (area, &work_area, &work_area))
        *area = work_area;

      workareas += 4;
    }

  XFree (ret_workarea);

  return TRUE;
}

void
gdk_x11_screen_get_work_area (GdkX11Screen *x11_screen,
                              GdkRectangle *area)
{
  Atom            workarea;
  Atom            type;
  Window          win;
  int             format;
  gulong          num;
  gulong          leftovers;
  gulong          max_len = 4 * 32;
  guchar         *ret_workarea = NULL;
  long           *workareas;
  int             result;
  int             desktop;
  Display        *display;

  display = GDK_SCREEN_XDISPLAY (x11_screen);
  workarea = XInternAtom (display, "_NET_WORKAREA", True);

  /* Defaults in case of error */
  area->x = 0;
  area->y = 0;
  area->width = WidthOfScreen (x11_screen->xscreen);
  area->height = HeightOfScreen (x11_screen->xscreen);

  if (!gdk_x11_screen_supports_net_wm_hint (x11_screen,
                                            g_intern_static_string ("_NET_WORKAREA")))
    return;

  if (workarea == None)
    return;

  win = XRootWindow (display, gdk_x11_screen_get_screen_number (x11_screen));
  result = XGetWindowProperty (display,
                               win,
                               workarea,
                               0,
                               max_len,
                               False,
                               AnyPropertyType,
                               &type,
                               &format,
                               &num,
                               &leftovers,
                               &ret_workarea);
  if (result != Success ||
      type == None ||
      format == 0 ||
      leftovers ||
      num % 4 != 0)
    goto out;

  desktop = get_current_desktop (x11_screen);
  if (desktop + 1 > num / 4) /* fvwm gets this wrong */
    goto out;

  workareas = (long *) ret_workarea;
  area->x = workareas[desktop * 4];
  area->y = workareas[desktop * 4 + 1];
  area->width = workareas[desktop * 4 + 2];
  area->height = workareas[desktop * 4 + 3];

  area->x /= x11_screen->surface_scale;
  area->y /= x11_screen->surface_scale;
  area->width /= x11_screen->surface_scale;
  area->height /= x11_screen->surface_scale;

out:
  if (ret_workarea)
    XFree (ret_workarea);
}

/**
 * gdk_x11_screen_get_xscreen:
 * @screen: a `GdkX11Screen`
 *
 * Returns the screen of a `GdkX11Screen`.
 *
 * Returns: (transfer none): an Xlib Screen*
 */
Screen *
gdk_x11_screen_get_xscreen (GdkX11Screen *screen)
{
  return screen->xscreen;
}

/**
 * gdk_x11_screen_get_screen_number:
 * @screen: a `GdkX11Screen`
 *
 * Returns the index of a `GdkX11Screen`.
 *
 * Returns: the position of @screen among the screens
 *   of its display
 */
int
gdk_x11_screen_get_screen_number (GdkX11Screen *screen)
{
  return screen->screen_num;
}

static void
notify_surface_monitor_change (GdkX11Display *display,
                               GdkMonitor    *monitor)
{
  GHashTableIter iter;
  GdkSurface *surface;

  /* We iterate the surfaces via the hash table here because it's the only
   * thing that contains all the surfaces.
   */
  if (display->xid_ht == NULL)
    return;

  g_hash_table_iter_init (&iter, display->xid_ht);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&surface))
    {
      gdk_x11_surface_check_monitor (surface, monitor);
    }
}

static GdkX11Monitor *
find_monitor_by_output (GdkX11Display *x11_display, XID output)
{
  int i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      g_object_unref (monitor);
      if (monitor->output == output)
        return monitor;
    }

  return NULL;
}

static GdkSubpixelLayout
translate_subpixel_order (int subpixel)
{
  switch (subpixel)
    {
    case 1: return GDK_SUBPIXEL_LAYOUT_HORIZONTAL_RGB;
    case 2: return GDK_SUBPIXEL_LAYOUT_HORIZONTAL_BGR;
    case 3: return GDK_SUBPIXEL_LAYOUT_VERTICAL_RGB;
    case 4: return GDK_SUBPIXEL_LAYOUT_VERTICAL_BGR;
    case 5: return GDK_SUBPIXEL_LAYOUT_NONE;
    default: return GDK_SUBPIXEL_LAYOUT_UNKNOWN;
    }
}

static gboolean
init_randr15 (GdkX11Screen *x11_screen)
{
#ifdef HAVE_RANDR15
  GdkDisplay *display = GDK_SCREEN_DISPLAY (x11_screen);
  GdkX11Display *x11_display = GDK_X11_DISPLAY (display);
  XRRScreenResources *resources;
  RROutput primary_output = None;
  RROutput first_output = None;
  int i;
  XRRMonitorInfo *rr_monitors;
  int num_rr_monitors;

  if (!x11_display->have_randr15)
    return FALSE;

  resources = XRRGetScreenResourcesCurrent (x11_screen->xdisplay,
                                            x11_screen->xroot_window);
  if (!resources)
    return FALSE;

  rr_monitors = XRRGetMonitors (x11_screen->xdisplay,
                                x11_screen->xroot_window,
                                True,
                                &num_rr_monitors);
  if (!rr_monitors)
    return FALSE;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      monitor->add = FALSE;
      monitor->remove = TRUE;
      g_object_unref (monitor);
    }

  for (i = 0; i < num_rr_monitors; i++)
    {
      RROutput output = rr_monitors[i].outputs[0];
      XRROutputInfo *output_info;
      GdkX11Monitor *monitor;
      GdkRectangle geometry;
      GdkRectangle newgeo;
      char *name;
      char *manufacturer = NULL;
      int refresh_rate = 0;

      gdk_x11_display_error_trap_push (display);
      output_info = XRRGetOutputInfo (x11_screen->xdisplay, resources, output);
      if (gdk_x11_display_error_trap_pop (display))
        continue;

      if (output_info == NULL)
        continue;

      if (output_info->connection == RR_Disconnected)
        {
          XRRFreeOutputInfo (output_info);
          continue;
        }

      if (first_output == None)
        first_output = output;

      if (output_info->crtc)
        {
          XRRCrtcInfo *crtc;
          int j;

          gdk_x11_display_error_trap_push (display);
          crtc = XRRGetCrtcInfo (x11_screen->xdisplay, resources,
                                 output_info->crtc);
          if (gdk_x11_display_error_trap_pop (display))
            {
              XRRFreeOutputInfo (output_info);
              continue;
            }

          for (j = 0; j < resources->nmode; j++)
            {
              XRRModeInfo *xmode = &resources->modes[j];
              if (xmode->id == crtc->mode)
                {
                  if (xmode->hTotal != 0 && xmode->vTotal != 0)
                    refresh_rate = (1000 * xmode->dotClock) / (xmode->hTotal * xmode->vTotal);
                  break;
                }
            }

          XRRFreeCrtcInfo (crtc);
        }

      /* Fetch minimal manufacturer information (PNP ID) from EDID */
      {
        #define EDID_LENGTH 128
        Atom actual_type, edid_atom;
        char tmp[3];
        int actual_format;
        unsigned char *prop;
        unsigned long nbytes, bytes_left;
        Display *disp = GDK_DISPLAY_XDISPLAY (x11_display);

        edid_atom = XInternAtom (disp, RR_PROPERTY_RANDR_EDID, FALSE);

        gdk_x11_display_error_trap_push (display);
        XRRGetOutputProperty (disp, output,
                              edid_atom,
                              0,
                              EDID_LENGTH,
                              FALSE,
                              FALSE,
                              AnyPropertyType,
                              &actual_type,
                              &actual_format,
                              &nbytes,
                              &bytes_left,
                              &prop);
        if (gdk_x11_display_error_trap_pop (display))
          {
            XRRFreeOutputInfo (output_info);
            continue;
          }

        // Check partial EDID header (whole header: 00 ff ff ff ff ff ff 00)
        if (nbytes >= EDID_LENGTH && prop[0] == 0x00 && prop[1] == 0xff)
          {
            /* decode the Vendor ID from three 5 bit words packed into 2 bytes
             * /--08--\/--09--\
             * 7654321076543210
             * |\---/\---/\---/
             * R  C1   C2   C3 */
            tmp[0] = 'A' + ((prop[8] & 0x7c) / 4) - 1;
            tmp[1] = 'A' + ((prop[8] & 0x3) * 8) + ((prop[9] & 0xe0) / 32) - 1;
            tmp[2] = 'A' + (prop[9] & 0x1f) - 1;

            manufacturer = g_strndup (tmp, sizeof (tmp));
          }

        XFree(prop);
        #undef EDID_LENGTH
      }

      monitor = find_monitor_by_output (x11_display, output);
      if (monitor)
        monitor->remove = FALSE;
      else
        {
          monitor = g_object_new (GDK_TYPE_X11_MONITOR,
                                  "display", display,
                                  NULL);
          monitor->output = output;
          monitor->add = TRUE;
          g_list_store_append (x11_display->monitors, monitor);
        }

      gdk_monitor_get_geometry (GDK_MONITOR (monitor), &geometry);
      name = g_strndup (output_info->name, output_info->nameLen);

      newgeo.x = rr_monitors[i].x / x11_screen->surface_scale;
      newgeo.y = rr_monitors[i].y / x11_screen->surface_scale;
      newgeo.width = rr_monitors[i].width / x11_screen->surface_scale;
      newgeo.height = rr_monitors[i].height / x11_screen->surface_scale;

      gdk_monitor_set_geometry (GDK_MONITOR (monitor), &newgeo);
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                     rr_monitors[i].mwidth,
                                     rr_monitors[i].mheight);
      gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor),
                                       translate_subpixel_order (output_info->subpixel_order));
      gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh_rate);
      gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), x11_screen->surface_scale);
      gdk_monitor_set_model (GDK_MONITOR (monitor), name);
      gdk_monitor_set_connector (GDK_MONITOR (monitor), name);
      gdk_monitor_set_manufacturer (GDK_MONITOR (monitor), manufacturer);
      g_free (manufacturer);
      g_free (name);

      if (rr_monitors[i].primary)
        primary_output = monitor->output;

      XRRFreeOutputInfo (output_info);
    }

  XRRFreeMonitors (rr_monitors);
  XRRFreeScreenResources (resources);

  for (i = g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)) - 1; i >= 0; i--)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);

      notify_surface_monitor_change (x11_display, GDK_MONITOR (monitor));
      if (monitor->remove)
        {
          g_list_store_remove (x11_display->monitors, i);
          gdk_monitor_invalidate (GDK_MONITOR (monitor));
        }
      g_object_unref (monitor);
    }

  x11_display->primary_monitor = 0;
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      g_object_unref (monitor);
      if (monitor->output == primary_output)
        {
          x11_display->primary_monitor = i;
          break;
        }

      /* No RandR1.3+ available or no primary set, fall back to prefer LVDS as primary if present */
      if (primary_output == None &&
          g_ascii_strncasecmp (gdk_monitor_get_model (GDK_MONITOR (monitor)), "LVDS", 4) == 0)
        {
          x11_display->primary_monitor = i;
          break;
        }

      /* No primary specified and no LVDS found */
      if (monitor->output == first_output)
        x11_display->primary_monitor = i;
    }

  return g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)) > 0;
#endif

  return FALSE;
}

static gboolean
init_randr13 (GdkX11Screen *x11_screen)
{
#ifdef HAVE_RANDR
  GdkDisplay *display = GDK_SCREEN_DISPLAY (x11_screen);
  GdkX11Display *x11_display = GDK_X11_DISPLAY (display);
  XRRScreenResources *resources;
  RROutput primary_output = None;
  RROutput first_output = None;
  int i;

  if (!x11_display->have_randr13)
      return FALSE;

  resources = XRRGetScreenResourcesCurrent (x11_screen->xdisplay,
                                            x11_screen->xroot_window);
  if (!resources)
    return FALSE;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      monitor->add = FALSE;
      monitor->remove = TRUE;
      g_object_unref (monitor);
    }

  for (i = 0; i < resources->noutput; ++i)
    {
      RROutput output = resources->outputs[i];
      XRROutputInfo *output_info;

      gdk_x11_display_error_trap_push (display);
      output_info = XRRGetOutputInfo (x11_screen->xdisplay, resources, output);

      if (gdk_x11_display_error_trap_pop (display))
        continue;

      if (output_info->connection == RR_Disconnected)
        {
          XRRFreeOutputInfo (output_info);
          continue;
        }

      if (output_info->crtc)
	{
	  GdkX11Monitor *monitor;
          XRRCrtcInfo *crtc;
          char *name;
          GdkRectangle geometry;
          GdkRectangle newgeo;
          int j;
          int refresh_rate = 0;

          gdk_x11_display_error_trap_push (display);
          crtc = XRRGetCrtcInfo (x11_screen->xdisplay, resources, output_info->crtc);

          if (gdk_x11_display_error_trap_pop (display))
            {
              XRRFreeOutputInfo (output_info);
              continue;
            }

          for (j = 0; j < resources->nmode; j++)
            {
              XRRModeInfo *xmode = &resources->modes[j];
              if (xmode->id == crtc->mode)
                {
                  if (xmode->hTotal != 0 && xmode->vTotal != 0)
                    refresh_rate = (1000 * xmode->dotClock) / (xmode->hTotal * xmode->vTotal);
                  break;
                }
            }

          monitor = find_monitor_by_output (x11_display, output);
          if (monitor)
            monitor->remove = FALSE;
          else
            {
              monitor = g_object_new (gdk_x11_monitor_get_type (),
                                      "display", display,
                                      NULL);
              monitor->output = output;
              monitor->add = TRUE;
              g_list_store_append (x11_display->monitors, monitor);
            }

          gdk_monitor_get_geometry (GDK_MONITOR (monitor), &geometry);
          name = g_strndup (output_info->name, output_info->nameLen);

          newgeo.x = crtc->x / x11_screen->surface_scale;
          newgeo.y = crtc->y / x11_screen->surface_scale;
          newgeo.width = crtc->width / x11_screen->surface_scale;
          newgeo.height = crtc->height / x11_screen->surface_scale;

          gdk_monitor_set_geometry (GDK_MONITOR (monitor), &newgeo);
          gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                         output_info->mm_width,
                                         output_info->mm_height);
          gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor),
                                           translate_subpixel_order (output_info->subpixel_order));
          gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh_rate);
          gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), x11_screen->surface_scale);
          gdk_monitor_set_model (GDK_MONITOR (monitor), name);

          g_free (name);

          XRRFreeCrtcInfo (crtc);
	}

      XRRFreeOutputInfo (output_info);
    }

  if (resources->noutput > 0)
    first_output = resources->outputs[0];

  XRRFreeScreenResources (resources);

  /* Which usable multihead data is not returned in non RandR 1.2+ X driver? */

  for (i = g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)) - 1; i >= 0; i--)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);

      notify_surface_monitor_change (x11_display, GDK_MONITOR (monitor));
      if (monitor->remove)
        {
          g_list_store_remove (x11_display->monitors, i);
          gdk_monitor_invalidate (GDK_MONITOR (monitor));
        }
      g_object_unref (monitor);
    }

  x11_display->primary_monitor = 0;
  gdk_x11_display_error_trap_push (display);
  primary_output = XRRGetOutputPrimary (x11_screen->xdisplay,
                                        x11_screen->xroot_window);
  gdk_x11_display_error_trap_pop_ignored (display);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkX11Monitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      g_object_unref (monitor);
      if (monitor->output == primary_output)
        {
          x11_display->primary_monitor = i;
          break;
        }

      /* No RandR1.3+ available or no primary set, fall back to prefer LVDS as primary if present */
      if (primary_output == None &&
          g_ascii_strncasecmp (gdk_monitor_get_model (GDK_MONITOR (monitor)), "LVDS", 4) == 0)
        {
          x11_display->primary_monitor = i;
          break;
        }

      /* No primary specified and no LVDS found */
      if (monitor->output == first_output)
        x11_display->primary_monitor = i;
    }

  return g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)) > 0;
#endif

  return FALSE;
}

static void
init_no_multihead (GdkX11Screen *x11_screen)
{
  GdkX11Display *x11_display = GDK_X11_DISPLAY (x11_screen->display);
  GdkX11Monitor *monitor;
  int width_mm, height_mm;
  int width, height;
  int i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);
      monitor->add = FALSE;
      monitor->remove = TRUE;
      g_object_unref (monitor);
    }

  monitor = find_monitor_by_output (x11_display, 0);
  if (monitor)
    monitor->remove = FALSE;
  else
    {
      monitor = g_object_new (gdk_x11_monitor_get_type (),
                              "display", x11_display,
                              NULL);
      monitor->output = 0;
      monitor->add = TRUE;
      g_list_store_append (x11_display->monitors, monitor);
    }

  width_mm = WidthMMOfScreen (x11_screen->xscreen);
  height_mm = HeightMMOfScreen (x11_screen->xscreen);
  width = WidthOfScreen (x11_screen->xscreen);
  height = HeightOfScreen (x11_screen->xscreen);

  gdk_monitor_set_geometry (GDK_MONITOR (monitor), &(GdkRectangle) { 0, 0, width, height });
  gdk_monitor_set_physical_size (GDK_MONITOR (monitor), width_mm, height_mm);
  gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), x11_screen->surface_scale);

  x11_display->primary_monitor = 0;

  for (i = g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)) - 1; i >= 0; i--)
    {
      monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);

      notify_surface_monitor_change (x11_display, GDK_MONITOR (monitor));
      if (monitor->remove)
        {
          g_list_store_remove (x11_display->monitors, i);
          gdk_monitor_invalidate (GDK_MONITOR (monitor));
        }
      g_object_unref (monitor);
    }
}

static void
init_multihead (GdkX11Screen *screen)
{
  if (!init_randr15 (screen) &&
      !init_randr13 (screen))
    init_no_multihead (screen);
}

GdkX11Screen *
_gdk_x11_screen_new (GdkDisplay *display,
		     int	 screen_number)
{
  GdkX11Screen *x11_screen;
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  const char *scale_str;

  x11_screen = g_object_new (GDK_TYPE_X11_SCREEN, NULL);

  x11_screen->display = display;
  x11_screen->xdisplay = display_x11->xdisplay;
  x11_screen->xscreen = ScreenOfDisplay (display_x11->xdisplay, screen_number);
  x11_screen->screen_num = screen_number;
  x11_screen->xroot_window = RootWindow (display_x11->xdisplay, screen_number);
  x11_screen->wmspec_check_window = None;
  /* we want this to be always non-null */
  x11_screen->window_manager_name = g_strdup ("unknown");

  scale_str = g_getenv ("GDK_SCALE");
  if (scale_str)
    {
      x11_screen->fixed_surface_scale = TRUE;
      x11_screen->surface_scale = atol (scale_str);
      if (x11_screen->surface_scale <= 0)
        x11_screen->surface_scale = 1;
    }
  else
    x11_screen->surface_scale = 1;

  init_randr_support (x11_screen);
  init_multihead (x11_screen);

  return x11_screen;
}

void
_gdk_x11_screen_set_surface_scale (GdkX11Screen *x11_screen,
				  int           scale)
{
  GdkX11Display *x11_display = GDK_X11_DISPLAY (x11_screen->display);
  GList *toplevels, *l;
  int i;

  if (x11_screen->surface_scale == scale)
    return;

  x11_screen->surface_scale = scale;

  toplevels = gdk_x11_display_get_toplevel_windows (x11_screen->display);

  for (l = toplevels; l != NULL; l = l->next)
    {
      GdkSurface *surface = l->data;

      _gdk_x11_surface_set_surface_scale (surface, scale);
    }

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (x11_display->monitors)); i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (x11_display->monitors), i);

      gdk_monitor_set_scale_factor (monitor, scale);

      g_object_unref (monitor);
    }

  /* We re-read the monitor sizes so we can apply the new scale */
  process_monitors_change (x11_screen);
}

static void
init_randr_support (GdkX11Screen *x11_screen)
{
  /* NB: This is also needed for XSettings, so don't remove. */
  XSelectInput (GDK_SCREEN_XDISPLAY (x11_screen),
                x11_screen->xroot_window,
                StructureNotifyMask);

#ifdef HAVE_RANDR
  if (!GDK_X11_DISPLAY (GDK_SCREEN_DISPLAY (x11_screen))->have_randr12)
    return;

  XRRSelectInput (GDK_SCREEN_XDISPLAY (x11_screen),
                  x11_screen->xroot_window,
                  RRScreenChangeNotifyMask
                  | RRCrtcChangeNotifyMask
                  | RROutputPropertyNotifyMask);
#endif
}

static void
process_monitors_change (GdkX11Screen *screen)
{
  init_multihead (screen);
}

void
_gdk_x11_screen_size_changed (GdkX11Screen *screen,
			      const XEvent *event)
{
#ifdef HAVE_RANDR
  GdkX11Display *display_x11;

  display_x11 = GDK_X11_DISPLAY (GDK_SCREEN_DISPLAY (screen));

  if (display_x11->have_randr13 && event->type == ConfigureNotify)
    return;

  XRRUpdateConfiguration ((XEvent *) event);
#else
  if (event->type != ConfigureNotify)
    return;
#endif

  process_monitors_change (screen);
}

void
_gdk_x11_screen_get_edge_monitors (GdkX11Screen *x11_screen,
                                   int       *top,
                                   int       *bottom,
                                   int       *left,
                                   int       *right)
{
#ifdef HAVE_XFREE_XINERAMA
  int           top_most_pos = HeightOfScreen (x11_screen->xscreen);
  int           left_most_pos = WidthOfScreen (x11_screen->xscreen);
  int           bottom_most_pos = 0;
  int           right_most_pos = 0;
  int           i;
  XineramaScreenInfo *x_monitors;
  int x_n_monitors;
#endif

  *top = *bottom = *left = *right = -1;

#ifdef HAVE_XFREE_XINERAMA
  if (!XineramaIsActive (x11_screen->xdisplay))
    return;

  x_monitors = XineramaQueryScreens (x11_screen->xdisplay, &x_n_monitors);
  if (x_n_monitors <= 0 || x_monitors == NULL)
    {
      if (x_monitors)
        XFree (x_monitors);

      return;
    }

  for (i = 0; i < x_n_monitors; i++)
    {
      if (left && left_most_pos > x_monitors[i].x_org)
	{
	  left_most_pos = x_monitors[i].x_org;
	  *left = i;
	}
      if (right && right_most_pos < x_monitors[i].x_org + x_monitors[i].width)
	{
	  right_most_pos = x_monitors[i].x_org + x_monitors[i].width;
	  *right = i;
	}
      if (top && top_most_pos > x_monitors[i].y_org)
	{
	  top_most_pos = x_monitors[i].y_org;
	  *top = i;
	}
      if (bottom && bottom_most_pos < x_monitors[i].y_org + x_monitors[i].height)
	{
	  bottom_most_pos = x_monitors[i].y_org + x_monitors[i].height;
	  *bottom = i;
	}
    }

  XFree (x_monitors);
#endif
}

void
_gdk_x11_screen_window_manager_changed (GdkX11Screen *screen)
{
  g_signal_emit (screen, signals[WINDOW_MANAGER_CHANGED], 0);
}

gboolean
gdk_x11_screen_get_setting (GdkX11Screen   *x11_screen,
			    const char *name,
			    GValue      *value)
{
  const GValue *setting;

  if (x11_screen->xsettings == NULL)
    goto out;
  setting = g_hash_table_lookup (x11_screen->xsettings, name);
  if (setting == NULL)
    goto out;

  if (!g_value_transform (setting, value))
    {
      g_warning ("Cannot transform xsetting %s of type %s to type %s\n",
		 name,
		 g_type_name (G_VALUE_TYPE (setting)),
		 g_type_name (G_VALUE_TYPE (value)));
      goto out;
    }

  return TRUE;

 out:
  return _gdk_x11_screen_get_xft_setting (x11_screen, name, value);
}

static void
cleanup_atoms(gpointer data)
{
  NetWmSupportedAtoms *supported_atoms = data;
  if (supported_atoms->atoms)
      XFree (supported_atoms->atoms);
  g_free (supported_atoms);
}

static Window
get_net_supporting_wm_check (GdkX11Screen *screen,
                             Window        window)
{
  GdkDisplay *display;
  Atom type;
  int format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;
  Window value;

  display = screen->display;
  type = None;
  data = NULL;
  value = None;

  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (screen->xdisplay, window,
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &n_items, &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (type == XA_WINDOW)
    value = *(Window *)data;

  if (data)
    XFree (data);

  return value;
}

static void
fetch_net_wm_check_window (GdkX11Screen *x11_screen)
{
  GdkDisplay *display;
  Window window;
  int error;

  display = x11_screen->display;

  g_return_if_fail (GDK_X11_DISPLAY (display)->trusted_client);

  if (x11_screen->wmspec_check_window != None)
    return; /* already have it */

  window = get_net_supporting_wm_check (x11_screen, x11_screen->xroot_window);
  if (window == None)
    return;

  if (window != get_net_supporting_wm_check (x11_screen, window))
    return;

  gdk_x11_display_error_trap_push (display);

  /* Find out if this WM goes away, so we can reset everything. */
  XSelectInput (x11_screen->xdisplay, window, StructureNotifyMask);

  error = gdk_x11_display_error_trap_pop (display);
  if (!error)
    {
      /* We check the window property again because after XGetWindowProperty()
       * and before XSelectInput() the window may have been recycled in such a
       * way that XSelectInput() doesn't fail but the window is no longer what
       * we want.
       */
      if (window != get_net_supporting_wm_check (x11_screen, window))
        return;

      x11_screen->wmspec_check_window = window;
      x11_screen->need_refetch_net_supported = TRUE;
      x11_screen->need_refetch_wm_name = TRUE;

      /* Careful, reentrancy */
      _gdk_x11_screen_window_manager_changed (x11_screen);
    }
}

/**
 * gdk_x11_screen_supports_net_wm_hint:
 * @screen: the relevant `GdkX11Screen`.
 * @property_name: name of the WM property
 *
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager supports a certain hint from the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * When using this function, keep in mind that the window manager
 * can change over time; so you shouldn’t use this function in
 * a way that impacts persistent application state. A common bug
 * is that your application can start up before the window manager
 * does when the user logs in, and before the window manager starts
 * gdk_x11_screen_supports_net_wm_hint() will return %FALSE for every property.
 * You can monitor the window_manager_changed signal on `GdkX11Screen` to detect
 * a window manager change.
 *
 * Returns: %TRUE if the window manager supports @property
 **/
gboolean
gdk_x11_screen_supports_net_wm_hint (GdkX11Screen *x11_screen,
				     const char   *property_name)
{
  gulong i;
  NetWmSupportedAtoms *supported_atoms;
  GdkDisplay *display;
  Atom atom;

  display = x11_screen->display;

  if (!G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    return FALSE;

  supported_atoms = g_object_get_data (G_OBJECT (x11_screen), "gdk-net-wm-supported-atoms");
  if (!supported_atoms)
    {
      supported_atoms = g_new0 (NetWmSupportedAtoms, 1);
      g_object_set_data_full (G_OBJECT (x11_screen), "gdk-net-wm-supported-atoms", supported_atoms, cleanup_atoms);
    }

  fetch_net_wm_check_window (x11_screen);

  if (x11_screen->wmspec_check_window == None)
    return FALSE;

  if (x11_screen->need_refetch_net_supported)
    {
      /* WM has changed since we last got the supported list,
       * refetch it.
       */
      Atom type;
      int format;
      gulong bytes_after;

      x11_screen->need_refetch_net_supported = FALSE;

      if (supported_atoms->atoms)
        XFree (supported_atoms->atoms);

      supported_atoms->atoms = NULL;
      supported_atoms->n_atoms = 0;

      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), x11_screen->xroot_window,
                          gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTED"),
                          0, G_MAXLONG, False, XA_ATOM, &type, &format,
                          &supported_atoms->n_atoms, &bytes_after,
                          (guchar **)&supported_atoms->atoms);

      if (type != XA_ATOM)
        return FALSE;
    }

  if (supported_atoms->atoms == NULL)
    return FALSE;

  atom = gdk_x11_get_xatom_by_name_for_display (display, property_name);

  for (i = 0; i < supported_atoms->n_atoms; i++)
    {
      if (supported_atoms->atoms[i] == atom)
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_x11_screen_get_window_manager_name:
 * @screen: a `GdkX11Screen`
 *
 * Returns the name of the window manager for @screen.
 *
 * Returns: the name of the window manager screen @screen, or
 * "unknown" if the window manager is unknown. The string is owned by GDK
 * and should not be freed.
 **/
const char*
gdk_x11_screen_get_window_manager_name (GdkX11Screen *x11_screen)
{
  GdkDisplay *display;

  display = x11_screen->display;

  if (!G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    return x11_screen->window_manager_name;

  fetch_net_wm_check_window (x11_screen);

  if (x11_screen->need_refetch_wm_name)
    {
      /* Get the name of the window manager */
      x11_screen->need_refetch_wm_name = FALSE;

      g_free (x11_screen->window_manager_name);
      x11_screen->window_manager_name = g_strdup ("unknown");

      if (x11_screen->wmspec_check_window != None)
        {
          Atom type;
          int format;
          gulong n_items;
          gulong bytes_after;
          char *name;

          name = NULL;

          gdk_x11_display_error_trap_push (display);

          XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                              x11_screen->wmspec_check_window,
                              gdk_x11_get_xatom_by_name_for_display (display,
                                                                     "_NET_WM_NAME"),
                              0, G_MAXLONG, False,
                              gdk_x11_get_xatom_by_name_for_display (display,
                                                                     "UTF8_STRING"),
                              &type, &format,
                              &n_items, &bytes_after,
                              (guchar **)&name);

          gdk_x11_display_error_trap_pop_ignored (display);

          if (name != NULL)
            {
              g_free (x11_screen->window_manager_name);
              x11_screen->window_manager_name = g_strdup (name);
              XFree (name);
            }
        }
    }

  return x11_screen->window_manager_name;
}

static void
gdk_x11_screen_class_init (GdkX11ScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_x11_screen_dispose;
  object_class->finalize = gdk_x11_screen_finalize;

  signals[WINDOW_MANAGER_CHANGED] =
    g_signal_new (g_intern_static_string ("window-manager-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkX11ScreenClass, window_manager_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static guint32
get_netwm_cardinal_property (GdkX11Screen *x11_screen,
                             const char   *name)
{
  guint32 prop = 0;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;

  if (!gdk_x11_screen_supports_net_wm_hint (x11_screen, name))
    return 0;

  XGetWindowProperty (x11_screen->xdisplay,
                      x11_screen->xroot_window,
                      gdk_x11_get_xatom_by_name_for_display (GDK_SCREEN_DISPLAY (x11_screen), name),
                      0, G_MAXLONG,
                      False, XA_CARDINAL, &type, &format, &nitems,
                      &bytes_after, &data);
  if (type == XA_CARDINAL)
    {
      prop = *(gulong *)data;
      XFree (data);
    }

  return prop;
}

/**
 * gdk_x11_screen_get_number_of_desktops:
 * @screen: a `GdkX11Screen`
 *
 * Returns the number of workspaces for @screen when running under a
 * window manager that supports multiple workspaces, as described
 * in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * Returns: the number of workspaces, or 0 if workspaces are not supported
 */
guint32
gdk_x11_screen_get_number_of_desktops (GdkX11Screen *screen)
{
  return get_netwm_cardinal_property (screen, "_NET_NUMBER_OF_DESKTOPS");
}

/**
 * gdk_x11_screen_get_current_desktop:
 * @screen: a `GdkX11Screen`
 *
 * Returns the current workspace for @screen when running under a
 * window manager that supports multiple workspaces, as described
 * in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * Returns: the current workspace, or 0 if workspaces are not supported
 */
guint32
gdk_x11_screen_get_current_desktop (GdkX11Screen *screen)
{
  return get_netwm_cardinal_property (screen, "_NET_CURRENT_DESKTOP");
}
