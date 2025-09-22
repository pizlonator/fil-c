// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include "atoi/strtou_noneg.h"

#include <stdint.h>


extern inline uintmax_t strtou_noneg(const char *s, char **restrict endp,
    int base, uintmax_t min, uintmax_t max, int *restrict status);
