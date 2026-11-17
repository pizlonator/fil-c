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

#include "config.h"

#include <algorithm>
#include <cmath>

#include "cairo-glue.hh"

#if VTE_GTK == 4
#include <gdk/gdk.h>
#include "glib-glue.hh"
#endif

#include "drawing-context.hh"
#include "minifont.hh"

#define MINIFONT_CACHE_MAX_SIZE 128

typedef struct _CachedMinifont
{
        gunichar c;                 // the actual unichar to draw
        unsigned width : 12;        // the width of the cell
        unsigned height : 13;       // the height of the cell
        unsigned scale_factor : 3;  // the scale factor (1..7)
        unsigned x_off : 2;         // x_offset for patterns (0..3)
        unsigned y_off : 2;         // y_offset for patterns (0..3)
                                    // 8-byte boundary for cached_minifont_equal()
        unsigned : 0;
        unsigned xpad : 12;
        unsigned ypad : 13;

        GList link;

        /* An 8-bit alpha only surface */
#if VTE_GTK == 3
        cairo_surface_t *surface;
#elif VTE_GTK == 4
        GdkTexture *texture;
#endif
} CachedMinifont;

static GHashTable *minifont_cache;
static GQueue minifonts;
static guint minifont_gc_source;

static inline guint
cached_minifont_hash(gconstpointer data)
{
        const CachedMinifont *mf = (const CachedMinifont *)data;

        return (mf->c & 0xFFFF) ^
               (((mf->scale_factor-1) & 0x3) << 30) ^
               ((mf->width & 0x1F) << 25) ^
               ((mf->height & 0x1F) << 20) ^
               (mf->x_off << 18) ^
               (mf->y_off << 16);
}

static inline gboolean
cached_minifont_equal(gconstpointer data1,
                      gconstpointer data2)
{
        return memcmp (data1, data2, 8) == 0;
}

static void
cached_minifont_free (gpointer data)
{
        CachedMinifont *mf = (CachedMinifont *)data;

        g_queue_unlink (&minifonts, &mf->link);
#if VTE_GTK == 3
        cairo_surface_destroy (mf->surface);
#elif VTE_GTK == 4
        g_object_unref (mf->texture);
#endif
        g_free (mf);
}

static const CachedMinifont *
cached_minifont_lookup(vteunistr c,
                       int width,
                       int height,
                       int scale_factor,
                       int x_off,
                       int y_off)
{
        CachedMinifont key;

        if G_UNLIKELY (minifont_cache == nullptr)
                return nullptr;

        key.c = c;
        key.width = width;
        key.height = height;
        key.scale_factor = scale_factor;
        key.x_off = x_off;
        key.y_off = y_off;

        // We could use an MRU here to track the minifont surface/textures
        // but they are fast enough to create on demand if we even reach our
        // threshold that it's cheaper than MRU tracking on lookups.

        return (const CachedMinifont *)g_hash_table_lookup (minifont_cache, (gpointer)&key);
}

static gboolean
cached_minifont_gc_worker(gpointer data)
{
        minifont_gc_source = 0;

        while (minifonts.length > MINIFONT_CACHE_MAX_SIZE) {
                CachedMinifont *mf = (CachedMinifont *)g_queue_peek_tail (&minifonts);
                g_hash_table_remove (minifont_cache, (gpointer)mf);
        }

        return G_SOURCE_REMOVE;
}

static inline void
cached_minifont_add(CachedMinifont *mf)
{
        if G_UNLIKELY (minifont_cache == NULL)
                minifont_cache = g_hash_table_new_full (cached_minifont_hash,
                                                        cached_minifont_equal,
                                                        cached_minifont_free,
                                                        nullptr);

        g_queue_push_head_link (&minifonts, &mf->link);
        g_hash_table_add (minifont_cache, mf);

        if G_UNLIKELY (minifont_gc_source == 0 && minifonts.length > MINIFONT_CACHE_MAX_SIZE)
                minifont_gc_source = g_idle_add (cached_minifont_gc_worker, nullptr);
}

static inline void
cached_minifont_draw(const CachedMinifont *mf,
                     vte::view::DrawingContext const& context,
                     int x,
                     int y,
                     int width,
                     int height,
                     vte::color::rgb const* fg)
{
        x -= mf->xpad;
        y -= mf->ypad;
        width += 2 * mf->xpad;
        height += 2 * mf->ypad;

        // Our surface includes padding on all sides to help with situations
        // where glyphs should appear to overlap adjacent cells.
#if VTE_GTK == 3
        context.draw_surface_with_color_mask(mf->surface, x, y, width, height, fg);
#elif VTE_GTK == 4
        context.draw_surface_with_color_mask(mf->texture, x, y, width, height, fg);
#endif
}

static cairo_surface_t *
create_surface(int width,
               int height,
               int xpad,
               int ypad,
               int scale_factor)
{
        width += xpad * 2;
        width *= scale_factor;

        height += ypad * 2;
        height *= scale_factor;

        auto surface = cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);
        cairo_surface_set_device_scale(surface, scale_factor, scale_factor);
        return surface;
}

/* pixman data must have stride 0 mod 4 */

// Note that the LR and RL patterns are not mirrors of each other,
// but instead the RL pattern is the mirrored pattern that then is
// additionally shifted 1 row upwards. This makes the pattern tile
// seamlessly when they are used to fill a rectangle of any given
// (fixed) width and height that are then put next to each other
// horizontally or vertically.
// See issue#2672.
static unsigned char const hatching_pattern_lr_data[16] = {
        0xff, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0x00,
        0x00, 0x00, 0x00, 0xff,
};
static unsigned char const hatching_pattern_rl_data[16] = {
        0x00, 0x00, 0xff, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff,
};

static unsigned char const checkerboard_pattern_data[16] = {
        0xff, 0xff, 0x00, 0x00,
        0xff, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0xff,
        0x00, 0x00, 0xff, 0xff,
};
static unsigned char const checkerboard_reverse_pattern_data[16] = {
        0x00, 0x00, 0xff, 0xff,
        0x00, 0x00, 0xff, 0xff,
        0xff, 0xff, 0x00, 0x00,
        0xff, 0xff, 0x00, 0x00,
};

// U+1FB97 HEAVY HORIZONTAL FILL
static unsigned char const heavy_horizontal_fill_pattern_data[16] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
};

// U+1CC40 SPARSE HORIZONTAL FILL
static unsigned char const sparse_horizontal_fill_pattern_data[16] = {
        0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
};

// U+1CC41 SPARSE VERTICAL FILL
static unsigned char const sparse_vertical_fill_pattern_data[16] = {
        0x00, 0xff, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
};

// U+1CC42 ORTHOGONAL CROSSHATCH FILL
static unsigned char const orthogonal_crosshatch_fill_pattern_data[16] = {
        0x00, 0xff, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0x00, 0xff, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
};

// U+1CC43 DIAGONAL CROSSHATCH FILL
static unsigned char const diagonal_crosshatch_fill_pattern_data[16] = {
        0xff, 0x00, 0xff, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0xff, 0x00, 0xff, 0x00,
        0x00, 0x00, 0x00, 0xff,
};

// U+1CC44 DENSE VERTICAL FILL
static unsigned char const dense_vertical_fill_pattern_data[16] = {
        0x00, 0xff, 0x00, 0xff,
        0x00, 0xff, 0x00, 0xff,
        0x00, 0xff, 0x00, 0xff,
        0x00, 0xff, 0x00, 0xff,
};

// U+1CC45 DENSE HORIZONTAL FILL
static unsigned char const dense_horizontal_fill_pattern_data[16] = {
        0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
};

// U+1CC46 SPECKLE FILL FRAME-1
static unsigned char const speckle_frame1_fill_pattern_data[64] = {
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
        0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
};

// U+1CC47 SPECKLE FILL FRAME-2
static unsigned char const speckle_frame2_fill_pattern_data[64] = {
        0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
};

#define DEFINE_STATIC_PATTERN_FUNC(name,data,width,height,stride) \
static cairo_pattern_t* \
name(void) \
{ \
        static cairo_pattern_t* pattern = nullptr; \
\
        if (pattern == nullptr) { \
                auto surface = cairo_image_surface_create_for_data(const_cast<unsigned char*>(data), \
                                                                   CAIRO_FORMAT_A8, \
                                                                   width, \
                                                                   height, \
                                                                   stride); \
                pattern = cairo_pattern_create_for_surface(surface); \
                cairo_surface_destroy(surface); \
\
                cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT); \
                cairo_pattern_set_filter (pattern, CAIRO_FILTER_FAST); \
       } \
\
       return pattern; \
}

DEFINE_STATIC_PATTERN_FUNC(create_hatching_pattern_lr, hatching_pattern_lr_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_hatching_pattern_rl, hatching_pattern_rl_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_checkerboard_pattern, checkerboard_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_checkerboard_reverse_pattern, checkerboard_reverse_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_heavy_horizontal_fill_pattern, heavy_horizontal_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_sparse_horizontal_fill_pattern, sparse_horizontal_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_sparse_vertical_fill_pattern, sparse_vertical_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_orthogonal_crosshatch_fill_pattern, orthogonal_crosshatch_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_diagonal_crosshatch_fill_pattern, diagonal_crosshatch_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_dense_vertical_fill_pattern, dense_vertical_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_dense_horizontal_fill_pattern, dense_horizontal_fill_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_speckle_frame1_fill_pattern, speckle_frame1_fill_pattern_data, 8, 8, 8)
DEFINE_STATIC_PATTERN_FUNC(create_speckle_frame2_fill_pattern, speckle_frame2_fill_pattern_data, 8, 8, 8)

#undef DEFINE_STATIC_PATTERN_FUNC

static inline void
diagonal_slope_1_1(cairo_t* cr,
                   double x,
                   double y,
                   int width,
                   int height,
                   int line_width,
                   uint32_t v) noexcept
{
        // These characters draw outside their cell, so we need to
        // enlarge the drawing surface.
        auto const dx = (line_width + 1) / 2;
        cairo_rectangle(cr, x - dx, y, width + 2 * dx, height);
        cairo_clip(cr);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_width(cr, line_width);
        if (v & 2) {
                cairo_move_to(cr, x, y);
                cairo_line_to(cr, x + width, y + height);
                cairo_stroke(cr);
        }
        if (v & 1) {
                cairo_move_to(cr, x + width, y);
                cairo_line_to(cr, x, y + height);
                cairo_stroke(cr);
        }
}

static inline void
diagonal_double(cairo_t* cr,
                double x,
                double y,
                int width,
                int height,
                int line_width,
                uint32_t v) noexcept
{
        cairo_rectangle(cr, x, y, width, height);
        cairo_clip(cr);

        cairo_set_line_width(cr, line_width);

        auto const x1 = x + width;
        auto const y0 = v & 1 ? y + height : y;
        auto const y1 = v & 1 ? y : y + height;

        auto const dy = std::min(line_width * 3, height / 2);
        cairo_move_to(cr, x, y0 - dy);
        cairo_line_to(cr, x1, y1 - dy);
        cairo_stroke(cr);

        cairo_move_to(cr, x, y0 + dy);
        cairo_line_to(cr, x1, y1 + dy);
        cairo_stroke(cr);
}

static inline void
diagonal_double_middle(cairo_t* cr,
                       double x,
                       double y,
                       int width,
                       int height,
                       int line_width,
                       uint32_t v) noexcept
{
        double const xcenter = x + width / 2 + (width & 1 ? 0.5 : 0.0);
        double const ycenter = y + height / 2 + (height & 1 ? 0.5 : 0.0);

        cairo_rectangle(cr, x, y, width, height);
        cairo_clip(cr);

        cairo_set_line_width(cr, line_width);

        auto const x1 = x + width;
        auto const y0 = v & 1 ? y + height : y;
        auto const y1 = v & 1 ? y : y + height;

        cairo_move_to(cr, x, y0);
        cairo_line_to(cr, xcenter, ycenter);
        cairo_line_to(cr, x1, y0);
        cairo_stroke(cr);

        cairo_move_to(cr, x, ycenter);
        cairo_line_to(cr, xcenter, y1);
        cairo_line_to(cr, x1, ycenter);
        cairo_stroke(cr);
}

// draw half- and double-slope diagonals U+1FBD0..U+1FBD7
// and used to compose U+1FBDC..U+1FBDF.
static inline void
diagonal(cairo_t* cr,
         double x,
         double y,
         int width,
         int height,
         int xoffset,
         int yoffset,
         int xstep,
         int ystep,
         int line_width,
         uint32_t v) noexcept
{
        // These need to be perfectly symmetrical, so not using
        // left_half/top_half as center.  Also in order to perfectly
        // connect diagonally with each other, draw the line outside
        // the cell area and clip the result to the cell. Also makes
        // it so there's no need to even calculate xcenter or ycenter.

        auto const x0 = x + xoffset;
        auto const x1 = x0 + xstep;
        auto const y0 = y + yoffset;
        auto const y1 = y0 + ystep;

        // These are allowed to draw horizontally outside of their cell,
        // but only in the direction where the line goes to a cell corner,
        // so v=0, 2, 4, 7 open at the left but clipped at the right edge,
        // and  v=1, 3, 5, 6 clipped at the left edge and open at the right.
        cairo_save(cr);
        cairo_rectangle(cr,
                        (v == 0 || v == 2 || v == 4 || v == 7) ? x - line_width : x,
                        y,
                        width + line_width,
                        height);
        cairo_clip(cr);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_width(cr, line_width);

        cairo_move_to(cr, v & 4 ? x1 : x0, v & 2 ? y0 : y1);
        cairo_line_to(cr, v & 4 ? x0 : x1, v & 2 ? y1 : y0);
        cairo_stroke(cr);

        cairo_restore(cr); // unclip
}

// half-slope diagonals U+1FBD0..U+1FBD3
static inline void
diagonal_slope_2_1(cairo_t* cr,
                   double x,
                   double y,
                   int width,
                   int height,
                   int line_width,
                   uint32_t v) noexcept
{
        return diagonal(cr, x, y, width, height,
                        v & 1 ? -width : 0, 0,
                        2 * width, height,
                        line_width,
                        v);
}

// double-slope diagonals U+1FBD4..U+1FBD7
static inline void
diagonal_slope_1_2(cairo_t* cr,
                   double x,
                   double y,
                   int width,
                   int height,
                   int line_width,
                   uint32_t v) noexcept
{
        return diagonal(cr, x, y, width, height,
                        0, v & 1 ? -height : 0,
                        width, 2 * height,
                        line_width,
                        v);
}

// half diagonals to center U+1FBD8..U+1FBDB
static inline void
diagonal_to_center(cairo_t* cr,
                   double x,
                   double y,
                   int width,
                   int height,
                   int line_width,
                   uint8_t v) noexcept
{
        // These need to be perfectly symmetrical, so not using
        // left_half/top_half as center.
        // These need to perfectly connect diagonally to
        // U+2571..U+2573.

        double const xcenter = x + width / 2 + (width & 1 ? 0.5 : 0.0);
        double const ycenter = y + height / 2 + (height & 1 ? 0.5 : 0.0);

        cairo_rectangle(cr, x - line_width, y, width + 2 * line_width, height);
        cairo_clip(cr);

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_width(cr, line_width);

        double const xp[4] = {x, x + width, x + width, x};
        double const yp[4] = {y, y, y + height, y + height};

        v &= 3;
        cairo_move_to(cr, xp[v], yp[v]);
        cairo_line_to(cr, xcenter, ycenter);
        ++v;
        v &= 3;
        cairo_line_to(cr, xp[v], yp[v]);
        cairo_stroke(cr);
}

static inline void
middle_diagonal(cairo_t* cr,
                double x,
                double y,
                int width,
                int height,
                int line_width,
                uint8_t v) noexcept
{
        // These need to be perfectly symmetrical, so not using
        // left_half/top_half as center.  Also in order to perfectly
        // connect diagonally with each other, draw the line outside
        // the cell area and clip the result to the cell. Also makes
        // it so there's no need to even calculate ycenter.

        double const xcenter = x + width / 2 + (width & 1 ? 0.5 : 0.0);

        cairo_rectangle(cr, x, y, width, height);
        cairo_clip(cr);

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_width(cr, line_width);

        if (v & 1) {
                /* upper left */
                cairo_move_to(cr, xcenter, y);
                cairo_line_to(cr, xcenter - width, y + height);
                cairo_stroke(cr);
        }
        if (v & 2) {
                /* upper right */
                cairo_move_to(cr, xcenter, y);
                cairo_line_to(cr, xcenter + width, y + height);
                cairo_stroke(cr);
        }
        if (v & 4) {
                /* lower left */
                cairo_move_to(cr, xcenter - width, y);
                cairo_line_to(cr, xcenter, y + height);
                cairo_stroke(cr);
        }
        if (v & 8) {
                /* lower right */
                cairo_move_to(cr, xcenter + width, y);
                cairo_line_to(cr, xcenter, y + height);
                cairo_stroke(cr);
        }
}

static void
rectangle(cairo_t* cr,
          double x,
          double y,
          double w,
          double h,
          int xdenom,
          int ydenom,
          int xb1,
          int yb1,
          int xb2,
          int yb2)
{
        int const x1 = (w) * (xb1) / (xdenom);
        int const y1 = (h) * (yb1) / (ydenom);
        int const x2 = (w) * (xb2) / (xdenom);
        int const y2 = (h) * (yb2) / (ydenom);
        cairo_rectangle ((cr), (x) + x1, (y) + y1, MAX(x2 - x1, 1), MAX(y2 - y1, 1));
        cairo_fill (cr);
}

inline void
quadrant(cairo_t* cr,
         uint8_t value,
         int x,
         int y,
         int width,
         int height) noexcept
{
        auto const width_half = std::max(width / 2, 1);
        auto const height_half = std::max(height / 2, 1);

        cairo_set_line_width(cr, 0);
        if (value & 0b0001u)
                cairo_rectangle(cr, x, y, width_half, height_half);
        if (value & 0b0010u)
                cairo_rectangle(cr, x + width_half, y, width - width_half, height_half);
        if (value & 0b0100u)
                cairo_rectangle(cr, x, y + height_half, width_half, height - height_half);
        if (value & 0b1000u)
                cairo_rectangle(cr, x + width_half, y + height_half, width - width_half, height - height_half);

        cairo_fill(cr);
}

inline void
sextant(cairo_t* cr,
        uint8_t value,
        int x,
        int y,
        int width,
        int height) noexcept
{
        if (width < 2 || height < 3)
                [[unlikely]] return; // nothing to draw

        auto const width_half = width / 2;
        auto const height_third = height / 3;
        auto const extra_height = height % 3 ? 1 : 0;

        auto row = [&](uint8_t v,
                       int y0,
                       int h) noexcept
        {
                if (v & 0b01u)
                        cairo_rectangle(cr, x, y0, width_half, h);
                if (v & 0b10u)
                        cairo_rectangle(cr, x + width_half, y0, width - width_half, h);
        };

        cairo_set_line_width(cr, 0);

        // If height isn't divisibly by 3, distribute the extra pixels to
        // the middle first, then the bottom.

        int const yd[4] = {0, height_third, height_third * 2 + extra_height, height};
        row(value, y, yd[1] - yd[0]);
        row(value >> 2, y + yd[1], yd[2] - yd[1]);
        row(value >> 4, y + yd[2], yd[3] - yd[2]);
        cairo_fill(cr);
}

inline void
octant(cairo_t* cr,
       uint8_t value,
       int x,
       int y,
       int width,
       int height) noexcept
{
        if (width < 2 || height < 4)
                [[unlikely]] return; // nothing to draw

        auto const width_half = width / 2;
        auto const height_quarter = height / 4;
        auto const extra_height = height % 4;

        auto row = [&](uint8_t v,
                       int y0,
                       int h) noexcept
        {
                if (v & 0b01u)
                        cairo_rectangle(cr, x, y0, width_half, h);
                if (v & 0b10u)
                        cairo_rectangle(cr, x + width_half, y0, width - width_half, h);
        };

        cairo_set_line_width(cr, 0);

        // If height isn't divisibly by 4, distribute the extra pixels to
        // the 3rd row first, then the 2nd, then the 4th.
        // FIXME: make sure this connects correctly with the one-eights
        // as well as the quarter blocks.

        int const heights[4] = {
                height_quarter,
                height_quarter + (extra_height > 2 ? 1 : 0),
                height_quarter + (extra_height ? 1 : 0),
                height_quarter + (extra_height > 1 ? 1 : 0)
        };
        for (auto i = 0; i < 4; ++i) {
                row(value, y, heights[i]);
                value >>= 2;
                y += heights[i];
        }

        cairo_fill(cr);
}

inline void
sixteenth(cairo_t* cr,
          uint16_t value,
          int x,
          int y,
          int width,
          int height) noexcept
{
        if (width < 4 || height < 4) [[unlikely]]
                return; // don't draw anything

        auto const width_quarter = width / 4;
        auto const extra_width = width & 3;
        auto const height_quarter = height / 4;
        auto const extra_height = height & 3;

        // Note! Some of these sixteenths are used to draw octants, i.e.
        // BLOCK OCTANT-8 = U+1CEA0 RIGHT HALF LOWER ONE QUARTER BLOCK
        // BLOCK OCTANT-7 = U+1CEA3 LEFT HALF LOWER ONE QUARTER BLOCK
        // BLOCK OCTANT-1 = U+1CEA8 LEFT HALF UPPER ONE QUARTER BLOCK
        // BLOCK OCTANT-2 = U+1CEAB RIGHT HALF UPPER ONE QUARTER BLOCK
        // and so this code must absolutely draw them as if drawn by
        // octant() above.

        // If width isn't divisibly by 4, distribute the extra pixels to
        // the 3rd column first, then the 2nd, then the 4th.
        // FIXME: make sure this connects correctly with the one-eights
        // as well as the sextants and octants.
        int const widths[4] = {
                width_quarter,
                width_quarter + (extra_width > 2 ? 1 : 0),
                width_quarter + (extra_width ? 1 : 0),
                width_quarter + (extra_width > 1 ? 1 : 0)
        };

        // If height isn't divisibly by 4, distribute the extra pixels to
        // the 3rd row first, then the 2nd, then the 4th.
        // FIXME: make sure this connects correctly with the one-eights
        // as well as the quadrants, sextants and octants.
        int const heights[4] = {
                height_quarter,
                height_quarter + (extra_height > 2 ? 1 : 0),
                height_quarter + (extra_height ? 1 : 0),
                height_quarter + (extra_height > 1 ? 1 : 0)
        };

        cairo_set_line_width(cr, 0);

        auto y0 = y;
        for (auto i = 0; i < 4; ++i) {
                auto x0 = x;
                for (auto j = 0; j < 4; ++j) {
                        if (value & 0b1u)
                                cairo_rectangle(cr, x0, y0, widths[j], heights[i]);
                        value >>= 1;
                        x0 += widths[j];
                }

                y0 += heights[i];
        }

        cairo_fill(cr);
}

inline constexpr int
scanline_y(int value,
           int height,
           int line_width) noexcept
{
        /* There are 9 scanlines, but only the odd scanlines (1, 3, 5, 7,
         * and 9) are actually in unicode.
         * To get the space assigned to each scanline, we divide the
         * height by 9 and distribute the remainder space in this order:
         * scanline 5, 4, 7, 2, 6, 3, 8, 1.
         * This ensures that the remainder is added first to the bottom
         * half towards the centre, and that the spacing between the odd
         * scanlines are at most 1px different.
         *
         * Since scanline 5 is unified with U+2500 BOX DRAWINGS LIGHT HORIZONTAL,
         * the other scanlines are aligned so that scanline 5 coincides with
         * U+2500, that is, has y position upper_half - light_line_width / 2.
         */

        // FIMXE: this doesn't work for height < 9. Since we only need the odd
        // scanlines, we can make this work fine for height = 5..8, but for
        // heights < 5, need to still at least align 1 to top, 5 to middle, and
        // 9 to bottom

        auto const h = height / 9;
        auto const r = height % 9;
        auto y = height / 2 - line_width / 2 + (value - 5) * h;

        auto extra = [&r](int v) constexpr noexcept -> auto { return r >= v ? 1 : 0; };

        switch (value) {
        case 1: y -= extra(8); [[fallthrough]];
        case 2: y -= extra(4); [[fallthrough]];
        case 3: y -= extra(6); [[fallthrough]];
        case 4: y -= extra(2); [[fallthrough]];
        case 5: break;
        case 9: y += extra(7); [[fallthrough]];
        case 8: y += extra(3); [[fallthrough]];
        case 7: y += extra(5); [[fallthrough]];
        case 6: y += extra(1); break;
        default: __builtin_unreachable(); break;
        }

        return y;
}

inline void
scanline(cairo_t* cr,
         int value,
         int x,
         int y,
         int width,
         int height,
         int line_width) noexcept
{
        cairo_rectangle(cr,
                        x,
                        y + scanline_y(value, height, line_width),
                        width,
                        line_width);
        cairo_fill(cr);
}

inline void
circle_segment(cairo_t* cr,
               int x,
               int y,
               int width,
               int height,
               int line_width,
               int dx,
               int dy,
               int r) noexcept
{
        // The naive way to draw the ellipse would lead to non-uniform stroke
        // width.  To make the stroke width uniform, restore the transformation
        // before stroking.  See https://www.cairographics.org/cookbook/ellipses/

        cairo_rectangle(cr, x, y, width, height);
        cairo_clip(cr);

        auto matrix = cairo_matrix_t{};
        cairo_get_matrix(cr, &matrix);

        cairo_translate(cr, x + dx * width, y + dy * height);
        cairo_scale(cr, 1., double(height) / double(width));
        cairo_new_sub_path(cr); // or cairo_new_path() ?
        cairo_arc(cr,
                  0., 0., // centre
                  r * width - line_width, // radius
                  0., // start angle
                  2. * M_PI); // end angle
        cairo_close_path(cr);

        cairo_set_matrix(cr, &matrix);
        cairo_set_line_width(cr, line_width);
        cairo_stroke(cr);
}

static void
polygon(cairo_t* cr,
        double x,
        double y,
        double w,
        double h,
        int xdenom,
        int ydenom,
        int8_t const* cc)
{
        int x1 = (w) * (cc[0]) / (xdenom);
        int y1 = (h) * (cc[1]) / (ydenom);
        cairo_move_to ((cr), (x) + x1, (y) + y1);
        int i = 2;
        while (cc[i] != -1) {
                x1 = (w) * (cc[i]) / (xdenom);
                y1 = (h) * (cc[i + 1]) / (ydenom);
                cairo_line_to ((cr), (x) + x1, (y) + y1);
                i += 2;
        }
        cairo_fill (cr);
}

static void
pattern(cairo_t* cr,
        cairo_pattern_t* pattern,
        double x,
        double y,
        double width,
        double height)
{
        cairo_rectangle(cr, x, y, width, height);
        cairo_clip(cr);
        cairo_mask(cr, pattern);
}

/* Create separated mosaic patterns.
 * Transparent pixels will not be drawn; opaque pixels will draw that part of the
 * mosaic onto the target surface.
 */

inline vte::Freeable<cairo_pattern_t>
create_quadrant_separation_pattern(int width,
                                   int height,
                                   int line_thickness)
{
        auto surface = vte::take_freeable(cairo_image_surface_create(CAIRO_FORMAT_A1, width, height));
        // or CAIRO_FORMAT_A8, whichever is better/faster?

        auto cr = vte::take_freeable(cairo_create(surface.get()));

        /* It's not quite clear how the separated quadrants should be drawn.
         *
         * The L2/21-235 Sources document shows the separation being drawn as
         * blanking a line on the left and top parts of each 2x2 block.
         *
         * Here, we blank a line on the left and *bottom* of each 2x2 block,
         * for consistency with how we draw the separated sextants / mosaics,
         * see below.
         */

        /* First, fill completely with transparent pixels */
        cairo_set_source_rgba(cr.get(), 0., 0., 0., 0.);
        cairo_rectangle(cr.get(), 0, 0, width, height);
        cairo_fill(cr.get());

        /* Now, fill the reduced blocks with opaque pixels */

        auto const pel = line_thickness; /* see the separated sextants below */

        cairo_set_source_rgba(cr.get(), 0., 0., 0., 1.);

        if (width > 2 * pel && height > 2 * pel) {

                auto const width_half = width / 2;
                auto const height_half = height / 2;

                int const y[3] = { 0, height_half, height };
                int const x[3] = { 0, width_half, width };
                // FIXMEchpe: or use 2 * width_half instead of width, so that for width odd,
                // the extra row of pixels is unlit, and the lit blocks have equal width?
                // and similar for height?

                for (auto yi = 0; yi < 2; ++yi) {
                        for (auto xi = 0; xi < 2; xi++) {
                                cairo_rectangle(cr.get(),
                                                x[xi] + pel,
                                                y[yi],
                                                x[xi+1] - x[xi] - pel,
                                                y[yi+1] - y[yi] - pel);
                        }
                }
        }

        cairo_fill(cr.get());

        auto pattern = vte::take_freeable(cairo_pattern_create_for_surface(surface.get()));

        cairo_pattern_set_extend(pattern.get(), CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(pattern.get(), CAIRO_FILTER_NEAREST);

        return pattern;
}

inline vte::Freeable<cairo_pattern_t>
create_sextant_separation_pattern(int width,
                                  int height,
                                  int line_thickness)
{
        auto surface = vte::take_freeable(cairo_image_surface_create(CAIRO_FORMAT_A1, width, height));
        // or CAIRO_FORMAT_A8, whichever is better/faster?

        auto cr = vte::take_freeable(cairo_create(surface.get()));

        /* It's not quite clear how the separated mosaics should be drawn.
         *
         * ITU-T T.101 Annex C, C.2.1.2, and Annex D, D.5.4, show the separation
         * being done by blanking a line on the left and bottom parts only of each
         * of the 3x2 blocks.
         * The minitel specification STUM 1B, Schéma 2.7 also shows them drawn that
         * way.
         *
         * On the other hand, ETS 300 706 §15.7.1, Table 47, shows the separation
         * being done by blanking a line around all four sides of each of the
         * 3x2 blocks.
         * That is also how ITU-T T.100 §5.4.2.1, Figure 6, shows the separation.
         *
         * Each of these has its own drawbacks. The T.101 way makes the 3x2 blocks
         * asymmetric, leaving differing amount of lit pixels for the smooth mosaics
         * comparing a mosaic with its corresponding vertically mirrored mosaic. It
         * keeps more lit pixels overall, which make it more suitable for low-resolution
         * display, which is probably why minitel uses that.
         * The ETS 300 706 way keeps symmetry, but removes even more lit pixels.
         *
         * Here we implement the T.101 way.
         */

        /* FIXMEchpe: Check that this fulfills [T.101 Appendix IV]:
         * "All separated and contiguous mosaics shall be uniquely presented for character
         * field sizes greater than or equal to dx = 6/256, dy = 8/256 [see D.8.3.3, item 7)]."
         */

        /* First, fill completely with transparent pixels */
        cairo_set_source_rgba(cr.get(), 0., 0., 0., 0.);
        cairo_rectangle(cr.get(), 0, 0, width, height);
        cairo_fill(cr.get());

        /* Now, fill the reduced blocks with opaque pixels */

        auto const pel = line_thickness; /* see T.101 D.5.3.2.2.6 for definition of 'logical pel' */

        cairo_set_line_width(cr.get(), 0);

        // If height isn't divisibly by 3, distribute the extra pixels to
        // the middle first, then the bottom.

        if (width > 2 * pel && height > 3 * pel) [[likely]] {

                auto const width_half = width / 2;
                auto const height_third = height / 3;
                auto const extra_height = height % 3 ? 1 : 0;

                // Just like in sextant() above,
                // if height isn't divisibly by 3, distribute the extra pixels to
                // the middle first, then the bottom.

                int const y[] = {0, height_third, height_third * 2 + extra_height, height};
                int const x[] = { 0, width_half, width };
                // FIXMEchpe: or use 2 * width_half instead of width, so that for width odd,
                // the extra row of pixels is unlit, and the lit blocks have equal width?

                cairo_set_source_rgba(cr.get(), 0., 0., 0., 1.);

                for (auto yi = 0; yi < 3; ++yi) {
                        for (auto xi = 0; xi < 2; xi++) {
                                cairo_rectangle(cr.get(), x[xi] + pel, y[yi], x[xi+1] - x[xi] - pel, y[yi+1] - y[yi] - pel);
                        }
                }
        }

        cairo_fill(cr.get());

        auto pattern = vte::take_freeable(cairo_pattern_create_for_surface(surface.get()));

        cairo_pattern_set_extend(pattern.get(), CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(pattern.get(), CAIRO_FILTER_NEAREST);

        return pattern;
}

#include "box-drawing.hh"

namespace vte::view {

// Minifont

void
Minifont::get_char_padding(vteunistr c,
                           int font_width,
                           int font_height,
                           int& xpad,
                           int& ypad) const noexcept
{
        xpad = ypad = 0;

        switch (c) {
        case 0x2571: // box drawings light diagonal upper right to lower left
        case 0x2572: // box drawings light diagonal upper left to lower right
        case 0x2573: // box drawings light diagonal cross
                // U+1FBD0 BOX DRAWINGS LIGHT DIAGONAL MIDDLE RIGHT TO LOWER LEFT ...
                // U+1FBDF BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO MIDDLE RIGHT TO LOWER LEFT
        case 0x1fbd0 ... 0x1fbdf: {
                // These characters draw outside their cell, so we need to
                // enlarge the drawing surface.

                // Exclude the spacing for line width computation.
                auto const light_line_width = std::max(font_width / 5, 1);
                auto const dx = (light_line_width + 1) / 2;
                xpad = dx;
                break;
        }

        default: [[likely]]
                break;
        }
}

/* Draw the graphic representation of a line-drawing or special graphics
 * character. */
void
Minifont::draw_graphic(cairo_t* cr,
                       vteunistr c,
                       vte::color::rgb const* fg,
                       int cell_width,
                       int cell_height,
                       int x,
                       int y,
                       int font_width,
                       int columns,
                       int font_height,
                       int scale_factor) const
{
        cairo_save(cr);

        auto const width = cell_width * columns;
        auto const height = cell_height;

        int xcenter, xright, ycenter, ybottom;
        int upper_half, left_half;
        int light_line_width, heavy_line_width;
        double adjust;

        upper_half = height / 2;
        left_half = width / 2;

        /* Exclude the spacing for line width computation. */
        light_line_width = font_width / 5;
        light_line_width = MAX (light_line_width, 1);

        if (c >= 0x2550 && c <= 0x256c) {
                heavy_line_width = 3 * light_line_width;
        } else {
                heavy_line_width = light_line_width + 2;
        }

        xcenter = x + left_half;
        ycenter = y + upper_half;
        xright = x + width;
        ybottom = y + height;

        switch (c) {

        case 0x23b8: /* LEFT VERTICAL BOX LINE */
                cairo_rectangle(cr,
                                x, y,
                                light_line_width, height);
                cairo_fill(cr);
                break;
        case 0x23b9: /* RIGHT VERTICAL BOX LINE */
                cairo_rectangle(cr,
                                x + width - light_line_width, y,
                                light_line_width, height);
                cairo_fill(cr);
                break;

        case 0x23ba: /* HORIZONTAL SCAN LINE-1 */
                scanline(cr, 1, x, y, width, height, light_line_width);
                break;
        case 0x23bb: /* HORIZONTAL SCAN LINE-3 */
                scanline(cr, 3, x, y, width, height, light_line_width);
                break;

        /* Note: HORIZONTAL SCAN LINE-5 is unified with U+2500 BOX DRAWINGS LIGHT HORIZONTAL */

        case 0x23bc: /* HORIZONTAL SCAN LINE-7 */
                scanline(cr, 7, x, y, width, height, light_line_width);
                break;
        case 0x23bd: /* HORIZONTAL SCAN LINE-9 */
                scanline(cr, 9, x, y, width, height, light_line_width);
                break;

        /* Box Drawing */
        case 0x1fbaf: /* box drawings light horizontal with vertical stroke */
                rectangle(cr, x + left_half - light_line_width / 2, y,
                          light_line_width, height, 1, 3, 0, 1, 1, 2);
                c = 0x2500;
                [[fallthrough]];
        case 0x2500: /* box drawings light horizontal */
        case 0x2501: /* box drawings heavy horizontal */
        case 0x2502: /* box drawings light vertical */
        case 0x2503: /* box drawings heavy vertical */
        case 0x250c: /* box drawings light down and right */
        case 0x250d: /* box drawings down light and right heavy */
        case 0x250e: /* box drawings down heavy and right light */
        case 0x250f: /* box drawings heavy down and right */
        case 0x2510: /* box drawings light down and left */
        case 0x2511: /* box drawings down light and left heavy */
        case 0x2512: /* box drawings down heavy and left light */
        case 0x2513: /* box drawings heavy down and left */
        case 0x2514: /* box drawings light up and right */
        case 0x2515: /* box drawings up light and right heavy */
        case 0x2516: /* box drawings up heavy and right light */
        case 0x2517: /* box drawings heavy up and right */
        case 0x2518: /* box drawings light up and left */
        case 0x2519: /* box drawings up light and left heavy */
        case 0x251a: /* box drawings up heavy and left light */
        case 0x251b: /* box drawings heavy up and left */
        case 0x251c: /* box drawings light vertical and right */
        case 0x251d: /* box drawings vertical light and right heavy */
        case 0x251e: /* box drawings up heavy and right down light */
        case 0x251f: /* box drawings down heavy and right up light */
        case 0x2520: /* box drawings vertical heavy and right light */
        case 0x2521: /* box drawings down light and right up heavy */
        case 0x2522: /* box drawings up light and right down heavy */
        case 0x2523: /* box drawings heavy vertical and right */
        case 0x2524: /* box drawings light vertical and left */
        case 0x2525: /* box drawings vertical light and left heavy */
        case 0x2526: /* box drawings up heavy and left down light */
        case 0x2527: /* box drawings down heavy and left up light */
        case 0x2528: /* box drawings vertical heavy and left light */
        case 0x2529: /* box drawings down light and left up heavy */
        case 0x252a: /* box drawings up light and left down heavy */
        case 0x252b: /* box drawings heavy vertical and left */
        case 0x252c: /* box drawings light down and horizontal */
        case 0x252d: /* box drawings left heavy and right down light */
        case 0x252e: /* box drawings right heavy and left down light */
        case 0x252f: /* box drawings down light and horizontal heavy */
        case 0x2530: /* box drawings down heavy and horizontal light */
        case 0x2531: /* box drawings right light and left down heavy */
        case 0x2532: /* box drawings left light and right down heavy */
        case 0x2533: /* box drawings heavy down and horizontal */
        case 0x2534: /* box drawings light up and horizontal */
        case 0x2535: /* box drawings left heavy and right up light */
        case 0x2536: /* box drawings right heavy and left up light */
        case 0x2537: /* box drawings up light and horizontal heavy */
        case 0x2538: /* box drawings up heavy and horizontal light */
        case 0x2539: /* box drawings right light and left up heavy */
        case 0x253a: /* box drawings left light and right up heavy */
        case 0x253b: /* box drawings heavy up and horizontal */
        case 0x253c: /* box drawings light vertical and horizontal */
        case 0x253d: /* box drawings left heavy and right vertical light */
        case 0x253e: /* box drawings right heavy and left vertical light */
        case 0x253f: /* box drawings vertical light and horizontal heavy */
        case 0x2540: /* box drawings up heavy and down horizontal light */
        case 0x2541: /* box drawings down heavy and up horizontal light */
        case 0x2542: /* box drawings vertical heavy and horizontal light */
        case 0x2543: /* box drawings left up heavy and right down light */
        case 0x2544: /* box drawings right up heavy and left down light */
        case 0x2545: /* box drawings left down heavy and right up light */
        case 0x2546: /* box drawings right down heavy and left up light */
        case 0x2547: /* box drawings down light and up horizontal heavy */
        case 0x2548: /* box drawings up light and down horizontal heavy */
        case 0x2549: /* box drawings right light and left vertical heavy */
        case 0x254a: /* box drawings left light and right vertical heavy */
        case 0x254b: /* box drawings heavy vertical and horizontal */
        case 0x2550: /* box drawings double horizontal */
        case 0x2551: /* box drawings double vertical */
        case 0x2552: /* box drawings down single and right double */
        case 0x2553: /* box drawings down double and right single */
        case 0x2554: /* box drawings double down and right */
        case 0x2555: /* box drawings down single and left double */
        case 0x2556: /* box drawings down double and left single */
        case 0x2557: /* box drawings double down and left */
        case 0x2558: /* box drawings up single and right double */
        case 0x2559: /* box drawings up double and right single */
        case 0x255a: /* box drawings double up and right */
        case 0x255b: /* box drawings up single and left double */
        case 0x255c: /* box drawings up double and left single */
        case 0x255d: /* box drawings double up and left */
        case 0x255e: /* box drawings vertical single and right double */
        case 0x255f: /* box drawings vertical double and right single */
        case 0x2560: /* box drawings double vertical and right */
        case 0x2561: /* box drawings vertical single and left double */
        case 0x2562: /* box drawings vertical double and left single */
        case 0x2563: /* box drawings double vertical and left */
        case 0x2564: /* box drawings down single and horizontal double */
        case 0x2565: /* box drawings down double and horizontal single */
        case 0x2566: /* box drawings double down and horizontal */
        case 0x2567: /* box drawings up single and horizontal double */
        case 0x2568: /* box drawings up double and horizontal single */
        case 0x2569: /* box drawings double up and horizontal */
        case 0x256a: /* box drawings vertical single and horizontal double */
        case 0x256b: /* box drawings vertical double and horizontal single */
        case 0x256c: /* box drawings double vertical and horizontal */
        case 0x2574: /* box drawings light left */
        case 0x2575: /* box drawings light up */
        case 0x2576: /* box drawings light right */
        case 0x2577: /* box drawings light down */
        case 0x2578: /* box drawings heavy left */
        case 0x2579: /* box drawings heavy up */
        case 0x257a: /* box drawings heavy right */
        case 0x257b: /* box drawings heavy down */
        case 0x257c: /* box drawings light left and heavy right */
        case 0x257d: /* box drawings light up and heavy down */
        case 0x257e: /* box drawings heavy left and light right */
        case 0x257f: /* box drawings heavy up and light down */
        {
                guint32 bitmap = _vte_draw_box_drawing_bitmaps[c - 0x2500];
                int xboundaries[6] = { 0,
                                       left_half - heavy_line_width / 2,
                                       left_half - light_line_width / 2,
                                       left_half - light_line_width / 2 + light_line_width,
                                       left_half - heavy_line_width / 2 + heavy_line_width,
                                       width};
                int yboundaries[6] = { 0,
                                       upper_half - heavy_line_width / 2,
                                       upper_half - light_line_width / 2,
                                       upper_half - light_line_width / 2 + light_line_width,
                                       upper_half - heavy_line_width / 2 + heavy_line_width,
                                       height};
                int xi, yi;
                cairo_set_line_width(cr, 0);
                for (yi = 4; yi >= 0; yi--) {
                        for (xi = 4; xi >= 0; xi--) {
                                if (bitmap & 1) {
                                        cairo_rectangle(cr,
                                                        x + xboundaries[xi],
                                                        y + yboundaries[yi],
                                                        xboundaries[xi + 1] - xboundaries[xi],
                                                        yboundaries[yi + 1] - yboundaries[yi]);
                                        cairo_fill(cr);
                                }
                                bitmap >>= 1;
                        }
                }
                break;
        }

        case 0x2504: /* box drawings light triple dash horizontal */
        case 0x2505: /* box drawings heavy triple dash horizontal */
        case 0x2506: /* box drawings light triple dash vertical */
        case 0x2507: /* box drawings heavy triple dash vertical */
        case 0x2508: /* box drawings light quadruple dash horizontal */
        case 0x2509: /* box drawings heavy quadruple dash horizontal */
        case 0x250a: /* box drawings light quadruple dash vertical */
        case 0x250b: /* box drawings heavy quadruple dash vertical */
        case 0x254c: /* box drawings light double dash horizontal */
        case 0x254d: /* box drawings heavy double dash horizontal */
        case 0x254e: /* box drawings light double dash vertical */
        case 0x254f: /* box drawings heavy double dash vertical */
        {
                const guint v = c - 0x2500;
                int size, line_width;

                size = (v & 2) ? height : width;

                switch (v >> 2) {
                case 1: /* triple dash */
                {
                        double segment = size / 8.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 2: /* quadruple dash */
                {
                        double segment = size / 11.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 19: /* double dash */
                {
                        double segment = size / 5.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                }

                line_width = (v & 1) ? heavy_line_width : light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;

                cairo_set_line_width(cr, line_width);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
                if (v & 2) {
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, y + height);
                } else {
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, x + width, ycenter + adjust);
                }
                cairo_stroke(cr);
                break;
        }

        case 0x256d: /* box drawings light arc down and right */
        case 0x256e: /* box drawings light arc down and left */
        case 0x256f: /* box drawings light arc up and left */
        case 0x2570: /* box drawings light arc up and right */
        {
                const guint v = c - 0x256d;
                int line_width;
                int radius;

                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

                line_width = light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;
                cairo_set_line_width(cr, line_width);

                radius = (font_width + 2) / 3;
                radius = MAX(radius, heavy_line_width);

                if (v & 2) {
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, ycenter - radius + 2 * adjust);
                } else {
                        cairo_move_to(cr, xcenter + adjust, ybottom);
                        cairo_line_to(cr, xcenter + adjust, ycenter + radius);
                }
                cairo_stroke(cr);

                cairo_arc(cr,
                          (v == 1 || v == 2) ? xcenter - radius + 2 * adjust
                                             : xcenter + radius,
                          (v & 2) ? ycenter - radius + 2 * adjust
                                  : ycenter + radius,
                          radius - adjust,
                          (v + 2) * M_PI / 2.0, (v + 3) * M_PI / 2.0);
                cairo_stroke(cr);

                if (v == 1 || v == 2) {
                        cairo_move_to(cr, xcenter - radius + 2 * adjust, ycenter + adjust);
                        cairo_line_to(cr, x, ycenter + adjust);
                } else {
                        cairo_move_to(cr, xcenter + radius, ycenter + adjust);
                        cairo_line_to(cr, xright, ycenter + adjust);
                }

                cairo_stroke(cr);
                break;
        }

        case 0x2571: /* box drawings light diagonal upper right to lower left */
        case 0x2572: /* box drawings light diagonal upper left to lower right */
        case 0x2573: /* box drawings light diagonal cross */
                diagonal_slope_1_1(cr, x, y, width, height, light_line_width, c & 3);
                break;

        /* Block Elements */
        case 0x2580: /* upper half block */
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x2581: /* lower one eighth block */
        case 0x2582: /* lower one quarter block */
        case 0x2583: /* lower three eighths block */
        case 0x2584: /* lower half block */
        case 0x2585: /* lower five eighths block */
        case 0x2586: /* lower three quarters block */
        case 0x2587: /* lower seven eighths block */
        {
                const guint v = 0x2588 - c;
                rectangle(cr, x, y, width, height, 1, 8,  0, v,  1, 8);
                break;
        }

        case 0x2588: /* full block */
        case 0x2589: /* left seven eighths block */
        case 0x258a: /* left three quarters block */
        case 0x258b: /* left five eighths block */
        case 0x258c: /* left half block */
        case 0x258d: /* left three eighths block */
        case 0x258e: /* left one quarter block */
        case 0x258f: /* left one eighth block */
        {
                const guint v = 0x2590 - c;
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  v, 1);
                break;
        }

        case 0x2590: /* right half block */
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                break;

        case 0x2591: /* light shade */
        case 0x2592: /* medium shade */
        case 0x2593: /* dark shade */
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       (c - 0x2590) / 4.);
                cairo_rectangle(cr, x, y, width, height);
                cairo_fill (cr);
                break;

        case 0x2594: /* upper one eighth block */
        {
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                break;
        }

        case 0x2595: /* right one eighth block */
        {
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;
        }

        case 0x2596: /* quadrant lower left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x2597: /* quadrant lower right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x2598: /* quadrant upper left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                break;

        case 0x2599: /* quadrant upper left and lower left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  2, 2);
                break;

        case 0x259a: /* quadrant upper left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x259b: /* quadrant upper left and upper right and lower left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x259c: /* quadrant upper left and upper right and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x259d: /* quadrant upper right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                break;

        case 0x259e: /* quadrant upper right and lower left */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x259f: /* quadrant upper right and lower left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  2, 2);
                break;

        case 0x25e2: /* black lower right triangle */
        {
                static int8_t const coords[] = { 0, 1,  1, 0,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e3: /* black lower left triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 1,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e4: /* black upper left triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e5: /* black upper right triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb00:
        case 0x1fb01:
        case 0x1fb02:
        case 0x1fb03:
        case 0x1fb04:
        case 0x1fb05:
        case 0x1fb06:
        case 0x1fb07:
        case 0x1fb08:
        case 0x1fb09:
        case 0x1fb0a:
        case 0x1fb0b:
        case 0x1fb0c:
        case 0x1fb0d:
        case 0x1fb0e:
        case 0x1fb0f:
        case 0x1fb10:
        case 0x1fb11:
        case 0x1fb12:
        case 0x1fb13:
        case 0x1fb14:
        case 0x1fb15:
        case 0x1fb16:
        case 0x1fb17:
        case 0x1fb18:
        case 0x1fb19:
        case 0x1fb1a:
        case 0x1fb1b:
        case 0x1fb1c:
        case 0x1fb1d:
        case 0x1fb1e:
        case 0x1fb1f:
        case 0x1fb20:
        case 0x1fb21:
        case 0x1fb22:
        case 0x1fb23:
        case 0x1fb24:
        case 0x1fb25:
        case 0x1fb26:
        case 0x1fb27:
        case 0x1fb28:
        case 0x1fb29:
        case 0x1fb2a:
        case 0x1fb2b:
        case 0x1fb2c:
        case 0x1fb2d:
        case 0x1fb2e:
        case 0x1fb2f:
        case 0x1fb30:
        case 0x1fb31:
        case 0x1fb32:
        case 0x1fb33:
        case 0x1fb34:
        case 0x1fb35:
        case 0x1fb36:
        case 0x1fb37:
        case 0x1fb38:
        case 0x1fb39:
        case 0x1fb3a:
        case 0x1fb3b:
        {
                guint32 bitmap = c - 0x1fb00 + 1;
                if (bitmap >= 0x15) bitmap++;
                if (bitmap >= 0x2a) bitmap++;
                sextant(cr, bitmap, x, y, width, height);
                break;
        }

        case 0x1fb3c:
        case 0x1fb3d:
        case 0x1fb3e:
        case 0x1fb3f:
        case 0x1fb40:
        case 0x1fb41:
        case 0x1fb42:
        case 0x1fb43:
        case 0x1fb44:
        case 0x1fb45:
        case 0x1fb46:
        case 0x1fb47:
        case 0x1fb48:
        case 0x1fb49:
        case 0x1fb4a:
        case 0x1fb4b:
        case 0x1fb4c:
        case 0x1fb4d:
        case 0x1fb4e:
        case 0x1fb4f:
        case 0x1fb50:
        case 0x1fb51:
        case 0x1fb52:
        case 0x1fb53:
        case 0x1fb54:
        case 0x1fb55:
        case 0x1fb56:
        case 0x1fb57:
        case 0x1fb58:
        case 0x1fb59:
        case 0x1fb5a:
        case 0x1fb5b:
        case 0x1fb5c:
        case 0x1fb5d:
        case 0x1fb5e:
        case 0x1fb5f:
        case 0x1fb60:
        case 0x1fb61:
        case 0x1fb62:
        case 0x1fb63:
        case 0x1fb64:
        case 0x1fb65:
        case 0x1fb66:
        case 0x1fb67:
        {
                auto const v = c - 0x1fb3c;
                static int8_t const coords[46][11] = {
                        { 0, 2,  1, 3,  0, 3,  -1 },                /* 3c */
                        { 0, 2,  2, 3,  0, 3,  -1 },                /* 3d */
                        { 0, 1,  1, 3,  0, 3,  -1 },                /* 3e */
                        { 0, 1,  2, 3,  0, 3,  -1 },                /* 3f */
                        { 0, 0,  1, 3,  0, 3,  -1 },                /* 40 */
                        { 0, 1,  1, 0,  2, 0,  2, 3,  0, 3,  -1 },  /* 41 */
                        { 0, 1,  2, 0,  2, 3,  0, 3,  -1 },         /* 42 */
                        { 0, 2,  1, 0,  2, 0,  2, 3,  0, 3,  -1 },  /* 43 */
                        { 0, 2,  2, 0,  2, 3,  0, 3,  -1 },         /* 44 */
                        { 0, 3,  1, 0,  2, 0,  2, 3,  -1 },         /* 45 */
                        { 0, 2,  2, 1,  2, 3,  0, 3,  -1 },         /* 46 */
                        { 1, 3,  2, 2,  2, 3,  -1 },                /* 47 */
                        { 0, 3,  2, 2,  2, 3,  -1 },                /* 48 */
                        { 1, 3,  2, 1,  2, 3,  -1 },                /* 49 */
                        { 0, 3,  2, 1,  2, 3,  -1 },                /* 4a */
                        { 1, 3,  2, 0,  2, 3,  -1 },                /* 4b */
                        { 0, 0,  1, 0,  2, 1,  2, 3,  0, 3,  -1 },  /* 4c */
                        { 0, 0,  2, 1,  2, 3,  0, 3,  -1 },         /* 4d */
                        { 0, 0,  1, 0,  2, 2,  2, 3,  0, 3,  -1 },  /* 4e */
                        { 0, 0,  2, 2,  2, 3,  0, 3,  -1 },         /* 4f */
                        { 0, 0,  1, 0,  2, 3,  0, 3,  -1 },         /* 50 */
                        { 0, 1,  2, 2,  2, 3,  0, 3,  -1 },         /* 51 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  0, 2,  -1 },  /* 52 */
                        { 0, 0,  2, 0,  2, 3,  0, 2,  -1 },         /* 53 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  0, 1,  -1 },  /* 54 */
                        { 0, 0,  2, 0,  2, 3,  0, 1,  -1 },         /* 55 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  -1 },         /* 56 */
                        { 0, 0,  1, 0,  0, 1,  -1 },                /* 57 */
                        { 0, 0,  2, 0,  0, 1,  -1 },                /* 58 */
                        { 0, 0,  1, 0,  0, 2,  -1 },                /* 59 */
                        { 0, 0,  2, 0,  0, 2,  -1 },                /* 5a */
                        { 0, 0,  1, 0,  0, 3,  -1 },                /* 5b */
                        { 0, 0,  2, 0,  2, 1,  0, 2,  -1 },         /* 5c */
                        { 0, 0,  2, 0,  2, 2,  1, 3,  0, 3,  -1 },  /* 5d */
                        { 0, 0,  2, 0,  2, 2,  0, 3,  -1 },         /* 5e */
                        { 0, 0,  2, 0,  2, 1,  1, 3,  0, 3,  -1 },  /* 5f */
                        { 0, 0,  2, 0,  2, 1,  0, 3,  -1 },         /* 60 */
                        { 0, 0,  2, 0,  1, 3,  0, 3,  -1 },         /* 61 */
                        { 1, 0,  2, 0,  2, 1,  -1 },                /* 62 */
                        { 0, 0,  2, 0,  2, 1,  -1 },                /* 63 */
                        { 1, 0,  2, 0,  2, 2,  -1 },                /* 64 */
                        { 0, 0,  2, 0,  2, 2,  -1 },                /* 65 */
                        { 1, 0,  2, 0,  2, 3,  -1 },                /* 66 */
                        { 0, 0,  2, 0,  2, 2,  0, 1,  -1 },         /* 67 */
                };
                polygon(cr, x, y, width, height, 2, 3, coords[v]);
                break;
        }

        case 0x1fb68:
        case 0x1fb69:
        case 0x1fb6a:
        case 0x1fb6b:
        case 0x1fb6c:
        case 0x1fb6d:
        case 0x1fb6e:
        case 0x1fb6f:
        {
                auto const v = c - 0x1fb68;
                static int8_t const coords[8][11] = {
                        { 0, 0,  2, 0,  2, 2,  0, 2,  1, 1,  -1 },  /* 68 */
                        { 0, 0,  1, 1,  2, 0,  2, 2,  0, 2,  -1 },  /* 69 */
                        { 0, 0,  2, 0,  1, 1,  2, 2,  0, 2,  -1 },  /* 6a */
                        { 0, 0,  2, 0,  2, 2,  1, 1,  0, 2,  -1 },  /* 6b */
                        { 0, 0,  1, 1,  0, 2,  -1 },                /* 6c */
                        { 0, 0,  2, 0,  1, 1,  -1 },                /* 6d */
                        { 1, 1,  2, 0,  2, 2,  -1 },                /* 6e */
                        { 1, 1,  2, 2,  0, 2,  -1 },                /* 6f */
                };
                polygon(cr, x, y, width, height, 2, 2, coords[v]);
                break;
        }

        case 0x1fb70:
        case 0x1fb71:
        case 0x1fb72:
        case 0x1fb73:
        case 0x1fb74:
        case 0x1fb75:
        {
                auto const v = c - 0x1fb70 + 1;
                rectangle(cr, x, y, width, height, 8, 1,  v, 0,  v + 1, 1);
                break;
        }

        case 0x1fb76:
        case 0x1fb77:
        case 0x1fb78:
        case 0x1fb79:
        case 0x1fb7a:
        case 0x1fb7b:
        {
                auto const v = c - 0x1fb76 + 1;
                rectangle(cr, x, y, width, height, 1, 8,  0, v,  1, v + 1);
                break;
        }

        case 0x1fb7c:
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  1, 1);
                break;

        case 0x1fb7d:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  1, 1);
                break;

        case 0x1fb7e:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;

        case 0x1fb7f:
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;

        case 0x1fb80:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                break;

        case 0x1fb81:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 1, 8,  0, 2,  1, 3);
                rectangle(cr, x, y, width, height, 1, 8,  0, 4,  1, 5);
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                break;

        case 0x1fb82:
        case 0x1fb83:
        case 0x1fb84:
        case 0x1fb85:
        case 0x1fb86:
        {
                auto v = c - 0x1fb82 + 2;
                if (v >= 4) v++;
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, v);
                break;
        }

        case 0x1fb87:
        case 0x1fb88:
        case 0x1fb89:
        case 0x1fb8a:
        case 0x1fb8b:
        {
                auto v = c - 0x1fb87 + 2;
                if (v >= 4) v++;
                rectangle(cr, x, y, width, height, 8, 1,  8 - v, 0,  8, 1);
                break;
        }

        case 0x1fb8c:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                break;

        case 0x1fb8d:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                break;

        case 0x1fb8e:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x1fb8f:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                break;

        case 0x1fb90:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 1,  0, 0,  1, 1);
                break;

        case 0x1fb91:
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                break;

        case 0x1fb92:
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x1fb93:
#if 0
                /* codepoint not assigned */
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
#endif
                break;

        case 0x1fb94:
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                break;

        case 0x1fb95:
                pattern(cr, create_checkerboard_pattern(), x, y, width, height);
                break;

        case 0x1fb96:
                pattern(cr, create_checkerboard_reverse_pattern(), x, y, width, height);
                break;

        case 0x1fb97:
                pattern(cr, create_heavy_horizontal_fill_pattern(), x, y, width, height);
                break;

        case 0x1fb98:
                pattern(cr, create_hatching_pattern_lr(), x, y, width, height);
                break;

        case 0x1fb99:
                pattern(cr, create_hatching_pattern_rl(), x, y, width, height);
                break;

        case 0x1fb9a:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9b:
        {
                static int8_t coords[] = { 0, 0,  1, 1,  1, 0,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9c:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9d:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  1, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9e:
        {
                static int8_t const coords[] = { 0, 1,  1, 0,  1, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9f:
        {
                static int8_t const coords[] = { 0, 0,  1, 1,  0, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        // U+1FBA0 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE LEFT
        // U+1FBA1 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE RIGHT
        // U+1FBA2 BOX DRAWINGS LIGHT DIAGONAL MIDDLE LEFT TO LOWER CENTRE
        // U+1FBA3 BOX DRAWINGS LIGHT DIAGONAL MIDDLE RIGHT TO LOWER CENTRE
        // U+1FBA4 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE LEFT TO LOWER CENTRE
        // U+1FBA5 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE RIGHT TO LOWER CENTRE
        // U+1FBA6 BOX DRAWINGS LIGHT DIAGONAL MIDDLE LEFT TO LOWER CENTRE TO MIDDLE RIGHT
        // U+1FBA7 BOX DRAWINGS LIGHT DIAGONAL MIDDLE LEFT TO UPPER CENTRE TO MIDDLE RIGHT
        // U+1FBA8 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE LEFT AND MIDDLE RIGHT TO LOWER CENTRE
        // U+1FBA9 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE RIGHT AND MIDDLE LEFT TO LOWER CENTRE
        // U+1FBAA BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE RIGHT TO LOWER CENTRE TO MIDDLE LEFT
        // U+1FBAB BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO MIDDLE LEFT TO LOWER CENTRE TO MIDDLE RIGHT
        // U+1FBAC BOX DRAWINGS LIGHT DIAGONAL MIDDLE LEFT TO UPPER CENTRE TO MIDDLE RIGHT TO LOWER CENTRE
        // U+1FBAD BOX DRAWINGS LIGHT DIAGONAL MIDDLE RIGHT TO UPPER CENTRE TO MIDDLE LEFT TO LOWER CENTRE
        // U+1FBAE BOX DRAWINGS LIGHT DIAGONAL DIAMOND
        case 0x1fba0 ... 0x1fbae: {
                static constinit uint8_t const map[15] = { 0b0001, 0b0010, 0b0100, 0b1000, 0b0101, 0b1010, 0b1100, 0b0011,
                                                           0b1001, 0b0110, 0b1110, 0b1101, 0b1011, 0b0111, 0b1111 };
                middle_diagonal(cr, x, y, width, height, light_line_width, map[c - 0x1fba0]);
                break;
        }

                // U+1FBBD NEGATIVE DIAGONAL CROSS
        case 0x1fbbd: {
                cairo_push_group(cr);

                cairo_rectangle(cr, x, y, width, height);
                cairo_fill(cr);
                cairo_set_source_rgba(cr, 0, 0, 0, 0);
                cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);

                diagonal_slope_1_1(cr, x, y, width, height, light_line_width, 3);

                cairo_pop_group_to_source(cr);
                cairo_paint(cr);
                break;
        }

                // U+1FBBE NEGATIVE DIAGONAL MIDDLE RIGHT TO LOWER CENTRE
                // U+1FBBF NEGATIVE DIAGONAL DIAMOND
        case 0x1fbbe ... 0x1fbbf: {
                static constinit uint8_t const map[2] = { 0b1000, 0b1111 };
                cairo_push_group(cr);

                cairo_rectangle(cr, x, y, width, height);
                cairo_fill(cr);
                cairo_set_source_rgba(cr, 0, 0, 0, 0);
                cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);

                middle_diagonal(cr, x, y, width, height, light_line_width, map[c - 0x1fbbe]);
                cairo_pop_group_to_source(cr);
                cairo_paint(cr);
                break;
        }

        case 0x1cc21 ... 0x1cc2f: { /* separated block quadrant-* */
                cairo_push_group(cr);
                quadrant(cr, c - 0x1cc10, x, y, width, height);
                cairo_pop_group_to_source(cr);
                cairo_mask(cr, create_quadrant_separation_pattern(width, height, light_line_width).get());
                break;
        }

        case 0x1ce51 ... 0x1ce8f: { /* separated block sextant-* */
                cairo_push_group(cr);
                sextant(cr, c - 0x1ce50, x, y, width, height);
                cairo_pop_group_to_source(cr);
                cairo_mask(cr, create_sextant_separation_pattern(width, height, light_line_width).get());
                break;
        }

        case 0x1cd00 ... 0x1cde5: { /* block octant-* */
                static constinit uint8_t const octant_value [] = {
                        0b0000'0100, /* U+1CD00 BLOCK OCTANT-3       */
                        0b0000'0110, /* U+1CD01 BLOCK OCTANT-23      */
                        0b0000'0111, /* U+1CD02 BLOCK OCTANT-123     */
                        0b0000'1000, /* U+1CD03 BLOCK OCTANT-4       */
                        0b0000'1001, /* U+1CD04 BLOCK OCTANT-14      */
                        0b0000'1011, /* U+1CD05 BLOCK OCTANT-124     */
                        0b0000'1100, /* U+1CD06 BLOCK OCTANT-34      */
                        0b0000'1101, /* U+1CD07 BLOCK OCTANT-134     */
                        0b0000'1110, /* U+1CD08 BLOCK OCTANT-234     */
                        0b0001'0000, /* U+1CD09 BLOCK OCTANT-5       */
                        0b0001'0001, /* U+1CD0A BLOCK OCTANT-15      */
                        0b0001'0010, /* U+1CD0B BLOCK OCTANT-25      */
                        0b0001'0011, /* U+1CD0C BLOCK OCTANT-125     */
                        0b0001'0101, /* U+1CD0D BLOCK OCTANT-135     */
                        0b0001'0110, /* U+1CD0E BLOCK OCTANT-235     */
                        0b0001'0111, /* U+1CD0F BLOCK OCTANT-1235    */
                        0b0001'1000, /* U+1CD10 BLOCK OCTANT-45      */
                        0b0001'1001, /* U+1CD11 BLOCK OCTANT-145     */
                        0b0001'1010, /* U+1CD12 BLOCK OCTANT-245     */
                        0b0001'1011, /* U+1CD13 BLOCK OCTANT-1245    */
                        0b0001'1100, /* U+1CD14 BLOCK OCTANT-345     */
                        0b0001'1101, /* U+1CD15 BLOCK OCTANT-1345    */
                        0b0001'1110, /* U+1CD16 BLOCK OCTANT-2345    */
                        0b0001'1111, /* U+1CD17 BLOCK OCTANT-12345   */
                        0b0010'0000, /* U+1CD18 BLOCK OCTANT-6       */
                        0b0010'0001, /* U+1CD19 BLOCK OCTANT-16      */
                        0b0010'0010, /* U+1CD1A BLOCK OCTANT-26      */
                        0b0010'0011, /* U+1CD1B BLOCK OCTANT-126     */
                        0b0010'0100, /* U+1CD1C BLOCK OCTANT-36      */
                        0b0010'0101, /* U+1CD1D BLOCK OCTANT-136     */
                        0b0010'0110, /* U+1CD1E BLOCK OCTANT-236     */
                        0b0010'0111, /* U+1CD1F BLOCK OCTANT-1236    */
                        0b0010'1001, /* U+1CD20 BLOCK OCTANT-146     */
                        0b0010'1010, /* U+1CD21 BLOCK OCTANT-246     */
                        0b0010'1011, /* U+1CD22 BLOCK OCTANT-1246    */
                        0b0010'1100, /* U+1CD23 BLOCK OCTANT-346     */
                        0b0010'1101, /* U+1CD24 BLOCK OCTANT-1346    */
                        0b0010'1110, /* U+1CD25 BLOCK OCTANT-2346    */
                        0b0010'1111, /* U+1CD26 BLOCK OCTANT-12346   */
                        0b0011'0000, /* U+1CD27 BLOCK OCTANT-56      */
                        0b0011'0001, /* U+1CD28 BLOCK OCTANT-156     */
                        0b0011'0010, /* U+1CD29 BLOCK OCTANT-256     */
                        0b0011'0011, /* U+1CD2A BLOCK OCTANT-1256    */
                        0b0011'0100, /* U+1CD2B BLOCK OCTANT-356     */
                        0b0011'0101, /* U+1CD2C BLOCK OCTANT-1356    */
                        0b0011'0110, /* U+1CD2D BLOCK OCTANT-2356    */
                        0b0011'0111, /* U+1CD2E BLOCK OCTANT-12356   */
                        0b0011'1000, /* U+1CD2F BLOCK OCTANT-456     */
                        0b0011'1001, /* U+1CD30 BLOCK OCTANT-1456    */
                        0b0011'1010, /* U+1CD31 BLOCK OCTANT-2456    */
                        0b0011'1011, /* U+1CD32 BLOCK OCTANT-12456   */
                        0b0011'1100, /* U+1CD33 BLOCK OCTANT-3456    */
                        0b0011'1101, /* U+1CD34 BLOCK OCTANT-13456   */
                        0b0011'1110, /* U+1CD35 BLOCK OCTANT-23456   */
                        0b0100'0001, /* U+1CD36 BLOCK OCTANT-17      */
                        0b0100'0010, /* U+1CD37 BLOCK OCTANT-27      */
                        0b0100'0011, /* U+1CD38 BLOCK OCTANT-127     */
                        0b0100'0100, /* U+1CD39 BLOCK OCTANT-37      */
                        0b0100'0101, /* U+1CD3A BLOCK OCTANT-137     */
                        0b0100'0110, /* U+1CD3B BLOCK OCTANT-237     */
                        0b0100'0111, /* U+1CD3C BLOCK OCTANT-1237    */
                        0b0100'1000, /* U+1CD3D BLOCK OCTANT-47      */
                        0b0100'1001, /* U+1CD3E BLOCK OCTANT-147     */
                        0b0100'1010, /* U+1CD3F BLOCK OCTANT-247     */
                        0b0100'1011, /* U+1CD40 BLOCK OCTANT-1247    */
                        0b0100'1100, /* U+1CD41 BLOCK OCTANT-347     */
                        0b0100'1101, /* U+1CD42 BLOCK OCTANT-1347    */
                        0b0100'1110, /* U+1CD43 BLOCK OCTANT-2347    */
                        0b0100'1111, /* U+1CD44 BLOCK OCTANT-12347   */
                        0b0101'0001, /* U+1CD45 BLOCK OCTANT-157     */
                        0b0101'0010, /* U+1CD46 BLOCK OCTANT-257     */
                        0b0101'0011, /* U+1CD47 BLOCK OCTANT-1257    */
                        0b0101'0100, /* U+1CD48 BLOCK OCTANT-357     */
                        0b0101'0110, /* U+1CD49 BLOCK OCTANT-2357    */
                        0b0101'0111, /* U+1CD4A BLOCK OCTANT-12357   */
                        0b0101'1000, /* U+1CD4B BLOCK OCTANT-457     */
                        0b0101'1001, /* U+1CD4C BLOCK OCTANT-1457    */
                        0b0101'1011, /* U+1CD4D BLOCK OCTANT-12457   */
                        0b0101'1100, /* U+1CD4E BLOCK OCTANT-3457    */
                        0b0101'1101, /* U+1CD4F BLOCK OCTANT-13457   */
                        0b0101'1110, /* U+1CD50 BLOCK OCTANT-23457   */
                        0b0110'0000, /* U+1CD51 BLOCK OCTANT-67      */
                        0b0110'0001, /* U+1CD52 BLOCK OCTANT-167     */
                        0b0110'0010, /* U+1CD53 BLOCK OCTANT-267     */
                        0b0110'0011, /* U+1CD54 BLOCK OCTANT-1267    */
                        0b0110'0100, /* U+1CD55 BLOCK OCTANT-367     */
                        0b0110'0101, /* U+1CD56 BLOCK OCTANT-1367    */
                        0b0110'0110, /* U+1CD57 BLOCK OCTANT-2367    */
                        0b0110'0111, /* U+1CD58 BLOCK OCTANT-12367   */
                        0b0110'1000, /* U+1CD59 BLOCK OCTANT-467     */
                        0b0110'1001, /* U+1CD5A BLOCK OCTANT-1467    */
                        0b0110'1010, /* U+1CD5B BLOCK OCTANT-2467    */
                        0b0110'1011, /* U+1CD5C BLOCK OCTANT-12467   */
                        0b0110'1100, /* U+1CD5D BLOCK OCTANT-3467    */
                        0b0110'1101, /* U+1CD5E BLOCK OCTANT-13467   */
                        0b0110'1110, /* U+1CD5F BLOCK OCTANT-23467   */
                        0b0110'1111, /* U+1CD60 BLOCK OCTANT-123467  */
                        0b0111'0000, /* U+1CD61 BLOCK OCTANT-567     */
                        0b0111'0001, /* U+1CD62 BLOCK OCTANT-1567    */
                        0b0111'0010, /* U+1CD63 BLOCK OCTANT-2567    */
                        0b0111'0011, /* U+1CD64 BLOCK OCTANT-12567   */
                        0b0111'0100, /* U+1CD65 BLOCK OCTANT-3567    */
                        0b0111'0101, /* U+1CD66 BLOCK OCTANT-13567   */
                        0b0111'0110, /* U+1CD67 BLOCK OCTANT-23567   */
                        0b0111'0111, /* U+1CD68 BLOCK OCTANT-123567  */
                        0b0111'1000, /* U+1CD69 BLOCK OCTANT-4567    */
                        0b0111'1001, /* U+1CD6A BLOCK OCTANT-14567   */
                        0b0111'1010, /* U+1CD6B BLOCK OCTANT-24567   */
                        0b0111'1011, /* U+1CD6C BLOCK OCTANT-124567  */
                        0b0111'1100, /* U+1CD6D BLOCK OCTANT-34567   */
                        0b0111'1101, /* U+1CD6E BLOCK OCTANT-134567  */
                        0b0111'1110, /* U+1CD6F BLOCK OCTANT-234567  */
                        0b0111'1111, /* U+1CD70 BLOCK OCTANT-1234567 */
                        0b1000'0001, /* U+1CD71 BLOCK OCTANT-18      */
                        0b1000'0010, /* U+1CD72 BLOCK OCTANT-28      */
                        0b1000'0011, /* U+1CD73 BLOCK OCTANT-128     */
                        0b1000'0100, /* U+1CD74 BLOCK OCTANT-38      */
                        0b1000'0101, /* U+1CD75 BLOCK OCTANT-138     */
                        0b1000'0110, /* U+1CD76 BLOCK OCTANT-238     */
                        0b1000'0111, /* U+1CD77 BLOCK OCTANT-1238    */
                        0b1000'1000, /* U+1CD78 BLOCK OCTANT-48      */
                        0b1000'1001, /* U+1CD79 BLOCK OCTANT-148     */
                        0b1000'1010, /* U+1CD7A BLOCK OCTANT-248     */
                        0b1000'1011, /* U+1CD7B BLOCK OCTANT-1248    */
                        0b1000'1100, /* U+1CD7C BLOCK OCTANT-348     */
                        0b1000'1101, /* U+1CD7D BLOCK OCTANT-1348    */
                        0b1000'1110, /* U+1CD7E BLOCK OCTANT-2348    */
                        0b1000'1111, /* U+1CD7F BLOCK OCTANT-12348   */
                        0b1001'0000, /* U+1CD80 BLOCK OCTANT-58      */
                        0b1001'0001, /* U+1CD81 BLOCK OCTANT-158     */
                        0b1001'0010, /* U+1CD82 BLOCK OCTANT-258     */
                        0b1001'0011, /* U+1CD83 BLOCK OCTANT-1258    */
                        0b1001'0100, /* U+1CD84 BLOCK OCTANT-358     */
                        0b1001'0101, /* U+1CD85 BLOCK OCTANT-1358    */
                        0b1001'0110, /* U+1CD86 BLOCK OCTANT-2358    */
                        0b1001'0111, /* U+1CD87 BLOCK OCTANT-12358   */
                        0b1001'1000, /* U+1CD88 BLOCK OCTANT-458     */
                        0b1001'1001, /* U+1CD89 BLOCK OCTANT-1458    */
                        0b1001'1010, /* U+1CD8A BLOCK OCTANT-2458    */
                        0b1001'1011, /* U+1CD8B BLOCK OCTANT-12458   */
                        0b1001'1100, /* U+1CD8C BLOCK OCTANT-3458    */
                        0b1001'1101, /* U+1CD8D BLOCK OCTANT-13458   */
                        0b1001'1110, /* U+1CD8E BLOCK OCTANT-23458   */
                        0b1001'1111, /* U+1CD8F BLOCK OCTANT-123458  */
                        0b1010'0001, /* U+1CD90 BLOCK OCTANT-168     */
                        0b1010'0010, /* U+1CD91 BLOCK OCTANT-268     */
                        0b1010'0011, /* U+1CD92 BLOCK OCTANT-1268    */
                        0b1010'0100, /* U+1CD93 BLOCK OCTANT-368     */
                        0b1010'0110, /* U+1CD94 BLOCK OCTANT-2368    */
                        0b1010'0111, /* U+1CD95 BLOCK OCTANT-12368   */
                        0b1010'1000, /* U+1CD96 BLOCK OCTANT-468     */
                        0b1010'1001, /* U+1CD97 BLOCK OCTANT-1468    */
                        0b1010'1011, /* U+1CD98 BLOCK OCTANT-12468   */
                        0b1010'1100, /* U+1CD99 BLOCK OCTANT-3468    */
                        0b1010'1101, /* U+1CD9A BLOCK OCTANT-13468   */
                        0b1010'1110, /* U+1CD9B BLOCK OCTANT-23468   */
                        0b1011'0000, /* U+1CD9C BLOCK OCTANT-568     */
                        0b1011'0001, /* U+1CD9D BLOCK OCTANT-1568    */
                        0b1011'0010, /* U+1CD9E BLOCK OCTANT-2568    */
                        0b1011'0011, /* U+1CD9F BLOCK OCTANT-12568   */
                        0b1011'0100, /* U+1CDA0 BLOCK OCTANT-3568    */
                        0b1011'0101, /* U+1CDA1 BLOCK OCTANT-13568   */
                        0b1011'0110, /* U+1CDA2 BLOCK OCTANT-23568   */
                        0b1011'0111, /* U+1CDA3 BLOCK OCTANT-123568  */
                        0b1011'1000, /* U+1CDA4 BLOCK OCTANT-4568    */
                        0b1011'1001, /* U+1CDA5 BLOCK OCTANT-14568   */
                        0b1011'1010, /* U+1CDA6 BLOCK OCTANT-24568   */
                        0b1011'1011, /* U+1CDA7 BLOCK OCTANT-124568  */
                        0b1011'1100, /* U+1CDA8 BLOCK OCTANT-34568   */
                        0b1011'1101, /* U+1CDA9 BLOCK OCTANT-134568  */
                        0b1011'1110, /* U+1CDAA BLOCK OCTANT-234568  */
                        0b1011'1111, /* U+1CDAB BLOCK OCTANT-1234568 */
                        0b1100'0001, /* U+1CDAC BLOCK OCTANT-178     */
                        0b1100'0010, /* U+1CDAD BLOCK OCTANT-278     */
                        0b1100'0011, /* U+1CDAE BLOCK OCTANT-1278    */
                        0b1100'0100, /* U+1CDAF BLOCK OCTANT-378     */
                        0b1100'0101, /* U+1CDB0 BLOCK OCTANT-1378    */
                        0b1100'0110, /* U+1CDB1 BLOCK OCTANT-2378    */
                        0b1100'0111, /* U+1CDB2 BLOCK OCTANT-12378   */
                        0b1100'1000, /* U+1CDB3 BLOCK OCTANT-478     */
                        0b1100'1001, /* U+1CDB4 BLOCK OCTANT-1478    */
                        0b1100'1010, /* U+1CDB5 BLOCK OCTANT-2478    */
                        0b1100'1011, /* U+1CDB6 BLOCK OCTANT-12478   */
                        0b1100'1100, /* U+1CDB7 BLOCK OCTANT-3478    */
                        0b1100'1101, /* U+1CDB8 BLOCK OCTANT-13478   */
                        0b1100'1110, /* U+1CDB9 BLOCK OCTANT-23478   */
                        0b1100'1111, /* U+1CDBA BLOCK OCTANT-123478  */
                        0b1101'0000, /* U+1CDBB BLOCK OCTANT-578     */
                        0b1101'0001, /* U+1CDBC BLOCK OCTANT-1578    */
                        0b1101'0010, /* U+1CDBD BLOCK OCTANT-2578    */
                        0b1101'0011, /* U+1CDBE BLOCK OCTANT-12578   */
                        0b1101'0100, /* U+1CDBF BLOCK OCTANT-3578    */
                        0b1101'0101, /* U+1CDC0 BLOCK OCTANT-13578   */
                        0b1101'0110, /* U+1CDC1 BLOCK OCTANT-23578   */
                        0b1101'0111, /* U+1CDC2 BLOCK OCTANT-123578  */
                        0b1101'1000, /* U+1CDC3 BLOCK OCTANT-4578    */
                        0b1101'1001, /* U+1CDC4 BLOCK OCTANT-14578   */
                        0b1101'1010, /* U+1CDC5 BLOCK OCTANT-24578   */
                        0b1101'1011, /* U+1CDC6 BLOCK OCTANT-124578  */
                        0b1101'1100, /* U+1CDC7 BLOCK OCTANT-34578   */
                        0b1101'1101, /* U+1CDC8 BLOCK OCTANT-134578  */
                        0b1101'1110, /* U+1CDC9 BLOCK OCTANT-234578  */
                        0b1101'1111, /* U+1CDCA BLOCK OCTANT-1234578 */
                        0b1110'0000, /* U+1CDCB BLOCK OCTANT-678     */
                        0b1110'0001, /* U+1CDCC BLOCK OCTANT-1678    */
                        0b1110'0010, /* U+1CDCD BLOCK OCTANT-2678    */
                        0b1110'0011, /* U+1CDCE BLOCK OCTANT-12678   */
                        0b1110'0100, /* U+1CDCF BLOCK OCTANT-3678    */
                        0b1110'0101, /* U+1CDD0 BLOCK OCTANT-13678   */
                        0b1110'0110, /* U+1CDD1 BLOCK OCTANT-23678   */
                        0b1110'0111, /* U+1CDD2 BLOCK OCTANT-123678  */
                        0b1110'1000, /* U+1CDD3 BLOCK OCTANT-4678    */
                        0b1110'1001, /* U+1CDD4 BLOCK OCTANT-14678   */
                        0b1110'1010, /* U+1CDD5 BLOCK OCTANT-24678   */
                        0b1110'1011, /* U+1CDD6 BLOCK OCTANT-124678  */
                        0b1110'1100, /* U+1CDD7 BLOCK OCTANT-34678   */
                        0b1110'1101, /* U+1CDD8 BLOCK OCTANT-134678  */
                        0b1110'1110, /* U+1CDD9 BLOCK OCTANT-234678  */
                        0b1110'1111, /* U+1CDDA BLOCK OCTANT-1234678 */
                        0b1111'0001, /* U+1CDDB BLOCK OCTANT-15678   */
                        0b1111'0010, /* U+1CDDC BLOCK OCTANT-25678   */
                        0b1111'0011, /* U+1CDDD BLOCK OCTANT-125678  */
                        0b1111'0100, /* U+1CDDE BLOCK OCTANT-35678   */
                        0b1111'0110, /* U+1CDDF BLOCK OCTANT-235678  */
                        0b1111'0111, /* U+1CDE0 BLOCK OCTANT-1235678 */
                        0b1111'1000, /* U+1CDE1 BLOCK OCTANT-45678   */
                        0b1111'1001, /* U+1CDE2 BLOCK OCTANT-145678  */
                        0b1111'1011, /* U+1CDE3 BLOCK OCTANT-1245678 */
                        0b1111'1101, /* U+1CDE4 BLOCK OCTANT-1345678 */
                        0b1111'1110, /* U+1CDE5 BLOCK OCTANT-2345678 */
                };
                octant(cr, octant_value[c - 0x1cd00], x, y, width, height);
                break;
        }

        case 0x1ce90 ... 0x1ceaf: { /* sixteenths */
                static constinit uint16_t const sixteenth_value [] = {
                        0b0000'0000'0000'0001, /* U+1CE90 UPPER LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0000'0010, /* U+1CE91 UPPER CENTRE LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0000'0100, /* U+1CE92 UPPER CENTRE RIGHT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0000'1000, /* U+1CE93 UPPER RIGHT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0001'0000, /* U+1CE94 UPPER MIDDLE LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0010'0000, /* U+1CE95 UPPER MIDDLE CENTRE LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0000'0100'0000, /* U+1CE96 UPPER MIDDLE CENTRE RIGHT ONE SIXTEENTH BLOCK */
                        0b0000'0000'1000'0000, /* U+1CE97 UPPER MIDDLE RIGHT ONE SIXTEENTH BLOCK */
                        0b0000'0001'0000'0000, /* U+1CE98 LOWER MIDDLE LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0010'0000'0000, /* U+1CE99 LOWER MIDDLE CENTRE LEFT ONE SIXTEENTH BLOCK */
                        0b0000'0100'0000'0000, /* U+1CE9A LOWER MIDDLE CENTRE RIGHT ONE SIXTEENTH BLOCK */
                        0b0000'1000'0000'0000, /* U+1CE9B LOWER MIDDLE RIGHT ONE SIXTEENTH BLOCK */
                        0b0001'0000'0000'0000, /* U+1CE9C LOWER LEFT ONE SIXTEENTH BLOCK */
                        0b0010'0000'0000'0000, /* U+1CE9D LOWER CENTRE LEFT ONE SIXTEENTH BLOCK */
                        0b0100'0000'0000'0000, /* U+1CE9E LOWER CENTRE RIGHT ONE SIXTEENTH BLOCK */
                        0b1000'0000'0000'0000, /* U+1CE9F LOWER RIGHT ONE SIXTEENTH BLOCK */
                        0b1100'0000'0000'0000, /* U+1CEA0 RIGHT HALF LOWER ONE QUARTER BLOCK */ /* Note: must draw as if BLOCK OCTANT-8 */
                        0b1110'0000'0000'0000, /* U+1CEA1 RIGHT THREE QUARTERS LOWER ONE QUARTER BLOCK */
                        0b0111'0000'0000'0000, /* U+1CEA2 LEFT THREE QUARTERS LOWER ONE QUARTER BLOCK */
                        0b0011'0000'0000'0000, /* U+1CEA3 LEFT HALF LOWER ONE QUARTER BLOCK */ /* Note: must draw as if BLOCK OCTANT-7 */
                        0b0001'0001'0000'0000, /* U+1CEA4 LOWER HALF LEFT ONE QUARTER BLOCK */
                        0b0001'0001'0001'0000, /* U+1CEA5 LOWER THREE QUARTERS LEFT ONE QUARTER BLOCK */
                        0b0000'0001'0001'0001, /* U+1CEA6 UPPER THREE QUARTERS LEFT ONE QUARTER BLOCK */
                        0b0000'0000'0001'0001, /* U+1CEA7 UPPER HALF LEFT ONE QUARTER BLOCK */
                        0b0000'0000'0000'0011, /* U+1CEA8 LEFT HALF UPPER ONE QUARTER BLOCK */ /* Note: must draw as if BLOCK OCTANT-1 */
                        0b0000'0000'0000'0111, /* U+1CEA9 LEFT THREE QUARTERS UPPER ONE QUARTER BLOCK */
                        0b0000'0000'0000'1110, /* U+1CEAA RIGHT THREE QUARTERS UPPER ONE QUARTER BLOCK */
                        0b0000'0000'0000'1100, /* U+1CEAB RIGHT HALF UPPER ONE QUARTER BLOCK */ /* Note: must draw as if BLOCK OCTANT-2 */
                        0b0000'0000'1000'1000, /* U+1CEAC UPPER HALF RIGHT ONE QUARTER BLOCK */
                        0b0000'1000'1000'1000, /* U+1CEAD UPPER THREE QUARTERS RIGHT ONE QUARTER BLOCK */
                        0b1000'1000'1000'0000, /* U+1CEAE LOWER THREE QUARTERS RIGHT ONE QUARTER BLOCK */
                        0b1000'1000'0000'0000, /* U+1CEAF LOWER HALF RIGHT ONE QUARTER BLOCK */
                };
                sixteenth(cr, sixteenth_value[c - 0x1ce90], x, y, width, height);
                break;
        }

        case 0x1fbce:   /* U+1FBCE LEFT TWO THIRDS BLOCK */
        case 0x1fbcf: { /* U+1FBCF LEFT ONE THIRD BLOCK */
                // To make the SGR 7 (reverse) of one be the mirror of the other,
                // don't simply use width/3 for the second.
                auto const width_two_thirds = width * 2 / 3;
                if (c & 1) {
                        cairo_rectangle(cr, x, y, width - width_two_thirds, height);
                } else {
                        cairo_rectangle(cr, x, y, width_two_thirds, height);
                }
                cairo_fill(cr);
                break;
        }

        case 0x1fbd0:   // U+1FBD0 BOX DRAWINGS LIGHT DIAGONAL MIDDLE RIGHT TO LOWER LEFT
        case 0x1fbd1:   // U+1FBD1 BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO MIDDLE LEFT
        case 0x1fbd2:   // U+1FBD2 BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO MIDDLE RIGHT
        case 0x1fbd3: { // U+1FBD3 BOX DRAWINGS LIGHT DIAGONAL MIDDLE LEFT TO LOWER RIGHT
                diagonal_slope_2_1(cr, x, y, width, height, light_line_width, c & 7);
                break;
        }
        case 0x1fbd4:   // U+1FBD4 BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER CENTRE
        case 0x1fbd5:   // U+1FBD5 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO LOWER RIGHT
        case 0x1fbd6:   // U+1FBD6 BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER CENTRE
        case 0x1fbd7: { // U+1FBD7 BOX DRAWINGS LIGHT DIAGONAL UPPER CENTRE TO LOWER LEFT
                // double-slope diagonals
                diagonal_slope_1_2(cr, x, y, width, height, light_line_width, c & 7);
                break;
        }
        case 0x1fbd8:   // U+1FBD8 BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO MIDDLE CENTRE TO UPPER RIGHT
        case 0x1fbd9:   // U+1FBD9 BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO MIDDLE CENTRE TO LOWER RIGHT
        case 0x1fbda:   // U+1FBDA BOX DRAWINGS LIGHT DIAGONAL LOWER LEFT TO MIDDLE CENTRE TO LOWER RIGHT
        case 0x1fbdb: { // U+1FBDB BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO MIDDLE CENTRE TO LOWER LEFT
                // these connect to the diagonals U+2571..U+2573
                diagonal_to_center(cr, x, y, width, height, light_line_width, c);
        break;
        }
        case 0x1fbdc:   // U+1FBDC BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER CENTRE TO UPPER RIGHT
        case 0x1fbde: { // U+1FBDE BOX DRAWINGS LIGHT DIAGONAL LOWER LEFT TO UPPER CENTRE TO LOWER RIGHT
                // these connect to the double-slope diagonals U+1FBD4..U+1FBD7
                auto const v = c == 0x1fbdc ? 4 : 5;
                diagonal_slope_1_2(cr, x, y, width, height, light_line_width, v);
                diagonal_slope_1_2(cr, x, y, width, height, light_line_width, v + 2);
                break;
        }

        case 0x1fbdd:   // U+1FBDD BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO MIDDLE LEFT TO LOWER RIGHT
        case 0x1fbdf: { // U+1FBDF BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO MIDDLE RIGHT TO LOWER LEFT
                // these connect to the half-slope diagonals U+1FBD4..U+1FBD7
                auto const v = c == 0x1fbdd ? 1 : 0;
                diagonal_slope_2_1(cr, x, y, width, height, light_line_width, v);
                diagonal_slope_2_1(cr, x, y, width, height, light_line_width, v + 2);
                break;
        }

        case 0x1fbe4 ... 0x1fbe5: {
                // FIXME make sure this displays exactly as the
                // corresponding sixteenths (see above) would!
                static constinit uint8_t const quadrant_value[] = {
                        0b0001, /* U+1FBE4 UPPER CENTRE ONE QUARTER BLOCK */
                        0b0100, /* U+1FBE5 LOWER CENTRE ONE QUARTER BLOCK */
                };
                auto const dx = width / 4;
                quadrant(cr, quadrant_value[c - 0x1fbe4], x + dx, y, width, height);
                break;
        }
        case 0x1fbe6 ... 0x1fbe7: {
                static constinit uint8_t const octant_value[] = {
                        0b0001'0100, /* U+1FBE6 MIDDLE LEFT ONE QUARTER BLOCK */
                        0b0010'1000, /* U+1FBE7 MIDDLE RIGHT ONE QUARTER BLOCK */
                };
                octant(cr, octant_value[c - 0x1fbe6], x, y, width, height);
                break;
        }

        case 0x1cc1b: /* BOX DRAWING LIGHT HORIZONTAL AND UPPER RIGHT */
        case 0x1cc1c: /* BOX DRAWING LIGHT HORIZONTAL AND LOWER RIGHT */ {
                /* Apparently these have no LEFT counterparts; note that
                 * U+11CC1D..E below are *not* them!
                 */
                auto const top = (c == 0x1cc1b);
                cairo_rectangle(cr,
                                x, y + upper_half - light_line_width / 2,
                                width, light_line_width);
                cairo_rectangle(cr,
                                x + width - light_line_width,
                                y + (top ? 0 : upper_half - light_line_width / 2),
                                light_line_width,
                                (top ? upper_half : height - upper_half) + light_line_width / 2);
                cairo_fill(cr);
                break;
        }

        case 0x1cc1d: /* BOX DRAWING LIGHT TOP AND UPPER LEFT */
        case 0x1cc1e: /* BOX DRAWING LIGHT BOTTOM AND LOWER LEFT */ {
                auto const top = (c == 0x1cc1d);
                auto const ys = scanline_y(top ? 1 : 9, height, light_line_width);

                cairo_rectangle(cr, x, y + ys, width, light_line_width);
                cairo_rectangle(cr, x, y + (top ? ys : upper_half),
                                light_line_width,
                                top ? upper_half - ys : ys - upper_half + light_line_width);
                cairo_fill(cr);
                break;
        }

                // U+1CC1F BOX DRAWINGS DOUBLE DIAGONAL UPPER RIGHT TO LOWER LEFT
                // U+1CC20 BOX DRAWINGS DOUBLE DIAGONAL UPPER LEFT TO LOWER RIGHT
        case 0x1cc1f ... 0x1cc20: {
                diagonal_double(cr, x, y, width, height, light_line_width, c & 1);
                break;
        }

        case 0x1ce16: /* BOX DRAWING LIGHT VERTICAL AND TOP RIGHT */
        case 0x1ce17: /* BOX DRAWING LIGHT VERTICAL AND BOTTOM RIGHT */
        case 0x1ce18: /* BOX DRAWING LIGHT VERTICAL AND TOP LEFT */
        case 0x1ce19: /* BOX DRAWING LIGHT VERTICAL AND BOTTOM LEFT */ {
                auto const top = (c & 1) == 0;
                auto const left = (c >= 0x1ce18);
                auto const sy = scanline_y(top ? 1 : 9, height, light_line_width);

                if (top)
                        cairo_rectangle(cr,
                                        x + left_half - light_line_width / 2,
                                        y + sy,
                                        light_line_width,
                                        height - sy);
                else
                        cairo_rectangle(cr,
                                        x + left_half - light_line_width / 2,
                                        y,
                                        light_line_width,
                                        sy + light_line_width);
                cairo_fill(cr);

                if (left)
                        cairo_rectangle(cr,
                                        x, y + sy,
                                        left_half + light_line_width / 2,
                                        light_line_width);
                else
                        cairo_rectangle(cr,
                                        x + left_half - light_line_width / 2, y + sy,
                                        width - left_half + light_line_width / 2,
                                        light_line_width);
                cairo_fill(cr);

                break;
        }

        case 0x1cc30 ... 0x1cc34: // UPPER LEFT TWELFTH CIRCLE … UPPER MIDDLE LEFT TWELFTH CIRCLE
        case 0x1cc37: // UPPER MIDDLE RIGHT TWELFTH CIRCLE
        case 0x1cc38: // LOWER MIDDLE LEFT TWELFTH CIRCLE
        case 0x1cc3b ... 0x1cc3f: { // LOWER MIDDLE RIGHT TWELFTH CIRCLE … LOWER RIGHT TWELFTH CIRCLE
                // These characters are the 12 segments of a circle inscribed into
                // a 4x4 cell square, in this order: 0x1cc30 +
                //   0 1 2 3
                //   4     7
                //   8     b
                //   c d e f
                //
                // The problem here is that in our usual 1:2 cell aspect,
                // this is a very excentric ellipse, not a circle.

                auto const v = int(c - 0x1cc30);
                circle_segment(cr, x, y, width, height, light_line_width,
                               2 - (v & 0x3), 2 - (v >> 2), 2);
                break;
        }

        case 0x1cc35: // UPPER LEFT QUARTER CIRCLE
        case 0x1cc36: // UPPER RIGHT QUARTER CIRCLE
        case 0x1cc39: // LOWER LEFT QUARTER CIRCLE
        case 0x1cc3a: { // LOWER RIGHT QUARTER CIRCLE
                // These characters are the 4 segments of a circle inscribed into
                // a 2x2 cell square, in this order: 0x1cc30 +
                //   5 6
                //   9 a
                //
                // The problem here is that in our usual 1:2 cell aspect,
                // this is a very excentric ellipse, not a circle.

                auto const v = int(c - 0x1cc30);
                circle_segment(cr, x, y, width, height, light_line_width,
                               2 - (v & 0x3), 2 - (v >> 2), 1);
                break;
        }

        case 0x1cc40: // U+1CC40 SPARSE HORIZONTAL FILL
                pattern(cr, create_sparse_horizontal_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc41: // U+1CC41 SPARSE VERTICAL FILL
                pattern(cr, create_sparse_vertical_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc42: // U+1CC42 ORTHOGONAL CROSSHATCH FILL
                pattern(cr, create_orthogonal_crosshatch_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc43: // U+1CC43 DIAGONAL CROSSHATCH FILL
                pattern(cr, create_diagonal_crosshatch_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc44: // U+1CC44 DENSE VERTICAL FILL
                pattern(cr, create_dense_vertical_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc45: // U+1CC45 DENSE HORIZONTAL FILL
                pattern(cr, create_dense_horizontal_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc46: // U+1CC46 SPECKLE FILL FRAME-1
                pattern(cr, create_speckle_frame1_fill_pattern(), x, y, width, height);
                break;
        case 0x1cc47: // U+1CC47 SPECKLE FILL FRAME-2
                pattern(cr, create_speckle_frame2_fill_pattern(), x, y, width, height);
                break;

                // U+1CE09 BOX DRAWINGS DOUBLE DIAGONAL LOWER LEFT TO MIDDLE CENTRE TO LOWER RIGHT
                // U+1CE0A BOX DRAWINGS DOUBLE DIAGONAL UPPER LEFT TO MIDDLE CENTRE TO UPPER RIGHT
        case 0x1ce09 ... 0x1ce0a:
                diagonal_double_middle(cr, x, y, width, height, light_line_width, c & 1);
                break;

        default:
                cairo_set_source_rgba (cr, 1., 0., 1., 1.);
                cairo_rectangle(cr, x, y, width, height);
                cairo_fill(cr);
                break;
                // g_assert_not_reached();
        }

        cairo_restore(cr);
}

// MinifontCache

vte::Freeable<cairo_t>
MinifontCache::begin_cairo(int x,
                           int y,
                           int width,
                           int height,
                           int xpad,
                           int ypad,
                           int scale_factor) const
{
        auto surface = vte::take_freeable(create_surface(width, height, xpad, ypad, scale_factor));
        auto cr = vte::take_freeable(cairo_create(surface.get()));
        cairo_set_source_rgba(cr.get(), 1, 1, 1, 1);
        cairo_translate(cr.get(), -x + xpad, -y + ypad);
        return cr;
}

#if VTE_GTK == 4
GdkTexture*
MinifontCache::surface_to_texture(cairo_t *cr) const
{
        auto const surface = cairo_get_target(cr);
        cairo_surface_flush(surface);

        auto const data = cairo_image_surface_get_data(surface);
        auto const width = cairo_image_surface_get_width(surface);
        auto const height = cairo_image_surface_get_height(surface);
        auto const stride = cairo_image_surface_get_stride(surface);
        auto const bytes = vte::take_freeable(g_bytes_new(data, height * stride));
        auto const texture = gdk_memory_texture_new(width,
                                                    height,
                                                    GDK_MEMORY_A8,
                                                    bytes.get(),
                                                    stride);
        return texture;
}
#endif // VTE_GTK == 4

void
MinifontCache::draw_graphic(DrawingContext const& context,
                            vteunistr c,
                            vte::color::rgb const* fg,
                            int x,
                            int y,
                            int font_width,
                            int columns,
                            int font_height,
                            int scale_factor)
{
        int width = context.cell_width() * columns;
        int height = context.cell_height();

        auto xoff = 0, yoff = 0;

        switch (c) {
        case 0x1fb95 ... 0x1fb99:
        case 0x1cc40 ... 0x1cc47:
                // actually U+1CC46..7 are 8x8 pattern, but since they're
                // random speckle fills it shouldn't matter too much to
                // only use a 4x4 alignment.
                xoff = x & 0x3;
                yoff = y & 0x3;
                [[fallthrough]];
        default: [[likely]] {
                auto const cached = cached_minifont_lookup(c, width, height, scale_factor, xoff, yoff);

                if (cached) [[likely]] {
                        cached_minifont_draw(cached, context, x, y, width, height, fg);
                        return;
                }
        }
        }

        // Fall back to using the cairo minifont
        auto xpad = 0, ypad = 0;
        get_char_padding(c, font_width, font_height, xpad, ypad);

        auto const cr = begin_cairo(x, y, width, height, xpad, ypad, scale_factor);
        Minifont::draw_graphic(cr.get(),
                               c,
                               fg,
                               context.cell_width(),
                               context.cell_height(),
                               x,
                               y,
                               font_width,
                               columns,
                               font_height,
                               scale_factor);

        // ... and cache the result
        auto mf = g_new0 (CachedMinifont, 1);
        mf->link.data = mf;
        mf->c = c;
        mf->width = width;
        mf->height = height;
        mf->scale_factor = scale_factor;
        mf->x_off = xoff;
        mf->y_off = yoff;
        mf->xpad = xpad;
        mf->ypad = ypad;
#if VTE_GTK == 3
        mf->surface = cairo_surface_reference(cairo_get_target(cr.get()));
#elif VTE_GTK == 4
        mf->texture = surface_to_texture(cr.get());
#endif // VTE_GTK

        cached_minifont_add(mf);

        // ... and draw from cache
        cached_minifont_draw(mf, context, x, y, width, height, fg);
}

// MinifontGsk

#if VTE_GTK == 4

void
MinifontGsk::rectangle(DrawingContext const& context,
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
                       int yb2) const
{
        int const x1 = (w) * (xb1) / (xdenom);
        int const y1 = (h) * (yb1) / (ydenom);
        int const x2 = (w) * (xb2) / (xdenom);
        int const y2 = (h) * (yb2) / (ydenom);

        context.fill_rectangle((x) + x1, (y) + y1, MAX(x2 - x1, 1), MAX(y2 - y1, 1), fg, alpha);
}

void
MinifontGsk::draw_graphic(DrawingContext const& context,
                          vteunistr c,
                          vte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height,
                          int scale_factor)
{
        int width = context.cell_width() * columns;
        int height = context.cell_height();

        // The glyphs we can draw can be separated into two classes.
        //
        // The first class (our fast path), are a simple rectangle
        // or small series of rectangles which can be drawn using
        // GskColorNode on GTK 4.
        //
        // The second class are more complex in that they require
        // drawing arcs or some form of bit pattern that would not
        // be suited well to a GskColorNode per glyph.
        //
        // To avoid overhead for the fast path, we check for those
        // up front before every trying to lookup a CachedMinifont.
        // While GHashTable is fast, it's much slower than doing the
        // least amount of work up-front for the fast path.

        switch (c) {

        /* Block Elements */
        case 0x2580: /* upper half block */
                rectangle(context, fg, 1, x, y, width, height, 1, 2,  0, 0,  1, 1);
                return;

        case 0x2581: /* lower one eighth block */
        case 0x2582: /* lower one quarter block */
        case 0x2583: /* lower three eighths block */
        case 0x2584: /* lower half block */
        case 0x2585: /* lower five eighths block */
        case 0x2586: /* lower three quarters block */
        case 0x2587: /* lower seven eighths block */
        {
                const guint v = 0x2588 - c;
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, v,  1, 8);
                return;
        }

        case 0x2588: /* full block */
        case 0x2589: /* left seven eighths block */
        case 0x258a: /* left three quarters block */
        case 0x258b: /* left five eighths block */
        case 0x258c: /* left half block */
        case 0x258d: /* left three eighths block */
        case 0x258e: /* left one quarter block */
        case 0x258f: /* left one eighth block */
        {
                const guint v = 0x2590 - c;
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  0, 0,  v, 1);
                return;
        }

        case 0x2590: /* right half block */
                rectangle(context, fg, 1, x, y, width, height, 2, 1,  1, 0,  2, 1);
                return;

        case 0x2591: /* light shade */
        case 0x2592: /* medium shade */
        case 0x2593: /* dark shade */
                context.fill_rectangle(x, y, width, height, fg, (c - 0x2590) / 4.);
                return;

        case 0x2594: /* upper one eighth block */
        {
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, 1);
                return;
        }

        case 0x2595: /* right one eighth block */
        {
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  7, 0,  8, 1);
                return;
        }

        case 0x2596: /* quadrant lower left */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 1,  1, 2);
                return;

        case 0x2597: /* quadrant lower right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 1,  2, 2);
                return;

        case 0x2598: /* quadrant upper left */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 0,  1, 1);
                return;

        case 0x2599: /* quadrant upper left and lower left and lower right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 1,  2, 2);
                return;

        case 0x259a: /* quadrant upper left and lower right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 1,  2, 2);
                return;

        case 0x259b: /* quadrant upper left and upper right and lower left */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 1,  1, 2);
                return;

        case 0x259c: /* quadrant upper left and upper right and lower right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 1,  2, 2);
                return;

        case 0x259d: /* quadrant upper right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 0,  2, 1);
                return;

        case 0x259e: /* quadrant upper right and lower left */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 1,  1, 2);
                return;

        case 0x259f: /* quadrant upper right and lower left and lower right */
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(context, fg, 1, x, y, width, height, 2, 2,  0, 1,  2, 2);
                return;

        case 0x1fb70:
        case 0x1fb71:
        case 0x1fb72:
        case 0x1fb73:
        case 0x1fb74:
        case 0x1fb75:
        {
                auto const v = c - 0x1fb70 + 1;
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  v, 0,  v + 1, 1);
                return;
        }

        case 0x1fb76:
        case 0x1fb77:
        case 0x1fb78:
        case 0x1fb79:
        case 0x1fb7a:
        case 0x1fb7b:
        {
                auto const v = c - 0x1fb76 + 1;
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, v,  1, v + 1);
                return;
        }

        case 0x1fb7c:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  0, 0,  1, 1);
                return;

        case 0x1fb7d:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  0, 0,  1, 1);
                return;

        case 0x1fb7e:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  7, 0,  8, 1);
                return;

        case 0x1fb7f:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  7, 0,  8, 1);
                return;

        case 0x1fb80:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 7,  1, 8);
                return;

        case 0x1fb81:
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 2,  1, 3);
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 4,  1, 5);
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 7,  1, 8);
                return;

        case 0x1fb82:
        case 0x1fb83:
        case 0x1fb84:
        case 0x1fb85:
        case 0x1fb86:
        {
                auto v = c - 0x1fb82 + 2;
                if (v >= 4) v++;
                rectangle(context, fg, 1, x, y, width, height, 1, 8,  0, 0,  1, v);
                return;
        }

        case 0x1fb87:
        case 0x1fb88:
        case 0x1fb89:
        case 0x1fb8a:
        case 0x1fb8b:
        {
                auto v = c - 0x1fb87 + 2;
                if (v >= 4) v++;
                rectangle(context, fg, 1, x, y, width, height, 8, 1,  8 - v, 0,  8, 1);
                return;
        }

        case 0x1fb8c:
                rectangle(context, fg, .5, x, y, width, height, 2, 1,  0, 0,  1, 1);
                return;

        case 0x1fb8d:
                rectangle(context, fg, .5, x, y, width, height, 2, 1,  1, 0,  2, 1);
                return;

        case 0x1fb8e:
                rectangle(context, fg, .5, x, y, width, height, 1, 2,  0, 0,  1, 1);
                return;

        case 0x1fb8f:
                rectangle(context, fg, .5, x, y, width, height, 1, 2,  0, 1,  1, 2);
                return;

        case 0x1fb90:
                rectangle(context, fg, .5, x, y, width, height, 1, 1,  0, 0,  1, 1);
                return;

        case 0x1fb91:
                rectangle(context, fg, 1., x, y, width, height, 1, 2,  0, 0,  1, 1);
                rectangle(context, fg, .5, x, y, width, height, 1, 2,  0, 1,  1, 2);
                return;

        case 0x1fb92:
                rectangle(context, fg, 1., x, y, width, height, 1, 2,  0, 1,  1, 2);
                rectangle(context, fg, .5, x, y, width, height, 1, 2,  0, 0,  1, 1);
                return;

        case 0x1fb93:
#if 0
                /* codepoint not assigned */
                rectangle(context, fg, 1., x, y, width, height, 2, 1,  0, 0,  1, 1);
                rectangle(context, fg, .5, x, y, width, height, 2, 1,  1, 0,  2, 1);
#endif
                return;

        case 0x1fb94:
                rectangle(context, fg, 1., x, y, width, height, 2, 1,  1, 0,  2, 1);
                rectangle(context, fg, .5, x, y, width, height, 2, 1,  0, 0,  1, 1);
                return;

        default:
                return MinifontCache::draw_graphic(context, c, fg, x, y, font_width, columns, font_height, scale_factor);
        }
}

#endif // VTE_GTK == 4

} // namespace vte::view
