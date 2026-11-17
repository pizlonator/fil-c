/*
 * Copyright © 2019 Christian Persch
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
#include <memory>
#include <optional>
#include <string_view>

namespace vte::base {

char** get_icu_charsets(bool aliases = true);

bool get_icu_charset_supported(char const* charset);

bool get_icu_charset_is_ecma35(char const* charset);

std::optional<std::string> convert_icu_u8_to_charset(char const* to_charset,
                                                     std::string_view const str);

std::shared_ptr<UConverter> make_icu_converter(char const* charset,
                                               GError** error = nullptr);

std::shared_ptr<UConverter> clone_icu_converter(UConverter* other,
                                                GError** error = nullptr);

} // namespace vte::base
