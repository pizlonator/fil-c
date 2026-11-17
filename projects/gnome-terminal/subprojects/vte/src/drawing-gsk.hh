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

#pragma once

#include <gtk/gtk.h>

#include "drawing-context.hh"
#include "glib-glue.hh"
#include "minifont.hh"

#define GDK_ARRAY_NAME vte_glyphs
#define GDK_ARRAY_TYPE_NAME VteGlyphs
#define GDK_ARRAY_ELEMENT_TYPE PangoGlyphInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 128
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

namespace vte {
namespace view {

typedef struct _r8g8b8a8
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} r8g8b8a8;

static_assert(sizeof(r8g8b8a8) == 4, "Wrong size");

class DrawingGsk final : public DrawingContext {
public:
        DrawingGsk() noexcept;
        ~DrawingGsk() override;

        DrawingGsk(DrawingGsk const&) = delete;
        DrawingGsk(DrawingGsk&&) = delete;
        DrawingGsk& operator=(DrawingGsk const&) = delete;
        DrawingGsk& operator=(DrawingGsk&&) = delete;

        void set_snapshot(GtkSnapshot *snapshot) noexcept;

        cairo_t* begin_cairo(int x,
                             int y,
                             int width,
                             int height) const override;
        void end_cairo(cairo_t *cr) const override;

        /* We don't perform any clipping because we render the entire
         * scene graph and let Gsk perform the difference to determine
         * the appropriate damage area.
         */
        inline void clip(Rectangle const* rect) const override { }
        inline void unclip() const override { }

        inline void clip_border(Rectangle const* rect) const override
        {
                gtk_snapshot_push_clip(m_snapshot, rect->graphene());
        }
        inline void unclip_border() const override
        {
                gtk_snapshot_pop(m_snapshot);
        }

        inline void translate(double x,
                              double y) const override
        {
                auto const point = GRAPHENE_POINT_INIT((float)x, (float)y);
                gtk_snapshot_save(m_snapshot);
                gtk_snapshot_translate(m_snapshot, &point);
        }
        inline void untranslate() const override
        {
                gtk_snapshot_restore(m_snapshot);
        }

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

        void draw_surface_with_color_mask(GdkTexture *texture,
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
                                         size_t n_columns,
                                         vte::color::rgb const* color) override {
                assert(column + n_columns <= m_background_cols);

                auto const fill = r8g8b8a8{uint8_t(color->red >> 8),
                                           uint8_t(color->green >> 8),
                                           uint8_t(color->blue >> 8),
                                           uint8_t(0xffu)};
                std::fill_n(m_background_data.get() + (row * m_background_cols + column),
                            n_columns,
                            fill);

                m_background_set = true;
        }

        void begin_background(Rectangle const& rect,
                              size_t columns,
                              size_t rows) override;
        void flush_background(Rectangle const& rect) override;

private:
        GtkSnapshot *m_snapshot{nullptr}; // unowned
        VteGlyphs m_glyphs;
        MinifontGsk m_minifont{};

        vte::glib::FreePtr<r8g8b8a8> m_background_data;
        size_t m_background_len;
        size_t m_background_cols;
        size_t m_background_rows;
        bool m_background_set{false};

        void flush_glyph_string(PangoFont* font,
                                const GdkRGBA* rgba);

};

} // namespace view
} // namespace vte
