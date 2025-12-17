/*
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos, Ander Juaristi
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

#ifndef GNUTLS_LIB_TLS13_SESSION_TICKET_H
#define GNUTLS_LIB_TLS13_SESSION_TICKET_H

int _gnutls13_recv_session_ticket(gnutls_session_t session,
				  gnutls_buffer_st *buf);
int _gnutls13_send_session_ticket(gnutls_session_t session, unsigned nr,
				  unsigned again);

static inline bool _gnutls13_can_send_session_ticket(gnutls_session_t session)
{
	return session->key.stek_initialized &&
	       !(session->internals.flags & GNUTLS_NO_TICKETS);
}

int _gnutls13_unpack_session_ticket(gnutls_session_t session,
				    gnutls_datum_t *data,
				    tls13_ticket_st *ticket_data);

inline static void tls13_ticket_deinit(tls13_ticket_st *ticket)
{
	zeroize_temp_key(&ticket->resumption_master_secret,
			 sizeof(ticket->resumption_master_secret));

	_gnutls_free_datum(&ticket->ticket);
	memset(ticket, 0, sizeof(tls13_ticket_st));
}

#endif /* GNUTLS_LIB_TLS13_SESSION_TICKET_H */
