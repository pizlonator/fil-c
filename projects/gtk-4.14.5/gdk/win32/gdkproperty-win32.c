/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <pango/pangowin32.h>

#include "gdkdisplayprivate.h"
#include "gdkprivate-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkwin32.h"

static char *
_get_system_font_name (HDC hdc)
{
  NONCLIENTMETRICSW ncm;
  PangoFontDescription *font_desc;
  char *result, *font_desc_string;
  int logpixelsy;
  int font_size;

  ncm.cbSize = sizeof(NONCLIENTMETRICSW);
  if (!SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    return NULL;

  logpixelsy = GetDeviceCaps (hdc, LOGPIXELSY);
  font_desc = pango_win32_font_description_from_logfontw (&ncm.lfMessageFont);
  font_desc_string = pango_font_description_to_string (font_desc);
  pango_font_description_free (font_desc);

  /* https://docs.microsoft.com/en-us/windows/desktop/api/wingdi/ns-wingdi-taglogfonta */
  font_size = -MulDiv (ncm.lfMessageFont.lfHeight, 72, logpixelsy);
  result = g_strdup_printf ("%s %d", font_desc_string, font_size);
  g_free (font_desc_string);

  return result;
}

gboolean
_gdk_win32_get_setting (const char *name,
                        GValue      *value)
{
  if (strcmp ("gtk-alternative-button-order", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-sort-arrows", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-cursor-blink", name) == 0)
    {
      gboolean blinks = (GetCaretBlinkTime () != INFINITE);
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %s\n", name, blinks ? "TRUE" : "FALSE"));
      g_value_set_boolean (value, blinks);
      return TRUE;
    }
  else if (strcmp ("gtk-cursor-theme-size", name) == 0)
    {
      int cursor_size = GetSystemMetrics (SM_CXCURSOR);
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, cursor_size));
      g_value_set_int (value, cursor_size);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDOUBLECLK), GetSystemMetrics (SM_CYDOUBLECLK));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-time", name) == 0)
    {
      int i = GetDoubleClickTime ();
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-font-name", name) == 0)
    {
      char *font_name = NULL;
      HDC hdc = NULL;

      if ((hdc = GetDC (HWND_DESKTOP)) != NULL)
        {
          font_name = _get_system_font_name (hdc);
          ReleaseDC (HWND_DESKTOP, hdc);
          hdc = NULL;
        }

      if (font_name)
        {
          GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, font_name));
          g_value_take_string (value, font_name);
          return TRUE;
        }
      else
        {
          g_warning ("gdk_win32_get_setting: Detecting the system font failed");
          return FALSE;
        }
    }
  else if (strcmp ("gtk-hint-font-metrics", name) == 0)
    {
      gboolean hint_font_metrics = TRUE;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name,
                             hint_font_metrics ? "TRUE" : "FALSE"));
      g_value_set_boolean (value, hint_font_metrics);
      return TRUE;
    }
  else if (strcmp ("gtk-im-module", name) == 0)
    {
      const char *im_module = _gdk_input_locale_is_ime ? "ime" : "";
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, im_module));
      g_value_set_static_string (value, im_module);
      return TRUE;
    }
  else if (strcmp ("gtk-overlay-scrolling", name) == 0)
    {
      DWORD val = 0;
      DWORD sz = sizeof (val);
      LSTATUS ret = 0;

      ret = RegGetValueW (HKEY_CURRENT_USER, L"Control Panel\\Accessibility", L"DynamicScrollbars", RRF_RT_DWORD, NULL, &val, &sz);
      if (ret == ERROR_SUCCESS)
        {
          gboolean overlay_scrolling = val != 0;
          GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name,
                                 overlay_scrolling ? "TRUE" : "FALSE"));
          g_value_set_boolean (value, overlay_scrolling);
          return TRUE;
        }
    }
  else if (strcmp ("gtk-shell-shows-desktop", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-split-cursor", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : FALSE\n", name));
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else if (strcmp ("gtk-theme-name", name) == 0)
    {
      HIGHCONTRASTW hc;
      memset (&hc, 0, sizeof (hc));
      hc.cbSize = sizeof (hc);
      if (API_CALL (SystemParametersInfoW, (SPI_GETHIGHCONTRAST, sizeof (hc), &hc, 0)))
        {
          if (hc.dwFlags & HCF_HIGHCONTRASTON)
            {
              const char *theme_name = "Default-hc";

              GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %s\n", name, theme_name));
              g_value_set_string (value, theme_name);
              return TRUE;
            }
        }
    }
  else if (strcmp ("gtk-xft-antialias", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-dpi", name) == 0)
    {
      GdkWin32Display *display = GDK_WIN32_DISPLAY (_gdk_display);

      if (display->dpi_aware_type == PROCESS_SYSTEM_DPI_AWARE &&
          !display->has_fixed_scale)
        {
          HDC hdc = GetDC (NULL);

          if (hdc != NULL)
            {
              int dpi = GetDeviceCaps (GetDC (NULL), LOGPIXELSX);
              ReleaseDC (NULL, hdc);

              if (dpi >= 96)
                {
                  int xft_dpi = 1024 * dpi / display->surface_scale;
                  GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %d\n", name, xft_dpi));
                  g_value_set_int (value, xft_dpi);
                  return TRUE;
                }
            }
        }
    }
  else if (strcmp ("gtk-xft-hinting", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-hintstyle", name) == 0)
    {
      g_value_set_static_string (value, "hintfull");
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }
  else if (strcmp ("gtk-xft-rgba", name) == 0)
    {
      unsigned int orientation = 0;
      if (SystemParametersInfoW (SPI_GETFONTSMOOTHINGORIENTATION, 0, &orientation, 0))
        {
          if (orientation == FE_FONTSMOOTHINGORIENTATIONRGB)
            g_value_set_static_string (value, "rgb");
          else if (orientation == FE_FONTSMOOTHINGORIENTATIONBGR)
            g_value_set_static_string (value, "bgr");
          else
            g_value_set_static_string (value, "none");
        }
      else
        g_value_set_static_string (value, "none");

      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }

  return FALSE;
}
