/*
 * Copyright © 2013 Red Hat, Inc.
 * Copyright © 2013 Marcin Slusarz <marcin.slusarz@gmail.com>
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <libudev.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "linux/input.h"
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#endif
#ifdef __FreeBSD__
#include <termios.h>
#endif

#include <valgrind/valgrind.h>

#include "util-backtrace.h"
#include "util-files.h"
#include "util-libinput.h"

#include "builddir.h"
#include "libinput-util.h"
#include "litest-int.h"
#include "litest-runner.h"
#include "litest.h"
#include "quirks.h"

static struct list all_test_suites = LIST_INIT(all_test_suites); /* struct suite */

static int jobs;
extern bool use_colors;
extern bool in_debugger;
extern bool verbose;
extern bool run_deviceless;
extern struct suite *current_suite;

static bool
is_debugger_attached(void)
{
	int status;
	bool rc;
	int pid = fork();

	if (pid == -1)
		return 0;

	if (pid == 0) {
		int ppid = getppid();
		if (ptrace(PTRACE_ATTACH, ppid, NULL, 0) == 0) {
			waitpid(ppid, NULL, 0);
			ptrace(PTRACE_CONT, ppid, NULL, 0);
			ptrace(PTRACE_DETACH, ppid, NULL, 0);
			rc = false;
		} else {
			rc = true;
		}
		_exit(rc);
	} else {
		waitpid(pid, &status, 0);
		rc = WEXITSTATUS(status);
	}

	return !!rc;
}

static void
litest_list_tests(struct list *tests)
{
	struct suite *s;
	const char *last_test_name = "<invalid>";
	const char *last_dev_name = "<invalid>";

	printf("groups:\n");
	list_for_each(s, tests, node) {
		struct test *t;
		printf("  - group: \"%s\"\n", s->name);
		printf("    tests:\n");
		list_for_each(t, &s->tests, node) {
			bool same_test = streq(last_test_name, t->name);
			bool same_dev = streq(last_dev_name, t->devname);

			if (!same_test) {
				printf("      - name: \"%s\"\n", t->name);
				printf("        devices:\n");
			}

			if (!same_test || !same_dev) {
				last_test_name = t->name;
				last_dev_name = t->devname;
				printf("          - name: \"%s\"\n", t->devname);
			}
		}
	}
}

extern const struct test_device __start_test_device_section, __stop_test_device_section;

static void
litest_init_test_devices(void)
{
	const struct test_device *t;
	for (t = &__start_test_device_section; t < &__stop_test_device_section; t++)
		litest_add_test_device(&t->device->node);
}

extern const struct test_collection __start_test_collection_section,
	__stop_test_collection_section;

static void
setup_tests(void)
{
	const struct test_collection *c;

	for (c = &__start_test_collection_section; c < &__stop_test_collection_section;
	     c++) {
		struct suite *s;
		s = zalloc(sizeof(*s));
		s->name = safe_strdup(c->name);

		list_init(&s->tests);
		list_append(&all_test_suites, &s->node);

		current_suite = s;
		c->setup();
		current_suite = NULL;
	}
}

static int
check_device_access(void)
{
	if (getuid() != 0) {
		fprintf(stderr,
			"%s must be run as root.\n",
			program_invocation_short_name);
		return 77;
	}

	if (access("/dev/uinput", F_OK) == -1 &&
	    access("/dev/input/uinput", F_OK) == -1) {
		fprintf(stderr, "uinput device is missing, skipping tests.\n");
		return 77;
	}

	return 0;
}

static void
litest_free_test_list(struct list *tests)
{
	struct suite *s;

	list_for_each_safe(s, tests, node) {
		struct test *t;

		list_for_each_safe(t, &s->tests, node) {
			litest_test_parameters_unref(t->params);
			free(t->name);
			free(t->devname);
			list_remove(&t->node);
			free(t);
		}

		list_remove(&s->node);
		free(s->name);
		free(s);
	}
}

int
main(int argc, char **argv)
{
	enum litest_mode mode;
	int rc;
	const char *meson_testthreads;

	use_colors = getenv("FORCE_COLOR") || isatty(STDERR_FILENO);
	if (getenv("NO_COLOR"))
		use_colors = false;

	in_debugger = is_debugger_attached();
	if (in_debugger) {
		jobs = 0;
	} else if ((meson_testthreads = getenv("MESON_TESTTHREADS")) == NULL ||
		   !safe_atoi(meson_testthreads, &jobs)) {
		jobs = get_nprocs();
		if (!RUNNING_ON_VALGRIND)
			jobs *= 2;
	}

	if (getenv("LITEST_VERBOSE"))
		verbose = true;

	mode = litest_parse_argv(argc, argv, &jobs);
	if (mode == LITEST_MODE_ERROR)
		return EXIT_FAILURE;

	litest_init_test_devices();

	setup_tests();
	if (list_empty(&all_test_suites)) {
		fprintf(stderr, "Error: filters are too strict, no tests to run.\n");
		return EXIT_FAILURE;
	}

	if (mode == LITEST_MODE_LIST) {
		litest_list_tests(&all_test_suites);
		return EXIT_SUCCESS;
	}

	if (!run_deviceless && (rc = check_device_access()) != 0)
		return rc;

	enum litest_runner_result result = litest_run(&all_test_suites, jobs);

	litest_free_test_list(&all_test_suites);

	switch (result) {
	case LITEST_PASS:
		return EXIT_SUCCESS;
	case LITEST_SKIP:
		return 77;
	default:
		return result;
	}
}
