/*
 * pam_listfile module
 *
 * by Elliot Lee <sopwith@redhat.com>, Red Hat Software. July 25, 1996.
 * log refused access error christopher mccrory <chrismcc@netus.com> 1998/7/11
 *
 * This code began life as the pam_rootok module.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#ifdef PAM_DEBUG
#include <assert.h>
#endif

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

/* --- authentication management functions (only) --- */

/* Extended Items that are not directly available via pam_get_item() */
#define EI_GROUP (1 << 0)
#define EI_SHELL (1 << 1)

/* Constants for apply= parameter */
#define APPLY_TYPE_NULL		0
#define APPLY_TYPE_NONE		1
#define APPLY_TYPE_USER		2
#define APPLY_TYPE_GROUP	3

#define LESSER(a, b) ((a) < (b) ? (a) : (b))

static int
pam_listfile(pam_handle_t *pamh, int argc, const char **argv)
{
    int retval = -1;
    int onerr = PAM_SERVICE_ERR;
    int citem = 0;
    int extitem = 0;
    int sense = -1;
    int quiet = 0;
    int i;
    const void *void_citemp;
    const char *citemp;
    const char *ifname=NULL;
    char *aline=NULL;
    const char *apply_val;
    struct stat fileinfo;
    FILE *inf;
    int apply_type;
    size_t n=0;

    /* Stuff for "extended" items */
    struct passwd *userinfo;

    apply_type=APPLY_TYPE_NULL;
    apply_val = "";

    for(i=0; i < argc; i++) {
	const char *str;

	/* option quiet has no value */
	if(!strcmp(argv[i],"quiet")) {
	    quiet = 1;
	    continue;
	}

	if(strchr(argv[i], '=') == NULL) {
	    pam_syslog(pamh,LOG_ERR, "Bad option: \"%s\"", argv[i]);
	    continue;
	}
	if ((str = pam_str_skip_prefix(argv[i], "onerr=")) != NULL) {
	    if(!strcmp(str,"succeed"))
		onerr = PAM_SUCCESS;
	    else if(!strcmp(str,"fail"))
		onerr = PAM_SERVICE_ERR;
	    else {
		pam_syslog(pamh, LOG_ERR, "Unknown option: %s", argv[i]);
		if (retval == -1)
		    retval = PAM_SERVICE_ERR;
		continue;
	    }
	} else if ((str = pam_str_skip_prefix(argv[i], "sense=")) != NULL) {
	    if(!strcmp(str,"allow"))
		sense=0;
	    else if(!strcmp(str,"deny"))
		sense=1;
	    else {
		pam_syslog(pamh, LOG_ERR, "Unknown option: %s", argv[i]);
		if (retval == -1)
		    retval = onerr;
		continue;
	    }
	} else if ((str = pam_str_skip_prefix(argv[i], "file=")) != NULL) {
	    ifname = str;
	} else if ((str = pam_str_skip_prefix(argv[i], "item=")) != NULL) {
	    if(!strcmp(str,"user"))
		citem = PAM_USER;
	    else if(!strcmp(str,"tty"))
		citem = PAM_TTY;
	    else if(!strcmp(str,"rhost"))
		citem = PAM_RHOST;
	    else if(!strcmp(str,"ruser"))
		citem = PAM_RUSER;
	    else { /* These items are related to the user, but are not
		      directly gettable with pam_get_item */
		citem = PAM_USER;
		if(!strcmp(str,"group"))
		    extitem = EI_GROUP;
		else if(!strcmp(str,"shell"))
		    extitem = EI_SHELL;
		else
		    citem = 0;
	    }
	} else if ((str = pam_str_skip_prefix(argv[i], "apply=")) != NULL) {
	    apply_type=APPLY_TYPE_NONE;
	    if (*str=='@') {
		apply_type=APPLY_TYPE_GROUP;
		apply_val = str+1;
	    } else {
		apply_type=APPLY_TYPE_USER;
		apply_val = str;
	    }
	} else {
	    pam_syslog(pamh,LOG_ERR, "Unknown option: %s",argv[i]);
	    if (retval == -1)
		retval = onerr;
	    continue;
	}
    }

    if (!citem) {
	pam_syslog(pamh,LOG_ERR,
		  "Unknown item or item not specified");
	if (retval == -1)
	    retval = onerr;
    }

    if (!ifname) {
	pam_syslog(pamh,LOG_ERR, "List filename not specified");
	if (retval == -1)
	    retval = onerr;
    }

    if (sense == -1) {
	pam_syslog(pamh,LOG_ERR,
		  "Unknown sense or sense not specified");
	if (retval == -1)
	    retval = onerr;
    }

    if ((apply_type == APPLY_TYPE_NONE) ||
	((apply_type != APPLY_TYPE_NULL) && (*apply_val == '\0'))) {
	pam_syslog(pamh,LOG_ERR,
		  "Invalid usage for apply= parameter");
	if (retval == -1)
	    retval = onerr;
    }

    if (retval != -1)
	return retval;

    /* Check if it makes sense to use the apply= parameter */
    if (apply_type != APPLY_TYPE_NULL) {
	if((citem==PAM_USER) || (citem==PAM_RUSER)) {
	    pam_syslog(pamh,LOG_WARNING,
		      "Non-sense use for apply= parameter");
	    apply_type=APPLY_TYPE_NULL;
	}
	if(extitem && (extitem==EI_GROUP)) {
	    pam_syslog(pamh,LOG_WARNING,
		      "Non-sense use for apply= parameter");
	    apply_type=APPLY_TYPE_NULL;
	}
    }

    /* Short-circuit - test if this session applies for this user */
    {
	const char *user_name;
	int rval;

	rval=pam_get_user(pamh,&user_name,NULL);
	if(rval==PAM_SUCCESS && user_name[0]) {
	    /* Got it ? Valid ? */
	    if(apply_type==APPLY_TYPE_USER) {
		if(strcmp(user_name, apply_val)) {
		    /* Does not apply to this user */
#ifdef PAM_DEBUG
		    pam_syslog(pamh,LOG_DEBUG,
			      "don't apply: apply=%s, user=%s",
			     apply_val,user_name);
#endif /* PAM_DEBUG */
		    return PAM_IGNORE;
		}
	    } else if(apply_type==APPLY_TYPE_GROUP) {
		if(!pam_modutil_user_in_group_nam_nam(pamh,user_name,apply_val)) {
		    /* Not a member of apply= group */
#ifdef PAM_DEBUG
		    pam_syslog(pamh,LOG_DEBUG,

			     "don't apply: %s not a member of group %s",
			     user_name,apply_val);
#endif /* PAM_DEBUG */
		    return PAM_IGNORE;
		}
	    }
	}
    }

    retval = pam_get_item(pamh,citem,&void_citemp);
    citemp = void_citemp;
    if(retval != PAM_SUCCESS) {
	return onerr;
    }
    if((citem == PAM_USER) && !citemp) {
	retval = pam_get_user(pamh,&citemp,NULL);
	if (retval != PAM_SUCCESS) {
	    return PAM_SERVICE_ERR;
	}
    }
    if((citem == PAM_TTY) && citemp) {
        /* Normalize the TTY name. */
        const char *str = pam_str_skip_prefix(citemp, "/dev/");
        if (str != NULL)
            citemp = str;
    }

    if(!citemp || (strlen(citemp) == 0)) {
	/* The item was NULL - we are sure not to match */
	return sense?PAM_SUCCESS:PAM_AUTH_ERR;
    }

    if(extitem) {
	switch(extitem) {
	    case EI_GROUP:
		/* Just ignore, call pam_modutil_in_group... later */
		break;
	    case EI_SHELL:
		/* Assume that we have already gotten PAM_USER in
		   pam_get_item() - a valid assumption since citem
		   gets set to PAM_USER in the extitem switch */
		userinfo = pam_modutil_getpwnam(pamh, citemp);
		if (userinfo == NULL) {
		    pam_syslog(pamh, LOG_NOTICE, "getpwnam(%s) failed",
			     citemp);
		    return onerr;
		}
		citemp = userinfo->pw_shell;
		break;
	    default:
		pam_syslog(pamh,LOG_ERR,

			 "Internal weirdness, unknown extended item %d",
			 extitem);
		return onerr;
	}
    }
#ifdef PAM_DEBUG
    pam_syslog(pamh,LOG_INFO,

	     "Got file = %s, item = %d, value = %s, sense = %d",
	     ifname, citem, citemp, sense);
#endif
    if(lstat(ifname,&fileinfo)) {
	if(!quiet)
		pam_syslog(pamh,LOG_ERR, "Couldn't open %s",ifname);
	return onerr;
    }

    if((fileinfo.st_mode & S_IWOTH)
       || !S_ISREG(fileinfo.st_mode)) {
	/* If the file is world writable or is not a
	   normal file, return error */
	pam_syslog(pamh,LOG_ERR,
		 "%s is either world writable or not a normal file",
		 ifname);
	return PAM_AUTH_ERR;
    }

    inf = fopen(ifname,"r");
    if(inf == NULL) { /* Check that we opened it successfully */
	if (onerr == PAM_SERVICE_ERR) {
	    /* Only report if it's an error... */
	    pam_syslog(pamh,LOG_ERR,  "Error opening %s", ifname);
	}
	return onerr;
    }
    /* There should be no more errors from here on */
    retval=PAM_AUTH_ERR;
    /* This loop assumes that PAM_SUCCESS == 0
       and PAM_AUTH_ERR != 0 */
#ifdef PAM_DEBUG
    assert(PAM_SUCCESS == 0);
    assert(PAM_AUTH_ERR != 0);
#endif
    while(retval && getline(&aline,&n,inf) != -1) {
	const char *a = aline;

	aline[strcspn(aline, "\r\n")] = '\0';
	if(aline[0] == '\0')
	    continue;
	if(citem == PAM_TTY) {
	    const char *str = pam_str_skip_prefix(a, "/dev/");
	    if (str != NULL)
		a = str;
	}
	if (extitem == EI_GROUP) {
	    retval = !pam_modutil_user_in_group_nam_nam(pamh,
		citemp, aline);
	} else {
	    retval = strcmp(a, citemp);
	}
    }

    free(aline);
    fclose(inf);
    if ((sense && retval) || (!sense && !retval)) {
#ifdef PAM_DEBUG
	pam_syslog(pamh,LOG_INFO,
		 "Returning PAM_SUCCESS, retval = %d", retval);
#endif
	return PAM_SUCCESS;
    }
    else {
	const void *service;
	const char *user_name;
#ifdef PAM_DEBUG
	pam_syslog(pamh,LOG_INFO,
		 "Returning PAM_AUTH_ERR, retval = %d", retval);
#endif
	(void) pam_get_item(pamh, PAM_SERVICE, &service);
	(void) pam_get_user(pamh, &user_name, NULL);
	if (!quiet)
	    pam_syslog (pamh, LOG_NOTICE, "Refused user %s for service %s",
	                user_name, (const char *)service);
	return PAM_AUTH_ERR;
    }
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
    return pam_listfile(pamh, argc, argv);
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
    return pam_listfile(pamh, argc, argv);
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
    return pam_listfile(pamh, argc, argv);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
    return pam_listfile(pamh, argc, argv);
}

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
    return pam_listfile(pamh, argc, argv);
}
