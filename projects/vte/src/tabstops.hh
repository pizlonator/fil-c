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

#include <cassert>
#include <cstdint>
#include <cstring>

#include "debug.h"

#include "vtedefines.hh"

namespace vte {

namespace terminal {

class Tabstops {
public:
        using position_t = unsigned int;
        using signed_position_t = int;

        static inline constexpr position_t const npos = -1;

private:
        typedef unsigned long storage_t;

        /* Number of bits used in storage */
        position_t m_size{0};
        /* Number of blocks in m_storage */
        position_t m_capacity{0};
        /* Bit storage */
        storage_t* m_storage{nullptr};

        inline position_t bits() const noexcept
        {
                return 8 * sizeof(storage_t);
        }

        inline position_t block(position_t position) const noexcept
        {
                return position / bits();
        }

        inline position_t block_first(position_t position) const noexcept
        {
                return position & ~(bits() - 1);
        }

        /* Mask with exactly the position's bit set */
        inline storage_t mask(position_t position) const noexcept
        {
                return storage_t(1) << (position & (bits() - 1));
        }

        /* Mask with all bits set from position (excl.) up to the MSB */
        inline storage_t mask_lower(position_t position) const noexcept
        {
                return ~(mask(position) | (mask(position) - 1));
        }

        /* Mask with all bits set from 0 to position (excl.) */
        inline storage_t mask_upper(position_t position) const noexcept
        {
                return mask(position) - 1;
        }

        inline position_t next_position(position_t position) const noexcept
        {
                auto b = block(position);
                auto v = m_storage[b] & mask_lower(position);
                if (v != 0)
                        return b * bits() + __builtin_ctzl(v);

                while (++b < m_capacity) {
                        v = m_storage[b];
                        if (v != 0)
                                return b * bits() + __builtin_ctzl(v);
                }

                return npos;
        }

        inline position_t previous_position(position_t position) const noexcept
        {
                auto b = block(position);
                auto v = m_storage[b] & mask_upper(position);
                if (v != 0)
                        return (b + 1) * bits() - __builtin_clzl(v) - 1;

                while (b > 0) {
                        v = m_storage[--b];
                        if (v != 0)
                                return (b + 1) * bits() - __builtin_clzl(v) - 1;
                }

                return npos;
        }

public:
        Tabstops(position_t size = VTE_COLUMNS,
                 bool set_default = true,
                 position_t tab_width = VTE_TAB_WIDTH) noexcept
        {
                resize(size, set_default, tab_width);
        }

        Tabstops(Tabstops const&) = delete;
        Tabstops(Tabstops&&) = delete;

        ~Tabstops()
        {
                free(m_storage);
        };

        Tabstops& operator=(Tabstops const&) = delete;
        Tabstops& operator=(Tabstops&&) = delete;

        inline position_t size() const noexcept
        {
                return m_size;
        }

        void resize(position_t size,
                    bool set_default = true,
                    position_t tab_width = VTE_TAB_WIDTH) noexcept
        {
                /* We want an even number of blocks */
                auto const new_capacity = ((size + 8 * sizeof(storage_t) - 1) / (8 * sizeof(storage_t)) + 1) & ~1;
                vte_assert_cmpuint(new_capacity % 2, ==, 0);
                vte_assert_cmpuint(new_capacity * 8 * sizeof(storage_t), >=, size);

                if (new_capacity > m_capacity) {
                        auto const new_capacity_bytes = new_capacity * sizeof(storage_t);
                        m_storage = reinterpret_cast<storage_t*>(realloc(m_storage, new_capacity_bytes));
                }

                if (size > m_size) {
                        /* Clear storage */
                        auto b = block(m_size);
                        m_storage[b] &= mask_upper(m_size);
                        while (++b < new_capacity)
                                m_storage[b] = 0;
                }

                auto const old_size = m_size;
                m_size = size;
                m_capacity = new_capacity;

                if (set_default) {
                        auto const r = old_size % tab_width;
                        position_t start =  r ? old_size + tab_width - r : old_size;
                        for (position_t i = start; i < m_size; i += tab_width)
                                set(i);
                }
        }

        inline void clear() noexcept
        {
                memset(m_storage, 0, m_capacity * sizeof(m_storage[0]));
        }

        inline void reset(position_t tab_width = VTE_TAB_WIDTH) noexcept
        {
                clear();
                for (position_t i = 0; i < m_size; i += tab_width)
                        set(i);
        }

        inline void set(position_t position) noexcept
        {
                assert(position < m_size);
                m_storage[block(position)] |= mask(position);
        }

        inline void unset(position_t position) noexcept
        {
                assert(position < m_size);
                m_storage[block(position)] &= ~mask(position);
        }

        inline bool get(position_t position) const noexcept
        {
                return (m_storage[block(position)] & mask(position)) != 0;
        }

        inline position_t get_next(position_t position,
                                   int count = 1,
                                   position_t endpos = npos) const noexcept
        {
                while (count-- && position < m_size && position < endpos)
                        position = next_position(position);
                return position < endpos ? position : endpos;
        }

        inline position_t get_previous(position_t position,
                                       int count = 1,
                                       position_t endpos = npos) const noexcept
        {
                while (count-- && position != npos && (endpos == npos || position > endpos))
                        position = previous_position(position);
                return (position != npos && (endpos == npos || position > endpos)) ? position : endpos;
        }
};

} // namespace terminal

} // namespace vte
