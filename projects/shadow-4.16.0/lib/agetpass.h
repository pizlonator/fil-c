/*
 * SPDX-FileCopyrightText: 2022-2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIB_AGETPASS_H_
#define SHADOW_INCLUDE_LIB_AGETPASS_H_


#include <config.h>

#include "attr.h"
#include "defines.h"


void erase_pass(char *pass);
ATTR_MALLOC(erase_pass)
char *agetpass(const char *prompt);
char *agetpass_stdin();


#endif  // include guard
