/*
 * pam_time module
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/6/22
 * (File syntax and much other inspiration from the shadow package
 * shadow-960129)
 * Field parsing rewritten by Tomas Mraz <tm@t8m.info>
 */

#include "config.h"

#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>
#include "pam_inline.h"

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#endif

#define PAM_TIME_CONF	(SCONFIG_DIR "/time.conf")
#ifdef VENDOR_SCONFIG_DIR
#define VENDOR_PAM_TIME_CONF (VENDOR_SCONFIG_DIR "/time.conf")
#endif

#define PAM_TIME_BUFLEN        1000
#define FIELD_SEPARATOR        ';'   /* this is new as of .02 */

#define PAM_DEBUG_ARG       0x0001
#define PAM_NO_AUDIT        0x0002

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

typedef enum { AND, OR } operator;

static int
_pam_parse (const pam_handle_t *pamh, int argc, const char **argv, const char **conffile)
{
    int ctrl = 0;

    *conffile = NULL;
    /* step through arguments */
    for (; argc-- > 0; ++argv) {
	const char *str;

	/* generic options */

	if (!strcmp(*argv, "debug")) {
	    ctrl |= PAM_DEBUG_ARG;
	} else if (!strcmp(*argv, "noaudit")) {
	    ctrl |= PAM_NO_AUDIT;
	} else if ((str = pam_str_skip_prefix(*argv, "conffile=")) != NULL) {
	    if (str[0] == '\0') {
		    pam_syslog(pamh, LOG_ERR,
                       "conffile= specification missing argument - ignored");
            } else {
		  *conffile = str;
		  D(("new Configuration File: %s", *conffile));
	    }
        } else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    if (*conffile == NULL) {
	*conffile = PAM_TIME_CONF;
#ifdef VENDOR_PAM_TIME_CONF
	/*
	 * Check whether PAM_TIME_CONF file is available.
	 * If it does not exist, fall back to VENDOR_PAM_TIME_CONF file.
	 */
	struct stat buffer;
	if (stat(*conffile, &buffer) != 0 && errno == ENOENT) {
	    *conffile = VENDOR_PAM_TIME_CONF;
	}
#endif
    }

    return ctrl;
}

/* --- static functions for checking whether the user should be let in --- */

static char *
shift_buf(char *mem, int from)
{
    char *start = mem;
    while ((*mem = mem[from]) != '\0')
	++mem;
    pam_overwrite_n(mem, PAM_TIME_BUFLEN - (mem - start));
    return mem;
}

static void
trim_spaces(char *buf, char *from)
{
    while (from > buf) {
	--from;
	if (*from == ' ')
	    *from = '\0';
	else
	    break;
    }
}

#define STATE_NL       0 /* new line starting */
#define STATE_COMMENT  1 /* inside comment */
#define STATE_FIELD    2 /* field following */
#define STATE_EOF      3 /* end of file or error */

static int
read_field(const pam_handle_t *pamh, int fd, char **buf, int *from, int *state, const char *file)
{
    char *to;
    char *src;
    int i;
    char c;
    int onspace;

    /* is buf set ? */
    if (! *buf) {
	*buf = calloc(1, PAM_TIME_BUFLEN+1);
	if (! *buf) {
	    pam_syslog(pamh, LOG_CRIT, "out of memory");
	    D(("no memory"));
	    *state = STATE_EOF;
	    return -1;
	}
	*from = 0;
        *state = STATE_NL;
	fd = open(file, O_RDONLY);
	if (fd < 0) {
	    pam_syslog(pamh, LOG_ERR, "error opening %s: %m", file);
	    _pam_drop(*buf);
	    *state = STATE_EOF;
	    return -1;
	}
    }


    if (*from > 0)
	to = shift_buf(*buf, *from);
    else
	to = *buf;

    while (fd != -1 && to - *buf < PAM_TIME_BUFLEN) {
	i = pam_modutil_read(fd, to, PAM_TIME_BUFLEN - (to - *buf));
	if (i < 0) {
	    pam_syslog(pamh, LOG_ERR, "error reading %s: %m", file);
	    close(fd);
	    pam_overwrite_n(*buf, PAM_TIME_BUFLEN);
	    _pam_drop(*buf);
	    *state = STATE_EOF;
	    return -1;
	} else if (!i) {
	    close(fd);
	    fd = -1;          /* end of file reached */
	}

	to += i;
    }

    if (to == *buf) {
	/* nothing previously in buf, nothing read */
	_pam_drop(*buf);
	*state = STATE_EOF;
	return -1;
    }

    pam_overwrite_n(to, PAM_TIME_BUFLEN - (to - *buf));

    to = *buf;
    onspace = 1; /* delete any leading spaces */

    for (src = to; (c=*src) != '\0'; ++src) {
	if (*state == STATE_COMMENT && c != '\n') {
		continue;
	}

	switch (c) {
	    case '\n':
		*state = STATE_NL;
                *to = '\0';
		*from = (src - *buf) + 1;
		trim_spaces(*buf, to);
		return fd;

	    case '\t':
            case ' ':
		if (!onspace) {
		    onspace = 1;
		    *to++ = ' ';
		}
		break;

            case '!':
		onspace = 1; /* ignore following spaces */
		*to++ = '!';
		break;

	    case '#':
		*state = STATE_COMMENT;
		break;

	    case FIELD_SEPARATOR:
		*state = STATE_FIELD;
                *to = '\0';
		*from = (src - *buf) + 1;
		trim_spaces(*buf, to);
		return fd;

	    case '\\':
		if (src[1] == '\n') {
		    ++src; /* skip it */
		    break;
		}
		/* fallthrough */
	    default:
		*to++ = c;
		onspace = 0;
	}
	if (src > to)
	    *src = '\0'; /* clearing */
    }

    if (*state != STATE_COMMENT) {
	*state = STATE_COMMENT;
	pam_syslog(pamh, LOG_ERR, "field too long - ignored");
	**buf = '\0';
    } else {
	*to = '\0';
	trim_spaces(*buf, to);
    }

    *from = 0;
    return fd;
}

/* read a member from a field */

static int
logic_member(const char *string, int *at)
{
     int c,to;
     int done=0;
     int token=0;

     to=*at;
     do {
	  c = string[to++];

	  switch (c) {

	  case '\0':
	       --to;
	       done = 1;
	       break;

	  case '&':
	  case '|':
	  case '!':
	       if (token) {
		    --to;
	       }
	       done = 1;
	       break;

	  default:
	       if (isalpha((unsigned char)c) || c == '*' || isdigit((unsigned char)c) || c == '_'
		    || c == '-' || c == '.' || c == '/' || c == ':') {
		    token = 1;
	       } else if (token) {
		    --to;
		    done = 1;
	       } else {
		    ++*at;
	       }
	  }
     } while (!done);

     return to - *at;
}

typedef enum { VAL, OP } expect;

static int
logic_field(pam_handle_t *pamh, const void *me, const char *x, int rule,
	    int (*agrees)(pam_handle_t *pamh,
			      const void *, const char *, int, int))
{
     int left=FALSE, right, not=FALSE;
     operator oper=OR;
     int at=0, l;
     expect next=VAL;

     while ((l = logic_member(x,&at))) {
	  int c = x[at];

	  if (next == VAL) {
	       if (c == '!')
		    not = !not;
	       else if (isalpha((unsigned char)c) || c == '*' || isdigit((unsigned char)c) || c == '_'
                    || c == '-' || c == '.' || c == '/' || c == ':') {
		    right = not ^ agrees(pamh, me, x+at, l, rule);
		    if (oper == AND)
			 left &= right;
		    else
			 left |= right;
		    next = OP;
	       } else {
		    pam_syslog(pamh, LOG_ERR,
			       "garbled syntax; expected name (rule #%d)",
			       rule);
		    return FALSE;
	       }
	  } else {   /* OP */
	       switch (c) {
	       case '&':
		    oper = AND;
		    break;
	       case '|':
		    oper = OR;
		    break;
	       default:
		    pam_syslog(pamh, LOG_ERR,
			       "garbled syntax; expected & or | (rule #%d)",
			       rule);
		    D(("%c at %d",c,at));
		    return FALSE;
	       }
	       next = VAL;
	       not = FALSE;
	  }
	  at += l;
     }

     return left;
}

static int
is_same(pam_handle_t *pamh UNUSED, const void *A, const char *b,
	int len, int rule UNUSED)
{
     int i;
     const char *a;

     a = A;
     for (i=0; len > 0; ++i, --len) {
	  if (b[i] != a[i]) {
	       if (b[i++] == '*') {
		    return (!--len || !strncmp(b+i,a+strlen(a)-len,len));
	       } else
		    return FALSE;
	  }
     }

     /* Ok, we know that b is a substring from A and does not contain
        wildcards, but now the length of both strings must be the same,
        too. In this case it means, a[i] has to be the end of the string. */
     if (a[i] != '\0')
          return FALSE;

     return ( !len );
}

typedef struct {
     int day;             /* array of 7 bits, one set for today */
     int minute;            /* integer, hour*100+minute for now */
} TIME;

static struct day {
     const char *d;
     int bit;
} const days[11] = {
     { "su", 01 },
     { "mo", 02 },
     { "tu", 04 },
     { "we", 010 },
     { "th", 020 },
     { "fr", 040 },
     { "sa", 0100 },
     { "wk", 076 },
     { "wd", 0101 },
     { "al", 0177 },
     { NULL, 0 }
};

static TIME
time_now(void)
{
     struct tm *local;
     time_t the_time;
     TIME this;

     the_time = time((time_t *)0);                /* get the current time */
     local = localtime(&the_time);
     this.day = days[local->tm_wday].bit;
     this.minute = local->tm_hour*100 + local->tm_min;

     D(("day: 0%o, time: %.4d", this.day, this.minute));
     return this;
}

/* take the current date and see if the range "date" passes it */
static int
check_time(pam_handle_t *pamh, const void *AT, const char *times,
	   int len, int rule)
{
     int not,pass;
     int marked_day, time_start, time_end;
     const TIME *at;
     int i,j=0;

     at = AT;
     D(("checking: 0%o/%.4d vs. %s", at->day, at->minute, times));

     if (times == NULL) {
	  /* this should not happen */
	  pam_syslog(pamh, LOG_CRIT,
		     "internal error in file %s at line %d",
		     __FILE__, __LINE__);
	  return FALSE;
     }

     if (times[j] == '!') {
	  ++j;
	  not = TRUE;
     } else {
	  not = FALSE;
     }

     for (marked_day = 0; len > 0 && isalpha((unsigned char)times[j]); --len) {
	  int this_day=-1;

	  D(("%c%c ?", times[j], times[j+1]));
	  for (i=0; days[i].d != NULL; ++i) {
	       if (tolower((unsigned char)times[j]) == days[i].d[0]
		   && tolower((unsigned char)times[j+1]) == days[i].d[1] ) {
		    this_day = days[i].bit;
		    break;
	       }
	  }
	  j += 2;
	  if (this_day == -1) {
	       pam_syslog(pamh, LOG_ERR, "bad day specified (rule #%d)", rule);
	       return FALSE;
	  }
	  marked_day ^= this_day;
     }
     if (marked_day == 0) {
	  pam_syslog(pamh, LOG_ERR, "no day specified");
	  return FALSE;
     }
     D(("day range = 0%o", marked_day));

     time_start = 0;
     for (i=0; len > 0 && i < 4 && isdigit((unsigned char)times[i+j]); ++i, --len) {
	  time_start *= 10;
	  time_start += times[i+j]-'0';        /* is this portable? */
     }
     j += i;

     if (times[j] == '-') {
	  time_end = 0;
	  for (i=1; len > 0 && i < 5 && isdigit((unsigned char)times[i+j]); ++i, --len) {
	       time_end *= 10;
	       time_end += times[i+j]-'0';    /* is this portable */
	  }
	  j += i;
     } else
	  time_end = -1;

     D(("i=%d, time_end=%d, times[j]='%c'", i, time_end, times[j]));
     if (i != 5 || time_end == -1) {
	  pam_syslog(pamh, LOG_ERR, "no/bad times specified (rule #%d)", rule);
	  return TRUE;
     }
     D(("times(%d to %d)", time_start,time_end));
     D(("marked_day = 0%o", marked_day));

     /* compare with the actual time now */

     pass = FALSE;
     if (time_start < time_end) {    /* start < end ? --> same day */
	  if ((at->day & marked_day) && (at->minute >= time_start)
	      && (at->minute < time_end)) {
	       D(("time is listed"));
	       pass = TRUE;
	  }
     } else {                                    /* spans two days */
	  if ((at->day & marked_day) && (at->minute >= time_start)) {
	       D(("caught on first day"));
	       pass = TRUE;
	  } else {
	       marked_day <<= 1;
	       marked_day |= (marked_day & 0200) ? 1:0;
	       D(("next day = 0%o", marked_day));
	       if ((at->day & marked_day) && (at->minute <= time_end)) {
		    D(("caught on second day"));
		    pass = TRUE;
	       }
	  }
     }

     return (not ^ pass);
}

static int
check_account(pam_handle_t *pamh, const char *service,
	      const char *tty, const char *user, const char *file)
{
     int from=0, state=STATE_NL, fd=-1;
     char *buffer=NULL;
     int count=0;
     TIME here_and_now;
     int retval=PAM_SUCCESS;

     here_and_now = time_now();                     /* find current time */
     do {
	  int good=TRUE,intime;

	  /* here we get the service name field */

	  fd = read_field(pamh, fd, &buffer, &from, &state, file);
	  if (!buffer || !buffer[0]) {
	       /* empty line .. ? */
	       continue;
	  }
	  ++count;

	  if (state != STATE_FIELD) {
	       pam_syslog(pamh, LOG_ERR,
			  "%s: malformed rule #%d", file, count);
	       continue;
	  }

	  good = logic_field(pamh, service, buffer, count, is_same);
	  D(("with service: %s", good ? "passes":"fails" ));

	  /* here we get the terminal name field */

	  fd = read_field(pamh, fd, &buffer, &from, &state, file);
	  if (state != STATE_FIELD) {
	       pam_syslog(pamh, LOG_ERR,
			  "%s: malformed rule #%d", file, count);
	       continue;
	  }
	  good &= logic_field(pamh, tty, buffer, count, is_same);
	  D(("with tty: %s", good ? "passes":"fails" ));

	  /* here we get the username field */

	  fd = read_field(pamh, fd, &buffer, &from, &state, file);
	  if (state != STATE_FIELD) {
	       pam_syslog(pamh, LOG_ERR,
			  "%s: malformed rule #%d", file, count);
	       continue;
	  }
	  /* If buffer starts with @, we are using netgroups */
	  if (buffer[0] == '@')
#ifdef HAVE_INNETGR
	    good &= innetgr (&buffer[1], NULL, user, NULL);
#else
	    pam_syslog (pamh, LOG_ERR, "pam_time does not have netgroup support");
#endif
	  else
	    good &= logic_field(pamh, user, buffer, count, is_same);
	  D(("with user: %s", good ? "passes":"fails" ));

	  /* here we get the time field */

	  fd = read_field(pamh, fd, &buffer, &from, &state, file);
	  if (state == STATE_FIELD) {
	       pam_syslog(pamh, LOG_ERR,
			  "%s: poorly terminated rule #%d", file, count);
	       continue;
	  }

	  intime = logic_field(pamh, &here_and_now, buffer, count, check_time);
	  D(("with time: %s", intime ? "passes":"fails" ));

	  if (good && !intime) {
	       /*
		* for security parse whole file..  also need to ensure
		* that the buffer is free()'d and the file is closed.
		*/
	       retval = PAM_PERM_DENIED;
	  } else {
	       D(("rule passed"));
	  }
     } while (state != STATE_EOF);

     return retval;
}

/* --- public account management functions --- */

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
    const void *service=NULL, *void_tty=NULL;
    const char *tty;
    const char *user=NULL;
    const char *conf_file = NULL;
    int ctrl;
    int rv;

    ctrl = _pam_parse(pamh, argc, argv, &conf_file);

    if (ctrl & PAM_DEBUG_ARG) {
	pam_syslog(pamh, LOG_DEBUG, "conffile=%s", conf_file);
    }

    /* set service name */

    if (pam_get_item(pamh, PAM_SERVICE, &service)
	!= PAM_SUCCESS || service == NULL) {
	pam_syslog(pamh, LOG_ERR, "cannot find the current service name");
	return PAM_ABORT;
    }

    /* set username */

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || *user == '\0') {
	pam_syslog(pamh, LOG_NOTICE, "cannot determine user name");
	return PAM_USER_UNKNOWN;
    }

    /* set tty name */

    if (pam_get_item(pamh, PAM_TTY, &void_tty) != PAM_SUCCESS
	|| void_tty == NULL) {
	D(("PAM_TTY not set, probing stdin"));
	tty = ttyname(STDIN_FILENO);
	if (tty == NULL) {
	    tty = "";
	}
	if (pam_set_item(pamh, PAM_TTY, tty) != PAM_SUCCESS) {
	    pam_syslog(pamh, LOG_ERR, "couldn't set tty name");
	    return PAM_ABORT;
	}
    }
    else
      tty = void_tty;

    if (tty[0] == '/') {   /* full path */
        const char *t;
        tty++;
        if ((t = strchr(tty, '/')) != NULL) {
            tty = t + 1;
        }
    }

    /* good, now we have the service name, the user and the terminal name */

    D(("service=%s", (const char *) service));
    D(("user=%s", user));
    D(("tty=%s", tty));

    rv = check_account(pamh, service, tty, user, conf_file);
    if (rv != PAM_SUCCESS) {
#ifdef HAVE_LIBAUDIT
	if (!(ctrl & PAM_NO_AUDIT)) {
            pam_modutil_audit_write(pamh, AUDIT_ANOM_LOGIN_TIME,
		    "pam_time", rv); /* ignore return value as we fail anyway */
        }
#endif
	if (ctrl & PAM_DEBUG_ARG) {
	    pam_syslog(pamh, LOG_DEBUG, "user %s rejected", user);
	}
    }
    return rv;
}

/* end of module definition */
