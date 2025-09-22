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

#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/landlock.h>

#include "setpriv-landlock.h"

#include "strutils.h"
#include "xalloc.h"
#include "nls.h"
#include "c.h"

#ifndef HAVE_LANDLOCK_CREATE_RULESET
static inline int landlock_create_ruleset(
		const struct landlock_ruleset_attr *attr,
		size_t size, uint32_t flags)
{
	return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif

#ifndef HAVE_LANDLOCK_ADD_RULE
static inline int landlock_add_rule(
		int ruleset_fd, enum landlock_rule_type rule_type,
		const void *rule_attr, uint32_t flags)
{
	return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type,
		       rule_attr, flags);
}
#endif

#ifndef HAVE_LANDLOCK_RESTRICT_SELF
static inline int landlock_restrict_self(int ruleset_fd, uint32_t flags)
{
	return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

#define SETPRIV_EXIT_PRIVERR 127	/* how we exit when we fail to set privs */

struct landlock_rule_entry {
	struct list_head head;
	enum landlock_rule_type rule_type;
	union {
		struct landlock_path_beneath_attr path_beneath_attr;
	};
};

static const struct {
	unsigned long long value;
	const char *type;
} landlock_access_fs[] = {
	{ LANDLOCK_ACCESS_FS_EXECUTE,     "execute"     },
	{ LANDLOCK_ACCESS_FS_WRITE_FILE,  "write-file"  },
	{ LANDLOCK_ACCESS_FS_READ_FILE,   "read-file"   },
	{ LANDLOCK_ACCESS_FS_READ_DIR,    "read-dir"    },
	{ LANDLOCK_ACCESS_FS_REMOVE_DIR,  "remove-dir"  },
	{ LANDLOCK_ACCESS_FS_REMOVE_FILE, "remove-file" },
	{ LANDLOCK_ACCESS_FS_MAKE_CHAR,   "make-char"   },
	{ LANDLOCK_ACCESS_FS_MAKE_DIR,    "make-dir"    },
	{ LANDLOCK_ACCESS_FS_MAKE_REG,    "make-reg"    },
	{ LANDLOCK_ACCESS_FS_MAKE_SOCK,   "make-sock"   },
	{ LANDLOCK_ACCESS_FS_MAKE_FIFO,   "make-fifo"   },
	{ LANDLOCK_ACCESS_FS_MAKE_BLOCK,  "make-block"  },
	{ LANDLOCK_ACCESS_FS_MAKE_SYM,    "make-sym"    },
#ifdef LANDLOCK_ACCESS_FS_REFER
	{ LANDLOCK_ACCESS_FS_REFER,       "refer"       },
#endif
#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
	{ LANDLOCK_ACCESS_FS_TRUNCATE,    "truncate"    },
#endif
};

static long landlock_access_to_mask(const char *str, size_t len)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(landlock_access_fs); i++)
		if (strncmp(landlock_access_fs[i].type, str, len) == 0)
			return landlock_access_fs[i].value;
	return -1;
}

static uint64_t parse_landlock_fs_access(const char *list)
{
	unsigned long r = 0;
	size_t i;

	/* without argument, match all */
	if (list[0] == '\0') {
		for (i = 0; i < ARRAY_SIZE(landlock_access_fs); i++)
			r |= landlock_access_fs[i].value;
	} else {
		if (string_to_bitmask(list, &r, landlock_access_to_mask))
			errx(EXIT_FAILURE,
			     _("could not parse landlock fs access: %s"), list);
	}

	return r;
}

void parse_landlock_access(struct setpriv_landlock_opts *opts, const char *str)
{
	const char *type;
	size_t i;

	if (strcmp(str, "fs") == 0) {
		for (i = 0; i < ARRAY_SIZE(landlock_access_fs); i++)
			opts->access_fs |= landlock_access_fs[i].value;
		return;
	}

	type = startswith(str, "fs:");
	if (type)
		opts->access_fs |= parse_landlock_fs_access(type);
}

void parse_landlock_rule(struct setpriv_landlock_opts *opts, const char *str)
{
	struct landlock_rule_entry *rule = xmalloc(sizeof(*rule));
	const char *accesses, *path;
	char *accesses_part;
	int parent_fd;

	accesses = startswith(str, "path-beneath:");
	if (!accesses)
		errx(EXIT_FAILURE, _("invalid landlock rule: %s"), str);
	path = strchr(accesses, ':');
	if (!path)
		errx(EXIT_FAILURE, _("invalid landlock rule: %s"), str);
	rule->rule_type = LANDLOCK_RULE_PATH_BENEATH;

	accesses_part = xstrndup(accesses, path - accesses);
	rule->path_beneath_attr.allowed_access = parse_landlock_fs_access(accesses_part);
	free(accesses_part);

	path++;

	parent_fd = open(path, O_RDONLY | O_PATH | O_CLOEXEC);
	if (parent_fd == -1)
		err(EXIT_FAILURE, _("could not open file for landlock: %s"), path);

	rule->path_beneath_attr.parent_fd = parent_fd;

	list_add(&rule->head, &opts->rules);
}

void init_landlock_opts(struct setpriv_landlock_opts *opts)
{
	INIT_LIST_HEAD(&opts->rules);
}

void do_landlock(const struct setpriv_landlock_opts *opts)
{
	struct landlock_rule_entry *rule;
	struct list_head *entry;
	int fd, ret;

	if (!opts->access_fs)
		return;

	const struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = opts->access_fs,
	};

	fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	if (fd == -1)
		err(SETPRIV_EXIT_PRIVERR, _("landlock_create_ruleset failed"));

	list_for_each(entry, &opts->rules) {
		rule = list_entry(entry, struct landlock_rule_entry, head);

		assert(rule->rule_type == LANDLOCK_RULE_PATH_BENEATH);

		ret = landlock_add_rule(fd, rule->rule_type, &rule->path_beneath_attr, 0);
		if (ret == -1)
			err(SETPRIV_EXIT_PRIVERR, _("adding landlock rule failed"));
	}

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
		err(SETPRIV_EXIT_PRIVERR, _("disallow granting new privileges for landlock failed"));

	if (landlock_restrict_self(fd, 0) == -1)
		err(SETPRIV_EXIT_PRIVERR, _("landlock_restrict_self faild"));
}

void usage_setpriv(FILE *out)
{
	size_t i;

	fprintf(out, "\n");
	fprintf(out, _("Landlock accesses:\n"));
	fprintf(out, " Access: fs\n");
	fprintf(out, " Rule types: path-beneath\n");

	fprintf(out, " Rules: ");
	for (i = 0; i < ARRAY_SIZE(landlock_access_fs); i++) {
		fprintf(out, "%s", landlock_access_fs[i].type);
		if (i == ARRAY_SIZE(landlock_access_fs) - 1)
			fprintf(out, "\n");
		else
			fprintf(out, ",");
	}
}
