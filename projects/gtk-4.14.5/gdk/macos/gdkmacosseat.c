/*
 * Copyright © 2020 Red Hat, Inc.
 * Copyright © 2021 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkdeviceprivate.h"
#include "gdkdevicetoolprivate.h"

#include "gdkmacosdevice.h"
#include "gdkmacosseat-private.h"

#include "gdkprivate.h"

typedef struct
{
  NSUInteger device_id;
  char *name;

  GdkDevice *logical_device;
  GdkDevice *stylus_device;
  GdkSeat *seat;

  GdkDeviceTool *current_tool;

  int axis_indices[GDK_AXIS_LAST];
  double axes[GDK_AXIS_LAST];
} GdkMacosTabletData;

struct _GdkMacosSeat
{
  GdkSeat parent_instance;

  GdkMacosDisplay *display;

  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;

  GdkMacosTabletData *current_tablet;
  GPtrArray *tablets;
  GPtrArray *tools;
};

struct _GdkMacosSeatClass
{
  GdkSeatClass parent_class;
};

G_DEFINE_TYPE (GdkMacosSeat, gdk_macos_seat, GDK_TYPE_SEAT)

#define KEYBOARD_EVENTS (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |    \
                         GDK_FOCUS_CHANGE_MASK)
#define TOUCH_EVENTS    (GDK_TOUCH_MASK)
#define POINTER_EVENTS  (GDK_POINTER_MOTION_MASK |                      \
                         GDK_BUTTON_PRESS_MASK |                        \
                         GDK_BUTTON_RELEASE_MASK |                      \
                         GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |     \
                         GDK_ENTER_NOTIFY_MASK |                        \
                         GDK_LEAVE_NOTIFY_MASK |                        \
                         GDK_PROXIMITY_IN_MASK |                        \
                         GDK_PROXIMITY_OUT_MASK)

static void
gdk_macos_tablet_data_free (gpointer user_data)
{
  GdkMacosTabletData *tablet = user_data;

  gdk_seat_device_removed (GDK_SEAT (tablet->seat), tablet->stylus_device);
  gdk_seat_device_removed (GDK_SEAT (tablet->seat), tablet->logical_device);

  _gdk_device_set_associated_device (tablet->logical_device, NULL);
  _gdk_device_set_associated_device (tablet->stylus_device, NULL);

  g_object_unref (tablet->logical_device);
  g_object_unref (tablet->stylus_device);

  g_free (tablet->name);
  g_free (tablet);
}

static void
gdk_macos_seat_dispose (GObject *object)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (object);

  if (self->logical_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (self), self->logical_pointer);
      g_clear_object (&self->logical_pointer);
    }

  if (self->logical_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (self), self->logical_keyboard);
      g_clear_object (&self->logical_pointer);
    }

  g_clear_pointer (&self->tablets, g_ptr_array_unref);
  g_clear_pointer (&self->tools, g_ptr_array_unref);

  G_OBJECT_CLASS (gdk_macos_seat_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_macos_seat_get_capabilities (GdkSeat *seat)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);
  GdkSeatCapabilities caps = 0;

  if (self->logical_pointer)
    caps |= GDK_SEAT_CAPABILITY_POINTER;
  if (self->logical_keyboard)
    caps |= GDK_SEAT_CAPABILITY_KEYBOARD;

  return caps;
}

static GdkGrabStatus
gdk_macos_seat_grab (GdkSeat                *seat,
                     GdkSurface             *surface,
                     GdkSeatCapabilities     capabilities,
                     gboolean                owner_events,
                     GdkCursor              *cursor,
                     GdkEvent               *event,
                     GdkSeatGrabPrepareFunc  prepare_func,
                     gpointer                prepare_func_data)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  gboolean was_visible;

  was_visible = gdk_surface_get_mapped (surface);

  if (prepare_func)
    (prepare_func) (seat, surface, prepare_func_data);

  if (!gdk_surface_get_mapped (surface))
    {
      g_critical ("Surface %p has not been mapped in GdkSeatGrabPrepareFunc",
                  surface);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (capabilities & GDK_SEAT_CAPABILITY_ALL_POINTING)
    {
      /* ALL_POINTING spans 3 capabilities; get the mask for the ones we have */
      GdkEventMask pointer_evmask = 0;

      /* We let tablet styli take over the pointer cursor */
      if (capabilities & (GDK_SEAT_CAPABILITY_POINTER |
                          GDK_SEAT_CAPABILITY_TABLET_STYLUS))
        {
          pointer_evmask |= POINTER_EVENTS;
        }

      if (capabilities & GDK_SEAT_CAPABILITY_TOUCH)
        pointer_evmask |= TOUCH_EVENTS;

      status = gdk_device_grab (self->logical_pointer, surface,
                                owner_events,
                                pointer_evmask, cursor,
                                evtime);
    }

  if (status == GDK_GRAB_SUCCESS &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      status = gdk_device_grab (self->logical_keyboard, surface,
                                owner_events,
                                KEYBOARD_EVENTS, cursor,
                                evtime);

      if (status != GDK_GRAB_SUCCESS)
        {
          if (capabilities & ~GDK_SEAT_CAPABILITY_KEYBOARD)
            gdk_device_ungrab (self->logical_pointer, evtime);
        }
    }

  if (status != GDK_GRAB_SUCCESS && !was_visible)
    gdk_surface_hide (surface);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return status;
}

static void
gdk_macos_seat_ungrab (GdkSeat *seat)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gdk_device_ungrab (self->logical_pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (self->logical_keyboard, GDK_CURRENT_TIME);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GdkDevice *
gdk_macos_seat_get_logical_device (GdkSeat             *seat,
                                   GdkSeatCapabilities  capability)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);

  /* There must be only one flag set */
  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return self->logical_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return self->logical_keyboard;
    default:
      g_warning ("Unhandled capability %x", capability);
      break;
    }

  return NULL;
}

static GList *
gdk_macos_seat_get_devices (GdkSeat             *seat,
                            GdkSeatCapabilities  capabilities)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);
  GList *physical_devices = NULL;

  if (self->logical_pointer && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    physical_devices = g_list_prepend (physical_devices, self->logical_pointer);

  if (self->logical_keyboard && (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD))
    physical_devices = g_list_prepend (physical_devices, self->logical_keyboard);

  if (capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS)
    {
      for (guint i = 0; i < self->tablets->len; i++)
        {
          GdkMacosTabletData *tablet = g_ptr_array_index (self->tablets, i);

          physical_devices = g_list_prepend (physical_devices, tablet->stylus_device);
        }
    }

  return physical_devices;
}

static GList *
gdk_macos_seat_get_tools (GdkSeat *seat)
{
  GdkMacosSeat *self = GDK_MACOS_SEAT (seat);
  GdkDeviceTool *tool;
  GList *tools = NULL;

  for (guint i = 0; i < self->tools->len; i++)
    {
      tool = g_ptr_array_index (self->tools, i);
      tools = g_list_prepend (tools, tool);
    }

  return tools;
}

static void
gdk_macos_seat_class_init (GdkMacosSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_macos_seat_dispose;

  seat_class->get_capabilities = gdk_macos_seat_get_capabilities;
  seat_class->grab = gdk_macos_seat_grab;
  seat_class->ungrab = gdk_macos_seat_ungrab;
  seat_class->get_logical_device = gdk_macos_seat_get_logical_device;
  seat_class->get_devices = gdk_macos_seat_get_devices;
  seat_class->get_tools = gdk_macos_seat_get_tools;
}

static void
gdk_macos_seat_init (GdkMacosSeat *self)
{
  self->tablets = g_ptr_array_new_with_free_func (gdk_macos_tablet_data_free);
  self->tools = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
init_devices (GdkMacosSeat *self)
{
  /* pointer */
  self->logical_pointer = g_object_new (GDK_TYPE_MACOS_DEVICE,
                                        "name", "Core Pointer",
                                        "source", GDK_SOURCE_MOUSE,
                                        "has-cursor", TRUE,
                                        "display", self->display,
                                        "seat", self,
                                        NULL);

  /* keyboard */
  self->logical_keyboard = g_object_new (GDK_TYPE_MACOS_DEVICE,
                                         "name", "Core Keyboard",
                                         "source", GDK_SOURCE_KEYBOARD,
                                         "has-cursor", FALSE,
                                         "display", self->display,
                                         "seat", self,
                                         NULL);

  /* link both */
  _gdk_device_set_associated_device (self->logical_pointer, self->logical_keyboard);
  _gdk_device_set_associated_device (self->logical_keyboard, self->logical_pointer);

  gdk_seat_device_added (GDK_SEAT (self), self->logical_pointer);
  gdk_seat_device_added (GDK_SEAT (self), self->logical_keyboard);
}

GdkSeat *
_gdk_macos_seat_new (GdkMacosDisplay *display)
{
  GdkMacosSeat *self;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_MACOS_SEAT,
                       "display", display,
                       NULL);

  self->display = display;

  init_devices (self);

  return GDK_SEAT (g_steal_pointer (&self));
}

static GdkDeviceToolType
get_device_tool_type_from_nsevent (NSEvent *nsevent)
{
  GdkDeviceToolType tool_type;

  switch ([nsevent pointingDeviceType])
    {
    case NSPointingDeviceTypePen:
      tool_type = GDK_DEVICE_TOOL_TYPE_PEN;
      break;
    case NSPointingDeviceTypeEraser:
      tool_type = GDK_DEVICE_TOOL_TYPE_ERASER;
      break;
    case NSPointingDeviceTypeCursor:
      tool_type = GDK_DEVICE_TOOL_TYPE_MOUSE;
      break;
    case NSPointingDeviceTypeUnknown:
    default:
      tool_type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;
    }

  return tool_type;
}

static GdkAxisFlags
get_device_tool_axes_from_nsevent (NSEvent *nsevent)
{
  /* TODO: do we need to be smarter about the capabilities? */
  return GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT | GDK_AXIS_FLAG_PRESSURE |
         GDK_AXIS_FLAG_ROTATION;
}

static GdkMacosTabletData *
create_tablet_data_from_nsevent (GdkMacosSeat *self,
                                 NSEvent      *nsevent)
{
  GdkMacosTabletData *tablet;
  GdkDisplay *display = gdk_seat_get_display (GDK_SEAT (self));
  GdkDevice *logical_device, *stylus_device;
  char *logical_name;
  char *vid, *pid;

  tablet = g_new0 (GdkMacosTabletData, 1);
  tablet->seat = GDK_SEAT (self);
  tablet->device_id = [nsevent deviceID];
  /* FIXME: find a better name */
  tablet->name = g_strdup_printf ("Tablet %lu", [nsevent deviceID]);

  vid = g_strdup_printf ("%.4lx", [nsevent vendorID]);
  pid = g_strdup_printf ("%.4lx", [nsevent tabletID]);

  logical_name = g_strdup_printf ("Logical pointer for %s", tablet->name);
  logical_device = g_object_new (GDK_TYPE_MACOS_DEVICE,
                                 "name", logical_name,
                                 "source", GDK_SOURCE_MOUSE,
                                 "has-cursor", TRUE,
                                 "display", display,
                                 "seat", self,
                                 NULL);

  stylus_device = g_object_new (GDK_TYPE_MACOS_DEVICE,
                                "name", tablet->name,
                                "source", GDK_SOURCE_PEN,
                                "has-cursor", FALSE,
                                "display", display,
                                "seat", self,
                                "vendor-id", vid,
                                "product-id", pid,
                                NULL);

  tablet->logical_device = logical_device;
  tablet->stylus_device = stylus_device;

  _gdk_device_set_associated_device (logical_device, self->logical_keyboard);
  _gdk_device_set_associated_device (stylus_device, logical_device);

  gdk_seat_device_added (GDK_SEAT (self), logical_device);
  gdk_seat_device_added (GDK_SEAT (self), stylus_device);

  g_free (logical_name);
  g_free (vid);
  g_free (pid);

  return tablet;
}

static GdkMacosTabletData *
get_tablet_data_from_nsevent (GdkMacosSeat *self,
                              NSEvent      *nsevent)
{
  GdkMacosTabletData *tablet = NULL;

  for (guint i = 0; i < self->tablets->len; i++)
    {
      GdkMacosTabletData *t = g_ptr_array_index (self->tablets, i);

      if (t->device_id == [nsevent deviceID])
        {
          tablet = t;
          break;
        }
    }

  if (!tablet)
    tablet = create_tablet_data_from_nsevent (self, nsevent);

  return tablet;
}

static void
device_tablet_clone_tool_axes (GdkMacosTabletData *tablet,
                               GdkDeviceTool      *tool)
{
  int axis_pos;

  g_object_freeze_notify (G_OBJECT (tablet->stylus_device));
  _gdk_device_reset_axes (tablet->stylus_device);

  _gdk_device_add_axis (tablet->stylus_device, GDK_AXIS_X, 0, 0, 0);
  _gdk_device_add_axis (tablet->stylus_device, GDK_AXIS_Y, 0, 0, 0);

  if (tool->tool_axes & (GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT))
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_XTILT, -1.0, 1.0, 0);
      tablet->axis_indices[GDK_AXIS_XTILT] = axis_pos;

      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_YTILT, -1.0, 1.0, 0);
      tablet->axis_indices[GDK_AXIS_YTILT] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_PRESSURE)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_PRESSURE, 0.0, 1.0, 0);
      tablet->axis_indices[GDK_AXIS_PRESSURE] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_ROTATION)
    {
      axis_pos = _gdk_device_add_axis (tablet->stylus_device,
                                       GDK_AXIS_ROTATION, 0.0, 1.0, 0);
      tablet->axis_indices[GDK_AXIS_ROTATION] = axis_pos;
    }

  g_object_thaw_notify (G_OBJECT (tablet->stylus_device));
}

static void
mimic_device_axes (GdkDevice *logical,
                   GdkDevice *physical)
{
  double axis_min, axis_max, axis_resolution;
  GdkAxisUse axis_use;
  int axis_count;

  g_object_freeze_notify (G_OBJECT (logical));
  _gdk_device_reset_axes (logical);
  axis_count = gdk_device_get_n_axes (physical);

  for (int i = 0; i < axis_count; i++)
    {
      _gdk_device_get_axis_info (physical, i, &axis_use, &axis_min,
                                 &axis_max, &axis_resolution);
      _gdk_device_add_axis (logical, axis_use, axis_min,
                            axis_max, axis_resolution);
    }

  g_object_thaw_notify (G_OBJECT (logical));
}

void
_gdk_macos_seat_handle_tablet_tool_event (GdkMacosSeat *seat,
                                          NSEvent      *nsevent)
{
  GdkDeviceToolType tool_type;
  GdkMacosTabletData *tablet;
  GdkDeviceTool *tool;

  g_return_if_fail (GDK_IS_MACOS_SEAT (seat));
  g_return_if_fail (nsevent != NULL);

  tablet = get_tablet_data_from_nsevent (seat, nsevent);

  tool_type = get_device_tool_type_from_nsevent (nsevent);

  if (tool_type == GDK_DEVICE_TOOL_TYPE_UNKNOWN)
    {
      g_warning ("Unknown device tool detected");
      return;
    }

  tool = gdk_seat_get_tool (GDK_SEAT (seat), [nsevent tabletID], [nsevent deviceID], tool_type);

  if ([nsevent isEnteringProximity])
    {
      if (!tool)
        {
          tool = gdk_device_tool_new ([nsevent tabletID], [nsevent vendorID], tool_type,
                                      get_device_tool_axes_from_nsevent (nsevent));
          g_ptr_array_add (seat->tools, tool);
        }

      gdk_device_update_tool (tablet->stylus_device, tool);
      tablet->current_tool = tool;
      device_tablet_clone_tool_axes (tablet, tool);
      mimic_device_axes (tablet->logical_device, tablet->stylus_device);
      seat->current_tablet = tablet;
    }
  else
    {
      gdk_device_update_tool (tablet->stylus_device, NULL);
      tablet->current_tool = NULL;
      seat->current_tablet = NULL;
    }
}

gboolean
_gdk_macos_seat_get_tablet (GdkMacosSeat   *seat,
                            GdkDevice     **logical_device,
                            GdkDeviceTool **tool)
{
  g_return_val_if_fail (GDK_IS_MACOS_SEAT (seat), FALSE);

  if (!seat->current_tablet)
    return FALSE;

  *logical_device = seat->current_tablet->logical_device;
  *tool = seat->current_tablet->current_tool;

  return TRUE;
}

double *
_gdk_macos_seat_get_tablet_axes_from_nsevent (GdkMacosSeat *seat,
                                              NSEvent      *nsevent)
{
  GdkMacosTabletData *tablet;
  int axis_index;

  g_return_val_if_fail (GDK_IS_MACOS_SEAT (seat), NULL);
  g_return_val_if_fail (nsevent != NULL, NULL);

  tablet = seat->current_tablet;
  if (!tablet || !tablet->current_tool)
    return NULL;

  if (tablet->current_tool->tool_axes & (GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT))
    {
      axis_index = tablet->axis_indices[GDK_AXIS_XTILT];
      _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                                  [nsevent tilt].x, &tablet->axes[GDK_AXIS_XTILT]);

      axis_index = tablet->axis_indices[GDK_AXIS_YTILT];
      _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                                  [nsevent tilt].y, &tablet->axes[GDK_AXIS_YTILT]);
    }

  if (tablet->current_tool->tool_axes & GDK_AXIS_FLAG_PRESSURE)
    {
      axis_index = tablet->axis_indices[GDK_AXIS_PRESSURE];
      _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                                  [nsevent pressure], &tablet->axes[GDK_AXIS_PRESSURE]);
    }

  if (tablet->current_tool->tool_axes & GDK_AXIS_FLAG_ROTATION)
    {
      axis_index = tablet->axis_indices[GDK_AXIS_ROTATION];
      _gdk_device_translate_axis (tablet->stylus_device, axis_index,
                                  [nsevent rotation], &tablet->axes[GDK_AXIS_ROTATION]);
    }

  return g_memdup2 (tablet->axes, sizeof (double) * GDK_AXIS_LAST);
}
