/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pam_private.h"

#include <stdlib.h>
#include <string.h>

static struct pam_data *_pam_locate_data(const pam_handle_t *pamh,
					 const char *name)
{
    struct pam_data *data;

    D(("called"));

    IF_NO_PAMH(pamh, NULL);

    data = pamh->data;

    while (data) {
	if (!strcmp(data->name, name)) {
	    return data;
	}
	data = data->next;
    }

    return NULL;
}

int pam_set_data(
    pam_handle_t *pamh,
    const char *module_data_name,
    void *data,
    void (*cleanup)(pam_handle_t *pamh, void *data, int error_status))
{
    struct pam_data *data_entry;

    D(("called"));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_APP(pamh)) {
	D(("called from application!?"));
	return PAM_SYSTEM_ERR;
    }

    /* module_data_name should not be NULL */
    if (module_data_name == NULL) {
	D(("called with NULL as module_data_name"));
	return PAM_SYSTEM_ERR;
    }

    /* first check if there is some data already. If so clean it up */

    if ((data_entry = _pam_locate_data(pamh, module_data_name))) {
	if (data_entry->cleanup) {
	    data_entry->cleanup(pamh, data_entry->data,
				PAM_DATA_REPLACE | PAM_SUCCESS );
	}
    } else if ((data_entry = malloc(sizeof(*data_entry)))) {
	char *tname;

	if ((tname = _pam_strdup(module_data_name)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT,
		       "pam_set_data: no memory for data name");
	    _pam_drop(data_entry);
	    return PAM_BUF_ERR;
	}
	data_entry->next = pamh->data;
	pamh->data = data_entry;
	data_entry->name = tname;
    } else {
	pam_syslog(pamh, LOG_CRIT,
		   "pam_set_data: cannot allocate data entry");
	return PAM_BUF_ERR;
    }

    data_entry->data = data;           /* note this could be NULL */
    data_entry->cleanup = cleanup;

    return PAM_SUCCESS;
}

int pam_get_data(
    const pam_handle_t *pamh,
    const char *module_data_name,
    const void **datap)
{
    struct pam_data *data;

    D(("called"));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_APP(pamh)) {
	D(("called from application!?"));
	return PAM_SYSTEM_ERR;
    }

    /* module_data_name should not be NULL */
    if (module_data_name == NULL) {
	D(("called with NULL as module_data_name"));
	return PAM_SYSTEM_ERR;
    }

    data = _pam_locate_data(pamh, module_data_name);
    if (data) {
	*datap = data->data;
	return PAM_SUCCESS;
    }

    return PAM_NO_MODULE_DATA;
}

void _pam_free_data(pam_handle_t *pamh, int status)
{
    struct pam_data *last;
    struct pam_data *data;

    D(("called"));

    IF_NO_PAMH(pamh, /* no return value for void fn */);
    data = pamh->data;

    while (data) {
	last = data;
	data = data->next;
	if (last->cleanup) {
	    last->cleanup(pamh, last->data, status);
	}
	_pam_drop(last->name);
	_pam_drop(last);
    }
}
