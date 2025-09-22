/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2012 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2012-2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_COLORS_H
#define UTIL_LINUX_COLORS_H

#include <stdio.h>
#include <unistd.h>

#include "color-names.h"

/* --color[=WHEN] */
enum colortmode {
	UL_COLORMODE_AUTO = 0,
	UL_COLORMODE_NEVER,
	UL_COLORMODE_ALWAYS,
	UL_COLORMODE_UNDEF,

	__UL_NCOLORMODES	/* last */
};

#ifdef USE_COLORS_BY_DEFAULT
# define USAGE_COLORS_DEFAULT	_("colors are enabled by default")
#else
# define USAGE_COLORS_DEFAULT   _("colors are disabled by default")
#endif

extern int colormode_from_string(const char *str);
extern int colormode_or_err(const char *str, const char *errmsg);

/* Initialize the global variable UL_COLOR_TERM_OK */
extern int colors_init(int mode, const char *util_name);

/* Returns 1 or 0 */
extern int colors_wanted(void);

/* Returns UL_COLORMODE_* */
extern int colors_mode(void);

/* temporary enable/disable colors */
extern void colors_off(void);
extern void colors_on(void);


/* Set the color */
extern void color_fenable(const char *seq, FILE *f);

extern void color_scheme_fenable(const char *name, const char *dflt, FILE *f);
extern const char *color_scheme_get_sequence(const char *name, const char *dflt);

static inline void color_enable(const char *seq)
{
	color_fenable(seq, stdout);
}

static inline void color_scheme_enable(const char *name, const char *dflt)
{
	color_scheme_fenable(name, dflt, stdout);
}

/* Reset colors to default */
extern void color_fdisable(FILE *f);

static inline void color_disable(void)
{
	color_fdisable(stdout);
}

const char *color_get_disable_sequence(void);

#endif /* UTIL_LINUX_COLORS_H */
