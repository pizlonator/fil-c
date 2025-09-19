/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util-macros.h"
#include "util-strings.h"

/**
 * Print a backtrace for this process using gstack.
 *
 * If use_colors is true, highlight_after may specify
 * a substring for a line after which the remaining backtrace is
 * colored.
 *
 * If use_colors is true, highlight_before may specify
 * a substring for a line before which the backtrace is
 * colored.
 *
 * If use_colors is true, highlight_extra may specify
 * a substring for a line that has extra highlighting.
 */
static inline void
backtrace_print(FILE *fp,
		bool use_colors,
		const char *highlight_after,
		const char *highlight_before,
		const char *highlight_extra)
{
#ifdef HAVE_GSTACK
	pid_t parent, child;
	int pipefd[2];

	if (pipe(pipefd) == -1)
		return;

	parent = getpid();
	child = fork();

	if (child == 0) {
		char pid[8];

		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);

		sprintf(pid, "%d", parent);

		execlp("gstack", "gstack", pid, NULL);
		exit(errno);
	}

	/* parent */
	int status;

	close(pipefd[1]);
	waitpid(child, &status, 0);

	status = WEXITSTATUS(status);
	if (status != 0) {
		fprintf(fp,
			"ERROR: gstack failed, no backtrace available: %s\n",
			strerror(status));
		goto out;
	}

	char buf[2048] = { 0 };
	fprintf(fp, "\nBacktrace:\n");
	read(pipefd[0], buf, sizeof(buf) - 1);
	if (!use_colors || (!highlight_after && !highlight_before)) {
		fprintf(fp, "%s\n", buf);
	} else {
		size_t nlines;
		char **lines = strv_from_string(buf, "\n", &nlines);
		char **line = lines;
		bool highlight = highlight_after == NULL;
		while (line && *line) {
			if (highlight && highlight_before &&
			    strstr(*line, highlight_before))
				highlight = false;

			const char *hlcolor = highlight ? ANSI_BRIGHT_CYAN : "";

			if (highlight && highlight_extra &&
			    strstr(*line, highlight_extra))
				hlcolor = ANSI_BRIGHT_MAGENTA;

			fprintf(fp,
				"%s%s%s\n",
				hlcolor,
				*line,
				highlight ? ANSI_NORMAL : "");
			if (!highlight && highlight_after &&
			    strstr(*line, highlight_after))
				highlight = true;
			line++;
		}
		strv_free(lines);
	}
out:
	close(pipefd[0]);
#endif
}
