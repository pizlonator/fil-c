/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstwin32ipcutils.h"
#include <windows.h>
#include <string>
#include <mutex>

static ULONG global_index = 0;

static DWORD
gst_win32_ipc_get_pid (void)
{
  static std::once_flag once_flag;
  static DWORD pid = 0;

  std::call_once (once_flag,[&]() {
        pid = GetCurrentProcessId ();
      });

  return pid;
}

/* Create unique prefix for named shared memory */
gchar *
gst_win32_ipc_get_mmf_prefix (void)
{
  std::string prefix = "Local\\gst.win32.ipc." +
      std::to_string (gst_win32_ipc_get_pid ()) + std::string (".") +
      std::to_string (InterlockedIncrement (&global_index)) + std::string (".");

  return g_strdup (prefix.c_str ());
}

gboolean
gst_win32_ipc_clock_is_qpc (GstClock * clock)
{
  GstClockType clock_type = GST_CLOCK_TYPE_MONOTONIC;
  GstClock *mclock;

  if (G_OBJECT_TYPE (clock) != GST_TYPE_SYSTEM_CLOCK)
    return FALSE;

  g_object_get (clock, "clock-type", &clock_type, nullptr);
  if (clock_type != GST_CLOCK_TYPE_MONOTONIC)
    return FALSE;

  mclock = gst_clock_get_master (clock);
  if (!mclock)
    return TRUE;

  gst_object_unref (mclock);

  return FALSE;
}
