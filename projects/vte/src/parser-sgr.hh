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

#if !defined(_VTE_SGR) || !defined(_VTE_NGR)
#error "Must define _VTE_SGR and _VTE_NGR before including this file"
#endif

#define SGR(set, name, value) _VTE_SGR(set##_##name, value)
#define NGR(set, name, value) _VTE_NGR(set##_##name, value)

SGR(SET, BOLD, 1)
SGR(SET, DIM, 2)
SGR(SET, ITALIC, 3)
SGR(SET, UNDERLINE, 4)
SGR(SET, BLINK, 5)
SGR(SET, BLINK_RAPID, 6)
SGR(SET, REVERSE, 7)
SGR(SET, INVISIBLE, 8)
SGR(SET, STRIKETHROUGH, 9)
SGR(SET, UNDERLINE_DOUBLE, 21)
SGR(SET, FORE_LEGACY_START, 30)
SGR(SET, FORE_LEGACY_END, 37)
SGR(SET, FORE_SPEC, 38)
SGR(SET, BACK_LEGACY_START, 40)
SGR(SET, BACK_LEGACY_END, 47)
SGR(SET, BACK_SPEC, 48)
SGR(SET, OVERLINE, 53)
SGR(SET, DECO_SPEC, 58)
SGR(SET, FORE_LEGACY_BRIGHT_START, 90)
SGR(SET, FORE_LEGACY_BRIGHT_END, 97)
SGR(SET, BACK_LEGACY_BRIGHT_START, 100)
SGR(SET, BACK_LEGACY_BRIGHT_END, 107)
SGR(RESET, ALL, 0)
SGR(RESET, BOLD_AND_DIM, 22)
SGR(RESET, ITALIC, 23)
SGR(RESET, UNDERLINE, 24)
SGR(RESET, BLINK, 25)
SGR(RESET, REVERSE, 27)
SGR(RESET, INVISIBLE, 28)
SGR(RESET, STRIKETHROUGH, 29)
SGR(RESET, FORE, 39)
SGR(RESET, BACK, 49)
SGR(RESET, OVERLINE, 55)
SGR(RESET, DECO, 59)

NGR(SET, FONT_FIRST, 10)
NGR(SET, FONT_LAST, 19)
NGR(SET, FONT_FRAKTUR, 20)
NGR(SET, PROPORTIONAL, 26)
NGR(SET, FRAMED, 51)
NGR(SET, ENCIRCLED, 52)
NGR(SET, IDEOGRAM_UNDERLINE, 60)
NGR(SET, IDEOGRAM_DOUBLE_UNDERLINE, 61)
NGR(SET, IDEOGRAM_OVERLINE, 62)
NGR(SET, IDEOGRAM_DOUBLE_OVERLINE, 63)
NGR(SET, IDEOGRAM_STRESS_MARK, 64)
NGR(RESET, PROPORTIONAL, 50)
NGR(RESET, FRAMED_OR_ENCIRCLED, 54)
NGR(RESET, IDEOGRAM, 65)

#undef SGR
#undef NGR
