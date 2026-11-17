/*
 * Copyright © 2024 Tau Gärtli
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
#include "color-palette.hh"

namespace vte::osc_colors {

enum class OSCColorIndexKind
{
        Palette,
        // An unimplemented special color (OSC 5)
        Unimplemented,
};

enum class OSCValuedColorSequenceKind
{
        // OSC 4 and 104
        XTermColor,
        // OSC 5 and 105
        XTermSpecialColor,
};

// Represents a color index that can be set or queried using OSC 4, 5, 10, .., 19.
//
// Known but unimplemented special colors are tracked for the purposes of reporting
// using the special `unimplemented` value.

// FIXME C++23: this feels like expected<ColorPaletteIndex, enum { Unimplemented }>
// reflects this conceptually better, and it's also simpler to use std stuff

class OSCColorIndex {
public:
        constexpr OSCColorIndex() = default;

        constexpr OSCColorIndex(color_palette::ColorPaletteIndex index) noexcept
                : m_kind(OSCColorIndexKind::Palette),
                  m_index(index)
        {
        }

        static constexpr auto unimplemented() noexcept -> OSCColorIndex
        {
                return OSCColorIndex(OSCColorIndexKind::Unimplemented, color_palette::ColorPaletteIndex(0));
        }

        constexpr auto kind() const noexcept -> OSCColorIndexKind { return m_kind; }
        constexpr auto palette_index() const noexcept -> color_palette::ColorPaletteIndex { return m_index; }
        auto fallback_palette_index() const noexcept -> std::optional<color_palette::ColorPaletteIndex>;

        auto static from_sequence(OSCValuedColorSequenceKind osc,
                                  int value) noexcept -> std::optional<OSCColorIndex>;

private:

        explicit constexpr OSCColorIndex(OSCColorIndexKind kind,
                                         color_palette::ColorPaletteIndex index) noexcept
                : m_kind(kind),
                  m_index(index)
        {
        }

        OSCColorIndexKind m_kind{OSCColorIndexKind::Palette};
        color_palette::ColorPaletteIndex m_index{0};
};

} // namespace vte::osc_colors
