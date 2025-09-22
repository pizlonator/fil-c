/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
 */

#ifndef UTIL_LINUX_SETPRIV_LANDLOCK
#define UTIL_LINUX_SETPRIV_LANDLOCK

#ifdef HAVE_LINUX_LANDLOCK_H

#include <stdint.h>

#include "list.h"

struct setpriv_landlock_opts {
	uint64_t access_fs;
	struct list_head rules;
};

void do_landlock(const struct setpriv_landlock_opts *opts);
void parse_landlock_access(struct setpriv_landlock_opts *opts, const char *str);
void parse_landlock_rule(struct setpriv_landlock_opts *opts, const char *str);
void init_landlock_opts(struct setpriv_landlock_opts *opts);
void usage_setpriv(FILE *out);

#else

#include "c.h"
#include "nls.h"

struct setpriv_landlock_opts {};

static inline void do_landlock(const void *opts __attribute__((unused))) {}
static inline void parse_landlock_access(
		void *opts __attribute__((unused)),
		const char *str __attribute__((unused)))
{
	errx(EXIT_FAILURE, _("no support for landlock"));
}
#define parse_landlock_rule parse_landlock_access
static inline void init_landlock_opts(void *opts __attribute__((unused))) {}
static inline void usage_setpriv(FILE *out __attribute__((unused))) {}

#endif /* HAVE_LINUX_LANDLOCK_H */

#endif
