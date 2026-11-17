// Copyright © 2023 Christian Persch
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"

#include "color-parser.hh"
#include "color.hh"

#include <algorithm>
#include <cctype>
#include <charconv>

#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstring>

#include <glib.h>

#include "color-names.hh"

namespace vte::color::impl {

// BEGIN code copied from gtk+ and pango

// The code below was copied from gtk+, there under LGPL2+; used and modified
// here and distributed under LGPL3+.
//
// Below is the copyright notice from gtk+/gdk/gdkrgba.c:
//
// Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library. If not, see <http://www.gnu.org/licenses/>.
//
// Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
// file for a list of people on the GTK+ Team.  See the ChangeLog
// files for a list of changes.  These files are distributed with
// GTK+ at ftp://ftp.gtk.org/pub/gtk/.
//
// ---
//
// The code below was copied from pango. Below is the copyright notice from
// pango/pango-color.c for the below functions:
//
// The following routines come from Tk, via the Win32
// port of GDK. The licensing terms on these (longer than the functions) is:
//
// This software is copyrighted by the Regents of the University of
// California, Sun Microsystems, Inc., and other parties.  The following
// terms apply to all files associated with the software unless explicitly
// disclaimed in individual files.
//
// The authors hereby grant permission to use, copy, modify, distribute,
// and license this software and its documentation for any purpose, provided
// that existing copyright notices are retained in all copies and that this
// notice is included verbatim in any distributions. No written agreement,
// license, or royalty fee is required for any of the authorized uses.
// Modifications to this software may be copyrighted by their authors
// and need not follow the licensing terms described here, provided that
// the new terms are clearly indicated on the first page of each file where
// they apply.
//
// IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
// FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
// ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
// DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
// IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
// NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
// MODIFICATIONS.
//
// GOVERNMENT USE: If you are acquiring this software on behalf of the
// U.S. government, the Government shall have only "Restricted Rights"
// in the software and related documentation as defined in the Federal
// Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
// are acquiring the software on behalf of the Department of Defense, the
// software shall be classified as "Commercial Computer Software" and the
// Government shall have only "Restricted Rights" as defined in Clause
// 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
// authors grant the U.S. Government and others acting in its behalf
// permission to use and distribute the software in accordance with the
// terms specified in this license.

static constexpr std::optional<color_tuple>
rgba_from_hsla(float hue,
               float saturation,
               float lightness,
               float alpha) noexcept
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        if (saturation == 0)
                return std::make_tuple(lightness, lightness, lightness, alpha);
#pragma GCC diagnostic pop

        auto const m2 = (lightness <= 0.5f) ? (lightness * (1.0f + saturation)) : (lightness + saturation - lightness * saturation);
        auto const m1 = 2.0f * lightness - m2;

        if (hue < -240 || hue > 360) {
                std::feclearexcept(FE_ALL_EXCEPT);
                hue = fmod(hue, 360);
                if (std::fetestexcept(FE_INVALID))
                        return std::nullopt;
        }

        auto translate = [m1, m2](auto hv) constexpr noexcept -> auto
        {
                while (hv > 360)
                        hv -= 360;
                while (hv < 0)
                        hv += 360;

                if (hv < 60)
                        return m1 + (m2 - m1) * hv / 60;
                else if (hv < 180)
                        return m2;
                else if (hv < 240)
                        return m1 + (m2 - m1) * (240 - hv) / 60;
                else
                        return m1;
        };

        return std::make_tuple(translate(hue + 120),
                               translate(hue),
                               translate(hue - 120),
                               alpha);
}

static int
color_name_index_compare_exact(void const* strp,
                               void const* idxp) noexcept
{
        auto const str = reinterpret_cast<char const*>(strp);
        auto const idx = reinterpret_cast<color::color_name_index const*>(idxp);

        return strcmp(str, color_names_string + idx->offset);
}

static int
color_name_index_compare_inexact(void const* strp,
                                 void const* idxp) noexcept
{
        auto const str = reinterpret_cast<char const*>(strp);
        auto const idx = reinterpret_cast<color::color_name_index const*>(idxp);
        auto const idxstr = color_names_string + idx->offset;

        // This performs an case-insensitive string comparision while
        // skipping spaces. Note that idxstr is already all-lowercase and
        // contains no spaces.
        auto s1 = str, s2 = idxstr;
        while (*s1 && *s2) {
                if (*s1 == ' ') {
                        ++s1;
                        continue;
                }

                auto const c = std::tolower(*s1);
                if (c != *s2)
                        return int(c) - int(*s2);

                ++s1;
                ++s2;
        }

        return int(std::tolower(*s1)) - int(*s2);
}

static std::optional<color_tuple>
parse_named(std::string const& spec,
            bool exact) noexcept
{
        if (auto const idx =
            reinterpret_cast<color::color_name_index*>(bsearch(spec.c_str(),
                                                               color::color_names_indices,
                                                               G_N_ELEMENTS(color::color_names_indices),
                                                               sizeof(color::color_name_index),
                                                               exact
                                                               ? color_name_index_compare_exact
                                                               : color_name_index_compare_inexact))) {
                return from_bits(idx->color, 8, false);
        }

        return std::nullopt;
}

#define SKIP_WHITESPACES(s) while (*(s) == ' ') (s)++;

/* Parses a single color component from a rgb() or rgba() specification
 * according to CSS3 rules. Compared to exact CSS3 parsing we are liberal
 * in what we accept as follows:
 *
 *  - For non-percentage values, we accept floats in the range 0-255
 *    not just [0-9]+ integers
 *  - For percentage values we accept any float, not just [ 0-9]+ | [0-9]* “.” [0-9]+
 *  - We accept mixed percentages and non-percentages in a single
 *    rgb() or rgba() specification.
 */
static bool
parse_rgb_value(char const *str,
                char const **endp,
                float *number)
{
        const char *p;
        char *end;

        *number = g_ascii_strtod (str, &end);
        *endp = end;
        if (errno == ERANGE || *endp == str ||
            std::isinf (*number) || std::isnan (*number))
                return false;

        p = end;

        SKIP_WHITESPACES (p);

        if (*p == '%')
                {
                        *endp = (char const*)(p + 1);
                        *number = std::clamp(*number / 100.0f, 0.0f, 1.0f);
                }
        else
                {
                        *number = std::clamp(*number / 255.0f, 0.0f, 1.0f);
                }

        return true;
}

/*
 * parse:
 * @spec: the string specifying the color
 *
 * Parses a textual representation of a color.
 *
 * The string can be either one of:
 *
 * - A standard name (Taken from the CSS specification).
 * - A hexadecimal value in the form “\#rgb”, “\#rrggbb”,
 *   “\#rrrgggbbb” or ”\#rrrrggggbbbb”
 * - A hexadecimal value in the form “\#rgba”, “\#rrggbbaa”,
 *   or ”\#rrrrggggbbbbaaaa”
 * - A RGB color in the form “rgb(r,g,b)” (In this case the color
 *   will have full opacity)
 * - A rgba color in the form “rgba(r,g,b,a)”
 * - A HSL color in the form "hsl(hue, saturation, lightness)"
 * - A HSLA color in the form "hsla(hue, saturation, lightness, alpha)"
 *
 * Where “r”, “g”, “b” and “a” are respectively the red, green,
 * blue and alpha color values. In the last two cases, “r”, “g”,
 * and “b” are either integers in the range 0 to 255 or percentage
 * values in the range 0% to 100%, and a is a floating point value
 * in the range 0 to 1.
 *
 * Returns: the color, or %nullopt
 */
std::optional<color_tuple>
parse_csslike(std::string const& spec) noexcept
{
        if (spec[0] == '#') {
                auto alpha = false;
                auto bits = 0;

                switch (spec.size()) {
                case 5: alpha = true; [[fallthrough]];
                case 4: bits = 4; break;
                case 9: alpha = true; [[fallthrough]];
                case 7: bits = 8; break;
                default: return std::nullopt;
                }

                auto value = uint64_t{};
                auto const start = spec.c_str() + 1;
                auto const end = spec.c_str() + spec.size();
                auto const rv = std::from_chars(start, end, value, 16);
                if (rv.ec != std::errc{} || rv.ptr != end)
                        return std::nullopt;

                return from_bits(value, bits, alpha);
        }

        auto str = spec.c_str();
        auto has_alpha = false;
        auto is_hsl = false;
        auto r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

        if (strncmp (str, "rgba", 4) == 0) {
                has_alpha = true;
                is_hsl = false;
                str += 4;
        } else if (strncmp (str, "rgb", 3) == 0) {
                has_alpha = false;
                is_hsl = false;
                a = 1;
                str += 3;
        } else if (strncmp (str, "hsla", 4) == 0) {
                has_alpha = true;
                is_hsl = true;
                str += 4;
        } else if (strncmp (str, "hsl", 3) == 0) {
                has_alpha = false;
                is_hsl = true;
                a = 1;
                str += 3;
        } else {
                return parse_named(spec, true);
        }

        SKIP_WHITESPACES (str);

        if (*str != '(')
                return std::nullopt;

        str++;

        /* Parse red */
        SKIP_WHITESPACES (str);
        if (!parse_rgb_value (str, &str, &r))
                return std::nullopt;
        SKIP_WHITESPACES (str);

        if (*str != ',')
                return std::nullopt;

        str++;

        /* Parse green */
        SKIP_WHITESPACES (str);
        if (!parse_rgb_value (str, &str, &g))
                return std::nullopt;
        SKIP_WHITESPACES (str);

        if (*str != ',')
                return std::nullopt;

        str++;

        /* Parse blue */
        SKIP_WHITESPACES (str);
        if (!parse_rgb_value (str, &str, &b))
                return std::nullopt;
        SKIP_WHITESPACES (str);

        if (has_alpha)
                {
                        if (*str != ',')
                                return std::nullopt;

                        str++;

                        SKIP_WHITESPACES (str);
                        char *p;
                        a = g_ascii_strtod (str, &p);
                        if (errno == ERANGE || p == str ||
                            std::isinf (a) || std::isnan (a))
                                return std::nullopt;
                        str = p;
                        SKIP_WHITESPACES (str);
                }

        if (*str != ')')
                return std::nullopt;

        str++;

        SKIP_WHITESPACES (str);

        if (*str != '\0')
                return std::nullopt;

        if (is_hsl)
                return rgba_from_hsla(r * 255.0f,
                                      std::clamp(g, 0.0f, 1.0f),
                                      std::clamp(b, 0.0f, 1.0f),
                                      std::clamp(a, 0.0f, 1.0f));
        else
                return std::make_tuple(std::clamp(r, 0.0f, 1.0f),
                                       std::clamp(g, 0.0f, 1.0f),
                                       std::clamp(b, 0.0f, 1.0f),
                                       std::clamp(a, 0.0f, 1.0f));
}

#undef SKIP_WHITESPACES

std::optional<color_tuple>
parse_x11like(std::string const& spec) noexcept
{
        if (spec[0] == '#') {
                auto bits = 0;
                switch (spec.size()) {
                case 4: bits = 4; break;
                case 7: bits = 8; break;
                case 10: bits = 12; break;
                case 13: bits = 16; break;
                default: return std::nullopt;
                }

                auto value = uint64_t{};
                auto const start = spec.c_str() + 1;
                auto const end = spec.c_str() + spec.size();
                auto const rv = std::from_chars(start, end, value, 16);
                if (rv.ec != std::errc{} || rv.ptr != end)
                        return std::nullopt;

                return from_bits(value, bits, false);
        }

        if (spec.starts_with("rgb:")) {
                auto bits = 0;
                switch (spec.size()) {
                case 9: bits = 4; break;
                case 12: bits = 8; break;
                case 15: bits = 12; break;
                case 18: bits = 16; break;
                default: return std::nullopt;
                }

                auto start = spec.c_str() + 4;
                auto const end = spec.c_str() + spec.size();

                // Note that the length check above makes sure that @r, @g, @b,
                // don't exceed @bits.
                auto r = UINT64_C(0), b = UINT64_C(0), g = UINT64_C(0);
                auto rv = std::from_chars(start, end, r, 16);
                if (rv.ec != std::errc{} || rv.ptr == end || *rv.ptr != '/')
                        return std::nullopt;
                rv = std::from_chars(rv.ptr + 1, end, g, 16);
                if (rv.ec != std::errc{} || rv.ptr == end || *rv.ptr != '/')
                        return std::nullopt;
                rv = std::from_chars(rv.ptr + 1, end, b, 16);
                if (rv.ec != std::errc{} || rv.ptr != end)
                        return std::nullopt;

                return from_bits(r << (2 * bits) |g << bits | b, bits, false);
        }

        // Not going to support these obsolete, rarely-used formats:
        // rgbi:<red>/<green>/<blue>
        // CIEXYZ:<X>/<Y>/<Z>
        // CIEuvY:<u>/<v>/<Y>
        // CIExyY:<x>/<y>/<Y>
        // CIELab:<L>/<a>/<b>
        // CIELuv:<L>/<u>/<v>
        // TekHVC:<H>/<V>/<C>

        return parse_named(spec, false);
}

// END code copied/adapted from gtk+ and pango

// Color to RGB(A) packed (BE)
inline constexpr uint64_t
to_bits(color_tuple const& tuple,
        int bits,
        bool alpha)
{
        auto to_bits = [bits](float v) constexpr noexcept -> uint64_t {
                return uint64_t(v * 65535.) >> (16 - bits);
        };

        auto [r, g, b, a] = tuple;

        auto v = to_bits(r);
        v <<= bits;
        v |= to_bits(g);
        v <<= bits;
        v |= to_bits(b);
        if (alpha) {
                v <<= bits;
                v |= to_bits(a);
        }

        return v;
}

std::string
to_string(color_tuple const& tuple,
          bool alpha,
          color_output_format fmt)
{
        switch (fmt) {
                using enum color_output_format;
        case HEX: {
                char buf[32];
                auto const len = g_snprintf(buf,
                                            sizeof(buf),
                                            alpha ? "#%08X" : "#%06X",
                                            unsigned(impl::to_bits(tuple, 8, alpha)));
                return {buf, size_t(len)};
        }

        default:
                __builtin_unreachable();
                break;
        }
}

} // namespace vte::color::impl
