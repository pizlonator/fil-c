/*
 * Sysctl - A utility to read and manipulate the sysctl parameters
 *
 * Copyright © 2009-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2012-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2017-2018 Werner Fink <werner@suse.de>
 * Copyright © 2014      Jaromir Capik <jcapik@redhat.com>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2002-2007 Albert Cahalan
 * Copyright © 1999      George Staikos
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Part of this code comes from systemd, especially sysctl.c
 * Changelog:
 *            v1.01:
 *                   - added -p <preload> to preload values from a file
 *            Horms:
 *                   - added -q to be quiet when modifying values
 *
 */

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "xalloc.h"
#include "procio.h"

/*
 *    Globals...
 */
static const char PROC_PATH[] = "/proc/sys/";
static const char DEFAULT_PRELOAD[] = "/etc/sysctl.conf";
static const char *DEPRECATED[] = {
	"base_reachable_time",
	"retrans_time",
	""
};
static bool IgnoreDeprecated;
static bool NameOnly;
static bool PrintName;
static bool PrintNewline;
static bool IgnoreError;
static bool Quiet;
static bool DryRun;
static char *pattern;

#define LINELEN 4096
static char *iobuf;
static size_t iolen = LINELEN;

typedef struct SysctlSetting {
    char *key;
    char *path;
    char *value;
    bool ignore_failure;
    bool glob_exclude;
    struct SysctlSetting *next;
} SysctlSetting;

typedef struct SettingList {
    struct SysctlSetting *head;
    struct SysctlSetting *tail;
} SettingList;

#define GLOB_CHARS "*?["
static inline bool string_is_glob(const char *p)
{
    return !!strpbrk(p, GLOB_CHARS);
}


/* Function prototypes. */
static int pattern_match(const char *string, const char *pat);
static int DisplayAll(const char *restrict const path);

static inline bool is_proc_path(
	const char *path)
{
    char *resolved_path;

    if ( (resolved_path = realpath(path, NULL)) == NULL)
	return false;

    if (strncmp(PROC_PATH, resolved_path, strlen(PROC_PATH)) == 0) {
	free(resolved_path);
	return true;
    }

    xwarnx(_("Path is not under %s: %s"), PROC_PATH, path);
    free(resolved_path);
    return false;
}

static void slashdot(char *restrict p, char old, char new)
{
	int warned = 1;
	p = strpbrk(p, "/.");
	if (!p)
		/* nothing -- can't be, but oh well */
		return;
	if (*p == new)
		/* already in desired format */
		return;
	while (p) {
		char c = *p;
		if ((*(p + 1) == '/' || *(p + 1) == '.') && warned) {
			xwarnx(_("separators should not be repeated: %s"), p);
			warned = 0;
		}
		if (c == old)
			*p = new;
		if (c == new)
			*p = old;
		p = strpbrk(p + 1, "/.");
	}
}

#if 0  // avoid '-Wunused-function' warning
static void setting_free(SysctlSetting *s) {
    if (!s)
	return;

    free(s->key);
    free(s->path);
    free(s->value);
    free(s);
}
#endif

static SysctlSetting *setting_new(
	const char *key,
	const char *value,
	bool ignore_failure,
    bool glob_exclude) {

    SysctlSetting *s = NULL;
    char *path = NULL;
    int proc_len;

    proc_len = strlen(PROC_PATH);
    /* used to open the file */
    path = xmalloc(strlen(key) + proc_len + 2);
    strcpy(path, PROC_PATH);
    if (key[0] == '-')
        strcat(path + proc_len, key+1);
    else
        strcat(path + proc_len, key);
    /* change . to / for path */
    slashdot(path + proc_len, '.', '/');

    s = xmalloc(sizeof(SysctlSetting));

    *s = (SysctlSetting) {
        .key = strdup(key),
        .path = path,
        .value = value? strdup(value): NULL,
        .ignore_failure = ignore_failure,
        .glob_exclude = glob_exclude,
        .next = NULL,
    };

    return s;
}

static void settinglist_add(SettingList *l, SysctlSetting *s) {
    SysctlSetting *old_tail;

    if (!l)
        return;

    if (l->head == NULL)
        l->head = s;

    if (l->tail != NULL) {
        old_tail = l->tail;
        old_tail->next = s;
    }
    l->tail = s;
}

static SysctlSetting *settinglist_findpath(const SettingList *l, const char *path) {
    SysctlSetting *node;

    for (node=l->head; node != NULL; node = node->next) {
        if (strcmp(node->path, path) == 0)
            return node;
    }
    return NULL;
}

/* Function prototypes. */
static int pattern_match(const char *string, const char *pat);
static int DisplayAll(const char *restrict const path);

/*
 * Display the usage format
 */
static void __attribute__ ((__noreturn__))
    Usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] [variable[=value] ...]\n"),
		program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_("  -a, --all            display all variables\n"), out);
	fputs(_("  -A                   alias of -a\n"), out);
	fputs(_("  -X                   alias of -a\n"), out);
	fputs(_("      --deprecated     include deprecated parameters to listing\n"), out);
	fputs(_("      --dry-run        Print the key and values but do not write\n"), out);
	fputs(_("  -b, --binary         print value without new line\n"), out);
	fputs(_("  -e, --ignore         ignore unknown variables errors\n"), out);
	fputs(_("  -N, --names          print variable names without values\n"), out);
	fputs(_("  -n, --values         print only values of the given variable(s)\n"), out);
	fputs(_("  -p, --load[=<file>]  read values from file\n"), out);
	fputs(_("  -f                   alias of -p\n"), out);
	fputs(_("      --system         read values from all system directories\n"), out);
	fputs(_("  -r, --pattern <expression>\n"
		"                       select setting that match expression\n"), out);
	fputs(_("  -q, --quiet          do not echo variable set\n"), out);
	fputs(_("  -w, --write          enable writing a value to variable\n"), out);
	fputs(_("  -o                   does nothing\n"), out);
	fputs(_("  -x                   does nothing\n"), out);
	fputs(_("  -d                   alias of -h\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("sysctl(8)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * Strip left/leading side of a string
 */
static char *lstrip(char *line)
{
    char *start;

    if (!line || !*line)
        return line;

    start = line;
    while(isspace(*start)) start++;

    return start;
}

/*
 * Strip right/trailing side of a string
 * by placing a \0
 */
static void rstrip(char *line)
{
    char *end;

    if (!line || !*line)
        return;

    end = line + strlen(line) - 1;
    while(end > line && isspace(*end)) end--;

    end[1] = '\0';
}

#if 0  // avoid '-Wunused-function' warning
/*
 * Strip the leading and trailing spaces from a string
 */
static char *StripLeadingAndTrailingSpaces(char *oneline)
{
	char *t;

	if (!oneline || !*oneline)
		return oneline;

	t = oneline;
	t += strlen(oneline) - 1;

	while ((*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r') && t != oneline)
		*t-- = 0;

	t = oneline;

	while ((*t == ' ' || *t == '\t') && *t != 0)
		t++;

	return t;
}
#endif

/*
 * Read a sysctl setting
 */
static int ReadSetting(const char *restrict const name)
{
	int rc = EXIT_SUCCESS;
	char *restrict tmpname;
	char *restrict outname;
	ssize_t rlen;
	FILE *restrict fp;
	struct stat ts;

	if (!name || !*name) {
		xwarnx(_("\"%s\" is an unknown key"), name);
		return -1;
	}

	/* used to open the file */
	tmpname = xmalloc(strlen(name) + strlen(PROC_PATH) + 2);
	strcpy(tmpname, PROC_PATH);
	strcat(tmpname, name);
	/* change . to / */
	slashdot(tmpname + strlen(PROC_PATH), '.', '/');

	/* used to display the output */
	outname = xstrdup(name);
	/* change / to . */
	slashdot(outname, '/', '.');

	if (stat(tmpname, &ts) < 0) {
		if (!IgnoreError) {
			xwarn(_("cannot stat %s"), tmpname);
			rc = EXIT_FAILURE;
		}
		goto out;
	}
	if ((ts.st_mode & S_IRUSR) == 0)
		goto out;

	if (!is_proc_path(tmpname)) {
	    rc = -1;
	    goto out;
	}

	if (S_ISDIR(ts.st_mode)) {
		size_t len;
		len = strlen(tmpname);
		tmpname[len] = '/';
		tmpname[len + 1] = '\0';
		rc = DisplayAll(tmpname);
		goto out;
	}

	if (pattern && !pattern_match(outname, pattern)) {
		rc = EXIT_SUCCESS;
		goto out;
	}

	if (NameOnly) {
		fprintf(stdout, "%s\n", outname);
		goto out;
	}

	fp = fprocopen(tmpname, "r");

	if (!fp) {
		switch (errno) {
		case ENOENT:
			if (!IgnoreError) {
				xwarnx(_("\"%s\" is an unknown key"), outname);
				rc = EXIT_FAILURE;
			}
			break;
		case EACCES:
			xwarnx(_("permission denied on key '%s'"), outname);
			rc = EXIT_FAILURE;
			break;
		case EIO:	    /* Ignore stable_secret below /proc/sys/net/ipv6/conf */
			rc = EXIT_FAILURE;
			break;
		default:
			xwarn(_("reading key \"%s\""), outname);
			rc = EXIT_FAILURE;
			break;
		}
	} else {
		errno = 0;
		if ((rlen = getline(&iobuf, &iolen, fp)) > 0) {
			/* this loop is required, see
			 * /sbin/sysctl -a | egrep -6 dev.cdrom.info
			 */
			do {
				char *nlptr;
				if (PrintName) {
					fprintf(stdout, "%s = ", outname);
					do {
						fprintf(stdout, "%s", iobuf);
						nlptr = &iobuf[strlen(iobuf) - 1];
						/* already has the \n in it */
						if (*nlptr == '\n')
							break;
					} while ((rlen = getline(&iobuf, &iolen, fp)) > 0);
					if (*nlptr != '\n')
						putchar('\n');
				} else {
					if (!PrintNewline) {
						nlptr = strchr(iobuf, '\n');
						if (nlptr)
							*nlptr = '\0';
					}
					fprintf(stdout, "%s", iobuf);
				}
			} while ((rlen = getline(&iobuf, &iolen, fp)) > 0);
		} else {
			switch (errno) {
			case EACCES:
				xwarnx(_("permission denied on key '%s'"),
				       outname);
				rc = EXIT_FAILURE;
				break;
			case EISDIR: {
					size_t len;
					len = strlen(tmpname);
					tmpname[len] = '/';
					tmpname[len + 1] = '\0';
					fclose(fp);
					rc = DisplayAll(tmpname);
					goto out;
				}
			case EIO:	    /* Ignore stable_secret below /proc/sys/net/ipv6/conf */
				rc = EXIT_FAILURE;
				break;
			default:
				xwarnx(_("reading key \"%s\""), outname);
				rc = EXIT_FAILURE;
			case 0:
				break;
			}
		}
		fclose(fp);
	}
      out:
	free(tmpname);
	free(outname);
	return rc;
}

static int is_deprecated(char *filename)
{
	int i;
	for (i = 0; strlen(DEPRECATED[i]); i++) {
		if (strcmp(DEPRECATED[i], filename) == 0)
			return 1;
	}
	return 0;
}

/*
 * Display all the sysctl settings
 */
static int DisplayAll(const char *restrict const path)
{
	int rc = EXIT_SUCCESS;
	int rc2;
	DIR *restrict dp;
	struct dirent *restrict de;
	struct stat ts;

	dp = opendir(path);

	if (!dp) {
		xwarnx(_("unable to open directory \"%s\""), path);
		rc = EXIT_FAILURE;
	} else {
		readdir(dp);	/* skip .  */
		readdir(dp);	/* skip .. */
		while ((de = readdir(dp))) {
			char *restrict tmpdir;
			if (IgnoreDeprecated && is_deprecated(de->d_name))
				continue;
			tmpdir =
			    (char *restrict) xmalloc(strlen(path) +
						     strlen(de->d_name) +
						     2);
			sprintf(tmpdir, "%s%s", path, de->d_name);
			rc2 = stat(tmpdir, &ts);
			if (rc2 != 0) {
				xwarn(_("cannot stat %s"), tmpdir);
			} else {
				if (S_ISDIR(ts.st_mode)) {
					strcat(tmpdir, "/");
					DisplayAll(tmpdir);
				} else {
					rc |=
					    ReadSetting(tmpdir +
							strlen(PROC_PATH));
				}
			}
			free(tmpdir);
		}
		closedir(dp);
	}
	return rc;
}

/*
 * Write a sysctl setting
 */
static int WriteSetting(
    const char *key,
    const char *path,
    const char *value,
    const bool ignore_failure)
{
    int rc = EXIT_SUCCESS;
    FILE *fp;
    struct stat ts;
    char *dotted_key;

    if (!key || !path)
        return rc;

    if (stat(path, &ts) < 0) {
        if (!IgnoreError) {
            xwarn(_("cannot stat %s"), path);
            rc = EXIT_FAILURE;
        }
        return rc;
    }

    if (!is_proc_path(path)) {
        return EXIT_FAILURE;
    }

    /* Convert the globbed path into a dotted key */
    if ( (dotted_key = strdup(path + strlen(PROC_PATH))) == NULL) {
	xerrx(EXIT_FAILURE, _("strdup key"));
	return EXIT_FAILURE;
    }
    slashdot(dotted_key, '/', '.');

    if ((ts.st_mode & S_IWUSR) == 0) {
        errno = EPERM;
        xwarn(_("setting key \"%s\""), dotted_key);
	free(dotted_key);
        return rc;
    }

    if (S_ISDIR(ts.st_mode)) {
        errno = EISDIR;
        xwarn(_("setting key \"%s\""), dotted_key);
	free(dotted_key);
        return rc;
    }

    if (!DryRun) {
        if ((fp = fprocopen(path, "w")) == NULL) {
            switch (errno) {
            case ENOENT:
                if (!IgnoreError) {
                    xwarnx(_("\"%s\" is an unknown key%s"),
                           dotted_key, (ignore_failure?_(", ignoring"):""));
                    if (!ignore_failure)
                        rc = EXIT_FAILURE;
                }
                break;
            case EPERM:
            case EROFS:
            case EACCES:
                xwarnx(_("permission denied on key \"%s\"%s"),
                       dotted_key, (ignore_failure?_(", ignoring"):""));
                break;
            default:
                xwarn(_("setting key \"%s\"%s"),
                      dotted_key, (ignore_failure?_(", ignoring"):""));
                break;
            }
            if (!ignore_failure && errno != ENOENT)
                rc = EXIT_FAILURE;
        } else {
            if (0 < fprintf(fp, "%s\n", value))
                rc = EXIT_SUCCESS;
            if (close_stream(fp) != 0) {
                xwarn(_("setting key \"%s\""), dotted_key);
		free(dotted_key);
                return EXIT_FAILURE;
            }
        }
    }
    if ((rc == EXIT_SUCCESS && !Quiet) || DryRun) {
        if (NameOnly) {
            printf("%s\n", dotted_key);
        } else {
            if (PrintName) {
                printf("%s = %s\n", dotted_key, value);
            } else {
                if (PrintNewline)
                    printf("%s\n", value);
                else
                    printf("%s", value);
            }
        }
    }
    free(dotted_key);
    return rc;
}

/*
 * parse each configuration line, there are multiple ways of specifying
 * a key/value here:
 *
 * key = value                               simple setting
 * -key = value                              ignore errors
 * key.pattern.*.with.glob = value           set keys that match glob
 * -key.pattern.exclude.with.glob            dont set this value
 * key.pattern.override.with.glob = value    set this glob match to value
 *
 */

static SysctlSetting *parse_setting_line(
    const char *path,
    const int linenum,
    char *line)
{
    char *key;
    char *value;
    bool glob_exclude = FALSE;
    bool ignore_failure = FALSE;

    key = lstrip(line);
    if (strlen(key) < 2)
        return NULL;

    /* skip over comments */
    if (key[0] == '#' || key[0] == ';')
        return NULL;

    if (pattern && !pattern_match(key, pattern))
        return NULL;

    value = strchr(key, '=');
    if (value == NULL) {
        if (key[0] == '-') {
            glob_exclude = TRUE;
            key++;
            value = NULL;
            rstrip(key);
        } else {
            xwarnx(_("%s(%d): invalid syntax, continuing..."),
                   path, linenum);
            return NULL;
        }
    } else {
        value[0]='\0';
        if (key[0] == '-') {
            ignore_failure = TRUE;
            key++;
        }
        value++; // skip over =
        value=lstrip(value);
        rstrip(value);
        rstrip(key);
    }
    return setting_new(key, value, ignore_failure, glob_exclude);
}

/* Go through the setting list, expand and sort out
 * setting globs and actually write the settings out
 */
static int write_setting_list(const SettingList *sl)
{
    SysctlSetting *node;
    int rc = EXIT_SUCCESS;

    for (node=sl->head; node != NULL; node=node->next) {
        if (node->glob_exclude)
            continue;

        if (string_is_glob(node->path)) {
            glob_t globbuf;
            int i;

            if (glob(node->path, 0, NULL, &globbuf) != 0)
                continue;

            for(i=0; i < globbuf.gl_pathc; i++) {
                if (settinglist_findpath(sl, globbuf.gl_pathv[i]))
                    continue; // override or exclude

                rc |= WriteSetting(node->key, globbuf.gl_pathv[i], node->value,
                                   node->ignore_failure);
            }
        } else {
            rc |= WriteSetting(node->key, node->path, node->value,
                               node->ignore_failure);
        }


    }

    return rc;
}

static int pattern_match(const char *string, const char *pat)
{
	int status;
	regex_t re;

	if (regcomp(&re, pat, REG_EXTENDED | REG_NOSUB) != 0)
		return (0);
	status = regexec(&re, string, (size_t) 0, NULL, 0);
	regfree(&re);
	if (status != 0)
		return (0);
	return (1);
}

/*
 * Preload the sysctl's from the conf file.  We parse the file and then
 * reform it (strip out whitespace).
 */
static int Preload(SettingList *setlist, const char *restrict const filename)
{
	FILE *fp;
	int n = 0;
	int rc = EXIT_SUCCESS;
	ssize_t rlen;
	glob_t globbuf;
	int globerr;
	int globflg;
	int j;

	globflg = GLOB_NOCHECK;
#ifdef GLOB_BRACE
	globflg |= GLOB_BRACE;
#endif
#ifdef GLOB_TILDE
	globflg |= GLOB_TILDE;
#else
	if (filename[0] == '~')
		xwarnx(_("GLOB_TILDE is not supported on your platform, "
			 "the tilde in \"%s\" won't be expanded."), filename);
#endif
	globerr = glob(filename, globflg, NULL, &globbuf);

	if (globerr != 0 && globerr != GLOB_NOMATCH)
		xerr(EXIT_FAILURE, _("glob failed"));

	for (j = 0; j < globbuf.gl_pathc; j++) {
		fp = (globbuf.gl_pathv[j][0] == '-' && !globbuf.gl_pathv[j][1])
		    ? stdin : fopen(globbuf.gl_pathv[j], "r");
		if (!fp) {
			xwarn(_("cannot open \"%s\""), globbuf.gl_pathv[j]);
            return EXIT_FAILURE;
		}

		while ((rlen =  getline(&iobuf, &iolen, fp)) > 0) {
            SysctlSetting *setting;

			n++;

			if (rlen < 2)
				continue;

            if ( (setting = parse_setting_line(globbuf.gl_pathv[j], n, iobuf))
                 == NULL)
                continue;
            settinglist_add(setlist, setting);
		}

		fclose(fp);
	}
	return rc;
}

struct pair {
	char *name;
	char *value;
};

static int sortpairs(const void *A, const void *B)
{
	const struct pair *a = *(struct pair * const *) A;
	const struct pair *b = *(struct pair * const *) B;
	return strcmp(a->name, b->name);
}

static int PreloadSystem(SettingList *setlist)
{
	unsigned di, i;
	const char *dirs[] = {
		"/etc/sysctl.d",
		"/run/sysctl.d",
		"/usr/local/lib/sysctl.d",
		"/usr/lib/sysctl.d",
		"/lib/sysctl.d",
	};
	struct pair **cfgs = NULL;
	unsigned ncfgs = 0;
	int rc = EXIT_SUCCESS;
	struct stat ts;
	enum { nprealloc = 16 };

	for (di = 0; di < sizeof(dirs) / sizeof(dirs[0]); ++di) {
		struct dirent *de;
		DIR *dp = opendir(dirs[di]);
		if (!dp)
			continue;

		while ((de = readdir(dp))) {
			if (!strcmp(de->d_name, ".")
			    || !strcmp(de->d_name, ".."))
				continue;
			if (strlen(de->d_name) < 5
			    || strcmp(de->d_name + strlen(de->d_name) - 5, ".conf"))
				continue;
			/* check if config already known */
			for (i = 0; i < ncfgs; ++i) {
				if (cfgs && !strcmp(cfgs[i]->name, de->d_name))
					break;
			}
			if (i < ncfgs)
				/* already in */
				continue;

			if (ncfgs % nprealloc == 0)
				cfgs =
				    xrealloc(cfgs,
					     sizeof(struct pair *) * (ncfgs +
								      nprealloc));

			if (cfgs) {
				cfgs[ncfgs] =
				    xmalloc(sizeof(struct pair) +
					    strlen(de->d_name) * 2 + 2 +
					    strlen(dirs[di]) + 1);
				cfgs[ncfgs]->name =
				    (char *)cfgs[ncfgs] + sizeof(struct pair);
				strcpy(cfgs[ncfgs]->name, de->d_name);
				cfgs[ncfgs]->value =
				    (char *)cfgs[ncfgs] + sizeof(struct pair) +
				    strlen(cfgs[ncfgs]->name) + 1;
				sprintf(cfgs[ncfgs]->value, "%s/%s", dirs[di],
					de->d_name);
				ncfgs++;
			} else {
				xerrx(EXIT_FAILURE, _("internal error"));
			}

		}
		closedir(dp);
	}
	qsort(cfgs, ncfgs, sizeof(struct cfg *), sortpairs);

	for (i = 0; i < ncfgs; ++i) {
		if (!Quiet)
			printf(_("* Applying %s ...\n"), cfgs[i]->value);
		rc |= Preload(setlist, cfgs[i]->value);
	}


	if (stat(DEFAULT_PRELOAD, &ts) == 0 && S_ISREG(ts.st_mode)) {
		if (!Quiet)
			printf(_("* Applying %s ...\n"), DEFAULT_PRELOAD);
		rc |= Preload(setlist, DEFAULT_PRELOAD);
	}

	/* cleaning */
	for (i = 0; i < ncfgs; ++i) {
		free(cfgs[i]);
	}
	if (cfgs) free(cfgs);

	return rc;
}

/*
 * Main...
 */
int main(int argc, char *argv[])
{
	bool WriteMode = false;
	bool DisplayAllOpt = false;
	bool preloadfileOpt = false;
	int ReturnCode = 0;
	int c;
	int rc = 0;
	const char *preloadfile = NULL;
    SettingList *setlist;

	enum {
		DEPRECATED_OPTION = CHAR_MAX + 1,
		SYSTEM_OPTION,
        DRYRUN_OPTION
	};
	static const struct option longopts[] = {
		{"all", no_argument, NULL, 'a'},
		{"deprecated", no_argument, NULL, DEPRECATED_OPTION},
		{"dry-run", no_argument, NULL, DRYRUN_OPTION},
		{"binary", no_argument, NULL, 'b'},
		{"ignore", no_argument, NULL, 'e'},
		{"names", no_argument, NULL, 'N'},
		{"values", no_argument, NULL, 'n'},
		{"load", optional_argument, NULL, 'p'},
		{"quiet", no_argument, NULL, 'q'},
		{"write", no_argument, NULL, 'w'},
		{"system", no_argument, NULL, SYSTEM_OPTION},
		{"pattern", required_argument, NULL, 'r'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	PrintName = true;
	PrintNewline = true;
	IgnoreError = false;
	Quiet = false;
	IgnoreDeprecated = true;
    DryRun = false;
    setlist = xmalloc(sizeof(SettingList));
    setlist->head = NULL;
    setlist->tail = NULL;

	if (argc < 2)
		Usage(stderr);

	while ((c =
		getopt_long(argc, argv, "bneNwfp::qoxaAXr:Vdh", longopts,
			    NULL)) != -1) {
		switch (c) {
		case 'b':
			/* This is "binary" format, which means more for BSD. */
			PrintNewline = false;
			/* FALL THROUGH */
		case 'n':
			PrintName = false;
			break;
		case 'e':
			/*
			 * For FreeBSD, -e means a "%s=%s\n" format.
			 * ("%s: %s\n" default). We (and NetBSD) use
			 * "%s = %s\n" always, and -e to ignore errors.
			 */
			IgnoreError = true;
			break;
		case 'N':
			NameOnly = true;
			break;
		case 'w':
			WriteMode = true;
			break;
		case 'f':	/* the NetBSD way */
		case 'p':
			preloadfileOpt = true;
			if (optarg)
				preloadfile = optarg;
			break;
		case 'q':
			Quiet = true;
			break;
		case 'o':	/* BSD: binary values too, 1st 16 bytes in hex */
		case 'x':	/* BSD: binary values too, whole thing in hex */
			/* does nothing */ ;
			break;
		case 'a':	/* string and integer values (for Linux, all of them) */
		case 'A':	/* same as -a -o */
		case 'X':	/* same as -a -x */
			DisplayAllOpt = true;
			break;
		case DEPRECATED_OPTION:
			IgnoreDeprecated = false;
			break;
		case SYSTEM_OPTION:
			IgnoreError = true;
			rc |= PreloadSystem(setlist);
            rc |= write_setting_list(setlist);
            return rc;
        case DRYRUN_OPTION:
            DryRun = true;
            break;
		case 'r':
			pattern = xstrdup(optarg);
			break;
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'd':	/* BSD: print description ("vm.kvm_size: Size of KVM") */
		case 'h':	/* BSD: human-readable (did FreeBSD 5 make -e default?) */
		case '?':
			Usage(stdout);
		default:
			Usage(stderr);
		}
	}

	argc -= optind;
	argv += optind;

	iobuf = xmalloc(iolen);

	if (DisplayAllOpt)
		return DisplayAll(PROC_PATH);

	if (preloadfileOpt) {
		int ret = EXIT_SUCCESS, i;
		if (!preloadfile) {
			if (!argc) {
				ret |= Preload(setlist, DEFAULT_PRELOAD);
			}
		} else {
			/* This happens when -pfile option is
			 * used without space. */
			ret |= Preload(setlist, preloadfile);
		}
		for (i = 0; i < argc; i++)
			ret |= Preload(setlist, argv[i]);
        ret |= write_setting_list(setlist);
		return ret;
	}

	if (argc < 1)
		xerrx(EXIT_FAILURE, _("no variables specified\n"
				      "Try `%s --help' for more information."),
		      program_invocation_short_name);
	if (NameOnly && Quiet)
		xerrx(EXIT_FAILURE, _("options -N and -q cannot coexist\n"
				      "Try `%s --help' for more information."),
		      program_invocation_short_name);

	for ( ; *argv; argv++) {
		if (WriteMode || strchr(*argv, '=')) {
            SysctlSetting *s;
            if ( (s = parse_setting_line("command line", 0, *argv)) != NULL)
                ReturnCode |= WriteSetting(s->key, s->path, s->value,
                                           s->ignore_failure);
            else
                ReturnCode |= EXIT_FAILURE;
        } else
			ReturnCode += ReadSetting(*argv);
	}
	return ReturnCode;
}
