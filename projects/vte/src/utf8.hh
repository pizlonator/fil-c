/*
 * Copyright © 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cstdint>

namespace vte {

namespace base {

/* See https://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for more
 * information on this branchless UTF-8 decoder.
 */
class UTF8Decoder {
public:
        enum {
                ACCEPT = 0,
                REJECT = 12,
                REJECT_REWIND = 108
        };

        UTF8Decoder() noexcept = default;
        UTF8Decoder(UTF8Decoder const&) noexcept = default;
        UTF8Decoder(UTF8Decoder&&) noexcept = default;
        ~UTF8Decoder() noexcept = default;

        UTF8Decoder& operator= (UTF8Decoder const&) = delete;
        UTF8Decoder& operator= (UTF8Decoder&&) = delete;

        inline constexpr uint32_t codepoint() const noexcept { return m_codepoint; }

        inline uint32_t decode(uint32_t byte) noexcept {
                uint32_t type = kTable[byte];
                m_codepoint = (m_state != ACCEPT) ?
                        (byte & 0x3fu) | (m_codepoint << 6) :
                        (0xff >> type) & (byte);

                m_state = kTable[256 + m_state + type];
                return m_state;
        }

        inline void reset() noexcept {
                m_state = ACCEPT;
                m_codepoint = 0xfffdU;
        }

        inline bool flush() noexcept {
                auto state = m_state;
                if (m_state != ACCEPT)
                        reset();
                return state != m_state;
        }

private:
        uint32_t m_state{ACCEPT};
        uint32_t m_codepoint{0};

        static uint8_t const kTable[];

}; // class UTF8Decoder

} // namespace base

} // namespace vte
