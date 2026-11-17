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

#include "config.h"

#include "utf8.hh"

#include <cstring>
#include <string>

#include <glib.h>

using namespace std::literals;
using namespace vte::base;

UTF8Decoder decoder{};

static void
test_utf8_decoder_decode(void)
{
        decoder.reset();

        uint8_t buf[7];
        uint32_t state = UTF8Decoder::ACCEPT;
        for (uint32_t cp = 0; cp < 0x110000u; ++cp) {
                if ((cp & 0xfffff800) == 0xd800u)
                        continue; // surrogate

                int len = g_unichar_to_utf8(cp, (char*)buf);
                for (int i = 0; i < len; ++i)
                        state = decoder.decode(buf[i]);
                g_assert_cmpint(state, ==, UTF8Decoder::ACCEPT);
                g_assert_cmpuint(decoder.codepoint(), ==, cp);
        }
}

static void
decode(uint8_t const* in,
       size_t len,
       std::u32string& out)
{
        decoder.reset();

        auto const iend = in + len;
        for (auto iptr = in; iptr < iend; ++iptr) {
                switch (decoder.decode(*iptr)) {
                case vte::base::UTF8Decoder::REJECT_REWIND:
                        /* Note that this will never lead to a loop, since in the
                         * next round this byte *will* be consumed.
                         */
                        --iptr;
                        [[fallthrough]];
                case vte::base::UTF8Decoder::REJECT:
                        decoder.reset();
                        /* Fall through to insert the U+FFFD replacement character. */
                        [[fallthrough]];
                case vte::base::UTF8Decoder::ACCEPT:
                        out.push_back(decoder.codepoint());
                        break;
                default:
                        break;
                }
        }

        /* If we get EOS without having just accepted a character,
         * we need to insert a replacement character since we're
         * aborting a sequence mid-way.
         */
        if (decoder.flush()) {
                out.push_back(decoder.codepoint());
        }
}

static void
assert_u32streq(std::u32string const& str1,
                std::u32string const& str2)
{
        g_assert_cmpuint(str1.size(), ==, str2.size());
        g_assert_true(str1 == str2);
}

static void
assert_decode(char const* in,
              ssize_t len,
              std::u32string const& expected)
{
        std::u32string converted;
        decode((uint8_t const*)in, len != -1 ? size_t(len) : strlen(in), converted);
        assert_u32streq(converted, expected);
}

static void
test_utf8_decoder_replacement(void)
{
        /* The following test vectors are copied from rust encoding_rs/src/utf8.rs:
         *
         * Copyright 2015-2016 Mozilla Foundation
         *
         * Permission is hereby granted, free of charge, to any
         * person obtaining a copy of this software and associated
         * documentation files (the "Software"), to deal in the
         * Software without restriction, including without
         * limitation the rights to use, copy, modify, merge,
         * publish, distribute, sublicense, and/or sell copies of
         * the Software, and to permit persons to whom the Software
         * is furnished to do so, subject to the following
         * conditions:
         *
         * The above copyright notice and this permission notice
         * shall be included in all copies or substantial portions
         * of the Software.
         *
         * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
         * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
         * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
         * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
         * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
         * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
         * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
         * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
         * DEALINGS IN THE SOFTWARE.
         */

        // Empty
        assert_decode("", -1, U""s);
        // NUL
        assert_decode("\0", 1, U"\0"s);
        // ASCII
        assert_decode("ab", -1, U"ab"s);
        // Low BMP
        assert_decode("a\xC3\xA4Z", -1, U"a\u00E4Z"s);
        // High BMP
        assert_decode("a\xE2\x98\x83Z", -1, U"a\u2603Z"s);
        // Astral
        assert_decode("a\xF0\x9F\x92\xa9Z", -1, U"a\U0001F4A9Z"s);
        // Low BMP with last byte missing
        assert_decode("a\xC3Z", -1, U"a\uFFFDZ"s);
        assert_decode("a\xC3", -1, U"a\uFFFD"s);
        // High BMP with last byte missing
        assert_decode("a\xE2\x98Z", -1, U"a\uFFFDZ"s);
        assert_decode("a\xE2\x98", -1, U"a\uFFFD"s);
        // Astral with last byte missing
        assert_decode("a\xF0\x9F\x92Z", -1, U"a\uFFFDZ"s);
        assert_decode("a\xF0\x9F\x92", -1, U"a\uFFFD"s);
        // Lone highest continuation
        assert_decode("a\xBFZ", -1, U"a\uFFFDZ"s);
        assert_decode("a\xBF", -1, U"a\uFFFD"s);
        // Two lone highest continuations
        assert_decode("a\xBF\xBFZ", -1, U"a\uFFFD\uFFFDZ"s);
        assert_decode("a\xBF\xBF", -1, U"a\uFFFD\uFFFD"s);
        // Low BMP followed by lowest lone continuation
        assert_decode("a\xC3\xA4\x80Z", -1, U"a\u00E4\uFFFDZ"s);
        assert_decode("a\xC3\xA4\x80", -1, U"a\u00E4\uFFFD"s);
        // Low BMP followed by highest lone continuation
        assert_decode("a\xC3\xA4\xBFZ", -1, U"a\u00E4\uFFFDZ"s);
        assert_decode("a\xC3\xA4\xBF", -1, U"a\u00E4\uFFFD"s);
        // High BMP followed by lowest lone continuation
        assert_decode("a\xE2\x98\x83\x80Z", -1, U"a\u2603\uFFFDZ"s);
        assert_decode("a\xE2\x98\x83\x80", -1, U"a\u2603\uFFFD"s);
        // High BMP followed by highest lone continuation
        assert_decode("a\xE2\x98\x83\xBFZ", -1, U"a\u2603\uFFFDZ"s);
        assert_decode("a\xE2\x98\x83\xBF", -1, U"a\u2603\uFFFD"s);
        // Astral followed by lowest lone continuation
        assert_decode("a\xF0\x9F\x92\xA9\x80Z", -1, U"a\U0001F4A9\uFFFDZ"s);
        assert_decode("a\xF0\x9F\x92\xA9\x80", -1, U"a\U0001F4A9\uFFFD"s);
        // Astral followed by highest lone continuation
        assert_decode("a\xF0\x9F\x92\xA9\xBFZ", -1, U"a\U0001F4A9\uFFFDZ"s);
        assert_decode("a\xF0\x9F\x92\xA9\xBF", -1, U"a\U0001F4A9\uFFFD"s);

        // Boundary conditions
        // Lowest single-byte
        assert_decode("Z\x00", 2, U"Z\0"s);
        assert_decode("Z\x00Z", 3, U"Z\0Z"s);
        // Lowest single-byte as two-byte overlong sequence
        assert_decode("a\xC0\x80", -1, U"a\uFFFD\uFFFD"s);
        assert_decode("a\xC0\x80Z", -1, U"a\uFFFD\uFFFDZ"s);
        // Lowest single-byte as three-byte overlong sequence
        assert_decode("a\xE0\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xE0\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // Lowest single-byte as four-byte overlong sequence
        assert_decode("a\xF0\x80\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x80\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // One below lowest single-byte
        assert_decode("a\xFF", -1, U"a\uFFFD"s);
        assert_decode("a\xFFZ", -1, U"a\uFFFDZ"s);
        // Highest single-byte
        assert_decode("a\x7F", -1, U"a\u007F"s);
        assert_decode("a\x7FZ", -1, U"a\u007FZ"s);
        // Highest single-byte as two-byte overlong sequence
        assert_decode("a\xC1\xBF", -1, U"a\uFFFD\uFFFD"s);
        assert_decode("a\xC1\xBFZ", -1, U"a\uFFFD\uFFFDZ"s);
        // Highest single-byte as three-byte overlong sequence
        assert_decode("a\xE0\x81\xBF", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xE0\x81\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // Highest single-byte as four-byte overlong sequence
        assert_decode("a\xF0\x80\x81\xBF", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x80\x81\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // One past highest single byte (also lone continuation)
        assert_decode("a\x80Z", -1, U"a\uFFFDZ"s);
        assert_decode("a\x80", -1, U"a\uFFFD"s);
        // Two lone continuations
        assert_decode("a\x80\x80Z", -1, U"a\uFFFD\uFFFDZ"s);
        assert_decode("a\x80\x80", -1, U"a\uFFFD\uFFFD"s);
        // Three lone continuations
        assert_decode("a\x80\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        assert_decode("a\x80\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        // Four lone continuations
        assert_decode("a\x80\x80\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        assert_decode("a\x80\x80\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        // Lowest two-byte
        assert_decode("a\xC2\x80", -1, U"a\u0080"s);
        assert_decode("a\xC2\x80Z", -1, U"a\u0080Z"s);
        // Lowest two-byte as three-byte overlong sequence
        assert_decode("a\xE0\x82\x80", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xE0\x82\x80Z", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // Lowest two-byte as four-byte overlong sequence
        assert_decode("a\xF0\x80\x82\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x80\x82\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Lead one below lowest two-byte
        assert_decode("a\xC1\x80", -1, U"a\uFFFD\uFFFD"s);
        assert_decode("a\xC1\x80Z", -1, U"a\uFFFD\uFFFDZ"s);
        // Trail one below lowest two-byte
        assert_decode("a\xC2\x7F", -1, U"a\uFFFD\u007F"s);
        assert_decode("a\xC2\x7FZ", -1, U"a\uFFFD\u007FZ"s);
        // Highest two-byte
        assert_decode("a\xDF\xBF", -1, U"a\u07FF"s);
        assert_decode("a\xDF\xBFZ", -1, U"a\u07FFZ"s);
        // Highest two-byte as three-byte overlong sequence
        assert_decode("a\xE0\x9F\xBF", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xE0\x9F\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // Highest two-byte as four-byte overlong sequence
        assert_decode("a\xF0\x80\x9F\xBF", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x80\x9F\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Lowest three-byte
        assert_decode("a\xE0\xA0\x80", -1, U"a\u0800"s);
        assert_decode("a\xE0\xA0\x80Z", -1, U"a\u0800Z"s);
        // Lowest three-byte as four-byte overlong sequence
        assert_decode("a\xF0\x80\xA0\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x80\xA0\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Highest below surrogates
        assert_decode("a\xED\x9F\xBF", -1, U"a\uD7FF"s);
        assert_decode("a\xED\x9F\xBFZ", -1, U"a\uD7FFZ"s);
        // Highest below surrogates as four-byte overlong sequence
        assert_decode("a\xF0\x8D\x9F\xBF", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x8D\x9F\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // First surrogate
        assert_decode("a\xED\xA0\x80", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xED\xA0\x80Z", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // First surrogate as four-byte overlong sequence
        assert_decode("a\xF0\x8D\xA0\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x8D\xA0\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Last surrogate
        assert_decode("a\xED\xBF\xBF", -1, U"a\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xED\xBF\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFDZ"s);
        // Last surrogate as four-byte overlong sequence
        assert_decode("a\xF0\x8D\xBF\xBF", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x8D\xBF\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Lowest above surrogates
        assert_decode("a\xEE\x80\x80", -1, U"a\uE000"s);
        assert_decode("a\xEE\x80\x80Z", -1, U"a\uE000Z"s);
        // Lowest above surrogates as four-byte overlong sequence
        assert_decode("a\xF0\x8E\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x8E\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Highest three-byte
        assert_decode("a\xEF\xBF\xBF", -1, U"a\uFFFF"s);
        assert_decode("a\xEF\xBF\xBFZ", -1, U"a\uFFFFZ"s);
        // Highest three-byte as four-byte overlong sequence
        assert_decode("a\xF0\x8F\xBF\xBF", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF0\x8F\xBF\xBFZ", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);
        // Lowest four-byte
        assert_decode("a\xF0\x90\x80\x80", -1, U"a\U00010000"s);
        assert_decode("a\xF0\x90\x80\x80Z", -1, U"a\U00010000Z"s);
        // Highest four-byte
        assert_decode("a\xF4\x8F\xBF\xBF", -1, U"a\U0010FFFF"s);
        assert_decode("a\xF4\x8F\xBF\xBFZ", -1, U"a\U0010FFFFZ"s);
        // One past highest four-byte
        assert_decode("a\xF4\x90\x80\x80", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("a\xF4\x90\x80\x80Z", -1, U"a\uFFFD\uFFFD\uFFFD\uFFFDZ"s);

        // Highest four-byte with last byte replaced with 0xFF
        assert_decode("a\xF4\x8F\xBF\xFF", -1, U"a\uFFFD\uFFFD"s);
        assert_decode("a\xF4\x8F\xBF\xFFZ", -1, U"a\uFFFD\uFFFDZ"s);

        // Test old-style-UTF-8 sequences
        // Five-byte (lowest and highest)
        assert_decode("\xF8\x80\x80\x80\x80", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("\xF8\xBF\xBF\xBF\xBF", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);
        // Six-byte (lowest and highest)
        assert_decode("\xFC\x80\x80\x80\x80\x80", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("\xFD\xBF\xBF\xBF\xBF\xBF", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);

        // 0xFE "start byte"
        assert_decode("\xFE\x80\x80\x80\x80\x80\x80", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);
        assert_decode("\xFE\xBF\xBF\xBF\xBF\xBF\xBF", -1, U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"s);
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/utf8/decoder/decode", test_utf8_decoder_decode);
        g_test_add_func("/vte/utf8/decoder/replacement", test_utf8_decoder_replacement);

        return g_test_run();
}
