/******************************************************************************
 * A module for Linux-PAM that will set the default security context after login
 * via PAM.
 *
 * Copyright (c) 2003-2008 Red Hat, Inc.
 * Written by Dan Walsh <dwalsh@redhat.com>
 * Additional improvements by Tomas Mraz <tmraz@redhat.com>
 *
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

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"
#include "pam_i18n.h"

#include <selinux/selinux.h>
#include <selinux/get_context_list.h>
#include <selinux/context.h>
#include <selinux/get_default_type.h>

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#include <sys/select.h>
#endif

/* Send audit message */
static void
send_audit_message(const pam_handle_t *pamh, int success, const char *default_context,
		   const char *selected_context)
{
#ifdef HAVE_LIBAUDIT
	char *msg = NULL;
	int audit_fd = audit_open();
	char *default_raw = NULL;
	char *selected_raw = NULL;
	const void *tty = NULL, *rhost = NULL;
	if (audit_fd < 0) {
		if (errno == EINVAL || errno == EPROTONOSUPPORT ||
                                        errno == EAFNOSUPPORT) {
			goto fallback; /* No audit support in kernel */
		}
		pam_syslog(pamh, LOG_ERR, "Error connecting to audit system: %m");
		goto fallback;
	}
	(void)pam_get_item(pamh, PAM_TTY, &tty);
	(void)pam_get_item(pamh, PAM_RHOST, &rhost);
	if (selinux_trans_to_raw_context(default_context, &default_raw) < 0) {
		pam_syslog(pamh, LOG_ERR, "Error translating default context '%s'.", default_context);
		default_raw = NULL;
	}
	if (selinux_trans_to_raw_context(selected_context, &selected_raw) < 0) {
		pam_syslog(pamh, LOG_ERR, "Error translating selected context '%s'.", selected_context);
		selected_raw = NULL;
	}
	if ((msg = pam_asprintf("op=pam_selinux default-context=%s selected-context=%s",
				default_raw ? default_raw : (default_context ? default_context : "?"),
				selected_raw ? selected_raw : (selected_context ? selected_context : "?"))) == NULL) {
		pam_syslog(pamh, LOG_ERR, "Error allocating memory.");
		goto fallback;
	}
	if (audit_log_user_message(audit_fd, AUDIT_USER_ROLE_CHANGE,
				   msg, rhost, NULL, tty, success) <= 0) {
		pam_syslog(pamh, LOG_ERR, "Error sending audit message: %m");
		goto fallback;
	}
	goto cleanup;

      fallback:
#endif /* HAVE_LIBAUDIT */
        pam_syslog(pamh, LOG_NOTICE, "pam: default-context=%s selected-context=%s success %d",
		   default_context ? default_context : "(null)",
		   selected_context ? selected_context : "(null)", success);

#ifdef HAVE_LIBAUDIT
      cleanup:
	free(msg);
	freecon(default_raw);
	freecon(selected_raw);
	if (audit_fd >= 0)
		close(audit_fd);
#endif /* HAVE_LIBAUDIT */
}

static int
send_text (pam_handle_t *pamh, const char *text, int debug)
{
  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "%s", text);
  return pam_info (pamh, "%s", text);
}

/*
 * This function sends a message to the user and gets the response. The caller
 * is responsible for freeing the responses.
 */
static int
query_response (pam_handle_t *pamh, const char *text, const char *def,
		char **response, int debug)
{
  int rc;
  if (def)
    rc = pam_prompt (pamh, PAM_PROMPT_ECHO_ON, response, "%s [%s] ", text, def);
  else
    rc = pam_prompt (pamh, PAM_PROMPT_ECHO_ON, response, "%s ", text);

  if (*response == NULL) {
    rc = PAM_CONV_ERR;
  }

  if (rc != PAM_SUCCESS) {
    pam_syslog(pamh, LOG_WARNING, "No response to query: %s", text);
  } else  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "%s %s", text, *response);
  return rc;
}

static char *
config_context (pam_handle_t *pamh, const char *defaultcon, int use_current_range, int debug)
{
  char *newcon = NULL;
  context_t new_context;
  int mls_enabled = is_selinux_mls_enabled();
  char *response=NULL;
  char *type=NULL;
  char resp_val = 0;

  pam_prompt (pamh, PAM_TEXT_INFO, NULL, _("The default security context is %s."), defaultcon);

  while (1) {
    if (query_response(pamh,
		   _("Would you like to enter a different role or level?"), "n",
		   &response, debug) == PAM_SUCCESS) {
	resp_val = response[0];
	_pam_drop(response);
    } else {
	resp_val = 'N';
    }
    if ((resp_val == 'y') || (resp_val == 'Y'))
      {
        if ((new_context = context_new(defaultcon)) == NULL)
	    goto fail_set;

	/* Allow the user to enter role and level individually */
	if (query_response(pamh, _("role:"), context_role_get(new_context),
		       &response, debug) == PAM_SUCCESS && response[0]) {
	  if (get_default_type(response, &type)) {
	    pam_prompt(pamh, PAM_ERROR_MSG, NULL,
		       _("There is no default type for role %s."), response);
	    _pam_drop(response);
	    continue;
	  } else {
	    if (context_role_set(new_context, response))
	      goto fail_set;
	    if (context_type_set (new_context, type))
	      goto fail_set;
	    _pam_drop(type);
	  }
	}
	_pam_drop(response);

	if (mls_enabled)
	  {
	    if (use_current_range) {
	        char *mycon = NULL;
	        context_t my_context;

		if (getcon(&mycon) != 0)
		    goto fail_set;
		my_context = context_new(mycon);
	        if (my_context == NULL) {
		    freecon(mycon);
		    goto fail_set;
		}
		freecon(mycon);
		if (context_range_set(new_context, context_range_get(my_context))) {
		    context_free(my_context);
		    goto fail_set;
		}
		context_free(my_context);
	    } else if (query_response(pamh, _("level:"), context_range_get(new_context),
			   &response, debug) == PAM_SUCCESS && response[0]) {
		if (context_range_set(new_context, response))
		    goto fail_set;
	    }
	    _pam_drop(response);
	  }

	if (debug)
	  pam_syslog(pamh, LOG_NOTICE, "Selected Security Context %s", context_str(new_context));

        /* Get the string value of the context and see if it is valid. */
        if (!security_check_context(context_str(new_context))) {
	  newcon = strdup(context_str(new_context));
	  if (newcon == NULL)
	    goto fail_set;
	  context_free(new_context);

	  /* we have to check that this user is allowed to go into the
	     range they have specified ... role is tied to an seuser, so that'll
	     be checked at setexeccon time */
	  if (mls_enabled &&
	      selinux_check_access(defaultcon, newcon, "context", "contains", NULL) != 0) {
	    pam_syslog(pamh, LOG_NOTICE, "Security context %s is not allowed for %s", defaultcon, newcon);

	    send_audit_message(pamh, 0, defaultcon, newcon);

	    free(newcon);
	    goto fail_range;
	  }
	  return newcon;
	}
	else {
	  send_audit_message(pamh, 0, defaultcon, context_str(new_context));
	  send_text(pamh,_("This is not a valid security context."),debug);
	}
        context_free(new_context); /* next time around allocates another */
      }
    else
      return strdup(defaultcon);
  } /* end while */

  return NULL;

 fail_set:
  free(type);
  _pam_drop(response);
  context_free (new_context);
  send_audit_message(pamh, 0, defaultcon, NULL);
 fail_range:
  return NULL;
}

static char *
context_from_env (pam_handle_t *pamh, const char *defaultcon, int env_params, int use_current_range, int debug)
{
  char *newcon = NULL;
  context_t new_context;
  context_t my_context = NULL;
  int mls_enabled = is_selinux_mls_enabled();
  const char *env = NULL;
  char *type = NULL;
  int fail = 1;

  if ((new_context = context_new(defaultcon)) == NULL)
    goto fail_set;

  if (env_params && (env = pam_getenv(pamh, "SELINUX_ROLE_REQUESTED")) != NULL && env[0] != '\0') {
    if (debug)
	pam_syslog(pamh, LOG_NOTICE, "Requested role: %s", env);

    if (get_default_type(env, &type)) {
	pam_syslog(pamh, LOG_NOTICE, "No default type for role %s", env);
	goto fail_set;
    } else {
	if (context_role_set(new_context, env))
	    goto fail_set;
	if (context_type_set(new_context, type))
	    goto fail_set;
    }
  }

  if (mls_enabled) {
    if ((env = pam_getenv(pamh, "SELINUX_USE_CURRENT_RANGE")) != NULL && env[0] == '1') {
        if (debug)
	    pam_syslog(pamh, LOG_NOTICE, "SELINUX_USE_CURRENT_RANGE is set");
	use_current_range = 1;
    }

    if (use_current_range) {
        char *mycon = NULL;

	if (getcon(&mycon) != 0)
	    goto fail_set;
        my_context = context_new(mycon);
        if (my_context == NULL) {
            freecon(mycon);
	    goto fail_set;
	}
	freecon(mycon);
	env = context_range_get(my_context);
    } else {
        env = pam_getenv(pamh, "SELINUX_LEVEL_REQUESTED");
    }

    if (env != NULL && env[0] != '\0') {
        if (debug)
	    pam_syslog(pamh, LOG_NOTICE, "Requested level: %s", env);
	if (context_range_set(new_context, env))
	    goto fail_set;
    }
  }

  newcon = strdup(context_str(new_context));
  if (newcon == NULL)
    goto fail_set;

  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Selected Security Context %s", newcon);

  /* Get the string value of the context and see if it is valid. */
  if (security_check_context(newcon)) {
    pam_syslog(pamh, LOG_NOTICE, "Not a valid security context %s", newcon);

    goto fail_set;
  }

  /* we have to check that this user is allowed to go into the
     range they have specified ... role is tied to an seuser, so that'll
     be checked at setexeccon time */
  if (mls_enabled &&
      selinux_check_access(defaultcon, newcon, "context", "contains", NULL) != 0) {
    pam_syslog(pamh, LOG_NOTICE, "Security context %s is not allowed for %s", defaultcon, newcon);

    goto fail_set;
  }

  fail = 0;

 fail_set:
  free(type);
  context_free(my_context);
  context_free(new_context);
  if (fail) {
    send_audit_message(pamh, 0, defaultcon, newcon);
    freecon(newcon);
    newcon = NULL;
  }
  return newcon;
}

#define DATANAME "pam_selinux_context"
typedef struct {
  char *exec_context;
  char *prev_exec_context;
  char *default_user_context;
  char *tty_context;
  char *prev_tty_context;
  char *tty_path;
} module_data_t;

static void
free_module_data(module_data_t *data)
{
  free(data->tty_path);
  freecon(data->prev_tty_context);
  freecon(data->tty_context);
  freecon(data->default_user_context);
  freecon(data->prev_exec_context);
  if (data->exec_context != data->default_user_context)
    freecon(data->exec_context);
  free(data);
}

static void
cleanup(pam_handle_t *pamh UNUSED, void *data, int err UNUSED)
{
  free_module_data(data);
}

static const module_data_t *
get_module_data(const pam_handle_t *pamh)
{
  const void *data;

  return (pam_get_data(pamh, DATANAME, &data) == PAM_SUCCESS) ? data : NULL;
}

static const char *
get_item(const pam_handle_t *pamh, int item_type)
{
  const void *item;

  return (pam_get_item(pamh, item_type, &item) == PAM_SUCCESS) ? item : NULL;
}

static int
set_exec_context(const pam_handle_t *pamh, const char *context)
{
  if (setexeccon(context) == 0)
    return 0;
  pam_syslog(pamh, LOG_ERR, "Setting executable context \"%s\" failed: %m",
	     context ? context : "");
  return -1;
}

static int
set_file_context(const pam_handle_t *pamh, const char *context,
		 const char *file)
{
  if (!file)
    return 0;
  if (setfilecon(file, context) == 0 || errno == ENOENT)
    return 0;
  pam_syslog(pamh, LOG_ERR, "Setting file context \"%s\" failed for %s: %m",
	     context ? context : "", file);
  return -1;
}

static int
compute_exec_context(pam_handle_t *pamh, module_data_t *data,
		     int select_context, int use_current_range,
		     int env_params, int debug)
{
  const char *username;

#ifdef HAVE_GETSEUSER
  const char *service;
#endif
  char *seuser = NULL;
  char *level = NULL;
  char **contextlist = NULL;
  int num_contexts = 0;
  const struct passwd *pwd;

  if (!(username = get_item(pamh, PAM_USER))) {
    pam_syslog(pamh, LOG_ERR, "Cannot obtain the user name");
    return PAM_USER_UNKNOWN;
  }

  if ((pwd = pam_modutil_getpwnam(pamh, username)) != NULL) {
    username = pwd->pw_name;
  } /* ignore error and keep using original username */

  /* compute execute context */
#ifdef HAVE_GETSEUSER
  if (!(service = get_item(pamh, PAM_SERVICE))) {
    pam_syslog(pamh, LOG_ERR, "Cannot obtain the service name");
    return PAM_SESSION_ERR;
  }
  if (getseuser(username, service, &seuser, &level) == 0) {
#else
  if (getseuserbyname(username, &seuser, &level) == 0) {
#endif
    num_contexts = get_ordered_context_list_with_level(seuser, level, NULL,
						       &contextlist);
    if (debug)
      pam_syslog(pamh, LOG_DEBUG, "Username= %s SELinux User= %s Level= %s",
		 username, seuser, level);
    free(level);
  }
  if (num_contexts > 0) {
    free(seuser);
    data->default_user_context = strdup(contextlist[0]);
    freeconary(contextlist);
    if (!data->default_user_context) {
      pam_syslog(pamh, LOG_CRIT, "Out of memory");
      return PAM_BUF_ERR;
    }

    data->exec_context = data->default_user_context;
    if (select_context)
      data->exec_context = config_context(pamh, data->default_user_context,
					  use_current_range, debug);
    else if (env_params || use_current_range)
      data->exec_context = context_from_env(pamh, data->default_user_context,
					    env_params, use_current_range,
					    debug);
  }

  if (!data->exec_context) {
    pam_syslog(pamh, LOG_ERR, "Unable to get valid context for %s", username);
    pam_prompt(pamh, PAM_ERROR_MSG, NULL,
	       _("A valid context for %s could not be obtained."), username);
  }

  if (getexeccon(&data->prev_exec_context) < 0)
    data->prev_exec_context = NULL;

  return PAM_SUCCESS;
}

static int
compute_tty_context(const pam_handle_t *pamh, module_data_t *data)
{
  const char *tty = get_item(pamh, PAM_TTY);
  security_class_t tclass;

  if (!tty || !*tty || !strcmp(tty, "ssh")
      || pam_str_skip_prefix(tty, "NODEV") != NULL) {
    tty = ttyname(STDIN_FILENO);
    if (!tty || !*tty)
      tty = ttyname(STDOUT_FILENO);
    if (!tty || !*tty)
      tty = ttyname(STDERR_FILENO);
    if (!tty || !*tty)
      return PAM_SUCCESS;
  }

  if (pam_str_skip_prefix(tty, "/dev/") == NULL) {
    data->tty_path = pam_asprintf("%s%s", "/dev/", tty);
  } else {
    data->tty_path = strdup(tty);
  }

  if (!data->tty_path) {
    pam_syslog(pamh, LOG_CRIT, "Out of memory");
    return PAM_BUF_ERR;
  }

  if (getfilecon(data->tty_path, &data->prev_tty_context) < 0) {
    data->prev_tty_context = NULL;
    if (errno == ENOENT) {
      free(data->tty_path);
      data->tty_path = NULL;
      return PAM_SUCCESS;
    }
    pam_syslog(pamh, LOG_ERR, "Failed to get current context for %s: %m",
	       data->tty_path);
    return (security_getenforce() != 0) ? PAM_SESSION_ERR : PAM_SUCCESS;
  }

  tclass = string_to_security_class("chr_file");
  if (tclass == 0) {
    pam_syslog(pamh, LOG_ERR, "Failed to get chr_file security class");
    freecon(data->prev_tty_context);
    data->prev_tty_context = NULL;
    free(data->tty_path);
    data->tty_path = NULL;
    return (security_getenforce() != 0) ? PAM_SESSION_ERR : PAM_SUCCESS;
  }

  if (security_compute_relabel(data->exec_context, data->prev_tty_context,
			       tclass, &data->tty_context)) {
    data->tty_context = NULL;
    pam_syslog(pamh, LOG_ERR, "Failed to compute new context for %s: %m",
	       data->tty_path);
    freecon(data->prev_tty_context);
    data->prev_tty_context = NULL;
    free(data->tty_path);
    data->tty_path = NULL;
    return (security_getenforce() != 0) ? PAM_SESSION_ERR : PAM_SUCCESS;
  }

  return PAM_SUCCESS;
}

static int
restore_context(const pam_handle_t *pamh, const module_data_t *data, int debug)
{
  int err;

  if (!data) {
    if (debug)
      pam_syslog(pamh, LOG_NOTICE, "No context to restore");
    return PAM_SUCCESS;
  }

  if (debug && data->tty_path)
    pam_syslog(pamh, LOG_NOTICE,
	       "Restore file context of tty %s: [%s] -> [%s]",
	       data->tty_path,
	       data->tty_context ? data->tty_context : "",
	       data->prev_tty_context ? data->prev_tty_context : "");
  err = set_file_context(pamh, data->prev_tty_context, data->tty_path);

  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Restore executable context: [%s] -> [%s]",
	       data->exec_context,
	       data->prev_exec_context ? data->prev_exec_context : "");
  err |= set_exec_context(pamh, data->prev_exec_context);

  if (err && security_getenforce() != 0)
    return PAM_SESSION_ERR;

  return PAM_SUCCESS;
}

static int
set_context(pam_handle_t *pamh, const module_data_t *data,
	    int debug, int verbose)
{
  int rc, err;

  if (debug && data->tty_path)
    pam_syslog(pamh, LOG_NOTICE, "Set file context of tty %s: [%s] -> [%s]",
	       data->tty_path,
	       data->prev_tty_context ? data->prev_tty_context : "",
	       data->tty_context ? data->tty_context : "");
  err = set_file_context(pamh, data->tty_context, data->tty_path);

  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Set executable context: [%s] -> [%s]",
	       data->prev_exec_context ? data->prev_exec_context : "",
	       data->exec_context);
  rc = set_exec_context(pamh, data->exec_context);
  err |= rc;

  send_audit_message(pamh, !rc, data->default_user_context, data->exec_context);
  if (verbose && !rc) {
    char msg[PATH_MAX];

    pam_sprintf(msg,
	     _("Security context %s has been assigned."), data->exec_context);
    send_text(pamh, msg, debug);
  }
#ifdef HAVE_SETKEYCREATECON
  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Set key creation context to %s",
	       data->exec_context ? data->exec_context : "");
  rc = setkeycreatecon(data->exec_context);
  err |= rc;
  if (rc)
    pam_syslog(pamh, LOG_ERR, "Setting key creation context %s failed: %m",
	       data->exec_context ? data->exec_context : "");
  if (verbose && !rc) {
    char msg[PATH_MAX];

    pam_sprintf(msg,
	     _("Key creation context %s has been assigned."), data->exec_context);
    send_text(pamh, msg, debug);
  }
#endif

  if (err && security_getenforce() != 0)
    return PAM_SESSION_ERR;

  return PAM_SUCCESS;
}

static int
create_context(pam_handle_t *pamh, int argc, const char **argv,
	       int debug, int verbose)
{
  int i;
  int ttys = 1;
  int select_context = 0;
  int use_current_range = 0;
  int env_params = 0;
  module_data_t *data;

  /* Parse arguments. */
  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "nottys") == 0) {
      ttys = 0;
    }
    if (strcmp(argv[i], "select_context") == 0) {
      select_context = 1;
    }
    if (strcmp(argv[i], "use_current_range") == 0) {
      use_current_range = 1;
    }
    if (strcmp(argv[i], "env_params") == 0) {
      env_params = 1;
    }
  }

  if (is_selinux_enabled() <= 0) {
    if (debug)
      pam_syslog(pamh, LOG_NOTICE, "SELinux is not enabled");
    return PAM_SUCCESS;
  }

  if (select_context && env_params) {
    pam_syslog(pamh, LOG_ERR,
	       "select_context cannot be used with env_params");
    select_context = 0;
  }

  if (!(data = calloc(1, sizeof(*data)))) {
    pam_syslog(pamh, LOG_CRIT, "Out of memory");
    return PAM_BUF_ERR;
  }

  i = compute_exec_context(pamh, data, select_context, use_current_range,
			   env_params, debug);
  if (i != PAM_SUCCESS) {
    free_module_data(data);
    return i;
  }

  if (!data->exec_context) {
    free_module_data(data);
    return (security_getenforce() != 0) ? PAM_SESSION_ERR : PAM_SUCCESS;
  }

  if (ttys && (i = compute_tty_context(pamh, data)) != PAM_SUCCESS) {
    free_module_data(data);
    return i;
  }

  if ((i = pam_set_data(pamh, DATANAME, data, cleanup)) != PAM_SUCCESS) {
    pam_syslog(pamh, LOG_ERR, "Error saving context: %m");
    free_module_data(data);
    return i;
  }

  return set_context(pamh, data, debug, verbose);
}

int
pam_sm_authenticate(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
  /* Fail by default. */
  return PAM_AUTH_ERR;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
  return PAM_SUCCESS;
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
  const module_data_t *data;
  int i, debug = 0, verbose = 0, close_session = 0, restore = 0;

  /* Parse arguments. */
  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "debug") == 0) {
      debug = 1;
    }
    if (strcmp(argv[i], "verbose") == 0) {
      verbose = 1;
    }
    if (strcmp(argv[i], "close") == 0) {
      close_session = 1;
    }
    if (strcmp(argv[i], "restore") == 0) {
      restore = 1;
    }
  }

  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Open Session");

  /* Is this module supposed to execute close_session only? */
  if (close_session)
    return PAM_SUCCESS;

  data = get_module_data(pamh);

  /* Is this module supposed only to restore original context? */
  if (restore)
    return restore_context(pamh, data, debug);

  /* If there is a saved context, this module is supposed to set it again. */
  return data ? set_context(pamh, data, debug, verbose) :
    create_context(pamh, argc, argv, debug, verbose);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
  int i, debug = 0, open_session = 0;

  /* Parse arguments. */
  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "debug") == 0) {
      debug = 1;
    }
    if (strcmp(argv[i], "open") == 0) {
      open_session = 1;
    }
  }

  if (debug)
    pam_syslog(pamh, LOG_NOTICE, "Close Session");

  /* Is this module supposed to execute open_session only? */
  if (open_session)
    return PAM_SUCCESS;

  /* Restore original context. */
  return restore_context(pamh, get_module_data(pamh), debug);
}
