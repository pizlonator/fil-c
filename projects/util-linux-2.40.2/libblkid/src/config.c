/*
 * config.c - blkid.conf routines
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdint.h>
#include <stdarg.h>
#if defined (HAVE_LIBECONF)
#include <libeconf.h>
#endif

#include "blkidP.h"
#include "env.h"
#include "pathnames.h"

static int parse_evaluate(struct blkid_config *conf, char *s)
{
	while(s && *s) {
		char *sep;

		if (conf->nevals >= __BLKID_EVAL_LAST)
			goto err;
		sep = strchr(s, ',');
		if (sep)
			*sep = '\0';
		if (strcmp(s, "udev") == 0)
			conf->eval[conf->nevals] = BLKID_EVAL_UDEV;
		else if (strcmp(s, "scan") == 0)
			conf->eval[conf->nevals] = BLKID_EVAL_SCAN;
		else
			goto err;
		conf->nevals++;
		if (sep)
			s = sep + 1;
		else
			break;
	}
	return 0;
err:
	DBG(CONFIG, ul_debug(
		"config file: unknown evaluation method '%s'.", s));
	return -1;
}

#ifndef HAVE_LIBECONF
static int parse_next(FILE *fd, struct blkid_config *conf)
{
	char buf[BUFSIZ];
	char *s;

	/* read the next non-blank non-comment line */
	do {
		if (fgets (buf, sizeof(buf), fd) == NULL)
			return feof(fd) ? 0 : -1;
		s = strchr (buf, '\n');
		if (!s) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(fd))
				s = strchr (buf, '\0');
			else {
				DBG(CONFIG, ul_debug(
					"config file: missing newline at line '%s'.",
					buf));
				return -1;
			}
		}
		*s = '\0';
		if (--s >= buf && *s == '\r')
			*s = '\0';

		s = buf;
		while (*s == ' ' || *s == '\t')		/* skip space */
			s++;

	} while (*s == '\0' || *s == '#');

	if (!strncmp(s, "SEND_UEVENT=", 12)) {
		s += 12;
		if (*s && !strcasecmp(s, "yes"))
			conf->uevent = TRUE;
		else if (*s)
			conf->uevent = FALSE;
	} else if (!strncmp(s, "CACHE_FILE=", 11)) {
		s += 11;
		free(conf->cachefile);
		if (*s)
			conf->cachefile = strdup(s);
		else
			conf->cachefile = NULL;
	} else if (!strncmp(s, "EVALUATE=", 9)) {
		s += 9;
		if (*s && parse_evaluate(conf, s) == -1)
			return -1;
	} else {
		DBG(CONFIG, ul_debug(
			"config file: unknown option '%s'.", s));
		return -1;
	}
	return 0;
}
#endif /* !HAVE_LIBECONF */

/* return real config data or built-in default */
struct blkid_config *blkid_read_config(const char *filename)
{
	struct blkid_config *conf;

	conf = calloc(1, sizeof(*conf));
	if (!conf)
		return NULL;
	conf->uevent = -1;


	if (!filename)
		filename = safe_getenv("BLKID_CONF");

#ifndef HAVE_LIBECONF

	FILE *f;
	if (!filename)
		filename = BLKID_CONFIG_FILE;

	DBG(CONFIG, ul_debug("reading config file: %s.", filename));
	f = fopen(filename, "r" UL_CLOEXECSTR);
	if (!f) {
		DBG(CONFIG, ul_debug("%s: does not exist, using built-in default", filename));
		goto dflt;
	}
	while (!feof(f)) {
		if (parse_next(f, conf)) {
			DBG(CONFIG, ul_debug("%s: parse error", filename));
			goto err;
		}
	}

#else /* !HAVE_LIBECONF */

	static econf_file *file = NULL;
	char *line = NULL;
	bool uevent = false;
	econf_err error;

	if (filename) {
		DBG(CONFIG, ul_debug("reading config file: %s.", filename));
		error = econf_readFile(&file, filename, "= \t", "#");
	} else {
		error = econf_readDirs(&file,
				       _PATH_VENDORDIR,
				       "/etc", "blkid", "conf", "= \t", "#");
	}

	if (error) {
		if (error == ECONF_NOFILE) {
			if (filename)
				DBG(CONFIG,
				    ul_debug("%s: does not exist, using built-in default", filename));
			else
				DBG(CONFIG,
				    ul_debug("No configuration file blkid.conf found, using built-in default "));
			goto dflt;
		} else {
			if (filename)
				DBG(CONFIG, ul_debug("%s: parse error:%s", filename, econf_errString(error)));
			else
				DBG(CONFIG, ul_debug("parse error:%s", econf_errString(error)));

			goto err;
		}
	}

	if ((error = econf_getBoolValue(file, NULL, "SEND_UEVENT", &uevent))) {
		if (error != ECONF_NOKEY) {
			DBG(CONFIG, ul_debug("couldn't fetch SEND_UEVENT corrently: %s", econf_errString(error)));
			goto err;
		} else {
			DBG(CONFIG, ul_debug("key SEND_UEVENT not found, using built-in default "));
		}
	} else {
		conf->uevent = uevent ? TRUE : FALSE;
	}

	if ((error = econf_getStringValue(file, NULL, "CACHE_FILE", &(conf->cachefile)))) {
		conf->cachefile = NULL;
		if (error != ECONF_NOKEY) {
			DBG(CONFIG, ul_debug("couldn't fetch CACHE_FILE correctly: %s", econf_errString(error)));
			goto err;
		} else {
			DBG(CONFIG, ul_debug("key CACHE_FILE not found, using built-in default "));
		}
	}

	if ((error = econf_getStringValue(file, NULL, "EVALUATE", &line))) {
		conf->nevals = 0;
		if (error != ECONF_NOKEY) {
			DBG(CONFIG, ul_debug("couldn't fetch EVALUATE correctly: %s", econf_errString(error)));
			goto err;
		} else {
			DBG(CONFIG, ul_debug("key CACHE_FILE not found, using built-in default "));
		}
	} else {
		if (line && *line && parse_evaluate(conf, line) == -1)
			goto err;
	}

#endif /* HAVE_LIBECONF */
dflt:
	if (!conf->nevals) {
		conf->eval[0] = BLKID_EVAL_UDEV;
		conf->eval[1] = BLKID_EVAL_SCAN;
		conf->nevals = 2;
	}
	if (!conf->cachefile)
		conf->cachefile = strdup(BLKID_CACHE_FILE);
	if (conf->uevent == -1)
		conf->uevent = TRUE;
#ifndef HAVE_LIBECONF
	if (f)
		fclose(f);
#else
	econf_free(file);
	free(line);
#endif
	return conf;
err:
	free(conf->cachefile);
	free(conf);
#ifndef HAVE_LIBECONF
	fclose(f);
#else
	econf_free(file);
	free(line);
#endif
	return NULL;
}

void blkid_free_config(struct blkid_config *conf)
{
	if (!conf)
		return;
	free(conf->cachefile);
	free(conf);
}

#ifdef TEST_PROGRAM
/*
 * usage: tst_config [<filename>]
 */
int main(int argc, char *argv[])
{
	int i;
	struct blkid_config *conf;
	char *filename = NULL;

	blkid_init_debug(BLKID_DEBUG_ALL);

	if (argc == 2)
		filename = argv[1];

	conf = blkid_read_config(filename);
	if (!conf)
		return EXIT_FAILURE;

	printf("EVALUATE:    ");
	for (i = 0; i < conf->nevals; i++)
		printf("%s ", conf->eval[i] == BLKID_EVAL_UDEV ? "udev" : "scan");
	printf("\n");

	printf("SEND UEVENT: %s\n", conf->uevent ? "TRUE" : "FALSE");
	printf("CACHE_FILE:  %s\n", conf->cachefile);

	blkid_free_config(conf);
	return EXIT_SUCCESS;
}
#endif
