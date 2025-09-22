/*
 * SPDX-FileCopyrightText:  2022 - 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#include <config.h>

#ident "$Id$"

#include "bit.h"

#include <limits.h>


extern inline unsigned long bit_ceilul(unsigned long x);
extern inline unsigned long bit_ceil_wrapul(unsigned long x);
extern inline int leading_zerosul(unsigned long x);
