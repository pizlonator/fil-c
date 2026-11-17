/*
 * Copyright © 2019 Christian Persch
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

#pragma once

#include "vte/vtepty.h"
#include "pty.hh"

vte::base::Pty* _vte_pty_get_impl(VtePty* pty);

bool _vte_pty_spawn_sync(VtePty* pty,
                         char const* working_directory,
                         char const* const* argv,
                         char const* const* envv,
                         GSpawnFlags spawn_flags,
                         GSpawnChildSetupFunc child_setup,
                         gpointer child_setup_data,
                         GDestroyNotify child_setup_data_destroy,
                         GPid* child_pid /* out */,
                         int timeout,
                         GCancellable* cancellable,
                         GError** error) noexcept;

bool _vte_pty_check_envv(char const* const* envv) noexcept;

bool _vte_pty_set_size(VtePty *pty,
                       int rows,
                       int columns,
                       int cell_height_px,
                       int cell_width_px,
                       GError **error) noexcept;
