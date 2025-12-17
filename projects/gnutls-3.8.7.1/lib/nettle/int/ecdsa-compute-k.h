/*
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Author: Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#ifndef GNUTLS_LIB_NETTLE_INT_ECDSA_COMPUTE_K_H
#define GNUTLS_LIB_NETTLE_INT_ECDSA_COMPUTE_K_H

#include <gnutls/gnutls.h>
#include <nettle/bignum.h> /* includes gmp.h */

int _gnutls_ecc_curve_to_dsa_q(mpz_t q, gnutls_ecc_curve_t curve);

#endif /* GNUTLS_LIB_NETTLE_INT_ECDSA_COMPUTE_K_H */
