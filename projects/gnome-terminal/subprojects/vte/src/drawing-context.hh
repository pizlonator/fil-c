/*
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright © 2020 Christian Persch
 * Copyright © 2023 Christian Hergert
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

#include <memory>

#include <cairo.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "attr.hh"
#include "cairo-glue.hh"
#include "fwd.hh"
#include "minifont.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

#define VTE_DRAW_NORMAL      0
#define VTE_DRAW_BOLD        1
#define VTE_DRAW_ITALIC      2
#define VTE_DRAW_BOLD_ITALIC 3

namespace vte {
namespace view {

struct Rectangle {
private:
#if VTE_GTK == 3
        cairo_rectangle_int_t m_rect;
#elif VTE_GTK == 4
        graphene_rect_t m_rect;
#endif

public:
        constexpr Rectangle() :
                Rectangle{0, 0, 0, 0}
        {
        }

        constexpr Rectangle(cairo_rectangle_int_t const *rect) :
                Rectangle(rect->x, rect->y, rect->width, rect->height)
        {
        }

#if VTE_GTK == 4
        constexpr Rectangle(graphene_rect_t const *rect) :
                Rectangle(rect->origin.x, rect->origin.y,
                          rect->size.width, rect->size.height)
        {
        }
#endif

        constexpr Rectangle(float x, float y, float w, float h) {
#if VTE_GTK == 3
                m_rect.x = int(x);
                m_rect.y = int(y);
                m_rect.width = int(w);
                m_rect.height = int(h);
#elif VTE_GTK == 4
                m_rect.origin.x = x;
                m_rect.origin.y = y;
                m_rect.size.width = w;
                m_rect.size.height = h;
#endif
        }

        constexpr Rectangle(int x, int y, int w, int h) {
#if VTE_GTK == 3
                m_rect.x = x;
                m_rect.y = y;
                m_rect.width = w;
                m_rect.height = h;
#elif VTE_GTK == 4
                m_rect.origin.x = float(x);
                m_rect.origin.y = float(y);
                m_rect.size.width = float(w);
                m_rect.size.height = float(h);
#endif
        }

        inline constexpr void advance_y (int by) {
#if VTE_GTK == 3
                m_rect.y += by;
#elif VTE_GTK == 4
                m_rect.origin.y += by;
#endif
        }

        inline constexpr void move_y (int y) {
#if VTE_GTK == 3
                m_rect.y = y;
#elif VTE_GTK == 4
                m_rect.origin.y = y;
#endif
        }

        inline void path(cairo_t *cr) const {
#if VTE_GTK == 3
                cairo_rectangle(cr, m_rect.x, m_rect.y, m_rect.width, m_rect.height);
#elif VTE_GTK == 4
                cairo_rectangle(cr, m_rect.origin.x, m_rect.origin.y,
                                m_rect.size.width, m_rect.size.height);
#endif
        }

#if VTE_GTK == 3
        inline constexpr cairo_rectangle_int_t const* cairo() const { return &m_rect; }
#elif VTE_GTK == 4
        inline constexpr graphene_rect_t const* graphene() const { return &m_rect; }
        inline constexpr cairo_rectangle_int_t cairo() const {
                return cairo_rectangle_int_t{
                        int(m_rect.origin.x),
                        int(m_rect.origin.y),
                        int(m_rect.size.width),
                        int(m_rect.size.height)
                };
        }
#endif
};

class DrawingContext {
public:

        /* A request to draw a particular character spanning a given number of columns
           at the given location.  Unlike most APIs, (x,y) specifies the top-left
           corner of the cell into which the character will be drawn instead of the
           left end of the baseline. */
        struct TextRequest {
                vteunistr c;
                int16_t x, y, columns;

                /* Char has RTL resolved directionality, mirror if mirrorable. */
                uint8_t mirror : 1;

                /* Add box drawing chars to the set of mirrorable characters. */
                uint8_t box_mirror : 1;
        };

        DrawingContext() noexcept = default;
        virtual ~DrawingContext();

        DrawingContext(DrawingContext const&) = delete;
        DrawingContext(DrawingContext&&) = delete;
        DrawingContext& operator=(DrawingContext const&) = delete;
        DrawingContext& operator=(DrawingContext&&) = delete;

        virtual cairo_t* begin_cairo(int x,
                                     int y,
                                     int width,
                                     int height) const = 0;
        virtual void end_cairo(cairo_t *cr) const = 0;

        virtual void clip(Rectangle const* rect) const = 0;
        virtual void unclip() const = 0;

        // Clipping for widget border is kept separate from general
        // clipping because GSK and Cairo need to do separate things.
        virtual void clip_border(Rectangle const* rect) const { clip(rect); }
        virtual void unclip_border() const { unclip(); }

        virtual void translate(double x,
                               double y) const = 0;
        virtual void untranslate() const = 0;

        virtual void clear(int x,
                           int y,
                           int width,
                           int height,
                           vte::color::rgb const* color,
                           double alpha) const = 0;
        virtual void fill_rectangle(int x,
                                    int y,
                                    int width,
                                    int height,
                                    vte::color::rgb const* color) const = 0;
        virtual void fill_rectangle(int x,
                                    int y,
                                    int width,
                                    int height,
                                    vte::color::rgb const* color,
                                    double alpha) const = 0;
        virtual void draw_rectangle(int x,
                                    int y,
                                    int width,
                                    int height,
                                    vte::color::rgb const* color) const = 0;

        virtual void begin_background(Rectangle const& rect,
                                      size_t columns,
                                      size_t rows) = 0;
        virtual void fill_cell_background(size_t column,
                                          size_t row,
                                          size_t n_colums,
                                          vte::color::rgb const* color) = 0;
        virtual void flush_background(Rectangle const& rect) = 0;

        virtual void draw_surface_with_color_mask(
#if VTE_GTK == 3
                                                  cairo_surface_t *surface,
#elif VTE_GTK == 4
                                                  GdkTexture *texture,
#endif
                                                  int x,
                                                  int y,
                                                  int width,
                                                  int height,
                                                  vte::color::rgb const* color) const = 0;

        void draw_undercurl(int x,
                            double y,
                            double line_width,
                            int count,
                            int scale_factor,
                            vte::color::rgb const* color);

        void clear_font_cache();
        void set_text_font(GtkWidget* widget,
                           PangoFontDescription const* fontdesc,
                           cairo_font_options_t const* font_options,
                           double cell_width_scale,
                           double cell_height_scale);
        void get_text_metrics(int* cell_width,
                              int* cell_height,
                              int* char_ascent,
                              int* char_descent,
                              GtkBorder* char_spacing);
        void get_char_edges(vteunistr c,
                            int columns,
                            uint32_t attr,
                            int& left,
                            int& right);
        virtual void draw_text(TextRequest* requests,
                               gsize n_requests,
                               uint32_t attr,
                               vte::color::rgb const* color) = 0;
        void draw_line(int x,
                       int y,
                       int xp,
                       int yp,
                       int line_width,
                       vte::color::rgb const *color);

        auto cell_width()  const noexcept { return m_cell_width; }
        auto cell_height() const noexcept { return m_cell_height; }

        inline auto scale_factor() const noexcept { return m_scale_factor; }
        inline void set_scale_factor(int scale_factor) { m_scale_factor = scale_factor; }

protected:

        // std::array<vte::base::RefPtr<FontInfo>, 4> m_fonts{};
        FontInfo* m_fonts[4]{nullptr, nullptr, nullptr, nullptr};
        int m_cell_width{1};
        int m_cell_height{1};
        int m_scale_factor{1};
        GtkBorder m_char_spacing{1, 1, 1, 1};

        /* Cache the undercurl's rendered look. */
        vte::Freeable<cairo_surface_t> m_undercurl_surface{};
        int m_undercurl_surface_scale{0};
}; // class DrawingContext

} // namespace view
} // namespace vte

class _vte_draw_autoclip_t {
private:
        vte::view::DrawingContext& m_draw;
public:
        _vte_draw_autoclip_t(vte::view::DrawingContext& draw,
                             vte::view::Rectangle const* rect)
                : m_draw{draw}
        {
                m_draw.clip(rect);
        }

        ~_vte_draw_autoclip_t()
        {
                m_draw.unclip();
        }
};

static inline unsigned
attr_to_style(uint32_t attr)
{
	auto style = unsigned{0};
	if (attr & VTE_ATTR_BOLD)
		style |= VTE_DRAW_BOLD;
	if (attr & VTE_ATTR_ITALIC)
		style |= VTE_DRAW_ITALIC;
	return style;
}

static inline constexpr double
_vte_draw_get_undercurl_rad(gint width)
{
        return width / 2. / G_SQRT2;
}

static inline constexpr double
_vte_draw_get_undercurl_arc_height(gint width)
{
        return _vte_draw_get_undercurl_rad(width) * (1. - G_SQRT2 / 2.);
}

static inline double
_vte_draw_get_undercurl_height(gint width, double line_width)
{
        return 2. * _vte_draw_get_undercurl_arc_height(width) + line_width;
}

static inline void
_vte_set_source_color(cairo_t* cr,
                      vte::color::rgb const *color)
{
        cairo_set_source_rgba(cr,
                              color->red / 65535.,
                              color->green / 65535.,
                              color->blue / 65535.,
                              1.0);
}

static inline void
_vte_set_source_color_alpha(cairo_t* cr,
                            vte::color::rgb const *color,
                            double alpha)
{
        cairo_set_source_rgba(cr,
                              color->red / 65535.,
                              color->green / 65535.,
                              color->blue / 65535.,
                              alpha);
}
