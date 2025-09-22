/*
 * Copyright(c) 2017-2019 Free Software Foundation, Inc.
 *
 * This file is part of libtasn1.
 *
 * Libtasn1 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Libtasn1 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libtasn1.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stddef.h>		/* size_t */
#include <stdint.h>		/* uint8_t */

#if defined __clang__ || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
# pragma GCC diagnostic ignored "-Wunused"
# pragma GCC diagnostic ignored "-Wpedantic"
#endif

#ifdef __cplusplus
extern "C"
#endif
int LLVMFuzzerTestOneInput (const uint8_t * data, size_t size);
