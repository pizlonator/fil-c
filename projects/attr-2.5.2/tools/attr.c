/*
 * Copyright (c) 2000-2002,2004 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <locale.h>

#include <attr/attributes.h>

#include "misc.h"

#define	SETOP		1		/* do a SET operation */
#define	GETOP		2		/* do a GET operation */
#define	REMOVEOP	3		/* do a REMOVE operation */
#define	LISTOP		4		/* do a LIST operation */

#define	BUFSIZE		(60*1024)	/* buffer size for LIST operations */

static char *progname;

void
usage(void)
{
	fprintf(stderr, _(
"Usage: %s [-LRSq] -s attrname [-V attrvalue] pathname  # set value\n"
"       %s [-LRSq] -g attrname pathname                 # get value\n"
"       %s [-LRSq] -r attrname pathname                 # remove attr\n"
"       %s [-LRq]  -l pathname                          # list attrs \n"
"      -s reads a value from stdin and -g writes a value to stdout\n"),
		progname, progname, progname, progname);
	exit(1);
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
int
main(int argc, char **argv)
{
	char *attrname, *attrvalue, *filename, *buffer;
	int attrlength, attrflags;
	int opflag, i, ch, error, follow, verbose, rootflag, secureflag;
	attrlist_t *alist;
	attrlist_ent_t *aep;
	attrlist_cursor_t cursor;

	progname = basename(argv[0]);

	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/*
	 * Pick up and validate the arguments.
	 */
	verbose = 1;
	follow = opflag = rootflag = secureflag = 0;
	attrname = attrvalue = NULL;
	while ((ch = getopt(argc, argv, "s:V:g:r:lqLRS")) != EOF) {
		switch (ch) {
		case 's':
			if ((opflag != 0) && (opflag != SETOP)) {
				fprintf(stderr,
				  _("Only one of -s, -g, -r, or -l allowed\n"));
				usage();
			}
			opflag = SETOP;
			attrname = optarg;
			break;
		case 'V':
			if ((opflag != 0) && (opflag != SETOP)) {
				fprintf(stderr, _("-V only allowed with -s\n"));
				usage();
			}
			opflag = SETOP;
			attrvalue = optarg;
			break;
		case 'g':
			if (opflag) {
				fprintf(stderr,
				  _("Only one of -s, -g, -r, or -l allowed\n"));
				usage();
			}
			opflag = GETOP;
			attrname = optarg;
			break;
		case 'r':
			if (opflag) {
				fprintf(stderr,
				  _("Only one of -s, -g, -r, or -l allowed\n"));
				usage();
			}
			opflag = REMOVEOP;
			attrname = optarg;
			break;
		case 'l':
			if (opflag) {
				fprintf(stderr,
				  _("Only one of -s, -g, -r, or -l allowed\n"));
				usage();
			}
			opflag = LISTOP;
			break;
		case 'L':
			follow++;
			break;
		case 'R':
			rootflag++;
			break;
		case 'S':
			secureflag++;
			break;
		case 'q':
			verbose = 0;
			break;
		default:
			fprintf(stderr, _("Unrecognized option: %c\n"),
				(char)ch);
			usage();
			break;
		}
	}
	if (optind != argc-1) {
		fprintf(stderr, _("A filename to operate on is required\n"));
		usage();
	}
	filename = argv[optind];

	attrflags = ((!follow ? ATTR_DONTFOLLOW : 0) |
		     (secureflag ? ATTR_SECURE : 0) |
		     (rootflag ? ATTR_ROOT : 0));
	/*
	 * Break out into option-specific processing.
	 */
	switch (opflag) {
	case SETOP:
		if (!attrname) {
		  fprintf(stderr, _("-V only allowed with -s\n"));
		  usage();
		}
		if (attrvalue == NULL) {
			attrvalue = malloc(ATTR_MAX_VALUELEN);
			if (attrvalue == NULL) {
				perror("malloc");
				exit(1);
			}
			attrlength =
				fread(attrvalue, 1, ATTR_MAX_VALUELEN, stdin);
		} else {
			attrlength = strlen(attrvalue);
		}
		error = attr_set(filename, attrname, attrvalue,
					   attrlength, attrflags);
		if (error) {
			perror("attr_set");
			fprintf(stderr, _("Could not set \"%s\" for %s\n"),
					attrname, filename);
			exit(1);
		}
		if (verbose) {
			printf(_("Attribute \"%s\" set to a %d byte value "
			       "for %s:\n"), attrname, attrlength, filename);
			fwrite(attrvalue, 1, attrlength, stdout);
			printf("\n");
		}
		break;

	case GETOP:
		attrvalue = malloc(ATTR_MAX_VALUELEN);
		if (attrvalue == NULL) {
			perror("malloc");
			exit(1);
		}
		attrlength = ATTR_MAX_VALUELEN;
		error = attr_get(filename, attrname, attrvalue,
					   &attrlength, attrflags);
		if (error) {
			perror("attr_get");
			fprintf(stderr, _("Could not get \"%s\" for %s\n"),
					attrname, filename);
			exit(1);
		}
		if (verbose) {
			printf(_("Attribute \"%s\" had a %d byte value "
				"for %s:\n"), attrname, attrlength, filename);
		}
		fwrite(attrvalue, 1, attrlength, stdout);
		if (verbose) {
			printf("\n");
		}
		break;

	case REMOVEOP:
		error = attr_remove(filename, attrname, attrflags);
		if (error) {
			perror("attr_remove");
			fprintf(stderr, _("Could not remove \"%s\" for %s\n"),
					attrname, filename);
			exit(1);
		}
		break;

	case LISTOP:
		if ((buffer = malloc(BUFSIZE)) == NULL) {
			perror("malloc");
			exit(1);
		}
		memset(&cursor, 0, sizeof(cursor));
		do {
			error = attr_list(filename, buffer, BUFSIZE,
					  attrflags, &cursor);
			if (error) {
				perror("attr_list");
				fprintf(stderr,
					_("Could not list %s\n"),
					filename);
				exit(1);
			}

			alist = (attrlist_t *)buffer;
			for (i = 0; i < alist->al_count; i++) {
				aep = (attrlist_ent_t *)&buffer[ alist->al_offset[i] ];
				if (verbose) {
					printf(
			_("Attribute \"%s\" has a %d byte value for %s\n"),
						aep->a_name, aep->a_valuelen,
						filename);
				} else {
					printf("%s\n", aep->a_name);
				}
			}
		} while (alist->al_more);
		break;

	default:
		fprintf(stderr,
			_("At least one of -s, -g, -r, or -l is required\n"));
		usage();
		break;
	}

	return(0);
}
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
