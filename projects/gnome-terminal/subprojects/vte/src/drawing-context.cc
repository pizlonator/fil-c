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

#include <cmath>

#include "bidi.hh"
#include "debug.h"
#include "drawing-context.hh"
#include "fonts-pangocairo.hh"

namespace vte {
namespace view {

DrawingContext::~DrawingContext()
{
        clear_font_cache();
}

void
DrawingContext::clear_font_cache()
{
        // m_fonts = {};

        for (auto style = int{0}; style < 4; ++style) {
                if (m_fonts[style] != nullptr)
                        m_fonts[style]->unref();
                m_fonts[style] = nullptr;
        }
}

void
DrawingContext::set_text_font(GtkWidget* widget,
                              PangoFontDescription const* fontdesc,
                              cairo_font_options_t const* font_options,
                              double cell_width_scale,
                              double cell_height_scale)
{
	PangoFontDescription *bolddesc   = nullptr;
	PangoFontDescription *italicdesc = nullptr;
	PangoFontDescription *bolditalicdesc = nullptr;
	gint normal, bold, ratio;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_set_text_font\n");

        clear_font_cache();

	/* calculate bold font desc */
	bolddesc = pango_font_description_copy (fontdesc);
        if (pango_font_description_get_set_fields(bolddesc) & PANGO_FONT_MASK_WEIGHT) {
                auto const weight = pango_font_description_get_weight(bolddesc);
                auto const bold_weight = std::min(1000, weight + VTE_FONT_WEIGHT_BOLDENING);
                pango_font_description_set_weight(bolddesc, PangoWeight(bold_weight));
        } else {
                pango_font_description_set_weight (bolddesc, PANGO_WEIGHT_BOLD);
        }

	/* calculate italic font desc */
	italicdesc = pango_font_description_copy (fontdesc);
	pango_font_description_set_style (italicdesc, PANGO_STYLE_ITALIC);

	/* calculate bold italic font desc */
	bolditalicdesc = pango_font_description_copy (bolddesc);
	pango_font_description_set_style (bolditalicdesc, PANGO_STYLE_ITALIC);

	m_fonts[VTE_DRAW_NORMAL]  = FontInfo::create_for_widget(widget, fontdesc, font_options);
	m_fonts[VTE_DRAW_BOLD]    = FontInfo::create_for_widget(widget, bolddesc, font_options);
	m_fonts[VTE_DRAW_ITALIC]  = FontInfo::create_for_widget(widget, italicdesc, font_options);
	m_fonts[VTE_DRAW_ITALIC | VTE_DRAW_BOLD] =
                FontInfo::create_for_widget(widget, bolditalicdesc, font_options);
	pango_font_description_free (bolddesc);
	pango_font_description_free (italicdesc);
	pango_font_description_free (bolditalicdesc);

	/* Decide if we should keep this bold font face, per bug 54926:
	 *  - reject bold font if it is not within 10% of normal font width
	 */
	normal = VTE_DRAW_NORMAL;
	bold   = normal | VTE_DRAW_BOLD;
	ratio = m_fonts[bold]->width() * 100 / m_fonts[normal]->width();
	if (abs(ratio - 100) > 10) {
		_vte_debug_print (VTE_DEBUG_DRAW,
			"Rejecting bold font (%i%%).\n", ratio);
                m_fonts[bold]->unref();
                m_fonts[bold] = m_fonts[normal]->ref();
	}
	normal = VTE_DRAW_ITALIC;
	bold   = normal | VTE_DRAW_BOLD;
	ratio = m_fonts[bold]->width() * 100 / m_fonts[normal]->width();
	if (abs(ratio - 100) > 10) {
		_vte_debug_print (VTE_DEBUG_DRAW,
			"Rejecting italic bold font (%i%%).\n", ratio);
                m_fonts[bold]->unref();
                m_fonts[bold] = m_fonts[normal]->ref();
	}

        /* Apply letter spacing and line spacing. */
        m_cell_width = m_fonts[VTE_DRAW_NORMAL]->width() * cell_width_scale;
        m_char_spacing.left = (m_cell_width - m_fonts[VTE_DRAW_NORMAL]->width()) / 2;
        m_char_spacing.right = (m_cell_width - m_fonts[VTE_DRAW_NORMAL]->width() + 1) / 2;
        m_cell_height = m_fonts[VTE_DRAW_NORMAL]->height() * cell_height_scale;
        m_char_spacing.top = (m_cell_height - m_fonts[VTE_DRAW_NORMAL]->height() + 1) / 2;
        m_char_spacing.bottom = (m_cell_height - m_fonts[VTE_DRAW_NORMAL]->height()) / 2;

        m_undercurl_surface.reset();
}

void
DrawingContext::get_text_metrics(int* cell_width,
                                 int* cell_height,
                                 int* char_ascent,
                                 int* char_descent,
                                 GtkBorder* char_spacing)
{
	g_return_if_fail (m_fonts[VTE_DRAW_NORMAL] != nullptr);

        if (cell_width)
                *cell_width = m_cell_width;
        if (cell_height)
                *cell_height = m_cell_height;
        if (char_ascent)
                *char_ascent = m_fonts[VTE_DRAW_NORMAL]->ascent();
        if (char_descent)
                *char_descent = m_fonts[VTE_DRAW_NORMAL]->height() - m_fonts[VTE_DRAW_NORMAL]->ascent();
        if (char_spacing)
                *char_spacing = m_char_spacing;
}

/* Stores the left and right edges of the given glyph, relative to the cell's left edge. */
void
DrawingContext::get_char_edges(vteunistr c,
                               int columns,
                               uint32_t attr,
                               int& left,
                               int& right)
{
        if (Minifont::unistr_is_local_graphic(c)) [[unlikely]] {
                left = 0;
                right = m_cell_width * columns;
                return;
        }

        int l, w, normal_width, fits_width;

        if (G_UNLIKELY (m_fonts[VTE_DRAW_NORMAL] == nullptr)) {
                left = 0;
                right = 0;
                return;
        }

        w = m_fonts[attr_to_style(attr)]->get_unistr_info(c)->width;
        normal_width = m_fonts[VTE_DRAW_NORMAL]->width() * columns;
        fits_width = m_cell_width * columns;

        if (G_LIKELY (w <= normal_width)) {
                /* The regular case: The glyph is not wider than one (CJK: two) regular character(s).
                 * Align to the left, after applying half (CJK: one) letter spacing. */
                l = m_char_spacing.left + (columns == 2 ? m_char_spacing.right : 0);
        } else if (G_UNLIKELY (w <= fits_width)) {
                /* Slightly wider glyph, but still fits in the cell (spacing included). This case can
                 * only happen with nonzero letter spacing. Center the glyph in the cell(s). */
                l = (fits_width - w) / 2;
        } else {
                /* Even wider glyph: doesn't fit in the cell. Align at left and overflow on the right. */
                l = 0;
        }

        left = l;
        right = l + w;
}

void
DrawingContext::draw_line(int x,
                          int y,
                          int xp,
                          int yp,
                          int line_width,
                          vte::color::rgb const *color)
{
        fill_rectangle(x, y,
                       MAX(line_width, xp - x + 1), MAX(line_width, yp - y + 1),
                       color);
}

void
DrawingContext::draw_undercurl(int x,
                               double y,
                               double line_width,
                               int count,
                               int scale_factor,
                               vte::color::rgb const *color)
{
        /* The end of the curly line slightly overflows to the next cell, so the canvas
         * caching the rendered look has to be wider not to chop this off. */
        gint x_padding = line_width + 1;  /* ceil, kind of */

        gint surface_top = y;  /* floor */

        gint undercurl_height = _vte_draw_get_undercurl_height(m_cell_width, line_width);

        /* Give extra space vertically to include the bounding box for antialiasing
         * and the y_bottom+1 below.
         */
        constexpr auto const extra_space = 1;
        auto cr = begin_cairo(x, y - extra_space, count * m_cell_width, undercurl_height + 2 * extra_space + 1);

        cairo_save (cr);

        _vte_debug_print (VTE_DEBUG_DRAW,
                          "draw_undercurl (x=%d, y=%f, count=%d, color=(%d,%d,%d))\n",
                          x, y, count,
                          color->red, color->green, color->blue);

        if (m_undercurl_surface_scale != scale_factor)
                m_undercurl_surface.reset();

        if (G_UNLIKELY (!m_undercurl_surface)) {
                /* Cache the undercurl's look. The design assumes that until the cached look is
                 * invalidated (the font is changed), this method is always called with the "y"
                 * parameter having the same fractional part, and the same "line_width" parameter.
                 * For caching, only the fractional part of "y" is used.
                 */
                double rad = _vte_draw_get_undercurl_rad(m_cell_width);
                double y_bottom = y + undercurl_height;
                double y_center = (y + y_bottom) / 2.;
                gint surface_bottom = y_bottom + 1;  /* ceil, kind of */

                _vte_debug_print (VTE_DEBUG_DRAW,
                                  "caching undercurl shape\n");

                /* Add a line_width of margin horizontally on both sides, for nice antialias overflowing.
                 * Add pixel margin to top/bottom for curl antialiasing.
                 */
                m_undercurl_surface_scale = scale_factor;
                m_undercurl_surface = vte::take_freeable
                        (cairo_surface_create_similar(cairo_get_target(cr),
                                                      CAIRO_CONTENT_ALPHA,
                                                      m_cell_width + 2 * x_padding,
                                                      surface_bottom - surface_top + 2));
                auto undercurl_cr = vte::take_freeable(cairo_create(m_undercurl_surface.get()));
                cairo_translate(undercurl_cr.get(), 0, 1);
                cairo_set_operator(undercurl_cr.get(), CAIRO_OPERATOR_OVER);
                /* First quarter circle, similar to the left half of the tilde symbol. */
                cairo_arc(undercurl_cr.get(), x_padding + m_cell_width / 4., y_center - surface_top + m_cell_width / 4., rad, G_PI * 5 / 4, G_PI * 7 / 4);
                /* Second quarter circle, similar to the right half of the tilde symbol. */
                cairo_arc_negative(undercurl_cr.get(), x_padding + m_cell_width * 3 / 4., y_center - surface_top - m_cell_width / 4., rad, G_PI * 3 / 4, G_PI / 4);
                cairo_set_line_width (undercurl_cr.get(), line_width);
                cairo_stroke(undercurl_cr.get());
        }

        /* Paint the cached look of the undercurl using the desired color.
         * The cached look takes the fractional part of "y" into account,
         * here we only offset by its integer part. */
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        _vte_set_source_color(cr, color);
        for (int i = 0; i < count; i++) {
                cairo_mask_surface(cr, m_undercurl_surface.get(), x - x_padding + i * m_cell_width, surface_top);
        }

        cairo_restore (cr);

        end_cairo(cr);
}


} // namespace view
} // namespace vte
