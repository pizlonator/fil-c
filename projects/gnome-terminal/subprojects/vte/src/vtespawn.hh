/* gspawn.h - Process launching
 *
 *  Copyright 2000 Red Hat, Inc.
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

#include <glib.h>

int _vte_execute(char const* file,
                 char** argv,
                 char** envp,
                 char const* path_env,
                 void* workbuf,
                 size_t workbufsize) noexcept;

void _vte_write_err (int fd,
                     int msg) noexcept;
