/*
 * setleds.c - aeb, 940130, 940909, 991008
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sysexits.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

#include "libcommon.h"

/**
 * BITWISE_NOT is the bitwise complement of x with cancelled out integer
 * promotions.
 */
#define BITWISE_NOT(x) (_Generic((x), \
	unsigned char: (unsigned char)(~(x)) \
))

#define BITMASK_UNSET(x, mask) ((x) & BITWISE_NOT(mask))
#define BITMASK_SET(x, mask) ((x) | (mask))

static void __attribute__((noreturn))
usage(int rc, const struct kbd_help *options)
{
	const struct kbd_help *h;
	fprintf(stderr, _("Usage: %s [option...] [[+|-][ num | caps | scroll %s]]\n"),
			get_progname(),
#ifdef __sparc__
	        "| compose "
#else
	        ""
#endif
		);
	fprintf(stderr, "\n");
	fprintf(stderr, _("Thus,\n"
	                  "	setleds +caps -num\n"
	                  "will set CapsLock, clear NumLock and leave ScrollLock unchanged.\n"
	                  "The settings before and after the change (if any) are reported\n"
	                  "when the -v option is given or when no change is requested.\n"
	                  "Normally, setleds influences the vt flag settings\n"
	                  "(and these are usually reflected in the leds).\n"
	                  "With -L, setleds only sets the leds, and leaves the flags alone.\n"
	                  "With -D, setleds sets both the flags and the default flags, so\n"
	                  "that a subsequent reset will not change the flags.\n"));

	if (options) {
		int max = 0;

		fprintf(stderr, "\n");
		fprintf(stderr, _("Options:"));
		fprintf(stderr, "\n");

		for (h = options; h && h->opts; h++) {
			int len = (int) strlen(h->opts);
			if (max < len)
				max = len;
		}
		max += 2;

		for (h = options; h && h->opts; h++)
			fprintf(stderr, "  %-*s %s\n", max, h->opts, h->desc);
	}

	fprintf(stderr, "\n");
	fprintf(stderr, _("Report bugs to authors.\n"));
	fprintf(stderr, "\n");

	exit(rc);
}

#define onoff(a) ((a) ? _("on ") : _("off"))

/* report the bits, in the order seen on the (my) keyboard */
#define LED_NLOCK 1
#define LED_CMPOSE 2
#define LED_SCRLCK 4
#define LED_CLOCK 8

static void
sunreport(int leds)
{
	printf("NumLock %s   Compose %s   ScrollLock %s   CapsLock %s\n",
	       onoff(leds & LED_NLOCK),
	       onoff(leds & LED_CMPOSE),
	       onoff(leds & LED_SCRLCK),
	       onoff(leds & LED_CLOCK));
}

static void
report(int leds)
{
	printf("NumLock %s   CapsLock %s   ScrollLock %s\n",
	       onoff(leds & LED_NUM),
	       onoff(leds & LED_CAP),
	       onoff(leds & LED_SCR));
}

struct led {
	const char *name;
	unsigned char bit;
	unsigned char sunbit;
} leds[] = {
	{ "scroll", LED_SCR, LED_SCRLCK },
	{ "num", LED_NUM, LED_NLOCK },
	{ "caps", LED_CAP, LED_CLOCK },
#ifdef __sparc__
	{ "compose", 0, LED_CMPOSE }
#endif
};

static void
getleds(unsigned char *cur_leds)
{
	if (ioctl(0, KDGETLED, cur_leds)) {
		kbd_error(EX_OSERR, errno, _("Error reading current led setting. "
		                             "Maybe stdin is not a VT?: "
		                             "ioctl KDGETLED"));
	}
}

static int
setleds(unsigned char cur_leds)
{
	if (ioctl(0, KDSETLED, cur_leds)) {
		kbd_warning(errno, "ioctl KDSETLED");
		return -1;
	}
	return 0;
}

static void
getflags(unsigned char *flags)
{
	if (ioctl(0, KDGKBLED, flags))
		kbd_error(EX_OSERR, errno, _("Unable to read keyboard flags"));
}

static int sunkbdfd = -1;

#ifndef KIOCGLED
#define arg_state __attribute__((unused))
#else
#define arg_state
#endif

static void
sungetleds(arg_state unsigned char *cur_leds)
{
#ifdef KIOCGLED
	if (ioctl(sunkbdfd, KIOCGLED, cur_leds)) {
		kbd_error(EX_OSERR, errno, _("Error reading current led setting from /dev/kbd: "
		                             "ioctl %s"), "KIOCGLED");
	}
#else
	kbd_error(EX_OSERR, 0, _("ioctl %s unavailable?"), "KIOCGLED");
#endif
}

#ifndef KIOCSLED
#define arg_state __attribute__((unused))
#else
#define arg_state
#endif

static void
sunsetleds(arg_state unsigned char *cur_leds)
{
#ifdef KIOCSLED
	if (ioctl(sunkbdfd, KIOCSLED, cur_leds)) {
		kbd_error(EX_OSERR, errno, _("Error reading current led setting from /dev/kbd: "
		                             "ioctl %s"), "KIOCSLED");
	}
#else
	kbd_error(EX_OSERR, 0, _("ioctl %s unavailable?"), "KIOCSLED");
#endif
}

static int
parse_let_option(char *ap, unsigned char *nval, unsigned char *ndef,
		unsigned char *nsunval, unsigned char *nsundef)
{
	struct led *lp;
	unsigned char sign = 1;

	switch (ap[0]) {
		case '+':
			/* by default: set */
			ap++;
			break;
		case '-':
			sign = 0;
			ap++;
			break;
	}

	for (lp = leds; (unsigned)(lp - leds) < sizeof(leds) / sizeof(leds[0]); lp++) {
		if (!strcmp(ap, lp->name)) {
			if (sign) {
				*nval |= lp->bit;
				*nsunval |= lp->sunbit;
			}
			*ndef |= lp->bit;
			*nsundef |= lp->sunbit;
			return 0;
		}
	}

	return -1;
}

struct kbd_option {
	const char *shrtopt;
	const char *longopt;
	int  val;
};

int main(int argc, char **argv)
{
	int has_changes = 0;
	int optL = 0, optD = 0, optF = 0, verbose = 0;
	unsigned char oleds, nleds, oflags, nflags, odefflags, ndefflags;
	unsigned char nval, ndef;
	unsigned char osunleds = 0, nsunleds, nsunval, nsundef;

	set_progname(argv[0]);
	setuplocale();

	const struct kbd_option opts[] = {
		{ "-D", "--default", 'D' },
		{ "-F", "--flags",   'F' },
		{ "-L", "--leds",    'L' },
		{ "-h", "--help",    'h' },
		{ "-v", "--verbose", 'v' },
		{ "-V", "--version", 'V' },
		{ NULL, NULL, 0 }
	};
	const struct kbd_help opthelp[] = {
		{ "-D, --default",     _("change both the VT flags and their default settings.") },
		{ "-F, --flags",       _("change the VT flags.") },
		{ "-L, --leds",        _("change only the leds.") },
		{ "-V, --version",     _("print version number.")     },
		{ "-v, --verbose",     _("be more verbose.")          },
		{ "-h, --help",        _("print this usage message.") },
		{ NULL, NULL }
	};

	nval    = 0;
	ndef    = 0;
	nsunval = 0;
	nsundef = 0;

	while (argc > 1) {
		const struct kbd_option *opt;

		for (opt = opts; opt->longopt; opt++) {
			if (strcmp(argv[1], opt->shrtopt) && strcmp(argv[1], opt->longopt))
				continue;

			switch (opt->val) {
				case 'D':
					optD = 1;
					break;
				case 'F':
					optF = 1;
					break;
				case 'L':
					optL = 1;
					break;
				case 'v':
					verbose = 1;
					break;
				case 'V':
					print_version_and_exit();
					break;
				case 'h':
					usage(EX_OK, opthelp);
					break;
				default:
					kbd_warning(0, _("Unrecognized argument: %s"), argv[1]);
					usage(EX_USAGE, opthelp);
			}

			goto next_arg;
		}

		if (parse_let_option(argv[1], &nval, &ndef, &nsunval, &nsundef) < 0) {
			kbd_warning(0, _("Unrecognized argument: %s"), argv[1]);
			usage(EX_USAGE, opthelp);
		}

		has_changes = 1;
next_arg:
		argc--;
		argv++;
	}

#ifdef __sparc__
	if ((sunkbdfd = open("/dev/kbd", O_RDONLY)) < 0) {
		kbd_error(EX_OSFILE, errno, "open /dev/kbd");
		/* exit(1); */
	}
#endif

	getflags(&oflags);
	getleds(&oleds);

	if (sunkbdfd >= 0)
		sungetleds(&osunleds);

	odefflags = ndefflags = ((oflags >> 4) & 7);
	oflags = nflags = (oflags & 7);

	if (!has_changes) {
		if (optL) {
			nleds = 0xff;
			if (setleds(nleds)) {
				kbd_error(EX_OSERR, 0, _("Error resetting ledmode"));
			}
		}

		/* If nothing to do, report, even if not verbose */
		if (!optD && !optL && !optF)
			optD = optL = optF = 1;
		if (optD) {
			printf(_("Current default flags:  "));
			report(odefflags);
		}
		if (optF) {
			printf(_("Current flags:          "));
			report(oflags & 07);
		}
		if (optL) {
			printf(_("Current leds:           "));
			if (sunkbdfd >= 0)
				sunreport(osunleds);
			else
				report(oleds);
		}
		return EX_OK;
	}

	if (!optL)
		optF = 1;

	if (optD) {
		ndefflags = BITMASK_SET(BITMASK_UNSET(odefflags, ndef), nval);
		if (verbose) {
			printf(_("Old default flags:    "));
			report(odefflags);
			printf(_("New default flags:    "));
			report(ndefflags);
		}
	}
	if (optF) {
		nflags = BITMASK_SET(BITMASK_UNSET(oflags, ndef), nval);
		if (verbose) {
			printf(_("Old flags:            "));
			report(oflags & 07);
			printf(_("New flags:            "));
			report(nflags & 07);
		}
	}
	if (optD || optF) {
		if (ioctl(0, KDSKBLED, (ndefflags << 4) | nflags)) {
			kbd_error(EX_OSERR, errno, "ioctl KDSKBLED");
		}
	}
	if (optL) {
		if (sunkbdfd >= 0) {
			nsunleds = BITMASK_SET(BITMASK_UNSET(osunleds, nsundef), nsunval);
			if (verbose) {
				printf(_("Old leds:             "));
				sunreport(osunleds);
				printf(_("New leds:             "));
				sunreport(nsunleds);
			}
			sunsetleds(&nsunleds);
		} else {
			nleds = BITMASK_SET(BITMASK_UNSET(oleds, ndef), nval);
			if (verbose) {
				printf(_("Old leds:             "));
				report(oleds);
				printf(_("New leds:             "));
				report(nleds);
			}
			if (setleds(nleds))
				return EX_OSERR;
		}
	}

	return EX_OK;
}
