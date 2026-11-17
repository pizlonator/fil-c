/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TERMINAL_SEARCH_PROVIDER_H
#define TERMINAL_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_SEARCH_PROVIDER              (terminal_search_provider_get_type ())
#define TERMINAL_SEARCH_PROVIDER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_SEARCH_PROVIDER, TerminalSearchProvider))
#define TERMINAL_SEARCH_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_SEARCH_PROVIDER, TerminalSearchProviderClass))
#define TERMINAL_IS_SEARCH_PROVIDER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_SEARCH_PROVIDER))
#define TERMINAL_IS_SEARCH_PROVIDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_SEARCH_PROVIDER))
#define TERMINAL_SEARCH_PROVIDER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_SEARCH_PROVIDER, TerminalSearchProviderClass))

typedef struct _TerminalSearchProvider      TerminalSearchProvider;
typedef struct _TerminalSearchProviderClass TerminalSearchProviderClass;

GType terminal_search_provider_get_type (void);

TerminalSearchProvider *terminal_search_provider_new (void);

gboolean terminal_search_provider_dbus_register (TerminalSearchProvider  *provider,
                                                 GDBusConnection         *connection,
                                                 const char              *object_path,
                                                 GError                 **error);

void terminal_search_provider_dbus_unregister (TerminalSearchProvider  *provider,
                                               GDBusConnection         *connection,
                                               const char              *object_path);

G_END_DECLS

#endif /* !TERMINAL_SEARCH_PROVIDER_H */
