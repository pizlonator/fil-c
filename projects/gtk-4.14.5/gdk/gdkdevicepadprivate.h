/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2016 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include "gdkdevicepad.h"

G_BEGIN_DECLS

#define GDK_DEVICE_PAD_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDK_TYPE_DEVICE_PAD, GdkDevicePadInterface))

struct _GdkDevicePadInterface {
  GTypeInterface parent_interface;

  int (* get_n_groups)       (GdkDevicePad        *pad);

  int (* get_group_n_modes)  (GdkDevicePad        *pad,
                              int                  group);
  int (* get_n_features)     (GdkDevicePad        *pad,
                              GdkDevicePadFeature  feature);
  int (* get_feature_group)  (GdkDevicePad        *pad,
                              GdkDevicePadFeature  feature,
                              int                  idx);
};

G_END_DECLS

