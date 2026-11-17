/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "pp-print-device.h"

struct _PpPrintDevice
{
  GObject   parent_instance;

  gchar    *device_name;
  gchar    *display_name;
  gchar    *device_original_name;
  gchar    *device_make_and_model;
  gchar    *device_location;
  gchar    *device_info;
  gchar    *device_uri;
  gchar    *device_id;
  gchar    *device_ppd;
  gchar    *host_name;
  gint      host_port;
  gboolean  is_authenticated_server;
  gint      acquisition_method;
  gboolean  is_network_device;
};

G_DEFINE_TYPE (PpPrintDevice, pp_print_device, G_TYPE_OBJECT);

enum
{
  PROP_0 = 0,
  PROP_DEVICE_NAME,
  PROP_DISPLAY_NAME,
  PROP_DEVICE_ORIGINAL_NAME,
  PROP_DEVICE_MAKE_AND_MODEL,
  PROP_DEVICE_LOCATION,
  PROP_DEVICE_INFO,
  PROP_DEVICE_URI,
  PROP_DEVICE_ID,
  PROP_DEVICE_PPD,
  PROP_HOST_NAME,
  PROP_HOST_PORT,
  PROP_IS_AUTHENTICATED_SERVER,
  PROP_ACQUISITION_METHOD,
  PROP_IS_NETWORK_DEVICE
};

static void
pp_print_device_finalize (GObject *object)
{
  PpPrintDevice *self = PP_PRINT_DEVICE (object);

  g_clear_pointer (&self->device_name, g_free);
  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->device_original_name, g_free);
  g_clear_pointer (&self->device_make_and_model, g_free);
  g_clear_pointer (&self->device_location, g_free);
  g_clear_pointer (&self->device_info, g_free);
  g_clear_pointer (&self->device_uri, g_free);
  g_clear_pointer (&self->device_id, g_free);
  g_clear_pointer (&self->device_ppd, g_free);
  g_clear_pointer (&self->host_name, g_free);

  G_OBJECT_CLASS (pp_print_device_parent_class)->finalize (object);
}

static void
pp_print_device_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *param_spec)
{
  PpPrintDevice *self = PP_PRINT_DEVICE (object);

  switch (prop_id)
    {
      case PROP_DEVICE_NAME:
        g_value_set_string (value, self->device_name);
        break;
      case PROP_DISPLAY_NAME:
        g_value_set_string (value, self->display_name);
        break;
      case PROP_DEVICE_ORIGINAL_NAME:
        g_value_set_string (value, self->device_original_name);
        break;
      case PROP_DEVICE_MAKE_AND_MODEL:
        g_value_set_string (value, self->device_make_and_model);
        break;
      case PROP_DEVICE_LOCATION:
        g_value_set_string (value, self->device_location);
        break;
      case PROP_DEVICE_INFO:
        g_value_set_string (value, self->device_info);
        break;
      case PROP_DEVICE_URI:
        g_value_set_string (value, self->device_uri);
        break;
      case PROP_DEVICE_ID:
        g_value_set_string (value, self->device_id);
        break;
      case PROP_DEVICE_PPD:
        g_value_set_string (value, self->device_ppd);
        break;
      case PROP_HOST_NAME:
        g_value_set_string (value, self->host_name);
        break;
      case PROP_HOST_PORT:
        g_value_set_int (value, self->host_port);
        break;
      case PROP_IS_AUTHENTICATED_SERVER:
        g_value_set_boolean (value, self->is_authenticated_server);
        break;
      case PROP_ACQUISITION_METHOD:
        g_value_set_int (value, self->acquisition_method);
        break;
      case PROP_IS_NETWORK_DEVICE:
        g_value_set_boolean (value, self->is_network_device);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_print_device_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *param_spec)
{
  PpPrintDevice *self = PP_PRINT_DEVICE (object);

  switch (prop_id)
    {
      case PROP_DEVICE_NAME:
        g_free (self->device_name);
        self->device_name = g_value_dup_string (value);
        break;
      case PROP_DISPLAY_NAME:
        g_free (self->display_name);
        self->display_name = g_value_dup_string (value);
        break;
      case PROP_DEVICE_ORIGINAL_NAME:
        g_free (self->device_original_name);
        self->device_original_name = g_value_dup_string (value);
        break;
      case PROP_DEVICE_MAKE_AND_MODEL:
        g_free (self->device_make_and_model);
        self->device_make_and_model = g_value_dup_string (value);
        break;
      case PROP_DEVICE_LOCATION:
        g_free (self->device_location);
        self->device_location = g_value_dup_string (value);
        break;
      case PROP_DEVICE_INFO:
        g_free (self->device_info);
        self->device_info = g_value_dup_string (value);
        break;
      case PROP_DEVICE_URI:
        g_free (self->device_uri);
        self->device_uri = g_value_dup_string (value);
        break;
      case PROP_DEVICE_ID:
        g_free (self->device_id);
        self->device_id = g_value_dup_string (value);
        break;
      case PROP_DEVICE_PPD:
        g_free (self->device_ppd);
        self->device_ppd = g_value_dup_string (value);
        break;
      case PROP_HOST_NAME:
        g_free (self->host_name);
        self->host_name = g_value_dup_string (value);
        break;
      case PROP_HOST_PORT:
        self->host_port = g_value_get_int (value);
        break;
      case PROP_IS_AUTHENTICATED_SERVER:
        self->is_authenticated_server = g_value_get_boolean (value);
        break;
      case PROP_ACQUISITION_METHOD:
        self->acquisition_method = g_value_get_int (value);
        break;
      case PROP_IS_NETWORK_DEVICE:
        self->is_network_device = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_print_device_class_init (PpPrintDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = pp_print_device_set_property;
  gobject_class->get_property = pp_print_device_get_property;

  gobject_class->finalize = pp_print_device_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_NAME,
                                   g_param_spec_string ("device-name",
                                                        "Device name",
                                                        "Name of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY_NAME,
                                   g_param_spec_string ("display-name",
                                                        "Display name",
                                                        "Name of the device formatted for users",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_ORIGINAL_NAME,
                                   g_param_spec_string ("device-original-name",
                                                        "Device original name",
                                                        "Original name of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_MAKE_AND_MODEL,
                                   g_param_spec_string ("device-make-and-model",
                                                        "Device make and model",
                                                        "Make and model of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_LOCATION,
                                   g_param_spec_string ("device-location",
                                                        "Device location",
                                                        "Locaton of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_INFO,
                                   g_param_spec_string ("device-info",
                                                        "Device info",
                                                        "Information about the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_URI,
                                   g_param_spec_string ("device-uri",
                                                        "Device URI",
                                                        "URI of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_ID,
                                   g_param_spec_string ("device-id",
                                                        "DeviceID",
                                                        "DeviceID of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE_PPD,
                                   g_param_spec_string ("device-ppd",
                                                        "Device PPD",
                                                        "Name of the PPD of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_HOST_NAME,
                                   g_param_spec_string ("host-name",
                                                        "Host name",
                                                        "Hostname of the device",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_HOST_PORT,
                                   g_param_spec_int ("host-port",
                                                     "Host port",
                                                     "The port of the host",
                                                     0, G_MAXINT32, 0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_IS_AUTHENTICATED_SERVER,
                                   g_param_spec_boolean ("is-authenticated-server",
                                                         "Is authenticated server",
                                                         "Whether the device is a server which needs authentication",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACQUISITION_METHOD,
                                   g_param_spec_int ("acquisition-method",
                                                     "Acquisition method",
                                                     "Acquisition method of the device",
                                                     0, G_MAXINT32, 0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_IS_NETWORK_DEVICE,
                                   g_param_spec_boolean ("is-network-device",
                                                         "Network device",
                                                         "Whether the device is a network device",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
pp_print_device_init (PpPrintDevice *self)
{
}

PpPrintDevice *
pp_print_device_new ()
{
  return g_object_new (PP_TYPE_PRINT_DEVICE, NULL);
}

gchar *
pp_print_device_get_device_name (PpPrintDevice *self)
{
  return self->device_name;
}

gchar *
pp_print_device_get_display_name (PpPrintDevice *self)
{
  return self->display_name;
}

gchar *
pp_print_device_get_device_original_name (PpPrintDevice *self)
{
  return self->device_original_name;
}

gchar *
pp_print_device_get_device_make_and_model (PpPrintDevice *self)
{
  return self->device_make_and_model;
}

gchar *
pp_print_device_get_device_location (PpPrintDevice *self)
{
  return self->device_location;
}

gchar *
pp_print_device_get_device_info (PpPrintDevice *self)
{
  return self->device_info;
}

gchar *
pp_print_device_get_device_uri (PpPrintDevice *self)
{
  return self->device_uri;
}

gchar *
pp_print_device_get_device_id (PpPrintDevice *self)
{
  return self->device_id;
}

gchar *
pp_print_device_get_device_ppd (PpPrintDevice *self)
{
  return self->device_ppd;
}

gchar *
pp_print_device_get_host_name (PpPrintDevice *self)
{
  return self->host_name;
}

gint
pp_print_device_get_host_port (PpPrintDevice *self)
{
  return self->host_port;
}

gboolean
pp_print_device_is_authenticated_server (PpPrintDevice *self)
{
  return self->is_authenticated_server;
}

gint
pp_print_device_get_acquisition_method (PpPrintDevice *self)
{
  return self->acquisition_method;
}

gboolean
pp_print_device_is_network_device (PpPrintDevice *self)
{
  return self->is_network_device;
}

PpPrintDevice *
pp_print_device_copy (PpPrintDevice *self)
{
  return g_object_new (PP_TYPE_PRINT_DEVICE,
                       "device-name", pp_print_device_get_device_name (self),
                       "display-name", pp_print_device_get_display_name (self),
                       "device-original-name", pp_print_device_get_device_original_name (self),
                       "device-make-and-model", pp_print_device_get_device_make_and_model (self),
                       "device-location", pp_print_device_get_device_location (self),
                       "device-info", pp_print_device_get_device_info (self),
                       "device-uri", pp_print_device_get_device_uri (self),
                       "device-id", pp_print_device_get_device_id (self),
                       "device-ppd", pp_print_device_get_device_ppd (self),
                       "host-name", pp_print_device_get_host_name (self),
                       "host-port", pp_print_device_get_host_port (self),
                       "is-authenticated-server", pp_print_device_is_authenticated_server (self),
                       "acquisition-method", pp_print_device_get_acquisition_method (self),
                       "is-network-device", pp_print_device_is_network_device (self),
                       NULL);
}
