/*
 * SPDX-License-Identifier: GPL-1.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 1992  A. V. Le Blanc (LeBlanc@mcc.ac.uk)
 * Copyright (C) 2012  Davidlohr Bueso <dave@gnu.org>
 *
 * Copyright (C) 2007-2013 Karel Zak <kzak@redhat.com>
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <libsmartcols.h>
#ifdef HAVE_LIBREADLINE
# define _FUNCTION_DEF
# include <readline/readline.h>
#endif

#include "c.h"
#include "xalloc.h"
#include "all-io.h"
#include "nls.h"
#include "rpmatch.h"
#include "blkdev.h"
#include "mbsalign.h"
#include "pathnames.h"
#include "canonicalize.h"
#include "strutils.h"
#include "closestream.h"
#include "pager.h"

#include "fdisk.h"

#include "pt-sun.h"		/* to toggle flags */

#ifdef HAVE_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#ifdef HAVE_LINUX_BLKPG_H
# include <linux/blkpg.h>
#endif

int pwipemode = WIPEMODE_AUTO;
int device_is_used;
int is_interactive;
struct fdisk_table *original_layout;

static int wipemode = WIPEMODE_AUTO;

/*
 * fdisk debug stuff (see fdisk.h and include/debug.h)
 */
UL_DEBUG_DEFINE_MASK(fdisk);
UL_DEBUG_DEFINE_MASKNAMES(fdisk) = UL_DEBUG_EMPTY_MASKNAMES;

static void fdiskprog_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(fdisk, FDISKPROG_DEBUG_, 0, FDISK_DEBUG);
}

static void reply_sighandler(int sig __attribute__((unused)))
{
	DBG(ASK, ul_debug("got signal"));
}

static int reply_running;

#ifdef HAVE_LIBREADLINE
static char *reply_line;

static void reply_linehandler(char *line)
{
	reply_line = line;
	reply_running = 0;
	rl_callback_handler_remove();	/* avoid duplicate prompt */
}
#endif

int get_user_reply(const char *prompt, char *buf, size_t bufsz)
{
	struct sigaction oldact, act = {
		.sa_handler = reply_sighandler
	};
	struct pollfd fds[] = {
		{ .fd = fileno(stdin), .events = POLLIN }
	};
	size_t sz;
	int ret = 0;

	DBG(ASK, ul_debug("asking for user reply %s", is_interactive ? "[interactive]" : ""));

	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, &oldact);

#ifdef HAVE_LIBREADLINE
	if (is_interactive)
		rl_callback_handler_install(prompt, reply_linehandler);
#endif
	errno = 0;
	reply_running = 1;
	do {
		int rc;

		*buf = '\0';
#ifdef HAVE_LIBREADLINE
		if (!is_interactive)
#endif
		{
			fputs(prompt, stdout);
			fflush(stdout);
		}

		rc = poll(fds, 1, -1);
		if (rc == -1 && errno == EINTR)	{	/* interrupted by signal */
			DBG(ASK, ul_debug("cancel by CTRL+C"));
			ret = -ECANCELED;
			goto done;
		}
		if (rc == -1 && errno != EAGAIN) {	/* error */
			ret = -errno;
			goto done;
		}
#ifdef HAVE_LIBREADLINE
		if (is_interactive) {
			/* read input and copy to buf[] */
			rl_callback_read_char();
			if (!reply_running && reply_line) {
				sz = strlen(reply_line);
				if (sz == 0)
					buf[sz++] = '\n';
				else
					memcpy(buf, reply_line, min(sz, bufsz));
				buf[min(sz, bufsz - 1)] = '\0';
				free(reply_line);
				reply_line = NULL;
			}
		} else
#endif
		{
			if (!fgets(buf, bufsz, stdin))
				*buf = '\0';
			break;
		}
	} while (reply_running);

	if (!*buf) {
		DBG(ASK, ul_debug("cancel by CTRL+D"));
		ret = -ECANCELED;
		clearerr(stdin);
		goto done;
	}

	/*
	 * cleanup the reply
	 */
	sz = ltrim_whitespace((unsigned char *) buf);
	if (sz && *(buf + sz - 1) == '\n')
		*(buf + sz - 1) = '\0';

done:
#ifdef HAVE_LIBREADLINE
	if (is_interactive)
		rl_callback_handler_remove();
#endif
	sigaction(SIGINT, &oldact, NULL);
	DBG(ASK, ul_debug("user's reply: >>>%s<<< [rc=%d]", buf, ret));
	return ret;
}

static int ask_menu(struct fdisk_context *cxt, struct fdisk_ask *ask,
		    char *buf, size_t bufsz)

{
	const char *q = fdisk_ask_get_query(ask);
	int dft = fdisk_ask_menu_get_default(ask);

	if (q) {
		fputs(q, stdout);		/* print header */
		fputc('\n', stdout);
	}

	do {
		char prompt[128];
		int key, c, rc;
		const char *name, *desc;
		size_t i = 0;

		/* print menu items */
		while (fdisk_ask_menu_get_item(ask, i++, &key, &name, &desc) == 0)
			fprintf(stdout, "   %c   %s (%s)\n", key, name, desc);

		/* ask for key */
		snprintf(prompt, sizeof(prompt), _("Select (default %c): "), dft);
		rc = get_user_reply(prompt, buf, bufsz);
		if (rc)
			return rc;
		if (!*buf) {
			fdisk_info(cxt, _("Using default response %c."), dft);
			c = dft;
		} else
			c = tolower(buf[0]);

		/* check result */
		i = 0;
		while (fdisk_ask_menu_get_item(ask, i++, &key, NULL, NULL) == 0) {
			if (c == key) {
				fdisk_ask_menu_set_result(ask, c);
				return 0;	/* success */
			}
		}
		fdisk_warnx(cxt, _("Value out of range."));
	} while (1);

	return -EINVAL;
}


#define tochar(num)	((int) ('a' + num - 1))
static int ask_number(struct fdisk_context *cxt,
		      struct fdisk_ask *ask,
		      char *buf, size_t bufsz)
{
	char prompt[128] = { '\0' };
	const char *q = fdisk_ask_get_query(ask);
	const char *range = fdisk_ask_number_get_range(ask);

	uint64_t dflt = fdisk_ask_number_get_default(ask),
		 low = fdisk_ask_number_get_low(ask),
		 high = fdisk_ask_number_get_high(ask);
	int inchar = fdisk_ask_number_inchars(ask);

	assert(q);

	DBG(ASK, ul_debug("asking for number "
			"['%s', <%"PRIu64",%"PRIu64">, default=%"PRIu64", range: %s]",
			q, low, high, dflt, range));

	if (range && dflt >= low && dflt <= high) {
		if (inchar)
			snprintf(prompt, sizeof(prompt), _("%s (%s, default %c): "),
					q, range, tochar(dflt));
		else
			snprintf(prompt, sizeof(prompt), _("%s (%s, default %"PRIu64"): "),
					q, range, dflt);

	} else if (dflt >= low && dflt <= high) {
		if (inchar)
			snprintf(prompt, sizeof(prompt), _("%s (%c-%c, default %c): "),
					q, tochar(low), tochar(high), tochar(dflt));
		else
			snprintf(prompt, sizeof(prompt),
					_("%s (%"PRIu64"-%"PRIu64", default %"PRIu64"): "),
					q, low, high, dflt);
	} else if (inchar)
		snprintf(prompt, sizeof(prompt), _("%s (%c-%c): "),
				q, tochar(low), tochar(high));
	else
		snprintf(prompt, sizeof(prompt), _("%s (%"PRIu64"-%"PRIu64"): "),
				q, low, high);

	do {
		int rc = get_user_reply(prompt, buf, bufsz);
		uint64_t num = 0;

		if (rc)
			return rc;
		if (!*buf && dflt >= low && dflt <= high)
			return fdisk_ask_number_set_result(ask, dflt);

		if (isdigit_string(buf)) {
			char *end;

			errno = 0;
			num = strtoumax(buf, &end, 10);
			if (errno || buf == end || (end && *end))
				continue;
		} else if (inchar && isalpha(*buf)) {
			num = tolower(*buf) - 'a' + 1;
		} else
			rc = -EINVAL;

		if (rc == 0 && num >= low && num <= high)
			return fdisk_ask_number_set_result(ask, num);

		fdisk_warnx(cxt, _("Value out of range."));
	} while (1);

	return -1;
}

static int ask_offset(struct fdisk_context *cxt,
		      struct fdisk_ask *ask,
		      char *buf, size_t bufsz)
{
	char prompt[128] = { '\0' };
	const char *q = fdisk_ask_get_query(ask);
	const char *range = fdisk_ask_number_get_range(ask);

	uint64_t dflt = fdisk_ask_number_get_default(ask),
		 low = fdisk_ask_number_get_low(ask),
		 high = fdisk_ask_number_get_high(ask),
		 base = fdisk_ask_number_get_base(ask);

	assert(q);

	DBG(ASK, ul_debug("asking for offset ['%s', <%"PRIu64",%"PRIu64">, base=%"PRIu64", default=%"PRIu64", range: %s]",
				q, low, high, base, dflt, range));

	if (range && dflt >= low && dflt <= high)
		snprintf(prompt, sizeof(prompt), _("%s (%s, default %"PRIu64"): "),
		         q, range, dflt);
	else if (dflt >= low && dflt <= high)
		snprintf(prompt, sizeof(prompt),
		         _("%s (%"PRIu64"-%"PRIu64", default %"PRIu64"): "),
		         q, low, high, dflt);
	else
		snprintf(prompt, sizeof(prompt), _("%s (%"PRIu64"-%"PRIu64"): "),
		         q, low, high);

	do {
		uintmax_t num = 0;
		char sig = 0, *p;
		int pwr = 0;

		int rc = get_user_reply(prompt, buf, bufsz);
		if (rc)
			return rc;
		if (!*buf && dflt >= low && dflt <= high)
			return fdisk_ask_number_set_result(ask, dflt);

		p = buf;
		if (*p == '+' || *p == '-') {
			sig = *buf;
			p++;
		}

		rc = parse_size(p, &num, &pwr);
		if (rc)
			continue;
		DBG(ASK, ul_debug("parsed size: %ju", num));
		if (sig && pwr) {
			/* +{size}{K,M,...} specified, the "num" is in bytes */
			uint64_t unit = fdisk_ask_number_get_unit(ask);
			num += unit/2;	/* round */
			num /= unit;
		}
		if (sig == '+')
			num += base;
		else if (sig == '-' && fdisk_ask_number_is_wrap_negative(ask))
			num = high - num;
		else if (sig == '-')
			num = base - num;

		DBG(ASK, ul_debug("final offset: %ju [sig: %c, power: %d, %s]",
				num, sig, pwr,
				sig ? "relative" : "absolute"));
		if (num >= low && num <= high) {
			if (sig && pwr)
				fdisk_ask_number_set_relative(ask, 1);
			return fdisk_ask_number_set_result(ask, (uint64_t)num);
		}
		fdisk_warnx(cxt, _("Value out of range."));
	} while (1);

	return -1;
}

static unsigned int info_count;

static void fputs_info(struct fdisk_ask *ask, FILE *out)
{
	const char *msg;
	assert(ask);

	msg = fdisk_ask_print_get_mesg(ask);
	if (!msg)
		return;
	if (info_count == 1)
		fputc('\n', out);

	fputs(msg, out);
	fputc('\n', out);
}

int ask_callback(struct fdisk_context *cxt, struct fdisk_ask *ask,
		    void *data __attribute__((__unused__)))
{
	int rc = 0;
	char buf[BUFSIZ] = { '\0' };

	assert(cxt);
	assert(ask);

	if (fdisk_ask_get_type(ask) != FDISK_ASKTYPE_INFO)
		info_count = 0;

	switch(fdisk_ask_get_type(ask)) {
	case FDISK_ASKTYPE_MENU:
		return ask_menu(cxt, ask, buf, sizeof(buf));
	case FDISK_ASKTYPE_NUMBER:
		return ask_number(cxt, ask, buf, sizeof(buf));
	case FDISK_ASKTYPE_OFFSET:
		return ask_offset(cxt, ask, buf, sizeof(buf));
	case FDISK_ASKTYPE_INFO:
		if (!fdisk_is_listonly(cxt))
			info_count++;
		fputs_info(ask, stdout);
		break;
	case FDISK_ASKTYPE_WARNX:
		fflush(stdout);
		color_scheme_fenable("warn", UL_COLOR_RED, stderr);
		fputs(fdisk_ask_print_get_mesg(ask), stderr);
		color_fdisable(stderr);
		fputc('\n', stderr);
		break;
	case FDISK_ASKTYPE_WARN:
		fflush(stdout);
		color_scheme_fenable("warn", UL_COLOR_RED, stderr);
		fputs(fdisk_ask_print_get_mesg(ask), stderr);
		errno = fdisk_ask_print_get_errno(ask);
		fprintf(stderr, ": %m\n");
		color_fdisable(stderr);
		break;
	case FDISK_ASKTYPE_YESNO:
		fputc('\n', stdout);
		do {
			int x;
			fputs(fdisk_ask_get_query(ask), stdout);
			rc = get_user_reply(_(" [Y]es/[N]o: "), buf, sizeof(buf));
			if (rc)
				break;
			x = rpmatch(buf);
			if (x == RPMATCH_YES || x == RPMATCH_NO) {
				fdisk_ask_yesno_set_result(ask, x);
				break;
			}
		} while(1);
		DBG(ASK, ul_debug("yes-no ask: reply '%s' [rc=%d]", buf, rc));
		break;
	case FDISK_ASKTYPE_STRING:
	{
		char prmt[BUFSIZ];
		snprintf(prmt, sizeof(prmt), "%s: ", fdisk_ask_get_query(ask));
		fputc('\n', stdout);
		rc = get_user_reply(prmt, buf, sizeof(buf));
		if (rc == 0)
			fdisk_ask_string_set_result(ask, xstrdup(buf));
		DBG(ASK, ul_debug("string ask: reply '%s' [rc=%d]", buf, rc));
		break;
	}
	default:
		warnx(_("internal error: unsupported dialog type %d"), fdisk_ask_get_type(ask));
		return -EINVAL;
	}
	return rc;
}

static struct fdisk_parttype *ask_partition_type(struct fdisk_context *cxt, int *canceled)
{
	const char *q;
	struct fdisk_label *lb;

	assert(cxt);
	lb = fdisk_get_label(cxt, NULL);

	if (!lb)
		return NULL;

	*canceled = 0;

	if (fdisk_label_has_parttypes_shortcuts(lb))
		 q = fdisk_label_has_code_parttypes(lb) ?
			_("Hex code or alias (type L to list all): ") :
			_("Partition type or alias (type L to list all): ");
	else
	        q = fdisk_label_has_code_parttypes(lb) ?
			_("Hex code (type L to list all codes): ") :
			_("Partition type (type L to list all types): ");
	do {
		char buf[256] = { '\0' };
		int rc = get_user_reply(q, buf, sizeof(buf));

		if (rc) {
			if (rc == -ECANCELED)
				*canceled = 1;
			break;
		}

		if (buf[1] == '\0' && toupper(*buf) == 'L')
			list_partition_types(cxt);
		else if (*buf) {
			struct fdisk_parttype *t = fdisk_label_advparse_parttype(lb, buf,
								FDISK_PARTTYPE_PARSE_DATA
								| FDISK_PARTTYPE_PARSE_ALIAS
								| FDISK_PARTTYPE_PARSE_NAME
								| FDISK_PARTTYPE_PARSE_SEQNUM);
			if (!t)
				fdisk_info(cxt, _("Failed to parse '%s' partition type."), buf);
			return t;
		}
        } while (1);

	return NULL;
}


void list_partition_types(struct fdisk_context *cxt)
{
	size_t ntypes = 0, next = 0;
	struct fdisk_label *lb;
	int pager = 0;

	assert(cxt);
	lb = fdisk_get_label(cxt, NULL);
	if (!lb)
		return;
	ntypes = fdisk_label_get_nparttypes(lb);
	if (!ntypes)
		return;

	if (fdisk_label_has_code_parttypes(lb)) {
		/*
		 * Prints in 4 columns in format <hex> <name>
		 */
		size_t last[4], done = 0, size;
		int i;

		size = ntypes;

		for (i = 3; i >= 0; i--)
			last[3 - i] = done += (size + i - done) / (i + 1);
		i = done = 0;

		do {
			#define NAME_WIDTH 15
			char name[NAME_WIDTH * MB_LEN_MAX];
			size_t width = NAME_WIDTH;
			const struct fdisk_parttype *t = fdisk_label_get_parttype(lb, next);
			size_t ret;

			if (fdisk_parttype_get_name(t)) {
				printf("%s%02x ", i ? "  " : "\n",
						fdisk_parttype_get_code(t));
				ret = mbsalign(_(fdisk_parttype_get_name(t)),
						name, sizeof(name),
						&width, MBS_ALIGN_LEFT, 0);

				if (ret == (size_t)-1 || ret >= sizeof(name))
					printf("%-15.15s",
						_(fdisk_parttype_get_name(t)));
				else
					fputs(name, stdout);
			}

			next = last[i++] + done;
			if (i > 3 || next >= last[i]) {
				i = 0;
				next = ++done;
			}
		} while (done < last[0]);

		putchar('\n');
	} else {
		/*
		 * Prints 1 column in format <idx> <name> <typestr>
		 */
		size_t i;

		pager_open();
		pager = 1;

		for (i = 0; i < ntypes; i++) {
			const struct fdisk_parttype *t = fdisk_label_get_parttype(lb, i);
			printf("%3zu %-30s %s\n", i + 1,
					fdisk_parttype_get_name(t),
					fdisk_parttype_get_string(t));
		}

	}


	/*
	 * Aliases
	 */
	if (fdisk_label_has_parttypes_shortcuts(lb)) {
		const char *alias = NULL, *typestr = NULL;
		int rc = 0;

		fputs(_("\nAliases:\n"), stdout);

		for (next = 0; rc == 0 || rc == 2; next++) {
			/* rc: <0 error, 0 success, 1 end, 2 deprecated */
			rc = fdisk_label_get_parttype_shortcut(lb,
					next, &typestr, NULL, &alias);
			if (rc == 0)
				printf("   %-14s - %s\n", alias, typestr);
		}
	}

	if (pager)
		pager_close();

}

void toggle_dos_compatibility_flag(struct fdisk_context *cxt)
{
	struct fdisk_label *lb = fdisk_get_label(cxt, "dos");
	int flag;

	if (!lb)
		return;

	flag = !fdisk_dos_is_compatible(lb);
	fdisk_info(cxt, flag ?
			_("DOS Compatibility flag is set (DEPRECATED!)") :
			_("DOS Compatibility flag is not set"));

	fdisk_dos_enable_compatible(lb, flag);

	if (fdisk_is_label(cxt, DOS))
		fdisk_reset_alignment(cxt);	/* reset the current label */
}

static int strtosize_sectors(const char *str, unsigned long sector_size,
			     uintmax_t *res)
{
	size_t len = strlen(str);
	int insec = 0;
	int rc;

	if (!len)
		return 0;

	if (str[len - 1] == 'S' || str[len - 1] == 's') {
		insec = 1;
		str = strndup(str, len - 1); /* strip trailing 's' */
		if (!str)
			return -errno;
	}

	rc = strtosize(str, res);
	if (rc)
		return rc;

	if (insec) {
		*res *= sector_size;
		free((void *)str);
	}

	return 0;
}

void resize_partition(struct fdisk_context *cxt)
{
	struct fdisk_partition *pa = NULL, *npa = NULL, *next = NULL;
	char *query = NULL, *response = NULL, *default_size;
	struct fdisk_table *tb = NULL;
	uint64_t max_size, size, secs;
	size_t i;
	int rc;

	assert(cxt);

	rc = fdisk_ask_partnum(cxt, &i, FALSE);
	if (rc)
		goto err;

	rc = fdisk_partition_get_max_size(cxt, i, &max_size);
	if (rc)
		goto err;

	max_size *= fdisk_get_sector_size(cxt);

	default_size = size_to_human_string(0, max_size);
	xasprintf(&query, _("New <size>{K,M,G,T,P} in bytes or <size>S in sectors (default %s)"),
		  default_size);
	free(default_size);

	rc = fdisk_ask_string(cxt, query, &response);
	if (rc)
		goto err;

	size = max_size;
	rc = strtosize_sectors(response, fdisk_get_sector_size(cxt), &size);
	if (rc || size > max_size) {
		fdisk_warnx(cxt, _("Invalid size"));
		goto err;
	}

	npa = fdisk_new_partition();
	if (!npa)
		goto err;

	secs = size / fdisk_get_sector_size(cxt);
	fdisk_partition_size_explicit(npa, 1);
	fdisk_partition_set_size(npa, secs);

	rc = fdisk_set_partition(cxt, i, npa);
	if (rc)
		goto err;

	fdisk_info(cxt, _("Partition %zu has been resized."), i + 1);

out:
	free(query);
	free(response);
	fdisk_unref_partition(next);
	fdisk_unref_partition(pa);
	fdisk_unref_partition(npa);
	fdisk_unref_table(tb);
	return;

err:
	fdisk_warnx(cxt, _("Could not resize partition %zu: %s"),
		    i + 1, strerror(-rc));
	goto out;
}

void change_partition_type(struct fdisk_context *cxt)
{
	size_t i;
	struct fdisk_parttype *t = NULL;
	struct fdisk_partition *pa = NULL;
	const char *old = NULL;
	int canceled = 0;

	assert(cxt);

	if (fdisk_ask_partnum(cxt, &i, FALSE))
		return;

	if (fdisk_get_partition(cxt, i, &pa)) {
		fdisk_warnx(cxt, _("Partition %zu does not exist yet!"), i + 1);
		return;
	}

	t = (struct fdisk_parttype *) fdisk_partition_get_type(pa);
	old = t ? fdisk_parttype_get_name(t) : _("Unknown");

	do {
		t = ask_partition_type(cxt, &canceled);
		if (canceled)
			break;
	} while (!t);

	if (canceled == 0 && t && fdisk_set_partition_type(cxt, i, t) == 0)
		fdisk_info(cxt,
			_("Changed type of partition '%s' to '%s'."),
			old, t ? fdisk_parttype_get_name(t) : _("Unknown"));
	else
		fdisk_info(cxt,
			_("Type of partition %zu is unchanged: %s."),
			i + 1, old);

	fdisk_unref_partition(pa);
	fdisk_unref_parttype(t);
}

int print_partition_info(struct fdisk_context *cxt)
{
	struct fdisk_partition *pa = NULL;
	int rc = 0;
	size_t i, nfields;
	int *fields = NULL;
	struct fdisk_label *lb = fdisk_get_label(cxt, NULL);

	if ((rc = fdisk_ask_partnum(cxt, &i, FALSE)))
		return rc;

	if ((rc = fdisk_get_partition(cxt, i, &pa))) {
		fdisk_warnx(cxt, _("Partition %zu does not exist yet!"), i + 1);
		return rc;
	}

	if ((rc = fdisk_label_get_fields_ids_all(lb, cxt, &fields, &nfields)))
		goto clean_data;

	for (i = 0; i < nfields; ++i) {
		int id = fields[i];
		char *data = NULL;
		const struct fdisk_field *fd = fdisk_label_get_field(lb, id);

		if (!fd)
			continue;

		rc = fdisk_partition_to_string(pa, cxt, id, &data);
		if (rc < 0)
			goto clean_data;
		if (!data || !*data)
			continue;
		fdisk_info(cxt, "%15s: %s", fdisk_field_get_name(fd), data);
		free(data);
	}

clean_data:
	fdisk_unref_partition(pa);
	free(fields);
	return rc;
}

static size_t skip_empty(const unsigned char *buf, size_t i, size_t sz)
{
	size_t next;
	const unsigned char *p0 = buf + i;

	for (next = i + 16; next < sz; next += 16) {
		if (memcmp(p0, buf + next, 16) != 0)
			break;
	}

	return next == i + 16 ? i : next;
}

static void dump_buffer(off_t base, unsigned char *buf, size_t sz)
{
	size_t i, l, next = 0;

	if (!buf)
		return;
	for (i = 0, l = 0; i < sz; i++, l++) {
		if (l == 0) {
			if (!next)
				next = skip_empty(buf, i, sz);
			printf("%08jx ", (intmax_t)base + i);
		}
		printf(" %02x", buf[i]);
		if (l == 7)				/* words separator */
			fputs(" ", stdout);
		else if (l == 15) {
			fputc('\n', stdout);		/* next line */
			l = -1;
			if (next > i) {
				printf("*\n");
				i = next - 1;
			}
			next = 0;
		}
	}
	if (l > 0)
		printf("\n");
}

static void dump_blkdev(struct fdisk_context *cxt, const char *name,
			uint64_t offset, size_t size)
{
	int fd = fdisk_get_devfd(cxt);

	fdisk_info(cxt, _("\n%s: offset = %"PRIu64", size = %zu bytes."),
			name, offset, size);

	assert(fd >= 0);

	if (lseek(fd, (off_t) offset, SEEK_SET) == (off_t) -1)
		fdisk_warn(cxt, _("cannot seek"));
	else {
		unsigned char *buf = xmalloc(size);

		if (read_all(fd, (char *) buf, size) != (ssize_t) size)
			fdisk_warn(cxt, _("cannot read"));
		else
			dump_buffer(offset, buf, size);
		free(buf);
	}
}

void dump_firstsector(struct fdisk_context *cxt)
{
	assert(cxt);

	dump_blkdev(cxt, _("First sector"), 0, fdisk_get_sector_size(cxt));
}

void dump_disklabel(struct fdisk_context *cxt)
{
	int i = 0;
	const char *name = NULL;
	uint64_t offset = 0;
	size_t size = 0;

	assert(cxt);

	while (fdisk_locate_disklabel(cxt, i++, &name, &offset, &size) == 0 && size)
		dump_blkdev(cxt, name, offset, size);
}

static fdisk_sector_t get_dev_blocks(char *dev)
{
	int fd, ret;
	fdisk_sector_t size;

	if ((fd = open(dev, O_RDONLY|O_NONBLOCK)) < 0)
		err(EXIT_FAILURE, _("cannot open %s"), dev);
	ret = blkdev_get_sectors(fd, (unsigned long long *) &size);
	close(fd);
	if (ret < 0)
		err(EXIT_FAILURE, _("BLKGETSIZE ioctl failed on %s"), dev);
	return size/2;
}


void follow_wipe_mode(struct fdisk_context *cxt)
{
	int dowipe = wipemode == WIPEMODE_ALWAYS ? 1 : 0;

	if (isatty(STDIN_FILENO) && wipemode == WIPEMODE_AUTO)
		dowipe = 1;	/* do it in interactive mode */

	if (fdisk_is_ptcollision(cxt) && wipemode != WIPEMODE_NEVER)
		dowipe = 1;	/* always remove old PT */

	fdisk_enable_wipe(cxt, dowipe);
	if (dowipe)
		fdisk_warnx(cxt, _(
			"The device contains '%s' signature and it will be removed by a write command. "
			"See fdisk(8) man page and --wipe option for more details."),
			fdisk_get_collision(cxt));
	else
		fdisk_warnx(cxt, _(
			"The device contains '%s' signature and it may remain on the device. "
			"It is recommended to wipe the device with wipefs(8) or "
			"fdisk --wipe, in order to avoid possible collisions."),
			fdisk_get_collision(cxt));
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);

	fprintf(out,
	      _(" %1$s [options] <disk>         change partition table\n"
	        " %1$s [options] -l [<disk>...] list partition table(s)\n"),
	       program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display or manipulate a disk partition table.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -b, --sector-size <size>      physical and logical sector size\n"), out);
	fputs(_(" -B, --protect-boot            don't erase bootbits when creating a new label\n"), out);
	fputs(_(" -c, --compatibility[=<mode>]  mode is 'dos' or 'nondos' (default)\n"), out);
	fprintf(out,
	      _(" -L, --color[=<when>]          colorize output (%s, %s or %s)\n"), "auto", "always", "never");
	fprintf(out,
	        "                                 %s\n", USAGE_COLORS_DEFAULT);
	fputs(_(" -l, --list                    display partitions and exit\n"), out);
	fputs(_(" -x, --list-details            like --list but with more details\n"), out);

	fputs(_(" -n, --noauto-pt               don't create default partition table on empty devices\n"), out);
	fputs(_(" -o, --output <list>           output columns\n"), out);
	fputs(_(" -t, --type <type>             recognize specified partition table type only\n"), out);
	fputs(_(" -u, --units[=<unit>]          display units: 'cylinders' or 'sectors' (default)\n"), out);
	fputs(_(" -s, --getsz                   display device size in 512-byte sectors [DEPRECATED]\n"), out);
	fputs(_("     --bytes                   print SIZE in bytes rather than in human readable format\n"), out);
	fprintf(out,
	      _("     --lock[=<mode>]           use exclusive device lock (%s, %s or %s)\n"), "yes", "no", "nonblock");
	fprintf(out,
	      _(" -w, --wipe <mode>             wipe signatures (%s, %s or %s)\n"), "auto", "always", "never");
	fprintf(out,
	      _(" -W, --wipe-partitions <mode>  wipe signatures from new partitions (%s, %s or %s)\n"), "auto", "always", "never");

	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -C, --cylinders <number>      specify the number of cylinders\n"), out);
	fputs(_(" -H, --heads <number>          specify the number of heads\n"), out);
	fputs(_(" -S, --sectors <number>        specify the number of sectors per track\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(31));

	list_available_columns(out);

	fprintf(out, USAGE_MAN_TAIL("fdisk(8)"));
	exit(EXIT_SUCCESS);
}


enum {
	ACT_FDISK = 0,	/* default */
	ACT_LIST,
	ACT_LIST_DETAILS,
	ACT_SHOWSIZE
};

int main(int argc, char **argv)
{
	int rc, i, c, act = ACT_FDISK, noauto_pt = 0;
	int colormode = UL_COLORMODE_UNDEF;
	struct fdisk_context *cxt;
	char *outarg = NULL;
	const char *devname, *lockmode = NULL;
	enum {
		OPT_BYTES	= CHAR_MAX + 1,
		OPT_LOCK
	};
	static const struct option longopts[] = {
		{ "bytes",          no_argument,       NULL, OPT_BYTES },
		{ "color",          optional_argument, NULL, 'L' },
		{ "compatibility",  optional_argument, NULL, 'c' },
		{ "cylinders",      required_argument, NULL, 'C' },
		{ "heads",          required_argument, NULL, 'H' },
		{ "sectors",        required_argument, NULL, 'S' },
		{ "getsz",          no_argument,       NULL, 's' },
		{ "help",           no_argument,       NULL, 'h' },
		{ "list",           no_argument,       NULL, 'l' },
		{ "list-details",   no_argument,       NULL, 'x' },
		{ "lock",           optional_argument, NULL, OPT_LOCK },
		{ "noauto-pt",      no_argument,       NULL, 'n' },
		{ "sector-size",    required_argument, NULL, 'b' },
		{ "type",           required_argument, NULL, 't' },
		{ "units",          optional_argument, NULL, 'u' },
		{ "version",        no_argument,       NULL, 'V' },
		{ "output",         required_argument, NULL, 'o' },
		{ "protect-boot",   no_argument,       NULL, 'B' },
		{ "wipe",           required_argument, NULL, 'w' },
		{ "wipe-partitions",required_argument, NULL, 'W' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	fdisk_init_debug(0);
	scols_init_debug(0);
	fdiskprog_init_debug();

	cxt = fdisk_new_context();
	if (!cxt)
		err(EXIT_FAILURE, _("failed to allocate libfdisk context"));

	fdisk_set_ask(cxt, ask_callback, NULL);

	while ((c = getopt_long(argc, argv, "b:Bc::C:hH:lL::no:sS:t:u::vVw:W:x",
				longopts, NULL)) != -1) {
		switch (c) {
		case 'b':
		{
			size_t sz = strtou32_or_err(optarg,
					_("invalid sector size argument"));
			if (sz != 512 && sz != 1024 && sz != 2048 && sz != 4096)
				errx(EXIT_FAILURE, _("invalid sector size argument"));
			fdisk_save_user_sector_size(cxt, sz, sz);
			break;
		}
		case 'B':
			fdisk_enable_bootbits_protection(cxt, 1);
			break;
		case 'C':
			fdisk_save_user_geometry(cxt,
				strtou32_or_err(optarg,
						_("invalid cylinders argument")),
				0, 0);
			break;
		case 'c':
			if (optarg) {
				/* this setting is independent on the current
				 * actively used label
				 */
				char *p = *optarg == '=' ? optarg + 1 : optarg;
				struct fdisk_label *lb = fdisk_get_label(cxt, "dos");

				if (!lb)
					err(EXIT_FAILURE, _("not found DOS label driver"));
				if (strcmp(p, "dos") == 0)
					fdisk_dos_enable_compatible(lb, TRUE);
				else if (strcmp(p, "nondos") == 0)
					fdisk_dos_enable_compatible(lb, FALSE);
				else
					errx(EXIT_FAILURE, _("unknown compatibility mode '%s'"), p);
			}
			/* use default if no optarg specified */
			break;
		case 'H':
			fdisk_save_user_geometry(cxt, 0,
				strtou32_or_err(optarg,
						_("invalid heads argument")),
				0);
			break;
		case 'S':
			fdisk_save_user_geometry(cxt, 0, 0,
				strtou32_or_err(optarg,
					_("invalid sectors argument")));
			break;
		case 'l':
			act = ACT_LIST;
			break;
		case 'x':
			act = ACT_LIST_DETAILS;
			break;
		case 'L':
			colormode = UL_COLORMODE_AUTO;
			if (optarg)
				colormode = colormode_or_err(optarg,
						_("unsupported color mode"));
			break;
		case 'n':
			noauto_pt = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 's':
			act = ACT_SHOWSIZE;
			break;
		case 't':
		{
			struct fdisk_label *lb = NULL;

			while (fdisk_next_label(cxt, &lb) == 0)
				fdisk_label_set_disabled(lb, 1);

			lb = fdisk_get_label(cxt, optarg);
			if (!lb)
				errx(EXIT_FAILURE, _("unsupported disklabel: %s"), optarg);
			fdisk_label_set_disabled(lb, 0);
			break;
		}
		case 'u':
			if (optarg && *optarg == '=')
				optarg++;
			if (fdisk_set_unit(cxt, optarg) != 0)
				errx(EXIT_FAILURE, _("unsupported unit"));
			break;
		case 'V': /* preferred for util-linux */
		case 'v': /* for backward compatibility only */
			print_version(EXIT_SUCCESS);
		case 'w':
			wipemode = wipemode_from_string(optarg);
			if (wipemode < 0)
				errx(EXIT_FAILURE, _("unsupported wipe mode"));
			break;
		case 'W':
			pwipemode = wipemode_from_string(optarg);
			if (pwipemode < 0)
				errx(EXIT_FAILURE, _("unsupported wipe mode"));
			break;
		case 'h':
			usage();
		case OPT_BYTES:
			fdisk_set_size_unit(cxt, FDISK_SIZEUNIT_BYTES);
			break;
		case OPT_LOCK:
			lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				lockmode = optarg;
			}
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (argc-optind != 1 && fdisk_has_user_device_properties(cxt))
		warnx(_("The device properties (sector size and geometry) should"
			" be used with one specified device only."));

	colors_init(colormode, "fdisk");
	is_interactive = isatty(STDIN_FILENO);

	switch (act) {
	case ACT_LIST:
	case ACT_LIST_DETAILS:
		fdisk_enable_listonly(cxt, 1);

		if (act == ACT_LIST_DETAILS)
			fdisk_enable_details(cxt, 1);

		init_fields(cxt, outarg, NULL);

		if (argc > optind) {
			int k;

			for (rc = 0, k = optind; k < argc; k++)
				rc += print_device_pt(cxt, argv[k], 1, 0, k != optind);

			if (rc)
				return EXIT_FAILURE;
		} else
			print_all_devices_pt(cxt, 0);
		break;

	case ACT_SHOWSIZE:
		/* deprecated */
		if (argc - optind <= 0) {
			warnx(_("bad usage"));
			errtryhelp(EXIT_FAILURE);
		}
		for (i = optind; i < argc; i++) {
			uintmax_t blks = get_dev_blocks(argv[i]);

			if (argc - optind == 1)
				printf("%ju\n", blks);
			else
				printf("%s: %ju\n", argv[i], blks);
		}
		break;

	case ACT_FDISK:
		if (argc-optind != 1) {
			warnx(_("bad usage"));
			errtryhelp(EXIT_FAILURE);
		}

		/* Here starts interactive mode, use fdisk_{warn,info,..} functions */
		color_scheme_enable("welcome", UL_COLOR_GREEN);
		fdisk_info(cxt, _("Welcome to fdisk (%s)."), PACKAGE_STRING);
		color_disable();
		fdisk_info(cxt, _("Changes will remain in memory only, until you decide to write them.\n"
				  "Be careful before using the write command.\n"));

		devname = argv[optind];
		rc = fdisk_assign_device(cxt, devname, 0);
		if (rc == -EACCES) {
			rc = fdisk_assign_device(cxt, devname, 1);
			if (rc == 0)
				fdisk_warnx(cxt, _("Device is open in read-only mode."));
		}
		if (rc)
			err(EXIT_FAILURE, _("cannot open %s"), devname);

		if (fdisk_device_is_used(cxt))
			fdisk_warnx(cxt, _(
			"This disk is currently in use - repartitioning is probably a bad idea.\n"
			"It's recommended to umount all file systems, and swapoff all swap\n"
			"partitions on this disk.\n"));

		fflush(stdout);

		if (!fdisk_is_readonly(cxt)
		    && blkdev_lock(fdisk_get_devfd(cxt), devname, lockmode) != 0) {
			fdisk_deassign_device(cxt, 1);
			fdisk_unref_context(cxt);
			return EXIT_FAILURE;
		}

		if (fdisk_get_collision(cxt))
			follow_wipe_mode(cxt);

		if (!fdisk_has_label(cxt)) {
			fdisk_info(cxt, _("Device does not contain a recognized partition table."));
			if (!noauto_pt)
				fdisk_create_disklabel(cxt, NULL);

		} else if (fdisk_is_label(cxt, GPT) && fdisk_gpt_is_hybrid(cxt))
			fdisk_warnx(cxt, _(
				  "A hybrid GPT was detected. You have to sync "
				  "the hybrid MBR manually (expert command 'M')."));

		init_fields(cxt, outarg, NULL);		/* -o <columns> */

		if (!fdisk_is_readonly(cxt)) {
			fdisk_get_partitions(cxt, &original_layout);
			device_is_used = fdisk_device_is_used(cxt);
		}

		while (1)
			process_fdisk_menu(&cxt);
	}

	fdisk_unref_context(cxt);
	return EXIT_SUCCESS;
}
