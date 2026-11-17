/*
 * Copyright © 2020 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cairo.h>
#include <graphene.h>

#include "std-glue.hh"

namespace vte::graphene {

inline constexpr auto
make_rect(int x,
          int y,
          int width,
          int height)
{
        return GRAPHENE_RECT_INIT(float(x), float(y), float(width), float(height));
}

inline constexpr auto
make_rect(cairo_rectangle_int_t const* rect)
{
        return make_rect(rect->x, rect->y, rect->width, rect->height);
}

} // namespace vte::graphene

namespace vte {

// VTE_DECLARE_FREEABLE(graphene_rect_t, graphene_rect_free);

} // namespace vte::cairo
