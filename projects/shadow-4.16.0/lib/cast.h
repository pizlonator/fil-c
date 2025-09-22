// SPDX-FileCopyrightText: 2022-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_CAST_H_
#define SHADOW_INCLUDE_LIB_CAST_H_


#include <config.h>


#define const_cast(T, p)  _Generic(p, const T:  (T) (p))


#endif  // include guard
