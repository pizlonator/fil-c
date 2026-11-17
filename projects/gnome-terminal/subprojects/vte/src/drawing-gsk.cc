/*
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
#include "drawing-gsk.hh"
#include "fonts-pangocairo.hh"
#include "graphene-glue.hh"
#include "refptr.hh"

static inline PangoGlyphInfo *
vte_glyphs_grow (VteGlyphs *glyphs,
                 guint      count)
{
        g_assert (count > 0);

        guint len = vte_glyphs_get_size (glyphs);
        vte_glyphs_set_size (glyphs, len + count);
        return vte_glyphs_get (glyphs, len);
}

namespace vte {
namespace view {

DrawingGsk::DrawingGsk() noexcept
{
        vte_glyphs_init (&m_glyphs);
}

DrawingGsk::~DrawingGsk()
{
        vte_glyphs_clear (&m_glyphs);
}

void
DrawingGsk::set_snapshot(GtkSnapshot *snapshot) noexcept
{
        m_snapshot = snapshot;
}

void
DrawingGsk::clear(int x,
                  int y,
                  int width,
                  int height,
                  vte::color::rgb const* color,
                  double alpha) const
{
        fill_rectangle(x, y, width, height, color, alpha);
}

void
DrawingGsk::fill_rectangle(int x,
                           int y,
                           int width,
                           int height,
                           vte::color::rgb const* color) const
{
        g_assert(m_snapshot);
        g_assert(color);

        _vte_debug_print(VTE_DEBUG_DRAW,
                         "draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d))\n",
                         x,y,width,height,
                         color->red, color->green, color->blue);

        auto const rect = Rectangle{x, y, width, height};
        auto const rgba = color->rgba(1.0);
        gtk_snapshot_append_color(m_snapshot, &rgba, rect.graphene());
}

void
DrawingGsk::fill_rectangle(int x,
                           int y,
                           int width,
                           int height,
                           vte::color::rgb const* color,
                           double alpha) const
{
        g_assert(m_snapshot);
        g_assert(color);

        _vte_debug_print(VTE_DEBUG_DRAW,
                         "draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%.3f))\n",
                         x,y,width,height,
                         color->red, color->green, color->blue,
                         alpha);

        auto const rect = Rectangle{x, y, width, height};
        auto const rgba = color->rgba(alpha);
        gtk_snapshot_append_color(m_snapshot, &rgba, rect.graphene());
}

void
DrawingGsk::flush_glyph_string(PangoFont *font,
                               const GdkRGBA *color)
{
        PangoGlyphString glyph_string = {
                .num_glyphs = int(vte_glyphs_get_size (&m_glyphs)),
                .glyphs = vte_glyphs_get_data (&m_glyphs),
                .log_clusters = nullptr
        };

        if (glyph_string.num_glyphs == 0)
                return;

        /* Setup initial positioning for the text node */
        int x = glyph_string.glyphs[0].geometry.x_offset;
        auto const offset = GRAPHENE_POINT_INIT (float(x/PANGO_SCALE), float(0));
        x += glyph_string.glyphs[0].geometry.width;
        glyph_string.glyphs[0].geometry.x_offset = 0;

        /* Fix up our geometries relative to previous glyph so that we can
         * avoid any sort of translation/container node for most of our
         * strings. That should result in faster node diff'ing and building
         * the vertices for the texture atlas.
         */
        for (int i = 1; i < glyph_string.num_glyphs; i++) {
                glyph_string.glyphs[i].geometry.x_offset -= x;
                x += glyph_string.glyphs[i].geometry.width;
        }

        /* Create text node with offset of first glyph */
        auto node = gsk_text_node_new (font, &glyph_string, color, &offset);
        if (node != nullptr) {
                gtk_snapshot_append_node (m_snapshot, node);
                gsk_render_node_unref (node);
        }

        vte_glyphs_set_size (&m_glyphs, 0);
}


void
DrawingGsk::draw_text(TextRequest* requests,
                      gsize n_requests,
                      uint32_t attr,
                      vte::color::rgb const* color)
{
        if (_vte_debug_on (VTE_DEBUG_DRAW)) {
                GString *string = g_string_new ("");
                gchar *str;
                gsize n;
                for (n = 0; n < n_requests; n++) {
                        g_string_append_unichar (string, requests[n].c);
                }
                str = g_string_free (string, FALSE);
                g_printerr ("draw_text (\"%s\", len=%" G_GSIZE_FORMAT ", color=(%d,%d,%d), %s - %s)\n",
                            str, n_requests, color->red, color->green, color->blue,
                            (attr & VTE_ATTR_BOLD)   ? "bold"   : "normal",
                            (attr & VTE_ATTR_ITALIC) ? "italic" : "regular");
                g_free (str);
        }

        auto font = m_fonts[attr_to_style(attr)];
        gsize i;

        g_assert(font);
        g_assert(m_snapshot);

        if (n_requests == 0)
                return;

        auto const rgba = color->rgba(1.0);
        PangoFont *node_font = nullptr;

        vte_glyphs_set_size (&m_glyphs, 0);

        for (i = 0; i < n_requests; i++) {
                vteunistr c = requests[i].c;

                if (G_UNLIKELY (requests[i].mirror)) {
                        vte_bidi_get_mirror_char (c, requests[i].box_mirror, &c);
                }

                if (Minifont::unistr_is_local_graphic(c)) {
                        m_minifont.draw_graphic(*this,
                                                c,
                                                color,
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
                 * baselines, we offset by the normal font's ascent here. (Bug 137.) */
                y = requests[i].y + m_char_spacing.top + m_fonts[VTE_DRAW_NORMAL]->ascent();

                switch (uinfo->coverage()) {
                default:
                        g_assert_not_reached ();
                        break;
                case FontInfo::UnistrInfo::Coverage::UNKNOWN:
                        break;
                case FontInfo::UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING:
                        if (node_font != ufi->using_pango_glyph_string.font) {
                                flush_glyph_string (node_font, &rgba);
                                node_font = ufi->using_pango_glyph_string.font;
                        }

                        if (node_font == nullptr)
                                break;

                        auto const from_string = ufi->using_pango_glyph_string.glyph_string;
                        if (from_string->num_glyphs == 0)
                                break;

                        auto to_glyphs = vte_glyphs_grow (&m_glyphs, from_string->num_glyphs);
                        for (int j = 0; j < from_string->num_glyphs; j++) {
                                to_glyphs[j] = from_string->glyphs[j];
                                to_glyphs[j].geometry.x_offset += x * PANGO_SCALE;
                                to_glyphs[j].geometry.y_offset += y * PANGO_SCALE;
                                x += to_glyphs[j].geometry.width / PANGO_SCALE;
                        }

                        break;
                }
        }

        flush_glyph_string (node_font, &rgba);
}

void
DrawingGsk::draw_rectangle(int x,
                           int y,
                           int width,
                           int height,
                           vte::color::rgb const* color) const
{
        g_assert(color);
        g_assert(m_snapshot);

        _vte_debug_print (VTE_DEBUG_DRAW,
                          "draw_rectangle (%d, %d, %d, %d, color=(%d,%d,%d))\n",
                          x,y,width,height,
                          color->red, color->green, color->blue);

        static const float border_width[4] = {VTE_LINE_WIDTH, VTE_LINE_WIDTH, VTE_LINE_WIDTH, VTE_LINE_WIDTH};
        auto const rounded = GSK_ROUNDED_RECT_INIT (float(x), float(y), float(width), float(height));
        GdkRGBA rgba[4];
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = color->rgba(1.0);
        gtk_snapshot_append_border(m_snapshot, &rounded, border_width, rgba);
}

cairo_t*
DrawingGsk::begin_cairo(int x,
                        int y,
                        int width,
                        int height) const
{
        g_assert(m_snapshot);

        auto const bounds = GRAPHENE_RECT_INIT(float(x), float(y), float(width), float(height));
        return gtk_snapshot_append_cairo(m_snapshot, &bounds);
}

void
DrawingGsk::end_cairo(cairo_t *cr) const
{
        cairo_destroy(cr);
}

void
DrawingGsk::draw_surface_with_color_mask(GdkTexture *texture,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         vte::color::rgb const* color) const
{
        const auto bounds = vte::graphene::make_rect (x, y, width, height);
        const auto rgba = color->rgba();

        gtk_snapshot_push_mask(m_snapshot, GSK_MASK_MODE_ALPHA);
        gtk_snapshot_append_texture(m_snapshot, texture, &bounds);
        gtk_snapshot_pop(m_snapshot);
        gtk_snapshot_append_color(m_snapshot, &rgba, &bounds);
        gtk_snapshot_pop(m_snapshot);
}

void
DrawingGsk::begin_background(Rectangle const& rect,
                             size_t columns,
                             size_t rows)
{
        m_background_cols = columns;
        m_background_rows = rows;
        m_background_len = columns * rows;
        m_background_set = false;
        m_background_data = vte::glib::take_free_ptr(g_new0(r8g8b8a8, m_background_len));
}

void
DrawingGsk::flush_background(Rectangle const& rect)
{
        if (m_background_set) {
                auto bytes = vte::take_freeable
                        (g_bytes_new_take(m_background_data.release(),
                                          m_background_len * sizeof(r8g8b8a8)));
                auto texture = vte::glib::take_ref
                        (gdk_memory_texture_new(m_background_cols,
                                                m_background_rows,
                                                GDK_MEMORY_R8G8B8A8,
                                                bytes.get(),
                                                m_background_cols * sizeof(r8g8b8a8)));
                gtk_snapshot_append_scaled_texture(m_snapshot,
                                                   texture.get(),
                                                   GSK_SCALING_FILTER_NEAREST,
                                                   rect.graphene());
        } else {
                m_background_data.reset();
        }

        m_background_cols = 0;
        m_background_rows = 0;
        m_background_len = 0;
        m_background_set = false;
}

} // namespace view
} // namespace vte
