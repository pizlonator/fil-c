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

/**
 * GdkDevicePad:
 *
 * `GdkDevicePad` is an interface implemented by devices of type
 * %GDK_SOURCE_TABLET_PAD
 *
 * It allows querying the features provided by the pad device.
 *
 * Tablet pads may contain one or more groups, each containing a subset
 * of the buttons/rings/strips available. [method@Gdk.DevicePad.get_n_groups]
 * can be used to obtain the number of groups, [method@Gdk.DevicePad.get_n_features]
 * and [method@Gdk.DevicePad.get_feature_group] can be combined to find out
 * the number of buttons/rings/strips the device has, and how are they grouped.
 *
 * Each of those groups have different modes, which may be used to map each
 * individual pad feature to multiple actions. Only one mode is effective
 * (current) for each given group, different groups may have different
 * current modes. The number of available modes in a group can be found
 * out through [method@Gdk.DevicePad.get_group_n_modes], and the current mode
 * for a given group will be notified through events of type `GDK_PAD_GROUP_MODE`.
 */

#include "config.h"

#include "gdkdevicepad.h"
#include "gdkdevicepadprivate.h"
#include "gdkdeviceprivate.h"

G_DEFINE_INTERFACE (GdkDevicePad, gdk_device_pad, GDK_TYPE_DEVICE)

static void
gdk_device_pad_default_init (GdkDevicePadInterface *pad)
{
}

/**
 * gdk_device_pad_get_n_groups:
 * @pad: a `GdkDevicePad`
 *
 * Returns the number of groups this pad device has.
 *
 * Pads have at least one group. A pad group is a subcollection of
 * buttons/strip/rings that is affected collectively by a same
 * current mode.
 *
 * Returns: The number of button/ring/strip groups in the pad.
 */
int
gdk_device_pad_get_n_groups (GdkDevicePad *pad)
{
  GdkDevicePadInterface *iface = GDK_DEVICE_PAD_GET_IFACE (pad);

  g_return_val_if_fail (GDK_IS_DEVICE_PAD (pad), 0);

  return iface->get_n_groups (pad);
}

/**
 * gdk_device_pad_get_group_n_modes:
 * @pad: a `GdkDevicePad`
 * @group_idx: group to get the number of available modes from
 *
 * Returns the number of modes that @group may have.
 *
 * Returns: The number of modes available in @group.
 */
int
gdk_device_pad_get_group_n_modes (GdkDevicePad *pad,
                                  int           group_idx)
{
  GdkDevicePadInterface *iface = GDK_DEVICE_PAD_GET_IFACE (pad);

  g_return_val_if_fail (GDK_IS_DEVICE_PAD (pad), 0);
  g_return_val_if_fail (group_idx >= 0, 0);

  return iface->get_group_n_modes (pad, group_idx);
}

/**
 * gdk_device_pad_get_n_features:
 * @pad: a `GdkDevicePad`
 * @feature: a pad feature
 *
 * Returns the number of features a tablet pad has.
 *
 * Returns: The amount of elements of type @feature that this pad has.
 */
int
gdk_device_pad_get_n_features (GdkDevicePad        *pad,
                               GdkDevicePadFeature  feature)
{
  GdkDevicePadInterface *iface = GDK_DEVICE_PAD_GET_IFACE (pad);

  g_return_val_if_fail (GDK_IS_DEVICE_PAD (pad), 0);

  return iface->get_n_features (pad, feature);
}

/**
 * gdk_device_pad_get_feature_group:
 * @pad: a `GdkDevicePad`
 * @feature: the feature type to get the group from
 * @feature_idx: the index of the feature to get the group from
 *
 * Returns the group the given @feature and @idx belong to.
 *
 * f the feature or index do not exist in @pad, -1 is returned.
 *
 * Returns: The group number of the queried pad feature.
 */
int
gdk_device_pad_get_feature_group (GdkDevicePad        *pad,
                                  GdkDevicePadFeature  feature,
                                  int                  idx)
{
  GdkDevicePadInterface *iface = GDK_DEVICE_PAD_GET_IFACE (pad);

  g_return_val_if_fail (GDK_IS_DEVICE_PAD (pad), -1);
  g_return_val_if_fail (idx >= 0, -1);

  return iface->get_feature_group (pad, feature, idx);
}
