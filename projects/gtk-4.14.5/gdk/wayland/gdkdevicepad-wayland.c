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

#include "gdkwaylanddevice.h"
#include "gdkdevice-wayland-private.h"

#include "tablet-unstable-v2-client-protocol.h"

#include <gdk/gdkdevicepadprivate.h>

typedef struct _GdkWaylandDevicePad GdkWaylandDevicePad;
typedef struct _GdkWaylandDevicePadClass GdkWaylandDevicePadClass;

struct _GdkWaylandDevicePad
{
  GdkWaylandDevice parent_instance;
};

struct _GdkWaylandDevicePadClass
{
  GdkWaylandDeviceClass parent_class;
};

static void gdk_wayland_device_pad_iface_init (GdkDevicePadInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandDevicePad, gdk_wayland_device_pad,
                         GDK_TYPE_WAYLAND_DEVICE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DEVICE_PAD,
                                                gdk_wayland_device_pad_iface_init))

static int
gdk_wayland_device_pad_get_n_groups (GdkDevicePad *pad)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat),
                                    GDK_DEVICE (pad));
#ifdef G_DISABLE_ASSERT
  if (data == NULL)
    return 0;
#else
  g_assert (data != NULL);
#endif

  return g_list_length (data->mode_groups);
}

static int
gdk_wayland_device_pad_get_group_n_modes (GdkDevicePad *pad,
                                          int           n_group)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadGroupData *group;
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat),
                                    GDK_DEVICE (pad));
#ifdef G_DISABLE_ASSERT
  if (data == NULL)
    return 0;
#else
  g_assert (data != NULL);
#endif

  group = g_list_nth_data (data->mode_groups, n_group);
  if (!group)
    return -1;

  return group->n_modes;
}

static int
gdk_wayland_device_pad_get_n_features (GdkDevicePad        *pad,
                                       GdkDevicePadFeature  feature)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat),
                                    GDK_DEVICE (pad));
  g_assert (data != NULL);

  switch (feature)
    {
    case GDK_DEVICE_PAD_FEATURE_BUTTON:
      return data->n_buttons;
    case GDK_DEVICE_PAD_FEATURE_RING:
      return g_list_length (data->rings);
    case GDK_DEVICE_PAD_FEATURE_STRIP:
      return g_list_length (data->strips);
    default:
      return -1;
    }
}

static int
gdk_wayland_device_pad_get_feature_group (GdkDevicePad        *pad,
                                          GdkDevicePadFeature  feature,
                                          int                  idx)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadGroupData *group;
  GdkWaylandTabletPadData *data;
  GList *l;
  int i;

  data = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat),
                                    GDK_DEVICE (pad));
#ifdef G_DISABLE_ASSERT
  if (data == NULL)
    return -1;
#else
  g_assert (data != NULL);
#endif

  for (l = data->mode_groups, i = 0; l; l = l->next, i++)
    {
      group = l->data;

      switch (feature)
        {
        case GDK_DEVICE_PAD_FEATURE_BUTTON:
          if (g_list_find (group->buttons, GINT_TO_POINTER (idx)))
            return i;
          break;
        case GDK_DEVICE_PAD_FEATURE_RING:
          {
            gpointer ring;

            ring = g_list_nth_data (data->rings, idx);
            if (ring && g_list_find (group->rings, ring))
              return i;
            break;
          }
        case GDK_DEVICE_PAD_FEATURE_STRIP:
          {
            gpointer strip;
            strip = g_list_nth_data (data->strips, idx);
            if (strip && g_list_find (group->strips, strip))
              return i;
            break;
          }
        default:
          break;
        }
    }

  return -1;
}

static void
gdk_wayland_device_pad_iface_init (GdkDevicePadInterface *iface)
{
  iface->get_n_groups = gdk_wayland_device_pad_get_n_groups;
  iface->get_group_n_modes = gdk_wayland_device_pad_get_group_n_modes;
  iface->get_n_features = gdk_wayland_device_pad_get_n_features;
  iface->get_feature_group = gdk_wayland_device_pad_get_feature_group;
}

static void
gdk_wayland_device_pad_class_init (GdkWaylandDevicePadClass *klass)
{
}

static void
gdk_wayland_device_pad_init (GdkWaylandDevicePad *pad)
{
}

static GdkWaylandTabletPadGroupData *
tablet_pad_lookup_button_group (GdkWaylandTabletPadData *pad,
                                uint32_t                 button)
{
  GdkWaylandTabletPadGroupData *group;
  GList *l;

  for (l = pad->mode_groups; l; l = l->next)
    {
      group = l->data;

      if (g_list_find (group->buttons, GUINT_TO_POINTER (button)))
        return group;
    }

  return NULL;
}

/*<private>
 * gdk_wayland_device_pad_set_feedback:
 * @device: (type GdkWaylandDevice): a %GDK_SOURCE_TABLET_PAD device
 * @feature: Feature to set the feedback label for
 * @feature_idx: 0-indexed index of the feature to set the feedback label for
 * @label: Feedback label
 *
 * Sets the feedback label for the given feature/index.
 *
 * This may be used by the compositor to provide user feedback
 * of the actions available/performed.
 */
void
gdk_wayland_device_pad_set_feedback (GdkDevice           *device,
                                     GdkDevicePadFeature  feature,
                                     guint                feature_idx,
                                     const char          *label)
{
  GdkWaylandTabletPadData *pad;
  GdkWaylandTabletPadGroupData *group;
  GdkSeat *seat;

  seat = gdk_device_get_seat (device);
  pad = gdk_wayland_seat_find_pad (GDK_WAYLAND_SEAT (seat), device);
  if (!pad)
    return;

  if (feature == GDK_DEVICE_PAD_FEATURE_BUTTON)
    {
      group = tablet_pad_lookup_button_group (pad, feature_idx);
      if (!group)
        return;

      zwp_tablet_pad_v2_set_feedback (pad->wp_tablet_pad, feature_idx, label,
                                      group->mode_switch_serial);
    }
  else if (feature == GDK_DEVICE_PAD_FEATURE_RING)
    {
      struct zwp_tablet_pad_ring_v2 *wp_pad_ring;

      wp_pad_ring = g_list_nth_data (pad->rings, feature_idx);
      if (!wp_pad_ring)
        return;

      group = zwp_tablet_pad_ring_v2_get_user_data (wp_pad_ring);
      zwp_tablet_pad_ring_v2_set_feedback (wp_pad_ring, label,
                                           group->mode_switch_serial);

    }
  else if (feature == GDK_DEVICE_PAD_FEATURE_STRIP)
    {
      struct zwp_tablet_pad_strip_v2 *wp_pad_strip;

      wp_pad_strip = g_list_nth_data (pad->strips, feature_idx);
      if (!wp_pad_strip)
        return;

      group = zwp_tablet_pad_strip_v2_get_user_data (wp_pad_strip);
      zwp_tablet_pad_strip_v2_set_feedback (wp_pad_strip, label,
                                            group->mode_switch_serial);
    }
}
