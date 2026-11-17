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
#include <string>

#include <unicode/ucnv.h>

#include "icu-decoder.hh"

namespace vte::base {

class ICUConverter {
public:
        using converter_shared_type = std::shared_ptr<UConverter>;

        static std::unique_ptr<ICUConverter> make(char const *charset,
                                                  GError** error = nullptr);

        ICUConverter(char const* charset,
                     converter_shared_type charset_converter,
                     converter_shared_type u32_converter,
                     converter_shared_type u8_converter)
                : m_charset(charset),
                  m_charset_converter(charset_converter),
                  m_u32_converter(u32_converter),
                  m_u8_converter(u8_converter),
                  m_decoder(charset_converter, u32_converter)
        {
        }

        ~ICUConverter() = default;

        ICUConverter(ICUConverter const&) = delete;
        ICUConverter(ICUConverter&&) = delete;
        ICUConverter& operator= (ICUConverter const&) = delete;
        ICUConverter& operator= (ICUConverter&&) = delete;

        constexpr auto const& charset() const noexcept { return m_charset; }
        constexpr auto& decoder() noexcept { return m_decoder; }

        auto charset_converter() noexcept { return m_charset_converter.get(); }
        auto u32_converter() noexcept     { return m_u32_converter.get();     }
        auto u8_converter() noexcept      { return m_u8_converter.get();      }

        std::string convert(std::string_view const& data);

private:
        std::string m_charset;
        converter_shared_type m_charset_converter;
        converter_shared_type m_u32_converter;
        converter_shared_type m_u8_converter;
        vte::base::ICUDecoder m_decoder;

        /* Note that m_decoder will share m_charset_converter and only use it in the
         * toUnicode direction; and m_u32_decoder, and will use that only in the
         * fromUnicode direction.
         * convert() will only use m_charset_converter in the fromUnicode direction.
         */
};

} // namespace vte::base
