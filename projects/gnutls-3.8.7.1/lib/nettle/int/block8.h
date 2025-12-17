/* nettle-types.h

   Copyright (C) 2005, 2014 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see https://www.gnu.org/licenses/.
*/

#ifndef GNUTLS_LIB_NETTLE_BLOCK8_H
#define GNUTLS_LIB_NETTLE_BLOCK8_H

#include "config.h"

#ifndef HAVE_UNION_NETTLE_BLOCK8

/* An aligned 16-byte block. */
union gnutls_nettle_backport_nettle_block16 {
	uint8_t b[16];
	unsigned long w[16 / sizeof(unsigned long)];
	uint64_t u64[2];
};

union gnutls_nettle_backport_nettle_block8 {
	uint8_t b[8];
	uint64_t u64;
};

#undef nettle_block16
#undef nettle_block8

#define nettle_block16 gnutls_nettle_backport_nettle_block16
#define nettle_block8 gnutls_nettle_backport_nettle_block8

#endif

#endif /* GNUTLS_LIB_NETTLE_BLOCK8_H */
