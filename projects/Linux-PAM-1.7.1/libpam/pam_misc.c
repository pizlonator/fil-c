/* pam_misc.c -- This is random stuff
 *
 * Copyright (c) Andrew G. Morgan <morgan@kernel.org> 2000-2003
 * All rights reserved.
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

#include "pam_private.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>

#define DELIMITERS " \n\t"

char *_pam_tokenize(char *from, char **next)
/*
 * this function is a variant of the standard strtok_r, it differs in that
 * it uses a fixed set of delimiters and doesn't nul terminate tokens until
 * they are actually reached.
 */
{
     char *end;

     if (from == NULL && (from = *next) == NULL)
	  return from;

     /* look for first non-format char */
     from += strspn(from, DELIMITERS);

     if (*from == '[') {
	 /*
	  * special case, "[...]" is considered to be a single
	  * object.  Note, any '[' inside the outer "[...]" pair will
	  * survive.  Note, the first ']' will terminate this string,
	  * but that "\]" will get compressed into "]". That is:
	  *
	  *   "[..[..\]..]..." --> "..[..].."
	  */
	 char *to;
	 for (to=end=++from; *end && *end != ']'; ++to, ++end) {
	     if (*end == '\\' && end[1] == ']')
		 ++end;
	     if (to != end) {
		 *to = *end;
	     }
	 }
	 if (to != end) {
	     *to = '\0';
	 }
	 /* note, this string is stripped of its edges: "..." is what
            remains */
     } else if (*from) {
	 /* simply look for next blank char */
	 end = from + strcspn(from, DELIMITERS);
     } else {
	 return (*next = NULL);                    /* no tokens left */
     }

     /* now terminate what we have */
     if (*end)
	 *end++ = '\0';

     /* indicate what it left */
     if (*end) {
	 *next = end;
     } else {
	 *next = NULL;                      /* have found last token */
     }

     /* return what we have */
     return from;
}

/*
 * Safe duplication of character strings. "Paranoid"; don't leave
 * evidence of old token around for later stack analysis.
 */

char *_pam_strdup(const char *x)
{
     register char *new=NULL;

     if (x != NULL) {
	  if ((new = strdup(x)) == NULL) {
	       pam_syslog(NULL, LOG_CRIT, "_pam_strdup: failed to get memory");
	  }
	  x = NULL;
     }

     return new;                 /* return the duplicate or NULL on error */
}

/*
 * Safe duplication of memory buffers. "Paranoid"; don't leave
 * evidence of old token around for later stack analysis.
 */

char *_pam_memdup(const char *x, int len)
{
     register char *new=NULL;

     if (x != NULL) {
         if ((new = malloc(len)) == NULL) {
             len = 0;
             pam_syslog(NULL, LOG_CRIT, "_pam_memdup: failed to get memory");
         } else {
             memcpy (new, x, len);
         }
         x = NULL;
     }

     return new;                 /* return the duplicate or NULL on error */
}

/* Generate argv, argc from s */
/* caller must free(argv)     */

size_t _pam_mkargv(const char *s, char ***argv, int *argc)
{
    size_t l;
    size_t argvlen = 0;
    char **our_argv = NULL;

    D(("called: %s",s));

    *argc = 0;

    l = strlen(s);
    if (l && l < SIZE_MAX / (sizeof(char) + sizeof(char *))) {
	char **argvbuf;
	/* Overkill on the malloc, but not large */
	argvlen = (l + 1) * (sizeof(char) + sizeof(char *));
	if ((our_argv = argvbuf = malloc(argvlen)) == NULL) {
	    pam_syslog(NULL, LOG_CRIT, "pam_mkargv: null returned by malloc");
	    argvlen = 0;
	} else {
	    char *argvbufp;
	    char *tmp=NULL;
	    char *tok;
#ifdef PAM_DEBUG
	    unsigned count=0;
#endif
	    argvbufp = (char *) argvbuf + (l * sizeof(char *));
	    strcpy(argvbufp, s);
	    D(("[%s]",argvbufp));
	    while ((tok = _pam_tokenize(argvbufp, &tmp))) {
		D(("arg #%u",++count));
		D(("->[%s]",tok));
		*argvbuf++ = tok;
		if (*argc == INT_MAX) {
		    pam_syslog(NULL, LOG_CRIT,
			       "pam_mkargv: too many arguments");
		    argvlen = 0;
		    _pam_drop(our_argv);
		    break;
		}
		(*argc)++;
		argvbufp = NULL;
		D(("loop again?"));
	    }
	}
    }

    *argv = our_argv;

    D(("exiting"));

    return(argvlen);
}

/*
 * this function is used to protect the modules from accidental or
 * semi-malicious harm that an application may do to confuse the API.
 */

void _pam_sanitize(pam_handle_t *pamh)
{
    int old_caller_is = pamh->caller_is;

    /*
     * this is for security. We reset the auth-tokens here.
     */
    __PAM_TO_MODULE(pamh);
    pam_set_item(pamh, PAM_AUTHTOK, NULL);
    pam_set_item(pamh, PAM_OLDAUTHTOK, NULL);
    pamh->caller_is = old_caller_is;
}

/*
 * This function scans the array and replaces the _PAM_ACTION_UNDEF
 * entries with the default action.
 */

void _pam_set_default_control(int *control_array, int default_action)
{
    int i;

    for (i=0; i<_PAM_RETURN_VALUES; ++i) {
	if (control_array[i] == _PAM_ACTION_UNDEF) {
	    control_array[i] = default_action;
	}
    }
}

/*
 * This function is used to parse a control string.  This string is a
 * series of tokens of the following form:
 *
 *               "[ ]*return_code[ ]*=[ ]*action/[ ]".
 */

#include "pam_tokens.h"

void _pam_parse_control(int *control_array, char *tok)
{
    const char *error;
    int ret;

    while (*tok) {
	size_t len;
	int act;

	/* skip leading space */
	while (isspace((unsigned char)*tok) && *++tok);
	if (!*tok)
	    break;

	/* identify return code */
	for (ret=0; ret<=_PAM_RETURN_VALUES; ++ret) {
	    len = strlen(_pam_token_returns[ret]);
	    if (!strncmp(_pam_token_returns[ret], tok, len)) {
		break;
	    }
	}
	if (ret > _PAM_RETURN_VALUES || !*(tok += len)) {
	    error = "expecting return value";
	    goto parse_error;
	}

	/* observe '=' */
	while (isspace((unsigned char)*tok) && *++tok);
	if (!*tok || *tok++ != '=') {
	    error = "expecting '='";
	    goto parse_error;
	}

	/* skip leading space */
	while (isspace((unsigned char)*tok) && *++tok);
	if (!*tok) {
	    error = "expecting action";
	    goto parse_error;
	}

	/* observe action type */
	for (act=0; act < (-(_PAM_ACTION_UNDEF)); ++act) {
	    len = strlen(_pam_token_actions[act]);
	    if (!strncmp(_pam_token_actions[act], tok, len)) {
		act *= -1;
		tok += len;
		break;
	    }
	}
	if (act > 0) {
	    /*
	     * Either we have a number or we have hit an error.  In
	     * principle, there is nothing to stop us accepting
	     * negative offsets. (Although we would have to think of
	     * another way of encoding the tokens.)  However, I really
	     * think this would be both hard to administer and easily
	     * cause looping problems.  So, for now, we will just
	     * allow forward jumps.  (AGM 1998/1/7)
	     */
	    if (!isdigit((unsigned char)*tok)) {
		error = "expecting jump number";
		goto parse_error;
	    }
	    /* parse a number */
	    act = 0;
	    do {
		int digit = *tok - '0';
		if (act > INT_MAX / 10) {
		    error = "expecting smaller jump number";
		    goto parse_error;
		}
		act *= 10;
		if (act > INT_MAX - digit) {
		    error = "expecting smaller jump number";
		    goto parse_error;
		}
		act += digit;      /* XXX - this assumes ascii behavior */
	    } while (*++tok && isdigit((unsigned char)*tok));
	    if (! act) {
		/* we do not allow 0 jumps.  There is a token ('ignore')
                   for that */
		error = "expecting non-zero";
		goto parse_error;
	    }
	}

	/* set control_array element */
	if (ret != _PAM_RETURN_VALUES) {
	    control_array[ret] = act;
	} else {
	    /* set the default to 'act' */
	    _pam_set_default_control(control_array, act);
	}
    }

    /* that was a success */
    return;

parse_error:
    /* treat everything as bad */
    pam_syslog(NULL, LOG_ERR, "pam_parse: %s; [...%s]", error, tok);
    for (ret=0; ret<_PAM_RETURN_VALUES; control_array[ret++]=_PAM_ACTION_BAD);

}
