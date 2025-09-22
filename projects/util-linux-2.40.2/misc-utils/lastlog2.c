/* SPDX-License-Identifier: BSD-2-Clause

  Copyright (c) 2023, Thorsten Kukuk <kukuk@suse.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>

#include "nls.h"
#include "c.h"
#include "strutils.h"
#include "lastlog2.h"

static char *lastlog2_path = LL2_DEFAULT_DATABASE;

static int bflg;
static time_t b_days;
static int tflg;
static time_t t_days;
static int sflg;

static int print_entry(const char *user, int64_t ll_time,
		const char *tty, const char *rhost,
		const char *pam_service, const char *error)
{
	static int once = 0;
	char *datep;
	struct tm *tm, tm_buf;
	char datetime[80];
	/* IPv6 address is at maximum 39 characters.
	   But for LL-addresses (fe80+only) the interface should be set,
	   so LL-address + % + IFNAMSIZ. */
	const int maxIPv6Addrlen = 42;

	/* Print only if older than b days */
	if (bflg && ((time (NULL) - ll_time) < b_days))
		return 0;

	/* Print only if newer than t days */
	if (tflg && ((time (NULL) - ll_time) > t_days))
		return 0;
        /* this is necessary if you compile this on architectures with
           a 32bit time_t type. */
        time_t t_time = ll_time;
        tm = localtime_r(&t_time, &tm_buf);
	if (tm == NULL)
		datep = "(unknown)";
	else {
		strftime(datetime, sizeof(datetime), "%a %b %e %H:%M:%S %z %Y", tm);
		datep = datetime;
	}

	if (ll_time == 0)
		datep = "**Never logged in**";

	if (!once) {
		printf("Username         Port     From%*s Latest%*s%s\n",
		       maxIPv6Addrlen - 4, " ",
		       sflg ? (int) strlen(datep) -5 : 0,
		       " ", sflg ? "Service" : "");
		once = 1;
	}
	printf("%-16s %-8.8s %*s %s%*s%s\n", user, tty ? tty : "",
	       -maxIPv6Addrlen, rhost ? rhost : "", datep,
	       sflg ? 31 - (int) strlen(datep) : 0,
	       (sflg && pam_service) ? " " : "",
	       sflg ? (pam_service ? pam_service : "") : "");

	if (error)
		printf("\nError: %s\n", error);

	return 0;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *output = stdout;

	fputs(USAGE_HEADER, output);
	fprintf(output, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, output);
	fputs(_(" -b, --before DAYS       print only records older than DAYS\n"), output);
	fputs(_(" -C, --clear             clear record of a user (requires -u)\n"), output);
	fputs(_(" -d, --database FILE     use FILE as lastlog2 database\n"), output);
	fputs(_(" -i, --import FILE       import data from old lastlog file\n"), output);
	fputs(_(" -r, --rename NEWNAME    rename existing user to NEWNAME (requires -u)\n"), output);
	fputs(_(" -s, --service           display PAM service\n"), output);
	fputs(_(" -S, --set               set lastlog record to current time (requires -u)\n"), output);
	fputs(_(" -t, --time DAYS         print only lastlog records more recent than DAYS\n"), output);
	fputs(_(" -u, --user LOGIN        print lastlog record of the specified LOGIN\n"), output);

	fputs(USAGE_SEPARATOR, output);
	fprintf(output, USAGE_HELP_OPTIONS(25));
	fprintf(output, USAGE_MAN_TAIL("lastlog2(8)"));

	exit(EXIT_SUCCESS);
}

/* Check if an user exists on the system */
#define has_user(_x)	(getpwnam(_x) != NULL)

int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{"before",   required_argument, NULL, 'b'},
		{"clear",    no_argument,       NULL, 'C'},
		{"database", required_argument, NULL, 'd'},
		{"help",     no_argument,       NULL, 'h'},
		{"import",   required_argument, NULL, 'i'},
		{"rename",   required_argument, NULL, 'r'},
		{"service",  no_argument,       NULL, 's'},
		{"set",      no_argument,       NULL, 'S'},
		{"time",     required_argument, NULL, 't'},
		{"user",     required_argument, NULL, 'u'},
		{"version",  no_argument,       NULL, 'v'},
		{NULL, 0, NULL, '\0'}
	};
	char *error = NULL;
	int Cflg = 0;
	int iflg = 0;
	int rflg = 0;
	int Sflg = 0;
	int uflg = 0;
	const char *user = NULL;
	const char *newname = NULL;
	const char *lastlog_file = NULL;
	struct ll2_context *db_context = NULL;

	int c;

	while ((c = getopt_long(argc, argv, "b:Cd:hi:r:sSt:u:v", longopts, NULL)) != -1) {
		switch (c) {
		case 'b': /* before DAYS; Print only records older than DAYS */
			{
				unsigned long days;
				errno = 0;
				days = strtoul_or_err(optarg, _("Cannot parse days"));
				b_days = (time_t) days * (24L * 3600L) /* seconds/DAY */;
				bflg = 1;
			}
			break;
		case 'C': /* clear; Clear record of a user (requires -u) */
			Cflg = 1;
			break;
		case 'd': /* database <FILE>;   Use FILE as lastlog2 database */
			lastlog2_path = optarg;
			break;
		case 'h': /* help; Display this help message and exit */
			usage();
			break;
		case 'i': /* import <FILE>; Import data from old lastlog file */
			lastlog_file = optarg;
			iflg = 1;
			break;
		case 'r': /* rename <NEWNAME>; Rename existing user to NEWNAME (requires -u) */
			rflg = 1;
			newname = optarg;
			break;
		case 's': /* service; Display PAM service */
			sflg = 1;
			break;
		case 'S': /* set; Set lastlog record to current time (requires -u) */
			/* Set lastlog record of a user to the current time. */
			Sflg = 1;
			break;
		case 't': /* time <DAYS>; Print only lastlog records more recent than DAYS */
			{
				unsigned long days;
				errno = 0;
				days = strtoul_or_err(optarg, _("Cannot parse days"));
				t_days = (time_t) days * (24L * 3600L) /* seconds/DAY */;
				tflg = 1;
			}
			break;
		case 'u': /* user <LOGIN>; Print lastlog record of the specified LOGIN */
			uflg = 1;
			user = optarg;
			break;
		case 'v': /* version; Print version number and exit */
			print_version(EXIT_SUCCESS);
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if ((Cflg + Sflg + iflg) > 1)
		errx(EXIT_FAILURE, _("Option -C, -i and -S cannot be used together"));

	db_context = ll2_new_context(lastlog2_path);
	if (!db_context)
		errx(EXIT_FAILURE, _("Couldn't initialize lastlog2 environment"));

	if (iflg) {
		/* Importing entries */
		if (ll2_import_lastlog(db_context, lastlog_file, &error) != 0) {
			warnx(_("Couldn't import entries from '%s'"), lastlog_file);
			goto err;
		}
		goto done;
	}

	if (Cflg || Sflg || rflg) {
		/* udpating, inserting and removing entries */
		if (!uflg || strlen(user) == 0) {
			warnx(_("Options -C, -r and -S require option -u to specify the user"));
			goto err;
		}

		if ((Cflg || Sflg) && !has_user(user)) {
			warnx(_("User '%s' does not exist."), user);
			goto err;
		}

		if (Cflg) {
			if (ll2_remove_entry(db_context, user, &error) != 0) {
				warnx(_("Couldn't remove entry for '%s'"), user);
				goto err;
			}
		}

		if (Sflg) {
			time_t ll_time = 0;

			if (time(&ll_time) == -1) {
				warn(_("Could not determine current time"));
				goto err;
			}

			if (ll2_update_login_time(db_context, user, ll_time, &error) != 0) {
				warnx(_("Couldn't update login time for '%s'"), user);
				goto err;
			}
		}

		if (rflg) {
			if (ll2_rename_user(db_context, user, newname, &error) != 0) {
				warnx(_("Couldn't rename entry '%s' to '%s'"), user, newname);
				goto err;
			}
		}

		goto done;
	}

	if (user) {
		/* print user specific information */
		int64_t ll_time = 0;
		char *tty = NULL;
		char *rhost = NULL;
		char *service = NULL;

		if (!has_user(user)) {
			warnx(_("User '%s' does not exist."), user);
			goto err;
		}

		/* We ignore errors, if the user is not in the database he did never login */
		ll2_read_entry(db_context, user, &ll_time, &tty, &rhost,
			       &service, NULL);

		print_entry(user, ll_time, tty, rhost, service, NULL);
		goto done;
	}

	/* print all information */
	if (ll2_read_all(db_context, print_entry, &error) != 0) {
		warnx(_("Couldn't read entries for all users"));
		goto err;
	}

done:
	ll2_unref_context(db_context);
	exit(EXIT_SUCCESS);
err:
	ll2_unref_context(db_context);
	if (error)
		errx(EXIT_FAILURE, "%s", error);
	exit(EXIT_FAILURE);
}
