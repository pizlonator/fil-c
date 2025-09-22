/*
 * SPDX-FileCopyrightText: 2022-2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIBMISC_SIZEOF_H_
#define SHADOW_INCLUDE_LIBMISC_SIZEOF_H_


#include <config.h>

#include <limits.h>

#include "must_be.h"


#define memberof(T, member)  ((T){}.member)
#define WIDTHOF(x)           (sizeof(x) * CHAR_BIT)
#define SIZEOF_ARRAY(a)      (sizeof(a) + must_be_array(a))
#define NITEMS(a)            (SIZEOF_ARRAY((a)) / sizeof((a)[0]))
#define STRLEN(s)            (NITEMS(s) - 1)


#endif  // include guard
