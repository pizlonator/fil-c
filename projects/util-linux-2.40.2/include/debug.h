/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2014 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_DEBUG_H
#define UTIL_LINUX_DEBUG_H


/*
 * util-linux debug macros
 *
 * The debug stuff is based on <name>_debug_mask that controls what outputs is
 * expected. The mask is usually initialized by <NAME>_DEBUG= env.variable
 *
 * After successful initialization the flag <PREFIX>_DEBUG_INIT is always set
 * to the mask (this flag is required). The <PREFIX> is usually library API
 * prefix (e.g. MNT_) or program name (e.g. CFDISK_)
 *
 * In the code is possible to use
 *
 *	DBG(FOO, ul_debug("this is output for foo"));
 *
 * where for the FOO has to be defined <PREFIX>_DEBUG_FOO.
 *
 * It's possible to initialize the mask by comma delimited strings with
 * subsystem names (e.g. "LIBMOUNT_DEBUG=options,tab"). In this case is
 * necessary to define mask names array. This functionality is optional.
 *
 * It's strongly recommended to use UL_* macros to define/declare/use
 * the debug stuff.
 *
 * See disk-utils/cfdisk.c: cfdisk_init_debug()  for programs debug
 *  or libmount/src/init.c: mnt_init_debug()     for library debug
 *
 */

#include <stdarg.h>
#include <string.h>

struct ul_debug_maskname {
	const char *name;
	int mask;
	const char *help;
};
#define UL_DEBUG_EMPTY_MASKNAMES {{ NULL, 0, NULL }}
#define UL_DEBUG_DEFINE_MASKNAMES(m) static const struct ul_debug_maskname m ## _masknames[]
#define UL_DEBUG_MASKNAMES(m)	m ## _masknames

#define UL_DEBUG_MASK(m)         m ## _debug_mask
#define UL_DEBUG_DEFINE_MASK(m)  int UL_DEBUG_MASK(m)
#define UL_DEBUG_DECLARE_MASK(m) extern UL_DEBUG_DEFINE_MASK(m)

/*
 * Internal mask flags (above 0xffffff)
 */
#define __UL_DEBUG_FL_NOADDR	(1 << 24)	/* Don't print object address */


/* l - library name, p - flag prefix, m - flag postfix, x - function */
#define __UL_DBG(l, p, m, x) \
	do { \
		if ((p ## m) & l ## _debug_mask) { \
			fprintf(stderr, "%d: %s: %8s: ", getpid(), # l, # m); \
			x; \
		} \
	} while (0)

#define __UL_DBG_CALL(l, p, m, x) \
	do { \
		if ((p ## m) & l ## _debug_mask) { \
			x; \
		} \
	} while (0)

#define __UL_DBG_FLUSH(l, p) \
	do { \
		if (l ## _debug_mask && \
		    l ## _debug_mask != p ## INIT) { \
			fflush(stderr); \
		} \
	} while (0)

#define __UL_INIT_DEBUG_FROM_STRING(lib, pref, mask, str) \
	do { \
		if (lib ## _debug_mask & pref ## INIT) \
		; \
		else if (!mask && str) { \
			lib ## _debug_mask = ul_debug_parse_mask(lib ## _masknames, str); \
		} else \
			lib ## _debug_mask = mask; \
		if (lib ## _debug_mask) { \
			if (getuid() != geteuid() || getgid() != getegid()) { \
				lib ## _debug_mask |= __UL_DEBUG_FL_NOADDR; \
				fprintf(stderr, "%d: %s: don't print memory addresses (SUID executable).\n", getpid(), # lib); \
			} \
		} \
		lib ## _debug_mask |= pref ## INIT; \
	} while (0)


#define __UL_INIT_DEBUG_FROM_ENV(lib, pref, mask, env) \
	do { \
		const char *envstr = mask ? NULL : getenv(# env); \
		__UL_INIT_DEBUG_FROM_STRING(lib, pref, mask, envstr); \
	} while (0)



static inline void __attribute__ ((__format__ (__printf__, 1, 2)))
ul_debug(const char *mesg, ...)
{
	va_list ap;
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static inline int ul_debug_parse_mask(
			const struct ul_debug_maskname flagnames[],
			const char *mask)
{
	int res;
	char *ptr;

	/* let's check for a numeric mask first */
	res = strtoul(mask, &ptr, 0);

	/* perhaps it's a comma-separated string? */
	if (ptr && *ptr && flagnames && flagnames[0].name) {
		char *msbuf, *ms, *name;
		res = 0;

		ms = msbuf = strdup(mask);
		if (!ms)
			return res;

		while ((name = strtok_r(ms, ",", &ptr))) {
			const struct ul_debug_maskname *d;
			ms = ptr;

			for (d = flagnames; d && d->name; d++) {
				if (strcmp(name, d->name) == 0) {
					res |= d->mask;
					break;
				}
			}
			/* nothing else we can do by OR-ing the mask */
			if (res == 0xffff)
				break;
		}
		free(msbuf);
	} else if (ptr && strcmp(ptr, "all") == 0)
		res = 0xffff;

	return res;
}

static inline void ul_debug_print_masks(
			const char *env,
			const struct ul_debug_maskname flagnames[])
{
	const struct ul_debug_maskname *d;

	if (!flagnames)
		return;

	fprintf(stderr, "Available \"%s=<name>[,...]|<mask>\" debug masks:\n",
			env);
	for (d = flagnames; d && d->name; d++) {
		if (!d->help)
			continue;
		fprintf(stderr, "   %-8s [0x%06x] : %s\n",
				d->name, d->mask, d->help);
	}
}

#endif /* UTIL_LINUX_DEBUG_H */
