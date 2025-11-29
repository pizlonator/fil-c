/* GDK - The GIMP Drawing Kit
 * Copyright © 2005 Red Hat, Inc
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
 *
 * Based on code from xftdpy.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <fontconfig/fontconfig.h>

#ifndef FC_HINT_STYLE
#define FC_HINT_NONE        0
#define FC_HINT_SLIGHT      1
#define FC_HINT_MEDIUM      2
#define FC_HINT_FULL        3
#endif

#include <gdkscreen-x11.h>
#include <gdkprivate-x11.h>

#include "gdkdebugprivate.h"

static int
parse_boolean (char *v)
{
  char c0, c1;
  
  c0 = *v;
  if (g_ascii_isupper ((int)c0))
    c0 = g_ascii_tolower (c0);
  if (c0 == 't' || c0 == 'y' || c0 == '1')
    return 1;
  if (c0 == 'f' || c0 == 'n' || c0 == '0')
    return 0;
  if (c0 == 'o')
    {
      c1 = v[1];
      if (g_ascii_isupper ((int)c1))
	c1 = g_ascii_tolower (c1);
      if (c1 == 'n')
	return 1;
      if (c1 == 'f')
	return 0;
    }
  
  return -1;
}

static gboolean
get_boolean_default (GdkX11Screen *x11_screen,
		     const char *option,
		     gboolean    *value)
{
  Display *dpy = GDK_SCREEN_XDISPLAY (x11_screen);
  char *v;
  int i;

  if (gdk_display_get_debug_flags (GDK_SCREEN_DISPLAY (x11_screen)) & GDK_DEBUG_DEFAULT_SETTINGS)
      return FALSE;

  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      i = parse_boolean (v);
      if (i >= 0)
	{
	  *value = i;
	  return TRUE;
	}
    }
  
  return FALSE;
}

static gboolean
get_double_default (GdkX11Screen *x11_screen,
		    const char *option,
		    double      *value)
{
  Display *dpy = GDK_SCREEN_XDISPLAY (x11_screen);
  char     *v, *e;

  if (gdk_display_get_debug_flags (GDK_SCREEN_DISPLAY (x11_screen)) & GDK_DEBUG_DEFAULT_SETTINGS)
      return FALSE;

  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      /* Xft uses strtod, though localization probably wasn't
       * desired. For compatibility, we use the conservative
       * g_strtod() that accepts either localized or non-localized
       * decimal separator.
       */
      *value = g_strtod (v, &e);
      if (e != v)
	return TRUE;
    }
  
  return FALSE;
}

static gboolean
get_integer_default (GdkX11Screen *x11_screen,
		     const char *option,
		     int         *value)
{
  Display *dpy = GDK_SCREEN_XDISPLAY (x11_screen);
  char *v, *e;

  if (gdk_display_get_debug_flags (GDK_SCREEN_DISPLAY (x11_screen)) & GDK_DEBUG_DEFAULT_SETTINGS)
      return FALSE;

  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      if (FcNameConstant ((FcChar8 *) v, value))
	return TRUE;
      
      *value = strtol (v, &e, 0);
      if (e != v)
	return TRUE;
    }
  
  return FALSE;
}

static void
init_xft_settings (GdkX11Screen *x11_screen)
{
  double dpi_double;
  gboolean b;

  if (x11_screen->xft_init)
    return;

  x11_screen->xft_init = TRUE;

  if (!get_boolean_default (x11_screen, "antialias", &b))
    b = TRUE;
  x11_screen->xft_antialias = b;

  if (!get_boolean_default (x11_screen, "hinting", &b))
    b = TRUE;
  x11_screen->xft_hinting = b;

  if (!get_integer_default (x11_screen, "hintstyle", &x11_screen->xft_hintstyle))
    x11_screen->xft_hintstyle = FC_HINT_MEDIUM;

  if (!get_integer_default (x11_screen, "rgba", &x11_screen->xft_rgba))
    x11_screen->xft_rgba = FC_RGBA_UNKNOWN;

  if (!get_double_default (x11_screen, "dpi", &dpi_double))
    dpi_double = 96.0;

  x11_screen->xft_dpi = (int)(0.5 + PANGO_SCALE * dpi_double);
}

gboolean
_gdk_x11_screen_get_xft_setting (GdkX11Screen *x11_screen,
                                 const char   *name,
                                 GValue       *value)
{
  if (strncmp (name, "gtk-xft-", 8) != 0)
    return FALSE;

  name += 8;

  init_xft_settings (x11_screen);

  if (strcmp (name, "antialias") == 0)
    {
      g_value_set_int (value, x11_screen->xft_antialias);
      return TRUE;
    }
  else if (strcmp (name, "hinting") == 0)
    {
      g_value_set_int (value, x11_screen->xft_hinting);
      return TRUE;
    }
  else if (strcmp (name, "hintstyle") == 0)
    {
      const char *str;
      
      switch (x11_screen->xft_hintstyle)
	{
	case FC_HINT_NONE:
	  str = "hintnone";
	  break;
	case FC_HINT_SLIGHT:
	  str = "hintslight";
	  break;
	case FC_HINT_MEDIUM:
	  str = "hintmedium";
	  break;
	case FC_HINT_FULL:
	  str = "hintfull";
	  break;
	default:
	  return FALSE;
	}

      g_value_set_string (value, str);
      return TRUE;
    }
  else if (strcmp (name, "rgba") == 0)
    {
      const char *str;
      
      switch (x11_screen->xft_rgba)
	{
	case FC_RGBA_NONE:
	  str = "none";
	  break;
	case FC_RGBA_RGB:
	  str = "rgb";
	  break;
	case FC_RGBA_BGR:
	  str = "bgr";
	  break;
	case FC_RGBA_VRGB:
	  str = "vrgb";
	  break;
	case FC_RGBA_VBGR:
	  str = "vbgr";
	  break;
	case FC_RGBA_UNKNOWN:
	default:
	  return FALSE;
	}
	
      g_value_set_string (value, str);
      return TRUE; 
   }
  else if (strcmp (name, "dpi") == 0)
    {
      g_value_set_int (value, x11_screen->xft_dpi);
      return TRUE;
    }

  return FALSE;
}
