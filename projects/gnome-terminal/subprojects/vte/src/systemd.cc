/*
 * Copyright © 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "systemd.hh"

#include <memory>

#include <systemd/sd-login.h>

#include "glib-glue.hh"
#include "refptr.hh"

namespace vte::systemd {

bool
create_scope_for_pid_sync(pid_t pid,
                          int timeout,
                          GCancellable* cancellable,
                          GError** error)
{
        auto const parent_pid = getpid();

        {
                char* unit = nullptr;
                if (auto r = sd_pid_get_user_unit(parent_pid, &unit) < 0) {
                        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(-r),
                                    "Failed sd_pid_get_user_unit(%d): %s",
                                    pid,
                                    g_strerror(-r));
                        return false;
                }
                free(unit);
        }

        auto bus = vte::glib::take_ref(g_bus_get_sync(G_BUS_TYPE_SESSION, cancellable, error));
        if (!bus)
                return false;

        auto uuid = vte::glib::take_string(g_uuid_string_random());
        auto scope = vte::glib::take_string(g_strdup_printf("vte-spawn-%s.scope", uuid.get()));
        auto prgname = vte::glib::take_string(g_utf8_make_valid(g_get_prgname(), -1));
        auto description = vte::glib::take_string(g_strdup_printf("VTE child process %d launched by %s process %d", pid, prgname.get(), getpid()));

        auto builder_stack = GVariantBuilder{};
        auto builder = &builder_stack;
        g_variant_builder_init(builder, G_VARIANT_TYPE("(ssa(sv)a(sa(sv)))"));

        g_variant_builder_add(builder, "s", scope.get()); // unit name
        g_variant_builder_add(builder, "s", "fail");      // failure mode

        // Unit properties
        g_variant_builder_open(builder, G_VARIANT_TYPE("a(sv)"));

        g_variant_builder_add(builder, "(sv)", "Description", g_variant_new_string(description.get()));

        g_variant_builder_open(builder, G_VARIANT_TYPE("(sv)"));
        g_variant_builder_add(builder, "s", "PIDs");
        g_variant_builder_open(builder, G_VARIANT_TYPE("v"));
        g_variant_builder_open(builder, G_VARIANT_TYPE("au"));
        g_variant_builder_add(builder, "u", unsigned(pid));
        g_variant_builder_close(builder); // au
        g_variant_builder_close(builder); // v
        g_variant_builder_close(builder); // (sv)

        char* slice = nullptr;
        if (sd_pid_get_user_slice(parent_pid, &slice) >= 0) {
                g_variant_builder_add(builder, "(sv)", "Slice", g_variant_new_string(slice));
                free(slice);
        } else {
                // Fallback
                g_variant_builder_add(builder, "(sv)", "Slice", g_variant_new_string("app-org.gnome.vte.slice"));
        }

        g_variant_builder_close(builder); // a(sv)

        // No auxiliary units
        g_variant_builder_open(builder, G_VARIANT_TYPE("a(sa(sv))"));
        g_variant_builder_close(builder);

        // Create transient scope
        auto reply = vte::take_freeable
                (g_dbus_connection_call_sync(bus.get(),
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "StartTransientUnit",
                                             g_variant_builder_end(builder), // parameters
                                             G_VARIANT_TYPE("(o)"), // reply type,
                                             GDBusCallFlags{G_DBUS_CALL_FLAGS_NO_AUTO_START},
                                             timeout, // in ms
                                             cancellable,
                                             error));

        return bool(reply);
}

} // namespace vte::systemd
