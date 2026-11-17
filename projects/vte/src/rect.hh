// Copyright © 2024 Christian Persch
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

namespace vte {

template<typename T>
class point {
public:
        using coord_type = T;

        constexpr point() noexcept = default;

        constexpr point(coord_type x,
                        coord_type y) noexcept
                : m_x{x},
                  m_y{y}
        {
        }

        constexpr auto x() const noexcept { return m_x; }
        constexpr auto y() const noexcept { return m_y; }

        // translation
        friend constexpr auto operator+(point const& p1,
                                        point const& p2) noexcept -> point
        {
                return {p1.x() + p2.x(),
                        p1.y() + p2.y()};
        }

        constexpr auto operator+=(point const& p) noexcept -> point&
        {
                *this = *this + p;
                return *this;
        }

        friend constexpr auto operator-(point const& p1,
                                        point const& p2) noexcept -> point
        {
                return {p1.x() - p2.x(),
                        p1.y() - p2.y()};
        }

        constexpr auto operator-=(point const& p) noexcept -> point&
        {
                *this = *this - p;
                return *this;
        }

        // comparision
        friend constexpr auto operator==(point const& p1,
                                         point const& p2) noexcept -> bool = default;

private:
        coord_type m_x{};
        coord_type m_y{};
}; // class point

template<typename T, class Traits>
class rect {
public:
        using coord_type = T;
        using point_type = point<T>;
        using traits_type = Traits;

        constexpr rect() noexcept = default;

        constexpr rect(coord_type left,
                       coord_type top,
                       coord_type right,
                       coord_type bottom) noexcept
                : m_left{left},
                  m_top{top},
                  m_right{right},
                  m_bottom{bottom}
        {
        }

        constexpr rect(point_type p1,
                       point_type p2) noexcept
                : m_left{p1.x()},
                  m_top{p1.y()},
                  m_right{p2.x()},
                  m_bottom{p2.y()}
        {
        }

        constexpr auto left()   const noexcept { return m_left;   }
        constexpr auto top()    const noexcept { return m_top;    }
        constexpr auto right()  const noexcept { return m_right;  }
        constexpr auto bottom() const noexcept { return m_bottom; }

        constexpr auto topleft() const noexcept -> point_type { return {left(), top()}; }
        constexpr auto bottomright() const noexcept -> point_type { return {right(), bottom()}; }

        constexpr auto width() const noexcept { return traits_type::extent(left(), right()); }
        constexpr auto height() const noexcept { return traits_type::extent(top(), bottom()); }

        constexpr explicit operator bool() const noexcept { return !empty(); }

        constexpr auto empty() const noexcept -> bool
        {
                return !traits_type::cmp(left(), right()) ||
                        !traits_type::cmp(top(), bottom());
        }

        // union
        friend constexpr auto operator|(rect const& r1,
                                        rect const& r2) noexcept -> rect
        {
                // Union with empty rect returns the other rect.
                if (r1.empty())
                        return r2; // possibly also empty
                if (r2.empty())
                        return r1;

                return {std::min(r1.left(), r2.left()),
                        std::min(r1.top(), r2.top()),
                        std::min(r1.right(), r2.right()),
                        std::min(r1.bottom(), r2.bottom())};
        }

        constexpr auto operator|=(rect const& other) noexcept -> rect&
        {
                *this = *this | other;
                return *this;
        }

        // intersection
        friend constexpr auto operator&(rect const& r1,
                                        rect const& r2) noexcept -> rect
        {
                auto r = rect{std::max(r1.left(), r2.left()),
                              std::max(r1.top(), r2.top()),
                              std::min(r1.right(), r2.right()),
                              std::min(r1.bottom(), r2.bottom())};
                return !r.empty() ? r : rect{}; // if empty, return standard empty rect
        }

        constexpr auto operator&=(rect const& other) noexcept -> rect&
        {
                *this = *this & other;
                return *this;
        }

        // comparision
        friend constexpr auto operator==(rect const& r1,
                                         rect const& r2) noexcept -> bool = default;

        // translation
        friend constexpr auto operator+(rect const& r,
                                        point_type const& p) noexcept -> rect
        {
                return {r.left() + p.x(),
                        r.top() + p.y(),
                        r.right() + p.x(),
                        r.bottom() + p.y()};
        }

        constexpr auto operator+=(point_type const& p) noexcept -> rect&
        {
                *this = *this + p;
                return *this;
        }

        friend constexpr auto operator-(rect const& r,
                                        point_type const& p) noexcept -> rect
        {
                return {r.left() - p.x(),
                        r.top() - p.y(),
                        r.right() - p.x(),
                        r.bottom() - p.y()};
        }

        constexpr auto operator-=(point_type const& p) noexcept -> rect&
        {
                *this = *this - p;
                return *this;
        }

        // move to point
        constexpr auto move_to(point_type const& p) noexcept -> rect&
        {
                *this += p - topleft();
                return *this;
        }

        // contains
        constexpr auto contains(rect const& r) const noexcept -> bool
        {
                return r.left() >= left() &&
                        r.top() >= top() &&
                        r.right() <= right() &&
                        r.bottom() <= bottom();
        }

        constexpr auto contains(point_type const& p) const noexcept -> bool
        {
                return p.x() >= left() &&
                        p.y() >= top() &&
                        traits_type::cmp(p.x(), right()) &&
                        traits_type::cmp(p.y(), bottom());
        }

        // extends @this to the borders of @other if it had empty intersection
        // (note that this requires an inclusive rect to work).
        constexpr auto intersect_or_extend(rect const& other) noexcept -> rect&
        {
                if (auto const intersection = *this & other;
                    !intersection.empty()) {
                        *this = intersection;
                } else {
                        m_left = std::min(left(), other.right());
                        m_top = std::min(top(), other.bottom());
                        m_right = std::max(right(), other.left());
                        m_bottom = std::max(bottom(), other.top());
                        *this &= other;
                }

                return *this;
        }

        // makes @this the same dimensions as @other
        constexpr auto size_to(rect const& other) noexcept -> rect&
        {
                if (other) {
                        auto const p = topleft() + (other.bottomright() - other.topleft());
                        m_right = p.x();
                        m_bottom = p.y();
                } else {
                        *this = {};
                }
                return *this;
        }

        // clone, returns a copy of @this
        constexpr auto clone() const noexcept -> rect
        {
                return *this;
        }

private:

        // rect is empty by default
        static_assert(std::less<coord_type>{}(traits_type::less_than_zero,
                                              traits_type::zero), "Invalid");

        coord_type m_left{traits_type::zero};
        coord_type m_top{traits_type::zero};
        coord_type m_right{traits_type::less_than_zero};
        coord_type m_bottom{traits_type::less_than_zero};
}; // class rect

namespace detail {

template<typename T>
class less_or_equal {
public:
        using type = T;

        static constexpr auto cmp(type a,
                                  type b) noexcept -> bool
        {
                return a <= b;
        }

        static constexpr type zero = type(0);
        static constexpr type less_than_zero = type(-1);

        static constexpr auto extent(type a,
                                     type b) noexcept -> type
        {
                return a <= b ? b - a + 1 : 0;
        }

}; // class less_or_equal

} // namespace detail

template<typename T>
using rect_inclusive = rect<T, detail::less_or_equal<T>>;

// sanity check
static_assert(rect_inclusive<int>{}.empty(), "not empty by default");

} // namespace vte
