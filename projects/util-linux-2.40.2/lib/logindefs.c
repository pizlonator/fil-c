/*
 * Copyright (C) 2003, 2004, 2005 Thorsten Kukuk
 * Author: Thorsten Kukuk <kukuk@suse.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain any existing copyright
 *    notice, and this entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 *
 * 2. Redistributions in binary form must reproduce all prior and current
 *    copyright notices, this list of conditions, and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *   written permission.
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#include "c.h"
#include "closestream.h"
#include "logindefs.h"
#include "nls.h"
#include "pathnames.h"
#include "xalloc.h"


static void (*logindefs_loader)(void *) = NULL;
static void *logindefs_loader_data = NULL;

void logindefs_set_loader(void (*loader)(void *data), void *data)
{
	logindefs_loader = loader;
	logindefs_loader_data = data;
}

#ifndef HAVE_LIBECONF

struct item {
	char *name;		/* name of the option.  */
	char *value;		/* value of the option.  */
	char *path;		/* name of config file for this option.  */

	struct item *next;	/* pointer to next option.  */
};

static struct item *list = NULL;

void free_getlogindefs_data(void)
{
	struct item *ptr;

	ptr = list;
	while (ptr) {
		struct item *tmp = ptr->next;

		free(ptr->path);
		free(ptr->name);
		free(ptr->value);
		free(ptr);
		ptr = tmp;
	}

	list = NULL;
}

static void store(const char *name, const char *value, const char *path)
{
	struct item *new = xmalloc(sizeof(struct item));

	if (!name)
		abort();

	new->name = xstrdup(name);
	new->value = value && *value ? xstrdup(value) : NULL;
	new->path = xstrdup(path);
	new->next = list;
	list = new;
}

void logindefs_load_file(const char *filename)
{
	FILE *f;
	char buf[BUFSIZ];

	f = fopen(filename, "r");
	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {

		char *p, *name, *data = NULL;

		if (*buf == '#' || *buf == '\n')
			continue;	/* only comment or empty line */

		p = strchr(buf, '#');
		if (p)
			*p = '\0';
		else {
			size_t n = strlen(buf);
			if (n && *(buf + n - 1) == '\n')
				*(buf + n - 1) = '\0';
		}

		if (!*buf)
			continue;	/* empty line */

		/* ignore space at begin of the line */
		name = buf;
		while (*name && isspace((unsigned)*name))
			name++;

		/* go to the end of the name */
		data = name;
		while (*data && !(isspace((unsigned)*data) || *data == '='))
			data++;
		if (data > name && *data)
			*data++ = '\0';

		if (!*name || data == name)
			continue;

		/* go to the begin of the value */
		while (*data
		       && (isspace((unsigned)*data) || *data == '='
			   || *data == '"'))
			data++;

		/* remove space at the end of the value */
		p = data + strlen(data);
		if (p > data)
			p--;
		while (p > data && (isspace((unsigned)*p) || *p == '"'))
			*p-- = '\0';

		store(name, data, filename);
	}

	fclose(f);
}

static void load_defaults(void)
{
	if (logindefs_loader)
		logindefs_loader(logindefs_loader_data);
	else
		logindefs_load_file(_PATH_LOGINDEFS);
}

static struct item *search(const char *name)
{
	struct item *ptr;

	if (!list)
		load_defaults();

	ptr = list;
	while (ptr != NULL) {
		if (strcasecmp(name, ptr->name) == 0)
			return ptr;
		ptr = ptr->next;
	}

	return NULL;
}

static const char *search_config(const char *name)
{
	struct item *ptr;

	ptr = list;
	while (ptr != NULL) {
		if (strcasecmp(name, ptr->name) == 0)
			return ptr->path;
		ptr = ptr->next;
	}

	return NULL;
}

int getlogindefs_bool(const char *name, int dflt)
{
	struct item *ptr = search(name);
	return ptr && ptr->value ? (strcasecmp(ptr->value, "yes") == 0) : dflt;
}

unsigned long getlogindefs_num(const char *name, unsigned long dflt)
{
	struct item *ptr = search(name);
	char *end = NULL;
	unsigned long retval;

	if (!ptr || !ptr->value)
		return dflt;

	errno = 0;
	retval = strtoul(ptr->value, &end, 0);
	if (end && *end == '\0' && !errno)
		return retval;

	syslog(LOG_NOTICE, _("%s: %s contains invalid numerical value: %s"),
	       search_config(name), name, ptr->value);
	return dflt;
}

/*
 * Returns:
 *	@dflt		if @name not found
 *	""		(empty string) if found, but value not defined
 *	"string"	if found
 */
const char *getlogindefs_str(const char *name, const char *dflt)
{
	struct item *ptr = search(name);

	if (!ptr)
		return dflt;
	if (!ptr->value)
		return "";
	return ptr->value;
}

#else /* !HAVE_LIBECONF */

#include <libeconf.h>

static econf_file *file = NULL;

void free_getlogindefs_data(void)
{
	econf_free (file);
	file = NULL;
}

static void load_defaults(void)
{
	econf_err error;

	if (file != NULL)
	        free_getlogindefs_data();

	error = econf_readDirs(&file,
			_PATH_VENDORDIR,
			"/etc", "login", "defs", "= \t", "#");

	if (error)
	  syslog(LOG_NOTICE, _("Error reading login.defs: %s"),
		 econf_errString(error));

	if (logindefs_loader)
		logindefs_loader(logindefs_loader_data);

}

void logindefs_load_file(const char *filename)
{
	econf_file *file_l = NULL, *file_m = NULL;
	char *path;

	logindefs_loader = NULL; /* No recursion */

#if USE_VENDORDIR
	xasprintf(&path, _PATH_VENDORDIR"/%s", filename);

	if (!econf_readFile(&file_l, path, "= \t", "#")) {
	        if (file == NULL)
		        file = file_l;
	        else if (!econf_mergeFiles(&file_m, file, file_l)) {
		        econf_free(file);
			file = file_m;
			econf_free(file_l);
		}
	}
	free (path);
#endif

	xasprintf(&path, "/etc/%s", filename);

	if (!econf_readFile(&file_l, path, "= \t", "#")) {
	        if (file == NULL)
		        file = file_l;
	        else if (!econf_mergeFiles(&file_m, file, file_l)) {
	                econf_free(file);
			file = file_m;
			econf_free(file_l);
		}

	/* Try original filename, could be relative */
	} else if (!econf_readFile(&file_l, filename, "= \t", "#")) {
		if (file == NULL)
			file = file_l;
		else if (!econf_mergeFiles(&file_m, file, file_l)) {
			econf_free(file);
			file = file_m;
			econf_free(file_l);
		}
	}
	free (path);
}

int getlogindefs_bool(const char *name, int dflt)
{
        bool value;
	econf_err error;

	if (!file)
	        load_defaults();

	if (!file)
		return dflt;

	if ((error = econf_getBoolValue(file, NULL, name, &value))) {
	        if (error != ECONF_NOKEY)
	                syslog(LOG_NOTICE, _("couldn't fetch %s: %s"), name,
			       econf_errString(error));
		return dflt;
	}
	return value;
}

unsigned long getlogindefs_num(const char *name, unsigned long dflt)
{
	uint64_t value;
	econf_err error;

	if (!file)
	        load_defaults();

	if (!file)
		return dflt;

	if ((error = econf_getUInt64Value(file, NULL, name, &value))) {
	        if (error != ECONF_NOKEY)
		        syslog(LOG_NOTICE, _("couldn't fetch %s: %s"), name,
			       econf_errString(error));
		return dflt;
	}
	return value;
}

/*
 * Returns:
 *	@dflt		if @name not found
 *	""		(empty string) if found, but value not defined
 *	"string"	if found
 */
const char *getlogindefs_str(const char *name, const char *dflt)
{
        char *value;
	econf_err error;

	if (!file)
	        load_defaults();

	if (!file)
		return dflt;

	if ((error = econf_getStringValue(file, NULL, name, &value))) {
	        if (error != ECONF_NOKEY)
		  syslog(LOG_NOTICE, _("couldn't fetch %s: %s"), name,
			 econf_errString(error));
		return dflt;
	}
	if (value)
		return value;

	return xstrdup("");
}
#endif /* !HAVE_LIBECONF */

/*
 * For compatibility with shadow-utils we have to support additional
 * syntax for environment variables in login.defs(5) file. The standard
 * syntax is:
 *
 *	ENV_FOO   data
 *
 * but shadow-utils supports also
 *
 *	ENV_FOO FOO=data
 *
 * the FOO= prefix has to be remove before we call setenv().
 */
int logindefs_setenv(const char *name, const char *conf, const char *dflt)
{
	const char *val = getlogindefs_str(conf, dflt);
	const char *p;

	if (!val)
		return -1;

	p = strchr(val, '=');
	if (p) {
		size_t sz = strlen(name);

		if (strncmp(val, name, sz) == 0 && *(p + 1)) {
			val = p + 1;
			if (*val == '"')
				val++;
			if (!*val)
				val = dflt;
		}
	}

	return val ? setenv(name, val, 1) : -1;
}

/*
 * We need to check the effective UID/GID. For example, $HOME could be on a
 * root-squashed NFS or on an NFS with UID mapping, and access(2) uses the
 * real UID/GID.  Then open(2) seems as the surest solution.
 * -- kzak@redhat.com (10-Apr-2009)
 */
int effective_access(const char *path, int mode)
{
	int fd = open(path, mode);
	if (fd != -1)
		close(fd);
	return fd == -1 ? -1 : 0;
}

/*
 * Check the per-account or the global hush-login setting.
 *
 * Hushed mode is enabled:
 *
 * a) if a global (e.g. /etc/hushlogins) hush file exists:
 *     1) for ALL ACCOUNTS if the file is empty
 *     2) for the current user if the username or shell is found in the file
 *
 * b) if a ~/.hushlogin file exists
 *
 * The ~/.hushlogin file is ignored if the global hush file exists.
 *
 * The HUSHLOGIN_FILE login.def variable overrides the default hush filename.
 *
 * Note that shadow-utils login(1) does not support "a1)". The "a1)" is
 * necessary if you want to use PAM for "Last login" message.
 *
 * -- Karel Zak <kzak@redhat.com> (26-Aug-2011)
 *
 *
 * The per-account check requires some explanation: As root we may not be able
 * to read the directory of the user if it is on an NFS-mounted filesystem. We
 * temporarily set our effective uid to the user-uid, making sure that we keep
 * root privileges in the real uid.
 *
 * A portable solution would require a fork(), but we rely on Linux having the
 * BSD setreuid().
 */

int get_hushlogin_status(struct passwd *pwd, int force_check)
{
	const char *files[] = { _PATH_HUSHLOGINS, _PATH_HUSHLOGIN, NULL };
	const char *file;
	char buf[BUFSIZ];
	int i;

	file = getlogindefs_str("HUSHLOGIN_FILE", NULL);
	if (file) {
		if (!*file)
			return 0;	/* empty HUSHLOGIN_FILE defined */

		files[0] = file;
		files[1] = NULL;
	}

	for (i = 0; files[i]; i++) {
		int ok = 0;

		file = files[i];

		/* global hush-file */
		if (*file == '/') {
			struct stat st;
			FILE *f;

			if (stat(file, &st) != 0)
				continue;	/* file does not exist */

			if (st.st_size == 0)
				return 1;	/* for all accounts */

			f = fopen(file, "r");
			if (!f)
				continue;	/* ignore errors... */

			while (ok == 0 && fgets(buf, sizeof(buf), f)) {
				if (buf[0] != '\0')
					buf[strlen(buf) - 1] = '\0';
				ok = !strcmp(buf, *buf == '/' ? pwd->pw_shell :
								pwd->pw_name);
			}
			fclose(f);
			if (ok)
				return 1;	/* found username/shell */

			return 0;		/* ignore per-account files */
		}

		/* per-account setting */
		if (strlen(pwd->pw_dir) + strlen(file) + 2 > sizeof(buf))
			continue;

		if (snprintf(buf, sizeof(buf), "%s/%s", pwd->pw_dir, file) < 0)
			continue;

		if (force_check) {
			uid_t ruid = getuid();
			gid_t egid = getegid();

			if (setregid(-1, pwd->pw_gid) == 0 &&
			    setreuid(0, pwd->pw_uid) == 0)
				ok = effective_access(buf, O_RDONLY) == 0;

			if (setuid(0) != 0 ||
			    setreuid(ruid, 0) != 0 ||
			    setregid(-1, egid) != 0) {
				syslog(LOG_ALERT, _("hush login status: restore original IDs failed"));
				exit(EXIT_FAILURE);
			}
			if (ok)
				return 1;	/* enabled by user */
		}
		else {
			int rc;
			rc = effective_access(buf, O_RDONLY);
			if (rc == 0)
				return 1;

			if (rc == -1 && errno == EACCES)
				return -1;
		}

	}

	return 0;
}
#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	char *name, *type;
	close_stdout_atexit();

	if (argc <= 1)
		errx(EXIT_FAILURE, "usage: %s <filename> "
		     "[<str|num|bool> <valname>]", argv[0]);

	logindefs_load_file(argv[1]);

	if (argc != 4) {	/* list all */
#ifdef HAVE_LIBECONF
		int i;
		char *keys[] = {"END", "EMPTY", "CRAZY3", "CRAZY2", "CRAZY1",
				"BOOLEAN", "NUMBER", "STRING", "HELLO_WORLD",
				NULL};

		for (i = 0; keys[i] != NULL; i++) {
		  	char *value = NULL;

			econf_getStringValue(file, NULL, keys[i], &value);
		        printf ("%s: $%s: '%s'\n", argv[1], keys[i], value);
		}

		econf_free (file);

#else
		struct item *ptr;

		for (ptr = list; ptr; ptr = ptr->next)
			printf("%s: $%s: '%s'\n", ptr->path, ptr->name,
			       ptr->value);
#endif
		return EXIT_SUCCESS;
	}

	type = argv[2];
	name = argv[3];

	if (strcmp(type, "str") == 0)
		printf("$%s: '%s'\n", name, getlogindefs_str(name, "DEFAULT"));
	else if (strcmp(type, "num") == 0)
		printf("$%s: '%ld'\n", name, getlogindefs_num(name, 0));
	else if (strcmp(type, "bool") == 0)
		printf("$%s: '%s'\n", name,
		       getlogindefs_bool(name, 0) ? "Y" : "N");

	return EXIT_SUCCESS;
}
#endif
