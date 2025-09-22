/*
 * SPDX-FileCopyrightText: 2010       , Jakub Hrozek <jhrozek@redhat.com>
 * SPDX-FileCopyrightText: 2011       , Peter Vrabec <pvrabec@redhat.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifdef WITH_SELINUX

#include "defines.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>

#include <selinux/selinux.h>

#include <semanage/semanage.h>

#include "attr.h"
#include "prototypes.h"

#include "shadowlog_internal.h"


format_attr(printf, 3, 4)
static void semanage_error_callback (MAYBE_UNUSED void *varg,
                                     semanage_handle_t *handle,
                                     const char *fmt, ...)
{
	int ret;
	char * message = NULL;
	va_list ap;


	va_start (ap, fmt);
	ret = vasprintf (&message, fmt, ap);
	va_end (ap);
	if (ret < 0) {
		/* ENOMEM */
		return;
	}

	switch (semanage_msg_get_level (handle)) {
	case SEMANAGE_MSG_ERR:
	case SEMANAGE_MSG_WARN:
		fprintf (shadow_logfd, _("[libsemanage]: %s\n"), message);
		break;
	case SEMANAGE_MSG_INFO:
		/* nop */
		break;
	}

	free (message);
}


static semanage_handle_t *semanage_init (void)
{
	int ret;
	semanage_handle_t *handle = NULL;

	handle = semanage_handle_create ();
	if (NULL == handle) {
		fprintf (shadow_logfd,
		         _("Cannot create SELinux management handle\n"));
		return NULL;
	}

	semanage_msg_set_callback (handle, semanage_error_callback, NULL);

	ret = semanage_is_managed (handle);
	if (ret != 1) {
		fprintf (shadow_logfd, _("SELinux policy not managed\n"));
		goto fail;
	}

	ret = semanage_access_check (handle);
	if (ret < SEMANAGE_CAN_READ) {
		fprintf (shadow_logfd, _("Cannot read SELinux policy store\n"));
		goto fail;
	}

	ret = semanage_connect (handle);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Cannot establish SELinux management connection\n"));
		goto fail;
	}

	ret = semanage_begin_transaction (handle);
	if (ret != 0) {
		fprintf (shadow_logfd, _("Cannot begin SELinux transaction\n"));
		goto fail;
	}

	return handle;

fail:
	if (handle)
		semanage_disconnect (handle);
	semanage_handle_destroy (handle);
	return NULL;
}


static int semanage_user_mod (semanage_handle_t *handle,
                              semanage_seuser_key_t *key,
                              const char *login_name,
                              const char *seuser_name,
                              const char *serange)
{
	int ret;
	semanage_seuser_t *seuser = NULL;

	semanage_seuser_query (handle, key, &seuser);
	if (NULL == seuser) {
		fprintf (shadow_logfd,
		         _("Could not query seuser for %s\n"), login_name);
		ret = 1;
		goto done;
	}

	if (serange && semanage_mls_enabled(handle)) {
		ret = semanage_seuser_set_mlsrange (handle, seuser, serange);
		if (ret != 0) {
			fprintf (shadow_logfd,
			         _("Could not set serange for %s to %s\n"),
			         login_name, serange);
			ret = 1;
			goto done;
		}
	}

	ret = semanage_seuser_set_sename (handle, seuser, seuser_name);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Could not set sename for %s\n"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_modify_local (handle, key, seuser);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Could not modify login mapping for %s\n"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = 0;
done:
	semanage_seuser_free (seuser);
	return ret;
}


static int semanage_user_add (semanage_handle_t *handle,
                             const semanage_seuser_key_t *key,
                             const char *login_name,
                             const char *seuser_name,
                             const char *serange)
{
	int ret;
	semanage_seuser_t *seuser = NULL;

	ret = semanage_seuser_create (handle, &seuser);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Cannot create SELinux login mapping for %s\n"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_set_name (handle, seuser, login_name);
	if (ret != 0) {
		fprintf (shadow_logfd, _("Could not set name for %s\n"), login_name);
		ret = 1;
		goto done;
	}

	if (serange && semanage_mls_enabled(handle)) {
		ret = semanage_seuser_set_mlsrange (handle, seuser, serange);
		if (ret != 0) {
			fprintf (shadow_logfd,
			         _("Could not set serange for %s to %s\n"),
			         login_name, serange);
			ret = 1;
			goto done;
		}
	}

	ret = semanage_seuser_set_sename (handle, seuser, seuser_name);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Could not set SELinux user for %s\n"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_modify_local (handle, key, seuser);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Could not add login mapping for %s\n"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = 0;
done:
	semanage_seuser_free (seuser);
	return ret;
}


int set_seuser (const char *login_name, const char *seuser_name, const char *serange)
{
	semanage_handle_t *handle = NULL;
	semanage_seuser_key_t *key = NULL;
	int ret;
	int seuser_exists = 0;

	if (NULL == seuser_name) {
		/* don't care, just let system pick the defaults */
		return 0;
	}

	handle = semanage_init ();
	if (NULL == handle) {
		fprintf (shadow_logfd, _("Cannot init SELinux management\n"));
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_key_create (handle, login_name, &key);
	if (ret != 0) {
		fprintf (shadow_logfd, _("Cannot create SELinux user key\n"));
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_exists (handle, key, &seuser_exists);
	if (ret < 0) {
		fprintf (shadow_logfd, _("Cannot verify the SELinux user\n"));
		ret = 1;
		goto done;
	}

	if (0 != seuser_exists) {
		ret = semanage_user_mod (handle, key, login_name, seuser_name, serange);
		if (ret != 0) {
			fprintf (shadow_logfd,
			         _("Cannot modify SELinux user mapping\n"));
			ret = 1;
			goto done;
		}
	} else {
		ret = semanage_user_add (handle, key, login_name, seuser_name, serange);
		if (ret != 0) {
			fprintf (shadow_logfd,
			         _("Cannot add SELinux user mapping\n"));
			ret = 1;
			goto done;
		}
	}

	ret = semanage_commit (handle);
	if (ret < 0) {
		fprintf (shadow_logfd, _("Cannot commit SELinux transaction\n"));
		ret = 1;
		goto done;
	}

	ret = 0;
	reset_selinux_handle();

done:
	semanage_seuser_key_free (key);
	if (handle)
		semanage_disconnect (handle);
	semanage_handle_destroy (handle);
	return ret;
}


int del_seuser (const char *login_name)
{
	semanage_handle_t *handle = NULL;
	semanage_seuser_key_t *key = NULL;
	int ret;
	int exists = 0;

	handle = semanage_init ();
	if (NULL == handle) {
		fprintf (shadow_logfd, _("Cannot init SELinux management\n"));
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_key_create (handle, login_name, &key);
	if (ret != 0) {
		fprintf (shadow_logfd, _("Cannot create SELinux user key\n"));
		ret = 1;
		goto done;
	}

	ret = semanage_seuser_exists (handle, key, &exists);
	if (ret < 0) {
		fprintf (shadow_logfd, _("Cannot verify the SELinux user\n"));
		ret = 1;
		goto done;
	}

	if (0 == exists) {
		fprintf (shadow_logfd,
		         _("Login mapping for %s is not defined, OK if default mapping was used\n"),
		         login_name);
		ret = 0;  /* probably default mapping */
		goto done;
	}

	ret = semanage_seuser_exists_local (handle, key, &exists);
	if (ret < 0) {
		fprintf (shadow_logfd, _("Cannot verify the SELinux user\n"));
		ret = 1;
		goto done;
	}

	if (0 == exists) {
		fprintf (shadow_logfd,
		         _("Login mapping for %s is defined in policy, cannot be deleted\n"),
		         login_name);
		ret = 0; /* Login mapping defined in policy can't be deleted */
		goto done;
	}

	ret = semanage_seuser_del_local (handle, key);
	if (ret != 0) {
		fprintf (shadow_logfd,
		         _("Could not delete login mapping for %s"),
		         login_name);
		ret = 1;
		goto done;
	}

	ret = semanage_commit (handle);
	if (ret < 0) {
		fprintf (shadow_logfd, _("Cannot commit SELinux transaction\n"));
		ret = 1;
		goto done;
	}

	ret = 0;
done:
	semanage_seuser_key_free (key);
	if (handle)
		semanage_disconnect (handle);
	semanage_handle_destroy (handle);
	return ret;
}
#else				/* !WITH_SELINUX */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !WITH_SELINUX */
