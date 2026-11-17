// Copyright © 2008-2018, 2024 Christian Persch
// Copyright © Egmont Koblinger
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "parser.hh"
#include "vtedefines.hh"

namespace vte::parser {

namespace detail {

enum {
        VTE_SGR_COLOR_SPEC_RGB    = 2,
        VTE_SGR_COLOR_SPEC_LEGACY = 5
};

// collect_sgr_color:
// @seq:
// @idx: the index of the starting parameter (i.e. with value 38, 48, 58)
// @redbits:
// @greenbits:
// @bluebits:
// @color:
//
// Parse parameters of SGR 38, 48 or 58, starting at @index within @seq
// Returns %true if @seq contained colour parameters at @index, or %false
// otherwise.
// In each case, @idx is set to last consumed parameter,
// and the colour is returned in @color.
//
// The format looks like:
// - 256 color indexed palette:
//   - ^[[38:5:INDEXm
//      (de jure standard: ITU-T T.416 / ISO/IEC 8613-6; we also allow and
//      ignore further parameters)
//   - ^[[38;5;INDEXm
//      (de facto standard, understood by probably all terminal emulators
//      that support 256 colors)
// - true colors:
//   - ^[[38:2:[id]:RED:GREEN:BLUE[:...]m
//      (de jure standard: ITU-T T.416 / ISO/IEC 8613-6)
//   - ^[[38:2:RED:GREEN:BLUEm
//      (common misinterpretation of the standard,
//      FIXME: stop supporting it at some point)
//   - ^[[38;2;RED;GREEN;BLUEm
//      (de facto standard, understood by probably all terminal emulators
//      that support true colors)
//
// See https://gitlab.gnome.org/GNOME/vte/-/issues/1972
// and https://gitlab.gnome.org/GNOME/vte/-/issues/2460
// for details.
//
inline constexpr auto
collect_sgr_color(vte::parser::Sequence const& seq,
                  unsigned int &idx,
                  int redbits,
                  int greenbits,
                  int bluebits,
                  uint32_t& color) noexcept -> bool
{
        // Note that we don't have to check if the index is after the end of
        // the parameters list, since dereferencing is safe and returns -1.

        if (seq.param_nonfinal(idx)) {
                // Colon version
                switch (seq.param(++idx)) {
                case VTE_SGR_COLOR_SPEC_RGB: {
                        auto const n = seq.next(idx) - idx;
                        if (n < 4)
                                return false;
                        if (n > 4) {
                                // Consume a colourspace parameter; it must be default
                                if (!seq.param_default(++idx))
                                        return false;
                        }

                        auto const red = seq.param(++idx);
                        auto const green = seq.param(++idx);
                        auto const blue = seq.param(++idx);

                        // Check value limits 0...255
                        if ((red & 0xff) != red ||
                            (green & 0xff) != green ||
                            (blue & 0xff) != blue)
                                return false;

                        color = VTE_RGB_COLOR(redbits, greenbits, bluebits, red, green, blue);
                        return true;
                }
                case VTE_SGR_COLOR_SPEC_LEGACY: {
                        auto const n = seq.next(idx) - idx;
                        if (n < 2)
                                return false;

                        auto const v = seq.param(++idx);
                        if ((v & 0xff) != v)
                                return false;

                        color = (uint32_t)v;
                        return true;
                }
                }
        } else {
                // Semicolon version

                idx = seq.next(idx);
                switch (seq.param(idx)) {
                case VTE_SGR_COLOR_SPEC_RGB: {
                        // Consume 3 more parameters
                        idx = seq.next(idx);
                        auto const red = seq.param(idx);
                        idx = seq.next(idx);
                        auto const green = seq.param(idx);
                        idx = seq.next(idx);
                        auto const blue = seq.param(idx);

                        // Check value limits 0...255
                        if ((red & 0xff) != red ||
                            (green & 0xff) != green ||
                            (blue & 0xff) != blue)
                                return false;

                        color = VTE_RGB_COLOR(redbits, greenbits, bluebits, red, green, blue);
                        return true;
                }
                case VTE_SGR_COLOR_SPEC_LEGACY: {
                        // Consume 1 more parameter
                        idx = seq.next(idx);

                        auto const v = seq.param(idx);
                        if ((v & 0xff) != v)
                                return false;

                        color = (uint32_t)v;
                        return true;
                }
                }
        }

        return false;
}

} // namespace detail

template<class P>
inline constexpr auto
collect_sgr(Sequence const& seq,
            unsigned idx,
            P&& pen) noexcept -> void
{
        auto const n_params = seq.size();

        // If we had no SGR parameters, default to the defaults
        if (idx >= n_params) {
                pen.reset_sgr_attributes();
                return;
        }

        for (auto i = idx;
             i < n_params;
             i = seq.next(i)) {
                auto const param = seq.param(i);
                switch (param) {
                case -1:
                case VTE_SGR_RESET_ALL:
                        pen.reset_sgr_attributes();
                        break;
                case VTE_SGR_SET_BOLD:
                        pen.set_bold(true);
                        break;
                case VTE_SGR_SET_DIM:
                        pen.set_dim(true);
                        break;
                case VTE_SGR_SET_ITALIC:
                        pen.set_italic(true);
                        break;
                case VTE_SGR_SET_UNDERLINE: {
                        auto v = 1;
                        // If we have a subparameter, get it
                        if (seq.param_nonfinal(i)) {
                                v = seq.param_range(i + 1, 1, 0, 5, -2);
                                // Skip the subparam sequence if the subparam
                                // is outside the supported range. See issue
                                // https://gitlab.gnome.org/GNOME/vte/-/issues/2640
                                if (v == -2)
                                        break;
                        }
                        pen.set_underline(v);
                        break;
                }
                case VTE_SGR_SET_BLINK:
                case VTE_SGR_SET_BLINK_RAPID:
                        pen.set_blink(true);
                        break;
                case VTE_SGR_SET_REVERSE:
                        pen.set_reverse(true);
                        break;
                case VTE_SGR_SET_INVISIBLE:
                        pen.set_invisible(true);
                        break;
                case VTE_SGR_SET_STRIKETHROUGH:
                        pen.set_strikethrough(true);
                        break;
                case VTE_SGR_SET_UNDERLINE_DOUBLE:
                        pen.set_underline(2);
                        break;
                case VTE_SGR_RESET_BOLD_AND_DIM:
                        pen.unset(VTE_ATTR_BOLD_MASK | VTE_ATTR_DIM_MASK);
                        break;
                case VTE_SGR_RESET_ITALIC:
                        pen.set_italic(false);
                        break;
                case VTE_SGR_RESET_UNDERLINE:
                        pen.set_underline(0);
                        break;
                case VTE_SGR_RESET_BLINK:
                        pen.set_blink(false);
                        break;
                case VTE_SGR_RESET_REVERSE:
                        pen.set_reverse(false);
                        break;
                case VTE_SGR_RESET_INVISIBLE:
                        pen.set_invisible(false);
                        break;
                case VTE_SGR_RESET_STRIKETHROUGH:
                        pen.set_strikethrough(false);
                        break;
                case VTE_SGR_SET_FORE_LEGACY_START ... VTE_SGR_SET_FORE_LEGACY_END:
                        pen.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 30));
                        break;
                case VTE_SGR_SET_FORE_SPEC: {
                        auto fore = uint32_t{};
                        if (detail::collect_sgr_color(seq, i, 8, 8, 8, fore)) [[likely]]
                                pen.set_fore(fore);
                        break;
                }
                case VTE_SGR_RESET_FORE:
                        // Default foreground
                        pen.set_fore(VTE_DEFAULT_FG);
                        break;
                case VTE_SGR_SET_BACK_LEGACY_START ... VTE_SGR_SET_BACK_LEGACY_END:
                        pen.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 40));
                        break;
                case VTE_SGR_SET_BACK_SPEC: {
                        auto back = uint32_t{};
                        if (detail::collect_sgr_color(seq, i, 8, 8, 8, back)) [[likely]]
                                pen.set_back(back);
                        break;
                }
                case VTE_SGR_RESET_BACK:
                        // Default background
                        pen.set_back(VTE_DEFAULT_BG);
                        break;
                case VTE_SGR_SET_OVERLINE:
                        pen.set_overline(true);
                        break;
                case VTE_SGR_RESET_OVERLINE:
                        pen.set_overline(false);
                        break;
                case VTE_SGR_SET_DECO_SPEC: {
                        auto deco = uint32_t{};
                        if (detail::collect_sgr_color(seq, i, 4, 5, 4, deco)) [[likely]]
                                pen.set_deco(deco);
                        break;
                }
                case VTE_SGR_RESET_DECO:
                        // Default decoration color which is
                        // the same as the cell's foreground
                        pen.set_deco(VTE_DEFAULT_FG);
                        break;
                case VTE_SGR_SET_FORE_LEGACY_BRIGHT_START ... VTE_SGR_SET_FORE_LEGACY_BRIGHT_END:
                        pen.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 90) +
                                     VTE_COLOR_BRIGHT_OFFSET);
                        break;
                case VTE_SGR_SET_BACK_LEGACY_BRIGHT_START ... VTE_SGR_SET_BACK_LEGACY_BRIGHT_END:
                        pen.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 100) +
                                     VTE_COLOR_BRIGHT_OFFSET);
                        break;
                }
        }
}

template<class P>
inline constexpr auto
collect_decsgr(Sequence const& seq,
               unsigned idx,
               P&& pen) noexcept -> void
{
        auto const n_params = seq.size();

        // If we had no SGR parameters, default to the defaults
        if (idx >= n_params) {
                pen.reset_sgr_attributes();
                return;
        }

        for (auto i = idx;
             i < n_params;
             i = seq.next(i)) {
                auto const param = seq.param(i);
                switch (param) {
                case -1:
                case VTE_DECSGR_RESET_ALL:
                        pen.reset_sgr_attributes();
                        break;
                case VTE_DECSGR_SET_OVERLINE:
                        pen.set_overline(true);
                        break;
                case VTE_DECSGR_RESET_OVERLINE:
                        pen.set_overline(false);
                        break;
                default: // not supported
                        break;
                }
        }
}

} // namespace vte::parser
