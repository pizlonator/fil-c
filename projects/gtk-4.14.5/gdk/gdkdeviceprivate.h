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

#pragma once

#include "gdkdevice.h"
#include "gdkdevicetool.h"
#include "gdkevents.h"
#include "gdkseat.h"

G_BEGIN_DECLS

typedef enum
{
  GDK_GRAB_SUCCESS         = 0,
  GDK_GRAB_ALREADY_GRABBED = 1,
  GDK_GRAB_INVALID_TIME    = 2,
  GDK_GRAB_NOT_VIEWABLE    = 3,
  GDK_GRAB_FROZEN          = 4,
  GDK_GRAB_FAILED          = 5
} GdkGrabStatus;

typedef enum
{
  GDK_EXPOSURE_MASK             = 1 << 1,
  GDK_POINTER_MOTION_MASK       = 1 << 2,
  GDK_BUTTON_MOTION_MASK        = 1 << 4,
  GDK_BUTTON1_MOTION_MASK       = 1 << 5,
  GDK_BUTTON2_MOTION_MASK       = 1 << 6,
  GDK_BUTTON3_MOTION_MASK       = 1 << 7,
  GDK_BUTTON_PRESS_MASK         = 1 << 8,
  GDK_BUTTON_RELEASE_MASK       = 1 << 9,
  GDK_KEY_PRESS_MASK            = 1 << 10,
  GDK_KEY_RELEASE_MASK          = 1 << 11,
  GDK_ENTER_NOTIFY_MASK         = 1 << 12,
  GDK_LEAVE_NOTIFY_MASK         = 1 << 13,
  GDK_FOCUS_CHANGE_MASK         = 1 << 14,
  GDK_STRUCTURE_MASK            = 1 << 15,
  GDK_PROPERTY_CHANGE_MASK      = 1 << 16,
  GDK_PROXIMITY_IN_MASK         = 1 << 18,
  GDK_PROXIMITY_OUT_MASK        = 1 << 19,
  GDK_SCROLL_MASK               = 1 << 20,
  GDK_TOUCH_MASK                = 1 << 21,
  GDK_SMOOTH_SCROLL_MASK        = 1 << 22,
  GDK_TOUCHPAD_GESTURE_MASK     = 1 << 23,
  GDK_TABLET_PAD_MASK           = 1 << 24,
  GDK_ALL_EVENTS_MASK           = 0x3FFFFFE
} GdkEventMask;

#define GDK_DEVICE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE, GdkDeviceClass))
#define GDK_IS_DEVICE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE))
#define GDK_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE, GdkDeviceClass))

typedef struct _GdkDeviceClass GdkDeviceClass;

struct _GdkDevice
{
  GObject parent_instance;

  char *name;
  GdkInputSource source;
  gboolean has_cursor;
  GdkDisplay *display;
  /* The paired logical device for logical devices,
   * or the associated logical device for physical ones
   */
  GdkDevice *associated;
  GList *physical_devices;
  GArray *axes;
  guint num_touches;

  char *vendor_id;
  char *product_id;

  GdkSeat *seat;
  GdkDeviceTool *last_tool;

  guint32 timestamp;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;

  void (* set_surface_cursor)(GdkDevice *device,
                              GdkSurface *surface,
                              GdkCursor *cursor);

  GdkGrabStatus (* grab)     (GdkDevice        *device,
                              GdkSurface        *surface,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              GdkSurface        *confine_to,
                              GdkCursor        *cursor,
                              guint32           time_);
  void          (*ungrab)    (GdkDevice        *device,
                              guint32           time_);

  GdkSurface * (* surface_at_position) (GdkDevice       *device,
                                        double          *win_x,
                                        double          *win_y,
                                        GdkModifierType *mask);
};

void  _gdk_device_set_associated_device (GdkDevice *device,
                                         GdkDevice *relative);

void  _gdk_device_reset_axes (GdkDevice   *device);
guint _gdk_device_add_axis   (GdkDevice   *device,
                              GdkAxisUse   use,
                              double       min_value,
                              double       max_value,
                              double       resolution);
void _gdk_device_get_axis_info (GdkDevice  *device,
                                guint       index,
                                GdkAxisUse *use,
                                double     *min_value,
                                double     *max_value,
                                double     *resolution);

gboolean   _gdk_device_translate_surface_coord (GdkDevice *device,
                                                GdkSurface *surface,
                                                guint      index,
                                                double     value,
                                                double    *axis_value);

gboolean   _gdk_device_translate_screen_coord (GdkDevice *device,
                                               GdkSurface *surface,
                                               double     surface_root_x,
                                               double     surface_root_y,
                                               double     screen_width,
                                               double     screen_height,
                                               guint      index,
                                               double     value,
                                               double    *axis_value);

gboolean   _gdk_device_translate_axis         (GdkDevice *device,
                                               guint      index,
                                               double     value,
                                               double    *axis_value);

GdkTimeCoord ** _gdk_device_allocate_history  (GdkDevice *device,
                                               int        n_events);

GList * gdk_device_list_physical_devices        (GdkDevice *device);

void    _gdk_device_add_physical_device         (GdkDevice *device,
                                                 GdkDevice *physical);
void    _gdk_device_remove_physical_device      (GdkDevice *device,
                                                 GdkDevice *physical);

GdkSurface * _gdk_device_surface_at_position  (GdkDevice        *device,
                                               double           *win_x,
                                               double           *win_y,
                                               GdkModifierType  *mask);

void  gdk_device_set_seat  (GdkDevice *device,
                            GdkSeat   *seat);

void           gdk_device_update_tool (GdkDevice     *device,
                                       GdkDeviceTool *tool);

GdkGrabStatus gdk_device_grab (GdkDevice        *device,
                               GdkSurface        *surface,
                               gboolean          owner_events,
                               GdkEventMask      event_mask,
                               GdkCursor        *cursor,
                               guint32           time_);
void gdk_device_ungrab        (GdkDevice        *device,
                               guint32           time_);
int gdk_device_get_n_axes     (GdkDevice       *device);
gboolean gdk_device_get_axis  (GdkDevice         *device,
                               double            *axes,
                               GdkAxisUse         use,
                               double            *value);
GdkAxisUse gdk_device_get_axis_use  (GdkDevice         *device,
                                     guint              index_);

void gdk_device_set_timestamp (GdkDevice *device,
                               guint32    timestamp);

gboolean gdk_device_grab_info (GdkDisplay  *display,
                               GdkDevice   *device,
                               GdkSurface  **grab_surface,
                               gboolean    *owner_events);

G_END_DECLS

