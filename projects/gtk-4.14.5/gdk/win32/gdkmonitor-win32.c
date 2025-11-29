/*
 * Copyright © 2016 Red Hat, Inc
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

#include "gdkprivate-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkmonitor-win32.h"

#include <glib.h>
#include <gio/gio.h>

#include <cfgmgr32.h>
#include <devpropdef.h>
#include <setupapi.h>

#include "gdkprivate-win32.h"

G_DEFINE_TYPE (GdkWin32Monitor, gdk_win32_monitor, GDK_TYPE_MONITOR)

/* MinGW-w64 carelessly put DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER = -1 into this
 * enum, as documented by MSDN. However, with
 * DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL = 0x80000000 and
 * DISPLAYCONFIG_OUTPUT_TECHNOLOGY_FORCE_UINT32 = 0xFFFFFFFF
 * this had the effect of increasing enum size from 4 to 8 bytes,
 * when compiled by GCC (MSVC doesn't have this problem), breaking ABI.
 * At the moment of writing MinGW-w64 headers are still broken.
 * When they are fixed, replace 9999 with actual version numbers.
 * The fix below is not necessarily correct, but it works.
 */
#if SIZEOF_DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY == 4
#  define fixedDISPLAYCONFIG_PATH_INFO DISPLAYCONFIG_PATH_INFO
#  define fixedDISPLAYCONFIG_TARGET_DEVICE_NAME DISPLAYCONFIG_TARGET_DEVICE_NAME
#else
typedef enum {
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER                 = (int) -1,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15                  = (int) 0,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_SVIDEO                = (int) 1,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPOSITE_VIDEO       = (int) 2,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPONENT_VIDEO       = (int) 3,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI                   = (int) 4,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI                  = (int) 5,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS                  = (int) 6,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_D_JPN                 = (int) 8,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDI                   = (int) 9,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL  = (int) 10,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED  = (int) 11,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EXTERNAL          = (int) 12,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED          = (int) 13,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDTVDONGLE            = (int) 14,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL              = (int) 0x80000000,
  fixedDISPLAYCONFIG_OUTPUT_TECHNOLOGY_FORCE_UINT32          = (int) 0xFFFFFFFF
} fixedDISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY;

typedef struct fixedDISPLAYCONFIG_PATH_TARGET_INFO {
  LUID                                       adapterId;
  UINT32                                     id;
  UINT32                                     modeInfoIdx;
  fixedDISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY outputTechnology;
  DISPLAYCONFIG_ROTATION                     rotation;
  DISPLAYCONFIG_SCALING                      scaling;
  DISPLAYCONFIG_RATIONAL                     refreshRate;
  DISPLAYCONFIG_SCANLINE_ORDERING            scanLineOrdering;
  BOOL                                       targetAvailable;
  UINT32                                     statusFlags;
} fixedDISPLAYCONFIG_PATH_TARGET_INFO;

typedef struct fixedDISPLAYCONFIG_PATH_INFO
{
  DISPLAYCONFIG_PATH_SOURCE_INFO      sourceInfo;
  fixedDISPLAYCONFIG_PATH_TARGET_INFO targetInfo;
  UINT32                              flags;
} fixedDISPLAYCONFIG_PATH_INFO;

typedef struct fixedDISPLAYCONFIG_TARGET_DEVICE_NAME
{
  DISPLAYCONFIG_DEVICE_INFO_HEADER            header;
  DISPLAYCONFIG_TARGET_DEVICE_NAME_FLAGS      flags;
  fixedDISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY  outputTechnology;
  UINT16                                      edidManufactureId;
  UINT16                                      edidProductCodeId;
  UINT32                                      connectorInstance;
  WCHAR                                       monitorFriendlyDeviceName[64];
  WCHAR                                       monitorDevicePath[128];
} fixedDISPLAYCONFIG_TARGET_DEVICE_NAME;

#endif


/* MinGW-w64 does not have these functions in its import libraries
 * at the moment of writing.
 * Also, Windows Vista doesn't have these functions at all
 * (according to MSDN it does, but that is a lie), so we'd have
 * to load them manually anyway (otherwise GTK apps won't even start
 * on Vista).
 */
typedef LONG
(WINAPI *funcGetDisplayConfigBufferSizes) (UINT32 flags,
                                           UINT32* numPathArrayElements,
                                           UINT32* numModeInfoArrayElements);

typedef LONG
(WINAPI *funcQueryDisplayConfig) (UINT32 flags,
                                  UINT32* numPathArrayElements,
                                  fixedDISPLAYCONFIG_PATH_INFO* pathArray,
                                  UINT32* numModeInfoArrayElements,
                                  DISPLAYCONFIG_MODE_INFO* modeInfoArray,
                                  DISPLAYCONFIG_TOPOLOGY_ID* currentTopologyId);

typedef LONG
(WINAPI *funcDisplayConfigGetDeviceInfo) (DISPLAYCONFIG_DEVICE_INFO_HEADER* requestPacket);

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 1
#endif

#define G_GUID_FORMAT "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"
#define g_format_guid(guid) (guid)->Data1, \
                            (guid)->Data2, \
                            (guid)->Data3, \
                            (guid)->Data4[0], \
                            (guid)->Data4[1], \
                            (guid)->Data4[2], \
                            (guid)->Data4[3], \
                            (guid)->Data4[4], \
                            (guid)->Data4[5], \
                            (guid)->Data4[6], \
                            (guid)->Data4[7]

static gboolean
get_device_property (HDEVINFO          device_infoset,
                     SP_DEVINFO_DATA  *device_info_data,
                     DEVPROPKEY       *property_key,
                     gpointer         *r_buffer,
                     gsize            *r_buffer_size,
                     DEVPROPTYPE      *r_property_type,
                     GError          **error)
{
  DEVPROPTYPE property_type;
  gpointer property;
  DWORD property_size;

  property = NULL;
  property_size = 0;

  if (!SetupDiGetDevicePropertyW (device_infoset,
                                  device_info_data,
                                  property_key,
                                  &property_type,
                                  property,
                                  property_size,
                                  &property_size,
                                  0))
    {
      DWORD error_code = GetLastError ();

      if (error_code != ERROR_INSUFFICIENT_BUFFER)
        {
          char *emsg = g_win32_error_message (error_code);
          g_warning ("Failed to get device node property {" G_GUID_FORMAT "},%lu size: %s",
                     g_format_guid (&property_key->fmtid),
                     property_key->pid,
                     emsg);
          g_free (emsg);

          return FALSE;
        }
    }

  if (r_buffer)
    {
      property = g_malloc (property_size);

      if (!SetupDiGetDevicePropertyW (device_infoset,
                                      device_info_data,
                                      property_key,
                                      &property_type,
                                      property,
                                      property_size,
                                      &property_size,
                                      0))
        {
          DWORD error_code = GetLastError ();

          char *emsg = g_win32_error_message (error_code);
          g_warning ("Failed to get device node property {" G_GUID_FORMAT "},%lu: %s",
                     g_format_guid (&property_key->fmtid),
                     property_key->pid,
                     emsg);
          g_free (emsg);

          return FALSE;
        }

      *r_buffer = property;
    }

  if (r_buffer_size)
    *r_buffer_size = property_size;

  if (r_property_type)
    *r_property_type = property_type;

  return TRUE;
}

static GPtrArray *
get_monitor_devices (GdkWin32Display *win32_display)
{
  GPtrArray *monitor_array;
  HDEVINFO device_infoset;
  SP_DEVINFO_DATA device_info_data;
  DWORD device_index;
  GUID device_interface_monitor = {0xe6f07b5f, 0xee97, 0x4a90, {0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7}};
  DEVPROPKEY pkey_device_instance_id = {{0x78C34FC8, 0x104A, 0x4ACA, {0x9E, 0xA4, 0x52, 0x4D, 0x52, 0x99, 0x6E, 0x57}}, 256};
  DEVPROPKEY pkey_manufacturer = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 13};
  DEVPROPKEY pkey_display_name = {{0xB725F130, 0x47EF, 0x101A, {0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC}}, 10};

  monitor_array = g_ptr_array_new_with_free_func (g_object_unref);

  device_infoset = SetupDiGetClassDevs (&device_interface_monitor, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

  if (device_infoset == INVALID_HANDLE_VALUE)
    return monitor_array;

  for (device_index = 0; TRUE; device_index++)
    {
      gunichar2 *p;
      char *instance_path;
      gunichar2 *prop;
      DWORD proptype;
      HKEY device_registry_key;
      GdkWin32Monitor *w32mon;
      GdkMonitor *mon;
      unsigned char *edid;
      DWORD edid_size;
      DWORD edid_type;

      memset (&device_info_data, 0, sizeof (device_info_data));
      device_info_data.cbSize = sizeof (device_info_data);

      if (!SetupDiEnumDeviceInfo (device_infoset, device_index, &device_info_data))
        {
          DWORD error_code = GetLastError ();

          if (error_code == ERROR_NO_MORE_ITEMS)
            break;

          g_warning ("SetupDiEnumDeviceInfo() failed: %lu\n", error_code);
          break;
        }

      if (!get_device_property (device_infoset,
                                &device_info_data,
                                &pkey_device_instance_id,
                                (gpointer *) &prop,
                                NULL,
                                &proptype,
                                NULL))
        continue;

      if (proptype != DEVPROP_TYPE_STRING)
        {
          g_free (prop);
          continue;
        }

      w32mon = g_object_new (GDK_TYPE_WIN32_MONITOR, "display", win32_display, NULL);
      mon = GDK_MONITOR (w32mon);

      g_ptr_array_add (monitor_array, w32mon);

      /* Half-initialized monitors are candidates for removal */
      w32mon->remove = TRUE;

      /* device instance ID looks like: DISPLAY\FOO\X&XXXXXXX&X&UIDXXX */
      for (p = prop; p[0]; p++)
        if (p[0] == L'\\')
          p[0] = L'#';
      /* now device instance ID looks like: DISPLAY#FOO#X&XXXXXXX&X&UIDXXX */

      /* instance path looks like: \\?\DISPLAY#FOO#X&XXXXXXX&X&UIDXXX#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7} */
      instance_path = g_strdup_printf ("\\\\?\\%ls#{" G_GUID_FORMAT "}",
                                       prop,
                                       g_format_guid (&device_interface_monitor));
      w32mon->instance_path = g_utf8_strdown (instance_path, -1);
      g_free (instance_path);
      g_free (prop);

      if (get_device_property (device_infoset,
                               &device_info_data,
                               &pkey_manufacturer,
                               (gpointer *) &prop,
                               NULL, &proptype, NULL))
        {
          if (proptype == DEVPROP_TYPE_STRING)
            {
              char *manufacturer = g_utf16_to_utf8 (prop, -1, NULL, NULL, NULL);
              gdk_monitor_set_manufacturer (mon, manufacturer);
              g_free (manufacturer);
            }

          g_free (prop);
        }

      if (get_device_property (device_infoset,
                               &device_info_data,
                               &pkey_display_name,
                               (gpointer *) &prop,
                               NULL, &proptype, NULL))
        {
          if (proptype == DEVPROP_TYPE_STRING)
            {
              char *name = g_utf16_to_utf8 (prop, -1, NULL, NULL, NULL);
              gdk_monitor_set_model (mon, name);
              g_free (name);
            }

          g_free (prop);
        }

      device_registry_key = SetupDiOpenDevRegKey (device_infoset, &device_info_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

      if (device_registry_key == NULL || device_registry_key == INVALID_HANDLE_VALUE)
        continue;

      edid = NULL;
      edid_size = 0;


      if (RegQueryValueExW (device_registry_key, L"EDID",
                            NULL, &edid_type,
                            edid, &edid_size) == ERROR_SUCCESS)
        {
          edid = g_malloc (edid_size);

          if (RegQueryValueExW (device_registry_key, L"EDID",
                                NULL, &edid_type,
                                edid, &edid_size) == ERROR_SUCCESS)
            {
              gdk_monitor_set_physical_size (mon,
                                             ((edid[68] & 0x00F0) << 4) + edid[66],
                                             ((edid[68] & 0x000F) << 8) + edid[67]);
            }

          g_free (edid);
        }

      RegCloseKey (device_registry_key);
    }

  SetupDiDestroyDeviceInfoList (device_infoset);

  return monitor_array;
}

static void
populate_monitor_devices_from_display_config (GPtrArray *monitors)
{
  HMODULE user32;
  LONG return_code;
  funcGetDisplayConfigBufferSizes getDisplayConfigBufferSizes;
  funcQueryDisplayConfig queryDisplayConfig;
  funcDisplayConfigGetDeviceInfo displayConfigGetDeviceInfo;
  UINT32 dispconf_mode_count;
  UINT32 dispconf_path_count;
  fixedDISPLAYCONFIG_PATH_INFO *dispconf_paths;
  DISPLAYCONFIG_MODE_INFO *dispconf_modes;
  int path_index;

  user32 = LoadLibraryA ("user32.dll");

  if (user32 == NULL)
    return;

  getDisplayConfigBufferSizes = (funcGetDisplayConfigBufferSizes) GetProcAddress (user32,
                                                                                  "GetDisplayConfigBufferSizes");
  queryDisplayConfig = (funcQueryDisplayConfig) GetProcAddress (user32,
                                                                "QueryDisplayConfig");
  displayConfigGetDeviceInfo = (funcDisplayConfigGetDeviceInfo) GetProcAddress (user32,
                                                                                "DisplayConfigGetDeviceInfo");
  if (getDisplayConfigBufferSizes == NULL ||
      queryDisplayConfig == NULL ||
      displayConfigGetDeviceInfo == NULL)
    {
      /* This does happen on Windows Vista, so don't warn about this */
      FreeLibrary (user32);

      return;
    }

  return_code = getDisplayConfigBufferSizes (QDC_ONLY_ACTIVE_PATHS,
                                         &dispconf_path_count,
                                         &dispconf_mode_count);

  if (return_code != ERROR_SUCCESS)
    {
      g_warning ("Can't get displayconfig buffer size: 0x%lx\n", return_code);
      FreeLibrary (user32);

      return;
    }

  dispconf_paths = g_new (fixedDISPLAYCONFIG_PATH_INFO, dispconf_path_count);
  dispconf_modes = g_new (DISPLAYCONFIG_MODE_INFO, dispconf_mode_count);

  return_code = queryDisplayConfig (QDC_ONLY_ACTIVE_PATHS,
                                    &dispconf_path_count,
                                    dispconf_paths,
                                    &dispconf_mode_count,
                                    dispconf_modes,
                                    NULL);

  if (return_code != ERROR_SUCCESS)
    {
      g_free (dispconf_paths);
      g_free (dispconf_modes);
      FreeLibrary (user32);

      return;
    }

  for (path_index = 0; path_index < dispconf_path_count; path_index++)
    {
      fixedDISPLAYCONFIG_TARGET_DEVICE_NAME tdn;
      int i;
      GdkWin32Monitor *w32mon;
      GdkMonitor *mon;
      char *path, *path_lower;
      DISPLAYCONFIG_RATIONAL *refresh;

      tdn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
      tdn.header.size = sizeof (tdn);
      tdn.header.adapterId = dispconf_paths[path_index].targetInfo.adapterId;
      tdn.header.id = dispconf_paths[path_index].targetInfo.id;

      return_code = displayConfigGetDeviceInfo (&tdn.header);

      if (return_code != ERROR_SUCCESS)
        continue;

      path = g_utf16_to_utf8 (tdn.monitorDevicePath, -1, NULL, NULL, NULL);

      if (path == NULL)
        continue;

      path_lower = g_utf8_strdown (path, -1);

      g_free (path);

      for (i = 0, w32mon = NULL; i < monitors->len; i++)
        {
          GdkWin32Monitor *m = g_ptr_array_index (monitors, i);

          if (g_strcmp0 (m->instance_path, path_lower) != 0)
            continue;

          w32mon = m;
          break;
       }

      g_free (path_lower);

      if (w32mon == NULL)
        continue;

      if ((dispconf_paths[path_index].flags & DISPLAYCONFIG_PATH_ACTIVE) == 0)
        {
          w32mon->remove = TRUE;
          continue;
        }

      mon = GDK_MONITOR (w32mon);

      if (!tdn.flags.friendlyNameForced)
        {
          /* monitorFriendlyDeviceName is usually nicer */
          char *name = g_utf16_to_utf8 (tdn.monitorFriendlyDeviceName, -1, NULL, NULL, NULL);
          gdk_monitor_set_model (mon, name);
          g_free (name);
        }

      refresh = &dispconf_paths[path_index].targetInfo.refreshRate;
      gdk_monitor_set_refresh_rate (mon,
                                    refresh->Numerator * (UINT64) 1000 / refresh->Denominator);
    }

  g_free (dispconf_paths);
  g_free (dispconf_modes);

  FreeLibrary (user32);
}

typedef struct {
  GPtrArray *monitors;
  gboolean have_monitor_devices;
  GdkWin32Display *display;
} EnumMonitorData;

static BOOL CALLBACK
enum_monitor (HMONITOR hmonitor,
              HDC      hdc,
              LPRECT   rect,
              LPARAM   param)
{
  EnumMonitorData *data = (EnumMonitorData *) param;
  MONITORINFOEXW monitor_info;
  DWORD i_adapter;

  /* Grab monitor_info for this logical monitor */
  monitor_info.cbSize = sizeof (MONITORINFOEXW);
  GetMonitorInfoW (hmonitor, (MONITORINFO *) &monitor_info);

  /* Sidestep to enumerate display adapters */
  for (i_adapter = 0; TRUE; i_adapter++)
    {
      DISPLAY_DEVICEW dd;
      DEVMODEW dm;
      DWORD i_monitor;
      DWORD frequency;

      memset (&dd, 0, sizeof (dd));
      dd.cb = sizeof (dd);

      /* Get i_adapter'th adapter */
      if (!EnumDisplayDevicesW (NULL, i_adapter, &dd, EDD_GET_DEVICE_INTERFACE_NAME))
        break;

      if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0)
        continue;

      /* Match this display adapter to one for which we've got monitor_info
       * (logical monitor == adapter)
       */
      if (wcscmp (dd.DeviceName, monitor_info.szDevice) != 0)
        continue;

      dm.dmSize = sizeof (dm);

      /* Grab refresh rate for this adapter while we're at it */
      if (EnumDisplaySettingsW (dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm))
        frequency = dm.dmDisplayFrequency;
      else
        frequency = 0;

      /* Enumerate monitors connected to this display adapter */
      for (i_monitor = 0; TRUE; i_monitor++)
        {
          DISPLAY_DEVICEW dd_monitor;
          char *device_id_lower, *tmp;
          DWORD i;
          GdkWin32Monitor *w32mon;
          GdkMonitor *mon;
          GdkRectangle rect;
          int scale;

          memset (&dd_monitor, 0, sizeof (dd_monitor));
          dd_monitor.cb = sizeof (dd_monitor);

          if (data->have_monitor_devices)
            {
              /* Get i_monitor'th monitor */
              if (!EnumDisplayDevicesW (dd.DeviceName, i_monitor, &dd_monitor, EDD_GET_DEVICE_INTERFACE_NAME))
                break;

              tmp = g_utf16_to_utf8 (dd_monitor.DeviceID, -1, NULL, NULL, NULL);

              if (tmp == NULL)
                continue;

              device_id_lower = g_utf8_strdown (tmp, -1);
              g_free (tmp);

              /* Match this monitor to one of the monitor devices we found earlier */
              for (i = 0, w32mon = NULL; i < data->monitors->len; i++)
                {
                  GdkWin32Monitor *m = g_ptr_array_index (data->monitors, i);

                  if (g_strcmp0 (device_id_lower, m->instance_path) != 0)
                    continue;

                  w32mon = m;
                  break;
                }

              g_free (device_id_lower);

              if (w32mon == NULL)
                continue;

              w32mon->hmonitor = hmonitor;
            }
          else
            {
              /* Headless PC or a virtual machine, it has no monitor devices.
               * Make one up.
               */
              w32mon = g_object_new (GDK_TYPE_WIN32_MONITOR, "display", data->display, NULL);
              g_ptr_array_add (data->monitors, w32mon);
              i = data->monitors->len - 1;
              w32mon->madeup = TRUE;
            }

          mon = GDK_MONITOR (w32mon);

          if (gdk_monitor_get_model (mon) == NULL)
            {
              
              char *name = NULL;

              /* Only use dd.DeviceName as a last resort, as it is just
               * \\.\DISPLAYX\MonitorY (for some values of X and Y).
               */
              if (dd_monitor.DeviceName[0] != L'\0')
                name = g_utf16_to_utf8 (dd_monitor.DeviceName, -1, NULL, NULL, NULL);
              else if (dd.DeviceName[0] != L'\0')
                name = g_utf16_to_utf8 (dd.DeviceName, -1, NULL, NULL, NULL);

              if (name != NULL)
                gdk_monitor_set_model (mon, name);

              g_free (name);
            }

          /* GetDeviceCaps seems to provide a wild guess, prefer more precise EDID info */
          if (gdk_monitor_get_width_mm (mon) == 0 &&
              gdk_monitor_get_height_mm (mon) == 0)
            {
              HDC hDC = CreateDCW (L"DISPLAY", monitor_info.szDevice, NULL, NULL);

              gdk_monitor_set_physical_size (mon,
                                             GetDeviceCaps (hDC, HORZSIZE),
                                             GetDeviceCaps (hDC, VERTSIZE));
              DeleteDC (hDC);
            }

          /* frequency is in Hz and is unsigned long,
           * prefer more precise refresh_rate found earlier,
           * which comes as a Numerator & Denominator pair and is more precise.
           */
          if (gdk_monitor_get_refresh_rate (mon) == 0)
            gdk_monitor_set_refresh_rate (mon, frequency * 1000);

          /* This is the reason this function exists. This data is not available
           * via other functions.
           */
          rect.x = monitor_info.rcWork.left;
          rect.y = monitor_info.rcWork.top;
          rect.width = (monitor_info.rcWork.right - monitor_info.rcWork.left);
          rect.height = (monitor_info.rcWork.bottom - monitor_info.rcWork.top);
          /* This is temporary, scale will be applied below */
          w32mon->work_rect = rect;

          if (data->display->has_fixed_scale)
            scale = data->display->surface_scale;
          else
            {
              /* First acquire the scale using the current screen */
              scale = gdk_win32_display_get_monitor_scale_factor (data->display, NULL, NULL);

              /* acquire the scale using the monitor which the window is nearest on Windows 8.1+ */
              if (data->display->have_at_least_win81)
                {
                  HMONITOR hmonitor;
                  POINT pt;

                  pt.x = w32mon->work_rect.x + w32mon->work_rect.width / 2;
                  pt.y = w32mon->work_rect.y + w32mon->work_rect.height / 2;
                  hmonitor = MonitorFromPoint (pt, MONITOR_DEFAULTTONEAREST);
                  scale = gdk_win32_display_get_monitor_scale_factor (data->display, NULL, hmonitor);
                }
            }

          gdk_monitor_set_scale_factor (mon, scale);
          /* Now apply the scale to the work rectangle */
          w32mon->work_rect.x /= scale;
          w32mon->work_rect.y /= scale;
          w32mon->work_rect.width /= scale;
          w32mon->work_rect.height /= scale;

          rect.x = monitor_info.rcMonitor.left / scale;
          rect.y = monitor_info.rcMonitor.top / scale;
          rect.width = (monitor_info.rcMonitor.right - monitor_info.rcMonitor.left) / scale;
          rect.height = (monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top) / scale;
          gdk_monitor_set_geometry (mon, &rect);

          if (monitor_info.dwFlags & MONITORINFOF_PRIMARY && i != 0)
            {
              /* Put primary monitor at index 0, just in case somebody needs
               * to know which one is the primary.
               */
              GdkWin32Monitor *temp = g_ptr_array_index (data->monitors, 0);
              g_ptr_array_index (data->monitors, 0) = w32mon;
              g_ptr_array_index (data->monitors, i) = temp;
            }

          /* Work area is the most important component, actively used by GTK,
           * but our initial list of monitor devices did not have it.
           * Any monitor devices not matched in this functions will have
           * 0-filled work area and will therefore be useless, so let them
           * keep remove == TRUE and be removed further up the stack.
           */
          w32mon->remove = FALSE;

          /* One virtual monitor per display adapter */
          if (w32mon->madeup)
            break;
        }
    }

  return TRUE;
}

static void
prune_monitors (EnumMonitorData *data)
{
  int i;

  for (i = 0; i < data->monitors->len; i++)
    {
      GdkWin32Monitor *m;

      m = g_ptr_array_index (data->monitors, i);

      if (m->remove)
        g_ptr_array_remove_index (data->monitors, i--);
    }
}

GPtrArray *
_gdk_win32_display_get_monitor_list (GdkWin32Display *win32_display)
{
  EnumMonitorData data;

  data.display = win32_display;
  data.monitors = get_monitor_devices (win32_display);

  if (data.monitors->len != 0)
    {
      populate_monitor_devices_from_display_config (data.monitors);
      data.have_monitor_devices = TRUE;
    }
  else
    {
      data.have_monitor_devices = FALSE;
    }

  EnumDisplayMonitors (NULL, NULL, enum_monitor, (LPARAM) &data);

  prune_monitors (&data);

  if (data.monitors->len == 0 && data.have_monitor_devices)
    {
      /* We thought we had monitors, but enumeration eventually failed, and
       * we have none. Try again, this time making stuff up as we go.
       */
      data.have_monitor_devices = FALSE;
      EnumDisplayMonitors (NULL, NULL, enum_monitor, (LPARAM) &data);
      prune_monitors (&data);
    }

  return data.monitors;
}

static void
gdk_win32_monitor_finalize (GObject *object)
{
  GdkWin32Monitor *win32_monitor = GDK_WIN32_MONITOR (object);

  g_free (win32_monitor->instance_path);

  G_OBJECT_CLASS (gdk_win32_monitor_parent_class)->finalize (object);
}

int
_gdk_win32_monitor_compare (GdkWin32Monitor *a,
                            GdkWin32Monitor *b)
{
  if (a->instance_path != NULL &&
      b->instance_path != NULL)
    return g_strcmp0 (a->instance_path, b->instance_path);

  return a == b ? 0 : a < b ? -1 : 1;
}

/**
 * gdk_win32_monitor_get_workarea:
 * @monitor: a `GdkMonitor`
 * @workarea: (out): a `GdkRectangle` to be filled with the monitor workarea
 *
 * Retrieves the size and position of the “work area” on a monitor
 * within the display coordinate space.
 *
 * The returned geometry is in ”application pixels”, not in ”device pixels”
 * (see [method@Gdk.Monitor.get_scale_factor]).
 */
void
gdk_win32_monitor_get_workarea (GdkMonitor   *monitor,
                                GdkRectangle *dest)
{
  GdkWin32Monitor *win32_monitor = GDK_WIN32_MONITOR (monitor);

  *dest = win32_monitor->work_rect;
}

static void
gdk_win32_monitor_init (GdkWin32Monitor *monitor)
{
}

static void
gdk_win32_monitor_class_init (GdkWin32MonitorClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_win32_monitor_finalize;
}
