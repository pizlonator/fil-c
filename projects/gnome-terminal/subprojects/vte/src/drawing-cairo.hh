/*
 * Copyright (C) 2003 Red Hat, Inc.
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

#include "drawing-context.hh"
#include "minifont.hh"

namespace vte {
namespace view {

class DrawingCairo final : public DrawingContext {
public:
        DrawingCairo() noexcept = default;
        ~DrawingCairo() noexcept override = default;

        DrawingCairo(DrawingCairo const&) = delete;
        DrawingCairo(DrawingCairo&&) = delete;
        DrawingCairo& operator=(DrawingCairo const&) = delete;
        DrawingCairo& operator=(DrawingCairo&&) = delete;

        cairo_t* begin_cairo(int x,
                             int y,
                             int width,
                             int height) const override;
        void end_cairo(cairo_t *cr) const override;

        void clip(Rectangle const* rect) const override;
        void unclip() const override;

        void translate(double x,
                       double y) const override;
        void untranslate() const override;

        void clear(int x,
                   int y,
                   int width,
                   int height,
                   vte::color::rgb const* color,
                   double alpha) const override;
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color) const override;
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color,
                            double alpha) const override;
        void draw_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color) const override;

        void set_cairo(cairo_t* cr) noexcept;

        constexpr cairo_t* cairo() const noexcept { return m_cr; }

        void draw_surface_with_color_mask(cairo_surface_t *surface,
                                          int x,
                                          int y,
                                          int width,
                                          int height,
                                          vte::color::rgb const* color) const override;

        void draw_text(TextRequest* requests,
                       gsize n_requests,
                       uint32_t attr,
                       vte::color::rgb const* color) override;

        inline void fill_cell_background(size_t column,
                                         size_t row,
                                         size_t n_colums,
                                         vte::color::rgb const* color) override {
                fill_rectangle(column * cell_width(),
                               row * cell_height(),
                               cell_width() * n_colums,
                               cell_height(),
                               color);
        }
        inline void begin_background(Rectangle const& rect,
                                     size_t columns,
                                     size_t rows) override
        {
                cairo_save(m_cr);
                cairo_translate(m_cr, rect.cairo()->x, rect.cairo()->y);
        }

        inline void flush_background(Rectangle const& rect) override
        {
                cairo_restore(m_cr);
        }

private:
        cairo_t *m_cr{nullptr}; // unowned

        Minifont m_minifont{};
};

} // namespace view
} // namespace vte
