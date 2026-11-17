/*
 * Copyright © 2020 Christian Persch
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

#include <algorithm>
#include <exception>
#include <type_traits>
#include <memory>

namespace vte {

// This is like std::clamp, except that it doesn't throw when
// max_v<min_v, but instead returns min_v in that case.
template<typename T>
constexpr inline T const&
clamp(T const& v,
      T const& min_v,
      T const& max_v)
{
        return std::max(std::min(v, max_v), min_v);
}

// Converts from E to the underlying integral type, where E is an enum
// with integral underlying type.
template<typename E>
inline constexpr auto to_integral(E e) noexcept
        -> std::enable_if_t<std::is_enum_v<E> &&
                            std::is_integral_v<std::underlying_type_t<E>>,
                            std::underlying_type_t<E>>
{
        return static_cast<std::underlying_type_t<E>>(e);
}

#if VTE_DEBUG
void log_exception(char const* func = __builtin_FUNCTION(),
                   char const* filename = __builtin_FILE(),
                   int const line = __builtin_LINE()) noexcept;
#else
void log_exception() noexcept;
#endif

template <typename T, typename D, D func>
class FreeablePtrDeleter {
public:
        void operator()(T* obj) const
        {
                if (obj)
                        func(obj);
        }
};

template <typename T, typename D, D func>
using FreeablePtr = std::unique_ptr<T, FreeablePtrDeleter<T, D, func>>;

} // namespace vte

#define VTE_CXX_DEFINE_BITMASK(Type) \
  inline constexpr Type \
  operator&(Type lhs, Type rhs) noexcept \
  { return (Type)(vte::to_integral(lhs) & vte::to_integral(rhs)); } \
  \
  inline constexpr Type \
  operator~(Type v) noexcept \
  { return (Type)~vte::to_integral(v); }        \
  \
  inline constexpr Type \
  operator|(Type lhs, Type rhs) noexcept \
  { return (Type)(vte::to_integral(lhs) | vte::to_integral(rhs)); } \
  \
  inline constexpr Type \
  operator^(Type lhs, Type rhs) noexcept \
  { return (Type)(vte::to_integral(lhs) ^ vte::to_integral(rhs)); } \
  \
  inline constexpr Type& \
  operator&=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs & rhs; } \
  \
  inline constexpr Type& \
  operator|=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs | rhs; } \
  \
  inline constexpr Type& \
  operator^=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs ^ rhs; }
