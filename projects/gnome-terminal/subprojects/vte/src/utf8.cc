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

#include "utf8.hh"

#define RJ vte::base::UTF8Decoder::REJECT
#define RW vte::base::UTF8Decoder::REJECT_REWIND

uint8_t const vte::base::UTF8Decoder::kTable[] = {
        // The first part of the table maps bytes to character classes that
        // to reduce the size of the transition table and create bitmasks.
        // The classes are as follows:
        // 0x00..0x7f: 0
        // 0x80..0x8f: 1
        // 0x90..0x9f: 9
        // 0xa0..0xbf: 7
        // 0xc0..0xc1: 8
        // 0xc2..0xdf: 2
        // 0xe0:       10
        // 0xe1..0xec: 3
        // 0xed:       4
        // 0xee..0xef: 3
        // 0xf0:       11
        // 0xf1..0xf3: 6
        // 0xf4:       5
        // 0xf5..0xff: 8
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00..0x0f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10..0x1f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x20..0x2f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x30..0x3f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40..0x4f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x50..0x5f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x60..0x6f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x70..0x7f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x80..0x8f
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 0x90..0x9f
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // 0xa0..0xaf
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // 0xb0..0xbf
        8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xc0..0xcf
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xd0..0xdf
        10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, // 0xe0..0xef
        11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 0xf0..0xff

        // To understand this DFA, see transitions graph on the website
        // linked above.
        //
        // The following translates the states of the DFA to the
        // algorithm of the UTF-8 decoder from the W3 Encodings spec
        // [https://www.w3.org/TR/encoding/#utf-8]:
        //
        // DFA   │ bytes   bytes   lower   upper
        // state │ seen    needed  bound   bound
        // ──────┼─────────────────────────────────
        //   0   │ 0       0       0x80    0xbf
        //  12   │
        //  24   │ 1,2,3   1       0x80    0xbf
        //  36   │ 1,2     2       0x80    0xbf
        //  48   │ 1       2       0xa0    0xbf
        //  60   │ 1       2       0x80    0x9f
        //  72   │ 1       3       0x90    0xbf
        //  84   │ 1       3       0x80    0xbf
        //  96   │ 1       3       0x80    0x8f
        // 108   │
        //
        // If an unexpected byte is read in a non-ACCEPT/REJECT* state,
        // transition to REJECT_REWIND so that the decoder will read that
        // byte again after being reset; this makes the decoder conform
        // to the Unicode recommendation for insering replacement
        // characters, and to the W3 Encoding TR spec.
        //
        // If an unexpected byte is read in the ACCEPT or a REJECT* state,
        // transition to REJECT; that byte must not be read again, since
        // that would lead to an infinite loop.
        //
        // For each state (row), the table records which state will
        // be transitioned to when consuming a character of the class
        // (column).
        /*
         0   1   2   3   4   5   6   7   8   9  10  11 // character class
        */
         0, RJ, 24, 36, 60, 96, 84, RJ, RJ, RJ, 48, 72, // state 0 (accept)
        RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, // state 12 (reject)
        RW,  0, RW, RW, RW, RW, RW,  0, RW,  0, RW, RW, // state 24
        RW, 24, RW, RW, RW, RW, RW, 24, RW, 24, RW, RW, // state 36
        RW, RW, RW, RW, RW, RW, RW, 24, RW, RW, RW, RW, // state 48
        RW, 24, RW, RW, RW, RW, RW, RW, RW, 24, RW, RW, // state 60
        RW, RW, RW, RW, RW, RW, RW, 36, RW, 36, RW, RW, // state 72
        RW, 36, RW, RW, RW, RW, RW, 36, RW, 36, RW, RW, // state 84
        RW, 36, RW, RW, RW, RW, RW, RW, RW, RW, RW, RW, // state 96
        RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, // state 108 (reject-rewind)
};
