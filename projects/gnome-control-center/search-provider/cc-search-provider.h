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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include "cc-shell-search-provider-generated.h"
#include <shell/cc-shell-model.h>

G_BEGIN_DECLS

#define CC_TYPE_SEARCH_PROVIDER (cc_search_provider_get_type())

G_DECLARE_FINAL_TYPE (CcSearchProvider, cc_search_provider, CC, SEARCH_PROVIDER, GObject)

CcSearchProvider *cc_search_provider_new (void);

gboolean cc_search_provider_dbus_register   (CcSearchProvider  *provider,
                                             GDBusConnection   *connection,
                                             const char        *object_path,
                                             GError           **error);
void     cc_search_provider_dbus_unregister (CcSearchProvider  *provider,
                                             GDBusConnection   *connection,
                                             const char        *object_path);

G_END_DECLS
