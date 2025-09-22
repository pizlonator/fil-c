/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <config.h>

#ident "$Id$"

#include <stddef.h>

#include "memzero.h"


extern inline void memzero(void *ptr, size_t size);
extern inline void strzero(char *s);
