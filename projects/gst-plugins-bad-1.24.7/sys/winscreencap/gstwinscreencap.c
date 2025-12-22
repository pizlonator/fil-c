/* GStreamer
 * Copyright (C) 2007 Haakon Sporsheim <hakon.sporsheim@tandberg.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstwinscreencap.h"
#include "gstgdiscreencapsrc.h"
#include "gstdx9screencapsrc.h"

static BOOL CALLBACK
_diplay_monitor_enum (HMONITOR hMon, HDC hdc, LPRECT rect, LPARAM param)
{
  LPRECT *pp_rect = (LPRECT *) param;
  CopyRect (*pp_rect, rect);
  (*pp_rect)++;
  return TRUE;
}

RECT
gst_win32_get_monitor_rect (UINT index)
{
  RECT ret_rect;
  LPRECT data;

  data = (LPRECT) malloc (sizeof (RECT) * GetSystemMetrics (SM_CMONITORS));
  if (data) {
    LPRECT tmp = data;
    EnumDisplayMonitors (NULL, NULL, _diplay_monitor_enum, (LPARAM) & tmp);

    ret_rect = data[index];
    free (data);
  } else {
    ZeroMemory (&ret_rect, sizeof (RECT));
  }

  return ret_rect;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "gdiscreencapsrc",
          GST_RANK_NONE, GST_TYPE_GDISCREENCAPSRC)) {
    return FALSE;
  }

  if (!gst_element_register (plugin, "dx9screencapsrc",
          GST_RANK_NONE, GST_TYPE_DX9SCREENCAPSRC)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    winscreencap,
    "Screen capture plugin for Windows",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
