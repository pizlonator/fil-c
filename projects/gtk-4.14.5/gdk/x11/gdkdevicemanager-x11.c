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

#include "gdkx11devicemanager-xi2.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

/* Defines for VCP/VCK, to be used too
 * for the core protocol device manager
 */
#define VIRTUAL_CORE_POINTER_ID 2
#define VIRTUAL_CORE_KEYBOARD_ID 3

GdkX11DeviceManagerXI2 *
_gdk_x11_device_manager_new (GdkDisplay *display)
{
  int opcode, firstevent, firsterror;
  Display *xdisplay;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  if (XQueryExtension (xdisplay, "XInputExtension",
                       &opcode, &firstevent, &firsterror))
    {
      int major, minor;

      major = 2;
      minor = 4;

      if (XIQueryVersion (xdisplay, &major, &minor) != BadRequest)
        {
          GdkX11DeviceManagerXI2 *device_manager_xi2;

          GDK_DISPLAY_DEBUG (display, INPUT, "Creating XI2 (version %d.%d) device manager",
                                             major, minor);

          device_manager_xi2 = g_object_new (GDK_TYPE_X11_DEVICE_MANAGER_XI2,
                                             "display", display,
                                             "opcode", opcode,
                                             "major", major,
                                             "minor", minor,
                                             NULL);

          return device_manager_xi2;
        }
    }

  g_error ("XInput2 support not found on display");
}

/**
 * gdk_x11_device_manager_lookup:
 * @device_manager: (type GdkX11DeviceManagerXI2): a `GdkDeviceManager`
 * @device_id: a device ID, as understood by the XInput2 protocol
 *
 * Returns the `GdkDevice` that wraps the given device ID.
 *
 * Returns: (transfer none) (nullable) (type GdkX11DeviceXI2): The
 *   `GdkDevice` wrapping the device ID, or %NULL if the given ID
 *   doesn’t currently represent a device.
 */
GdkDevice *
gdk_x11_device_manager_lookup (GdkX11DeviceManagerXI2 *device_manager,
			       int                     device_id)
{
  g_return_val_if_fail (GDK_IS_X11_DEVICE_MANAGER_XI2 (device_manager), NULL);

  return _gdk_x11_device_manager_xi2_lookup (GDK_X11_DEVICE_MANAGER_XI2 (device_manager),
                                             device_id);
}

/**
 * gdk_x11_device_get_id:
 * @device: (type GdkX11DeviceXI2): a `GdkDevice`
 *
 * Returns the device ID as seen by XInput2.
 *
 * Returns: the XInput2 device ID
 */
int
gdk_x11_device_get_id (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return _gdk_x11_device_xi2_get_id (GDK_X11_DEVICE_XI2 (device));
}
