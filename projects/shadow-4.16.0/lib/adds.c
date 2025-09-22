// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include "adds.h"

#include <stddef.h>


extern inline long addsl2(long a, long b);
extern inline long addslN(size_t n, long addend[n]);

extern inline int cmpl(const void *p1, const void *p2);
