/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "prototypes.h"
#include "defines.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef USE_ECONF
#include <libeconf.h>
#endif

#include "alloc.h"
#include "atoi/str2i.h"
#include "getdef.h"
#include "shadowlog_internal.h"
#include "string/sprintf.h"


/*
 * A configuration item definition.
 */
struct itemdef {
	/*@null@*/const char *name;	/* name of the item                     */
	/*@null@*/char *value;		/* value given, or NULL if no value     */
};

#define PAMDEFS					\
	{"CHFN_AUTH", NULL},			\
	{"CHSH_AUTH", NULL},			\
	{"ENV_HZ", NULL},			\
	{"ENVIRON_FILE", NULL},			\
	{"ENV_TZ", NULL},			\
	{"FAILLOG_ENAB", NULL},			\
	{"FTMP_FILE", NULL},			\
	{"HMAC_CRYPTO_ALGO", NULL},		\
	{"ISSUE_FILE", NULL},			\
	{"LASTLOG_ENAB", NULL},			\
	{"LOGIN_STRING", NULL},			\
	{"MAIL_CHECK_ENAB", NULL},		\
	{"MOTD_FILE", NULL},			\
	{"NOLOGINS_FILE", NULL},		\
	{"OBSCURE_CHECKS_ENAB", NULL},		\
	{"PASS_ALWAYS_WARN", NULL},		\
	{"PASS_CHANGE_TRIES", NULL},		\
	{"PASS_MAX_LEN", NULL},			\
	{"PASS_MIN_LEN", NULL},			\
	{"PORTTIME_CHECKS_ENAB", NULL},		\
	{"QUOTAS_ENAB", NULL},			\
	{"SU_WHEEL_ONLY", NULL},		\
	{"ULIMIT", NULL},

/*
 * Items used in other tools (util-linux, etc.)
 */
#define FOREIGNDEFS				\
	{"ALWAYS_SET_PATH", NULL},		\
	{"ENV_ROOTPATH", NULL},			\
	{"LOGIN_KEEP_USERNAME", NULL},		\
	{"LOGIN_PLAIN_PROMPT", NULL},		\
	{"MOTD_FIRSTONLY", NULL},		\


#define NUMDEFS	(sizeof(def_table)/sizeof(def_table[0]))
static struct itemdef def_table[] = {
	{"CHFN_RESTRICT", NULL},
	{"CONSOLE_GROUPS", NULL},
	{"CONSOLE", NULL},
	{"CREATE_HOME", NULL},
	{"DEFAULT_HOME", NULL},
	{"ENCRYPT_METHOD", NULL},
	{"ENV_PATH", NULL},
	{"ENV_SUPATH", NULL},
	{"ERASECHAR", NULL},
	{"FAIL_DELAY", NULL},
	{"FAKE_SHELL", NULL},
	{"GID_MAX", NULL},
	{"GID_MIN", NULL},
	{"HOME_MODE", NULL},
	{"HUSHLOGIN_FILE", NULL},
	{"KILLCHAR", NULL},
	{"LASTLOG_UID_MAX", NULL},
	{"LOGIN_RETRIES", NULL},
	{"LOGIN_TIMEOUT", NULL},
	{"LOG_OK_LOGINS", NULL},
	{"LOG_UNKFAIL_ENAB", NULL},
	{"MAIL_DIR", NULL},
	{"MAIL_FILE", NULL},
	{"MAX_MEMBERS_PER_GROUP", NULL},
	{"MD5_CRYPT_ENAB", NULL},
	{"NONEXISTENT", NULL},
	{"PASS_MAX_DAYS", NULL},
	{"PASS_MIN_DAYS", NULL},
	{"PASS_WARN_AGE", NULL},
#ifdef USE_SHA_CRYPT
	{"SHA_CRYPT_MAX_ROUNDS", NULL},
	{"SHA_CRYPT_MIN_ROUNDS", NULL},
#endif
#ifdef USE_BCRYPT
	{"BCRYPT_MAX_ROUNDS", NULL},
	{"BCRYPT_MIN_ROUNDS", NULL},
#endif
#ifdef USE_YESCRYPT
	{"YESCRYPT_COST_FACTOR", NULL},
#endif
	{"SUB_GID_COUNT", NULL},
	{"SUB_GID_MAX", NULL},
	{"SUB_GID_MIN", NULL},
	{"SUB_UID_COUNT", NULL},
	{"SUB_UID_MAX", NULL},
	{"SUB_UID_MIN", NULL},
	{"SULOG_FILE", NULL},
	{"SU_NAME", NULL},
	{"SYS_GID_MAX", NULL},
	{"SYS_GID_MIN", NULL},
	{"SYS_UID_MAX", NULL},
	{"SYS_UID_MIN", NULL},
	{"TTYGROUP", NULL},
	{"TTYPERM", NULL},
	{"TTYTYPE_FILE", NULL},
	{"UID_MAX", NULL},
	{"UID_MIN", NULL},
	{"UMASK", NULL},
	{"USERDEL_CMD", NULL},
	{"USERGROUPS_ENAB", NULL},
#ifndef USE_PAM
	PAMDEFS
#endif
	{"SYSLOG_SG_ENAB", NULL},
	{"SYSLOG_SU_ENAB", NULL},
#ifdef WITH_TCB
	{"TCB_AUTH_GROUP", NULL},
	{"TCB_SYMLINKS", NULL},
	{"USE_TCB", NULL},
#endif
	{"FORCE_SHADOW", NULL},
	{"GRANT_AUX_GROUP_SUBIDS", NULL},
	{"PREVENT_NO_AUTH", NULL},
	{NULL, NULL}
};

#define NUMKNOWNDEFS	(sizeof(knowndef_table)/sizeof(knowndef_table[0]))
static struct itemdef knowndef_table[] = {
#ifdef USE_PAM
	PAMDEFS
#endif
	FOREIGNDEFS
	{NULL, NULL}
};

#ifdef USE_ECONF
#ifdef VENDORDIR
static const char* vendordir = VENDORDIR;
#else
static const char* vendordir = NULL;
#endif
static const char* sysconfdir = "/etc";
#else
#ifndef LOGINDEFS
#define LOGINDEFS "/etc/login.defs"
#endif

static const char* def_fname = LOGINDEFS;	/* login config defs file       */
#endif
static bool def_loaded = false;		/* are defs already loaded?     */

/* local function prototypes */
static /*@observer@*/ /*@null@*/struct itemdef *def_find (const char *, const char *);
static void def_load (void);


/*
 * getdef_str - get string value from table of definitions.
 *
 * Return point to static data for specified item, or NULL if item is not
 * defined.  First time invoked, will load definitions from the file.
 */

/*@observer@*/ /*@null@*/const char *getdef_str (const char *item)
{
	struct itemdef *d;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	return (NULL == d) ? NULL : d->value;
}


/*
 * getdef_bool - get boolean value from table of definitions.
 *
 * Return TRUE if specified item is defined as "yes", else FALSE.
 */

bool getdef_bool (const char *item)
{
	struct itemdef *d;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	if ((NULL == d) || (NULL == d->value)) {
		return false;
	}

	return (strcasecmp (d->value, "yes") == 0);
}


/*
 * getdef_num - get numerical value from table of definitions
 *
 * Returns numeric value of specified item, else the "dflt" value if
 * the item is not defined.  Octal (leading "0") and hex (leading "0x")
 * values are handled.
 */

int getdef_num (const char *item, int dflt)
{
	struct itemdef *d;
	long val;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	if ((NULL == d) || (NULL == d->value)) {
		return dflt;
	}

	if (   (str2sl(&val, d->value) == -1)
	    || (val > INT_MAX)
	    || (val < -1)) {
		fprintf (shadow_logfd,
		         _("configuration error - cannot parse %s value: '%s'"),
		         item, d->value);
		return dflt;
	}

	return val;
}


/*
 * getdef_unum - get unsigned numerical value from table of definitions
 *
 * Returns numeric value of specified item, else the "dflt" value if
 * the item is not defined.  Octal (leading "0") and hex (leading "0x")
 * values are handled.
 */

unsigned int getdef_unum (const char *item, unsigned int dflt)
{
	struct itemdef *d;
	long val;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	if ((NULL == d) || (NULL == d->value)) {
		return dflt;
	}

	if (   (str2sl(&val, d->value) == -1)
	    || (val < 0)
	    || (val > INT_MAX)) {
		fprintf (shadow_logfd,
		         _("configuration error - cannot parse %s value: '%s'"),
		         item, d->value);
		return dflt;
	}

	return val;
}


/*
 * getdef_long - get long integer value from table of definitions
 *
 * Returns numeric value of specified item, else the "dflt" value if
 * the item is not defined.  Octal (leading "0") and hex (leading "0x")
 * values are handled.
 */

long getdef_long (const char *item, long dflt)
{
	struct itemdef *d;
	long val;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	if ((NULL == d) || (NULL == d->value)) {
		return dflt;
	}

	if (str2sl(&val, d->value) == -1 || val < -1) {
		fprintf (shadow_logfd,
		         _("configuration error - cannot parse %s value: '%s'"),
		         item, d->value);
		return dflt;
	}

	return val;
}

/*
 * getdef_ulong - get unsigned long numerical value from table of definitions
 *
 * Returns numeric value of specified item, else the "dflt" value if
 * the item is not defined.  Octal (leading "0") and hex (leading "0x")
 * values are handled.
 */

unsigned long getdef_ulong (const char *item, unsigned long dflt)
{
	struct itemdef *d;
	unsigned long val;

	if (!def_loaded) {
		def_load ();
	}

	d = def_find (item, NULL);
	if ((NULL == d) || (NULL == d->value)) {
		return dflt;
	}

	if (str2ul(&val, d->value) == -1) {
		fprintf (shadow_logfd,
		         _("configuration error - cannot parse %s value: '%s'"),
		         item, d->value);
		return dflt;
	}

	return val;
}

/*
 * putdef_str - override the value read from /etc/login.defs
 * (also used when loading the initial defaults)
 */

int putdef_str (const char *name, const char *value, const char *srcfile)
{
	struct itemdef *d;
	char *cp;

	if (!def_loaded) {
		def_load ();
	}

	/*
	 * Locate the slot to save the value.  If this parameter
	 * is unknown then "def_find" will print an err message.
	 */
	d = def_find (name, srcfile);
	if (NULL == d)
		return -1;

	/*
	 * Save off the value.
	 */
	cp = strdup (value);
	if (NULL == cp) {
		(void) fputs (_("Could not allocate space for config info.\n"),
		              shadow_logfd);
		SYSLOG ((LOG_ERR, "could not allocate space for config info"));
		return -1;
	}

	free (d->value);
	d->value = cp;
	return 0;
}


/*
 * def_find - locate named item in table
 *
 * Search through a table of configurable items to locate the
 * specified configuration option.
 *
 * If srcfile is not NULL, and the item is not found, then report an error saying
 * the unknown item was used in this file.
 */

static /*@observer@*/ /*@null@*/struct itemdef *def_find (const char *name, const char *srcfile)
{
	struct itemdef *ptr;

	/*
	 * Search into the table.
	 */

	for (ptr = def_table; NULL != ptr->name; ptr++) {
		if (strcmp (ptr->name, name) == 0) {
			return ptr;
		}
	}

	/*
	 * Item was never found.
	 */

	for (ptr = knowndef_table; NULL != ptr->name; ptr++) {
		if (strcmp (ptr->name, name) == 0) {
			goto out;
		}
	}
	fprintf (shadow_logfd,
	         _("configuration error - unknown item '%s' (notify administrator)\n"),
	         name);
	if (srcfile != NULL)
		SYSLOG ((LOG_CRIT, "shadow: unknown configuration item '%s' in '%s'", name, srcfile));

out:
	return NULL;
}

/*
 * setdef_config_file - set the default configuration file path
 *
 * must be called prior to any def* calls.
 */

void setdef_config_file (const char* file)
{
#ifdef USE_ECONF
	char  *cp;

	xasprintf(&cp, "%s/%s", file, sysconfdir);
	sysconfdir = cp;
#ifdef VENDORDIR
	xasprintf(&cp, "%s/%s", file, vendordir);
	vendordir = cp;
#endif
#else
	def_fname = file;
#endif
}

/*
 * def_load - load configuration table
 *
 * Loads the user-configured options from the default configuration file
 */

#ifdef USE_ECONF
static void def_load (void)
{
	econf_file *defs_file = NULL;
	econf_err error;
	char **keys;
	size_t key_number;

	/*
	 * Set the initialized flag.
	 * (do it early to prevent recursion in putdef_str())
	 */
	def_loaded = true;

	error = econf_readDirs (&defs_file, vendordir, sysconfdir, "login", "defs", " \t", "#");
	if (error) {
		if (error == ECONF_NOFILE)
			return;

		SYSLOG ((LOG_CRIT, "cannot open login definitions [%s]",
			econf_errString(error)));
		exit (EXIT_FAILURE);
	}

	if ((error = econf_getKeys(defs_file, NULL, &key_number, &keys))) {
		SYSLOG ((LOG_CRIT, "cannot read login definitions [%s]",
			econf_errString(error)));
		exit (EXIT_FAILURE);
	}

	for (size_t i = 0; i < key_number; i++) {
		char *value;

		error = econf_getStringValue(defs_file, NULL, keys[i], &value);
		if (error) {
			SYSLOG ((LOG_CRIT, "failed reading key %zu from econf [%s]",
				i, econf_errString(error)));
			exit (EXIT_FAILURE);
		}

		/*
		 * Store the value in def_table.
		 *
		 * Ignore failures to load the login.defs file.
		 * The error was already reported to the user and to
		 * syslog. The tools will just use their default values.
		 */
		(void)putdef_str (keys[i], value, econf_getPath(defs_file));

		free(value);
	}

	econf_free (keys);
	econf_free (defs_file);
}
#else /* USE_ECONF */
static void def_load (void)
{
	int i;
	FILE *fp;
	char buf[1024], *name, *value, *s;

	/*
	 * Set the initialized flag.
	 * (do it early to prevent recursion in putdef_str())
	 */
	def_loaded = true;

	/*
	 * Open the configuration definitions file.
	 */
	fp = fopen (def_fname, "r");
	if (NULL == fp) {
		if (errno == ENOENT)
			return;

		int err = errno;
		SYSLOG ((LOG_CRIT, "cannot open login definitions %s [%s]",
		         def_fname, strerror (err)));
		exit (EXIT_FAILURE);
	}

	/*
	 * Go through all of the lines in the file.
	 */
	while (fgets (buf, sizeof (buf), fp) != NULL) {

		/*
		 * Trim trailing whitespace.
		 */
		for (i = (ptrdiff_t) strlen (buf) - 1; i >= 0; --i) {
			if (!isspace (buf[i])) {
				break;
			}
		}
		i++;
		buf[i] = '\0';

		/*
		 * Break the line into two fields.
		 */
		name = buf + strspn (buf, " \t");	/* first nonwhite */
		if (*name == '\0' || *name == '#')
			continue;	/* comment or empty */

		s = name + strcspn (name, " \t");	/* end of field */
		if (*s == '\0')
			continue;	/* only 1 field?? */

		*s++ = '\0';
		value = s + strspn (s, " \"\t");	/* next nonwhite */
		*(value + strcspn (value, "\"")) = '\0';

		/*
		 * Store the value in def_table.
		 *
		 * Ignore failures to load the login.defs file.
		 * The error was already reported to the user and to
		 * syslog. The tools will just use their default values.
		 */
		(void)putdef_str (name, value, def_fname);
	}

	if (ferror (fp) != 0) {
		int err = errno;
		SYSLOG ((LOG_CRIT, "cannot read login definitions %s [%s]",
		         def_fname, strerror (err)));
		exit (EXIT_FAILURE);
	}

	(void) fclose (fp);
}
#endif /* USE_ECONF */


#ifdef CKDEFS
int main (int argc, char **argv)
{
	int i;
	char *cp;
	struct itemdef *d;

	def_load ();

	for (i = 0; i < NUMDEFS; ++i) {
		d = def_find (def_table[i].name, NULL);
		if (NULL == d) {
			printf ("error - lookup '%s' failed\n",
			        def_table[i].name);
		} else {
			printf ("%4d %-24s %s\n", i + 1, d->name, d->value);
		}
	}
	for (i = 1; i < argc; i++) {
		cp = getdef_str (argv[1]);
		if (NULL != cp) {
			printf ("%s `%s'\n", argv[1], cp);
		} else {
			printf ("%s not found\n", argv[1]);
		}
	}
	exit (EXIT_SUCCESS);
}
#endif
