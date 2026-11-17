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

#include <glib.h>
#include <assert.h>
#include <cstdint>
#include <cstring>

/*
 * vte_seq_string_t:
 *
 * A type to hold the argument string of a DSC or OSC sequence.
 */
typedef struct vte_seq_string_t {
        uint32_t capacity;
        uint32_t len;
        uint32_t* buf;
} vte_seq_string_t;

#define VTE_SEQ_STRING_DEFAULT_CAPACITY (1 << 7) /* must be power of two */

#define VTE_SEQ_STRING_MAX_CAPACITY     (1 << 12)

/*
 * vte_seq_string_init:
 *
 * Returns: a new #vte_seq_string_t
 */
static inline void vte_seq_string_init(vte_seq_string_t* str) noexcept
{
        str->capacity = VTE_SEQ_STRING_DEFAULT_CAPACITY;
        str->len = 0;
        str->buf = (uint32_t*)g_malloc0_n(str->capacity, sizeof(uint32_t));
}

/*
 * vte_seq_string_free:
 * @string:
 *
 * Frees @string's storage and itself.
 */
static inline void vte_seq_string_free(vte_seq_string_t* str) noexcept
{
        g_free(str->buf);
}

/*
 * vte_seq_string_ensure_capacity:
 * @string:
 *
 * If @string's length is at capacity, and capacity is not maximal,
 * expands the string's capacity.
 *
 * Returns: %true if the string has capacity for at least one more character
 */
static inline bool vte_seq_string_ensure_capacity(vte_seq_string_t* str) noexcept
{
        if (str->len < str->capacity)
                return true;
        if (str->capacity >= VTE_SEQ_STRING_MAX_CAPACITY)
                return false;

        str->capacity *= 2;
        str->buf = (uint32_t*)g_realloc_n(str->buf, str->capacity, sizeof(uint32_t));
        return true;
}

/*
 * vte_seq_string_push:
 * @string:
 * @c: a character
 *
 * Appends @c to @str, or iff @str already has maximum length, does nothing.
 *
 * Returns: %true if the character was appended
 */
static inline bool vte_seq_string_push(vte_seq_string_t* str,
                                       uint32_t c) noexcept
{
        if (!vte_seq_string_ensure_capacity(str))
                return false;

        str->buf[str->len++] = c;
        return true;
}

/*
 * vte_seq_string_finish:
 * @string:
 *
 * Finishes @string; after this no more vte_seq_string_push() calls
 * are allowed until the string is reset with vte_seq_string_reset().
 */
static inline void vte_seq_string_finish(vte_seq_string_t* str)
{
}

/*
 * vte_seq_string_reset:
 * @string:
 *
 * Resets @string.
 */
static inline void vte_seq_string_reset(vte_seq_string_t* str) noexcept
{
        /* Zero length. However, don't clear the buffer, nor shrink the capacity. */
        str->len = 0;
}

/*
 * vte_seq_string_get:
 * @string:
 * @len: location to store the buffer length in code units
 *
 * Returns: the string's buffer as an array of uint32_t code units
 */
static constexpr inline uint32_t* vte_seq_string_get(vte_seq_string_t const* str,
                                                     size_t* len) noexcept
{
        assert(len != nullptr);
        *len = str->len;
        return str->buf;
}
