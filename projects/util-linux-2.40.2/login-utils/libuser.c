/*
 *   libuser.c -- Utilize libuser to set a user attribute
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 */

#include "libuser.h"

#include <grp.h>
#include <libuser/user.h>
#include <unistd.h>

#include "auth.h"
#include "c.h"
#include "nls.h"

static int auth_lu(const char *service_name, struct lu_context *ctx, uid_t uid,
			const char *username);

static int auth_lu(const char *service_name, struct lu_context *ctx, uid_t uid,
			const char *username) {
	if (!lu_uses_elevated_privileges(ctx)) {
		/* Drop privileges */
		if (setegid(getgid()) == -1)
			err(EXIT_FAILURE, _("Couldn't drop group privileges"));
		if (seteuid(getuid()) == -1)
			err(EXIT_FAILURE, _("Couldn't drop group privileges"));
		return TRUE;
	}

	return auth_pam(service_name, uid, username);
}

int set_value_libuser(const char *service_name, const char *username, uid_t uid,
			const char *attr, const char *val) {
	struct lu_context *ctx;
	struct lu_error *error = NULL;
	struct lu_ent *ent;

	ctx = lu_start(username, lu_user, NULL, NULL, lu_prompt_console_quiet,
			NULL, &error);
	if (ctx == NULL)
		errx(EXIT_FAILURE, _("libuser initialization failed: %s."),
			lu_strerror(error));

	if (!auth_lu(service_name, ctx, uid, username)) {
		errno = EACCES;
		err(EXIT_FAILURE, _("changing user attribute failed"));
	}

	/* Look up the user's record. */
	ent = lu_ent_new();
	if (lu_user_lookup_name(ctx, username, ent, &error) == FALSE) {
		lu_end(ctx);
		errx(EXIT_FAILURE, _("user \"%s\" does not exist."), username);
	}

	lu_ent_set_string(ent, attr, val);
	if (!lu_user_modify(ctx, ent, &error)) {
		lu_ent_free(ent);
		lu_end(ctx);
		errx(EXIT_FAILURE, _("user attribute not changed: %s"), lu_strerror(error));
	}
	lu_ent_free(ent);
	lu_end(ctx);

	return 0;
}
