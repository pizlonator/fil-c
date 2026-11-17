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

#include <memory>
#include <type_traits>

namespace vte {

template<typename T>
class FreeableDeleter;

template<typename T>
using Freeable = std::unique_ptr<T, FreeableDeleter<T>>;

template<typename T>
inline auto take_freeable(T* t) { return Freeable<T>{t}; }

#define VTE_DECLARE_FREEABLE(T, func) \
template<> \
class FreeableDeleter<T> { \
public: inline void operator()(T* t) { func(t); } \
}

template<typename S, typename V, V default_v = 0>
class ValueGetter {
public:
        explicit ValueGetter(S& storage,
                             V default_vv = default_v) noexcept
                : m_storage{storage},
                  m_value{default_vv}
        {
        }

        ~ValueGetter()
        {
                if constexpr (std::is_nothrow_assignable_v<S&, V>) {
                        m_storage = m_value;
                } else {
                        m_storage.reset(m_value);
                }
        }

        ValueGetter(ValueGetter const&) = delete;
        ValueGetter(ValueGetter&&) = delete;

        ValueGetter& operator=(ValueGetter const&) = delete;
        ValueGetter& operator=(ValueGetter&&) = delete;

        operator V*() noexcept { return &m_value; }
        V* operator&() noexcept { return &m_value; }

private:
        S& m_storage;
        V m_value;
};

template<typename S, typename V, V default_v = 0>
auto get_value(S& s) { return vte::ValueGetter<S, V, default_v>{s}; }

template<typename T>
using FreeableGetter = ValueGetter<Freeable<T>, T*, nullptr>;

template<typename T>
auto get_freeable(vte::Freeable<T>& freeable) { return vte::FreeableGetter<T>{freeable}; }

} // namespace vte
