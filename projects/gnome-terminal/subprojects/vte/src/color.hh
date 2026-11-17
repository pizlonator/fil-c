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

#include <charconv>
#include <concepts>
#include <optional>
#include <tuple>

#include <cstdint>

#include "fwd.hh"

namespace vte::color {

template<std::floating_point C>
class rgb_base;

template<std::floating_point C>
class rgba_base;

template<std::floating_point C>
class rgb_base {
public:
        using component_type = C;

        constexpr rgb_base() noexcept = default;

        template<std::floating_point F>
        constexpr rgb_base(F r,
                           F g,
                           F b) noexcept :
                m_red(r),
                m_green(g),
                m_blue(b)
        {
        }

        constexpr auto red()   const noexcept { return m_red;   }
        constexpr auto green() const noexcept { return m_green; }
        constexpr auto blue()  const noexcept { return m_blue;  }

        friend bool operator==(rgb_base const& lhs,
                               rgb_base const& rhs) = default;

private:
        component_type m_red{0};
        component_type m_green{0};
        component_type m_blue{0};

}; // class rgb_base

template<std::floating_point C>
class rgba_base : public rgb_base<C> {
public:
        using base_type = rgb_base<C>;
        using component_type = typename base_type::component_type;

        constexpr rgba_base() noexcept = default;

        template<std::floating_point F>
        constexpr rgba_base(F r,
                            F g,
                            F b,
                            F a = 1) noexcept :
                base_type(r, g, b),
                m_alpha(a)
        {
        }

        constexpr auto alpha() const noexcept { return m_alpha; }

        friend bool operator==(rgba_base const& lhs,
                               rgba_base const& rhs) = default;

private:
        component_type m_alpha{1};

}; // class rgba_base

template<typename Color>
class traits;

template<typename Color>
inline constexpr auto has_alpha_component_v = traits<Color>::has_alpha;

template<typename Color>
using component_type_t = typename traits<Color>::component_type;

template<std::floating_point C>
class traits<rgb_base<C>> final {
public:
        static inline constexpr bool has_alpha = false;
        using component_type = C;
};

template<std::floating_point C>
class traits<rgba_base<C>> final {
public:
        static inline constexpr bool has_alpha = true;
        using component_type = C;
};

namespace impl {

using color_tuple = std::tuple<float, float, float, float>;

template<class Color>
inline constexpr Color
from_tuple(color_tuple const& v) noexcept
{
        auto [r, g, b, a] = v;

        if constexpr (has_alpha_component_v<Color>)
                return Color(r, g, b, a);
        else
                return Color(r, g, b);
}

// Color from RGB(A) packed (BE)
inline constexpr color_tuple
from_bits(uint64_t value,
          int bits,
          bool alpha)
{
#if __has_cpp_attribute(assume)
        [[assume((bits <= 16) && (bits % 4) == 0)]];
#endif

        auto const mask = uint64_t{(1u << bits) - 1u};
        auto a = ((alpha ? value : mask) & mask) << (16 - bits);
        if (alpha)
                value >>= bits;
        auto b = (value & mask) << (16 - bits);
        value >>= bits;
        auto g = (value & mask) << (16 - bits);
        value >>= bits;
        auto r = (value & mask) << (16 - bits);
        while (bits < 16) {
                r |= (r >> bits);
                g |= (g >> bits);
                b |= (b >> bits);
                a |= (a >> bits);
                bits <<= 1;
        }

        auto const cmax = 65535.0f;
        return std::make_tuple(r / cmax, g / cmax, b / cmax, a / cmax);
}

} // namespace impl

// Color from RGB(A) packed (BE)
template<class Color>
inline constexpr Color
from_bits(uint64_t value,
          int bits,
          bool alpha)
{
        return impl::from_tuple<Color>(impl::from_bits(value, bits, alpha));
}

} // namespace vte::color
