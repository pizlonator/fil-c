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

namespace vte {
namespace terminal {
namespace modes {

#define VTE_MODES_MASK(shift) (1U << (shift))

#define MODE_ACCESSOR(name) \
        inline void set_##name(bool value) noexcept \
        { \
                set(e##name, value); \
        } \
        \
        inline constexpr bool name() const noexcept \
        { \
                return get(e##name); \
        }

#define MODE_FIXED_ACCESSOR(name,value) \
        inline constexpr bool name() const noexcept \
        { \
                return (value); \
        }

template <typename T>
static inline void vte_modes_set_bool(T* modes,
                                      unsigned int shift,
                                      bool value)
{
        if (value)
                *modes |= T(1U) << shift;
        else
                *modes &= ~(T(1U) << shift);
}

template <typename T>
static constexpr inline bool vte_modes_get_bool(T modes,
                                                unsigned int shift)
{
        return (modes >> shift) & 1U;
}

template <typename T>
static constexpr inline bool vte_modes_unset_bool(T* modes,
                                                  unsigned int shift)
{
        bool set = vte_modes_get_bool<T>(*modes, shift);
        vte_modes_set_bool<T>(modes, shift, false);
        return set;
}

template <typename T>
class Base
{
public:
        using Self = Base<T>;
        using Storage = T;

        constexpr Base(std::initializer_list<int> modes)
        {
                for (auto i : modes)
                        m_default_modes |= VTE_MODES_MASK(i);
                m_modes = m_default_modes;
        }

        ~Base() = default;
        Base(Self const&) = default;
        Base(Self&&) = default;
        Self& operator= (Self const&) = default;
        Self& operator= (Self&&) = default;

        inline void set(int bit,
                        bool value) noexcept
        {
                vte_modes_set_bool<Storage>(&m_modes, bit, value);
        }

        constexpr inline bool get(int bit) const noexcept
        {
                return vte_modes_get_bool<Storage>(m_modes, bit);
        }

        inline void set_modes(Storage value) noexcept
        {
                m_modes = value;
        }

        constexpr inline Storage get_modes() const noexcept
        {
                return m_modes;
        }

        void reset() noexcept
        {
                set_modes(m_default_modes);
        }

private:
        T m_modes{0};
        T m_default_modes{0};
};

class ECMA : public Base<uint8_t>
{
public:
        enum Modes {
                eUNKNOWN      = -3,
                eALWAYS_SET   = -2,
                eALWAYS_RESET = -1,

#define MODE(name,param) e##name,
#define MODE_FIXED(name,param,value)
#include "modes-ecma.hh"
#undef MODE
#undef MODE_FIXED
#define MODE(name,param)
#define MODE_FIXED(name,param,value) e##name,
#include "modes-ecma.hh"
#undef MODE
#undef MODE_FIXED
        };

        int mode_from_param(int param) const noexcept
        {
                switch (param) {
#define MODE(name,param) case param: return e##name;
#define MODE_FIXED(name,param,value) case param: return e##value;
#include "modes-ecma.hh"
#undef MODE
#undef MODE_FIXED
                default:
                        return eUNKNOWN;
                }
        }

        char const* mode_to_cstring(int param) const noexcept
        {
                switch (param) {
                case eUNKNOWN: return "UNKNOWN";
                case eALWAYS_SET: return "ALWAYS_SET";
                case eALWAYS_RESET: return "ALWAYS_RESET";
#define MODE(name,param) case e##name: return #name;
#define MODE_FIXED(name,param,value)
#include "modes-ecma.hh"
#undef MODE
#undef MODE_FIXED
                default:
                        return "INVALID";
                }
        }

#define MODE(name,param) MODE_ACCESSOR(name)
#define MODE_FIXED(name,param,value) MODE_FIXED_ACCESSOR(name, e##value == eALWAYS_SET)
#include "modes-ecma.hh"
#undef MODE
#undef MODE_FIXED

        constexpr ECMA() : Self{eBDSM} { }

}; // class ECMA

class Private : public Base<uint32_t>
{
public:
        enum Modes {
                eUNKNOWN      = -3,
                eALWAYS_SET   = -2,
                eALWAYS_RESET = -1,

#define MODE(name,param) e##name,
#define MODE_FIXED(name,param,value)
#include "modes-dec.hh"
#undef MODE
#undef MODE_FIXED
#define MODE(name,param)
#define MODE_FIXED(name,param,value) e##name,
#include "modes-dec.hh"
#undef MODE
#undef MODE_FIXED
        };

        int mode_from_param(int param) const noexcept
        {
                switch (param) {
#define MODE(name,param) case param: return e##name;
#define MODE_FIXED(name,param,value) case param: return e##value;
#include "modes-dec.hh"
#undef MODE
#undef MODE_FIXED
                default:
                        return eUNKNOWN;
                }
        }

        char const* mode_to_cstring(int param) const noexcept
        {
                switch (param) {
                case eUNKNOWN: return "UNKNOWN";
                case eALWAYS_SET: return "ALWAYS_SET";
                case eALWAYS_RESET: return "ALWAYS_RESET";
#define MODE(name,param) case e##name: return #name;
#define MODE_FIXED(name,param,value)
#include "modes-dec.hh"
#undef MODE
#undef MODE_FIXED
                default:
                        return "INVALID";
                }
        }

#define MODE(name,param) MODE_ACCESSOR(name)
#define MODE_FIXED(name,param,value) MODE_FIXED_ACCESSOR(name, e##value == eALWAYS_SET)
#include "modes-dec.hh"
#undef MODE
#undef MODE_FIXED

        constexpr Private() : Self{eDEC_AUTOWRAP,
                                   eDEC_TEXT_CURSOR,
                                   eVTE_BIDI_SWAP_ARROW_KEYS,
                                   eXTERM_ALTBUF_SCROLL,
                                   eXTERM_META_SENDS_ESCAPE,
                                   eXTERM_SIXEL_PRIVATE_COLOR_REGISTERS}
        {
        }

        inline void push_saved(int mode)
        {
                vte_modes_set_bool<Storage>(&m_saved_modes, mode, get(mode));
        }

        constexpr inline bool pop_saved(int mode)
        {
                return vte_modes_unset_bool<Storage>(&m_saved_modes, mode);
        }

        inline void clear_saved()
        {
                m_saved_modes = 0;
        }

private:
        Storage m_saved_modes{0};

}; // class Private

#undef MODE_ACCESSOR
#undef MODE_FIXED_ACCESSOR

} // namespace modes
} // namespace terminal
} // namespace vte
