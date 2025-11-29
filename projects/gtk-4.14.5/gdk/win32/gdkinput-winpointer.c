/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 the GTK team
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

#include <gdk/gdk.h>
#include "gdkwin32.h"
#include "gdkprivate-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdevice-virtual.h"
#include "gdkdevice-winpointer.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"
#include "gdkseatdefaultprivate.h"

#include "gdkinput-winpointer.h"

#include <windows.h>

#include <tchar.h>
#include <tpcshrd.h>
#include <hidsdi.h>

#define HID_STRING_BYTES_LIMIT 200
#define VID_PID_CHARS 4

typedef BOOL
(WINAPI *registerPointerDeviceNotifications_t)(HWND window, BOOL notifyRange);
typedef BOOL
(WINAPI *getPointerDevices_t)(UINT32 *deviceCount, POINTER_DEVICE_INFO *pointerDevices);
typedef BOOL
(WINAPI *getPointerDeviceCursors_t)(HANDLE device, UINT32 *cursorCount, POINTER_DEVICE_CURSOR_INFO *deviceCursors);
typedef BOOL
(WINAPI *getPointerDeviceRects_t)(HANDLE device, RECT *pointerDeviceRect, RECT *displayRect);
typedef BOOL
(WINAPI *getPointerType_t)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);
typedef BOOL
(WINAPI *getPointerCursorId_t)(UINT32 pointerId, UINT32 *cursorId);
typedef BOOL
(WINAPI *getPointerPenInfo_t)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *getPointerTouchInfo_t)(UINT32 pointerId, POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *getPointerPenInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_PEN_INFO *penInfo);
typedef BOOL
(WINAPI *getPointerTouchInfoHistory_t)(UINT32 pointerId, UINT32 *entriesCount, POINTER_TOUCH_INFO *touchInfo);
typedef BOOL
(WINAPI *setGestureConfig_t)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
typedef BOOL
(WINAPI *setWindowFeedbackSetting_t)(HWND hwnd, FEEDBACK_TYPE feedback, DWORD dwFlags, UINT32 size, const VOID *configuration);

static registerPointerDeviceNotifications_t registerPointerDeviceNotifications;
static getPointerDevices_t getPointerDevices;
static getPointerDeviceCursors_t getPointerDeviceCursors;
static getPointerDeviceRects_t getPointerDeviceRects;
static getPointerType_t getPointerType;
static getPointerCursorId_t getPointerCursorId;
static getPointerPenInfo_t getPointerPenInfo;
static getPointerTouchInfo_t getPointerTouchInfo;
static getPointerPenInfoHistory_t getPointerPenInfoHistory;
static getPointerTouchInfoHistory_t getPointerTouchInfoHistory;
static setGestureConfig_t setGestureConfig;
static setWindowFeedbackSetting_t setWindowFeedbackSetting;

static ATOM notifications_window_class;
static HWND notifications_window_handle;

static GPtrArray *ignored_interactions;

static inline void
winpointer_ignore_interaction (UINT32 pointer_id)
{
  g_ptr_array_add (ignored_interactions, GUINT_TO_POINTER (pointer_id));
}

static inline void
winpointer_remove_ignored_interaction (UINT32 pointer_id)
{
  g_ptr_array_remove_fast (ignored_interactions, GUINT_TO_POINTER (pointer_id));
}

static inline gboolean
winpointer_should_ignore_interaction (UINT32 pointer_id)
{
  return g_ptr_array_find (ignored_interactions, GUINT_TO_POINTER (pointer_id), NULL);
}

static inline guint32
winpointer_get_time (MSG *msg,
                     POINTER_INFO *info)
{
  return info->dwTime != 0 ? info->dwTime : msg->time;
}

static inline gboolean
winpointer_is_eraser (POINTER_PEN_INFO *pen_info)
{
  return (pen_info->penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0;
}

static inline gboolean
winpointer_should_filter_message (MSG *msg,
                                  POINTER_INPUT_TYPE type)
{
  switch (type)
    {
    case PT_TOUCH:
      return msg->message == WM_POINTERENTER ||
             msg->message == WM_POINTERLEAVE;
    break;
    }

  return FALSE;
}

static inline double*
copy_axes (double *axes)
{
  return g_memdup2 (axes, sizeof (double) * GDK_AXIS_LAST);
}

static GdkDeviceWinpointer*
winpointer_find_device_with_source (HANDLE device_handle,
                                    UINT32 cursor_id,
                                    GdkInputSource input_source)
{
  for (GList *l = _gdk_device_manager->winpointer_devices; l != NULL; l = l->next)
    {
      GdkDeviceWinpointer *device = (GdkDeviceWinpointer*) l->data;

      if (device->device_handle == device_handle &&
          device->start_cursor_id <= cursor_id &&
          device->end_cursor_id >= cursor_id &&
          gdk_device_get_source ((GdkDevice*) device) == input_source)
        {
          return device;
        }
    }

  return NULL;
}

static gboolean
winpointer_get_event_type (MSG *msg,
                           POINTER_INFO *info,
                           GdkEventType *evt_type)
{
  switch (info->pointerType)
    {
    case PT_PEN:
      switch (msg->message)
        {
        case WM_POINTERENTER:
          g_return_val_if_fail (IS_POINTER_NEW_WPARAM (msg->wParam), FALSE);
          *evt_type = GDK_PROXIMITY_IN;
        return TRUE;
        case WM_POINTERLEAVE:
          g_return_val_if_fail (!IS_POINTER_INRANGE_WPARAM (msg->wParam), FALSE);
          *evt_type = GDK_PROXIMITY_OUT;
        return TRUE;
        case WM_POINTERDOWN:
          *evt_type = GDK_BUTTON_PRESS;
        return TRUE;
        case WM_POINTERUP:
          *evt_type = GDK_BUTTON_RELEASE;
        return TRUE;
        case WM_POINTERUPDATE:
          *evt_type = GDK_MOTION_NOTIFY;
        return TRUE;
        }
    break;
    case PT_TOUCH:
      if (IS_POINTER_CANCELED_WPARAM (msg->wParam) ||
          !HAS_POINTER_CONFIDENCE_WPARAM (msg->wParam))
        {
          winpointer_ignore_interaction (GET_POINTERID_WPARAM (msg->wParam));

          if (((info->pointerFlags & POINTER_FLAG_INCONTACT) &&
               (info->pointerFlags & POINTER_FLAG_UPDATE)) ||
              (info->pointerFlags & POINTER_FLAG_UP))
            {
              *evt_type = GDK_TOUCH_CANCEL;
              return TRUE;
            }
          else
            return FALSE;
        }

      g_return_val_if_fail (msg->message != WM_POINTERENTER &&
                            msg->message != WM_POINTERLEAVE, FALSE);

      switch (msg->message)
        {
          case WM_POINTERDOWN:
            *evt_type = GDK_TOUCH_BEGIN;
          return TRUE;
          case WM_POINTERUP:
            *evt_type = GDK_TOUCH_END;
          return TRUE;
          case WM_POINTERUPDATE:
            if (!IS_POINTER_INCONTACT_WPARAM (msg->wParam))
              return FALSE;
            *evt_type = GDK_TOUCH_UPDATE;
          return TRUE;
        }
    break;
    }

  g_warn_if_reached ();

  return FALSE;
}

static void
winpointer_make_event (GdkDeviceWinpointer *device,
                       GdkDeviceTool *tool,
                       GdkSurface *surface,
                       MSG *msg,
                       POINTER_INFO *info)
{
  guint32 time = 0;
  double screen_x = 0.0;
  double screen_y = 0.0;
  double x = 0.0;
  double y = 0.0;
  unsigned int state = 0;
  unsigned int button = 0;
  double axes[GDK_AXIS_LAST];
  GdkEventSequence *sequence = NULL;
  gboolean emulating_pointer = FALSE;
  POINT client_area_coordinates;
  GdkWin32Surface *impl = NULL;
  GdkEventType evt_type;
  GdkEvent *evt = NULL;
  GdkDevice *core_device = NULL;

  core_device = _gdk_device_manager->core_pointer;

  if (!winpointer_get_event_type (msg, info, &evt_type))
    return;

  time = winpointer_get_time (msg, info);

  screen_x = device->origin_x + info->ptHimetricLocation.x * device->scale_x;
  screen_y = device->origin_y + info->ptHimetricLocation.y * device->scale_y;

  client_area_coordinates.x = 0;
  client_area_coordinates.y = 0;
  ClientToScreen (GDK_SURFACE_HWND (surface), &client_area_coordinates);
  x = screen_x - client_area_coordinates.x;
  y = screen_y - client_area_coordinates.y;

  impl = GDK_WIN32_SURFACE (surface);
  x /= impl->surface_scale;
  y /= impl->surface_scale;

  state = 0;
  /* Note that info->dwKeyStates is not reliable, use GetKeyState() */
  if (GetKeyState (VK_CONTROL) < 0)
    state |= GDK_CONTROL_MASK;
  if (GetKeyState (VK_SHIFT) < 0)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_ALT_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  device->last_button_mask = 0;
  if (((info->pointerFlags & POINTER_FLAG_FIRSTBUTTON) &&
       (info->ButtonChangeType != POINTER_CHANGE_FIRSTBUTTON_DOWN))
      || info->ButtonChangeType == POINTER_CHANGE_FIRSTBUTTON_UP)
    device->last_button_mask |= GDK_BUTTON1_MASK;
  if (((info->pointerFlags & POINTER_FLAG_SECONDBUTTON) &&
       (info->ButtonChangeType != POINTER_CHANGE_SECONDBUTTON_DOWN))
      || info->ButtonChangeType == POINTER_CHANGE_SECONDBUTTON_UP)
    device->last_button_mask |= GDK_BUTTON3_MASK;
  state |= device->last_button_mask;

  memset (axes, 0, sizeof (axes));
  switch (info->pointerType)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO *pen_info = (POINTER_PEN_INFO*) info;

        axes[GDK_AXIS_PRESSURE] = (pen_info->penMask & PEN_MASK_PRESSURE) ?
                                   pen_info->pressure / 1024.0 :
                                  (pen_info->pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) ?
                                   1.0 : 0.0;
        axes[GDK_AXIS_XTILT] = (pen_info->penMask & PEN_MASK_TILT_X) ?
                                pen_info->tiltX / 90.0 : 0.0;
        axes[GDK_AXIS_YTILT] = (pen_info->penMask & PEN_MASK_TILT_Y) ?
                                pen_info->tiltY / 90.0 : 0.0;
        axes[GDK_AXIS_ROTATION] = (pen_info->penMask & PEN_MASK_ROTATION) ?
                                   pen_info->rotation / 360.0 : 0.0;
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO *touch_info = (POINTER_TOUCH_INFO*) info;

        axes[GDK_AXIS_PRESSURE] = (touch_info->touchMask & TOUCH_MASK_PRESSURE) ?
                                   touch_info->pressure / 1024.0 :
                                  (touch_info->pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) ?
                                   1.0 : 0.0;
      }
    break;
    }

  sequence = (GdkEventSequence*) GUINT_TO_POINTER (info->pointerId);
  emulating_pointer = (info->pointerFlags & POINTER_FLAG_PRIMARY) != 0;
  button = (info->pointerFlags & POINTER_FLAG_FIRSTBUTTON) ||
           (info->ButtonChangeType == POINTER_CHANGE_FIRSTBUTTON_UP) ? 1 : 3;

  switch (evt_type)
    {
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      evt = gdk_proximity_event_new (evt_type,
                                     surface,
                                     core_device,
                                     tool,
                                     time);
    break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      evt = gdk_button_event_new (evt_type,
                                  surface,
                                  core_device,
                                  tool,
                                  time,
                                  state,
                                  button,
                                  x,
                                  y,
                                  copy_axes (axes));
    break;
    case GDK_MOTION_NOTIFY:
      evt = gdk_motion_event_new (surface,
                                  core_device,
                                  tool,
                                  time,
                                  state,
                                  x,
                                  y,
                                  copy_axes (axes));
    break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCH_END:
      evt = gdk_touch_event_new (evt_type,
                                 sequence,
                                 surface,
                                 core_device,
                                 time,
                                 state,
                                 x,
                                 y,
                                 copy_axes (axes),
                                 emulating_pointer);
    break;
    default:
      g_warn_if_reached ();
    break;
    }

  if (evt_type == GDK_PROXIMITY_OUT)
    gdk_device_update_tool ((GdkDevice*) device, NULL);

  if (G_LIKELY (evt))
    {
      _gdk_device_virtual_set_active (core_device, (GdkDevice*) device);
      _gdk_win32_append_event (evt);
    }
}

void
gdk_winpointer_input_events (GdkSurface *surface,
                             crossing_cb_t crossing_cb,
                             MSG *msg)
{
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;
  UINT32 cursor_id = 0;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return;
    }

  if (!getPointerCursorId (pointer_id, &cursor_id))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerCursorId");
      return;
    }

  if (winpointer_should_filter_message (msg, type))
    return;

  if (winpointer_should_ignore_interaction (pointer_id))
    return;

  switch (type)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO *infos = NULL;
        UINT32 history_count = 0;
        GdkDeviceWinpointer *device = NULL;
        GdkDeviceTool *tool = NULL;
        UINT32 h = 0;

        do
          {
            infos = g_new0 (POINTER_PEN_INFO, history_count);
            if (!getPointerPenInfoHistory (pointer_id, &history_count, infos))
              {
                WIN32_API_FAILED_LOG_ONCE ("GetPointerPenInfoHistory");
                g_free (infos);
                return;
              }
          }
        while (!infos && history_count > 0);

        if (G_UNLIKELY (history_count == 0))
          return;

        device = winpointer_find_device_with_source (infos->pointerInfo.sourceDevice, cursor_id, GDK_SOURCE_PEN);
        if (G_UNLIKELY (!device))
          {
            g_free (infos);
            return;
          }

        if (!winpointer_is_eraser (infos))
          tool = device->tool_pen;
        else
          tool = device->tool_eraser;

        gdk_device_update_tool ((GdkDevice*) device, tool);

        h = history_count - 1;

        if (crossing_cb)
          {
            POINT screen_pt = infos[h].pointerInfo.ptPixelLocation;
            guint32 event_time = winpointer_get_time (msg, &infos[h].pointerInfo);

            crossing_cb(GDK_DEVICE (device), surface, &screen_pt, event_time);
          }

        do
          winpointer_make_event (device, tool, surface, msg, (POINTER_INFO*) &infos[h]);
        while (h-- > 0);

        g_free (infos);
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO *infos = NULL;
        UINT32 history_count = 0;
        GdkDeviceWinpointer *device = NULL;
        UINT32 h = 0;

        do
          {
            infos = g_new0 (POINTER_TOUCH_INFO, history_count);
            if (!getPointerTouchInfoHistory (pointer_id, &history_count, infos))
              {
                WIN32_API_FAILED_LOG_ONCE ("GetPointerTouchInfoHistory");
                g_free (infos);
                return;
              }
          }
        while (!infos && history_count > 0);

        if (G_UNLIKELY (history_count == 0))
          return;

        device = winpointer_find_device_with_source (infos->pointerInfo.sourceDevice, cursor_id, GDK_SOURCE_TOUCHSCREEN);
        if (G_UNLIKELY (!device))
          {
            g_free (infos);
            return;
          }

        h = history_count - 1;

        if (crossing_cb)
          {
            POINT screen_pt = infos[h].pointerInfo.ptPixelLocation;
            guint32 event_time = winpointer_get_time (msg, &infos[h].pointerInfo);

            crossing_cb(GDK_DEVICE (device), surface, &screen_pt, event_time);
          }

        do
          winpointer_make_event (device, NULL, surface, msg, (POINTER_INFO*) &infos[h]);
        while (h-- > 0);

        g_free (infos);
      }
    break;
    }
}

gboolean
gdk_winpointer_get_message_info (MSG *msg,
                                 GdkDevice **device,
                                 guint32 *time)
{
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;
  UINT32 cursor_id = 0;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return FALSE;
    }

  if (!getPointerCursorId (pointer_id, &cursor_id))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerCursorId");
      return FALSE;
    }

  switch (type)
    {
    case PT_PEN:
      {
        POINTER_PEN_INFO pen_info;

        if (!getPointerPenInfo (pointer_id, &pen_info))
          {
            WIN32_API_FAILED_LOG_ONCE ("GetPointerPenInfo");
            return FALSE;
          }

        *device = (GdkDevice*) winpointer_find_device_with_source (pen_info.pointerInfo.sourceDevice, cursor_id, GDK_SOURCE_PEN);
        *time = winpointer_get_time (msg, &pen_info.pointerInfo);
      }
    break;
    case PT_TOUCH:
      {
        POINTER_TOUCH_INFO touch_info;

        if (!getPointerTouchInfo (pointer_id, &touch_info))
            {
              WIN32_API_FAILED_LOG_ONCE ("GetPointerTouchInfo");
              return FALSE;
            }

        *device = GDK_DEVICE (winpointer_find_device_with_source (touch_info.pointerInfo.sourceDevice,
                                                                  cursor_id,
                                                                  GDK_SOURCE_TOUCHSCREEN));

        *time = winpointer_get_time (msg, &touch_info.pointerInfo);
      }
    break;
    default:
      g_warn_if_reached ();
      return FALSE;
    break;
    }

  return *device ? TRUE : FALSE;
}

gboolean
gdk_winpointer_should_forward_message (MSG *msg)
{
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);
  POINTER_INPUT_TYPE type = PT_POINTER;

  if (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return TRUE;
    }

  return !(type == PT_PEN || type == PT_TOUCH);
}

void
gdk_winpointer_interaction_ended (MSG *msg)
{
  winpointer_remove_ignored_interaction (GET_POINTERID_WPARAM (msg->wParam));
}

static inline double
utils_rect_width (RECT *rect)
{
  return rect->right - rect->left;
}

static inline double
utils_rect_height (RECT *rect)
{
  return rect->bottom - rect->top;
}

static inline gboolean
utils_rect_is_degenerate (RECT *rect)
{
  return utils_rect_width (rect) == 0 || utils_rect_height (rect) == 0;
}

static gboolean
winpointer_device_update_scale_factors (GdkDeviceWinpointer *device)
{
  RECT device_rect;
  RECT display_rect;

  if (!getPointerDeviceRects (device->device_handle, &device_rect, &display_rect))
    {
      WIN32_API_FAILED ("GetPointerDeviceRects");
      return FALSE;
    }

  if (utils_rect_is_degenerate (&device_rect))
    {
      g_warning ("Invalid coordinates from GetPointerDeviceRects");
      return FALSE;
    }

  device->origin_x = display_rect.left;
  device->origin_y = display_rect.top;
  device->scale_x = utils_rect_width (&display_rect) / utils_rect_width (&device_rect);
  device->scale_y = utils_rect_height (&display_rect) / utils_rect_height (&device_rect);

  return TRUE;
}

static void
winpointer_get_device_details (HANDLE device,
                               char *vid,
                               char *pid,
                               char **manufacturer,
                               char **product)
{
  RID_DEVICE_INFO info;
  UINT wchars_count = 0;
  UINT size = 0;

  memset (&info, 0, sizeof (info));

  info.cbSize = sizeof (info);
  size = sizeof (info);

  if (GetRawInputDeviceInfoW (device, RIDI_DEVICEINFO, &info, &size) > 0 &&
      info.dwType == RIM_TYPEHID &&
      info.hid.dwVendorId > 0 &&
      info.hid.dwProductId > 0)
    {
      const char *format_string = "%0" G_STRINGIFY (VID_PID_CHARS) "x";

      g_snprintf (vid, VID_PID_CHARS + 1, format_string, (unsigned) info.hid.dwVendorId);
      g_snprintf (pid, VID_PID_CHARS + 1, format_string, (unsigned) info.hid.dwProductId);
    }

  if (GetRawInputDeviceInfoW (device, RIDI_DEVICENAME, NULL, &wchars_count) == 0)
    {
      gunichar2 *device_path = g_new0 (gunichar2, wchars_count);

      if (GetRawInputDeviceInfoW (device, RIDI_DEVICENAME, device_path, &wchars_count) > 0)
        {
          HANDLE device_file = CreateFileW (device_path,
                                            0,
                                            FILE_SHARE_READ |
                                            FILE_SHARE_WRITE |
                                            FILE_SHARE_DELETE,
                                            NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_SESSION_AWARE,
                                            NULL);

          if (device_file != INVALID_HANDLE_VALUE)
            {
              gunichar2 *buffer = g_malloc0 (HID_STRING_BYTES_LIMIT);

              if (HidD_GetManufacturerString (device_file, buffer, HID_STRING_BYTES_LIMIT))
                if (buffer[0])
                  *manufacturer = g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);

              if (HidD_GetProductString (device_file, buffer, HID_STRING_BYTES_LIMIT))
                if (buffer[0])
                  *product = g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);

              g_free (buffer);
              CloseHandle (device_file);
            }
        }

      g_free (device_path);
    }
}

static void
winpointer_create_device (POINTER_DEVICE_INFO *info,
                          GdkInputSource source)
{
  GdkDeviceWinpointer *device = NULL;
  GdkSeat *seat = NULL;
  unsigned num_touches = 0;
  char vid[VID_PID_CHARS + 1];
  char pid[VID_PID_CHARS + 1];
  char *manufacturer = NULL;
  char *product = NULL;
  char *base_name = NULL;
  char *name = NULL;
  UINT32 num_cursors = 0;
  GdkAxisFlags axes_flags = 0;

  seat = gdk_display_get_default_seat (_gdk_display);

  memset (pid, 0, VID_PID_CHARS + 1);
  memset (vid, 0, VID_PID_CHARS + 1);

  if (!getPointerDeviceCursors (info->device, &num_cursors, NULL))
    {
      WIN32_API_FAILED ("GetPointerDeviceCursors");
      return;
    }

  if (num_cursors == 0)
    return;

  winpointer_get_device_details (info->device, vid, pid, &manufacturer, &product);

  /* build up the name */
  if (!manufacturer && vid[0])
    manufacturer = g_strdup (vid);

  if (!product && pid[0])
    product = g_strdup (pid);

  if (manufacturer && product)
    base_name = g_strconcat (manufacturer, " ", product, NULL);

  if (!base_name && info->productString[0])
    base_name = g_utf16_to_utf8 (info->productString, -1, NULL, NULL, NULL);

  if (!base_name)
    base_name = g_strdup ("Unnamed");

  switch (source)
    {
    case GDK_SOURCE_PEN:
      name = g_strconcat (base_name, " Pen", NULL);
    break;

    case GDK_SOURCE_TOUCHSCREEN:
      num_touches = info->maxActiveContacts;
      name = g_strconcat (base_name, " Finger touch", NULL);
    break;

    default:
      name = g_strdup (base_name);
    break;
    }

  device = g_object_new (GDK_TYPE_DEVICE_WINPOINTER,
                         "display", _gdk_display,
                         "seat", seat,
                         "has-cursor", TRUE,
                         "source", source,
                         "name", name,
                         "num-touches", num_touches,
                         "vendor-id", vid[0] ? vid : NULL,
                         "product-id", pid[0] ? pid : NULL,
                         NULL);

  switch (source)
    {
    case GDK_SOURCE_PEN:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);
      axes_flags |= GDK_AXIS_FLAG_PRESSURE;

      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_XTILT, -1.0, 1.0, 1.0 / 90.0);
      axes_flags |= GDK_AXIS_FLAG_XTILT;

      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_YTILT, -1.0, 1.0, 1.0 / 90.0);
      axes_flags |= GDK_AXIS_FLAG_YTILT;

      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_ROTATION, 0.0, 1.0, 1.0 / 360.0);
      axes_flags |= GDK_AXIS_FLAG_ROTATION;
    break;

    case GDK_SOURCE_TOUCHSCREEN:
      _gdk_device_add_axis (GDK_DEVICE (device), GDK_AXIS_PRESSURE, 0.0, 1.0, 1.0 / 1024.0);
      axes_flags |= GDK_AXIS_FLAG_PRESSURE;
    break;

    default:
      g_warn_if_reached ();
    break;
    }

  device->device_handle = info->device;
  device->start_cursor_id = info->startingCursorId;
  device->end_cursor_id = info->startingCursorId + num_cursors - 1;

  if (!winpointer_device_update_scale_factors (device))
    {
      g_set_object (&device, NULL);
      goto cleanup;
    }

  switch (source)
    {
    case GDK_SOURCE_PEN:
      {
        device->tool_pen = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_PEN, axes_flags);
        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), device->tool_pen);

        device->tool_eraser = gdk_device_tool_new (0, 0, GDK_DEVICE_TOOL_TYPE_ERASER, axes_flags);
        gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), device->tool_eraser);
      }
    break;
    case GDK_SOURCE_TOUCHSCREEN:
    break;
    default:
      g_warn_if_reached ();
    break;
    }

  _gdk_device_manager->winpointer_devices = g_list_append (_gdk_device_manager->winpointer_devices, device);

  _gdk_device_set_associated_device (GDK_DEVICE (device), _gdk_device_manager->core_pointer);
  _gdk_device_add_physical_device (_gdk_device_manager->core_pointer, GDK_DEVICE (device));

  gdk_seat_default_add_physical_device (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));

cleanup:
  g_free (name);
  g_free (base_name);
  g_free (product);
  g_free (manufacturer);
}

static void
winpointer_create_devices (POINTER_DEVICE_INFO *info)
{
  switch (info->pointerDeviceType)
    {
    case POINTER_DEVICE_TYPE_INTEGRATED_PEN:
    case POINTER_DEVICE_TYPE_EXTERNAL_PEN:
      winpointer_create_device (info, GDK_SOURCE_PEN);
    break;
    case POINTER_DEVICE_TYPE_TOUCH:
      winpointer_create_device (info, GDK_SOURCE_TOUCHSCREEN);
    break;
    default:
      g_warn_if_reached ();
    break;
    }
}

static gboolean
winpointer_find_device_in_system_list (GdkDeviceWinpointer *device,
                                       POINTER_DEVICE_INFO *infos,
                                       UINT32 infos_count)
{
  for (UINT32 i = 0; i < infos_count; i++)
    {
      if (device->device_handle == infos[i].device &&
          device->start_cursor_id == infos[i].startingCursorId)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
winpointer_find_system_device_in_device_manager (POINTER_DEVICE_INFO *info)
{
  for (GList *l = _gdk_device_manager->winpointer_devices; l != NULL; l = l->next)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (l->data);

      if (device->device_handle == info->device &&
          device->start_cursor_id == info->startingCursorId)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static void
winpointer_enumerate_devices (void)
{
  POINTER_DEVICE_INFO *infos = NULL;
  UINT32 infos_count = 0;
  UINT32 i = 0;
  GList *current = NULL;

  do
    {
      infos = g_new0 (POINTER_DEVICE_INFO, infos_count);
      if (!getPointerDevices (&infos_count, infos))
        {
          WIN32_API_FAILED ("GetPointerDevices");
          g_free (infos);
          return;
        }
    }
  while (infos_count > 0 && !infos);

  current = _gdk_device_manager->winpointer_devices;

  while (current != NULL)
    {
      GdkDeviceWinpointer *device = GDK_DEVICE_WINPOINTER (current->data);
      GList *next = current->next;

      if (!winpointer_find_device_in_system_list (device, infos, infos_count))
        {
          GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (device));

          _gdk_device_manager->winpointer_devices = g_list_delete_link (_gdk_device_manager->winpointer_devices,
                                                                        current);

          gdk_device_update_tool (GDK_DEVICE (device), NULL);

          if (device->tool_pen)
            gdk_seat_default_remove_tool (GDK_SEAT_DEFAULT (seat), device->tool_pen);

          if (device->tool_eraser)
            gdk_seat_default_remove_tool (GDK_SEAT_DEFAULT (seat), device->tool_eraser);

          _gdk_device_set_associated_device (GDK_DEVICE (device), NULL);
          _gdk_device_remove_physical_device (_gdk_device_manager->core_pointer, GDK_DEVICE (device));

          gdk_seat_default_remove_physical_device (GDK_SEAT_DEFAULT (seat), GDK_DEVICE (device));

          g_object_unref (device);
        }
      else
        {
          winpointer_device_update_scale_factors (device);
        }

      current = next;
    }

  /* create new gdk devices */
  for (i = 0; i < infos_count; i++)
    {
      if (!winpointer_find_system_device_in_device_manager (&infos[i]))
        {
          winpointer_create_devices (&infos[i]);
        }
    }

  g_free (infos);
}

static LRESULT CALLBACK
winpointer_notifications_window_procedure (HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_POINTERDEVICECHANGE:
      winpointer_enumerate_devices ();
      return 0;
    }

  return DefWindowProcW (hWnd, uMsg, wParam, lParam);
}

static gboolean
winpointer_notif_window_create (void)
{
  WNDCLASSEXW wndclassex;

  memset (&wndclassex, 0, sizeof (wndclassex));
  wndclassex.cbSize = sizeof (wndclassex);
  wndclassex.lpszClassName = L"GdkWin32WinpointerNotificationsWindowClass";
  wndclassex.lpfnWndProc = winpointer_notifications_window_procedure;
  wndclassex.hInstance = this_module ();

  if ((notifications_window_class = RegisterClassExW (&wndclassex)) == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      return FALSE;
    }

  if (!(notifications_window_handle = CreateWindowExW (0,
                                                       (LPCWSTR)(guintptr)notifications_window_class,
                                                       L"GdkWin32 Winpointer Notifications",
                                                       0,
                                                       0, 0, 0, 0,
                                                       HWND_MESSAGE,
                                                       NULL,
                                                       this_module (),
                                                       NULL)))
    {
      WIN32_API_FAILED ("CreateWindowExW");
      return FALSE;
    }

  return TRUE;
}

static gboolean
winpointer_ensure_procedures (void)
{
  static HMODULE user32_dll = NULL;

  if (!user32_dll)
    {
      user32_dll = LoadLibraryW (L"user32.dll");
      if (!user32_dll)
        {
          WIN32_API_FAILED ("LoadLibraryW");
          return FALSE;
        }

      registerPointerDeviceNotifications = (registerPointerDeviceNotifications_t)
        GetProcAddress (user32_dll, "RegisterPointerDeviceNotifications");
      getPointerDevices = (getPointerDevices_t)
        GetProcAddress (user32_dll, "GetPointerDevices");
      getPointerDeviceCursors = (getPointerDeviceCursors_t)
        GetProcAddress (user32_dll, "GetPointerDeviceCursors");
      getPointerDeviceRects = (getPointerDeviceRects_t)
        GetProcAddress (user32_dll, "GetPointerDeviceRects");
      getPointerType = (getPointerType_t)
        GetProcAddress (user32_dll, "GetPointerType");
      getPointerCursorId = (getPointerCursorId_t)
        GetProcAddress (user32_dll, "GetPointerCursorId");
      getPointerPenInfo = (getPointerPenInfo_t)
        GetProcAddress (user32_dll, "GetPointerPenInfo");
      getPointerTouchInfo = (getPointerTouchInfo_t)
        GetProcAddress (user32_dll, "GetPointerTouchInfo");
      getPointerPenInfoHistory = (getPointerPenInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerPenInfoHistory");
      getPointerTouchInfoHistory = (getPointerTouchInfoHistory_t)
        GetProcAddress (user32_dll, "GetPointerTouchInfoHistory");
      setGestureConfig = (setGestureConfig_t)
        GetProcAddress (user32_dll, "SetGestureConfig");
      setWindowFeedbackSetting = (setWindowFeedbackSetting_t)
        GetProcAddress (user32_dll, "SetWindowFeedbackSetting");
    }

  return registerPointerDeviceNotifications &&
         getPointerDevices &&
         getPointerDeviceCursors &&
         getPointerDeviceRects &&
         getPointerType &&
         getPointerCursorId &&
         getPointerPenInfo &&
         getPointerTouchInfo &&
         getPointerPenInfoHistory &&
         getPointerTouchInfoHistory &&
         setGestureConfig;
}

gboolean
gdk_winpointer_initialize (void)
{
  if (!winpointer_ensure_procedures ())
    return FALSE;

  if (!winpointer_notif_window_create ())
    return FALSE;

  if (!registerPointerDeviceNotifications (notifications_window_handle, FALSE))
    {
      WIN32_API_FAILED ("RegisterPointerDeviceNotifications");
      return FALSE;
    }

  ignored_interactions = g_ptr_array_new ();

  winpointer_enumerate_devices ();

  return TRUE;
}

#ifndef MICROSOFT_TABLETPENSERVICE_PROPERTY
#define MICROSOFT_TABLETPENSERVICE_PROPERTY \
_T("MicrosoftTabletPenServiceProperty")
#endif

void
gdk_winpointer_initialize_surface (GdkSurface *surface)
{
  HWND hwnd = GDK_SURFACE_HWND (surface);
  ATOM key = 0;
  HANDLE val = (HANDLE)(TABLET_DISABLE_PRESSANDHOLD |
                        TABLET_DISABLE_PENTAPFEEDBACK |
                        TABLET_DISABLE_PENBARRELFEEDBACK |
                        TABLET_DISABLE_FLICKS |
                        TABLET_DISABLE_FLICKFALLBACKKEYS);

  winpointer_ensure_procedures ();

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  API_CALL (SetPropW, (hwnd, (LPCWSTR)(guintptr)key, val));
  GlobalDeleteAtom (key);

  if (setGestureConfig != NULL)
    {
      GESTURECONFIG gesture_config;
      memset (&gesture_config, 0, sizeof (gesture_config));

      gesture_config.dwID = 0;
      gesture_config.dwWant = 0;
      gesture_config.dwBlock = GC_ALLGESTURES;

      API_CALL (setGestureConfig, (hwnd, 0, 1, &gesture_config, sizeof (gesture_config)));
    }

  if (setWindowFeedbackSetting != NULL)
    {
      FEEDBACK_TYPE feedbacks[] = {
        FEEDBACK_TOUCH_CONTACTVISUALIZATION,
        FEEDBACK_PEN_BARRELVISUALIZATION,
        FEEDBACK_PEN_TAP,
        FEEDBACK_PEN_DOUBLETAP,
        FEEDBACK_PEN_PRESSANDHOLD,
        FEEDBACK_PEN_RIGHTTAP,
        FEEDBACK_TOUCH_TAP,
        FEEDBACK_TOUCH_DOUBLETAP,
        FEEDBACK_TOUCH_PRESSANDHOLD,
        FEEDBACK_TOUCH_RIGHTTAP,
        FEEDBACK_GESTURE_PRESSANDTAP
      };
      gsize i = 0;

      for (i = 0; i < G_N_ELEMENTS (feedbacks); i++)
        {
          BOOL setting = FALSE;

          API_CALL (setWindowFeedbackSetting, (hwnd, feedbacks[i], 0, sizeof (BOOL), &setting));
        }
    }
}

void
gdk_winpointer_finalize_surface (GdkSurface *surface)
{
  HWND hwnd = GDK_SURFACE_HWND (surface);
  ATOM key = 0;

  key = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
  RemovePropW (hwnd, (LPCWSTR)(guintptr)key);
  GlobalDeleteAtom (key);
}
