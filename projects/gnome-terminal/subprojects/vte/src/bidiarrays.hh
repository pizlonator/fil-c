/*
 * Copyright © 2023 Christian Hergert
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

#define GDK_ARRAY_NAME vte_bidi_chars
#define GDK_ARRAY_TYPE_NAME VteBidiChars
#define GDK_ARRAY_ELEMENT_TYPE gunichar
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 8
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#define GDK_ARRAY_NAME vte_bidi_indexes
#define GDK_ARRAY_TYPE_NAME VteBidiIndexes
#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 8
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"
