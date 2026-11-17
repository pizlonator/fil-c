/*
 * Copyright © 2018 Christian Persch
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

typedef uint64_t vte_color_triple_t;

#define VTE_COLOR_TRIPLE_FORE_SHIFT     (0)
#define VTE_COLOR_TRIPLE_BACK_SHIFT     (25)
#define VTE_COLOR_TRIPLE_DECO_SHIFT     (50)

#define VTE_COLOR_TRIPLE_RGB_MASK(rb,gb,bb) ((1ULL << ((rb) + (gb) + (bb) + 1)) - 1)
#define VTE_COLOR_TRIPLE_FORE_MASK      (VTE_COLOR_TRIPLE_RGB_MASK(8, 8, 8) << VTE_COLOR_TRIPLE_FORE_SHIFT)
#define VTE_COLOR_TRIPLE_BACK_MASK      (VTE_COLOR_TRIPLE_RGB_MASK(8, 8, 8) << VTE_COLOR_TRIPLE_BACK_SHIFT)
#define VTE_COLOR_TRIPLE_DECO_MASK      (VTE_COLOR_TRIPLE_RGB_MASK(4, 5, 4) << VTE_COLOR_TRIPLE_DECO_SHIFT)

#define VTE_COLOR_TRIPLE_INIT(fg,bg,dc) (uint64_t(fg) << VTE_COLOR_TRIPLE_FORE_SHIFT | \
                                         uint64_t(bg) << VTE_COLOR_TRIPLE_BACK_SHIFT | \
                                         uint64_t(dc) << VTE_COLOR_TRIPLE_DECO_SHIFT)

#define VTE_COLOR_TRIPLE_INIT_DEFAULT   (VTE_COLOR_TRIPLE_INIT(VTE_DEFAULT_FG, \
                                                               VTE_DEFAULT_BG, \
                                                               VTE_DEFAULT_FG))

static constexpr inline vte_color_triple_t vte_color_triple_init(void)
{
        return VTE_COLOR_TRIPLE_INIT_DEFAULT;
}

static constexpr inline vte_color_triple_t vte_color_triple_copy(vte_color_triple_t ct)
{
        return ct;
}

static inline void vte_color_triple_set(vte_color_triple_t* ct,
                                        uint64_t mask,
                                        unsigned value_shift,
                                        uint64_t value)
{
        *ct = (*ct & ~mask) | (value << value_shift);
}

static inline void vte_color_triple_unset(vte_color_triple_t* ct,
                                          uint64_t mask)
{
        *ct = (*ct & ~mask);
}

static inline void vte_color_triple_set_fore(vte_color_triple_t* ct,
                                             uint32_t fore)
{
        *ct = (*ct & ~VTE_COLOR_TRIPLE_FORE_MASK) | (uint64_t(fore)) << VTE_COLOR_TRIPLE_FORE_SHIFT;
}

static inline void vte_color_triple_set_back(vte_color_triple_t* ct,
                                             uint32_t back)
{
        *ct = (*ct & ~VTE_COLOR_TRIPLE_BACK_MASK) | (uint64_t(back)) << VTE_COLOR_TRIPLE_BACK_SHIFT;
}

static inline void vte_color_triple_set_deco(vte_color_triple_t* ct,
                                             uint32_t deco)
{
        *ct = (*ct & ~VTE_COLOR_TRIPLE_DECO_MASK) | (uint64_t(deco)) << VTE_COLOR_TRIPLE_DECO_SHIFT;
}

static inline constexpr uint32_t vte_color_triple_get_fore(vte_color_triple_t ct)
{
        return uint32_t((ct >> VTE_COLOR_TRIPLE_FORE_SHIFT) & VTE_COLOR_TRIPLE_RGB_MASK(8, 8, 8));
}

static inline constexpr uint32_t vte_color_triple_get_back(vte_color_triple_t ct)
{
        return uint32_t((ct >> VTE_COLOR_TRIPLE_BACK_SHIFT) & VTE_COLOR_TRIPLE_RGB_MASK(8, 8, 8));
}

static inline constexpr uint32_t vte_color_triple_get_deco(vte_color_triple_t ct)
{
        return uint32_t((ct >> VTE_COLOR_TRIPLE_DECO_SHIFT) & VTE_COLOR_TRIPLE_RGB_MASK(4, 5, 4));
}

static inline void vte_color_triple_get(vte_color_triple_t ct,
                                        uint32_t* fore,
                                        uint32_t* back,
                                        uint32_t* deco)
{
        *fore = vte_color_triple_get_fore(ct);
        *back = vte_color_triple_get_back(ct);
        *deco = vte_color_triple_get_deco(ct);
}
