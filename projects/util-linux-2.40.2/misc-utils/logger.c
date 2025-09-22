/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixed strerr(errno) in gettext calls
 */

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <sys/uio.h>

#include "all-io.h"
#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "pathnames.h"
#include "strutils.h"
#include "xalloc.h"
#include "strv.h"
#include "list.h"
#include "pwdutils.h"

#define	SYSLOG_NAMES
#include <syslog.h>

#ifdef HAVE_LIBSYSTEMD
# define SD_JOURNAL_SUPPRESS_LOCATION
# include <systemd/sd-daemon.h>
# include <systemd/sd-journal.h>
#endif

#ifdef HAVE_SYS_TIMEX_H
# include <sys/timex.h>
#endif

enum {
	TYPE_UDP = (1 << 1),
	TYPE_TCP = (1 << 2),
	ALL_TYPES = TYPE_UDP | TYPE_TCP
};

enum {
	AF_UNIX_ERRORS_OFF = 0,
	AF_UNIX_ERRORS_ON,
	AF_UNIX_ERRORS_AUTO
};

enum {
	OPT_PRIO_PREFIX = CHAR_MAX + 1,
	OPT_JOURNALD,
	OPT_RFC3164,
	OPT_RFC5424,
	OPT_SOCKET_ERRORS,
	OPT_MSGID,
	OPT_NOACT,
	OPT_ID,
	OPT_STRUCTURED_DATA_ID,
	OPT_STRUCTURED_DATA_PARAM,
	OPT_OCTET_COUNT
};

/* rfc5424 structured data */
struct structured_data {
	char *id;		/* SD-ID */
	char **params;		/* array with SD-PARAMs */

	struct list_head	sds;
};

struct logger_ctl {
	int fd;
	int pri;
	pid_t pid;			/* zero when unwanted */
	char *hdr;			/* the syslog header (based on protocol) */
	char const *tag;
	char *login;
	char *msgid;
	char *unix_socket;		/* -u <path> or default to _PATH_DEVLOG */
	char *server;
	char *port;
	int socket_type;
	size_t max_message_size;
	struct list_head user_sds;	/* user defined rfc5424 structured data */
	struct list_head reserved_sds;	/* standard rfc5424 structured data */

	void (*syslogfp)(struct logger_ctl *ctl);

	unsigned int
			unix_socket_errors:1,	/* whether to report or not errors */
			noact:1,		/* do not write to sockets */
			prio_prefix:1,		/* read priority from input */
			stderr_printout:1,	/* output message to stderr */
			rfc5424_time:1,		/* include time stamp */
			rfc5424_tq:1,		/* include time quality markup */
			rfc5424_host:1,		/* include hostname */
			skip_empty_lines:1,	/* do not send empty lines when processing files */
			octet_count:1;		/* use RFC6587 octet counting */
};

#define is_connected(_ctl)	((_ctl)->fd >= 0)
static void logger_reopen(struct logger_ctl *ctl);

/*
 * For tests we want to be able to control datetime outputs
 */
#ifdef TEST_LOGGER
static inline int logger_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	char *str = getenv("LOGGER_TEST_TIMEOFDAY");
	uintmax_t sec, usec;

	if (str && sscanf(str, "%ju.%ju", &sec, &usec) == 2) {
		tv->tv_sec = sec;
		tv->tv_usec = usec;
		return tv->tv_sec >= 0 && tv->tv_usec >= 0 ? 0 : -EINVAL;
	}

	return gettimeofday(tv, tz);
}

static inline char *logger_xgethostname(void)
{
	char *str = getenv("LOGGER_TEST_HOSTNAME");
	return str ? xstrdup(str) : xgethostname();
}

static inline pid_t logger_getpid(void)
{
	char *str = getenv("LOGGER_TEST_GETPID");
	unsigned int pid;

	if (str && sscanf(str, "%u", &pid) == 1)
		return pid;
	return getpid();
}


#undef HAVE_NTP_GETTIME		/* force to default non-NTP */

#else /* !TEST_LOGGER */
# define logger_gettimeofday(x, y)	gettimeofday(x, y)
# define logger_xgethostname		xgethostname
# define logger_getpid			getpid
#endif


static int decode(const char *name, const CODE *codetab)
{
	register const CODE *c;

	if (name == NULL || *name == '\0')
		return -1;
	if (isdigit(*name)) {
		int num;
		char *end = NULL;

		errno = 0;
		num = strtol(name, &end, 10);
		if (errno || name == end || (end && *end))
			return -1;
		for (c = codetab; c->c_name; c++)
			if (num == c->c_val)
				return num;
		return -1;
	}
	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return -1;
}

static int pencode(char *s)
{
	int facility, level;
	char *separator;

	assert(s);

	separator = strchr(s, '.');
	if (separator) {
		*separator = '\0';
		facility = decode(s, facilitynames);
		if (facility < 0)
			errx(EXIT_FAILURE, _("unknown facility name: %s"), s);
		s = ++separator;
	} else
		facility = LOG_USER;
	level = decode(s, prioritynames);
	if (level < 0)
		errx(EXIT_FAILURE, _("unknown priority name: %s"), s);
	if (facility == LOG_KERN)
		facility = LOG_USER;	/* kern is forbidden */
	return ((level & LOG_PRIMASK) | (facility & LOG_FACMASK));
}

static int unix_socket(struct logger_ctl *ctl, const char *path, int *socket_type)
{
	int fd = -1, i, type = -1;
	static struct sockaddr_un s_addr;	/* AF_UNIX address of local logger */

	if (strlen(path) >= sizeof(s_addr.sun_path))
		errx(EXIT_FAILURE, _("openlog %s: pathname too long"), path);

	s_addr.sun_family = AF_UNIX;
	xstrncpy(s_addr.sun_path, path, sizeof(s_addr.sun_path));

	for (i = 2; i; i--) {
		int st = -1;

		if (i == 2 && *socket_type & TYPE_UDP) {
			st = SOCK_DGRAM;
			type = TYPE_UDP;
		}
		if (i == 1 && *socket_type & TYPE_TCP) {
			st = SOCK_STREAM;
			type = TYPE_TCP;
		}
		if (st == -1 || (fd = socket(AF_UNIX, st, 0)) == -1)
			continue;
		if (connect(fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
			close(fd);
			continue;
		}
		break;
	}

	if (i == 0) {
		if (ctl->unix_socket_errors)
			err(EXIT_FAILURE, _("socket %s"), path);

		/* write_output() will try to reconnect */
		return -1;
	}

	/* replace ALL_TYPES with the real TYPE_* */
	if (type > 0 && type != *socket_type)
		*socket_type = type;
	return fd;
}

static int inet_socket(const char *servername, const char *port, int *socket_type)
{
	int fd, errcode, i, type = -1;
	struct addrinfo hints, *res;
	const char *p = port;

	for (i = 2; i; i--) {
		memset(&hints, 0, sizeof(hints));
		if (i == 2 && *socket_type & TYPE_UDP) {
			hints.ai_socktype = SOCK_DGRAM;
			type = TYPE_UDP;
			if (port == NULL)
				p = "syslog";
		}
		if (i == 1 && *socket_type & TYPE_TCP) {
			hints.ai_socktype = SOCK_STREAM;
			type = TYPE_TCP;
			if (port == NULL)
				p = "syslog-conn";
		}
		if (hints.ai_socktype == 0)
			continue;
		hints.ai_family = AF_UNSPEC;
		errcode = getaddrinfo(servername, p, &hints, &res);
		if (errcode != 0)
			errx(EXIT_FAILURE, _("failed to resolve name %s port %s: %s"),
			     servername, p, gai_strerror(errcode));
		if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
			freeaddrinfo(res);
			continue;
		}
		if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
			freeaddrinfo(res);
			close(fd);
			continue;
		}

		freeaddrinfo(res);
		break;
	}

	if (i == 0)
		errx(EXIT_FAILURE, _("failed to connect to %s port %s"), servername, p);

	/* replace ALL_TYPES with the real TYPE_* */
	if (type > 0 && type != *socket_type)
		*socket_type = type;
	return fd;
}

#ifdef HAVE_LIBSYSTEMD
static int journald_entry(struct logger_ctl *ctl, FILE *fp)
{
	struct iovec *iovec;
	char *buf = NULL;
	ssize_t sz;
	int n, lines = 0, vectors = 8, ret = 0, msgline = -1;
	size_t dummy = 0;

	iovec = xreallocarray(NULL, vectors, sizeof(struct iovec));
	while (1) {
		buf = NULL;
		sz = getline(&buf, &dummy, fp);
		if (sz == -1 ||
		   (sz = rtrim_whitespace((unsigned char *) buf)) == 0) {
			free(buf);
			break;
		}

		if (strncmp(buf, "MESSAGE=", 8) == 0) {
			if (msgline == -1)
				msgline = lines;	/* remember the first message */
			else {
				char *p = xrealloc(iovec[msgline].iov_base,
						   iovec[msgline].iov_len + sz - 8 + 2);

				iovec[msgline].iov_base = p;
				p += iovec[msgline].iov_len;
				*p++ = '\n';
				memcpy(p, buf + 8, sz - 8);
				free(buf);

				iovec[msgline].iov_len += sz - 8 + 1;
				continue;
			}
		}

		if (lines == vectors) {
			vectors *= 2;
			if (IOV_MAX < vectors)
				errx(EXIT_FAILURE, _("maximum input lines (%d) exceeded"), IOV_MAX);
			iovec = xreallocarray(iovec, vectors, sizeof(struct iovec));
		}
		iovec[lines].iov_base = buf;
		iovec[lines].iov_len = sz;
		++lines;
	}

	if (!ctl->noact)
		ret = sd_journal_sendv(iovec, lines);
	if (ctl->stderr_printout) {
		for (n = 0; n < lines; n++)
			fprintf(stderr, "%s\n", (char *) iovec[n].iov_base);
	}
	for (n = 0; n < lines; n++)
		free(iovec[n].iov_base);
	free(iovec);
	return ret;
}
#endif

/* this creates a timestamp based on current time according to the
 * fine rules of RFC3164, most importantly it ensures in a portable
 * way that the month day is correctly written (with a SP instead
 * of a leading 0). The function uses a static buffer which is
 * overwritten on the next call (just like ctime() does).
 */
static char const *rfc3164_current_time(void)
{
	static char time[32];
	struct timeval tv;
	struct tm tm;
	static char const * const monthnames[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
		"Sep", "Oct", "Nov", "Dec"
	};

	logger_gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	snprintf(time, sizeof(time),"%s %2d %2.2d:%2.2d:%2.2d",
		monthnames[tm.tm_mon], tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	return time;
}

#define next_iovec(ary, idx) __extension__ ({		\
		assert(ARRAY_SIZE(ary) > (size_t)idx);	\
		assert(idx >= 0);			\
		&ary[idx++];				\
})

#define iovec_add_string(ary, idx, str, len)		\
	do {						\
		struct iovec *v = next_iovec(ary, idx);	\
		v->iov_base = (void *) str;		\
		v->iov_len = len ? len : strlen(str);	\
	} while (0)

#define iovec_memcmp(ary, idx, str, len)		\
		memcmp((ary)[(idx) - 1].iov_base, str, len)

/* writes generated buffer to desired destination. For TCP syslog,
 * we use RFC6587 octet-stuffing (unless octet-counting is selected).
 * This is not great, but doing full blown RFC5425 (TLS) looks like
 * it is too much for the logger utility. If octet-counting is
 * selected, we use that.
 */
static void write_output(struct logger_ctl *ctl, const char *const msg)
{
	struct iovec iov[4];
	int iovlen = 0;
	char *octet = NULL;

	/* initial connect failed? */
	if (!ctl->noact && !is_connected(ctl))
		logger_reopen(ctl);

	/* 1) octet count */
	if (ctl->octet_count) {
		size_t len = xasprintf(&octet, "%zu ", strlen(ctl->hdr) + strlen(msg));
		iovec_add_string(iov, iovlen, octet, len);
	}

	/* 2) header */
	iovec_add_string(iov, iovlen, ctl->hdr, 0);

	/* 3) message */
	iovec_add_string(iov, iovlen, msg, 0);

	if (!ctl->noact && is_connected(ctl)) {
		struct msghdr message = { 0 };
#ifdef SCM_CREDENTIALS
		struct cmsghdr *cmhp;
		struct ucred *cred;
		union {
			struct cmsghdr cmh;
			char   control[CMSG_SPACE(sizeof(struct ucred))];
		} cbuf = { .control = { 0 } };
#endif

		/* 4) add extra \n to make sure message is terminated */
		if ((ctl->socket_type == TYPE_TCP) && !ctl->octet_count)
			iovec_add_string(iov, iovlen, "\n", 1);

		message.msg_iov = iov;
		message.msg_iovlen = iovlen;

#ifdef SCM_CREDENTIALS
		/* syslog/journald may follow local socket credentials rather
		 * than in the message PID. If we use --id as root than we can
		 * force kernel to accept another valid PID than the real logger(1)
		 * PID.
		 */
		if (ctl->pid && !ctl->server && ctl->pid != getpid()
		    && geteuid() == 0 && kill(ctl->pid, 0) == 0) {

			message.msg_control = cbuf.control;
			message.msg_controllen = CMSG_SPACE(sizeof(struct ucred));

			cmhp = CMSG_FIRSTHDR(&message);
			cmhp->cmsg_len = CMSG_LEN(sizeof(struct ucred));
			cmhp->cmsg_level = SOL_SOCKET;
			cmhp->cmsg_type = SCM_CREDENTIALS;
			cred = (struct ucred *) CMSG_DATA(cmhp);

			cred->pid = ctl->pid;
		}
#endif
		/* Note that logger(1) maybe executed for long time (as pipe
		 * reader) and connection endpoint (syslogd) may be restarted.
		 *
		 * The libc syslog() function reconnects on failed send().
		 * Let's do the same to be robust.    [kzak -- Oct 2017]
		 *
		 * MSG_NOSIGNAL is POSIX.1-2008 compatible, but it for example
		 * not supported by apple-darwin15.6.0.
		 */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif
		if (sendmsg(ctl->fd, &message, MSG_NOSIGNAL) < 0) {
			logger_reopen(ctl);
			if (sendmsg(ctl->fd, &message, MSG_NOSIGNAL) < 0)
				warn(_("send message failed"));
		}
	}

	if (ctl->stderr_printout) {
		/* make sure it's terminated for stderr */
		if (iovec_memcmp(iov, iovlen, "\n", 1) != 0)
			iovec_add_string(iov, iovlen, "\n", 1);

		ignore_result( writev(STDERR_FILENO, iov, iovlen) );
	}

	free(octet);
}

#define NILVALUE "-"
static void syslog_rfc3164_header(struct logger_ctl *const ctl)
{
	char pid[30], *hostname;

	*pid = '\0';
	if (ctl->pid)
		snprintf(pid, sizeof(pid), "[%d]", ctl->pid);

	if ((hostname = logger_xgethostname())) {
		char *dot = strchr(hostname, '.');
		if (dot)
			*dot = '\0';
	} else
		hostname = xstrdup(NILVALUE);

	xasprintf(&ctl->hdr, "<%d>%.15s %s %.200s%s: ",
		 ctl->pri, rfc3164_current_time(), hostname, ctl->tag, pid);

	free(hostname);
}

static inline struct list_head *get_user_structured_data(struct logger_ctl *ctl)
{
	return &ctl->user_sds;
}

static inline struct list_head *get_reserved_structured_data(struct logger_ctl *ctl)
{
	return &ctl->reserved_sds;
}

static int has_structured_data_id(struct list_head *ls, const char *id)
{
	struct list_head *p;

	if (!ls || list_empty(ls))
		return 0;

	list_for_each(p, ls) {
		struct structured_data *sd = list_entry(p, struct structured_data, sds);
		if (sd->id && strcmp(sd->id, id) == 0)
			return 1;
	}

	return 0;
}

static void add_structured_data_id(struct list_head *ls, const char *id)
{
	struct structured_data *sd;

	assert(id);

	if (has_structured_data_id(ls, id))
		errx(EXIT_FAILURE, _("structured data ID '%s' is not unique"), id);

	sd = xcalloc(1, sizeof(*sd));
	INIT_LIST_HEAD(&sd->sds);
	sd->id = xstrdup(id);

	list_add_tail(&sd->sds, ls);
}

static void add_structured_data_param(struct list_head *ls, const char *param)
{
	struct structured_data *sd;

	if (list_empty(ls))
		errx(EXIT_FAILURE, _("--sd-id was not specified for --sd-param %s"), param);

	assert(param);

	sd = list_last_entry(ls, struct structured_data, sds);

	if (strv_extend(&sd->params,  param))
		err_oom();
}

static void __attribute__ ((__format__ (__printf__, 2, 3)))
	add_structured_data_paramf(struct list_head *ls, const char *fmt, ...)
{
	struct structured_data *sd;
	va_list ap;
	int x;

	assert(!list_empty(ls));
	assert(fmt);

	sd = list_last_entry(ls, struct structured_data, sds);
	va_start(ap, fmt);
	x = strv_extendv(&sd->params, fmt, ap);
	va_end(ap);

	if (x)
		err_oom();
}

static char *strdup_structured_data(struct structured_data *sd)
{
	char *res, *tmp;

	if (strv_isempty(sd->params))
		return NULL;

	xasprintf(&res, "[%s %s]", sd->id,
			(tmp = strv_join(sd->params, " ")));
	free(tmp);
	return res;
}

static char *strdup_structured_data_list(struct list_head *ls)
{
	struct list_head *p;
	char *res = NULL;

	list_for_each(p, ls) {
		struct structured_data *sd = list_entry(p, struct structured_data, sds);
		char *one = strdup_structured_data(sd);
		char *tmp = res;

		if (!one)
			continue;
		res = strconcat(tmp, one);
		free(tmp);
		free(one);
	}

	return res;
}

static char *get_structured_data_string(struct logger_ctl *ctl)
{
	char *sys = NULL, *usr = NULL, *res;

	if (!list_empty(&ctl->reserved_sds))
		sys = strdup_structured_data_list(&ctl->reserved_sds);
	if (!list_empty(&ctl->user_sds))
		usr = strdup_structured_data_list(&ctl->user_sds);

	if (sys && usr) {
		res = strconcat(sys, usr);
		free(sys);
		free(usr);
	} else
		res = sys ? sys : usr;

	return res;
}

static int valid_structured_data_param(const char *str)
{
	char *s;
	char *eq  = strchr(str, '='),
	     *qm1 = strchr(str, '"'),
	     *qm2 = qm1 ? ul_strchr_escaped(qm1 + 1, '"') : NULL;

	/* something is missing */
	if (!eq || !qm1 || !qm2)
		return 0;

	/* ']' need to be escaped */
	for (s = qm1 + 1; s && *s; ) {
		char *p = strchr(s, ']');
		if (!p)
			break;
		if (p > qm2 || p == ul_strchr_escaped(s, ']'))
			return 0;
		s = p + 1;
	}

	/* '\' is allowed only before '[]"\' chars */
	for (s = qm1 + 1; s && *s; ) {
		char *p = strchr(s, '\\');
		if (!p)
			break;
		if (!strchr("[]\"\\", *(p + 1)))
			return 0;
		s = p + 1;
		if (*s == '\\')
			s++;
	}

	/* foo="bar" */
	return eq > str && eq < qm1 && eq + 1 == qm1 && qm1 < qm2 && *(qm2 + 1) == '\0';
}

/* SD-ID format:
 *	name@<private enterprise number>, e.g., "ourSDID@32473"
 */
static int valid_structured_data_id(const char *str)
{
	char *at = strchr(str, '@');
	const char *p;

	/* standardized IDs without @<digits> */
	if (!at && (strcmp(str, "timeQuality") == 0 ||
		    strcmp(str, "origin") == 0 ||
		    strcmp(str, "meta") == 0))
		return 1;

	if (!at || at == str || !*(at + 1))
		return 0;

	/* <digits> or <digits>.<digits>[...] */
	for (p = at + 1; p && *p; p++) {
		const char *end;

		if (isdigit_strend(p, &end))
			break;	/* only digits in the string */

		if (end == NULL || end == p ||
		    *end != '.' || *(end + 1) == '\0')
			return 0;
		p = end;
	}

	/* check for forbidden chars in the <name> */
	for (p = str; p < at; p++) {
		if (*p == '[' || *p == '=' || *p == '"' || *p == '@')
			return 0;
		if (isblank((unsigned char) *p) || iscntrl((unsigned char) *p))
			return 0;
	}
	return 1;
}


/* Some field mappings may be controversial, thus I give the reason
 * why this specific mapping was used:
 * APP-NAME <-- tag
 *    Some may argue that "logger" is a better fit, but we think
 *    this is better inline of what other implementations do. In
 *    rsyslog, for example, the TAG value is populated from APP-NAME.
 * PROCID <-- pid
 *    This is a relatively straightforward interpretation from
 *    RFC5424, sect. 6.2.6.
 * MSGID <-- msgid (from --msgid)
 *    One may argue that the string "logger" would be better suited
 *    here so that a receiver can identify the sender process.
 *    However, this does not sound like a good match to RFC5424,
 *    sect. 6.2.7.
 * Note that appendix A.1 of RFC5424 does not provide clear guidance
 * of how these fields should be used. This is the case because the
 * IETF working group couldn't arrive at a clear agreement when we
 * specified RFC5424. The rest of the field mappings should be
 * pretty clear from RFC5424. -- Rainer Gerhards, 2015-03-10
 */
static void syslog_rfc5424_header(struct logger_ctl *const ctl)
{
	char *time;
	char *hostname;
	char const *app_name = ctl->tag;
	char *procid;
	char *const msgid = xstrdup(ctl->msgid ? ctl->msgid : NILVALUE);
	char *structured = NULL;
	struct list_head *sd;

	if (ctl->rfc5424_time) {
		struct timeval tv;
		struct tm tm;

		logger_gettimeofday(&tv, NULL);
		if (localtime_r(&tv.tv_sec, &tm) != NULL) {
			char fmt[64];
			const size_t i = strftime(fmt, sizeof(fmt),
						  "%Y-%m-%dT%H:%M:%S.%%06u%z ", &tm);
			/* patch TZ info to comply with RFC3339 (we left SP at end) */
			fmt[i - 1] = fmt[i - 2];
			fmt[i - 2] = fmt[i - 3];
			fmt[i - 3] = ':';
			xasprintf(&time, fmt, tv.tv_usec);
		} else
			err(EXIT_FAILURE, _("localtime() failed"));
	} else
		time = xstrdup(NILVALUE);

	if (ctl->rfc5424_host) {
		if (!(hostname = logger_xgethostname()))
			hostname = xstrdup(NILVALUE);
		/* Arbitrary looking 'if (var < strlen()) checks originate from
		 * RFC 5424 - 6 Syslog Message Format definition.  */
		if (255 < strlen(hostname))
			errx(EXIT_FAILURE, _("hostname '%s' is too long"),
			     hostname);
	} else
		hostname = xstrdup(NILVALUE);

	if (48 < strlen(ctl->tag))
		errx(EXIT_FAILURE, _("tag '%s' is too long"), ctl->tag);

	if (ctl->pid)
		xasprintf(&procid, "%d", ctl->pid);
	else
		procid = xstrdup(NILVALUE);

	sd = get_reserved_structured_data(ctl);

	/* time quality structured data (maybe overwritten by --sd-id timeQuality) */
	if (ctl->rfc5424_tq && !has_structured_data_id(sd, "timeQuality")) {

		add_structured_data_id(sd, "timeQuality");
		add_structured_data_param(sd, "tzKnown=\"1\"");

#ifdef HAVE_NTP_GETTIME
		struct ntptimeval ntptv;

		if (ntp_gettime(&ntptv) == TIME_OK) {
			add_structured_data_param(sd, "isSynced=\"1\"");
			add_structured_data_paramf(sd, "syncAccuracy=\"%ld\"", ntptv.maxerror);
		} else
#endif
			add_structured_data_paramf(sd, "isSynced=\"0\"");
	}

	/* convert all structured data to string */
	structured = get_structured_data_string(ctl);
	if (!structured)
		structured = xstrdup(NILVALUE);

	xasprintf(&ctl->hdr, "<%d>1 %s %s %s %s %s %s ",
		ctl->pri,
		time,
		hostname,
		app_name,
		procid,
		msgid,
		structured);

	free(time);
	free(hostname);
	/* app_name points to ctl->tag, do NOT free! */
	free(procid);
	free(msgid);
	free(structured);
}

static void parse_rfc5424_flags(struct logger_ctl *ctl, char *s)
{
	char *in, *tok;

	in = s;
	while ((tok = strtok(in, ","))) {
		in = NULL;
		if (!strcmp(tok, "notime")) {
			ctl->rfc5424_time = 0;
			ctl->rfc5424_tq = 0;
		} else if (!strcmp(tok, "notq"))
			ctl->rfc5424_tq = 0;
		else if (!strcmp(tok, "nohost"))
			ctl->rfc5424_host = 0;
		else
			warnx(_("ignoring unknown option argument: %s"), tok);
	}
}

static int parse_unix_socket_errors_flags(char *s)
{
	if (!strcmp(s, "off"))
		return AF_UNIX_ERRORS_OFF;
	if (!strcmp(s, "on"))
		return AF_UNIX_ERRORS_ON;
	if (!strcmp(s, "auto"))
		return AF_UNIX_ERRORS_AUTO;
	warnx(_("invalid argument: %s: using automatic errors"), s);
	return AF_UNIX_ERRORS_AUTO;
}

static void syslog_local_header(struct logger_ctl *const ctl)
{
	char pid[32];

	if (ctl->pid)
		snprintf(pid, sizeof(pid), "[%d]", ctl->pid);
	else
		pid[0] = '\0';

	xasprintf(&ctl->hdr, "<%d>%s %s%s: ", ctl->pri, rfc3164_current_time(),
		ctl->tag, pid);
}

static void generate_syslog_header(struct logger_ctl *const ctl)
{
	free(ctl->hdr);
	ctl->hdr = NULL;
	ctl->syslogfp(ctl);
}

/* just open, nothing else */
static void __logger_open(struct logger_ctl *ctl)
{
	if (ctl->server) {
		ctl->fd = inet_socket(ctl->server, ctl->port, &ctl->socket_type);
	} else {
		if (!ctl->unix_socket)
			ctl->unix_socket = _PATH_DEVLOG;

		ctl->fd = unix_socket(ctl, ctl->unix_socket, &ctl->socket_type);
	}
}

/* open and initialize relevant @ctl tuff */
static void logger_open(struct logger_ctl *ctl)
{
	__logger_open(ctl);

	if (!ctl->syslogfp)
		ctl->syslogfp = ctl->server ? syslog_rfc5424_header :
					      syslog_local_header;
	if (!ctl->tag)
		ctl->tag = ctl->login = xgetlogin();
	if (!ctl->tag)
		ctl->tag = "<someone>";
}

/* re-open; usually after failed connection */
static void logger_reopen(struct logger_ctl *ctl)
{
	if (ctl->fd != -1)
		close(ctl->fd);
	ctl->fd = -1;

	__logger_open(ctl);
}

static void logger_command_line(struct logger_ctl *ctl, char **argv)
{
	/* note: we never re-generate the syslog header here, even if we
	 * generate multiple messages. If so, we think it is the right thing
	 * to do to report them with the same timestamp, as the user actually
	 * intended to send a single message.
	 */
	char *const buf = xmalloc(ctl->max_message_size + 1);
	char *p = buf;
	const char *endp = buf + ctl->max_message_size - 1;
	size_t len;

	while (*argv) {
		len = strlen(*argv);
		if (endp < p + len && p != buf) {
			write_output(ctl, buf);
			p = buf;
		}
		if (ctl->max_message_size < len) {
			(*argv)[ctl->max_message_size] = '\0'; /* truncate */
			write_output(ctl, *argv++);
			continue;
		}
		if (p != buf)
			*p++ = ' ';
		memmove(p, *argv++, len);
		*(p += len) = '\0';
	}
	if (p != buf)
		write_output(ctl, buf);
	free(buf);
}

static void logger_stdin(struct logger_ctl *ctl)
{
	/* note: we re-generate the syslog header for each log message to
	 * update header timestamps and to reflect possible priority changes.
	 */
	int default_priority = ctl->pri;
	char *buf = xmalloc(ctl->max_message_size + 2 + 2);
	int pri;
	int c;
	size_t i;

	c = getchar();
	while (c != EOF) {
		i = 0;
		if (ctl->prio_prefix && c == '<') {
			pri = 0;
			buf[i++] = c;
			while (isdigit(c = getchar()) && pri <= 191) {
				buf[i++] = c;
				pri = pri * 10 + c - '0';
			}
			if (c != EOF && c != '\n')
				buf[i++] = c;
			if (c == '>' && 0 <= pri && pri <= 191) {
				/* valid RFC PRI values */
				i = 0;
				if ((pri & LOG_FACMASK) == 0)
					pri |= (default_priority & LOG_FACMASK);
				ctl->pri = pri;
			} else
				ctl->pri = default_priority;

			if (c != EOF && c != '\n')
				c = getchar();
		}

		while (c != EOF && c != '\n' && i < ctl->max_message_size) {
			buf[i++] = c;
			c = getchar();
		}
		buf[i] = '\0';

		if (i > 0 || !ctl->skip_empty_lines) {
			generate_syslog_header(ctl);
			write_output(ctl, buf);
		}

		if (c == '\n')	/* discard line terminator */
			c = getchar();
	}

	free(buf);
}

static void logger_close(const struct logger_ctl *ctl)
{
	if (ctl->fd != -1 && close(ctl->fd) != 0)
		err(EXIT_FAILURE, _("close failed"));
	free(ctl->hdr);
	free(ctl->login);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [<message>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Enter messages into the system log.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -i                       log the logger command's PID\n"), out);
	fputs(_("     --id[=<id>]          log the given <id>, or otherwise the PID\n"), out);
	fputs(_(" -f, --file <file>        log the contents of this file\n"), out);
	fputs(_(" -e, --skip-empty         do not log empty lines when processing files\n"), out);
	fputs(_("     --no-act             do everything except the write the log\n"), out);
	fputs(_(" -p, --priority <prio>    mark given message with this priority\n"), out);
	fputs(_("     --octet-count        use rfc6587 octet counting\n"), out);
	fputs(_("     --prio-prefix        look for a prefix on every line read from stdin\n"), out);
	fputs(_(" -s, --stderr             output message to standard error as well\n"), out);
	fputs(_(" -S, --size <size>        maximum size for a single message\n"), out);
	fputs(_(" -t, --tag <tag>          mark every line with this tag\n"), out);
	fputs(_(" -n, --server <name>      write to this remote syslog server\n"), out);
	fputs(_(" -P, --port <port>        use this port for UDP or TCP connection\n"), out);
	fputs(_(" -T, --tcp                use TCP only\n"), out);
	fputs(_(" -d, --udp                use UDP only\n"), out);
	fputs(_("     --rfc3164            use the obsolete BSD syslog protocol\n"), out);
	fputs(_("     --rfc5424[=<snip>]   use the syslog protocol (the default for remote);\n"
		"                            <snip> can be notime, or notq, and/or nohost\n"), out);
	fputs(_("     --sd-id <id>         rfc5424 structured data ID\n"), out);
	fputs(_("     --sd-param <data>    rfc5424 structured data name=value\n"), out);
	fputs(_("     --msgid <msgid>      set rfc5424 message id field\n"), out);
	fputs(_(" -u, --socket <socket>    write to this Unix socket\n"), out);
	fputs(_("     --socket-errors[=<on|off|auto>]\n"
		"                          print connection errors when using Unix sockets\n"), out);
#ifdef HAVE_LIBSYSTEMD
	fputs(_("     --journald[=<file>]  write journald entry\n"), out);
#endif

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));
	fprintf(out, USAGE_MAN_TAIL("logger(1)"));

	exit(EXIT_SUCCESS);
}

/*
 * logger -- read and log utility
 *
 *	Reads from an input and arranges to write the result on the system
 *	log.
 */
int main(int argc, char **argv)
{
	struct logger_ctl ctl = {
		.fd = -1,
		.pid = 0,
		.pri = LOG_USER | LOG_NOTICE,
		.prio_prefix = 0,
		.tag = NULL,
		.unix_socket = NULL,
		.unix_socket_errors = 0,
		.server = NULL,
		.port = NULL,
		.hdr = NULL,
		.msgid = NULL,
		.socket_type = ALL_TYPES,
		.max_message_size = 1024,
		.rfc5424_time = 1,
		.rfc5424_tq = 1,
		.rfc5424_host = 1,
		.skip_empty_lines = 0
	};
	int ch;
	int stdout_reopened = 0;
	int unix_socket_errors_mode = AF_UNIX_ERRORS_AUTO;
#ifdef HAVE_LIBSYSTEMD
	FILE *jfd = NULL;
#endif
	static const struct option longopts[] = {
		{ "id",		   optional_argument, 0, OPT_ID		   },
		{ "stderr",	   no_argument,	      0, 's'		   },
		{ "file",	   required_argument, 0, 'f'		   },
		{ "no-act",        no_argument,       0, OPT_NOACT,	   },
		{ "priority",	   required_argument, 0, 'p'		   },
		{ "tag",	   required_argument, 0, 't'		   },
		{ "socket",	   required_argument, 0, 'u'		   },
		{ "socket-errors", required_argument, 0, OPT_SOCKET_ERRORS },
		{ "udp",	   no_argument,	      0, 'd'		   },
		{ "tcp",	   no_argument,	      0, 'T'		   },
		{ "server",	   required_argument, 0, 'n'		   },
		{ "port",	   required_argument, 0, 'P'		   },
		{ "version",	   no_argument,	      0, 'V'		   },
		{ "help",	   no_argument,	      0, 'h'		   },
		{ "octet-count",   no_argument,	      0, OPT_OCTET_COUNT   },
		{ "prio-prefix",   no_argument,	      0, OPT_PRIO_PREFIX   },
		{ "rfc3164",	   no_argument,	      0, OPT_RFC3164	   },
		{ "rfc5424",	   optional_argument, 0, OPT_RFC5424	   },
		{ "size",	   required_argument, 0, 'S'		   },
		{ "msgid",	   required_argument, 0, OPT_MSGID	   },
		{ "skip-empty",	   no_argument,	      0, 'e'		   },
		{ "sd-id",         required_argument, 0, OPT_STRUCTURED_DATA_ID          },
		{ "sd-param",      required_argument, 0, OPT_STRUCTURED_DATA_PARAM       },
#ifdef HAVE_LIBSYSTEMD
		{ "journald",	   optional_argument, 0, OPT_JOURNALD	   },
#endif
		{ NULL,		   0,		      0, 0		   }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	INIT_LIST_HEAD(&ctl.user_sds);
	INIT_LIST_HEAD(&ctl.reserved_sds);

	while ((ch = getopt_long(argc, argv, "ef:ip:S:st:u:dTn:P:Vh",
					    longopts, NULL)) != -1) {
		switch (ch) {
		case 'f':		/* file to log */
			if (freopen(optarg, "r", stdin) == NULL)
				err(EXIT_FAILURE, _("file %s"), optarg);
			stdout_reopened = 1;
			break;
		case 'e':
			ctl.skip_empty_lines = 1;
			break;
		case 'i':		/* log process id also */
			ctl.pid = logger_getpid();
			break;
		case OPT_ID:
			if (optarg) {
				const char *p = optarg;

				if (*p == '=')
					p++;
				ctl.pid = strtoul_or_err(optarg, _("failed to parse id"));
			} else
				ctl.pid = logger_getpid();
			break;
		case 'p':		/* priority */
			ctl.pri = pencode(optarg);
			break;
		case 's':		/* log to standard error */
			ctl.stderr_printout = 1;
			break;
		case 't':		/* tag */
			ctl.tag = optarg;
			break;
		case 'u':		/* unix socket */
			ctl.unix_socket = optarg;
			break;
		case 'S':		/* max message size */
			ctl.max_message_size = strtosize_or_err(optarg,
				_("failed to parse message size"));
			break;
		case 'd':
			ctl.socket_type = TYPE_UDP;
			break;
		case 'T':
			ctl.socket_type = TYPE_TCP;
			break;
		case 'n':
			ctl.server = optarg;
			break;
		case 'P':
			ctl.port = optarg;
			break;
		case OPT_OCTET_COUNT:
			ctl.octet_count = 1;
			break;
		case OPT_PRIO_PREFIX:
			ctl.prio_prefix = 1;
			break;
		case OPT_RFC3164:
			ctl.syslogfp = syslog_rfc3164_header;
			break;
		case OPT_RFC5424:
			ctl.syslogfp = syslog_rfc5424_header;
			if (optarg)
				parse_rfc5424_flags(&ctl, optarg);
			break;
		case OPT_MSGID:
			if (strchr(optarg, ' '))
				errx(EXIT_FAILURE, _("--msgid cannot contain space"));
			ctl.msgid = optarg;
			break;
#ifdef HAVE_LIBSYSTEMD
		case OPT_JOURNALD:
			if (optarg) {
				jfd = fopen(optarg, "r");
				if (!jfd)
					err(EXIT_FAILURE, _("cannot open %s"),
					    optarg);
			} else
				jfd = stdin;
			break;
#endif
		case OPT_SOCKET_ERRORS:
			unix_socket_errors_mode = parse_unix_socket_errors_flags(optarg);
			break;
		case OPT_NOACT:
			ctl.noact = 1;
			break;
		case OPT_STRUCTURED_DATA_ID:
			if (!valid_structured_data_id(optarg))
				errx(EXIT_FAILURE, _("invalid structured data ID: '%s'"), optarg);
			add_structured_data_id(get_user_structured_data(&ctl), optarg);
			break;
		case OPT_STRUCTURED_DATA_PARAM:
			if (!valid_structured_data_param(optarg))
				errx(EXIT_FAILURE, _("invalid structured data parameter: '%s'"), optarg);
			add_structured_data_param(get_user_structured_data(&ctl), optarg);
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (stdout_reopened && argc)
		warnx(_("--file <file> and <message> are mutually exclusive, message is ignored"));
#ifdef HAVE_LIBSYSTEMD
	if (jfd) {
		int ret = journald_entry(&ctl, jfd);
		if (stdin != jfd)
			fclose(jfd);
		if (ret)
			errx(EXIT_FAILURE, _("journald entry could not be written"));
		return EXIT_SUCCESS;
	}
#endif

	/* user overwrites built-in SD-ELEMENT */
	if (has_structured_data_id(get_user_structured_data(&ctl), "timeQuality"))
		ctl.rfc5424_tq = 0;

	switch (unix_socket_errors_mode) {
	case AF_UNIX_ERRORS_OFF:
		ctl.unix_socket_errors = 0;
		break;
	case AF_UNIX_ERRORS_ON:
		ctl.unix_socket_errors = 1;
		break;
	case AF_UNIX_ERRORS_AUTO:
		ctl.unix_socket_errors = ctl.noact || ctl.stderr_printout;
#ifdef HAVE_LIBSYSTEMD
		ctl.unix_socket_errors |= !!sd_booted();
#endif
		break;
	default:
		abort();
	}
	logger_open(&ctl);
	if (0 < argc) {
		generate_syslog_header(&ctl);
		logger_command_line(&ctl, argv);
	} else
		/* Note. --file <arg> reopens stdin making the below
		 * function to be used for file inputs. */
		logger_stdin(&ctl);

	logger_close(&ctl);
	return EXIT_SUCCESS;
}
