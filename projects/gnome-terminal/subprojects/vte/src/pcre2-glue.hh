/*
 * Copyright © 2015 Christian Persch
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

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>
#include <cstdint>

/* Assert compatibility of PCRE2 and GLib types */
static_assert(sizeof(PCRE2_UCHAR8) == sizeof (uint8_t), "PCRE2_UCHAR2 has wrong size");
static_assert(sizeof(PCRE2_SIZE) == sizeof (size_t), "PCRE2_SIZE has wrong size");
static_assert(PCRE2_UNSET == (size_t)-1, "PCRE2_UNSET has wrong value");
static_assert(PCRE2_ZERO_TERMINATED == (size_t)-1, "PCRE2_ZERO_TERMINATED has wrong value");

#include "std-glue.hh"

namespace vte {

VTE_DECLARE_FREEABLE(pcre2_code_8, pcre2_code_free_8);
VTE_DECLARE_FREEABLE(pcre2_compile_context_8, pcre2_compile_context_free_8);
VTE_DECLARE_FREEABLE(pcre2_match_context_8, pcre2_match_context_free_8);
VTE_DECLARE_FREEABLE(pcre2_match_data_8, pcre2_match_data_free_8);

} // namespace vte
