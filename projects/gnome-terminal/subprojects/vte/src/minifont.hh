/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 * Copyright © 2019, 2020 Christian Persch
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

#include <cstdint>

#include "fwd.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

#include "cairo-glue.hh"
#include "drawing-context.hh"

namespace vte {
namespace view {

class Minifont {
public:

#include "minifont-coverage.inc"

        /* Draw the graphic representation of a line-drawing or special graphics
         * character.
         */
        void draw_graphic(cairo_t* cr,
                          vteunistr c,
                          vte::color::rgb const* fg,
                          int cell_width,
                          int cell_height,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height,
                          int scale_factor) const;

        void get_char_padding(vteunistr c,
                              int font_width,
                              int font_height,
                              int& xpad,
                              int& ypad) const noexcept;

        void get_char_offset(vteunistr c,
                             int x,
                             int y,
                             int& xoff,
                             int& yoff) const noexcept;

}; // class Minifont

class MinifontCache : private Minifont {
public:

        void draw_graphic(DrawingContext const& context,
                          vteunistr c,
                          vte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height,
                          int scale_factor);

private:

        vte::Freeable<cairo_t> begin_cairo(int x,
                                           int y,
                                           int width,
                                           int height,
                                           int xpad,
                                           int ypad,
                                           int scale_factor) const;

#if VTE_GTK == 4
        GdkTexture *surface_to_texture(cairo_t*cr) const;
#endif

}; // class MinifontCache

#if VTE_GTK == 4

class MinifontGsk : private MinifontCache {
public:

        void draw_graphic(DrawingContext const& context,
                          vteunistr c,
                          vte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height,
                          int scale_factor);

private:

        void rectangle(DrawingContext const& context,
                       vte::color::rgb const* fg,
                       double alpha,
                       double x,
                       double y,
                       double w,
                       double h,
                       int xdenom,
                       int ydenom,
                       int xb1,
                       int yb1,
                       int xb2,
                       int yb2) const;

}; // class MinifontGsk

#endif // VTE_GTK == 4

} // namespace view
} // namespace vte
