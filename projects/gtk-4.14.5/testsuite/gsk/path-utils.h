/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

void assert_path_equal_func (const char *domain,
                             const char *file,
                             int         line,
                             const char *func,
                             GskPath    *path1,
                             GskPath    *path2,
                             float       epsilon);

#define assert_path_equal(p1,p2) assert_path_equal_func(G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, (p1),(p2), FLOAT_EPSILON)
#define assert_path_equal_with_epsilon(p1,p2, epsilon) \
    assert_path_equal_func(G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, (p1),(p2), (epsilon))

