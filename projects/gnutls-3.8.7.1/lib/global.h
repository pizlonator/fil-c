/*
 * Copyright (C) 2000-2012 Free Software Foundation, Inc.
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

#ifndef GNUTLS_LIB_GLOBAL_H
#define GNUTLS_LIB_GLOBAL_H

#include <libtasn1.h>
#include <gnutls/gnutls.h>

int gnutls_is_secure_memory(const void *mem);

extern asn1_node _gnutls_pkix1_asn;
extern asn1_node _gnutls_gnutls_asn;

/* removed const from asn1_node* to
 * prevent warnings, since libtasn1 doesn't
 * use the const keyword in its functions.
 */
#define _gnutls_get_gnutls_asn() ((asn1_node)_gnutls_gnutls_asn)
#define _gnutls_get_pkix() ((asn1_node)_gnutls_pkix1_asn)

extern gnutls_log_func _gnutls_log_func;
extern gnutls_audit_log_func _gnutls_audit_log_func;
extern int _gnutls_log_level;
extern int gnutls_crypto_init(void);
extern void gnutls_crypto_deinit(void);
extern void _gnutls_tpm_global_deinit(void);
extern void _gnutls_nss_keylog_deinit(void);

extern void _gnutls_prepare_to_load_system_priorities(void);
extern void _gnutls_unload_system_priorities(void);
extern bool _gnutls_allowlisting_mode(void);

#endif /* GNUTLS_LIB_GLOBAL_H */
