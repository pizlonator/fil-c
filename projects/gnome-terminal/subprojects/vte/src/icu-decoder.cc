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

#include <glib.h>

#include <cassert>
#include <memory>

#include "icu-decoder.hh"
#include "icu-glue.hh"

namespace vte::base {

/*
 * ICUDecoder::decode:
 * @sptr: inout pointer to input data
 * @flush: whether to flush
 *
 * Decodes input, and advances *@sptr for input consumed. At most
 * one byte of input is consumed; if flushing, no input is consumed.
 *
 * Returns: whether there is an output character available
 */
ICUDecoder::Result
ICUDecoder::decode(uint8_t const** sptr,
                   bool flush) noexcept
{
        switch (m_state) {
        case State::eOutput:
                if (++m_index < m_available)
                        return Result::eSomething;

                m_state = State::eInput;
                [[fallthrough]];
        case State::eInput: {
                /* Convert in two stages from charset to UTF-32, pivoting through UTF-16.
                 * This is similar to ucnv_convertEx(), but that API does not fit our
                 * requirements completely.
                 *
                 * This function is similar to ucnv_getNextUChar, except that it works
                 * with streaming (and thus may produce no output in some steps), while
                 * ucnv_getNextUChar does not.
                 */

                auto source_ptr = reinterpret_cast<char const**>(sptr);
                auto source_start = *source_ptr;
                auto source_limit = source_start + (1 - flush);

                auto target_u16_start = u16_buffer();
                auto target_u16_limit = u16_buffer_end();
                auto target_u16 = target_u16_start;

                m_err.reset();
                ucnv_toUnicode(m_charset_converter.get(),
                               &target_u16, target_u16_limit,
                               source_ptr, source_limit,
                               nullptr /* offsets */,
                               flush,
                               m_err);

                /* There should be no error here. We use the default callback
                 * which replaces invalid input with replacment character (either
                 * U+FFFD or SUB), and we should never hit U_BUFFER_OVERFLOW_ERROR,
                 * since we process only one input byte at a time and the output
                 * buffer holds at most 1 UTF-16 character (a high surrogate), and
                 * there are no charsets where the state is so deep as to suddently
                 * output 32 characters.
                 */
                if (m_err.isFailure()) {
                        m_state = State::eError;
                        return Result::eError;
                }

                /* Now convert from UTF-16 to UTF-32. There will be no overflow here
                 * either, since the output buffer is empty, and for each UTF-16 code
                 * point of input, the decoder will output at most one UTF-32 code
                 * point.
                 */

                auto target_u32_start = reinterpret_cast<char*>(u32_buffer());
                auto target_u32_limit = reinterpret_cast<char const*>(u32_buffer_end());
                auto target_u32 = target_u32_start;
                auto target_u16_u32_start = const_cast<char16_t const*>(target_u16_start);
                auto target_u16_u32_limit = target_u16;

                ucnv_fromUnicode(m_u32_converter.get(),
                                 &target_u32, target_u32_limit,
                                 &target_u16_u32_start, target_u16_u32_limit,
                                 nullptr /* offsets */,
                                 flush,
                                 m_err);
                if (m_err.isFailure()) {
                        m_state = State::eError;
                        return Result::eError;
                }

                if (target_u32 == target_u32_start) {
                        if (*source_ptr == source_start && !flush) {
                                /* The decoder produced neither output nor consumed input, and
                                 * wan't flushing? That shouldn't happen; go to error state,
                                 * requiring an explicit reset() to proceed further.
                                 */
                                m_state = State::eError;
                                return Result::eError;
                        }

                        return Result::eNothing;
                }

                /* We have some output! */
                assert((target_u32 - target_u32_start) % sizeof(m_u32_buffer[0]) == 0);
                m_available = (target_u32 - target_u32_start) / sizeof(m_u32_buffer[0]);
                assert(m_available >= 1);

                m_index = 0;
                return Result::eSomething;
        }

        case State::eError:
        default:
                return Result::eError;
        }
}

void
ICUDecoder::reset() noexcept
{
        ucnv_resetToUnicode(m_charset_converter.get());
        ucnv_resetFromUnicode(m_u32_converter.get());
        m_err.reset();
        m_state = State::eInput;
        m_available = 0;
        m_index = 0;
}

std::unique_ptr<ICUDecoder>
ICUDecoder::clone(ICUDecoder const& other)
{
        auto charset_converter = vte::base::clone_icu_converter(other.m_charset_converter.get());
        if (!charset_converter)
                return {};
        auto u32_converter = vte::base::clone_icu_converter(other.m_u32_converter.get());
        if (!u32_converter)
                return {};

        return std::make_unique<ICUDecoder>(std::move(charset_converter),
                                            std::move(u32_converter));
}

} // namespace vte::base
