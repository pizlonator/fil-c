/*
 * Copyright (C) 2000-2016 Free Software Foundation, Inc.
 * Copyright (C) 2015-2018 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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

#ifndef GNUTLS_LIB_TPM2_H
#define GNUTLS_LIB_TPM2_H

#include "pin.h"

/* Functions used outside tpm2.c */

void _gnutls_tpm2_deinit(void);
int _gnutls_load_tpm2_key(gnutls_privkey_t pkey, const gnutls_datum_t *fdata);

/* Functions only used in tpm2.c */

struct tpm2_info_st;

struct tpm2_info_st *tpm2_info_init(struct pin_info_st *pin);

void tpm2_esys_deinit(void);

void release_tpm2_ctx(struct tpm2_info_st *info);

int install_tpm2_key(struct tpm2_info_st *info, gnutls_privkey_t pkey,
		     unsigned int parent, bool emptyauth,
		     gnutls_datum_t *privdata, gnutls_datum_t *pubdata);

void tpm2_deinit_fn(gnutls_privkey_t key, void *priv);

int tpm2_rsa_sign_hash_fn(gnutls_privkey_t key, gnutls_sign_algorithm_t algo,
			  void *_info, unsigned int flags,
			  const gnutls_datum_t *data, gnutls_datum_t *sig);

int tpm2_ec_sign_hash_fn(gnutls_privkey_t key, gnutls_sign_algorithm_t algo,
			 void *_info, unsigned int flags,
			 const gnutls_datum_t *data, gnutls_datum_t *sig);

uint16_t tpm2_key_curve(struct tpm2_info_st *info);
int tpm2_rsa_key_bits(struct tpm2_info_st *info);

int tpm2_convert_public(gnutls_privkey_t key, void *userdata,
			gnutls_pk_params_st *params);

#endif /* GNUTLS_LIB_TPM2_H */
