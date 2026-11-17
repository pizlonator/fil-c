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

#include "config.h"

#include "icu-converter.hh"

#include <cassert>
#include <memory>

#include <unicode/errorcode.h>

#include "debug.h"
#include "icu-glue.hh"

namespace vte::base {

std::unique_ptr<ICUConverter>
ICUConverter::make(char const* charset,
                   GError** error)
{
        if (vte::base::get_icu_charset_is_ecma35(charset))
                return {};

        auto charset_converter = vte::base::make_icu_converter(charset, error);
        if (!charset_converter)
                return {};

        auto u32_converter = vte::base::make_icu_converter("utf32platformendian", error);
        if (!u32_converter)
                return {};

        auto u8_converter = vte::base::make_icu_converter("utf8", error);
        if (!u8_converter)
                return {};

        return std::make_unique<ICUConverter>(charset, charset_converter, u32_converter, u8_converter);
}

std::string
ICUConverter::convert(std::string_view const& data)
{
        /* We can't use ucnv_convertEx since that doesn't support preflighting.
         * Instead, convert to UTF-16 first, and the to the target, with
         * preflighting both times. This is slow, but this is the legacy
         * code path, so we don't care.
         */

        if (data.size() == 0)
                return {};

        ucnv_resetToUnicode(m_u8_converter.get());

        auto err = icu::ErrorCode{};
        auto u16_size = ucnv_toUChars(m_u8_converter.get(),
                                      nullptr, 0,
                                      data.data(), data.size(),
                                      err);
        if (err.isFailure() && (err.get() != U_BUFFER_OVERFLOW_ERROR)) {
                _vte_debug_print(VTE_DEBUG_CONVERSION,
                                 "Error converting from UTF-8 to UTF-16 in preflight: %s\n",
                                 err.errorName());
                return {};
        }

        auto u16_buffer = std::u16string{};
        if ((size_t)u16_size > u16_buffer.max_size()) // prevent exceptions
                return {};
        u16_buffer.resize(u16_size);

        err.reset();
        u16_size = ucnv_toUChars(m_u8_converter.get(),
                                 u16_buffer.data(),
                                 u16_buffer.size(),
                                 data.data(),
                                 data.size(),
                                 err);
        if (err.isFailure()) {
                _vte_debug_print(VTE_DEBUG_CONVERSION,
                                 "Error converting from UTF-8 to UTF-16: %s\n",
                                 err.errorName());
                return {};
        }

        /* Now convert to target */
        ucnv_resetFromUnicode(m_charset_converter.get());
        err.reset();
        auto target_size = ucnv_fromUChars(m_charset_converter.get(),
                                           nullptr, 0,
                                           u16_buffer.data(),
                                           u16_size,
                                           err);
        if (err.isFailure() && (err.get() != U_BUFFER_OVERFLOW_ERROR)) {
                _vte_debug_print(VTE_DEBUG_CONVERSION,
                                 "Error converting from UTF-8 to %s in preflight: %s\n",
                                 m_charset.c_str(),
                                 err.errorName());
                return {};
        }

        auto target_buffer = std::string{};
        if ((size_t)target_size > target_buffer.max_size()) // prevent exceptions
                return {};
        target_buffer.resize(target_size);

        err.reset();
        target_size = ucnv_fromUChars(m_charset_converter.get(),
                                      target_buffer.data(),
                                      target_buffer.capacity(),
                                      u16_buffer.data(),
                                      u16_size,
                                      err);
        if (err.isFailure()) {
                _vte_debug_print(VTE_DEBUG_CONVERSION,
                                 "Error converting from UTF-16 to %s: %s\n",
                                 m_charset.c_str(),
                                 err.errorName());
                return {};
        }

        return target_buffer;
}

} // namespace vte::base
