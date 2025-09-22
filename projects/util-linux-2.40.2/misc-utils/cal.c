/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kim Letkeman.
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
 */

/* 1999-02-01	Jean-Francois Bignolles: added option '-m' to display
 *		monday as the first day of the week.
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2000-09-01  Michael Charles Pruznick <dummy@netwiz.net>
 *             Added "-3" option to print prev/next month with current.
 *             Added overridable default MONTHS_IN_ROW and "-1" option to
 *             get traditional output when -3 is the default.  I hope that
 *             enough people will like -3 as the default that one day the
 *             product can be shipped that way.
 *
 * 2001-05-07  Pablo Saratxaga <pablo@mandrakesoft.com>
 *             Fixed the bugs with multi-byte charset (zg: cjk, utf-8)
 *             displaying. made the 'month year' ("%s %d") header translatable
 *             so it can be adapted to conventions used by different languages
 *             added support to read "first_weekday" locale information
 *             still to do: support for 'cal_direction' (will require a major
 *             rewrite of the displaying) and proper handling of RTL scripts
 */

#include <sys/types.h>

#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "c.h"
#include "closestream.h"
#include "colors.h"
#include "nls.h"
#include "mbsalign.h"
#include "strutils.h"
#include "optutils.h"
#include "timeutils.h"
#include "ttyutils.h"
#include "xalloc.h"

#define DOY_MONTH_WIDTH	27	/* -j month width */
#define DOM_MONTH_WIDTH	20	/* month width */

enum {
	CAL_COLOR_TODAY,
	CAL_COLOR_HEADER,
	CAL_COLOR_WEEKNUMBER,
	CAL_COLOR_WORKDAY,
	CAL_COLOR_WEEKEND
};

static const struct { const char * const scheme; const char * dflt; } colors[] =
{
        [CAL_COLOR_TODAY]      = { "today",      UL_COLOR_REVERSE },
        [CAL_COLOR_WEEKNUMBER] = { "weeknumber", UL_COLOR_REVERSE },
        [CAL_COLOR_HEADER]     = { "header",     ""               },
	[CAL_COLOR_WORKDAY]    = { "workday",    ""               },
	[CAL_COLOR_WEEKEND]    = { "weekend",    ""               }
};

static inline void cal_enable_color(int id)
{
	color_scheme_enable(colors[id].scheme, colors[id].dflt);
}

static inline const char *cal_get_color_sequence(int id)
{
	return color_scheme_get_sequence(colors[id].scheme, colors[id].dflt);
}

static inline void cal_disable_color(int id)
{
	const char *seq = cal_get_color_sequence(id);
	if (seq && seq[0])
		color_disable();
}

static inline const char *cal_get_color_disable_sequence(int id)
{
	const char *seq = cal_get_color_sequence(id);
	if (seq && seq[0])
		return UL_COLOR_RESET;
	else
		return "";
}

#include "widechar.h"

enum {
	GREGORIAN		= INT32_MIN,
	ISO			= INT32_MIN,
	GB1752			= 1752,
	DEFAULT_REFORM_YEAR	= 1752,
	JULIAN			= INT32_MAX
};

enum {
	SUNDAY = 0,
	MONDAY,
	TUESDAY,
	WEDNESDAY,
	THURSDAY,
	FRIDAY,
	SATURDAY,
	DAYS_IN_WEEK,
	NONEDAY
};

enum {
	JANUARY = 1,
	FEBRUARY,
	MARCH,
	APRIL,
	MAY,
	JUNE,
	JULY,
	AUGUST,
	SEPTEMBER,
	OCTOBER,
	NOVEMBER,
	DECEMBER
};

#define REFORMATION_MONTH	SEPTEMBER
#define	NUMBER_MISSING_DAYS	11		/* 11 day correction */
#define YDAY_AFTER_MISSING	258             /* 14th in Sep 1752 */

#define MONTHS_IN_YEAR		DECEMBER
#define DAYS_IN_MONTH		31
#define	MAXDAYS			42		/* slots in a month array */
#define	SPACE			-1		/* used in day array */

#define SMALLEST_YEAR		1

#define	DAY_LEN			3		/* 3 spaces per day */
#define	WEEK_LEN		(DAYS_IN_WEEK * DAY_LEN)
#define MONTHS_IN_YEAR_ROW	3		/* month columns in year view */
#define WNUM_LEN                3

#define FMT_ST_CHARS 300	/* 90 suffices in most locales */

static const int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

enum {
	WEEK_NUM_DISABLED = 0,
	WEEK_NUM_MASK=0xff,
	WEEK_NUM_ISO=0x100,
	WEEK_NUM_US=0x200,
};

enum {
	COLUMNS_MAX_THREE = -1,
	COLUMNS_AUTO = -2,
};

/* utf-8 can have up to 6 bytes per char; and an extra byte for ending \0 */
static char day_headings[(WEEK_LEN + 1) * 6 + 1];

struct cal_request {
	int day;
	int month;
	int32_t year;
	int week;
	int start_month;
};

struct cal_control {
	const char *full_month[MONTHS_IN_YEAR];	/* month names */
	const char *abbr_month[MONTHS_IN_YEAR];	/* abbreviated month names */
	const char *weekdays[DAYS_IN_WEEK];     /* day names */

	int reform_year;		/* Gregorian reform year */
	int colormode;			/* day and week number highlight */
	int num_months;			/* number of requested months */
	int span_months;		/* span the date */
	int months_in_row;		/* number of months horizontally in print out */
	int weekstart;			/* day the week starts, often Sun or Mon */
	int weektype;			/* WEEK_TYPE_{NONE,ISO,US} */
	size_t day_width;		/* day width in characters in printout */
	size_t week_width;		/* 7 * day_width + possible week num */
	size_t month_width;		/* width of a month (vertical mode) */
	int gutter_width;		/* spaces in between horizontal month outputs */
	struct cal_request req;		/* the times user is interested */
	unsigned int	julian:1,	/* julian output */
			header_year:1,	/* print year number */
			header_hint:1,	/* does month name + year need two lines to fit */
			vertical:1;	/* display the output in vertical */
};

struct cal_month {
	int days[MAXDAYS];		/* the day numbers, or SPACE */
	int weeks[MAXDAYS / DAYS_IN_WEEK];
	int month;
	int32_t year;
	struct cal_month *next;
};
/* function prototypes */
static int leap_year(const struct cal_control *ctl, int32_t year);
static int monthname_to_number(struct cal_control *ctl, const char *name);
static void weekdays_init(struct cal_control *ctl);
static void headers_init(struct cal_control *ctl);
static void cal_fill_month(struct cal_month *month, const struct cal_control *ctl);
static void cal_output_header(struct cal_month *month, const struct cal_control *ctl);
static void cal_output_months(struct cal_month *month, const struct cal_control *ctl);
static void cal_vert_output_months(struct cal_month *month, const struct cal_control *ctl);
static void monthly(const struct cal_control *ctl);
static void yearly(const struct cal_control *ctl);
static int day_in_year(const struct cal_control *ctl, int day,
		       int month, int32_t year);
static int day_in_week(const struct cal_control *ctl, int day,
		       int month, int32_t year);
static int week_number(int day, int month, int32_t year, const struct cal_control *ctl);
static int week_to_day(const struct cal_control *ctl);
static int center_str(const char *src, char *dest, size_t dest_size, size_t width);
static void center(const char *str, size_t len, int separate);
static int left_str(const char *src, char *dest, size_t dest_size, size_t width);
static void left(const char *str, size_t len, int separate);
static int parse_reform_year(const char *reform_year);
static void __attribute__((__noreturn__)) usage(void);

#ifdef TEST_CAL
static time_t cal_time(time_t *t)
{
	char *str = getenv("CAL_TEST_TIME");

	if (str) {
		uint64_t x = strtou64_or_err(str, "failed to parse CAL_TEST_TIME");

		*t = x;
		return *t;
	}

	return time(t);
}
#else
# define cal_time(t)	time(t)
#endif

int main(int argc, char **argv)
{
	struct tm local_time;
	time_t now;
	int ch = 0, yflag = 0, Yflag = 0, cols = COLUMNS_MAX_THREE;

	static struct cal_control ctl = {
		.reform_year = DEFAULT_REFORM_YEAR,
		.weekstart = SUNDAY,
		.span_months = 0,
		.colormode = UL_COLORMODE_UNDEF,
		.weektype = WEEK_NUM_DISABLED,
		.day_width = DAY_LEN,
		.gutter_width = 2,
		.req.day = 0,
		.req.month = 0
	};

	enum {
		OPT_COLOR = CHAR_MAX + 1,
		OPT_ISO,
		OPT_REFORM
	};

	static const struct option longopts[] = {
		{"one", no_argument, NULL, '1'},
		{"three", no_argument, NULL, '3'},
		{"sunday", no_argument, NULL, 's'},
		{"monday", no_argument, NULL, 'm'},
		{"julian", no_argument, NULL, 'j'},
		{"months", required_argument, NULL, 'n'},
		{"span", no_argument, NULL, 'S'},
		{"year", no_argument, NULL, 'y'},
		{"week", optional_argument, NULL, 'w'},
		{"color", optional_argument, NULL, OPT_COLOR},
		{"reform", required_argument, NULL, OPT_REFORM},
		{"iso", no_argument, NULL, OPT_ISO},
		{"version", no_argument, NULL, 'V'},
		{"twelve", no_argument, NULL, 'Y'},
		{"help", no_argument, NULL, 'h'},
		{"vertical", no_argument, NULL,'v'},
		{"columns", required_argument, NULL,'c'},
		{NULL, 0, NULL, 0}
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'Y','n','y' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

/*
 * The traditional Unix cal utility starts the week at Sunday,
 * while ISO 8601 starts at Monday. We read the start day from
 * the locale database, which can be overridden with the
 * -s (Sunday) or -m (Monday) options.
 */
#if HAVE_DECL__NL_TIME_WEEK_1STDAY
	/*
	 * You need to use 2 locale variables to get the first day of the week.
	 * This is needed to support first_weekday=2 and first_workday=1 for
	 * the rare case where working days span across 2 weeks.
	 * This shell script shows the combinations and calculations involved:
	 *
	 * for LANG in en_US ru_RU fr_FR csb_PL POSIX; do
	 *   printf "%s:\t%s + %s -1 = " $LANG $(locale week-1stday first_weekday)
	 *   date -d"$(locale week-1stday) +$(($(locale first_weekday)-1))day" +%w
	 * done
	 *
	 * en_US:  19971130 + 1 -1 = 0  #0 = sunday
	 * ru_RU:  19971130 + 2 -1 = 1
	 * fr_FR:  19971201 + 1 -1 = 1
	 * csb_PL: 19971201 + 2 -1 = 2
	 * POSIX:  19971201 + 7 -1 = 0
	 */
	{
		unsigned int wfd;
		union { unsigned int word; char *string; } val;
		val.string = nl_langinfo(_NL_TIME_WEEK_1STDAY);

		wfd = val.word;
		wfd = day_in_week(&ctl, wfd % 100, (wfd / 100) % 100,
				  wfd / (100 * 100));
		ctl.weekstart = (wfd + *nl_langinfo(_NL_TIME_FIRST_WEEKDAY) - 1) % DAYS_IN_WEEK;
	}
#endif
	while ((ch = getopt_long(argc, argv, "13mjn:sSywYvc:Vh", longopts, NULL)) != -1) {

		err_exclusive_options(ch, longopts, excl, excl_st);

		switch(ch) {
		case '1':
			ctl.num_months = 1;
			break;
		case '3':
			ctl.num_months = 3;
			ctl.span_months = 1;
			break;
		case 's':
			ctl.weekstart = SUNDAY;		/* default */
			break;
		case 'm':
			ctl.weekstart = MONDAY;
			break;
		case 'j':
			ctl.julian = 1;
			ctl.day_width = DAY_LEN + 1;
			break;
		case 'y':
			yflag = 1;
			break;
		case 'Y':
			Yflag = 1;
			break;
		case 'n':
			ctl.num_months = strtou32_or_err(optarg,
						_("invalid month argument"));
			break;
		case 'S':
			ctl.span_months = 1;
			break;
		case 'w':
			if (optarg) {
				ctl.req.week = strtos32_or_err(optarg,
						_("invalid week argument"));
				if (ctl.req.week < 1 || 54 < ctl.req.week)
					errx(EXIT_FAILURE,_("illegal week value: use 1-54"));
			}
			ctl.weektype = WEEK_NUM_US;	/* default per weekstart */
			break;
		case OPT_COLOR:
			ctl.colormode = UL_COLORMODE_AUTO;
			if (optarg)
				ctl.colormode = colormode_or_err(optarg,
						_("unsupported color mode"));
			break;
		case OPT_REFORM:
			ctl.reform_year = parse_reform_year(optarg);
			break;
		case OPT_ISO:
			ctl.reform_year = ISO;
			break;
		case 'v':
			ctl.vertical = 1;
			break;
		case 'c':
			if (strcmp(optarg, "auto") == 0)
				cols = COLUMNS_AUTO;
			else
				cols = strtosize_or_err(optarg,
						_("failed to parse columns"));
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

	if (ctl.weektype) {
		ctl.weektype = ctl.req.week & WEEK_NUM_MASK;
		ctl.weektype |= (ctl.weekstart == MONDAY ? WEEK_NUM_ISO : WEEK_NUM_US);
		ctl.week_width = (ctl.day_width * DAYS_IN_WEEK) + WNUM_LEN;
	} else
		ctl.week_width = ctl.day_width * DAYS_IN_WEEK;
	/*
	 * The day_width includes the space between days,
	 * as there is no leading space, remove 1
	 * */
	ctl.week_width -= 1;

	if (argc == 1 && !isdigit_string(*argv)) {
		usec_t x;
		/* cal <timestamp> */
		if (parse_timestamp(*argv, &x) == 0)
			now = (time_t) (x / 1000000);
		/* cal <monthname> */
		else if ((ctl.req.month = monthname_to_number(&ctl, *argv)) > 0)
			cal_time(&now);	/* this year */
		else
			errx(EXIT_FAILURE, _("failed to parse timestamp or unknown month name: %s"), *argv);
		argc = 0;
	} else
		cal_time(&now);

	localtime_r(&now, &local_time);

	switch(argc) {
	case 3:
		ctl.req.day = strtos32_or_err(*argv++, _("illegal day value"));
		if (ctl.req.day < 1 || DAYS_IN_MONTH < ctl.req.day)
			errx(EXIT_FAILURE, _("illegal day value: use 1-%d"), DAYS_IN_MONTH);
		/* fallthrough */
	case 2:
		if (isdigit(**argv))
			ctl.req.month = strtos32_or_err(*argv++, _("illegal month value: use 1-12"));
		else {
			ctl.req.month = monthname_to_number(&ctl, *argv);
			if (ctl.req.month < 0)
				errx(EXIT_FAILURE, _("unknown month name: %s"), *argv);
			argv++;
		}
		if (ctl.req.month < 1 || MONTHS_IN_YEAR < ctl.req.month)
			errx(EXIT_FAILURE, _("illegal month value: use 1-12"));
		/* fallthrough */
	case 1:
		ctl.req.year = strtos32_or_err(*argv++, _("illegal year value"));
		if (ctl.req.year < SMALLEST_YEAR)
			errx(EXIT_FAILURE, _("illegal year value: use positive integer"));
		if (ctl.req.year == JULIAN)
			errx(EXIT_FAILURE, _("illegal year value"));
		if (ctl.req.day) {
			int dm = days_in_month[leap_year(&ctl, ctl.req.year)]
					      [ctl.req.month];
			if (ctl.req.day > dm)
				errx(EXIT_FAILURE, _("illegal day value: use 1-%d"), dm);
			ctl.req.day = day_in_year(&ctl, ctl.req.day,
						  ctl.req.month, ctl.req.year);
		} else if ((int32_t) (local_time.tm_year + 1900) == ctl.req.year) {
			ctl.req.day = local_time.tm_yday + 1;
		}
		if (!ctl.req.month && !ctl.req.week) {
			ctl.req.month = local_time.tm_mon + 1;
			if (!ctl.num_months)
				yflag = 1;
		}
		break;
	case 0:
		ctl.req.day = local_time.tm_yday + 1;
		ctl.req.year = local_time.tm_year + 1900;
		if (!ctl.req.month)
			ctl.req.month = local_time.tm_mon + 1;
		break;
	default:
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	if (0 < ctl.req.week) {
		int yday = week_to_day(&ctl);
		int leap = leap_year(&ctl, ctl.req.year);
		int m = 1;

		if (yday < 1)
			errx(EXIT_FAILURE, _("illegal week value: year %d "
					     "doesn't have week %d"),
					ctl.req.year, ctl.req.week);
		while (m <= DECEMBER && yday > days_in_month[leap][m])
			yday -= days_in_month[leap][m++];
		if (DECEMBER < m && ctl.weektype & WEEK_NUM_ISO) {
			/* In some years (e.g. 2010 in ISO mode) it's possible
			 * to have a remnant of week 53 starting the year yet
			 * the year in question ends during 52, in this case
			 * we're assuming that early remnant is being referred
			 * to if 53 is given as argument. */
			if (ctl.req.week != week_number(31, DECEMBER, ctl.req.year - 1, &ctl))
				errx(EXIT_FAILURE,
					_("illegal week value: year %d "
					  "doesn't have week %d"),
					ctl.req.year, ctl.req.week);
		}
		if (!ctl.req.month)
			ctl.req.month = MONTHS_IN_YEAR < m ? 1 : m;
	}

	weekdays_init(&ctl);
	headers_init(&ctl);

	if (colors_init(ctl.colormode, "cal") == 0) {
		/* disable */
		ctl.req.day = 0;
		ctl.weektype &= ~WEEK_NUM_MASK;
	}

	if (yflag || Yflag) {
		ctl.gutter_width = 3;
		if (!ctl.num_months)
			ctl.num_months = MONTHS_IN_YEAR;
		if (yflag) {
			ctl.req.start_month = 1;	/* start from Jan */
			ctl.header_year = 1;		/* print year number */
		}
	}

	if (ctl.vertical)
		ctl.gutter_width = 1;

	if (ctl.num_months > 1 && ctl.months_in_row == 0) {
		ctl.months_in_row = MONTHS_IN_YEAR_ROW;		/* default */

		if (cols > 0)
			ctl.months_in_row = cols;
		else if (isatty(STDOUT_FILENO)) {
			int w, mw, extra, new_n;

			w = get_terminal_width(80);
			mw = ctl.julian ? DOY_MONTH_WIDTH : DOM_MONTH_WIDTH;

			if (w < mw)
				w = mw;

			extra = ((w / mw) - 1) * ctl.gutter_width;
			new_n = (w - extra) / mw;

			switch (cols) {
			case COLUMNS_MAX_THREE:
				if (new_n < MONTHS_IN_YEAR_ROW)
					ctl.months_in_row = new_n > 0 ? new_n : 1;
				break;
			case COLUMNS_AUTO:
				ctl.months_in_row = new_n > 0 ? new_n : 1;
				break;
			}
		}
	} else if (!ctl.months_in_row)
		ctl.months_in_row = 1;

	if (!ctl.num_months)
		ctl.num_months = 1;		/* display at least one month */

	if (yflag || Yflag)
		yearly(&ctl);
	else
		monthly(&ctl);

	return EXIT_SUCCESS;
}

/* leap year -- account for gregorian reformation in 1752 */
static int leap_year(const struct cal_control *ctl, int32_t year)
{
	if (year <= ctl->reform_year)
		return !(year % 4);

	return ( !(year % 4) && (year % 100) ) || !(year % 400);
}

static void init_monthnames(struct cal_control *ctl)
{
	size_t i;

	if (ctl->full_month[0] != NULL)
		return;		/* already initialized */

	for (i = 0; i < MONTHS_IN_YEAR; i++)
		ctl->full_month[i] = nl_langinfo(ALTMON_1 + i);
}

static void init_abbr_monthnames(struct cal_control *ctl)
{
	size_t i;

	if (ctl->abbr_month[0] != NULL)
		return;		/* already initialized */

	for (i = 0; i < MONTHS_IN_YEAR; i++)
		ctl->abbr_month[i] = nl_langinfo(_NL_ABALTMON_1 + i);
}

static int monthname_to_number(struct cal_control *ctl, const char *name)
{
	size_t i;

	init_monthnames(ctl);
	for (i = 0; i < MONTHS_IN_YEAR; i++)
		if (strcasecmp(ctl->full_month[i], name) == 0)
			return i + 1;

	init_abbr_monthnames(ctl);
	for (i = 0; i < MONTHS_IN_YEAR; i++)
		if (strcasecmp(ctl->abbr_month[i], name) == 0)
			return i + 1;

	return -EINVAL;
}

static void weekdays_init(struct cal_control *ctl)
{
	size_t wd;
	int i;

	for (i = 0; i < DAYS_IN_WEEK; i++) {
		wd = (i + ctl->weekstart) % DAYS_IN_WEEK;
		ctl->weekdays[i] = nl_langinfo(ABDAY_1 + wd);
	}
}
static void headers_init(struct cal_control *ctl)
{
	size_t i;
	char *cur_dh = day_headings;
	char tmp[FMT_ST_CHARS];
	int year_len;

	year_len = snprintf(tmp, sizeof(tmp), "%04d", ctl->req.year);

	if (year_len < 0 || (size_t)year_len >= sizeof(tmp)) {
		/* XXX impossible error */
		return;
	}

	for (i = 0; i < DAYS_IN_WEEK; i++) {
		size_t space_left;

		space_left = sizeof(day_headings) - (cur_dh - day_headings);
		if (i && space_left)
			strncat(cur_dh++, " ", space_left--);

		if (space_left <= (ctl->day_width - 1))
			break;
		cur_dh += center_str(ctl->weekdays[i], cur_dh,
				     space_left, ctl->day_width - 1);
	}

	init_monthnames(ctl);

	for (i = 0; i < MONTHS_IN_YEAR; i++) {
		/* The +1 after year_len is space in between month and year. */
		if (ctl->week_width < strlen(ctl->full_month[i]) + year_len)
			ctl->header_hint = 1;
	}
}

static void cal_fill_month(struct cal_month *month, const struct cal_control *ctl)
{
	int first_week_day = day_in_week(ctl, 1, month->month, month->year);
	int month_days;
	int i, j, weeklines = 0;

	if (ctl->julian)
		j = day_in_year(ctl, 1, month->month, month->year);
	else
		j = 1;
	month_days = j + days_in_month[leap_year(ctl, month->year)][month->month];

	/* True when Sunday is not first day in the output week. */
	if (ctl->weekstart) {
		first_week_day -= ctl->weekstart;
		if (first_week_day < 0)
			first_week_day = DAYS_IN_WEEK - ctl->weekstart;
		month_days += ctl->weekstart - 1;
	}

	/* Fill day array. */
	for (i = 0; i < MAXDAYS; i++) {
		if (0 < first_week_day) {
			month->days[i] = SPACE;
			first_week_day--;
			continue;
		}
		if (j < month_days) {
			if (month->year == ctl->reform_year &&
			    month->month == REFORMATION_MONTH &&
			    (j == 3 || j == 247))
				j += NUMBER_MISSING_DAYS;
			month->days[i] = j;
			j++;
			continue;
		}
		month->days[i] = SPACE;
		weeklines++;
	}

	/* Add week numbers */
	if (ctl->weektype) {
		int weeknum = week_number(1, month->month, month->year, ctl);
		weeklines = MAXDAYS / DAYS_IN_WEEK - weeklines / DAYS_IN_WEEK;
		for (i = 0; i < MAXDAYS / DAYS_IN_WEEK; i++) {
			if (0 < weeklines) {
				if (52 < weeknum)
					weeknum = week_number(month->days[i * DAYS_IN_WEEK], month->month, month->year, ctl);
				month->weeks[i] = weeknum++;
			} else
				month->weeks[i] = SPACE;
			weeklines--;
		}
	}
}

static void cal_output_header(struct cal_month *month, const struct cal_control *ctl)
{
	char out[FMT_ST_CHARS];
	struct cal_month *i;

	cal_enable_color(CAL_COLOR_HEADER);

	if (ctl->header_hint || ctl->header_year) {
		for (i = month; i; i = i->next) {
			snprintf(out, sizeof(out), "%s", ctl->full_month[i->month - 1]);
			center(out, ctl->week_width, i->next == NULL ? 0 : ctl->gutter_width);
		}
		if (!ctl->header_year) {
			fputc('\n', stdout);
			for (i = month; i; i = i->next) {
				snprintf(out, sizeof(out), "%04d", i->year);
				center(out, ctl->week_width, i->next == NULL ? 0 : ctl->gutter_width);
			}
		}
	} else {
		for (i = month; i; i = i->next) {
			snprintf(out, sizeof(out), "%s %04d", ctl->full_month[i->month - 1], i->year);
			center(out, ctl->week_width, i->next == NULL ? 0 : ctl->gutter_width);
		}
	}
	fputc('\n', stdout);
	for (i = month; i; i = i->next) {
		if (ctl->weektype) {
			if (ctl->julian)
				printf("%*s%s", (int)ctl->day_width - 1, "", day_headings);
			else
				printf("%*s%s", (int)ctl->day_width, "", day_headings);
		} else
			fputs(day_headings, stdout);
		if (i->next != NULL)
			printf("%*s", ctl->gutter_width, "");
	}
	cal_disable_color(CAL_COLOR_HEADER);
	fputc('\n', stdout);
}

static void cal_vert_output_header(struct cal_month *month,
				  const struct cal_control *ctl)
{
	char out[FMT_ST_CHARS];
	struct cal_month *m;
	int month_width;

	month_width = ctl->day_width * (MAXDAYS / DAYS_IN_WEEK);

	/* Padding for the weekdays */
	printf("%*s", (int)ctl->day_width + 1, "");

	if (ctl->header_hint || ctl->header_year) {
		for (m = month; m; m = m->next) {
			snprintf(out, sizeof(out), "%s", ctl->full_month[m->month - 1]);
			left(out, month_width, ctl->gutter_width);
		}
		if (!ctl->header_year) {
			fputc('\n', stdout);
			/* Padding for the weekdays */
			printf("%*s", (int)ctl->day_width + 1, "");

			for (m = month; m; m = m->next) {
				snprintf(out, sizeof(out), "%04d", m->year);
				left(out, month_width, ctl->gutter_width);
			}
		}
	} else {
		for (m = month; m; m = m->next) {
			snprintf(out, sizeof(out), "%s %04d", ctl->full_month[m->month - 1], m->year);
			left(out, month_width, ctl->gutter_width);
		}
	}
	fputc('\n', stdout);
}

#define fput_seq(_s)	do { if ((_s) && *(_s)) fputs((_s), stdout); } while(0)

static void cal_output_months(struct cal_month *month, const struct cal_control *ctl)
{
	int reqday, week_line, d;
	int skip;
	struct cal_month *i;
	int firstwork = ctl->weekstart == SUNDAY ? 1 : 0;	/* first workday in week */

	/* Let's keep sequence cached rather than search it for each day */
	const char *seq_wo_start = cal_get_color_sequence(CAL_COLOR_WORKDAY);
	const char *seq_wo_end = cal_get_color_disable_sequence(CAL_COLOR_WORKDAY);
	const char *seq_we_start = cal_get_color_sequence(CAL_COLOR_WEEKEND);
	const char *seq_we_end = cal_get_color_disable_sequence(CAL_COLOR_WEEKEND);

	for (week_line = 0; week_line < MAXDAYS / DAYS_IN_WEEK; week_line++) {
		for (i = month; i; i = i->next) {
			/* Determine the day that should be highlighted. */
			reqday = 0;
			if (i->month == ctl->req.month && i->year == ctl->req.year) {
				if (ctl->julian)
					reqday = ctl->req.day;
				else
					reqday = ctl->req.day + 1 -
						 day_in_year(ctl, 1, i->month,
							     i->year);
			}

			if (ctl->weektype) {
				if (0 < i->weeks[week_line]) {
					if ((ctl->weektype & WEEK_NUM_MASK) == i->weeks[week_line])
						printf("%s%2d%s",
						       cal_get_color_sequence(CAL_COLOR_WEEKNUMBER),
						       i->weeks[week_line],
						       cal_get_color_disable_sequence(CAL_COLOR_WEEKNUMBER));
					else
						printf("%2d", i->weeks[week_line]);
				} else
					printf("%2s", "");
				skip = ctl->day_width;
			} else
				/* First day of the week is one char narrower than the other days,
				 * unless week number is printed.  */
				skip = ctl->day_width - 1;

			for (d = DAYS_IN_WEEK * week_line;
			     d < DAYS_IN_WEEK * week_line + DAYS_IN_WEEK; d++) {

				int workday = d >= DAYS_IN_WEEK * week_line + firstwork &&
					      d <= DAYS_IN_WEEK * week_line + firstwork + 4;

				if (0 < i->days[d]) {
					fput_seq(workday ? seq_wo_start : seq_we_start);

					if (reqday == i->days[d])
						printf("%*s%s%*d%s",
							skip - (ctl->julian ? 3 : 2),
							"", cal_get_color_sequence(CAL_COLOR_TODAY), (ctl->julian ? 3 : 2),
							i->days[d], cal_get_color_disable_sequence(CAL_COLOR_TODAY));
					else
						printf("%*d", skip, i->days[d]);

					fput_seq(workday ? seq_wo_end : seq_we_end);
				} else
					printf("%*s", skip, "");

				if (skip < (int)ctl->day_width)
					skip++;
			}
			if (i->next != NULL)
				printf("%*s", ctl->gutter_width, "");
		}
		if (i == NULL)
			fputc('\n', stdout);
	}
}

static void
cal_vert_output_months(struct cal_month *month, const struct cal_control *ctl)
{
	int i, reqday, week, d;
	int skip;
	struct cal_month *m;

	skip = ctl->day_width;
	for (i = 0; i < DAYS_IN_WEEK; i++) {
		left(ctl->weekdays[i], ctl->day_width - 1, 0);
		for (m = month; m; m = m->next) {
			reqday = 0;
			if (m->month == ctl->req.month && m->year == ctl->req.year) {
				if (ctl->julian) {
					reqday = ctl->req.day;
				} else {
					reqday = ctl->req.day + 1 -
						 day_in_year(ctl, 1, m->month, m->year);
				}
			}
			for (week = 0; week < MAXDAYS / DAYS_IN_WEEK; week++) {
				d = i + DAYS_IN_WEEK * week;
				if (0 < m->days[d]) {
					if (reqday == m->days[d]) {
						printf("%*s%s%*d%s",
						       skip - (ctl->julian ? 3 : 2),
						       "",
						       cal_get_color_sequence(CAL_COLOR_TODAY),
						       (ctl->julian ? 3 : 2),
						       m->days[d],
						       cal_get_color_disable_sequence(CAL_COLOR_TODAY));
					} else {
						printf("%*d",  skip, m->days[d]);
					}
				} else {
					printf("%*s", skip, "");
				}
				skip = ctl->day_width;
			}
			if (m->next != NULL)
				printf("%*s", ctl->gutter_width, "");
		}
		fputc('\n', stdout);
	}
	if (!ctl->weektype)
		return;

	printf("%*s", (int)ctl->day_width - 1, "");
	for (m = month; m; m = m->next) {
		for (week = 0; week < MAXDAYS / DAYS_IN_WEEK; week++) {
			if (0 < m->weeks[week]) {
				if ((ctl->weektype & WEEK_NUM_MASK) == m->weeks[week])
					printf("%s%*d%s",
						 cal_get_color_sequence(CAL_COLOR_WEEKNUMBER),
						 skip - (ctl->julian ? 3 : 2),
						 m->weeks[week],
						 cal_get_color_disable_sequence(CAL_COLOR_WEEKNUMBER));
				else
					printf("%*d", skip, m->weeks[week]);
			} else
				printf("%*s", skip, "");
		}
		if (m->next != NULL)
			printf("%*s", ctl->gutter_width, "");
	}
	fputc('\n', stdout);
}


static void monthly(const struct cal_control *ctl)
{
	struct cal_month *m, *ms;
	int i, rows, month = ctl->req.start_month ? ctl->req.start_month : ctl->req.month;
	int32_t year = ctl->req.year;

	/* cal -3, cal -Y --span, etc. */
	if (ctl->span_months) {
		int new_month = month - ctl->num_months / 2;
		if (new_month < 1) {
			new_month *= -1;
			year -= (new_month / MONTHS_IN_YEAR) + 1;

			if (new_month > MONTHS_IN_YEAR)
				new_month %= MONTHS_IN_YEAR;
			month = MONTHS_IN_YEAR - new_month;
		} else
			month = new_month;
	}

	ms = xcalloc(ctl->months_in_row, sizeof(*ms));

	for (i = 0; i < ctl->months_in_row - 1; i++)
		ms[i].next = &ms[i + 1];

	rows = (ctl->num_months - 1) / ctl->months_in_row;
	for (i = 0; i < rows + 1 ; i++){
		if (i == rows && ctl->num_months % ctl->months_in_row > 0)
			for (int n = (ctl->num_months % ctl->months_in_row) - 1; n < ctl->months_in_row; n++)
				ms[n].next = NULL;

		for (m = ms; m; m = m->next){
			m->month = month++;
			m->year = year;
			if (MONTHS_IN_YEAR < month) {
				year++;
				month = 1;
			}
			cal_fill_month(m, ctl);
		}
		if (ctl->vertical) {
			if (i > 0)
				fputc('\n', stdout);		/* Add a line between row */

			cal_vert_output_header(ms, ctl);
			cal_vert_output_months(ms, ctl);
		} else {
			cal_output_header(ms, ctl);
			cal_output_months(ms, ctl);
		}
	}
	free(ms);
}

static void yearly(const struct cal_control *ctl)
{
	size_t year_width;

	year_width = (size_t) ctl->months_in_row * ctl->week_width
		     + ((size_t) ctl->months_in_row - 1) * ctl->gutter_width;

	if (ctl->header_year) {
		char out[FMT_ST_CHARS];

		snprintf(out, sizeof(out), "%04d", ctl->req.year);
		center(out, year_width, 0);
		fputs("\n\n", stdout);
	}
	monthly(ctl);
}

/*
 * day_in_year --
 *	return the 1 based day number within the year
 */
static int day_in_year(const struct cal_control *ctl,
		       int day, int month, int32_t year)
{
	int i, leap;

	leap = leap_year(ctl, year);
	for (i = 1; i < month; i++)
		day += days_in_month[leap][i];
	return day;
}

/*
 * day_in_week
 *	return the 0 based day number for any date from 1 Jan. 1 to
 *	31 Dec. 9999.  Assumes the Gregorian reformation eliminates
 *	3 Sep. 1752 through 13 Sep. 1752, and returns invalid weekday
 *	during the period of 11 days.
 */
static int day_in_week(const struct cal_control *ctl, int day,
		       int month, int32_t year)
{
	/*
	* The magic constants in the reform[] array are, in a simplified
	* sense, the remaining days after slicing into one week periods the total
	* days from the beginning of the year to the target month. That is,
	* weeks + reform[] days gets us to the target month. The exception is,
	* that for the months past February 'DOY - 1' must be used.
	*
	*   DoY (Day of Year): total days to the target month
	*
	*   Month            1  2  3  4   5   6   7   8   9  10  11  12
	*   DoY              0 31 59 90 120 151 181 212 243 273 304 334
	*   DoY % 7          0  3
	*   DoY - 1 % 7      - --  2  5   0   3   5   1   4   6   2   4
	*       reform[] = { 0, 3, 2, 5,  0,  3,  5,  1,  4,  6,  2,  4 };
	*
	*  Note: these calculations are for non leap years.
	*/
	static const int reform[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
	static const int old[]    = { 5, 1, 0, 3, 5, 1, 3, 6, 2, 4, 0, 2 };

	if (year != ctl->reform_year + 1)
		year -= month < MARCH;
	else
		year -= (month < MARCH) + 14;
	if (ctl->reform_year < year
	    || (year == ctl->reform_year && REFORMATION_MONTH < month)
	    || (year == ctl->reform_year
		&& month == REFORMATION_MONTH && 13 < day)) {
		return ((int64_t) year + (year / 4)
			- (year / 100) + (year / 400)
			+ reform[month - 1] + day) % DAYS_IN_WEEK;
	}
	if (year < ctl->reform_year
	    || (year == ctl->reform_year && month < REFORMATION_MONTH)
	    || (year == ctl->reform_year && month == REFORMATION_MONTH && day < 3))
		return ((int64_t) year + year / 4 + old[month - 1] + day)
			% DAYS_IN_WEEK;
	return NONEDAY;
}

/*
 * week_number
 *      return the week number of a given date, 1..54.
 *      Supports ISO-8601 and North American modes.
 *      Day may be given as Julian day of the year mode, in which
 *      case the month is disregarded entirely.
 */
static int week_number(int day, int month, int32_t year, const struct cal_control *ctl)
{
	int fday = 0, yday;
	const int wday = day_in_week(ctl, 1, JANUARY, year);

	if (ctl->weektype & WEEK_NUM_ISO)
		fday = wday + (wday >= FRIDAY ? -2 : 5);
	else {
		/* WEEK_NUM_US: Jan 1 is always First week, that may
		 * begin previous year.  That means there is very seldom
		 * more than 52 weeks, */
		fday = wday + 6;
	}
	/* For julian dates the month can be set to 1, the global julian
	 * variable cannot be relied upon here, because we may recurse
	 * internally for 31.12. which would not work. */
	if (day > DAYS_IN_MONTH)
		month = JANUARY;

	yday = day_in_year(ctl, day, month, year);
	if (year == ctl->reform_year && yday >= YDAY_AFTER_MISSING)
		fday -= NUMBER_MISSING_DAYS;

	/* Last year is last year */
	if (yday + fday < DAYS_IN_WEEK)
		return week_number(31, DECEMBER, year - 1, ctl);

	/* Or it could be part of the next year.  The reformation year had less
	 * days than 365 making this check invalid, but reformation year ended
	 * on Sunday and in week 51, so it's ok here. */
	if (ctl->weektype == WEEK_NUM_ISO && yday >= 363
	    && day_in_week(ctl, day, month, year) >= MONDAY
	    && day_in_week(ctl, day, month, year) <= WEDNESDAY
	    && day_in_week(ctl, 31, DECEMBER, year) >= MONDAY
	    && day_in_week(ctl, 31, DECEMBER, year) <= WEDNESDAY)
		return week_number(1, JANUARY, year + 1, ctl);

	return (yday + fday) / DAYS_IN_WEEK;
}

/*
 * week_to_day
 *      return the yday of the first day in a given week inside
 *      the given year. This may be something other than Monday
 *      for ISO-8601 modes. For North American numbering this
 *      always returns a Sunday.
 */
static int week_to_day(const struct cal_control *ctl)
{
	int yday, wday;

	wday = day_in_week(ctl, 1, JANUARY, ctl->req.year);
	yday = ctl->req.week * DAYS_IN_WEEK - wday;

	if (ctl->req.year == ctl->reform_year && yday >= YDAY_AFTER_MISSING)
		yday += NUMBER_MISSING_DAYS;

	if (ctl->weektype & WEEK_NUM_ISO)
		yday -= (wday >= FRIDAY ? -2 : 5);
	else
		yday -= 6;	/* WEEK_NUM_US */
	if (yday <= 0)
		return 1;

	return yday;
}

/*
 * Center string, handling multibyte characters appropriately.
 * In addition if the string is too large for the width it's truncated.
 * The number of trailing spaces may be 1 less than the number of leading spaces.
 */
static int center_str(const char* src, char* dest,
		      size_t dest_size, size_t width)
{
	return mbsalign(src, dest, dest_size, &width,
			MBS_ALIGN_CENTER, MBA_UNIBYTE_FALLBACK);
}

static void center(const char *str, size_t len, int separate)
{
	char lineout[FMT_ST_CHARS];

	center_str(str, lineout, ARRAY_SIZE(lineout), len);
	fputs(lineout, stdout);

	if (separate)
		printf("%*s", separate, "");
}
static int left_str(const char* src, char* dest,
		    size_t dest_size, size_t width)
{
	return mbsalign(src, dest, dest_size, &width,
			MBS_ALIGN_LEFT, MBA_UNIBYTE_FALLBACK);
}

static void left(const char *str, size_t len, int separate)
{
	char lineout[FMT_ST_CHARS];

	left_str(str, lineout, sizeof(lineout), len);
	fputs(lineout, stdout);

	if (separate)
		printf("%*s", separate, "");
}

static int parse_reform_year(const char *reform_year)
{
	size_t i;

	struct reform {
		char *name;
		int val;
	};

	struct reform years[] = {
	{"gregorian",	GREGORIAN},
	{"iso",		ISO},
	{"1752",	GB1752},
	{"julian",	JULIAN},
	};

	for (i = 0; i < ARRAY_SIZE(years); i++) {
		if (strcasecmp(reform_year, years[i].name) == 0)
			return years[i].val;
	}
	errx(EXIT_FAILURE, "invalid --reform value: '%s'", reform_year);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [[[day] month] year]\n"), program_invocation_short_name);
	fprintf(out, _(" %s [options] <timestamp|monthname>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display a calendar, or some part of it.\n"), out);
	fputs(_("Without any arguments, display the current month.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -1, --one             show only a single month (default)\n"), out);
	fputs(_(" -3, --three           show three months spanning the date\n"), out);
	fputs(_(" -n, --months <num>    show num months starting with date's month\n"), out);
	fputs(_(" -S, --span            span the date when displaying multiple months\n"), out);
	fputs(_(" -s, --sunday          Sunday as first day of week\n"), out);
	fputs(_(" -m, --monday          Monday as first day of week\n"), out);
	fputs(_(" -j, --julian          use day-of-year for all calendars\n"), out);
	fputs(_("     --reform <val>    Gregorian reform date (1752|gregorian|iso|julian)\n"), out);
	fputs(_("     --iso             alias for --reform=iso\n"), out);
	fputs(_(" -y, --year            show the whole year\n"), out);
	fputs(_(" -Y, --twelve          show the next twelve months\n"), out);
	fputs(_(" -w, --week[=<num>]    show US or ISO-8601 week numbers\n"), out);
	fputs(_(" -v, --vertical        show day vertically instead of line\n"), out);
	fputs(_(" -c, --columns <width> amount of columns to use\n"), out);
	fprintf(out,
	      _("     --color[=<when>]  colorize messages (%s, %s or %s)\n"), "auto", "always", "never");
	fprintf(out,
	        "                         %s\n", USAGE_COLORS_DEFAULT);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(23));
	fprintf(out, USAGE_MAN_TAIL("cal(1)"));

	exit(EXIT_SUCCESS);
}
