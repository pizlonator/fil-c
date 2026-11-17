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

#include <string_view>
#include <optional>
#include "vtedefines.hh"

namespace vte::color_palette {

// Keep in decreasing order of precedence.
enum class ColorSource
{
        Escape = 0,
        API = 1,
};

constexpr auto display_color_source(ColorSource source) noexcept -> char const*
{
        switch (source) {
        case ColorSource::Escape: return "escape";
        case ColorSource::API: return "API";
        }

        return "unknown";
}

// An index into the color palette. See vtedefines.hh for details
// on the encoding of these indexes.
class ColorPaletteIndex {
        using value_type = unsigned;
public:
        explicit constexpr ColorPaletteIndex(value_type value) noexcept
                : m_value(value) { }

        static constexpr auto default_fg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_DEFAULT_FG }; }
        static constexpr auto default_bg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex {VTE_DEFAULT_BG }; }
        static constexpr auto cursor_fg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_CURSOR_FG }; }
        static constexpr auto cursor_bg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_CURSOR_BG }; }
        static constexpr auto highlight_fg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_HIGHLIGHT_FG }; }
        static constexpr auto highlight_bg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_HIGHLIGHT_BG }; }
        static constexpr auto bold_fg() noexcept -> ColorPaletteIndex { return ColorPaletteIndex { VTE_BOLD_FG }; }

        friend constexpr auto operator<=>(ColorPaletteIndex,
                                          ColorPaletteIndex) noexcept = default;

        constexpr auto is_cursor() const noexcept -> bool { return *this == cursor_fg() || *this == cursor_bg(); }

        constexpr auto value() const noexcept -> value_type { return m_value; }

        constexpr auto is_valid() const noexcept -> bool
        {
                return /* m_value >= 0 && */ m_value < VTE_PALETTE_SIZE;
        }

private:
        value_type m_value{0};
};

} // namespace vte::color_palette
