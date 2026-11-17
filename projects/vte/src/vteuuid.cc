/*
 * Copyright © 2023 Christian Persch
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

/**
 * SECTION: vte-uuid
 * @short_description: Simple UUID class
 *
 * Since: 0.78
 */

#include "config.h"

#include <cstring>
#include <exception>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteuuid.h"

#include "uuid.hh"
#include "vteuuidinternal.hh"

#include "glib-glue.hh"
#include "vte-glue.hh"

static auto WRAP(vte::uuid* uuid) -> auto
{
        return reinterpret_cast<VteUuid*>(uuid);
}

static auto UNWRAP(VteUuid* wrapper) -> auto
{
        return std::launder(reinterpret_cast<vte::uuid*>(wrapper));
}

static auto UNWRAP(VteUuid const* wrapper) -> auto
{
        return std::launder(reinterpret_cast<vte::uuid const*>(wrapper));
}

/* Type registration */

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
G_DEFINE_BOXED_TYPE(VteUuid, vte_uuid,
                    GBoxedCopyFunc(vte_uuid_dup),
                    GBoxedFreeFunc(vte_uuid_free))
#pragma GCC diagnostic pop

VteUuid*
_vte_uuid_new_from_uuid(vte::uuid const& u) noexcept
try
{
        return WRAP(new vte::uuid{u});
}
catch (...)
{
        return nullptr;
}

VteUuid*
_vte_uuid_wrap(vte::uuid& u) noexcept
{
        return WRAP(std::addressof(u));
}

vte::uuid&
_vte_uuid_unwrap(VteUuid* u) noexcept
{
        return *UNWRAP(u);
}

vte::uuid const&
_vte_uuid_unwrap(VteUuid const* u) noexcept
{
        return *UNWRAP(u);
}

/**
 * VteUuid:
 *
 * An object representing an UUID.
 *
 * Since: 0.78
 */

/**
 * vte_uuid_new_v4:
 *
 * Creates a new random UUID.
 *
 * Returns: (transfer full): a new v4 UUID
 *
 * Since: 0.78
 */
VteUuid*
vte_uuid_new_v4(void) noexcept
try
{
        return WRAP(new vte::uuid(vte::uuid_v4));
}
catch (...)
{
        return nullptr;
}

/**
 * vte_uuid_new_v5:
 * @ns: (nullable): the namespace #VteUuid
 * @str: string data
 * @len: the length of @str, or -1 if @str is NUL terminated
 *
 * Creates a new UUID for @ns and @str.
 *
 * Returns: (transfer full): a new v5 UUID
 *
 * Since: 0.78
 */
VteUuid*
vte_uuid_new_v5(VteUuid const* ns,
                char const* str,
                gssize len) noexcept
try
{
        g_return_val_if_fail(ns, nullptr);
        g_return_val_if_fail(str, nullptr);

        return WRAP(new vte::uuid(vte::uuid_v5,
                                  _vte_uuid_unwrap(ns),
                                  {str, len == -1 ? strlen(str) : size_t(len)}));
}
catch (...)
{
        return nullptr;
 }

/**
 * vte_uuid_new_from_string:
 * @str: a string
 * @len: the length of @str, or -1 is @str is NUL terminated
 * @fmt: flags from #VteUuidFormat for which fmt(s) to accept
 *
 * Creates a new UUID from its string representation @str.
 *
 * Returns: (transfer full) (nullable): a new UUID, or %NULL is
 *   @str is not a valid UUID string representation
 *
 * Since: 0.78
 */
VteUuid*
vte_uuid_new_from_string(char const* str,
                         gssize len,
                         VteUuidFormat fmt) noexcept
try
{
        g_return_val_if_fail(str, nullptr);

        return WRAP(new vte::uuid({str, len == -1 ? strlen(str) : size_t(len)},
                                  vte::uuid::format(fmt)));
}
catch (...)
{
        return nullptr;
}

/**
 * vte_uuid_dup:
 * @uuid: (transfer none): a #VteUuid
 *
 * Creates a copy of @uuid.
 *
 * Returns: (transfer full): a new copy of @@uuid
 *
 * Since: 0.78
 */
VteUuid*
vte_uuid_dup(VteUuid const* uuid) noexcept
try
{
        g_return_val_if_fail(uuid != nullptr, nullptr);

        return _vte_uuid_new_from_uuid(_vte_uuid_unwrap(uuid));
}
catch (...)
{
        return nullptr;
}

/**
 * vte_uuid_free:
 * @uuid: (transfer full): a #VteUuid
 *
 * Frees @uuid.
 *
 * Since: 0.78
 */
void
vte_uuid_free(VteUuid* uuid) noexcept
try
{
        g_return_if_fail(uuid != nullptr);

        delete UNWRAP(uuid);
}
catch (...)
{
}

/**
 * vte_uuid_free_to_string:
 * @uuid: (transfer full): a #VteUuid
 * @fmt: a #VteUuidFormat
 * @len: (optional): a location to store the length of the returned string, or %NULL
 *
 * Frees @uuid and returns its string representation, see
 * vte_uuid_to_string() for more information.
 *
 * Returns: (transfer full): a string representation of @uuid
 *
 * Since: 0.78
 */
char*
vte_uuid_free_to_string(VteUuid* uuid,
                        VteUuidFormat fmt,
                        gsize* len) noexcept
try
{
        g_return_val_if_fail(uuid != nullptr, nullptr);

        auto const holder = vte::take_freeable(uuid);
        return vte_uuid_to_string(holder.get(), fmt, len);
}
catch (...)
{
        if (len)
                *len = 0;
        return nullptr;
}

/**
 * vte_uuid_free_to_string:
 * @uuid: a #VteUuid
 * @fmt: a #VteUuidFormat
 * @len: (optional): a location to store the length of the returned string, or %NULL
 *
 * Returns the string representation of @uuid.
 *
 * Returns: (transfer full): a string representation of @uuid
 *
 * Since: 0.78
 */
char*
vte_uuid_to_string(VteUuid const* uuid,
                   VteUuidFormat fmt,
                   gsize* len) noexcept
try
{
        g_return_val_if_fail(uuid, nullptr);

        auto str = _vte_uuid_unwrap(uuid).g_str(vte::uuid::format(fmt));
        if (len)
                *len = strlen(str.get());
        return str.release();
}
catch (...)
{
        if (len)
                *len = 0;
        return nullptr;
}

/**
 * vte_uuid_equal:
 * @uuid: a #VteUuid
 *
 * Compares @uuid and @other for equality.
 *
 * Returns: %TRUE iff @uuid and @other are equal
 *
 * Since: 0.78
 */
gboolean
vte_uuid_equal(VteUuid const* uuid,
               VteUuid const* other) noexcept
try
{
        g_return_val_if_fail(uuid, false);
        g_return_val_if_fail(other, false);

        return _vte_uuid_unwrap(uuid) == _vte_uuid_unwrap(other);
}
catch (...)
{
        return false;
}

/**
 * vte_uuid_validate_string:
 * @fmt: a #VteUuidFormat
 * @str: a string
 * @len: the length of @str, or -1 is @str is NUL terminated
 *
 * Checks whether @str is a valid string representation of an UUID.
 *
 * Returns: %TRUE iff @str is a valid string representation
 *
 * Since: 0.78
 */
gboolean
vte_uuid_validate_string(char const* str,
                         gssize len,
                         VteUuidFormat fmt) noexcept
try
{
        g_return_val_if_fail(str, false);

        return vte::uuid_string_is_valid({str, len == -1 ? strlen(str) : size_t(len)},
                                         vte::uuid::format(fmt));
}
catch (...)
{
        return false;
}
