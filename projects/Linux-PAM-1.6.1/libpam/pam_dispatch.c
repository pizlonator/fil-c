/* pam_dispatch.c - handles module function dispatch */

/*
 * Copyright (c) 1998, 2005 Andrew G. Morgan <morgan@kernel.org>
 *
 */

#include "pam_private.h"

#include <stdlib.h>
#include <stdio.h>

/*
 * this is the return code we return when a function pointer is NULL
 * or, the handler structure indicates a broken module config line
 */
#define PAM_MUST_FAIL_CODE        PAM_PERM_DENIED

/* impression codes - this gives some sense to the logical choices */
#define _PAM_UNDEF     0
#define _PAM_POSITIVE (+1)
#define _PAM_NEGATIVE (-1)

/* frozen chain required codes */
#define _PAM_PLEASE_FREEZE  0
#define _PAM_MAY_BE_FROZEN  1
#define _PAM_MUST_BE_FROZEN 2

/*
 * walk a stack of modules.  Interpret the administrator's instructions
 * when combining the return codes of each module.
 */

static int _pam_dispatch_aux(pam_handle_t *pamh, int flags, struct handler *h,
			     _pam_boolean resumed, int use_cached_chain)
{
    int depth, impression, status, skip_depth, prev_level, stack_level;
    struct _pam_substack_state *substates = NULL;

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (h == NULL) {
        const void *service=NULL;

	(void) pam_get_item(pamh, PAM_SERVICE, &service);
	pam_syslog(pamh, LOG_ERR, "no modules loaded for `%s' service",
		   service ? (const char *)service:"<unknown>" );
	service = NULL;
	return PAM_MUST_FAIL_CODE;
    }

    /* if we are recalling this module stack because a former call did
       not complete, we restore the state of play from pamh. */
    if (resumed) {
	skip_depth = pamh->former.depth;
	status = pamh->former.status;
	impression = pamh->former.impression;
	substates = pamh->former.substates;
	/* forget all that */
	pamh->former.impression = _PAM_UNDEF;
	pamh->former.status = PAM_MUST_FAIL_CODE;
	pamh->former.depth = 0;
	pamh->former.substates = NULL;
    } else {
	skip_depth = 0;
	substates = malloc(PAM_SUBSTACK_MAX_LEVEL * sizeof(*substates));
	if (substates == NULL) {
	    pam_syslog(pamh, LOG_CRIT,
		       "_pam_dispatch_aux: no memory for substack states");
	    return PAM_BUF_ERR;
	}
	substates[0].impression = impression = _PAM_UNDEF;
	substates[0].status = status = PAM_MUST_FAIL_CODE;
    }

    prev_level = 0;

    /* Loop through module logic stack */
    for (depth=0 ; h != NULL ; prev_level = stack_level, h = h->next, ++depth) {
	int retval, cached_retval, action;

        stack_level = h->stack_level;

	/* skip leading modules if they have already returned */
	if (depth < skip_depth) {
	    continue;
	}

	/* remember state if we are entering a substack */
	if (prev_level < stack_level) {
	    substates[stack_level].impression = impression;
	    substates[stack_level].status = status;
	}

	/* attempt to call the module */
	if (h->handler_type == PAM_HT_MUST_FAIL) {
	    D(("module poorly listed in PAM config; forcing failure"));
	    retval = PAM_MUST_FAIL_CODE;
	} else if (h->handler_type == PAM_HT_SUBSTACK) {
	    D(("skipping substack handler"));
	    continue;
	} else if (h->func == NULL) {
	    D(("module function is not defined, indicating failure"));
	    retval = PAM_MODULE_UNKNOWN;
	} else {
	    D(("passing control to module..."));
	    pamh->mod_name=h->mod_name;
	    pamh->mod_argc = h->argc;
	    pamh->mod_argv = h->argv;
	    retval = h->func(pamh, flags, h->argc, h->argv);
	    pamh->mod_name=NULL;
	    pamh->mod_argc = 0;
	    pamh->mod_argv = NULL;
	    D(("module returned: %s", pam_strerror(pamh, retval)));
	}

	/*
	 * PAM_INCOMPLETE return is special.  It indicates that the
	 * module wants to wait for the application before continuing.
	 * In order to return this, the module will have saved its
	 * state so it can resume from an equivalent position when it
	 * is called next time.  (This was added as of 0.65)
	 */
	if (retval == PAM_INCOMPLETE) {
	    pamh->former.impression = impression;
	    pamh->former.status = status;
	    pamh->former.depth = depth;
	    pamh->former.substates = substates;

	    D(("module %d returned PAM_INCOMPLETE", depth));
	    return retval;
	}

	/*
	 * use_cached_chain is how we ensure that the setcred and
         * close_session modules are called in the same order as they did
	 * when they were invoked as auth/open_session. This feature was
	 * added in 0.75 to make the behavior of pam_setcred sane.
	 */
	if (use_cached_chain != _PAM_PLEASE_FREEZE) {

	    /* a former stack execution should have frozen the chain */

	    cached_retval = *(h->cached_retval_p);
	    if (cached_retval == _PAM_INVALID_RETVAL) {

		/* This may be a problem condition. It implies that
		   the application is running setcred, close_session,
		   chauthtok(2nd) without having first run
		   authenticate, open_session, chauthtok(1st)
		   [respectively]. */

		D(("use_cached_chain is set to [%d],"
		   " but cached_retval == _PAM_INVALID_RETVAL",
		   use_cached_chain));

		/* In the case of close_session and setcred there is a
		   backward compatibility reason for allowing this, in
		   the chauthtok case we have encountered a bug in
		   libpam! */

		if (use_cached_chain == _PAM_MAY_BE_FROZEN) {
		    /* (not ideal) force non-frozen stack control. */
		    cached_retval = retval;
		} else {
		    D(("BUG in libpam -"
		       " chain is required to be frozen but isn't"));

		    /* cached_retval is already _PAM_INVALID_RETVAL */
		}
	    }
	} else {
	    /* this stack execution is defining the frozen chain */
	    cached_retval = h->cached_retval = retval;
	}

	/* verify that the return value is a valid one */
	if ((cached_retval < PAM_SUCCESS)
	    || (cached_retval >= _PAM_RETURN_VALUES)) {

	    retval = PAM_MUST_FAIL_CODE;
	    action = _PAM_ACTION_BAD;
	} else {
	    /* We treat the current retval with some respect. It may
	       (for example, in the case of setcred) have a value that
	       needs to be propagated to the user.  We want to use the
	       cached_retval to determine the modules to be executed
	       in the stacked chain, but we want to treat each
	       non-ignored module in the cached chain as now being
	       'required'. We only need to treat the,
	       _PAM_ACTION_IGNORE, _PAM_ACTION_IS_JUMP and
	       _PAM_ACTION_RESET actions specially. */

	    action = h->actions[cached_retval];
	}

	D(("use_cached_chain=%d action=%d cached_retval=%d retval=%d",
	   use_cached_chain, action, cached_retval, retval));

	/* decide what to do */
	switch (action) {
	case _PAM_ACTION_RESET:

	    impression = substates[stack_level].impression;
	    status = substates[stack_level].status;
	    break;

	case _PAM_ACTION_OK:
	case _PAM_ACTION_DONE:

	    if ( impression == _PAM_UNDEF
		 || (impression == _PAM_POSITIVE && status == PAM_SUCCESS) ) {
                /* in case of using cached chain
                   we could get here with PAM_IGNORE - don't return it */
                if ( retval != PAM_IGNORE || cached_retval == retval ) {
		    impression = _PAM_POSITIVE;
                    status = retval;
                }
	    }
	    if ( impression == _PAM_POSITIVE ) {
		if ( retval == PAM_SUCCESS ) {
		    h->grantor = 1;
		}

		if ( action == _PAM_ACTION_DONE ) {
		    goto decision_made;
		}
	    }
	    break;

	case _PAM_ACTION_BAD:
	case _PAM_ACTION_DIE:
#ifdef PAM_FAIL_NOW_ON
	    if ( cached_retval == PAM_ABORT ) {
		impression = _PAM_NEGATIVE;
		status = PAM_PERM_DENIED;
		goto decision_made;
	    }
#endif /* PAM_FAIL_NOW_ON */
	    if ( impression != _PAM_NEGATIVE ) {
		impression = _PAM_NEGATIVE;
	        /* Don't return with PAM_IGNORE as status */
		if ( retval == PAM_IGNORE )
		    status = PAM_MUST_FAIL_CODE;
		else
		    status = retval;
	    }
	    if ( action == _PAM_ACTION_DIE ) {
		goto decision_made;
	    }
	    break;

	case _PAM_ACTION_IGNORE:
	    break;

        /* if we get here, we expect action is a positive number --
           this is what the ...JUMP macro checks. */

	default:
	    if ( _PAM_ACTION_IS_JUMP(action) ) {

		/* If we are evaluating a cached chain, we treat this
		   module as required (aka _PAM_ACTION_OK) as well as
		   executing the jump. */

		if (use_cached_chain) {
		    if (impression == _PAM_UNDEF
			|| (impression == _PAM_POSITIVE
			    && status == PAM_SUCCESS) ) {
			if ( retval != PAM_IGNORE || cached_retval == retval ) {
			    if ( impression == _PAM_UNDEF && retval == PAM_SUCCESS ) {
				h->grantor = 1;
			    }
			    impression = _PAM_POSITIVE;
			    status = retval;
			}
		    }
		}

		/* this means that we need to skip #action stacked modules */
		while (h->next != NULL && h->next->stack_level >= stack_level && action > 0) {
		    do {
			h = h->next;
			++depth;
		    } while (h->next != NULL && h->next->stack_level > stack_level);
		    --action;
		}

		/* note if we try to skip too many modules action is
                   still non-zero and we snag the next if. */
	    }

	    /* this case is a syntax error: we can't succeed */
	    if (action) {
		pam_syslog(pamh, LOG_ERR, "bad jump in stack");
		impression = _PAM_NEGATIVE;
		status = PAM_MUST_FAIL_CODE;
	    }
	}
	continue;

decision_made:     /* by getting here we have made a decision */
	while (h->next != NULL && h->next->stack_level >= stack_level) {
	    h = h->next;
	    ++depth;
	}
    }

    /* Sanity check */
    if ( status == PAM_SUCCESS && impression != _PAM_POSITIVE ) {
	D(("caught on sanity check -- this is probably a config error!"));
	status = PAM_MUST_FAIL_CODE;
    }

    free(substates);
    /* We have made a decision about the modules executed */
    return status;
}

static void _pam_clear_grantors(struct handler *h)
{
    for (; h != NULL; h = h->next) {
	h->grantor = 0;
    }
}

/*
 * This function translates the module dispatch request into a pointer
 * to the stack of modules that will actually be run.  the
 * _pam_dispatch_aux() function (above) is responsible for walking the
 * module stack.
 */

int _pam_dispatch(pam_handle_t *pamh, int flags, int choice)
{
    struct handler *h = NULL;
    int retval = PAM_SYSTEM_ERR, use_cached_chain;
    _pam_boolean resumed;

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from a module!?"));
	goto end;
    }

    /* Load all modules, resolve all symbols */

    if ((retval = _pam_init_handlers(pamh)) != PAM_SUCCESS) {
	pam_syslog(pamh, LOG_ERR, "unable to dispatch function");
	goto end;
    }

    use_cached_chain = _PAM_PLEASE_FREEZE;

    switch (choice) {
    case PAM_AUTHENTICATE:
	h = pamh->handlers.conf.authenticate;
	break;
    case PAM_SETCRED:
	h = pamh->handlers.conf.setcred;
	use_cached_chain = _PAM_MAY_BE_FROZEN;
	break;
    case PAM_ACCOUNT:
	h = pamh->handlers.conf.acct_mgmt;
	break;
    case PAM_OPEN_SESSION:
	h = pamh->handlers.conf.open_session;
	break;
    case PAM_CLOSE_SESSION:
	h = pamh->handlers.conf.close_session;
	use_cached_chain = _PAM_MAY_BE_FROZEN;
	break;
    case PAM_CHAUTHTOK:
	h = pamh->handlers.conf.chauthtok;
	break;
    default:
	pam_syslog(pamh, LOG_ERR, "undefined fn choice; %d", choice);
	retval = PAM_ABORT;
	goto end;
    }

    if (h == NULL) {     /* there was no handlers.conf... entry; will use
			  * handlers.other... */
	switch (choice) {
	case PAM_AUTHENTICATE:
	    h = pamh->handlers.other.authenticate;
	    break;
	case PAM_SETCRED:
	    h = pamh->handlers.other.setcred;
	    break;
	case PAM_ACCOUNT:
	    h = pamh->handlers.other.acct_mgmt;
	    break;
	case PAM_OPEN_SESSION:
	    h = pamh->handlers.other.open_session;
	    break;
	case PAM_CLOSE_SESSION:
	    h = pamh->handlers.other.close_session;
	    break;
	case PAM_CHAUTHTOK:
	    h = pamh->handlers.other.chauthtok;
	    break;
	}
    }

    /* Did a module return an "incomplete state" last time? */
    if (pamh->former.choice != PAM_NOT_STACKED) {
	if (pamh->former.choice != choice) {
	    pam_syslog(pamh, LOG_ERR,
			    "application failed to re-exec stack [%d:%d]",
			    pamh->former.choice, choice);
	    retval = PAM_ABORT;
	    goto end;
	}
	resumed = PAM_TRUE;
    } else {
	resumed = PAM_FALSE;
	_pam_clear_grantors(h);
    }

    __PAM_TO_MODULE(pamh);

    /* call the list of module functions */
    pamh->choice = choice;
    retval = _pam_dispatch_aux(pamh, flags, h, resumed, use_cached_chain);

    __PAM_TO_APP(pamh);

    /* Should we recall where to resume next time? */
    if (retval == PAM_INCOMPLETE) {
	D(("module [%s] returned PAM_INCOMPLETE",
	   pamh->mod_name ? pamh->mod_name : "(NULL)"));
	pamh->former.choice = choice;
    } else {
	pamh->former.choice = PAM_NOT_STACKED;
    }

end:

#ifdef HAVE_LIBAUDIT
    if (choice != PAM_CHAUTHTOK || flags & PAM_UPDATE_AUTHTOK || retval != PAM_SUCCESS) {
	retval = _pam_auditlog(pamh, choice, retval, flags, h);
    }
#endif

    return retval;
}
