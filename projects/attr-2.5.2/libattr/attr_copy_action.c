/*
  Copyright (C) 2006 Andreas Gruenbacher <agruen@suse.de>, SuSE Linux AG.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this manual.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fnmatch.h>

#include "attr/libattr.h"
#define ERROR_CONTEXT_MACROS
#include "error_context.h"

#define ATTR_CONF SYSCONFDIR "/xattr.conf"

struct attr_action {
	struct attr_action *next;
	char *pattern;
	int action;
};

static struct attr_action *attr_actions;

static void
free_attr_actions(void)
{
	struct attr_action *tmp;

	while (attr_actions) {
		tmp = attr_actions->next;
		free(attr_actions->pattern);
		free(attr_actions);
		attr_actions = tmp;
	}
}

static int
attr_parse_attr_conf(struct error_context *ctx)
{
	char *text = NULL, *t;
	size_t size_guess = 4096, len;
	FILE *file;
	char *pattern = NULL;
	struct attr_action *new;
	int action;

	if (attr_actions)
		return 0;

repeat:
	if ((file = fopen(ATTR_CONF, "r")) == NULL) {
		if (errno == ENOENT)
			return 0;
		goto fail;
	}

	text = malloc(size_guess + 1);
	if (!text)
		goto fail;

	len = fread(text, 1, size_guess, file);
	if (ferror(file))
		goto fail;
	if (!feof(file)) {
		fclose(file);
		file = NULL;
		free(text);
		text = NULL;
		size_guess *= 2;
		goto repeat;
	}
	fclose(file);
	file = NULL;

	text[len] = 0;
	t = text;
	for (;;) {
		t += strspn(t, " \t\n");
		len = strcspn(t, " \t\n#");
		if (t[len] == '#') {
			if (len)
				goto parse_error;
			t += strcspn(t, "\n");
			continue;
		} else if (t[len] == 0)
			break;
		else if (t[len] == '\n')
			goto parse_error;
		pattern = strndup(t, len);
		if (!pattern)
			goto fail;
		t += len;

		t += strspn(t, " \t");
		len = strcspn(t, " \t\n#");
		if (len == 4 && !strncmp(t, "skip", 4))
			action = ATTR_ACTION_SKIP;
		else if (len == 11 && !strncmp(t, "permissions", 11))
			action = ATTR_ACTION_PERMISSIONS;
		else
			goto parse_error;
		t += len;
		t += strspn(t, " \t");
		if (*t != '#' && *t != '\n')
			goto parse_error;

		new = malloc(sizeof(struct attr_action));
		if (!new)
			goto parse_error;
		new->next = attr_actions;
		new->pattern = pattern;
		new->action = action;
		attr_actions = new;

		t += strcspn(t, "\n");
	}
	free(text);
	return 0;

parse_error:
	errno = EINVAL;

fail:
	{
		const char *q = quote (ctx, ATTR_CONF);
		error (ctx, "%s", q);
		quote_free (ctx, q);
	}

	free(pattern);
	if (file)
		fclose(file);
	free(text);
	free_attr_actions();
	return -1;
}

int
attr_copy_action(const char *name, struct error_context *ctx)
{
	if (!attr_parse_attr_conf(ctx)) {
		struct attr_action *action;
		for (action = attr_actions; action; action = action->next) {
			if (!fnmatch(action->pattern, name, 0))
				return action->action;
		}
	}
	return 0;
}
