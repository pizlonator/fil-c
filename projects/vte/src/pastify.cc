/*
 * Copyright © 2015, 2019, Egmont Koblinger
 * Copyright © 2015, 2018, 2019, 2020, 2021 Christian Persch
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

#include <config.h>

#include "pastify.hh"

namespace vte::terminal {

using namespace std::literals;

/*
 * pastify_string:
 * @str:
 * @insert_brackets:
 * @c1:
 *
 * Converts @str into a form safe for pasting to the child. Elide
 * C0 controls except NUL, HT, CR, LF, and C1 controls.
 * We also convert newlines to carriage returns, which more software
 * is able to cope with (cough, pico, cough).
 *
 * Also insert bracketed paste controls around the string if
 * @insert_brackets is true, using C1 CSI if @c1 is true or C0 controls
 * otherwise.
 */
std::string
pastify_string(std::string_view str,
               bool insert_brackets,
               bool c1)
{
        auto rv = std::string{};
        rv.reserve(str.size() + 1 + (insert_brackets ? 12 : 0));

        if (insert_brackets) {
                if (c1)
                        rv.append("\xc2\x9b" "200~");
                else
                        rv.append("\e[200~");
        }

        /* C0 \ { NUL, HT, CR, LF } + { DEL } + { C1 control start byte } */
        auto const controls = "\x01\x02\x03\x04\x05\x06\x07\x08\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f\xc2"sv;

        auto next_char = [&str](size_t pos) constexpr noexcept -> unsigned char
        {
                return (pos + 1) < str.size() ? str[pos + 1] : 0;
        };

        while (str.size() != 0) {
                auto run = str.find_first_of(controls, 0);

                rv.append(str, 0, run);
                if (run == str.npos)
                        break;

                switch (char8_t(str[run])) {
                case 0x01 ... 0x09:
                case 0x0b ... 0x0c:
                case 0x0e ... 0x1f:
                case 0x7f:
                        append_control_picture(rv, str[run]);
                        ++run;
                        break;

                case 0x0a: // LF
                        // We only get here for a lone LF; replace it with a CR too.
                        rv.push_back(0x0d);
                        ++run;
                        break;

                case 0x0d: // CR
                        // Keep a CR, but replace a CRLF with just a CR
                        rv.push_back(0x0d);
                        if (next_char(run) == 0x0a)
                                ++run; // skip

                        ++run;
                        break;

                case 0xc2: { // First byte of a 2-byte UTF-8 sequence
                        auto const c = next_char(run);
                        if (c >= 0x80 && c <= 0x9f) {
                                append_control_picture(rv, c);

                                // Skip both bytes of a C1 control
                                run += 2;
                        } else {
                                // Not a C1 control, keep this byte and continue
                                rv.push_back(0xc2);
                                ++run;
                        }
                        break;
                }

                default: // Can't happen
                        ++run;
                        break;
                }

                /* run is <= str.size() */
                str = str.substr(run);
        }

        if (insert_brackets) {
                if (c1)
                        rv.append("\xc2\x9b" "201~");
                else
                        rv.append("\e[201~");
        }

        return rv;
}

/*
 * append_control_picture:
 * @str:
 * @c:
 *
 * Appends the control picture for @ctrl (or if @ctrl has no control
 * picture in unicode, appends U+FFFD).
 */
void
append_control_picture(std::string& str,
                       char32_t ctrl)
{
        switch (ctrl) {
        case 0x00 ... 0x1f: /* C0 */
                // U+2400 SYMBOL FOR NULL
                // U+2401 SYMBOL FOR START OF HEADING
                // U+2402 SYMBOL FOR START OF TEXT
                // U+2403 SYMBOL FOR END OF TEXT
                // U+2404 SYMBOL FOR END OF TRANSMISSION
                // U+2405 SYMBOL FOR ENQUIRY
                // U+2406 SYMBOL FOR ACKNOWLEDGE
                // U+2407 SYMBOL FOR BELL
                // U+2408 SYMBOL FOR BACKSPACE
                // U+2409 SYMBOL FOR HORIZONTAL TABULATION
                // U+240A SYMBOL FOR LINE FEED
                // U+240B SYMBOL FOR VERTICAL TABULATION
                // U+240C SYMBOL FOR FORM FEED
                // U+240D SYMBOL FOR CARRIAGE RETURN
                // U+240E SYMBOL FOR SHIFT OUT
                // U+240F SYMBOL FOR SHIFT IN
                // U+2410 SYMBOL FOR DATA LINK ESCAPE
                // U+2411 SYMBOL FOR DEVICE CONTROL ONE
                // U+2412 SYMBOL FOR DEVICE CONTROL TWO
                // U+2413 SYMBOL FOR DEVICE CONTROL THREE
                // U+2414 SYMBOL FOR DEVICE CONTROL FOUR
                // U+2415 SYMBOL FOR NEGATIVE ACKNOWLEDGE
                // U+2416 SYMBOL FOR SYNCHRONOUS IDLE
                // U+2417 SYMBOL FOR END OF TRANSMISSION BLOCK
                // U+2418 SYMBOL FOR CANCEL
                // U+2419 SYMBOL FOR END OF MEDIUM
                // U+241A SYMBOL FOR SUBSTITUTE
                // U+241B SYMBOL FOR ESCAPE
                // U+241C SYMBOL FOR FILE SEPARATOR
                // U+241D SYMBOL FOR GROUP SEPARATOR
                // U+241E SYMBOL FOR RECORD SEPARATOR
                // U+241F SYMBOL FOR UNIT SEPARATOR
                str.push_back('\xe2');
                str.push_back('\x90');
                str.push_back(ctrl + 0x80);
                break;

        case 0x7f: /* DEL */
                str.append("\xe2\x90\xa1"); // U+2421 SYMBOL FOR DELETE
                break;

        case 0x80 ... 0x9f: /* C1 */
                // Unfortunately, over 20 years after being first proposed, unicode
                // **still** does not have control pictures for the C1 controls.
                //
                // Use U+FFFD instead.
                str.append("\xef\xbf\xbd");
                break;

        default:
                // This function may only be called for controls
                __builtin_unreachable();
        }
}

} // namespace vte::terminal
