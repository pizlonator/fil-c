/*
 * SPDX-FileCopyrightText:  2022 - 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#include <config.h>

#if !defined(HAVE_STPECPY)

#ident "$Id$"

#include "string/stpecpy.h"


extern inline char *stpecpy(char *dst, char *end, const char *restrict src);


#endif  // !HAVE_STPECPY
