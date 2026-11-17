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

#include <string>
#include "osc-colors.hh"

namespace vte::osc_colors {

using namespace vte::color_palette;

enum {
        OSC_5_BOLD = 0,
        OSC_5_UNDERLINE = 1,
        OSC_5_BLINK = 2,
        OSC_5_REVERSE = 3,
        OSC_5_ITALIC = 4,
};

static constexpr auto
index_from_osc_5(int value) noexcept -> std::optional<OSCColorIndex>
{
        switch (value) {
        case OSC_5_BOLD:
                return ColorPaletteIndex::bold_fg();
        case OSC_5_UNDERLINE:
        case OSC_5_BLINK:
        case OSC_5_REVERSE:
        case OSC_5_ITALIC:
                return OSCColorIndex::unimplemented();
        case -1: // default param
        default:
                return std::nullopt;
        }
}

static constexpr auto
index_from_osc_4(int value) noexcept -> std::optional<OSCColorIndex>
{
        // `OSC 4 ; 256+n` is an "alias" for `OSC 5 ; n`
        return (value >= 0 && value < 256)
                ? ColorPaletteIndex(value)
                : index_from_osc_5(value - 256);
}

auto
OSCColorIndex::from_sequence(OSCValuedColorSequenceKind osc,
                             int value) noexcept -> std::optional<OSCColorIndex>
{
        switch (osc) {
                using enum OSCValuedColorSequenceKind;

        case XTermColor:
                 return index_from_osc_4(value);
        case XTermSpecialColor:
                return index_from_osc_5(value);
        }
        return std::nullopt;
}

auto OSCColorIndex::fallback_palette_index() const noexcept -> std::optional<ColorPaletteIndex>
{
        switch (kind()) {
        case OSCColorIndexKind::Unimplemented:
                // The fallback for special colors is always the default foreground color.
                return ColorPaletteIndex::default_fg();
        case OSCColorIndexKind::Palette:
                switch (palette_index().value()) {
                case ColorPaletteIndex::bold_fg().value():
                case ColorPaletteIndex::cursor_bg().value():
                case ColorPaletteIndex::highlight_bg().value():
                        return ColorPaletteIndex::default_fg();
                case ColorPaletteIndex::highlight_fg().value():
                        return ColorPaletteIndex::default_bg();
                default:
                        return std::nullopt;
                }
        }

        return std::nullopt;
}

} // namespace vte::osc_colors
