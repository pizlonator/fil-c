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

#include <math.h>

#include "gdkdeviceprivate.h"
#include "gdkdevicetool.h"
#include "gdkdisplayprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkkeysprivate.h"

/**
 * GdkDevice:
 *
 * The `GdkDevice` object represents an input device, such
 * as a keyboard, a mouse, or a touchpad.
 *
 * See the [class@Gdk.Seat] documentation for more information
 * about the various kinds of devices, and their relationships.
 */

typedef struct _GdkAxisInfo GdkAxisInfo;

struct _GdkAxisInfo
{
  GdkAxisUse use;
  double min_axis;
  double max_axis;
  double min_value;
  double max_value;
  double resolution;
};

enum {
  CHANGED,
  TOOL_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


static void gdk_device_finalize     (GObject      *object);
static void gdk_device_dispose      (GObject      *object);
static void gdk_device_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void gdk_device_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE (GdkDevice, gdk_device, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_NAME,
  PROP_SOURCE,
  PROP_HAS_CURSOR,
  PROP_N_AXES,
  PROP_VENDOR_ID,
  PROP_PRODUCT_ID,
  PROP_SEAT,
  PROP_NUM_TOUCHES,
  PROP_TOOL,
  PROP_DIRECTION,
  PROP_HAS_BIDI_LAYOUTS,
  PROP_CAPS_LOCK_STATE,
  PROP_NUM_LOCK_STATE,
  PROP_SCROLL_LOCK_STATE,
  PROP_MODIFIER_STATE,
  LAST_PROP
};

static GParamSpec *device_props[LAST_PROP] = { NULL, };

static void
gdk_device_class_init (GdkDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_finalize;
  object_class->dispose = gdk_device_dispose;
  object_class->set_property = gdk_device_set_property;
  object_class->get_property = gdk_device_get_property;

  /**
   * GdkDevice:display: (attributes org.gtk.Property.get=gdk_device_get_display)
   *
   * The `GdkDisplay` the `GdkDevice` pertains to.
   */
  device_props[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:name: (attributes org.gtk.Property.get=gdk_device_get_name)
   *
   * The device name.
   */
  device_props[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:source: (attributes org.gtk.Property.get=gdk_device_get_source)
   *
   * Source type for the device.
   */
  device_props[PROP_SOURCE] =
      g_param_spec_enum ("source", NULL, NULL,
                         GDK_TYPE_INPUT_SOURCE,
                         GDK_SOURCE_MOUSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDevice:has-cursor: (attributes org.gtk.Property.get=gdk_device_get_has_cursor)
   *
   * Whether the device is represented by a cursor on the screen.
   */
  device_props[PROP_HAS_CURSOR] =
      g_param_spec_boolean ("has-cursor", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:n-axes:
   *
   * Number of axes in the device.
   */
  device_props[PROP_N_AXES] =
      g_param_spec_uint ("n-axes", NULL, NULL,
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:vendor-id: (attributes org.gtk.Property.get=gdk_device_get_vendor_id)
   *
   * Vendor ID of this device.
   *
   * See [method@Gdk.Device.get_vendor_id].
   */
  device_props[PROP_VENDOR_ID] =
      g_param_spec_string ("vendor-id", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:product-id: (attributes org.gtk.Property.get=gdk_device_get_product_id)
   *
   * Product ID of this device.
   *
   * See [method@Gdk.Device.get_product_id].
   */
  device_props[PROP_PRODUCT_ID] =
      g_param_spec_string ("product-id", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:seat: (attributes org.gtk.Property.get=gdk_device_get_seat)
   *
   * `GdkSeat` of this device.
   */
  device_props[PROP_SEAT] =
      g_param_spec_object ("seat", NULL, NULL,
                           GDK_TYPE_SEAT,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:num-touches: (attributes org.gtk.Property.get=gdk_device_get_num_touches)
   *
   * The maximal number of concurrent touches on a touch device.
   *
   * Will be 0 if the device is not a touch device or if the number
   * of touches is unknown.
   */
  device_props[PROP_NUM_TOUCHES] =
      g_param_spec_uint ("num-touches", NULL, NULL,
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:tool: (attributes org.gtk.Property.get=gdk_device_get_device_tool)
   *
   * The `GdkDeviceTool` that is currently used with this device.
   */
  device_props[PROP_TOOL] =
    g_param_spec_object ("tool", NULL, NULL,
                         GDK_TYPE_DEVICE_TOOL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:direction: (attributes org.gtk.Property.get=gdk_device_get_direction)
   *
   * The direction of the current layout.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_DIRECTION] =
      g_param_spec_enum ("direction", NULL, NULL,
                         PANGO_TYPE_DIRECTION, PANGO_DIRECTION_NEUTRAL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:has-bidi-layouts: (attributes org.gtk.Property.get=gdk_device_has_bidi_layouts)
   *
   * Whether the device has both right-to-left and left-to-right layouts.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_HAS_BIDI_LAYOUTS] =
      g_param_spec_boolean ("has-bidi-layouts", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:caps-lock-state: (attributes org.gtk.Property.get=gdk_device_get_caps_lock_state)
   *
   * Whether Caps Lock is on.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_CAPS_LOCK_STATE] =
      g_param_spec_boolean ("caps-lock-state", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:num-lock-state: (attributes org.gtk.Property.get=gdk_device_get_num_lock_state)
   *
   * Whether Num Lock is on.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_NUM_LOCK_STATE] =
      g_param_spec_boolean ("num-lock-state", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:scroll-lock-state: (attributes org.gtk.Property.get=gdk_device_get_scroll_lock_state)
   *
   * Whether Scroll Lock is on.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_SCROLL_LOCK_STATE] =
      g_param_spec_boolean ("scroll-lock-state", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:modifier-state: (attributes org.gtk.Property.get=gdk_device_get_modifier_state)
   *
   * The current modifier state of the device.
   *
   * This is only relevant for keyboard devices.
   */
  device_props[PROP_MODIFIER_STATE] =
      g_param_spec_flags ("modifier-state", NULL, NULL,
                          GDK_TYPE_MODIFIER_TYPE,
                          GDK_NO_MODIFIER_MASK,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, device_props);

  /**
   * GdkDevice::changed:
   * @device: the `GdkDevice`
   *
   * Emitted either when the number of either axes or keys changes.
   *
   * On X11 this will normally happen when the physical device
   * routing events through the logical device changes (for
   * example, user switches from the USB mouse to a tablet); in
   * that case the logical device will change to reflect the axes
   * and keys on the new physical device.
   */
  signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkDevice::tool-changed:
   * @device: the `GdkDevice`
   * @tool: The new current tool
   *
   * Emitted on pen/eraser devices whenever tools enter or leave proximity.
   */
  signals[TOOL_CHANGED] =
    g_signal_new (g_intern_static_string ("tool-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_DEVICE_TOOL);
}

static void
gdk_device_init (GdkDevice *device)
{
  device->axes = g_array_new (FALSE, TRUE, sizeof (GdkAxisInfo));
}

static void
gdk_device_finalize (GObject *object)
{
  GdkDevice *device = GDK_DEVICE (object);

  if (device->axes)
    {
      g_array_free (device->axes, TRUE);
      device->axes = NULL;
    }

  g_clear_pointer (&device->name, g_free);
  g_clear_pointer (&device->vendor_id, g_free);
  g_clear_pointer (&device->product_id, g_free);

  G_OBJECT_CLASS (gdk_device_parent_class)->finalize (object);
}

static void
gdk_device_dispose (GObject *object)
{
  GdkDevice *device = GDK_DEVICE (object);
  GdkDevice *associated = device->associated;

  if (associated)
    _gdk_device_remove_physical_device (associated, device);

  if (associated)
    {
      device->associated = NULL;

      if (associated->associated == device)
        _gdk_device_set_associated_device (associated, NULL);

      g_object_unref (associated);
    }

  g_clear_object (&device->last_tool);

  G_OBJECT_CLASS (gdk_device_parent_class)->dispose (object);
}

static void
gdk_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      device->display = g_value_get_object (value);
      break;
    case PROP_NAME:
      g_free (device->name);

      device->name = g_value_dup_string (value);
      break;
    case PROP_SOURCE:
      device->source = g_value_get_enum (value);
      break;
    case PROP_HAS_CURSOR:
      device->has_cursor = g_value_get_boolean (value);
      break;
    case PROP_VENDOR_ID:
      device->vendor_id = g_value_dup_string (value);
      break;
    case PROP_PRODUCT_ID:
      device->product_id = g_value_dup_string (value);
      break;
    case PROP_SEAT:
      device->seat = g_value_get_object (value);
      break;
    case PROP_NUM_TOUCHES:
      device->num_touches = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, device->display);
      break;
    case PROP_NAME:
      g_value_set_string (value, device->name);
      break;
    case PROP_SOURCE:
      g_value_set_enum (value, device->source);
      break;
    case PROP_HAS_CURSOR:
      g_value_set_boolean (value, device->has_cursor);
      break;
    case PROP_N_AXES:
      g_value_set_uint (value, device->axes->len);
      break;
    case PROP_VENDOR_ID:
      g_value_set_string (value, device->vendor_id);
      break;
    case PROP_PRODUCT_ID:
      g_value_set_string (value, device->product_id);
      break;
    case PROP_SEAT:
      g_value_set_object (value, device->seat);
      break;
    case PROP_NUM_TOUCHES:
      g_value_set_uint (value, device->num_touches);
      break;
    case PROP_TOOL:
      g_value_set_object (value, device->last_tool);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, gdk_device_get_direction (device));
      break;
    case PROP_HAS_BIDI_LAYOUTS:
      g_value_set_boolean (value, gdk_device_has_bidi_layouts (device));
      break;
    case PROP_CAPS_LOCK_STATE:
      g_value_set_boolean (value, gdk_device_get_caps_lock_state (device));
      break;
    case PROP_NUM_LOCK_STATE:
      g_value_set_boolean (value, gdk_device_get_num_lock_state (device));
      break;
    case PROP_SCROLL_LOCK_STATE:
      g_value_set_boolean (value, gdk_device_get_scroll_lock_state (device));
      break;
    case PROP_MODIFIER_STATE:
      g_value_set_flags (value, gdk_device_get_modifier_state (device));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_get_surface_at_position:
 * @device: pointer `GdkDevice` to query info to
 * @win_x: (out) (optional): return location for the X coordinate
 *   of the device location relative to the surface origin
 * @win_y: (out) (optional): return location for the Y coordinate
 *   of the device location relative to the surface origin
 *
 * Obtains the surface underneath @device, returning the location of the
 * device in @win_x and @win_y.
 *
 * Returns %NULL if the surface tree under @device is not known to GDK
 * (for example, belongs to another application).
 *
 * Returns: (nullable) (transfer none): the `GdkSurface` under the
 *   device position
 */
GdkSurface *
gdk_device_get_surface_at_position (GdkDevice *device,
                                    double    *win_x,
                                    double    *win_y)
{
  double tmp_x, tmp_y;
  GdkSurface *surface;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (device->source != GDK_SOURCE_KEYBOARD, NULL);

  surface = _gdk_device_surface_at_position (device, &tmp_x, &tmp_y, NULL);

  if (win_x)
    *win_x = tmp_x;
  if (win_y)
    *win_y = tmp_y;

  return surface;
}

/**
 * gdk_device_get_name:
 * @device: a GdkDevice`
 *
 * The name of the device, suitable for showing in a user interface.
 *
 * Returns: a name
 */
const char *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * gdk_device_get_has_cursor: (attributes org.gtk.Method.get_property=has-cursor)
 * @device: a `GdkDevice`
 *
 * Determines whether the pointer follows device motion.
 *
 * This is not meaningful for keyboard devices, which
 * don't have a pointer.
 *
 * Returns: %TRUE if the pointer follows device motion
 */
gboolean
gdk_device_get_has_cursor (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  return device->has_cursor;
}

/**
 * gdk_device_get_source: (attributes org.gtk.Method.get_property=source)
 * @device: a `GdkDevice`
 *
 * Determines the type of the device.
 *
 * Returns: a `GdkInputSource`
 */
GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->source;
}

/**
 * gdk_device_get_axis_use:
 * @device: a pointer `GdkDevice`
 * @index_: the index of the axi.
 *
 * Returns the axis use for @index_.
 *
 * Returns: a `GdkAxisUse` specifying how the axis is used.
 */
GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device,
                         guint      index_)
{
  GdkAxisInfo *info;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE);
  g_return_val_if_fail (device->source != GDK_SOURCE_KEYBOARD, GDK_AXIS_IGNORE);
  g_return_val_if_fail (index_ < device->axes->len, GDK_AXIS_IGNORE);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);

  return info->use;
}

/**
 * gdk_device_get_display: (attributes org.gtk.Method.get_property=display)
 * @device: a `GdkDevice`
 *
 * Returns the `GdkDisplay` to which @device pertains.
 *
 * Returns: (transfer none): a `GdkDisplay`
 */
GdkDisplay *
gdk_device_get_display (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->display;
}

void
_gdk_device_set_associated_device (GdkDevice *device,
                                   GdkDevice *associated)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (associated == NULL || GDK_IS_DEVICE (associated));

  g_set_object (&device->associated, associated);
}

/*
 * gdk_device_list_physical_devices:
 * @device: a logical `GdkDevice`
 *
 * Returns the list of physical devices attached to the given logical
 * `GdkDevice`.
 *
 * Returns: (nullable) (transfer container) (element-type GdkDevice):
 *   the list of physical devices attached to a logical `GdkDevice`
 */
GList *
gdk_device_list_physical_devices (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return g_list_copy (device->physical_devices);
}

void
_gdk_device_add_physical_device (GdkDevice *device,
                                 GdkDevice *physical)
{
  if (!g_list_find (device->physical_devices, physical))
    device->physical_devices = g_list_prepend (device->physical_devices, physical);
}

void
_gdk_device_remove_physical_device (GdkDevice *device,
                                    GdkDevice *physical)
{
  GList *elem;

  elem = g_list_find (device->physical_devices, physical);
  if (elem == NULL)
    return;

  device->physical_devices = g_list_delete_link (device->physical_devices, elem);
}

/*
 * gdk_device_get_n_axes:
 * @device: a pointer `GdkDevice`
 *
 * Returns the number of axes the device currently has.
 *
 * Returns: the number of axes.
 */
int
gdk_device_get_n_axes (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);
  g_return_val_if_fail (device->source != GDK_SOURCE_KEYBOARD, 0);

  return device->axes->len;
}

/*
 * gdk_device_get_axis: (skip)
 * @device: a `GdkDevice`
 * @axes: (array): pointer to an array of axes
 * @use: the use to look for
 * @value: (out): location to store the found value
 *
 * Interprets an array of `double` as axis values and get the value
 * for a given axis use.
 *
 * Returns: %TRUE if the given axis use was found, otherwise %FALSE
 */
gboolean
gdk_device_get_axis (GdkDevice  *device,
                     double     *axes,
                     GdkAxisUse  use,
                     double     *value)
{
  int i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (device->source != GDK_SOURCE_KEYBOARD, FALSE);

  if (axes == NULL)
    return FALSE;

  g_return_val_if_fail (device->axes != NULL, FALSE);

  for (i = 0; i < device->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (device->axes, GdkAxisInfo, i);

      if (axis_info.use != use)
        continue;

      if (value)
        *value = axes[i];

      return TRUE;
    }

  return FALSE;
}

static GdkEventMask
get_native_grab_event_mask (GdkEventMask grab_mask)
{
  /* Similar to the above but for pointer events only */
  return
    GDK_POINTER_MOTION_MASK |
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
    GDK_SCROLL_MASK |
    (grab_mask &
     ~(GDK_BUTTON_MOTION_MASK |
       GDK_BUTTON1_MOTION_MASK |
       GDK_BUTTON2_MOTION_MASK |
       GDK_BUTTON3_MOTION_MASK));
}

GdkGrabStatus
gdk_device_grab (GdkDevice        *device,
                 GdkSurface       *surface,
                 gboolean          owner_events,
                 GdkEventMask      event_mask,
                 GdkCursor        *cursor,
                 guint32           time_)
{
  GdkGrabStatus res;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_GRAB_FAILED);
  g_return_val_if_fail (GDK_IS_SURFACE (surface), GDK_GRAB_FAILED);
  g_return_val_if_fail (gdk_surface_get_display (surface) == gdk_device_get_display (device), GDK_GRAB_FAILED);

  if (GDK_SURFACE_DESTROYED (surface))
    return GDK_GRAB_NOT_VIEWABLE;

  res = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                             surface,
                                             owner_events,
                                             get_native_grab_event_mask (event_mask),
                                             NULL,
                                             cursor,
                                             time_);

  if (res == GDK_GRAB_SUCCESS)
    {
      GdkDisplay *display;
      gulong serial;

      display = gdk_surface_get_display (surface);
      serial = _gdk_display_get_next_serial (display);

      _gdk_display_add_device_grab (display,
                                    device,
                                    surface,
                                    owner_events,
                                    event_mask,
                                    serial,
                                    time_,
                                    FALSE);
    }

  return res;
}

void
gdk_device_ungrab (GdkDevice  *device,
                   guint32     time_)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  GDK_DEVICE_GET_CLASS (device)->ungrab (device, time_);
}

/* Private API */
void
_gdk_device_reset_axes (GdkDevice *device)
{
  int i;

  for (i = device->axes->len - 1; i >= 0; i--)
    g_array_remove_index (device->axes, i);

  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_N_AXES]);
}

guint
_gdk_device_add_axis (GdkDevice   *device,
                      GdkAxisUse   use,
                      double       min_value,
                      double       max_value,
                      double       resolution)
{
  GdkAxisInfo axis_info;
  guint pos;

  axis_info.use = use;
  axis_info.min_value = min_value;
  axis_info.max_value = max_value;
  axis_info.resolution = resolution;

  switch ((guint) use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      axis_info.min_axis = 0;
      axis_info.max_axis = 0;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      axis_info.min_axis = -1;
      axis_info.max_axis = 1;
      break;
    default:
      axis_info.min_axis = 0;
      axis_info.max_axis = 1;
      break;
    }

  device->axes = g_array_append_val (device->axes, axis_info);
  pos = device->axes->len - 1;

  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_N_AXES]);

  return pos;
}

void
_gdk_device_get_axis_info (GdkDevice   *device,
                           guint        index_,
                           GdkAxisUse   *use,
                           double       *min_value,
                           double       *max_value,
                           double       *resolution)
{
  GdkAxisInfo *info;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index_ < device->axes->len);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);

  *use = info->use;
  *min_value = info->min_value;
  *max_value = info->max_value;
  *resolution = info->resolution;
}

static GdkAxisInfo *
find_axis_info (GArray     *array,
                GdkAxisUse  use)
{
  GdkAxisInfo *info;
  int i;

  for (i = 0; i < GDK_AXIS_LAST; i++)
    {
      info = &g_array_index (array, GdkAxisInfo, i);

      if (info->use == use)
        return info;
    }

  return NULL;
}

gboolean
_gdk_device_translate_surface_coord (GdkDevice *device,
                                     GdkSurface *surface,
                                     guint      index_,
                                     double     value,
                                     double    *axis_value)
{
  GdkAxisInfo axis_info;
  GdkAxisInfo *axis_info_x, *axis_info_y;
  double device_width, device_height;
  double x_offset, y_offset;
  double x_scale, y_scale;
  double x_min, y_min;
  double x_resolution, y_resolution;
  double device_aspect;
  int surface_width, surface_height;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use != GDK_AXIS_X &&
      axis_info.use != GDK_AXIS_Y)
    return FALSE;

  if (axis_info.use == GDK_AXIS_X)
    {
      axis_info_x = &axis_info;
      axis_info_y = find_axis_info (device->axes, GDK_AXIS_Y);
      if (axis_info_y == NULL)
        return FALSE;
    }
  else
    {
      axis_info_x = find_axis_info (device->axes, GDK_AXIS_X);
      axis_info_y = &axis_info;
      if (axis_info_x == NULL)
        return FALSE;
    }

  device_width = axis_info_x->max_value - axis_info_x->min_value;
  device_height = axis_info_y->max_value - axis_info_y->min_value;

  x_min = axis_info_x->min_value;
  y_min = axis_info_y->min_value;

  surface_width = gdk_surface_get_width (surface);
  surface_height = gdk_surface_get_height (surface);

  x_resolution = axis_info_x->resolution;
  y_resolution = axis_info_y->resolution;

  /*
   * Some drivers incorrectly report the resolution of the device
   * as zero (in particular linuxwacom < 0.5.3 with usb tablets).
   * This causes the device_aspect to become NaN and totally
   * breaks windowed mode.  If this is the case, the best we can
   * do is to assume the resolution is non-zero is equal in both
   * directions (which is true for many devices).  The absolute
   * value of the resolution doesn't matter since we only use the
   * ratio.
   */
  if (x_resolution == 0 || y_resolution == 0)
    {
      x_resolution = 1;
      y_resolution = 1;
    }

  device_aspect = (device_height * y_resolution) /
    (device_width * x_resolution);

  if (device_aspect * surface_width >= surface_height)
    {
      /* device taller than surface */
      x_scale = surface_width / device_width;
      y_scale = (x_scale * x_resolution) / y_resolution;

      x_offset = 0;
      y_offset = - (device_height * y_scale - surface_height) / 2;
    }
  else
    {
      /* surface taller than device */
      y_scale = surface_height / device_height;
      x_scale = (y_scale * y_resolution) / x_resolution;

      y_offset = 0;
      x_offset = - (device_width * x_scale - surface_width) / 2;
    }

  if (axis_value)
    {
      if (axis_info.use == GDK_AXIS_X)
        *axis_value = x_offset + x_scale * (value - x_min);
      else
        *axis_value = y_offset + y_scale * (value - y_min);
    }

  return TRUE;
}

gboolean
_gdk_device_translate_screen_coord (GdkDevice *device,
                                    GdkSurface *surface,
                                    double     surface_root_x,
                                    double     surface_root_y,
                                    double     screen_width,
                                    double     screen_height,
                                    guint      index_,
                                    double     value,
                                    double    *axis_value)
{
  GdkAxisInfo axis_info;
  double axis_width, scale, offset;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use != GDK_AXIS_X &&
      axis_info.use != GDK_AXIS_Y)
    return FALSE;

  axis_width = axis_info.max_value - axis_info.min_value;

  if (axis_info.use == GDK_AXIS_X)
    {
      if (axis_width > 0)
        scale = screen_width / axis_width;
      else
        scale = 1;

      offset = - surface_root_x;
    }
  else
    {
      if (axis_width > 0)
        scale = screen_height / axis_width;
      else
        scale = 1;

      offset = - surface_root_y;
    }

  if (axis_value)
    *axis_value = offset + scale * (value - axis_info.min_value);

  return TRUE;
}

gboolean
_gdk_device_translate_axis (GdkDevice *device,
                            guint      index_,
                            double     value,
                            double    *axis_value)
{
  GdkAxisInfo axis_info;
  double axis_width, out;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use == GDK_AXIS_X ||
      axis_info.use == GDK_AXIS_Y)
    return FALSE;

  axis_width = axis_info.max_value - axis_info.min_value;
  out = (axis_info.max_axis * (value - axis_info.min_value) +
         axis_info.min_axis * (axis_info.max_value - value)) / axis_width;

  if (axis_value)
    *axis_value = out;

  return TRUE;
}

GdkSurface *
_gdk_device_surface_at_position (GdkDevice       *device,
                                 double          *win_x,
                                 double          *win_y,
                                 GdkModifierType *mask)
{
  return GDK_DEVICE_GET_CLASS (device)->surface_at_position (device,
                                                             win_x,
                                                             win_y,
                                                             mask);
}

/**
 * gdk_device_get_vendor_id: (attributes org.gtk.Method.get_property=vendor-id)
 * @device: a physical `GdkDevice`
 *
 * Returns the vendor ID of this device.
 *
 * This ID is retrieved from the device, and does not change.
 *
 * This function, together with [method@Gdk.Device.get_product_id],
 * can be used to eg. compose `GSettings` paths to store settings
 * for this device.
 *
 * ```c
 *  static GSettings *
 *  get_device_settings (GdkDevice *device)
 *  {
 *    const char *vendor, *product;
 *    GSettings *settings;
 *    GdkDevice *device;
 *    char *path;
 *
 *    vendor = gdk_device_get_vendor_id (device);
 *    product = gdk_device_get_product_id (device);
 *
 *    path = g_strdup_printf ("/org/example/app/devices/%s:%s/", vendor, product);
 *    settings = g_settings_new_with_path (DEVICE_SCHEMA, path);
 *    g_free (path);
 *
 *    return settings;
 *  }
 * ```
 *
 * Returns: (nullable): the vendor ID
 */
const char *
gdk_device_get_vendor_id (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->vendor_id;
}

/**
 * gdk_device_get_product_id: (attributes org.gtk.Method.get_property=product-id)
 * @device: a physical `GdkDevice`
 *
 * Returns the product ID of this device.
 *
 * This ID is retrieved from the device, and does not change.
 * See [method@Gdk.Device.get_vendor_id] for more information.
 *
 * Returns: (nullable): the product ID
 */
const char *
gdk_device_get_product_id (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->product_id;
}

void
gdk_device_set_seat (GdkDevice *device,
                     GdkSeat   *seat)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (!seat || GDK_IS_SEAT (seat));

  if (device->seat == seat)
    return;

  device->seat = seat;
  g_object_notify (G_OBJECT (device), "seat");
}

/**
 * gdk_device_get_seat: (attributes org.gtk.Method.get_property=seat)
 * @device: A `GdkDevice`
 *
 * Returns the `GdkSeat` the device belongs to.
 *
 * Returns: (transfer none): a `GdkSeat`
 */
GdkSeat *
gdk_device_get_seat (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->seat;
}

void
gdk_device_update_tool (GdkDevice     *device,
                        GdkDeviceTool *tool)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (g_set_object (&device->last_tool, tool))
    {
      g_object_notify (G_OBJECT (device), "tool");
      g_signal_emit (device, signals[TOOL_CHANGED], 0, tool);
    }
}

/**
 * gdk_device_get_num_touches:
 * @device: a `GdkDevice`
 *
 * Retrieves the number of touch points associated to @device.
 *
 * Returns: the number of touch points
 */
guint
gdk_device_get_num_touches (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_touches;
}

/**
 * gdk_device_get_device_tool: (attributes org.gtk.Method.get_property=tool)
 * @device: a `GdkDevice`
 *
 * Retrieves the current tool for @device.
 *
 * Returns: (transfer none) (nullable): the `GdkDeviceTool`
 */
GdkDeviceTool *
gdk_device_get_device_tool (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->last_tool;
}

/**
 * gdk_device_get_caps_lock_state: (attributes org.gtk.Method.get_property=caps-lock-state)
 * @device: a `GdkDevice`
 *
 * Retrieves whether the Caps Lock modifier of the keyboard is locked.
 *
 * This is only relevant for keyboard devices.
 *
 * Returns: %TRUE if Caps Lock is on for @device
 */
gboolean
gdk_device_get_caps_lock_state (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_get_caps_lock_state (keymap);

  return FALSE;
}

/**
 * gdk_device_get_num_lock_state: (attributes org.gtk.Method.get_property=num-lock-state)
 * @device: a ``GdkDevice`
 *
 * Retrieves whether the Num Lock modifier of the keyboard is locked.
 *
 * This is only relevant for keyboard devices.
 *
 * Returns: %TRUE if Num Lock is on for @device
 */
gboolean
gdk_device_get_num_lock_state (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_get_num_lock_state (keymap);

  return FALSE;
}

/**
 * gdk_device_get_scroll_lock_state: (attributes org.gtk.Method.get_property=scroll-lock-state)
 * @device: a `GdkDevice`
 *
 * Retrieves whether the Scroll Lock modifier of the keyboard is locked.
 *
 * This is only relevant for keyboard devices.
 *
 * Returns: %TRUE if Scroll Lock is on for @device
 */
gboolean
gdk_device_get_scroll_lock_state (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_get_scroll_lock_state (keymap);

  return FALSE;
}

/**
 * gdk_device_get_modifier_state: (attributes org.gtk.Method.get_property=modifier-state)
 * @device: a `GdkDevice`
 *
 * Retrieves the current modifier state of the keyboard.
 *
 * This is only relevant for keyboard devices.
 *
 * Returns: the current modifier state
 */
GdkModifierType
gdk_device_get_modifier_state (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_get_modifier_state (keymap);

  return 0;
}

/**
 * gdk_device_get_direction: (attributes org.gtk.Method.get_property=direction)
 * @device: a `GdkDevice`
 *
 * Returns the direction of effective layout of the keyboard.
 *
 * This is only relevant for keyboard devices.
 *
 * The direction of a layout is the direction of the majority
 * of its symbols. See [func@Pango.unichar_direction].
 *
 * Returns: %PANGO_DIRECTION_LTR or %PANGO_DIRECTION_RTL
 *   if it can determine the direction. %PANGO_DIRECTION_NEUTRAL
 *   otherwise
 */
PangoDirection
gdk_device_get_direction (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_get_direction (keymap);

  return PANGO_DIRECTION_NEUTRAL;
}

/**
 * gdk_device_has_bidi_layouts: (attributes org.gtk.Method.get_property=has-bidi-layouts)
 * @device: a `GdkDevice`
 *
 * Determines if layouts for both right-to-left and
 * left-to-right languages are in use on the keyboard.
 *
 * This is only relevant for keyboard devices.
 *
 * Returns: %TRUE if there are layouts with both directions, %FALSE otherwise
 */
gboolean
gdk_device_has_bidi_layouts (GdkDevice *device)
{
  GdkKeymap *keymap = gdk_display_get_keymap (device->display);

  if (device->source == GDK_SOURCE_KEYBOARD)
    return gdk_keymap_have_bidi_layouts (keymap);

  return FALSE;
}

void
gdk_device_set_timestamp (GdkDevice *device,
                          guint32    timestamp)
{
  device->timestamp = timestamp;
}

/**
 * gdk_device_get_timestamp:
 * @device: a `GdkDevice`
 *
 * Returns the timestamp of the last activity for this device.
 *
 * In practice, this means the timestamp of the last event that was
 * received from the OS for this device. (GTK may occasionally produce
 * events for a device that are not received from the OS, and will not
 * update the timestamp).
 *
 * Returns: the timestamp of the last activity for this device
 *
 * Since: 4.2
 */
guint32
gdk_device_get_timestamp (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_CURRENT_TIME);

  return device->timestamp;
}
