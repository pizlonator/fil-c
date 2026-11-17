/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 * Copyright © 2019, 2020 Christian Persch
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

#include "config.h"

#include "bidi.hh"
#include "debug.h"
#include "drawing-cairo.hh"
#include "fonts-pangocairo.hh"

/* cairo_show_glyphs accepts runs up to 102 glyphs before it allocates a
 * temporary array.
 *
 * Setting this to a large value can cause dramatic slow-downs for some
 * xservers (notably fglrx), see bug #410534.
 */
#define MAX_RUN_LENGTH 100

namespace vte {
namespace view {

void
DrawingCairo::set_cairo(cairo_t* cr) noexcept
{
        m_cr = cr;
}

void
DrawingCairo::clip(Rectangle const* rect) const
{
        g_assert(m_cr);
        g_assert(rect);

        cairo_save(m_cr);
        rect->path(m_cr);
        cairo_clip(m_cr);
}

void
DrawingCairo::unclip() const
{
        g_assert(m_cr);

        cairo_restore(m_cr);
}

void
DrawingCairo::translate(double x, double y) const
{
        cairo_save(m_cr);
        cairo_translate(m_cr, x, y);
}

void
DrawingCairo::untranslate() const
{
        cairo_restore(m_cr);
}

void
DrawingCairo::clear(int x,
                    int y,
                    int width,
                    int height,
                    vte::color::rgb const* color,
                    double alpha) const
{
        g_assert(m_cr);

        cairo_rectangle(m_cr, x, y, width, height);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_SOURCE);
        _vte_set_source_color_alpha(m_cr, color, alpha);
        cairo_fill(m_cr);
}

void
DrawingCairo::fill_rectangle(int x,
                             int y,
                             int width,
                             int height,
                             vte::color::rgb const* color) const
{
        g_assert(m_cr);
        g_assert(color);

        _vte_debug_print(VTE_DEBUG_DRAW,
                         "draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d))\n",
                         x,y,width,height,
                         color->red, color->green, color->blue);

        cairo_save(m_cr);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
        cairo_rectangle(m_cr, x, y, width, height);
        _vte_set_source_color(m_cr, color);
        cairo_fill(m_cr);
        cairo_restore(m_cr);
}

void
DrawingCairo::fill_rectangle(int x,
                             int y,
                             int width,
                             int height,
                             vte::color::rgb const* color,
                             double alpha) const
{
        g_assert(m_cr);
        g_assert(color);

        _vte_debug_print(VTE_DEBUG_DRAW,
                         "draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%.3f))\n",
                         x,y,width,height,
                         color->red, color->green, color->blue, alpha);

        cairo_save(m_cr);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
        cairo_rectangle(m_cr, x, y, width, height);
        _vte_set_source_color_alpha(m_cr, color, alpha);
        cairo_fill(m_cr);
        cairo_restore(m_cr);
}

void
DrawingCairo::draw_text(TextRequest* requests,
                        gsize n_requests,
                        uint32_t attr,
                        vte::color::rgb const* color)
{
        gsize i;
        cairo_scaled_font_t *last_scaled_font = nullptr;
        int n_cr_glyphs = 0;
        cairo_glyph_t cr_glyphs[MAX_RUN_LENGTH];
        auto font = m_fonts[attr_to_style(attr)];

        g_return_if_fail (font != nullptr);

        if (n_requests == 0)
                return;

        g_assert(m_cr);

        _vte_set_source_color(m_cr, color);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);

        for (i = 0; i < n_requests; i++) {
                vteunistr c = requests[i].c;

                if (G_UNLIKELY (requests[i].mirror)) {
                        vte_bidi_get_mirror_char (c, requests[i].box_mirror, &c);
                }

                if (Minifont::unistr_is_local_graphic(c)) {
                        m_minifont.draw_graphic(cairo(),
                                                c,
                                                color,
                                                cell_width(),
                                                cell_height(),
                                                requests[i].x, requests[i].y,
                                                font->width(), requests[i].columns, font->height(),
                                                scale_factor());
                        continue;
                }

                auto uinfo = font->get_unistr_info(c);
                auto ufi = &uinfo->m_ufi;
                int x, y, ye;

                get_char_edges(c, requests[i].columns, attr, x, ye /* unused */);
                x += requests[i].x;
                /* Bold/italic versions might have different ascents. In order to align their
                 * baselines, we offset by the normal font's ascent here. (Issue #137.) */
                y = requests[i].y + m_char_spacing.top + m_fonts[VTE_DRAW_NORMAL]->ascent();

                switch (uinfo->coverage()) {
                default:
                case FontInfo::UnistrInfo::Coverage::UNKNOWN:
                        break;
                case FontInfo::UnistrInfo::Coverage::USE_PANGO_LAYOUT_LINE:
                        cairo_move_to(m_cr, x, y);
                        pango_cairo_show_layout_line(m_cr,
                                                      ufi->using_pango_layout_line.line);
                        break;
                case FontInfo::UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING:
                        cairo_move_to(m_cr, x, y);
                        pango_cairo_show_glyph_string(m_cr,
                                                       ufi->using_pango_glyph_string.font,
                                                       ufi->using_pango_glyph_string.glyph_string);
                        break;
                case FontInfo::UnistrInfo::Coverage::USE_CAIRO_GLYPH:
                        if (last_scaled_font != ufi->using_cairo_glyph.scaled_font || n_cr_glyphs == MAX_RUN_LENGTH) {
                                if (n_cr_glyphs) {
                                        cairo_set_scaled_font(m_cr, last_scaled_font);
                                        cairo_show_glyphs(m_cr,
                                                           cr_glyphs,
                                                           n_cr_glyphs);
                                        n_cr_glyphs = 0;
                                }
                                last_scaled_font = ufi->using_cairo_glyph.scaled_font;
                        }
                        cr_glyphs[n_cr_glyphs].index = ufi->using_cairo_glyph.glyph_index;
                        cr_glyphs[n_cr_glyphs].x = x;
                        cr_glyphs[n_cr_glyphs].y = y;
                        n_cr_glyphs++;
                        break;
                }
        }
        if (n_cr_glyphs) {
                cairo_set_scaled_font(m_cr, last_scaled_font);
                cairo_show_glyphs(m_cr,
                                   cr_glyphs,
                                   n_cr_glyphs);
                n_cr_glyphs = 0;
        }
}

void
DrawingCairo::draw_rectangle(int x,
                             int y,
                             int width,
                             int height,
                             vte::color::rgb const* color) const
{
        g_assert(color);
        g_assert(m_cr);

        _vte_debug_print (VTE_DEBUG_DRAW,
                          "draw_rectangle (%d, %d, %d, %d, color=(%d,%d,%d))\n",
                          x,y,width,height,
                          color->red, color->green, color->blue);

        cairo_save(m_cr);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
        cairo_rectangle(m_cr, x+VTE_LINE_WIDTH/2., y+VTE_LINE_WIDTH/2., width-VTE_LINE_WIDTH, height-VTE_LINE_WIDTH);
        _vte_set_source_color(m_cr, color);
        cairo_set_line_width(m_cr, VTE_LINE_WIDTH);
        cairo_stroke (m_cr);
        cairo_restore(m_cr);
}

cairo_t*
DrawingCairo::begin_cairo(int x,
                          int y,
                          int width,
                          int height) const
{
        cairo_save(m_cr);
        return m_cr;
}

void
DrawingCairo::end_cairo(cairo_t *cr) const
{
        cairo_restore(cr);
}

void
DrawingCairo::draw_surface_with_color_mask(cairo_surface_t *surface,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           vte::color::rgb const* color) const
{
        auto cr = begin_cairo(x, y, width, height);

        _vte_set_source_color(m_cr, color);

        cairo_push_group(cr);
        cairo_rectangle(cr, x, y, width, height);
        cairo_fill(cr);
        cairo_pop_group_to_source(cr);
        cairo_mask_surface(cr, surface, x, y);

        end_cairo(cr);
}

} // namespace view
} // namespace vte
