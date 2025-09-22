/*
 * SPDX-FileCopyrightText: 1999       , Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>
#include <security/pam_appl.h>
#ifdef HAVE_SECURITY_PAM_MISC_H
# include <security/pam_misc.h>
#endif
#ifdef HAVE_SECURITY_OPENPAM_H
# include <security/openpam.h>
#endif


static const struct pam_conv conv = {
	SHADOW_PAM_CONVERSATION,
	NULL
};

/* compatibility with different versions of Linux-PAM */
#if !HAVE_DECL_PAM_ESTABLISH_CRED
#define PAM_ESTABLISH_CRED PAM_CRED_ESTABLISH
#endif
#if !HAVE_DECL_PAM_DELETE_CRED
#define PAM_DELETE_CRED PAM_CRED_DELETE
#endif
#if !HAVE_DECL_PAM_NEW_AUTHTOK_REQD
#define PAM_NEW_AUTHTOK_REQD PAM_AUTHTOKEN_REQD
#endif
#if !HAVE_DECL_PAM_DATA_SILENT
#define PAM_DATA_SILENT 0
#endif
