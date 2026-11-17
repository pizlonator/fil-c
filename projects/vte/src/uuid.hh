/*
 * Copyright © 2013-2015 Red Hat, Inc.
 * Copyright © 2022, 2023 Christian Persch
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
 *
 * Authors: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "glib-glue.hh"
#include "cxx-utils.hh"

#define VTE_DEFINE_UUID(timelow_, timemid_, timehigh_, clock_, node_) \
        vte::uuid{uint32_t(0x ## timelow_ ## u), \
                  uint16_t(0x ## timemid_ ## u), \
                  uint16_t(0x ## timehigh_ ## u), \
                  uint16_t(0x ## clock_ ## u), \
                  uint64_t(0x ## node_ ## ull)}

namespace vte {

struct uuid_v3_t { explicit uuid_v3_t() = default; };
struct uuid_v4_t { explicit uuid_v4_t() = default; };
struct uuid_v5_t { explicit uuid_v5_t() = default; };

inline uuid_v3_t uuid_v3{};
inline uuid_v4_t uuid_v4{};
inline uuid_v5_t uuid_v5{};

class uuid {
public:
        enum class format {
                // A string representation of the form
                // 06e023d5-86d8-420e-8103-383e4566087a
                // with no braces nor urn:uuid: prefix
                SIMPLE = 1u << 0,

                // A string representation of the form
                // {06e023d5-86d8-420e-8103-383e4566087a}
                BRACED = 1u << 1,

                // A string representation the form
                // urn:uuid:06e023d5-86d8-420e-8103-383e4566087a
                URN = 1u << 2,

                ANY = SIMPLE | BRACED | URN,
        };

        constexpr uuid() noexcept = default;

        constexpr uuid(uint32_t timelow,
                       uint16_t timemid,
                       uint16_t timehigh,
                       uint16_t clock,
                       uint64_t node) noexcept
                : m_bytes{
                                (uint8_t)(timelow >> 24 & 0xff),
                                (uint8_t)(timelow >> 16 & 0xff),
                                (uint8_t)(timelow >> 8  & 0xff),
                                (uint8_t)(timelow       & 0xff),
                                (uint8_t)(timemid >> 8  & 0xff),
                                (uint8_t)(timemid       & 0xff),
                                (uint8_t)(timehigh >> 8 & 0xff),
                                (uint8_t)(timehigh      & 0xff),
                                (uint8_t)(clock >> 8    & 0xff),
                                (uint8_t)(clock         & 0xff),
                                (uint8_t)(node >> 40    & 0xff),
                                (uint8_t)(node >> 32    & 0xff),
                                (uint8_t)(node >> 24    & 0xff),
                                (uint8_t)(node >> 16    & 0xff),
                                (uint8_t)(node >> 8     & 0xff),
                                (uint8_t)(node          & 0xff)
                        }
        {
                set_version(version());
        }

        /*
         * @str: a string representing a UUID
         *
         * Reads a UUID from its string representation and set the value in
         * @this. See uuid_string_is_valid() for examples of accepted string
         * representations.
         */
        explicit uuid(std::string_view str,
                      format fmt = format::ANY); // throws

        /*
         * Generates a random UUID (RFC 4122 version 4).
         */
        explicit uuid(uuid_v4_t) noexcept;

        /*
         * @name_space: a namespace #uuid
         * @name: a string
         *
         * Generates a UUID based on the MD5 hash of a name_space UUID and a
         * string (RFC 4122 version 3). MD5 is <emphasis>no longer considered
         * secure</emphasis>, and you should only use this if you need
         * interoperability with existing systems that use version 3 UUIDs.
         * For new code, you should use version 5 UUIDs.
         */
        uuid(uuid_v3_t,
             uuid const& name_space,
             std::string_view const& name) noexcept
                : uuid(3, name_space, name)
        {
        }

        /*
         * @name_space: a namespace #uuid
         * @name: a string
         *
         * Generates a UUID based on the SHA-1 hash of a name_space UUID and a
         * string (RFC 4122 version 5).
         */
        uuid(uuid_v5_t,
             uuid const& name_space,
             std::string_view const& name) noexcept
                : uuid(5, name_space, name)
        {
        }

        friend constexpr auto operator<=>(uuid const& lhs,
                                          uuid const& rhs) = default;

        constexpr auto is_nil() const noexcept
        {
                return operator==(uuid(), *this);
        }

        vte::glib::StringPtr g_str(format fmt = format::SIMPLE) const;

        std::string str(format fmt = format::SIMPLE) const;

private:
        uint8_t m_bytes[16]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        uuid(int version,
             uuid const& name_space,
             std::string_view const& name) noexcept;

        constexpr void set_version(int version) noexcept
        {
                /*
                 * Set the four most significant bits(bits 12 through 15) of the
                 * time_hi_and_version field to the 4-bit version number from
                 * Section 4.1.3.
                 */
                m_bytes[6] &= 0x0f;
                m_bytes[6] |= version << 4;
                /*
                 * Set the two most significant bits(bits 6 and 7) of the
                 * clock_seq_hi_and_reserved to zero and one, respectively.
                 */
                m_bytes[8] &= 0x3f;
                m_bytes[8] |= 0x80;
        }

        constexpr int version() const noexcept
        {
                return m_bytes[6] >> 4;
        }

        constexpr int variant() const noexcept
        {
                return (m_bytes[8] & 0xc0) >> 6;
        }

}; // class uuid

/*
 * uuid_string_is_valid:
 * @str: a string representing a UUID
 *
 * Parses the string @str and verify if it is a UUID.
 *
 * The function accepts the following syntaxes:
 *
 * - simple forms(e.g. f81d4fae-7dec-11d0-a765-00a0c91e6bf6)
 * - simple forms with curly braces(e.g.
 *   {f81d4fae-7dec-11d0-a765-00a0c91e6bf6})
 * - URN(e.g. urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6)
 *
 * Returns: %true if @str is a valid UUID, %false otherwise.
 */
        bool uuid_string_is_valid(std::string_view const& str,
                                  uuid::format fmt = uuid::format::ANY) noexcept;

        std::string uuid_string_random(uuid::format fmt = uuid::format::SIMPLE);

VTE_CXX_DEFINE_BITMASK(uuid::format)

} // namespace vte
