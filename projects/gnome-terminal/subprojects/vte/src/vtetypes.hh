/*
 * Copyright © 2015 Christian Persch
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

#include <pango/pango.h>
#include <gdk/gdk.h>
#include <errno.h>

#include <cassert>
#include <cstdint>
#include <memory>

#if VTE_DEBUG
#define IFDEF_DEBUG(str) str
#else
#define IFDEF_DEBUG(str)
#endif

namespace vte {

namespace grid {

        typedef long row_t;
        typedef long column_t;
        typedef int half_t;

        struct coords : public std::pair<row_t, column_t> {
        public:
                using base_type = std::pair<row_t, column_t>;

                coords() = default;
                coords(row_t r, column_t c) : base_type{r, c} { }

                inline void set_row(row_t r)       { first = r;  }
                inline void set_column(column_t c) { second = c; }

                inline row_t row()       const { return first;  }
                inline column_t column() const { return second; }

                IFDEF_DEBUG(char const* to_string() const);
        };

        struct halfcolumn_t : public std::pair<column_t, half_t> {
        public:
                using base_type = std::pair<column_t, half_t>;

                halfcolumn_t() = default;
                halfcolumn_t(column_t c, half_t h) : base_type{c, h} { }

                inline void set_column(column_t c) { first = c;  }
                inline void set_half(half_t h)     { second = h; }

                inline column_t column() const { return first;  }
                inline half_t half()     const { return second; }
        };

        struct halfcoords : public std::pair<row_t, halfcolumn_t> {
        public:
                using base_type = std::pair<row_t, halfcolumn_t>;

                halfcoords() = default;
                halfcoords(row_t r, halfcolumn_t hc) : base_type{r, hc} { }
                halfcoords(row_t r, column_t c, half_t h) : base_type{r, halfcolumn_t(c, h)} { }

                inline void set_row(row_t r)                { first = r;   }
                inline void set_halfcolumn(halfcolumn_t hc) { second = hc; }

                inline row_t row()               const { return first;  }
                inline halfcolumn_t halfcolumn() const { return second; }

                IFDEF_DEBUG(char const* to_string() const);
        };

        /* end is exclusive (or: start and end point to boundaries between cells) */
        struct span {
        public:
                span() = default;
                span(coords const& s, coords const& e) : m_start(s), m_end(e) { }
                span(row_t sr, column_t sc, row_t er, column_t ec) : m_start(sr, sc), m_end(er, ec) { }

                inline void set(coords const&s, coords const& e) { m_start = s; m_end = e; }
                inline void set_start(coords const& s) { m_start = s; }
                inline void set_end(coords const& e) { m_end = e; }

                inline bool operator == (span const& rhs) const { return m_start == rhs.m_start && m_end == rhs.m_end; }
                inline bool operator != (span const& rhs) const { return m_start != rhs.m_start || m_end != rhs.m_end; }

                inline coords const& start() const { return m_start; }
                inline coords const& end()   const { return m_end; }
                inline row_t start_row()       const { return m_start.row(); }
                inline row_t end_row()         const { return m_end.row(); }
                /* Get the last row that actually contains characters belonging to this span. */
                inline row_t last_row()        const { return m_end.column() > 0 ? m_end.row() : m_end.row() - 1; }
                inline column_t start_column() const { return m_start.column(); }
                inline column_t end_column()   const { return m_end.column(); }

                inline void clear() { m_start = coords(-1, -1); m_end = coords(-1, -1); }
                inline bool empty() const { return m_start >= m_end; }
                inline explicit operator bool() const { return !empty(); }

                inline bool contains(coords const& p) const { return m_start <= p && p < m_end; }
                // FIXME make "block" a member of the span? Or subclasses for regular and block spans?
                inline bool box_contains(coords const& p) const { return m_start.row() <= p.row() && p.row() <= m_end.row() &&
                                                                         m_start.column() <= p.column() && p.column() < m_end.column(); }

                inline bool contains(row_t row, column_t column) { return contains(coords(row, column)); }

                IFDEF_DEBUG(char const* to_string() const);

        private:
                coords m_start;
                coords m_end;
        };

} /* namespace grid */

namespace view {

        /* FIXMEchpe: actually 32-bit int would be sufficient here */
        typedef long coord_t;

        class coords {
        public:
                coords() = default;
                coords(coord_t x_, coord_t y_) : x(x_), y(y_) { }

                inline bool operator == (coords const& rhs) const { return x == rhs.x && y == rhs.y; }
                inline bool operator != (coords const& rhs) const { return x != rhs.x || y != rhs.y; }

                void swap(coords &rhs) { coords tmp = rhs; rhs = *this; *this = tmp; }

                IFDEF_DEBUG(char const* to_string() const);

        public:
                coord_t x;
                coord_t y;
        };

        class extents {
        public:
                extents() = default;
                extents(coord_t w, coord_t h) : m_width(w), m_height(h) { }

                inline coord_t width() const { return m_width; }
                inline coord_t height() const { return m_height; }

                inline bool operator == (extents const& rhs) const { return m_width == rhs.m_width && m_height == rhs.m_height; }
                inline bool operator != (extents const& rhs) const { return m_width != rhs.m_width || m_height != rhs.m_height; }

                IFDEF_DEBUG(char const* to_string() const);

        private:
                coord_t m_width;
                coord_t m_height;
        };

} /* namespace view */

namespace color {

        /* 24-bit (8 bit per channel) packed colour */
        /* FIXME: we could actually support 10 bit per channel */
        typedef guint32 packed;

        class rgb : public PangoColor {
        public:
                rgb() = default;
                rgb(PangoColor const& c) { *static_cast<PangoColor*>(this) = c; }
                rgb(GdkRGBA const* c);
                rgb(GdkRGBA const& c) : rgb(&c) { }
                rgb(uint16_t r, uint16_t g, uint16_t b)
                        : PangoColor{r, g, b} { }

                bool parse(char const* spec);

                void from_pango(PangoColor const& c) { *static_cast<PangoColor*>(this) = c; }

                inline bool operator == (rgb const& rhs) const {
                        return red == rhs.red && green == rhs.green && blue == rhs.blue;
                }

                inline GdkRGBA rgba(double alpha = 1.0) const {
                        return GdkRGBA{red/65535.f, green/65535.f, blue/65535.f, (float)alpha};
                }

                IFDEF_DEBUG(char const* to_string() const);
        };

} /* namespace color */

} /* namespace vte */
