/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_NLS_H
#define UTIL_LINUX_NLS_H

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#else
# undef setlocale
# define setlocale(Category, Locale) /* empty */
struct lconv
{
	char *decimal_point;
};
# undef localeconv
# define localeconv() NULL
#endif


#ifdef ENABLE_NLS
# include <libintl.h>
/*
 * For NLS support in the public shared libraries we have to specify text
 * domain name to be independent on the main program. For this purpose define
 * UL_TEXTDOMAIN_EXPLICIT before you include nls.h to your shared library code.
 */
# ifdef UL_TEXTDOMAIN_EXPLICIT
#  define _(Text) dgettext (UL_TEXTDOMAIN_EXPLICIT, Text)
# else
#  define _(Text) gettext (Text)
# endif
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
# define P_(Singular, Plural, n) ngettext (Singular, Plural, n)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) (Text)
# define N_(Text) (Text)
# define P_(Singular, Plural, n) ((n) == 1 ? (Singular) : (Plural))
#endif /* ENABLE_NLS */

#ifdef HAVE_LANGINFO_H
# include <langinfo.h>
#else

typedef int nl_item;
extern char *langinfo_fallback(nl_item item);

# define nl_langinfo	langinfo_fallback

enum {
	CODESET = 1,
	RADIXCHAR,
	THOUSEP,
	D_T_FMT,
	D_FMT,
	T_FMT,
	T_FMT_AMPM,
	AM_STR,
	PM_STR,

	DAY_1,
	DAY_2,
	DAY_3,
	DAY_4,
	DAY_5,
	DAY_6,
	DAY_7,

	ABDAY_1,
	ABDAY_2,
	ABDAY_3,
	ABDAY_4,
	ABDAY_5,
	ABDAY_6,
	ABDAY_7,

	MON_1,
	MON_2,
	MON_3,
	MON_4,
	MON_5,
	MON_6,
	MON_7,
	MON_8,
	MON_9,
	MON_10,
	MON_11,
	MON_12,

	ABMON_1,
	ABMON_2,
	ABMON_3,
	ABMON_4,
	ABMON_5,
	ABMON_6,
	ABMON_7,
	ABMON_8,
	ABMON_9,
	ABMON_10,
	ABMON_11,
	ABMON_12,

	ERA_D_FMT,
	ERA_D_T_FMT,
	ERA_T_FMT,
	ALT_DIGITS,
	CRNCYSTR,
	YESEXPR,
	NOEXPR
};

#endif /* !HAVE_LANGINFO_H */

#ifndef HAVE_LANGINFO_ALTMON
# define ALTMON_1 MON_1
# define ALTMON_2 MON_2
# define ALTMON_3 MON_3
# define ALTMON_4 MON_4
# define ALTMON_5 MON_5
# define ALTMON_6 MON_6
# define ALTMON_7 MON_7
# define ALTMON_8 MON_8
# define ALTMON_9 MON_9
# define ALTMON_10 MON_10
# define ALTMON_11 MON_11
# define ALTMON_12 MON_12
#endif /* !HAVE_LANGINFO_ALTMON */

#ifndef HAVE_LANGINFO_NL_ABALTMON
# define _NL_ABALTMON_1 ABMON_1
# define _NL_ABALTMON_2 ABMON_2
# define _NL_ABALTMON_3 ABMON_3
# define _NL_ABALTMON_4 ABMON_4
# define _NL_ABALTMON_5 ABMON_5
# define _NL_ABALTMON_6 ABMON_6
# define _NL_ABALTMON_7 ABMON_7
# define _NL_ABALTMON_8 ABMON_8
# define _NL_ABALTMON_9 ABMON_9
# define _NL_ABALTMON_10 ABMON_10
# define _NL_ABALTMON_11 ABMON_11
# define _NL_ABALTMON_12 ABMON_12
#endif /* !HAVE_LANGINFO_NL_ABALTMON */

#endif /* UTIL_LINUX_NLS_H */
