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

#include "gdkdevicetoolprivate.h"
#include "gdkenumtypes.h"
#include <glib/gi18n-lib.h>

/**
 * GdkDeviceTool:
 *
 * A physical tool associated to a `GdkDevice`.
 */

G_DEFINE_TYPE (GdkDeviceTool, gdk_device_tool, G_TYPE_OBJECT)

enum {
  TOOL_PROP_0,
  TOOL_PROP_SERIAL,
  TOOL_PROP_TOOL_TYPE,
  TOOL_PROP_AXES,
  TOOL_PROP_HARDWARE_ID,
  N_TOOL_PROPS
};

GParamSpec *tool_props[N_TOOL_PROPS] = { 0 };

static void
gdk_device_tool_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkDeviceTool *tool = GDK_DEVICE_TOOL (object);

  switch (prop_id)
    {
    case TOOL_PROP_SERIAL:
      tool->serial = g_value_get_uint64 (value);
      break;
    case TOOL_PROP_TOOL_TYPE:
      tool->type = g_value_get_enum (value);
      break;
    case TOOL_PROP_AXES:
      tool->tool_axes = g_value_get_flags (value);
      break;
    case TOOL_PROP_HARDWARE_ID:
      tool->hw_id = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_tool_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkDeviceTool *tool = GDK_DEVICE_TOOL (object);

  switch (prop_id)
    {
    case TOOL_PROP_SERIAL:
      g_value_set_uint64 (value, tool->serial);
      break;
    case TOOL_PROP_TOOL_TYPE:
      g_value_set_enum (value, tool->type);
      break;
    case TOOL_PROP_AXES:
      g_value_set_flags (value, tool->tool_axes);
      break;
    case TOOL_PROP_HARDWARE_ID:
      g_value_set_uint64 (value, tool->hw_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_tool_class_init (GdkDeviceToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_device_tool_set_property;
  object_class->get_property = gdk_device_tool_get_property;

  /**
   * GdkDeviceTool:serial: (attributes org.gtk.Property.get=gdk_device_tool_get_serial)
   *
   * The serial number of the tool.
   */
  tool_props[TOOL_PROP_SERIAL] = g_param_spec_uint64 ("serial", NULL, NULL,
                                                      0, G_MAXUINT64, 0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkDeviceTool:tool-type: (attributes org.gtk.Property.get=gdk_device_tool_get_tool_type)
   *
   * The type of the tool.
   */
  tool_props[TOOL_PROP_TOOL_TYPE] = g_param_spec_enum ("tool-type", NULL, NULL,
                                                       GDK_TYPE_DEVICE_TOOL_TYPE,
                                                       GDK_DEVICE_TOOL_TYPE_UNKNOWN,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS);

  /**
   * GdkDeviceTool:axes: (attributes org.gtk.Property.get=gdk_device_tool_get_axes)
   *
   * The axes of the tool.
   */
  tool_props[TOOL_PROP_AXES] = g_param_spec_flags ("axes", NULL, NULL,
                                                   GDK_TYPE_AXIS_FLAGS, 0,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT_ONLY);

  /**
   * GdkDeviceTool:hardware-id: (attributes org.gtk.Property.get=gdk_device_tool_get_hardware_id)
   *
   * The hardware ID of the tool.
   */
  tool_props[TOOL_PROP_HARDWARE_ID] = g_param_spec_uint64 ("hardware-id", NULL, NULL,
                                                           0, G_MAXUINT64, 0,
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT_ONLY |
                                                           G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_TOOL_PROPS, tool_props);
}

static void
gdk_device_tool_init (GdkDeviceTool *tool)
{
}

GdkDeviceTool *
gdk_device_tool_new (guint64           serial,
                     guint64           hw_id,
                     GdkDeviceToolType type,
                     GdkAxisFlags      tool_axes)
{
  return g_object_new (GDK_TYPE_DEVICE_TOOL,
                       "serial", serial,
                       "hardware-id", hw_id,
                       "tool-type", type,
                       "axes", tool_axes,
                       NULL);
}

/**
 * gdk_device_tool_get_serial: (attributes org.gtk.Method.get_property=serial)
 * @tool: a `GdkDeviceTool`
 *
 * Gets the serial number of this tool.
 *
 * This value can be used to identify a physical tool
 * (eg. a tablet pen) across program executions.
 *
 * Returns: The serial ID for this tool
 */
guint64
gdk_device_tool_get_serial (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, 0);

  return tool->serial;
}

/**
 * gdk_device_tool_get_hardware_id: (attributes org.gtk.Method.get_property=hardware-id)
 * @tool: a `GdkDeviceTool`
 *
 * Gets the hardware ID of this tool, or 0 if it's not known.
 *
 * When non-zero, the identifier is unique for the given tool model,
 * meaning that two identical tools will share the same @hardware_id,
 * but will have different serial numbers (see
 * [method@Gdk.DeviceTool.get_serial]).
 *
 * This is a more concrete (and device specific) method to identify
 * a `GdkDeviceTool` than [method@Gdk.DeviceTool.get_tool_type],
 * as a tablet may support multiple devices with the same
 * `GdkDeviceToolType`, but different hardware identifiers.
 *
 * Returns: The hardware identifier of this tool.
 */
guint64
gdk_device_tool_get_hardware_id (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, 0);

  return tool->hw_id;
}

/**
 * gdk_device_tool_get_tool_type: (attributes org.gtk.Method.get_property=tool-type)
 * @tool: a `GdkDeviceTool`
 *
 * Gets the `GdkDeviceToolType` of the tool.
 *
 * Returns: The physical type for this tool. This can be used to
 *   figure out what sort of pen is being used, such as an airbrush
 *   or a pencil.
 */
GdkDeviceToolType
gdk_device_tool_get_tool_type (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, GDK_DEVICE_TOOL_TYPE_UNKNOWN);

  return tool->type;
}

/**
 * gdk_device_tool_get_axes: (attributes org.gtk.Method.get_property=axes)
 * @tool: a `GdkDeviceTool`
 *
 * Gets the axes of the tool.
 *
 * Returns: the axes of @tool
 */
GdkAxisFlags
gdk_device_tool_get_axes (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, 0);

  return tool->tool_axes;
}
