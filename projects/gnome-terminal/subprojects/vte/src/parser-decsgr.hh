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

SGR(RESET, ALL, 0)
NGR(SET, SUPERSCRIPT, 4)
NGR(SET, SUBSCRIPT, 5)
SGR(SET, OVERLINE, 6)
NGR(SET, TRANSPARENCY, 8)
NGR(RESET, SUPERSUBSCRIPT, 24)
SGR(RESET, OVERLINE, 26)
NGR(RESET, TRANSPARENCY, 28)

#undef SGR
#undef NGR
