/*
 * Copyright © 2023 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <optional>
#include <string>

#include "color.hh"

namespace vte::color {


enum class color_output_format {
        HEX,
};

namespace impl {

std::optional<color_tuple> parse_csslike(std::string const& spec) noexcept;

std::optional<color_tuple> parse_x11like(std::string const& spec) noexcept;

std::string to_string(color_tuple const&,
                      bool alpha = false,
                      color_output_format fmt = color_output_format::HEX);

} // namespace impl

template<class Color>
std::optional<Color> parse_csslike(std::string const& spec) noexcept
{
        if (auto const v = impl::parse_csslike(spec)) {
                return impl::from_tuple<Color>(*v);
        }

        return std::nullopt;
}

template<class Color>
std::optional<Color> parse_x11like(std::string const& spec) noexcept
{
        if (auto const v = impl::parse_x11like(spec)) {
                return impl::from_tuple<Color>(*v);
        }

        return std::nullopt;
}

template<class Color>
std::optional<Color> parse_any(std::string const& spec) noexcept
{
        if (auto const v = impl::parse_csslike(spec)) {
                return impl::from_tuple<Color>(*v);
        }

        if (auto const v = impl::parse_x11like(spec)) {
                return impl::from_tuple<Color>(*v);
        }

        return std::nullopt;
}

template<class Color>
std::string
to_string(Color const& color,
          bool alpha = false,
          color_output_format fmt = color_output_format::HEX)
{
        return impl::to_string(std::make_tuple(color.red(), color.green(), color.blue(), color.alpha()),
                               alpha,
                               fmt);
}

} // namespace vte::color
