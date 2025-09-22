/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIB_STRNCPY_H_
#define SHADOW_INCLUDE_LIB_STRNCPY_H_


#include <config.h>

#include <string.h>

#include "sizeof.h"


#define STRNCPY(dst, src)  strncpy(dst, src, NITEMS(dst))


#endif  // include guard
