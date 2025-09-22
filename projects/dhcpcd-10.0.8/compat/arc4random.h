/*
 * Arc4 random number generator for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#ifndef ARC4RANDOM_H
#define ARC4RANDOM_H

#include <stdint.h>

uint32_t arc4random(void);
void arc4random_buf(void *, size_t);

#endif
