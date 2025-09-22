// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2007-2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * Originally written by Andries Brouwer
 */
#ifndef _UTF8_H
#define _UTF8_H

#include <stdint.h>

int32_t from_utf8(const unsigned char **inptr, long int cnt, int *err);

#define UTF8_BAD (-1)
#define UTF8_SHORT (-2)

#endif /* _UTF8_H */
