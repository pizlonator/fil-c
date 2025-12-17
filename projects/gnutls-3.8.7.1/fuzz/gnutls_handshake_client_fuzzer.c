/*
# Copyright 2016 Google Inc.
# Copyright 2017 Red Hat, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
*/

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <gnutls/crypto.h>
#include <gnutls/gnutls.h>
#include "handshake.h"
#include "fuzzer.h"

int __attribute__((visibility("protected")))
gnutls_rnd(gnutls_rnd_level_t level, void *data, size_t len)
{
	memset(data, 0xff, len);

	/* Flip the first byte to avoid infinite loop in the RSA
	 * blinding code of Nettle */
	if (len > 0)
		memset(data, 0x0, 1);
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	int res;
	gnutls_session_t session;
	gnutls_certificate_credentials_t xcred;
	struct mem_st memdata;
	unsigned int retry;

	res = gnutls_init(&session, GNUTLS_CLIENT);
	assert(res >= 0);

	res = gnutls_certificate_allocate_credentials(&xcred);
	assert(res >= 0);
	res = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	assert(res >= 0);

	/*res = gnutls_set_default_priority(session); */
	res = gnutls_priority_set_direct(
		session, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3", NULL);
	assert(res >= 0);

	memdata.data = data;
	memdata.size = size;

	gnutls_transport_set_push_function(session, error_push);
	gnutls_transport_set_pull_function(session, error_pull);
	gnutls_handshake_set_read_function(session, handshake_discard);

	retry = 0;
	do {
		res = gnutls_handshake(session);
		if (res == GNUTLS_E_AGAIN) {
			if (handshake_pull(session, &memdata) < 0) {
				res = GNUTLS_E_INTERNAL_ERROR;
				break;
			}
			if (retry > HANDSHAKE_MAX_RETRY_COUNT) {
				break;
			}
			retry++;
		} else {
			retry = 0;
		}
	} while (res < 0 && gnutls_error_is_fatal(res) == 0);

	gnutls_deinit(session);
	gnutls_certificate_free_credentials(xcred);
	return 0;
}
