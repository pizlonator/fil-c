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
#include <sys/epoll.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#ifdef HAVE_PIDFD_OPEN
#include <sys/syscall.h>
#endif
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <valgrind/valgrind.h>

#include "util-files.h"
#include "util-list.h"
#include "util-multivalue.h"
#include "util-stringbuf.h"

#include "litest-runner.h"

static bool use_jmpbuf; /* only used for max_forks = 0 */
static jmp_buf jmpbuf;

/* musl doesn't have this one but it's not that important */
#ifndef HAVE_SIGABBREV_NP
#define sigabbrev_np(...) "???"
#endif

static struct litest_runner *global_runner = NULL;

enum litest_runner_logfds {
	FD_STDOUT,
	FD_STDERR,
	FD_LOG,
	FD_VALGRIND,
	_FD_LAST,
};

struct litest_runner_test {
	struct litest_runner_test_description desc;
	struct list node;

	enum litest_runner_result result;
	int sig_or_errno;

	struct stringbuf logs[_FD_LAST];
	pid_t pid;              /* the test's PID, if any */
	int read_fds[_FD_LAST]; /* logging fds while the test is running */

	int epollfd;
	int pidfd;
	int timerfd;

	struct {
		uint64_t start_millis;
		uint64_t end_millis;
	} times;
};

struct litest_runner {
	size_t max_forks;
	unsigned int timeout;
	bool verbose;
	bool use_colors;
	bool exit_on_fail;
	FILE *fp;

	int terminating;

	struct list tests;          /* struct litest_runner_test */
	struct list tests_running;  /* struct litest_runner_test */
	struct list tests_complete; /* struct litest_runner_test */

	struct {
		time_t start;
		time_t end;
		uint64_t start_millis;
	} times;

	struct {
		litest_runner_global_setup_func_t setup;
		litest_runner_global_teardown_func_t teardown;
		void *userdata;
	} global;
};

/**
 * A global variable that the tests can use
 * to write log data to. Defaults to stdout
 * but if used by the tests it shows up as separate
 * from stdout in the logs.
 */
int testlog_fd = STDOUT_FILENO;

static void
close_pipes(int fds[_FD_LAST])
{
	for (int i = 0; i < _FD_LAST; i++) {
		fsync(fds[i]);
		xclose(&fds[i]);
	}
}

static int
init_pipes(int read_fds[_FD_LAST], int write_fds[_FD_LAST])
{
	int r;
	int i;
	int pipe_max_size = 4194304;

	for (i = 0; i < _FD_LAST; i++) {
		read_fds[i] = -1;
		write_fds[i] = -1;
	}

#ifdef __linux__
	{
		FILE *f;
		f = fopen("/proc/sys/fs/pipe-max-size", "re");
		if (f) {
			if (fscanf(f, "%d", &r) == 1)
				pipe_max_size = min(r, pipe_max_size);
			fclose(f);
		}
	}
#endif

	for (i = 0; i < _FD_LAST; i++) {
		int pipe[2];

		r = pipe2(pipe, O_CLOEXEC | O_NONBLOCK);
		if (r < 0)
			goto error;
		read_fds[i] = pipe[0];
		write_fds[i] = pipe[1];
#ifdef __linux__
		/* Max pipe buffers, to avoid scrambling if reading lags.
		 * Can't use blocking write fds, since reading too slow
		 * then affects execution.
		 */
		fcntl(write_fds[i], F_SETPIPE_SZ, pipe_max_size);
#endif
	}

	return 0;
error:
	r = -errno;
	close_pipes(read_fds);
	close_pipes(write_fds);
	return r;
}

static const char *
litest_runner_result_as_str(enum litest_runner_result result)
{
	switch (result) {
	CASE_RETURN_STRING(LITEST_PASS);
	CASE_RETURN_STRING(LITEST_NOT_APPLICABLE);
	CASE_RETURN_STRING(LITEST_FAIL);
	CASE_RETURN_STRING(LITEST_SYSTEM_ERROR);
	CASE_RETURN_STRING(LITEST_TIMEOUT);
	CASE_RETURN_STRING(LITEST_SKIP);
	}

	litest_abort_msg("Unknown result %d", result);
	return NULL;
}

static void
litest_runner_test_close(struct litest_runner_test *t)
{
	for (size_t i = 0; i < ARRAY_LENGTH(t->read_fds); i++) {
		xclose(&t->read_fds[i]);
	}
	xclose(&t->epollfd);
	xclose(&t->pidfd);
	xclose(&t->timerfd);
}

static void
litest_runner_test_destroy(struct litest_runner_test *t)
{
	list_remove(&t->node);
	close_pipes(t->read_fds);
	if (t->pid != 0) {
		kill(t->pid, SIGTERM);
		t->pid = 0;
	}
	litest_runner_test_close(t);
	for (int i = 0; i < _FD_LAST; i++) {
		stringbuf_reset(&t->logs[i]);
	}
	free(t);
}

static void
litest_runner_detach_tests(struct litest_runner *runner)
{
	struct litest_runner_test *t;

	list_for_each_safe(t, &runner->tests, node) {
		t->pid = 0;
	}
	list_for_each_safe(t, &runner->tests_complete, node) {
		t->pid = 0;
	}
	list_for_each_safe(t, &runner->tests_running, node) {
		t->pid = 0;
	}
}

void
litest_runner_destroy(struct litest_runner *runner)
{
	struct litest_runner_test *t;
	list_for_each_safe(t, &runner->tests, node) {
		litest_runner_test_destroy(t);
	}
	list_for_each_safe(t, &runner->tests_complete, node) {
		litest_runner_test_destroy(t);
	}
	list_for_each_safe(t, &runner->tests_running, node) {
		litest_runner_test_destroy(t);
	}
	free(runner);
}

static enum litest_runner_result
litest_runner_test_run(const struct litest_runner_test_description *desc)
{
	const struct litest_runner_test_env env = {
		.rangeval = desc->rangeval,
		.params = desc->params,
	};

	if (desc->setup)
		desc->setup(desc);

	enum litest_runner_result result = desc->func(&env);

	if (desc->teardown)
		desc->teardown(desc);

	return result;
}

static void
sighandler_forked_child(int signal)
{
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_DFL;
	sigaction(signal, &act, NULL);

	/* abort() was probably called by litest_assert... which inserts
	 * the backtrace anyway -  we only need to backtrace the other signals
	 */
	if (signal != SIGABRT)
		litest_backtrace(NULL);

	raise(signal);
}

static int
litest_runner_fork_test(struct litest_runner *runner, struct litest_runner_test *t)
{
	pid_t pid;
	struct sigaction act;
	int write_fds[_FD_LAST];
	int r;

	r = init_pipes(t->read_fds, write_fds);
	if (r < 0) {
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		int r = -errno;
		close_pipes(t->read_fds);
		close_pipes(write_fds);
		return r;
	}

	if (pid > 0) { /* parent */
		close_pipes(write_fds);
		t->pid = pid;
		return pid;
	}

	/* child */
	close_pipes(t->read_fds);

	/* Catch any crashers so we can insert a backtrace */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sighandler_forked_child;
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	/* SIGALARM is used for our timeout */
	sigaction(SIGALRM, &act, NULL);

	r = dup2(write_fds[FD_STDERR], STDERR_FILENO);
	litest_assert_errno_success(r);
	setlinebuf(stderr);
	r = dup2(write_fds[FD_STDOUT], STDOUT_FILENO);
	litest_assert_errno_success(r);
	setlinebuf(stdout);

	/* For convenience in the tests, let this be a global variable. */
	testlog_fd = write_fds[FD_LOG];

	/* We're forked so we have a full copy of everything - but all we want
	 * to do is avoid valgrind complaining about memleaks in the child
	 * process. So let's cleanup everything, copy the one thing we
	 * care about and proceed to the exit */
	struct litest_runner_test_description desc = t->desc;
	litest_runner_detach_tests(runner);
	litest_runner_destroy(runner);

	/* Now run the actual test */
	enum litest_runner_result result = litest_runner_test_run(&desc);

	close_pipes(write_fds);

	exit(result);
}

static char *
valgrind_logfile(pid_t pid)
{
	const char *prefix = getenv("LITEST_VALGRIND_LOGDIR");
	if (!prefix)
		prefix = ".";

	char *filename = strdup_printf("%s/valgrind.%d.log", prefix, pid);
	litest_assert_ptr_notnull(filename);

	return filename;
}

static void
collect_file(const char *filename, struct stringbuf *b)
{
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		_autofree_ char *msg =
			strdup_printf("Failed to find '%s': %m", filename);
		stringbuf_append_string(b, msg);
	} else {
		stringbuf_append_from_fd(b, fd, 0);
		close(fd);
	}
}

static bool
litest_runner_test_collect_child(struct litest_runner_test *t)
{
	int r;
	int status;

	r = waitpid(t->pid, &status, WNOHANG);
	if (r <= 0)
		return false;

	if (WIFEXITED(status)) {
		t->result = WEXITSTATUS(status);
		if (RUNNING_ON_VALGRIND && t->result == 3) {
			char msg[64];
			snprintf(msg,
				 sizeof(msg),
				 "valgrind exited with an error code, see logs\n");
			stringbuf_append_string(&t->logs[FD_LOG], msg);
			t->result = LITEST_SYSTEM_ERROR;
		}
		switch (t->result) {
		case LITEST_PASS:
		case LITEST_SKIP:
		case LITEST_NOT_APPLICABLE:
		case LITEST_FAIL:
		case LITEST_TIMEOUT:
		case LITEST_SYSTEM_ERROR:
			break;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
		case 0:
			/* if a test execve's itself allow for the normal
			 * exit codes to map to the results */
			t->result = LITEST_PASS;
			break;
#pragma GCC diagnostic pop
		default: {
			char msg[64];
			snprintf(msg,
				 sizeof(msg),
				 "Invalid test exit status %d",
				 t->result);
			stringbuf_append_string(&t->logs[FD_LOG], msg);
			t->result = LITEST_FAIL;
			break;
		}
		}
	} else {
		if (WIFSIGNALED(status)) {
			t->sig_or_errno = WTERMSIG(status);
			t->result = (t->sig_or_errno == t->desc.args.signal)
					    ? LITEST_PASS
					    : LITEST_FAIL;
		} else {
			t->result = LITEST_FAIL;
		}
	}

	uint64_t now = 0;
	now_in_us(&now);
	t->times.end_millis = us2ms(now);

	if (RUNNING_ON_VALGRIND) {
		_autofree_ char *filename = valgrind_logfile(t->pid);
		collect_file(filename, &t->logs[FD_VALGRIND]);
	}

	t->pid = 0;

	return true;
}

static int
litest_runner_test_setup_monitoring(struct litest_runner *runner,
				    struct litest_runner_test *t)
{
	int pidfd = -1, timerfd = -1, epollfd = -1;
	struct epoll_event ev[10];
	size_t nevents = 0;
	int r = 0;

#ifdef HAVE_PIDFD_OPEN
	pidfd = syscall(SYS_pidfd_open, t->pid, 0);
#else
	errno = ENOSYS;
#endif
	/* If we don't have pidfd, we use a timerfd to ping us every 200ms */
	if (pidfd < 0 && errno == ENOSYS) {
		pidfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
		if (pidfd == -1) {
			r = -errno;
			goto error;
		}
		r = timerfd_settime(pidfd,
				    0,
				    &((struct itimerspec){
					    .it_interval.tv_nsec = 200 * 1000 * 1000,
					    .it_value.tv_nsec = 200 * 1000 * 1000,
				    }),
				    NULL);
		if (r < 0) {
			r = -errno;
			goto error;
		}
	}

	/* Each test has an epollfd with:
	 *   - a timerfd so we can kill() it if it hangs
	 *   - a pidfd so we get notified when the test exits
	 *   - a pipe for stdout and a pipe for stderr
	 *   - a pipe for logging (the various pwtest functions)
	 *   - a pipe for the daemon's stdout
	 */
	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (timerfd < 0) {
		r = -errno;
		goto error;
	}
	timerfd_settime(timerfd,
			0,
			&((struct itimerspec){ .it_value.tv_sec = runner->timeout }),
			NULL);

	epollfd = epoll_create(1);
	if (epollfd < 0)
		goto error;
	ev[nevents++] = (struct epoll_event){ .events = EPOLLIN, .data.fd = pidfd };
	ev[nevents++] = (struct epoll_event){ .events = EPOLLIN,
					      .data.fd = t->read_fds[FD_STDOUT] };
	ev[nevents++] = (struct epoll_event){ .events = EPOLLIN,
					      .data.fd = t->read_fds[FD_STDERR] };
	ev[nevents++] = (struct epoll_event){ .events = EPOLLIN,
					      .data.fd = t->read_fds[FD_LOG] };
	ev[nevents++] = (struct epoll_event){ .events = EPOLLIN, .data.fd = timerfd };

	for (size_t i = 0; i < nevents; i++) {
		r = epoll_ctl(epollfd, EPOLL_CTL_ADD, ev[i].data.fd, &ev[i]);
		if (r < 0) {
			r = -errno;
			goto error;
		}
	}

	t->epollfd = epollfd;
	t->pidfd = pidfd;
	t->timerfd = timerfd;

	r = 0;

error:
	if (r) {
		xclose(&pidfd);
		xclose(&timerfd);
		xclose(&epollfd);
	}

	return -r;
}

static int
litest_runner_test_check_status(struct litest_runner_test *t)
{
	struct epoll_event e;

	if (t->pid == 0) /* nofork case */
		return 0;

	while (true) {
		/* FIXME: this is a busy wait ... */
		int r = epoll_wait(t->epollfd, &e, 1, 50);
		if (r == 0)
			return -EAGAIN;

		if (r == -1) {
			goto error;
		}

		if (e.data.fd == t->pidfd) {
			uint64_t buf;
			int ignore = read(t->pidfd,
					  &buf,
					  sizeof(buf)); /* for timerfd fallback */
			(void)ignore;
			if (litest_runner_test_collect_child(t)) {
				break;
			}
		} else if (e.data.fd == t->timerfd) {
			/* SIGALARM so we get the backtrace */
			kill(t->pid, SIGALRM);
			t->result = LITEST_TIMEOUT;
			waitpid(t->pid, NULL, 0);
			t->pid = 0;
			break;
		} else {
			for (int i = 0; i < _FD_LAST; i++) {
				if (e.data.fd == t->read_fds[i]) {
					stringbuf_append_from_fd(&t->logs[i],
								 e.data.fd,
								 1024);
				}
			}
		}
	};

	errno = 0;
error:
	return -errno;
}

static void
litest_runner_test_update_errno(struct litest_runner_test *t, int error)
{
	if (error)
		t->sig_or_errno = -error;

	if (error == SIGTERM) {
		char msg[64];
		snprintf(msg, sizeof(msg), "litest: tests terminated by signal\n");
		stringbuf_append_string(&t->logs[FD_LOG], msg);
		t->result = LITEST_SYSTEM_ERROR;
	}

	for (size_t i = 0; i < ARRAY_LENGTH(t->read_fds); i++) {
		stringbuf_append_from_fd(&t->logs[i], t->read_fds[i], 1024);
	}
}

__attribute__((noreturn)) void
litest_runner_abort(void)
{
	if (use_jmpbuf) {
		longjmp(jmpbuf, SIGABRT);
	} else {
		abort();
	}
}

static int
litest_runner_run_test(struct litest_runner *runner, struct litest_runner_test *t)
{
	int r;

	t->result = LITEST_SYSTEM_ERROR;

	uint64_t now = 0;
	now_in_us(&now);
	t->times.start_millis = us2ms(now);

	if (runner->max_forks == 0) {
		if (use_jmpbuf && setjmp(jmpbuf) == 0) {
			t->result = litest_runner_test_run(&t->desc);
		} else {
			t->result = LITEST_FAIL;
		}
		r = 0; /* -Wclobbered */
	} else {
		r = litest_runner_fork_test(runner, t);
		if (r >= 0)
			r = litest_runner_test_setup_monitoring(runner, t);
		litest_runner_test_update_errno(t, -r);
	}

	if (r >= 0) {
		list_remove(&t->node);
		list_append(&runner->tests_running, &t->node);
	}

	return r;
}

static void
print_lines(FILE *fp, const char *log, const char *prefix)
{
	size_t nlines = 0;
	_autostrvfree_ char **lines = strv_from_string(log, "\n", &nlines);

	for (size_t i = 0; i < nlines; i++) {
		fprintf(fp, "%s%s\n", prefix, lines[i]);
	}
}

void
_litest_test_param_fetch(const struct litest_test_parameters *params, ...)
{
	struct litest_test_param *p;

	const char *name;

	va_list args;
	va_start(args, params);

	while ((name = va_arg(args, const char *))) {
		bool found = false;
		char type = (char)va_arg(args, int);
		void **ptr = va_arg(args, void *);
		list_for_each(p, &params->test_params, link) {
			if (streq(p->name, name)) {
				if (tolower(p->value.type) != tolower(type))
					litest_abort_msg(
						"Paramter type mismatch: parameter '%s' is of type %c",
						p->name,
						p->value.type);
				found = true;
				multivalue_extract(&p->value, ptr);
				break;
			}
		}
		if (!found)
			litest_abort_msg("Unknown test parameter name '%s'", name);
	}

	va_end(args);
}

struct litest_test_parameters *
litest_test_parameters_new(void)
{
	struct litest_test_parameters *params = zalloc(sizeof *params);
	params->refcnt = 1;
	list_init(&params->test_params);
	return params;
}

struct litest_test_parameters *
litest_test_parameters_unref(struct litest_test_parameters *params)
{
	if (params) {
		assert(params->refcnt > 0);
		if (--params->refcnt == 0) {
			struct litest_test_param *p;
			list_for_each_safe(p, &params->test_params, link) {
				free(p);
			}
			free(params);
		}
	}
	return NULL;
}

static void
litest_runner_log_test_result(struct litest_runner *runner,
			      struct litest_runner_test *t)
{
	const char *color = NULL;
	const char *status = NULL;

	litest_assert_int_ge(t->result, (enum litest_runner_result)LITEST_PASS);
	litest_assert_int_le(t->result, (enum litest_runner_result)LITEST_SYSTEM_ERROR);

	switch (t->result) {
	case LITEST_PASS:
		color = ANSI_BOLD_GREEN;
		break;
	case LITEST_FAIL:
		color = ANSI_BOLD_RED;
		break;
	case LITEST_SKIP:
		color = ANSI_BOLD_YELLOW;
		break;
	case LITEST_NOT_APPLICABLE:
		color = ANSI_BLUE;
		break;
	case LITEST_TIMEOUT:
		color = ANSI_BOLD_CYAN;
		break;
	case LITEST_SYSTEM_ERROR:
		color = ANSI_BOLD_MAGENTA;
		break;
	}

	fprintf(runner->fp, "  - name: \"%s\"\n", t->desc.name);
	int min = t->desc.args.range.lower, max = t->desc.args.range.upper;
	if (range_is_valid(&t->desc.args.range))
		fprintf(runner->fp,
			"    rangeval: %d  # %d..%d\n",
			t->desc.rangeval,
			min,
			max);

	if (t->desc.params) {
		fprintf(runner->fp, "    params:\n");
		struct litest_test_param *p;
		list_for_each(p, &t->desc.params->test_params, link) {
			_autofree_ char *val = multivalue_as_str(&p->value);
			fprintf(runner->fp, "      %s: %s\n", p->name, val);
		}
	}

	fprintf(runner->fp,
		"    duration: %ld  # (ms), total test run time: %02d:%02d\n",
		t->times.end_millis - t->times.start_millis,
		(ms2s(t->times.end_millis - runner->times.start_millis)) / 60,
		(ms2s(t->times.end_millis - runner->times.start_millis)) % 60);

	status = litest_runner_result_as_str(t->result);
	fprintf(runner->fp,
		"    status: %s%s%s\n",
		runner->use_colors ? color : "",
		&status[7], /* skip LITEST_ prefix */
		runner->use_colors ? ANSI_NORMAL : "");

	switch (t->result) {
	case LITEST_PASS:
	case LITEST_SKIP:
	case LITEST_NOT_APPLICABLE:
		if (!runner->verbose)
			return;
		break;
	default:
		break;
	}

	if (t->sig_or_errno > 0)
		fprintf(runner->fp,
			"    signal: %d # SIG%s \n",
			t->sig_or_errno,
			sigabbrev_np(t->sig_or_errno));
	else if (t->sig_or_errno < 0)
		fprintf(runner->fp,
			"    errno: %d # %s\n",
			-t->sig_or_errno,
			strerror(-t->sig_or_errno));
	if (!stringbuf_is_empty(&t->logs[FD_LOG])) {
		fprintf(runner->fp, "    log: |\n");
		print_lines(runner->fp, t->logs[FD_LOG].data, "      ");
	}
	if (!stringbuf_is_empty(&t->logs[FD_STDOUT])) {
		fprintf(runner->fp, "    stdout: |\n");
		print_lines(runner->fp, t->logs[FD_STDOUT].data, "      ");
	}
	if (!stringbuf_is_empty(&t->logs[FD_STDERR])) {
		fprintf(runner->fp, "    stderr: |\n");
		print_lines(runner->fp, t->logs[FD_STDERR].data, "      ");
	}
	if (!stringbuf_is_empty(&t->logs[FD_VALGRIND])) {
		fprintf(runner->fp, "    valgrind: |\n");
		print_lines(runner->fp, t->logs[FD_VALGRIND].data, "      ");
	}
}

struct litest_runner *
litest_runner_new(void)
{
	struct litest_runner *runner = zalloc(sizeof *runner);

	list_init(&runner->tests);
	list_init(&runner->tests_complete);
	list_init(&runner->tests_running);
	runner->timeout = LITEST_RUNNER_DEFAULT_TIMEOUT;
	runner->max_forks = get_nprocs() * 2;
	runner->fp = stderr;

	return runner;
}

void
litest_runner_set_timeout(struct litest_runner *runner, unsigned int timeout)
{
	runner->timeout = timeout;
}

void
litest_runner_set_output_file(struct litest_runner *runner, FILE *fp)
{
	setlinebuf(fp);
	runner->fp = fp;
}

void
litest_runner_set_num_parallel(struct litest_runner *runner, size_t num_jobs)
{
	runner->max_forks = num_jobs;
}

void
litest_runner_set_verbose(struct litest_runner *runner, bool verbose)
{
	runner->verbose = verbose;
}

void
litest_runner_set_use_colors(struct litest_runner *runner, bool use_colors)
{
	runner->use_colors = use_colors;
}

void
litest_runner_set_exit_on_fail(struct litest_runner *runner, bool do_exit)
{
	runner->exit_on_fail = do_exit;
}

void
litest_runner_set_setup_funcs(struct litest_runner *runner,
			      litest_runner_global_setup_func_t setup,
			      litest_runner_global_teardown_func_t teardown,
			      void *userdata)
{
	runner->global.setup = setup;
	runner->global.teardown = teardown;
	runner->global.userdata = userdata;
}

void
litest_runner_add_test(struct litest_runner *runner,
		       const struct litest_runner_test_description *desc)
{
	struct litest_runner_test *t = zalloc(sizeof(*t));

	t->desc = *desc;
	t->epollfd = -1;
	t->pidfd = -1;
	t->timerfd = -1;

	for (int i = 0; i < _FD_LAST; i++) {
		stringbuf_init(&t->logs[i]);
	}

	for (size_t i = 0; i < ARRAY_LENGTH(t->read_fds); i++) {
		t->read_fds[i] = -1;
	}

	list_append(&runner->tests, &t->node);
}

static int
litest_runner_check_finished_tests(struct litest_runner *runner)
{
	struct litest_runner_test *running;
	size_t count = 0;

	list_for_each_safe(running, &runner->tests_running, node) {
		int r = litest_runner_test_check_status(running);
		if (r == -EAGAIN)
			continue;

		uint64_t now = 0;
		now_in_us(&now);
		running->times.end_millis = us2ms(now);

		if (r < 0)
			litest_runner_test_update_errno(running, -r);

		litest_runner_log_test_result(runner, running);
		litest_runner_test_close(running);
		list_remove(&running->node);
		list_append(&runner->tests_complete, &running->node);
		count++;
	}

	return count;
}

static void
runner_sighandler(int sig)
{
	struct litest_runner_test *t;
	struct litest_runner *runner = global_runner;

	list_for_each(t, &runner->tests_running, node) {
		if (t->pid != 0) {
			kill(t->pid, SIGTERM);
			t->pid = 0;
		}
	}

	global_runner->terminating = true;
}

static inline void
setup_sighandler(int sig)
{
	struct sigaction act, oact;
	int rc;

	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, sig);
	act.sa_flags = 0;
	act.sa_handler = runner_sighandler;
	rc = sigaction(sig, &act, &oact);
	litest_assert_int_ne(rc, -1);
}

enum litest_runner_result
litest_runner_run_tests(struct litest_runner *runner)
{
	struct litest_runner_test *t;
	size_t available_jobs = max(runner->max_forks, 1);
	char timestamp[64];
	struct tm *ltime;

	global_runner = runner; /* sigh, need this for signal handling */

	if (runner->global.setup)
		runner->global.setup(runner->global.userdata);

	use_jmpbuf = runner->max_forks == 0;

	setup_sighandler(SIGINT);

	uint64_t now = 0;
	now_in_us(&now);

	runner->times.start_millis = us2ms(now);
	runner->times.start = time(NULL);
	ltime = localtime(&runner->times.start);
	strftime(timestamp, sizeof(timestamp), "%FT%H:%M", ltime);
	fprintf(runner->fp, "start: %ld  # \"%s\"\n", runner->times.start, timestamp);
	fprintf(runner->fp, "jobs: %zd\n", runner->max_forks);
	fprintf(runner->fp, "tests:\n");
	list_for_each_safe(t, &runner->tests, node) {
		int r = litest_runner_run_test(runner, t);
		if (r >= 0) {
			available_jobs--;
		}

		/* Wait for something to become available */
		while (available_jobs == 0 && !runner->terminating) {
			int complete = litest_runner_check_finished_tests(runner);
			available_jobs += complete;
		}

		if (runner->terminating) {
			break;
		}

		if (runner->exit_on_fail) {
			bool do_exit = false;
			struct litest_runner_test *complete;
			list_for_each(complete, &runner->tests_complete, node) {
				switch (complete->result) {
				case LITEST_FAIL:
				case LITEST_SYSTEM_ERROR:
				case LITEST_TIMEOUT:
					do_exit = true;
					break;
				default:
					break;
				}
				if (do_exit)
					break;
			}
			if (do_exit)
				break;
		}
	}

	while (!runner->terminating && !list_empty(&runner->tests_running)) {
		litest_runner_check_finished_tests(runner);
	}

	if (runner->global.teardown)
		runner->global.teardown(runner->global.userdata);

	size_t npass = 0, nfail = 0, nskip = 0, nna = 0;
	size_t ncomplete = 0;

	list_for_each(t, &runner->tests_complete, node) {
		ncomplete++;
		switch (t->result) {
		case LITEST_PASS:
			npass++;
			break;
		case LITEST_NOT_APPLICABLE:
			nna++;
			break;
		case LITEST_FAIL:
		case LITEST_SYSTEM_ERROR:
		case LITEST_TIMEOUT:
			nfail++;
			break;
		case LITEST_SKIP:
			nskip++;
			break;
		}
	}

	runner->times.end = time(NULL);
	ltime = localtime(&runner->times.end);
	strftime(timestamp, sizeof(timestamp), "%FT%H:%M", ltime);
	fprintf(runner->fp, "end: %ld  # \"%s\"\n", runner->times.end, timestamp);
	fprintf(runner->fp,
		"duration: %ld  # (s) %02ld:%02ld\n",
		runner->times.end - runner->times.start,
		(runner->times.end - runner->times.start) / 60,
		(runner->times.end - runner->times.start) % 60);
	fprintf(runner->fp, "summary:\n");
	fprintf(runner->fp, "  completed: %zd\n", ncomplete);
	fprintf(runner->fp, "  pass: %zd\n", npass);
	fprintf(runner->fp, "  na: %zd\n", nna);
	fprintf(runner->fp, "  fail: %zd\n", nfail);
	fprintf(runner->fp, "  skip: %zd\n", nskip);
	if (nfail > 0) {
		fprintf(runner->fp, "  failed:\n");
		list_for_each(t, &runner->tests_complete, node) {
			switch (t->result) {
			case LITEST_FAIL:
			case LITEST_SYSTEM_ERROR:
			case LITEST_TIMEOUT:
				litest_runner_log_test_result(runner, t);
				break;
			default:
				break;
			}
		}
	}

	if (RUNNING_ON_VALGRIND) {
		_autofree_ char *filename = valgrind_logfile(getpid());
		_destroy_(stringbuf) *b = stringbuf_new();

		collect_file(filename, b);
		fprintf(runner->fp, "valgrind:\n");
		print_lines(runner->fp, b->data, "  ");
		fprintf(runner->fp,
			"# Valgrind log is incomplete, see %s for full log\n",
			filename);
	}

	enum litest_runner_result result = LITEST_PASS;

	/* Didn't finish */
	if (!list_empty(&runner->tests) || !list_empty(&runner->tests_running)) {
		result = LITEST_SYSTEM_ERROR;
	} else {
		list_for_each(t, &runner->tests_complete, node) {
			switch (t->result) {
			case LITEST_PASS:
			case LITEST_SKIP:
			case LITEST_NOT_APPLICABLE:
				break;
			default:
				result = LITEST_FAIL;
				break;
			}
		}
	}
	/* Status is always prefixed with LITEST_ */
	fprintf(runner->fp, "  status: %s\n", &litest_runner_result_as_str(result)[7]);

	return result;
}
