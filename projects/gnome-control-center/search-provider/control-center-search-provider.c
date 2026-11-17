/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include <gio/gio.h>

#include <shell/cc-panel-loader.h>
#include <shell/cc-shell-model.h>
#include "cc-search-provider.h"
#include "control-center-search-provider.h"

G_DEFINE_TYPE (CcSearchProviderApp, cc_search_provider_app, GTK_TYPE_APPLICATION);

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in milliseconds */

static gboolean
cc_search_provider_app_dbus_register (GApplication    *application,
                                      GDBusConnection *connection,
                                      const gchar     *object_path,
                                      GError         **error)
{
  CcSearchProviderApp *self;

  if (!G_APPLICATION_CLASS (cc_search_provider_app_parent_class)->dbus_register (application,
                                                                                   connection,
                                                                                   object_path,
                                                                                   error))
    return FALSE;

  self = CC_SEARCH_PROVIDER_APP (application);

  return cc_search_provider_dbus_register (self->search_provider, connection,
                                           object_path, error);
}

static void
cc_search_provider_app_dbus_unregister (GApplication    *application,
                                        GDBusConnection *connection,
                                        const gchar     *object_path)
{
  CcSearchProviderApp *self;

  self = CC_SEARCH_PROVIDER_APP (application);
  if (self->search_provider)
    cc_search_provider_dbus_unregister (self->search_provider, connection, object_path);

  G_APPLICATION_CLASS (cc_search_provider_app_parent_class)->dbus_unregister (application,
                                                                              connection,
                                                                              object_path);
}

static void
cc_search_provider_app_dispose (GObject *object)
{
  CcSearchProviderApp *self;

  self = CC_SEARCH_PROVIDER_APP (object);

  g_clear_object (&self->model);
  g_clear_object (&self->search_provider);

  G_OBJECT_CLASS (cc_search_provider_app_parent_class)->dispose (object);
}

static void
cc_search_provider_app_init (CcSearchProviderApp *self)
{
  self->search_provider = cc_search_provider_new ();
  g_application_set_inactivity_timeout (G_APPLICATION (self),
                                        INACTIVITY_TIMEOUT);

  /* HACK: get the inactivity timeout started */
  g_application_hold (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}

static void
cc_search_provider_app_startup (GApplication *application)
{
  CcSearchProviderApp *self;

  self = CC_SEARCH_PROVIDER_APP (application);

  G_APPLICATION_CLASS (cc_search_provider_app_parent_class)->startup (application);

  self->model = cc_shell_model_new ();
  cc_panel_loader_fill_model (self->model);
}

static void
cc_search_provider_app_class_init (CcSearchProviderAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = cc_search_provider_app_dispose;

  app_class->dbus_register = cc_search_provider_app_dbus_register;
  app_class->dbus_unregister = cc_search_provider_app_dbus_unregister;
  app_class->startup = cc_search_provider_app_startup;
}

CcShellModel *
cc_search_provider_app_get_model (CcSearchProviderApp *application)
{
  return application->model;
}

CcSearchProviderApp *
cc_search_provider_app_get ()
{
  static CcSearchProviderApp *singleton;

  if (singleton)
    return singleton;

  singleton = g_object_new (CC_TYPE_SEARCH_PROVIDER_APP,
                            "application-id", "org.gnome.Settings.SearchProvider",
                            "flags", G_APPLICATION_IS_SERVICE,
                            NULL);

  return singleton;
}

int main (int argc, char **argv)
{
  GApplication *app;

  app = G_APPLICATION (cc_search_provider_app_get ());
  return g_application_run (app, argc, argv);
}
