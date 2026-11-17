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

#pragma once

#include <cairo.h>

#include "std-glue.hh"

namespace vte {

VTE_DECLARE_FREEABLE(cairo_font_options_t, cairo_font_options_destroy);
VTE_DECLARE_FREEABLE(cairo_pattern_t, cairo_pattern_destroy);
VTE_DECLARE_FREEABLE(cairo_rectangle_list_t, cairo_rectangle_list_destroy);
VTE_DECLARE_FREEABLE(cairo_region_t, cairo_region_destroy);
VTE_DECLARE_FREEABLE(cairo_surface_t, cairo_surface_destroy);
VTE_DECLARE_FREEABLE(cairo_t, cairo_destroy);

} // namespace vte
