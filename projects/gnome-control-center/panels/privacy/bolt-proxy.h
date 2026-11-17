/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct BoltProxySignal
{

  const char *theirs;
  void (*handle)(GObject    *self,
                 GDBusProxy *bus_proxy,
                 GVariant   *params);

} BoltProxySignal;

#define BOLT_TYPE_PROXY (bolt_proxy_get_type ())
G_DECLARE_DERIVABLE_TYPE (BoltProxy, bolt_proxy, BOLT, PROXY, GDBusProxy)

struct _BoltProxyClass
{
  GDBusProxyClass parent;

  /* virtuals */
  const BoltProxySignal * (*get_dbus_signals) (guint *n);
};

gboolean          bolt_proxy_get_dbus_property (GObject    *proxy,
                                                GParamSpec *spec,
                                                GValue     *value);

gboolean          bolt_proxy_has_name_owner (BoltProxy *proxy);

gboolean          bolt_proxy_get_property_bool (BoltProxy  *proxy,
                                                const char *name,
                                                gboolean   *value);

gboolean          bolt_proxy_get_property_enum (BoltProxy  *proxy,
                                                const char *name,
                                                gint       *value);

gboolean          bolt_proxy_get_property_flags (BoltProxy  *proxy,
                                                 const char *name,
                                                 guint      *value);

gboolean          bolt_proxy_get_property_uint32 (BoltProxy  *proxy,
                                                  const char *name,
                                                  guint      *value);

gboolean          bolt_proxy_get_property_int64 (BoltProxy  *proxy,
                                                 const char *name,
                                                 gint64     *value);

gboolean          bolt_proxy_get_property_uint64 (BoltProxy  *proxy,
                                                  const char *name,
                                                  guint64    *value);

const char *      bolt_proxy_get_property_string (BoltProxy  *proxy,
                                                  const char *name);

gboolean          bolt_proxy_set_property (BoltProxy    *proxy,
                                           const char   *name,
                                           GVariant     *value,
                                           GCancellable *cancellable,
                                           GError      **error);

void              bolt_proxy_set_property_async (BoltProxy          *proxy,
                                                 const char         *name,
                                                 GVariant           *value,
                                                 GCancellable       *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer            user_data);

gboolean         bolt_proxy_set_property_finish (GAsyncResult *res,
                                                 GError      **error);

G_END_DECLS
