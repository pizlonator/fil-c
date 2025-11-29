/* GTK - The GIMP Toolkit
 * Copyright (C) 2018, Red Hat, Inc
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

#include "gtkcolorpickerportalprivate.h"
#include "gtkprivate.h"
#include <gio/gio.h>

struct _GtkColorPickerPortal
{
  GObject parent_instance;

  GDBusProxy *portal_proxy;
  guint portal_signal_id;
  GTask *task;
};

struct _GtkColorPickerPortalClass
{
  GObjectClass parent_class;
};

static GInitableIface *initable_parent_iface;
static void gtk_color_picker_portal_initable_iface_init (GInitableIface *iface);
static void gtk_color_picker_portal_iface_init (GtkColorPickerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorPickerPortal, gtk_color_picker_portal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gtk_color_picker_portal_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_PICKER, gtk_color_picker_portal_iface_init))

static gboolean
gtk_color_picker_portal_initable_init (GInitable     *initable,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  GtkColorPickerPortal *picker = GTK_COLOR_PICKER_PORTAL (initable);
  char *owner;
  GVariant *ret;
  guint version = 0;

  picker->portal_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        NULL,
                                                        PORTAL_BUS_NAME,
                                                        PORTAL_OBJECT_PATH,
                                                        PORTAL_SCREENSHOT_INTERFACE,
                                                        NULL,
                                                        error);

  if (picker->portal_proxy == NULL)
    {
      g_debug ("Failed to create screenshot portal proxy");
      return FALSE;
    }

  owner = g_dbus_proxy_get_name_owner (picker->portal_proxy);
  if (owner == NULL)
    {
      g_debug ("%s not provided", PORTAL_SCREENSHOT_INTERFACE);
      g_clear_object (&picker->portal_proxy);
      return FALSE;
    }
  g_free (owner);

  ret = g_dbus_proxy_get_cached_property (picker->portal_proxy, "version");
  if (ret)
    {
      version = g_variant_get_uint32 (ret);
      g_variant_unref (ret);
    }

  if (version != 2)
    {
      g_debug ("Screenshot portal version: %u", version);
      g_clear_object (&picker->portal_proxy);
      return FALSE;
    }

  return TRUE;
}

static void
gtk_color_picker_portal_initable_iface_init (GInitableIface *iface)
{
  initable_parent_iface = g_type_interface_peek_parent (iface);
  iface->init = gtk_color_picker_portal_initable_init;
}

static void
gtk_color_picker_portal_init (GtkColorPickerPortal *picker)
{
}

static void
gtk_color_picker_portal_finalize (GObject *object)
{
  GtkColorPickerPortal *picker = GTK_COLOR_PICKER_PORTAL (object);

  g_clear_object (&picker->portal_proxy);

  G_OBJECT_CLASS (gtk_color_picker_portal_parent_class)->finalize (object);
}

static void
gtk_color_picker_portal_class_init (GtkColorPickerPortalClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_color_picker_portal_finalize;
}

GtkColorPicker *
gtk_color_picker_portal_new (void)
{
  return GTK_COLOR_PICKER (g_initable_new (GTK_TYPE_COLOR_PICKER_PORTAL, NULL, NULL, NULL));
}

static void
portal_response_received (GDBusConnection *connection,
                          const char      *sender_name,
                          const char      *object_path,
                          const char      *interface_name,
                          const char      *signal_name,
                          GVariant        *parameters,
                          gpointer         user_data)
{
  GtkColorPickerPortal *picker = user_data;
  guint32 response;
  GVariant *ret;

  g_dbus_connection_signal_unsubscribe (connection, picker->portal_signal_id);
  picker->portal_signal_id = 0;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      double d1, d2, d3;

      if (g_variant_lookup (ret, "color", "(ddd)", &d1, &d2, &d3))
        g_task_return_pointer (picker->task,
                               gdk_rgba_copy (&(GdkRGBA){d1, d2, d3, 1.0f}), (GDestroyNotify)gdk_rgba_free);
      else
        g_task_return_new_error (picker->task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No color received");
    }
  else
    g_task_return_new_error (picker->task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "PickColor error");

  g_variant_unref (ret);

  g_clear_object (&picker->task);
}

static void
gtk_color_picker_portal_pick (GtkColorPicker      *cp,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GtkColorPickerPortal *picker = GTK_COLOR_PICKER_PORTAL (cp);
  GVariantBuilder options;
  GDBusConnection *connection;
  char *token;
  char *handle;

  if (picker->task)
    return;

  picker->task = g_task_new (picker, NULL, callback, user_data);

  connection = g_dbus_proxy_get_connection (picker->portal_proxy);

  handle = gtk_get_portal_request_path (connection, &token);
  picker->portal_signal_id = g_dbus_connection_signal_subscribe (connection,
                                                                 PORTAL_BUS_NAME,
                                                                 PORTAL_REQUEST_INTERFACE,
                                                                 "Response",
                                                                 handle,
                                                                 NULL,
                                                                 G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                                 portal_response_received,
                                                                 picker,
                                                                 NULL);

  g_free (handle);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_free (token);

  g_dbus_proxy_call (picker->portal_proxy,
                     "PickColor",
                     g_variant_new ("(sa{sv})", "", &options),
                     0,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static GdkRGBA *
gtk_color_picker_portal_pick_finish (GtkColorPicker  *cp,
                                     GAsyncResult    *res,
                                     GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (res, cp), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
gtk_color_picker_portal_iface_init (GtkColorPickerInterface *iface)
{
  iface->pick = gtk_color_picker_portal_pick;
  iface->pick_finish = gtk_color_picker_portal_pick_finish;
}
