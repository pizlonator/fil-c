#ifndef GNUTLS_LIB_ACCELERATED_KTLS_H
#define GNUTLS_LIB_ACCELERATED_KTLS_H

#include "gnutls_int.h"

void _gnutls_ktls_enable(gnutls_session_t session);

int _gnutls_ktls_set_keys(gnutls_session_t session,
			  gnutls_transport_ktls_enable_flags_t in);

ssize_t _gnutls_ktls_send_file(gnutls_session_t session, int fd, off_t *offset,
			       size_t count);

int _gnutls_ktls_send_handshake_msg(gnutls_session_t session,
				    gnutls_record_encryption_level_t level,
				    gnutls_handshake_description_t htype,
				    const void *data, size_t data_size);

int _gnutls_ktls_send_alert_msg(gnutls_session_t session,
				gnutls_record_encryption_level_t level,
				gnutls_alert_level_t alert_level,
				gnutls_alert_description_t alert_desc);

int _gnutls_ktls_send_control_msg(gnutls_session_t session,
				  unsigned char record_type, const void *data,
				  size_t data_size);
#define _gnutls_ktls_send(x, y, z) \
	_gnutls_ktls_send_control_msg(x, GNUTLS_APPLICATION_DATA, y, z);

int _gnutls_ktls_recv_control_msg(gnutls_session_t session,
				  unsigned char *record_type, void *data,
				  size_t data_size);

int _gnutls_ktls_recv_int(gnutls_session_t session, content_type_t type,
			  void *data, size_t data_size);
#define _gnutls_ktls_recv(x, y, z) \
	_gnutls_ktls_recv_int(x, GNUTLS_APPLICATION_DATA, y, z)

#endif /* GNUTLS_LIB_ACCELERATED_KTLS_H */
