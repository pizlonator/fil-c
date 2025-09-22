/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2012 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2012-2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#if defined(HAVE_LIBNCURSES) || defined(HAVE_LIBNCURSESW)
# if defined(HAVE_NCURSESW_NCURSES_H)
#  include <ncursesw/ncurses.h>
# elif defined(HAVE_NCURSES_NCURSES_H)
#  include <ncurses/ncurses.h>
# elif defined(HAVE_NCURSES_H)
#  include <ncurses.h>
# endif
# if defined(HAVE_NCURSESW_TERM_H)
#  include <ncursesw/term.h>
# elif defined(HAVE_NCURSES_TERM_H)
#  include <ncurses/term.h>
# elif defined(HAVE_TERM_H)
#  include <term.h>
# endif
#endif

#include "c.h"
#include "colors.h"
#include "pathnames.h"
#include "strutils.h"

#include "debug.h"

/*
 * Default behavior, may be overridden by terminal-colors.d/{enable,disable}.
 */
#ifdef USE_COLORS_BY_DEFAULT
# define UL_COLORMODE_DEFAULT	UL_COLORMODE_AUTO	/* check isatty() */
#else
# define UL_COLORMODE_DEFAULT	UL_COLORMODE_NEVER	/* no colors by default */
#endif

/*
 * terminal-colors.d debug stuff
 */
static UL_DEBUG_DEFINE_MASK(termcolors);
UL_DEBUG_DEFINE_MASKNAMES(termcolors) = UL_DEBUG_EMPTY_MASKNAMES;

#define TERMCOLORS_DEBUG_INIT	(1 << 1)
#define TERMCOLORS_DEBUG_CONF	(1 << 2)
#define TERMCOLORS_DEBUG_SCHEME	(1 << 3)
#define TERMCOLORS_DEBUG_ALL	0xFFFF

#define DBG(m, x)       __UL_DBG(termcolors, TERMCOLORS_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(termcolors, TERMCOLORS_DEBUG_, m, x)

/*
 * terminal-colors.d file types
 */
enum {
	UL_COLORFILE_DISABLE,		/* .disable */
	UL_COLORFILE_ENABLE,		/* .enable */
	UL_COLORFILE_SCHEME,		/* .scheme */

	__UL_COLORFILE_COUNT
};

struct ul_color_scheme {
	char *name;
	char *seq;
};

/*
 * Global colors control struct
 *
 * The terminal-colors.d/ evaluation is based on "scores":
 *
 *  filename                    score
 *  ---------------------------------------
 *  type			1
 *  @termname.type		10 + 1
 *  utilname.type		20 + 1
 *  utilname@termname.type	20 + 10 + 1
 *
 * the match with higher score wins. The score is per type.
 */
struct ul_color_ctl {
	const char	*utilname;	/* util name */
	const char	*termname;	/* terminal name ($TERM) */

	char		*sfile;	/* path to scheme */

	struct ul_color_scheme	*schemes;	/* array with color schemes */
	size_t			nschemes;	/* number of the items */
	size_t			schemes_sz;	/* number of the allocated items */

	int		mode;		/* UL_COLORMODE_* */
	unsigned int	has_colors : 1,	/* based on mode and scores[] */
			disabled   : 1, /* disable colors */
			cs_configured : 1, /* color schemes read */
			configured : 1; /* terminal-colors.d parsed */

	int		scores[__UL_COLORFILE_COUNT];	/* the best match */
};

/*
 * Control struct, globally shared.
 */
static struct ul_color_ctl ul_colors;

static void colors_free_schemes(struct ul_color_ctl *cc);
static int colors_read_schemes(struct ul_color_ctl *cc);

/*
 * qsort/bsearch buddy
 */
static int cmp_scheme_name(const void *a0, const void *b0)
{
	const struct ul_color_scheme *a = (const struct ul_color_scheme *) a0,
				     *b = (const struct ul_color_scheme *) b0;
	return strcmp(a->name, b->name);
}

/*
 * Resets control struct (note that we don't allocate the struct)
 */
static void colors_reset(struct ul_color_ctl *cc)
{
	if (!cc)
		return;

	colors_free_schemes(cc);

	free(cc->sfile);

	cc->sfile = NULL;
	cc->utilname = NULL;
	cc->termname = NULL;
	cc->mode = UL_COLORMODE_UNDEF;

	memset(cc->scores, 0, sizeof(cc->scores));
}

static void colors_debug(struct ul_color_ctl *cc)
{
	size_t i;

	if (!cc)
		return;

	printf("Colors:\n");
	printf("\tutilname = '%s'\n", cc->utilname);
	printf("\ttermname = '%s'\n", cc->termname);
	printf("\tscheme file = '%s'\n", cc->sfile);
	printf("\tmode = %s\n",
			cc->mode == UL_COLORMODE_UNDEF ? "undefined" :
			cc->mode == UL_COLORMODE_AUTO ?  "auto" :
			cc->mode == UL_COLORMODE_NEVER ? "never" :
			cc->mode == UL_COLORMODE_ALWAYS ? "always" : "???");
	printf("\thas_colors = %d\n", cc->has_colors);
	printf("\tdisabled = %d\n", cc->disabled);
	printf("\tconfigured = %d\n", cc->configured);
	printf("\tcs configured = %d\n", cc->cs_configured);

	fputc('\n', stdout);

	for (i = 0; i < ARRAY_SIZE(cc->scores); i++)
		printf("\tscore %s = %d\n",
				i == UL_COLORFILE_DISABLE ? "disable" :
				i == UL_COLORFILE_ENABLE ? "enable" :
				i == UL_COLORFILE_SCHEME ? "scheme" : "???",
				cc->scores[i]);

	fputc('\n', stdout);

	for (i = 0; i < cc->nschemes; i++) {
		printf("\tscheme #%02zu ", i);
		color_scheme_enable(cc->schemes[i].name, NULL);
		fputs(cc->schemes[i].name, stdout);
		color_disable();
		fputc('\n', stdout);
	}
	fputc('\n', stdout);
}

/*
 * Parses [[<utilname>][@<termname>].]<type>
 */
static int filename_to_tokens(const char *str,
			      const char **name, size_t *namesz,
			      const char **term, size_t *termsz,
			      int  *filetype)
{
	const char *type_start, *term_start, *p;

	if (!str || !*str || *str == '.' || strlen(str) > PATH_MAX)
		return -EINVAL;

	/* parse .type */
	p = strrchr(str, '.');
	type_start = p ? p + 1 : str;

	if (strcmp(type_start, "disable") == 0)
		*filetype = UL_COLORFILE_DISABLE;
	else if (strcmp(type_start, "enable") == 0)
		*filetype = UL_COLORFILE_ENABLE;
	else if (strcmp(type_start, "scheme") == 0)
		*filetype = UL_COLORFILE_SCHEME;
	else {
		DBG(CONF, ul_debug("unknown type '%s'", type_start));
		return 1;				/* unknown type */
	}

	if (type_start == str)
		return 0;				/* "type" only */

	/* parse @termname */
	p = strchr(str, '@');
	term_start = p ? p + 1 : NULL;
	if (term_start) {
		*term = term_start;
		*termsz = type_start - term_start - 1;
		if (term_start - 1 == str)
			return 0;			/* "@termname.type" */
	}

	/* parse utilname */
	p = term_start ? term_start : type_start;
	*name = str;
	*namesz = p - str - 1;

	return 0;
}

/*
 * Scans @dirname and select the best matches for UL_COLORFILE_* types.
 * The result is stored to cc->scores. The path to the best "scheme"
 * file is stored to cc->scheme.
 */
static int colors_readdir(struct ul_color_ctl *cc, const char *dirname)
{
	DIR *dir;
	int rc = 0;
	struct dirent *d;
	char sfile[PATH_MAX] = { '\0' };
	size_t namesz, termsz;

	if (!dirname || !cc || !cc->utilname || !*cc->utilname)
		return -EINVAL;

	DBG(CONF, ul_debug("reading dir: '%s'", dirname));

	dir = opendir(dirname);
	if (!dir)
		return -errno;

	namesz = strlen(cc->utilname);
	termsz = cc->termname ? strlen(cc->termname) : 0;

	while ((d = readdir(dir))) {
		int type, score = 1;
		const char *tk_name = NULL, *tk_term = NULL;
		size_t tk_namesz = 0, tk_termsz = 0;

		if (*d->d_name == '.')
			continue;
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type != DT_UNKNOWN && d->d_type != DT_LNK &&
		    d->d_type != DT_REG)
			continue;
#endif
		if (filename_to_tokens(d->d_name,
				       &tk_name, &tk_namesz,
				       &tk_term, &tk_termsz, &type) != 0)
			continue;

		/* count theoretical score before we check names to avoid
		 * unnecessary strcmp() */
		if (tk_name)
			score += 20;
		if (tk_term)
			score += 10;

		DBG(CONF, ul_debug("item '%s': score=%d "
			"[cur: %d, name(%zu): %s, term(%zu): %s]",
			d->d_name, score, cc->scores[type],
			tk_namesz, tk_name,
			tk_termsz, tk_term));


		if (score < cc->scores[type])
			continue;

		/* filter out by names */
		if (tk_namesz && (tk_namesz != namesz ||
				 strncmp(tk_name, cc->utilname, namesz) != 0))
			continue;

		if (tk_termsz && (termsz == 0 || tk_termsz != termsz ||
				  strncmp(tk_term, cc->termname, termsz) != 0))
			continue;

		DBG(CONF, ul_debug("setting '%s' from %d -to-> %d",
					type == UL_COLORFILE_SCHEME ? "scheme" :
					type == UL_COLORFILE_DISABLE ? "disable" :
					type == UL_COLORFILE_ENABLE ? "enable" : "???",
					cc->scores[type], score));
		cc->scores[type] = score;
		if (type == UL_COLORFILE_SCHEME)
			strncpy(sfile, d->d_name, sizeof(sfile));
	}

	if (*sfile) {
		sfile[sizeof(sfile) - 1] = '\0';
		if (asprintf(&cc->sfile, "%s/%s", dirname, sfile) <= 0)
			rc = -ENOMEM;
	} else {
		rc = -ENOENT;
	}

	closedir(dir);
	return rc;
}

/* atexit() wrapper */
static void colors_deinit(void)
{
	colors_reset(&ul_colors);
}

/*
 * Returns path to $XDG_CONFIG_HOME/terminal-colors.d
 */
static char *colors_get_homedir(char *buf, size_t bufsz)
{
	char *p = getenv("XDG_CONFIG_HOME");

	if (p) {
		snprintf(buf, bufsz, "%s/" _PATH_TERMCOLORS_DIRNAME, p);
		return buf;
	}

	p = getenv("HOME");
	if (p) {
		snprintf(buf, bufsz, "%s/.config/" _PATH_TERMCOLORS_DIRNAME, p);
		return buf;
	}

	return NULL;
}

/*
 * Adds one color sequence to array with color scheme.
 */
static int colors_add_scheme(struct ul_color_ctl *cc,
			     const char *name,
			     const char *seq0)
{
	struct ul_color_scheme *cs = NULL;
	char *seq = NULL;
	int rc;

	if (!cc || !name || !*name || !seq0 || !*seq0)
		return -EINVAL;

	DBG(SCHEME, ul_debug("add '%s'", name));

	seq = color_get_sequence(seq0);
	if (!seq)
		return -EINVAL;
	rc = -ENOMEM;

	/* enlarge the array */
	if (cc->nschemes == cc->schemes_sz) {
		void *tmp = reallocarray(cc->schemes, cc->nschemes + 10,
					 sizeof(struct ul_color_scheme));
		if (!tmp)
			goto err;
		cc->schemes = tmp;
		cc->schemes_sz = cc->nschemes + 10;
	}

	/* add a new item */
	cs = &cc->schemes[cc->nschemes];
	cs->seq = seq;
	cs->name = strdup(name);
	if (!cs->name)
		goto err;

	cc->nschemes++;
	return 0;
err:
	if (cs) {
		free(cs->seq);
		free(cs->name);
		cs->seq = cs->name = NULL;
	} else
		free(seq);
	return rc;
}

/*
 * Deallocates all regards to color schemes
 */
static void colors_free_schemes(struct ul_color_ctl *cc)
{
	size_t i;

	DBG(SCHEME, ul_debug("free scheme"));

	for (i = 0; i < cc->nschemes; i++) {
		free(cc->schemes[i].name);
		free(cc->schemes[i].seq);
	}

	free(cc->schemes);
	cc->schemes = NULL;
	cc->nschemes = 0;
	cc->schemes_sz = 0;
}

/*
 * The scheme configuration has to be sorted for bsearch
 */
static void colors_sort_schemes(struct ul_color_ctl *cc)
{
	if (!cc->nschemes)
		return;

	DBG(SCHEME, ul_debug("sort scheme"));

	qsort(cc->schemes, cc->nschemes,
	      sizeof(struct ul_color_scheme), cmp_scheme_name);
}

/*
 * Returns just one color scheme
 */
static struct ul_color_scheme *colors_get_scheme(struct ul_color_ctl *cc,
						 const char *name)
{
	struct ul_color_scheme key = { .name = (char *) name}, *res;

	if (!cc || !name || !*name)
		return NULL;

	if (!cc->cs_configured) {
		int rc = colors_read_schemes(cc);
		if (rc)
			return NULL;
	}
	if (!cc->nschemes)
		return NULL;

	DBG(SCHEME, ul_debug("search '%s'", name));

	res = bsearch(&key, cc->schemes, cc->nschemes,
				sizeof(struct ul_color_scheme),
				cmp_scheme_name);

	return res && res->seq ? res  : NULL;
}

/*
 * Parses filenames in terminal-colors.d
 */
static int colors_read_configuration(struct ul_color_ctl *cc)
{
	int rc = -ENOENT;
	char *dirname, buf[PATH_MAX];

	cc->termname = getenv("TERM");

	dirname = colors_get_homedir(buf, sizeof(buf));
	if (dirname)
		rc = colors_readdir(cc, dirname);		/* ~/.config */
	if (rc == -EPERM || rc == -EACCES || rc == -ENOENT)
		rc = colors_readdir(cc, _PATH_TERMCOLORS_DIR);	/* /etc */

	cc->configured = 1;
	return rc;
}

/*
 * Reads terminal-colors.d/ scheme file into array schemes
 */
static int colors_read_schemes(struct ul_color_ctl *cc)
{
	int rc = 0;
	FILE *f = NULL;
	char buf[BUFSIZ],
	     cn[129], seq[129];

	if (!cc->configured)
		rc = colors_read_configuration(cc);

	cc->cs_configured = 1;

	if (rc || !cc->sfile)
		goto done;

	DBG(SCHEME, ul_debug("reading file '%s'", cc->sfile));

	f = fopen(cc->sfile, "r");
	if (!f) {
		rc = -errno;
		goto done;
	}

	while (fgets(buf, sizeof(buf), f)) {
		char *p = strchr(buf, '\n');

		if (!p) {
			if (feof(f))
				p = strchr(buf, '\0');
			else {
				rc = -errno;
				goto done;
			}
		}
		*p = '\0';
		p = (char *) skip_blank(buf);
		if (*p == '\0' || *p == '#')
			continue;

		rc = sscanf(p, "%128[^ ] %128[^\n ]", cn, seq);
		if (rc == 2 && *cn && *seq) {
			rc = colors_add_scheme(cc, cn, seq);	/* set rc=0 on success */
			if (rc)
				goto done;
		}
	}
	rc = 0;

done:
	if (f)
		fclose(f);
	colors_sort_schemes(cc);

	return rc;
}


static void termcolors_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(termcolors, TERMCOLORS_DEBUG_, 0, TERMINAL_COLORS_DEBUG);
}

static int colors_terminal_is_ready(void)
{
	int ncolors = -1;

#if defined(HAVE_LIBNCURSES) || defined(HAVE_LIBNCURSESW)
	{
		int ret;

		if (setupterm(NULL, STDOUT_FILENO, &ret) == 0 && ret == 1)
			ncolors = tigetnum("colors");
	}
#endif
	if (1 < ncolors) {
		DBG(CONF, ul_debug("terminal is ready (supports %d colors)", ncolors));
		return 1;
	}

	DBG(CONF, ul_debug("terminal is NOT ready (no colors)"));
	return 0;
}

/**
 * colors_init:
 * @mode: UL_COLORMODE_*
 * @name: util argv[0]
 *
 * Initialize private color control struct and initialize the colors
 * status. The color schemes are parsed on demand by colors_get_scheme().
 *
 * Returns: >0 on success.
 */
int colors_init(int mode, const char *name)
{
	int ready = -1;
	struct ul_color_ctl *cc = &ul_colors;

	cc->utilname = name;

	termcolors_init_debug();

	if (mode != UL_COLORMODE_ALWAYS && !isatty(STDOUT_FILENO))
		cc->mode = UL_COLORMODE_NEVER;
	else
		cc->mode = mode;

	if (cc->mode == UL_COLORMODE_UNDEF
	    && (ready = colors_terminal_is_ready())) {
		int rc = colors_read_configuration(cc);
		if (rc)
			cc->mode = UL_COLORMODE_DEFAULT;
		else {

			/* evaluate scores */
			if (cc->scores[UL_COLORFILE_DISABLE] >
			    cc->scores[UL_COLORFILE_ENABLE])
				cc->mode = UL_COLORMODE_NEVER;
			else
				cc->mode = UL_COLORMODE_DEFAULT;

			atexit(colors_deinit);
		}
	}

	switch (cc->mode) {
	case UL_COLORMODE_AUTO:
		cc->has_colors = ready == -1 ? colors_terminal_is_ready() : ready;
		break;
	case UL_COLORMODE_ALWAYS:
		cc->has_colors = 1;
		break;
	case UL_COLORMODE_NEVER:
	default:
		cc->has_colors = 0;
	}

	ON_DBG(CONF, colors_debug(cc));

	return cc->has_colors;
}

/*
 * Temporary disable colors (this setting is independent on terminal-colors.d/)
 */
void colors_off(void)
{
	ul_colors.disabled = 1;
}

/*
 * Enable colors
 */
void colors_on(void)
{
	ul_colors.disabled = 0;
}

/*
 * Is terminal-colors.d/ configured to use colors?
 */
int colors_wanted(void)
{
	return ul_colors.has_colors;
}

/*
 * Returns mode
 */
int colors_mode(void)
{
	return ul_colors.mode;
}

/*
 * Enable @seq color
 */
void color_fenable(const char *seq, FILE *f)
{
	if (!ul_colors.disabled && ul_colors.has_colors && seq)
		fputs(seq, f);
}

/*
 * Returns escape sequence by logical @name, if undefined then returns @dflt.
 */
const char *color_scheme_get_sequence(const char *name, const char *dflt)
{
	struct ul_color_scheme *cs;

	if (ul_colors.disabled || !ul_colors.has_colors)
		return NULL;

	cs = colors_get_scheme(&ul_colors, name);
	return cs && cs->seq ? cs->seq : dflt;
}

/*
 * Enable color by logical @name, if undefined enable @dflt.
 */
void color_scheme_fenable(const char *name, const char *dflt, FILE *f)
{
	const char *seq = color_scheme_get_sequence(name, dflt);

	if (!seq)
		return;
	color_fenable(seq, f);
}


/*
 * Disable previously enabled color
 */
void color_fdisable(FILE *f)
{
	if (!ul_colors.disabled && ul_colors.has_colors)
		fputs(UL_COLOR_RESET, f);
}

/*
 * Get reset sequence
 */
const char *color_get_disable_sequence(void)
{
	if (!ul_colors.disabled && ul_colors.has_colors)
		return UL_COLOR_RESET;
	else
		return "";
}

/*
 * Parses @str to return UL_COLORMODE_*
 */
int colormode_from_string(const char *str)
{
	size_t i;
	static const char *modes[] = {
		[UL_COLORMODE_AUTO]   = "auto",
		[UL_COLORMODE_NEVER]  = "never",
		[UL_COLORMODE_ALWAYS] = "always",
		[UL_COLORMODE_UNDEF] = ""
	};

	if (!str || !*str)
		return -EINVAL;

	assert(ARRAY_SIZE(modes) == __UL_NCOLORMODES);

	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		if (strcasecmp(str, modes[i]) == 0)
			return i;
	}

	return -EINVAL;
}

/*
 * Parses @str and exit(EXIT_FAILURE) on error
 */
int colormode_or_err(const char *str, const char *errmsg)
{
	const char *p = str && *str == '=' ? str + 1 : str;
	int colormode;

	colormode = colormode_from_string(p);
	if (colormode < 0)
		errx(EXIT_FAILURE, "%s: '%s'", errmsg, p);

	return colormode;
}

#ifdef TEST_PROGRAM_COLORS
# include <getopt.h>
int main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "mode",	required_argument, NULL, 'm' },
		{ "color",	required_argument, NULL, 'c' },
		{ "color-scheme", required_argument, NULL, 'C' },
		{ "name",	required_argument, NULL, 'n' },
		{ NULL, 0, NULL, 0 }
	};
	int c, mode = UL_COLORMODE_UNDEF;	/* default */
	const char *color = "red", *name = NULL, *color_scheme = NULL;
	char *seq = NULL;

	while ((c = getopt_long(argc, argv, "C:c:m:n:", longopts, NULL)) != -1) {
		switch (c) {
		case 'c':
			color = optarg;
			break;
		case 'C':
			color_scheme = optarg;
			break;
		case 'm':
			mode = colormode_or_err(optarg, "unsupported color mode");
			break;
		case 'n':
			name = optarg;
			break;
		default:
			fprintf(stderr, "usage: %s [options]\n"
			" -m, --mode <auto|never|always>  default is undefined\n"
			" -c, --color <red|blue|...>      color for the test message\n"
			" -C, --color-scheme <name>       color for the test message\n"
			" -n, --name <utilname>           util name\n",
			program_invocation_short_name);
			return EXIT_FAILURE;
		}
	}

	colors_init(mode, name ? name : program_invocation_short_name);

	if (color_is_sequence(color))
		seq = strdup(color);
	else
		seq = color_get_sequence(color);

	if (color_scheme)
		color_scheme_enable(color_scheme, seq);
	else
		color_enable(seq);
	printf("Hello World!");
	color_disable();
	fputc('\n', stdout);

	free(seq);

	return EXIT_SUCCESS;
}
#endif /* TEST_PROGRAM_COLORS */

