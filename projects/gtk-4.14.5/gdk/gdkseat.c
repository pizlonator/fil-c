/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2015 Red Hat
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

#include "config.h"

#include <glib-object.h>
#include "gdkdisplay.h"
#include "gdkdevice.h"
#include "gdkdevicetoolprivate.h"
#include "gdkseatprivate.h"
#include "gdkdeviceprivate.h"
#include <glib/gi18n-lib.h>

/**
 * GdkSeat:
 *
 * The `GdkSeat` object represents a collection of input devices
 * that belong to a user.
 */

typedef struct _GdkSeatPrivate GdkSeatPrivate;

struct _GdkSeatPrivate
{
  GdkDisplay *display;
};

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  TOOL_ADDED,
  TOOL_REMOVED,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_DISPLAY,
  N_PROPS
};

static guint signals[N_SIGNALS] = { 0 };
static GParamSpec *props[N_PROPS] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkSeat, gdk_seat, G_TYPE_OBJECT)

static void
gdk_seat_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GdkSeatPrivate *priv = gdk_seat_get_instance_private (GDK_SEAT (object));

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_seat_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GdkSeatPrivate *priv = gdk_seat_get_instance_private (GDK_SEAT (object));

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_seat_class_init (GdkSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_seat_set_property;
  object_class->get_property = gdk_seat_get_property;

  /**
   * GdkSeat::device-added:
   * @seat: the object on which the signal is emitted
   * @device: the newly added `GdkDevice`.
   *
   * Emitted when a new input device is related to this seat.
   */
  signals [DEVICE_ADDED] =
    g_signal_new (g_intern_static_string ("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkSeatClass, device_added),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  /**
   * GdkSeat::device-removed:
   * @seat: the object on which the signal is emitted
   * @device: the just removed `GdkDevice`.
   *
   * Emitted when an input device is removed (e.g. unplugged).
   */
  signals [DEVICE_REMOVED] =
    g_signal_new (g_intern_static_string ("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkSeatClass, device_removed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  /**
   * GdkSeat::tool-added:
   * @seat: the object on which the signal is emitted
   * @tool: the new `GdkDeviceTool` known to the seat
   *
   * Emitted whenever a new tool is made known to the seat.
   *
   * The tool may later be assigned to a device (i.e. on
   * proximity with a tablet). The device will emit the
   * [signal@Gdk.Device::tool-changed] signal accordingly.
   *
   * A same tool may be used by several devices.
   */
  signals [TOOL_ADDED] =
    g_signal_new (g_intern_static_string ("tool-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE_TOOL);

  /**
   * GdkSeat::tool-removed:
   * @seat: the object on which the signal is emitted
   * @tool: the just removed `GdkDeviceTool`
   *
   * Emitted whenever a tool is no longer known to this @seat.
   */
  signals [TOOL_REMOVED] =
    g_signal_new (g_intern_static_string ("tool-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE_TOOL);

  /**
   * GdkSeat:display: (attributes org.gtk.Property.get=gdk_seat_get_display)
   *
   * `GdkDisplay` of this seat.
   */
  props[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
gdk_seat_init (GdkSeat *seat)
{
}

/**
 * gdk_seat_get_capabilities:
 * @seat: a `GdkSeat`
 *
 * Returns the capabilities this `GdkSeat` currently has.
 *
 * Returns: the seat capabilities
 */
GdkSeatCapabilities
gdk_seat_get_capabilities (GdkSeat *seat)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), GDK_SEAT_CAPABILITY_NONE);

  seat_class = GDK_SEAT_GET_CLASS (seat);
  return seat_class->get_capabilities (seat);
}

/*
 * gdk_seat_grab:
 * @seat: a `GdkSeat`
 * @surface: the `GdkSurface` which will own the grab
 * @capabilities: capabilities that will be grabbed
 * @owner_events: if %FALSE then all device events are reported with respect to
 *   @surface and are only reported if selected by @event_mask. If %TRUE then
 *   pointer events for this application are reported as normal, but pointer
 *   events outside this application are reported with respect to @surface and
 *   only if selected by @event_mask. In either mode, unreported events are
 *   discarded.
 * @cursor: (nullable): the cursor to display while the grab is active.
 *   If this is %NULL then the normal cursors are used for @surface and
 *   its descendants, and the cursor for @surface is used elsewhere.
 * @event: (nullable): the event that is triggering the grab, or %NULL if none
 *   is available.
 * @prepare_func: (nullable) (scope call): function to prepare the surface
 *   to be grabbed, it can be %NULL if @surface is visible before this call.
 * @prepare_func_data: (closure): user data to pass to @prepare_func
 *
 * Grabs the seat so that all events corresponding to the given @capabilities
 * are passed to this application.
 *
 * The grab remains in place until the seat is ungrabbed with
 * [method@Gdk.Seat.ungrab], or the surface becomes hidden. This overrides
 * any previous grab on the seat by this client.
 *
 * As a rule of thumb, if a grab is desired over %GDK_SEAT_CAPABILITY_POINTER,
 * all other "pointing" capabilities (eg. %GDK_SEAT_CAPABILITY_TOUCH) should
 * be grabbed too, so the user is able to interact with all of those while
 * the grab holds, you should thus use %GDK_SEAT_CAPABILITY_ALL_POINTING most
 * commonly.
 *
 * Grabs are used for operations which need complete control over the
 * events corresponding to the given capabilities. For example in GTK this
 * is used for Drag and Drop operations, popup menus and such.
 *
 * Note that if the event mask of a `GdkSurface` has selected both button press
 * and button release events, or touch begin and touch end, then a press event
 * will cause an automatic grab until the button is released, equivalent to a
 * grab on the surface with @owner_events set to %TRUE. This is done because
 * most applications expect to receive paired press and release events.
 *
 * If you set up anything at the time you take the grab that needs to be
 * cleaned up when the grab ends, you should handle the `GdkEventGrabBroken`
 * events that are emitted when the grab ends unvoluntarily.
 *
 * Returns: %GDK_GRAB_SUCCESS if the grab was successful.
 */
GdkGrabStatus
gdk_seat_grab (GdkSeat                *seat,
               GdkSurface              *surface,
               GdkSeatCapabilities     capabilities,
               gboolean                owner_events,
               GdkCursor              *cursor,
               GdkEvent               *event,
               GdkSeatGrabPrepareFunc  prepare_func,
               gpointer                prepare_func_data)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), GDK_GRAB_FAILED);
  g_return_val_if_fail (GDK_IS_SURFACE (surface), GDK_GRAB_FAILED);
  g_return_val_if_fail (gdk_surface_get_display (surface) == gdk_seat_get_display (seat), GDK_GRAB_FAILED);

  capabilities &= GDK_SEAT_CAPABILITY_ALL;
  g_return_val_if_fail (capabilities != GDK_SEAT_CAPABILITY_NONE, GDK_GRAB_FAILED);

  seat_class = GDK_SEAT_GET_CLASS (seat);

  return seat_class->grab (seat, surface, capabilities, owner_events, cursor,
                           event, prepare_func, prepare_func_data);
}

/*
 * gdk_seat_ungrab:
 * @seat: a `GdkSeat`
 *
 * Releases a grab.
 *
 * See [method@Gdk.Seat.grab] for more information.
 */
void
gdk_seat_ungrab (GdkSeat *seat)
{
  GdkSeatClass *seat_class;

  g_return_if_fail (GDK_IS_SEAT (seat));

  seat_class = GDK_SEAT_GET_CLASS (seat);
  seat_class->ungrab (seat);
}

/**
 * gdk_seat_get_devices:
 * @seat: a `GdkSeat`
 * @capabilities: capabilities to get devices for
 *
 * Returns the devices that match the given capabilities.
 *
 * Returns: (transfer container) (element-type GdkDevice): A list
 *   of `GdkDevices`. The list must be freed with g_list_free(),
 *   the elements are owned by GTK and must not be freed.
 */
GList *
gdk_seat_get_devices (GdkSeat             *seat,
                      GdkSeatCapabilities  capabilities)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), NULL);

  seat_class = GDK_SEAT_GET_CLASS (seat);
  return seat_class->get_devices (seat, capabilities);
}

/**
 * gdk_seat_get_pointer:
 * @seat: a `GdkSeat`
 *
 * Returns the device that routes pointer events.
 *
 * Returns: (transfer none) (nullable): a `GdkDevice` with pointer
 *   capabilities. This object is owned by GTK and must not be freed.
 */
GdkDevice *
gdk_seat_get_pointer (GdkSeat *seat)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), NULL);

  seat_class = GDK_SEAT_GET_CLASS (seat);
  return seat_class->get_logical_device (seat, GDK_SEAT_CAPABILITY_POINTER);
}

/**
 * gdk_seat_get_keyboard:
 * @seat: a `GdkSeat`
 *
 * Returns the device that routes keyboard events.
 *
 * Returns: (transfer none) (nullable): a `GdkDevice` with keyboard
 *   capabilities. This object is owned by GTK and must not be freed.
 */
GdkDevice *
gdk_seat_get_keyboard (GdkSeat *seat)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), NULL);

  seat_class = GDK_SEAT_GET_CLASS (seat);
  return seat_class->get_logical_device (seat, GDK_SEAT_CAPABILITY_KEYBOARD);
}

void
gdk_seat_device_added (GdkSeat   *seat,
                       GdkDevice *device)
{
  gdk_device_set_seat (device, seat);
  g_signal_emit (seat, signals[DEVICE_ADDED], 0, device);
}

void
gdk_seat_device_removed (GdkSeat   *seat,
                         GdkDevice *device)
{
  gdk_device_set_seat (device, NULL);
  g_signal_emit (seat, signals[DEVICE_REMOVED], 0, device);
}

/**
 * gdk_seat_get_display: (attributes org.gtk.Method.get_property=display)
 * @seat: a `GdkSeat`
 *
 * Returns the `GdkDisplay` this seat belongs to.
 *
 * Returns: (transfer none): a `GdkDisplay`. This object
 *   is owned by GTK and must not be freed.
 */
GdkDisplay *
gdk_seat_get_display (GdkSeat *seat)
{
  GdkSeatPrivate *priv = gdk_seat_get_instance_private (seat);

  g_return_val_if_fail (GDK_IS_SEAT (seat), NULL);

  return priv->display;
}

void
gdk_seat_tool_added (GdkSeat       *seat,
                     GdkDeviceTool *tool)
{
  g_signal_emit (seat, signals[TOOL_ADDED], 0, tool);
}

void
gdk_seat_tool_removed (GdkSeat       *seat,
                       GdkDeviceTool *tool)
{
  g_signal_emit (seat, signals[TOOL_REMOVED], 0, tool);
}

GdkDeviceTool *
gdk_seat_get_tool (GdkSeat          *seat,
                   guint64           serial,
                   guint64           hw_id,
                   GdkDeviceToolType type)
{
  GdkDeviceTool *match = NULL;
  GList *tools, *l;

  tools = gdk_seat_get_tools (seat);

  for (l = tools; l; l = l->next)
    {
      GdkDeviceTool *tool = l->data;

      if (tool->serial == serial && tool->hw_id == hw_id && tool->type == type)
        {
          match = tool;
          break;
        }
    }

  g_list_free (tools);

  return match;
}

/**
 * gdk_seat_get_tools:
 * @seat: a `GdkSeat`
 *
 * Returns all `GdkDeviceTools` that are known to the application.
 *
 * Returns: (transfer container) (element-type Gdk.DeviceTool):
 *   A list of tools. Free with g_list_free().
 */
GList *
gdk_seat_get_tools (GdkSeat *seat)
{
  GdkSeatClass *seat_class;

  g_return_val_if_fail (GDK_IS_SEAT (seat), NULL);

  seat_class = GDK_SEAT_GET_CLASS (seat);
  return seat_class->get_tools (seat);
}
