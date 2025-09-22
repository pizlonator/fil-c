/*
 * Copyright (C) 1980      Regents of the University of California.
 * Copyright (C) 2013-2019 Karel Zak <kzak@redhat.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>
#include <time.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <assert.h>
#include <inttypes.h>

#include "closestream.h"
#include "nls.h"
#include "c.h"
#include "ttyutils.h"
#include "all-io.h"
#include "monotonic.h"
#include "timeutils.h"
#include "strutils.h"
#include "xalloc.h"
#include "optutils.h"
#include "signames.h"
#include "pty-session.h"
#include "debug.h"

static UL_DEBUG_DEFINE_MASK(script);
UL_DEBUG_DEFINE_MASKNAMES(script) = UL_DEBUG_EMPTY_MASKNAMES;

#define SCRIPT_DEBUG_INIT	(1 << 1)
#define SCRIPT_DEBUG_PTY	(1 << 2)
#define SCRIPT_DEBUG_IO		(1 << 3)
#define SCRIPT_DEBUG_SIGNAL	(1 << 4)
#define SCRIPT_DEBUG_MISC	(1 << 5)
#define SCRIPT_DEBUG_ALL	0xFFFF

#define DBG(m, x)       __UL_DBG(script, SCRIPT_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(script, SCRIPT_DEBUG_, m, x)

#ifdef HAVE_LIBUTEMPTER
# include <utempter.h>
#endif

#define DEFAULT_TYPESCRIPT_FILENAME "typescript"

/*
 * Script is driven by stream (stdout/stdin) activity. It's possible to
 * associate arbitrary number of log files with the stream. We have two basic
 * types of log files: "timing file" (simple or multistream) and "data file"
 * (raw).
 *
 * The same log file maybe be shared between both streams. For example
 * multi-stream timing file is possible to use for stdin as well as for stdout.
 */
enum {
	SCRIPT_FMT_RAW = 1,		/* raw slave/master data */
	SCRIPT_FMT_TIMING_SIMPLE,	/* (classic) in format "<delta> <offset>" */
	SCRIPT_FMT_TIMING_MULTI,	/* (advanced) multiple streams in format "<type> <delta> <offset|etc> */
};

struct script_log {
	FILE	*fp;			/* file pointer (handler) */
	int	format;			/* SCRIPT_FMT_* */
	char	*filename;		/* on command line specified name */
	struct timeval oldtime;		/* previous entry log time (SCRIPT_FMT_TIMING_* only) */
	struct timeval starttime;

	unsigned int	initialized : 1;
};

struct script_stream {
	struct script_log **logs;	/* logs where to write data from stream */
	size_t nlogs;			/* number of logs */
	char ident;			/* stream identifier */
};

struct script_control {
	uint64_t outsz;		/* current output files size */
	uint64_t maxsz;		/* maximum output files size */

	struct script_stream	out;	/* output */
	struct script_stream	in;	/* input */

	struct script_log	*siglog;	/* log for signal entries */
	struct script_log	*infolog;	/* log for info entries */

	const char *ttyname;
	const char *ttytype;
	const char *command;
	char *command_norm;	/* normalized (without \n) */
	int ttycols;
	int ttylines;

	struct ul_pty *pty;	/* pseudo-terminal */
	pid_t child;		/* child pid */
	int childstatus;	/* child process exit value */

	unsigned int
	 append:1,		/* append output */
	 rc_wanted:1,		/* return child exit value */
	 flush:1,		/* flush after each write */
	 quiet:1,		/* suppress most output */
	 force:1,		/* write output to links */
	 isterm:1;		/* is child process running as terminal */
};

static ssize_t log_info(struct script_control *ctl, const char *name, const char *msgfmt, ...)
			__attribute__((__format__ (__printf__, 3, 4)));

static void script_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(script, SCRIPT_DEBUG_, 0, SCRIPT_DEBUG);
}

static void init_terminal_info(struct script_control *ctl)
{
	if (ctl->ttyname || !ctl->isterm)
		return;		/* already initialized */

	get_terminal_dimension(&ctl->ttycols, &ctl->ttylines);
	get_terminal_name(&ctl->ttyname, NULL, NULL);
	get_terminal_type(&ctl->ttytype);
}

/*
 * For tests we want to be able to control time output
 */
#ifdef TEST_SCRIPT
static inline time_t script_time(time_t *t)
{
	const char *str = getenv("SCRIPT_TEST_SECOND_SINCE_EPOCH");
	int64_t sec;

	if (!str || sscanf(str, "%"SCNi64, &sec) != 1)
		return time(t);
	if (t)
		*t = (time_t)sec;
	return (time_t)sec;
}
#else	/* !TEST_SCRIPT */
# define script_time(x) time(x)
#endif

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [file]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Make a typescript of a terminal session.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -I, --log-in <file>           log stdin to file\n"), out);
	fputs(_(" -O, --log-out <file>          log stdout to file (default)\n"), out);
	fputs(_(" -B, --log-io <file>           log stdin and stdout to file\n"), out);
	fputs(USAGE_SEPARATOR, out);

	fputs(_(" -T, --log-timing <file>       log timing information to file\n"), out);
	fputs(_(" -t[<file>], --timing[=<file>] deprecated alias to -T (default file is stderr)\n"), out);
	fputs(_(" -m, --logging-format <name>   force to 'classic' or 'advanced' format\n"), out);
	fputs(USAGE_SEPARATOR, out);

	fputs(_(" -a, --append                  append to the log file\n"), out);
	fputs(_(" -c, --command <command>       run command rather than interactive shell\n"), out);
	fputs(_(" -e, --return                  return exit code of the child process\n"), out);
	fputs(_(" -f, --flush                   run flush after each write\n"), out);
	fputs(_("     --force                   use output file even when it is a link\n"), out);
	fputs(_(" -E, --echo <when>             echo input in session (auto, always or never)\n"), out);
	fputs(_(" -o, --output-limit <size>     terminate if output files exceed size\n"), out);
	fputs(_(" -q, --quiet                   be quiet\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(31));
	fprintf(out, USAGE_MAN_TAIL("script(1)"));

	exit(EXIT_SUCCESS);
}

static struct script_log *get_log_by_name(struct script_stream *stream,
					  const char *name)
{
	size_t i;

	for (i = 0; i < stream->nlogs; i++) {
		struct script_log *log = stream->logs[i];
		if (strcmp(log->filename, name) == 0)
			return log;
	}
	return NULL;
}

static struct script_log *log_associate(struct script_control *ctl,
					struct script_stream *stream,
					const char *filename, int format)
{
	struct script_log *log;

	DBG(MISC, ul_debug("associate %s with stream", filename));

	assert(ctl);
	assert(filename);
	assert(stream);

	log = get_log_by_name(stream, filename);
	if (log)
		return log;	/* already defined */

	log = get_log_by_name(stream == &ctl->out ? &ctl->in : &ctl->out, filename);
	if (!log) {
		/* create a new log */
		log = xcalloc(1, sizeof(*log));
		log->filename = xstrdup(filename);
		log->format = format;
	}

	/* add log to the stream */
	stream->logs = xreallocarray(stream->logs,
			stream->nlogs + 1, sizeof(log));
	stream->logs[stream->nlogs] = log;
	stream->nlogs++;

	/* remember where to write info about signals */
	if (format == SCRIPT_FMT_TIMING_MULTI) {
		if (!ctl->siglog)
			ctl->siglog = log;
		if (!ctl->infolog)
			ctl->infolog = log;
	}

	return log;
}

static int log_close(struct script_control *ctl,
		      struct script_log *log,
		      const char *msg,
		      int status)
{
	int rc = 0;

	if (!log || !log->initialized)
		return 0;

	DBG(MISC, ul_debug("closing %s", log->filename));

	switch (log->format) {
	case SCRIPT_FMT_RAW:
	{
		char buf[FORMAT_TIMESTAMP_MAX];
		time_t tvec = script_time((time_t *)NULL);

		strtime_iso(&tvec, ISO_TIMESTAMP, buf, sizeof(buf));
		if (msg)
			fprintf(log->fp, _("\nScript done on %s [<%s>]\n"), buf, msg);
		else
			fprintf(log->fp, _("\nScript done on %s [COMMAND_EXIT_CODE=\"%d\"]\n"), buf, status);
		break;
	}
	case SCRIPT_FMT_TIMING_MULTI:
	{
		struct timeval now = { 0 }, delta = { 0 };

		gettime_monotonic(&now);
		timersub(&now, &log->starttime, &delta);

		log_info(ctl, "DURATION", "%"PRId64".%06"PRId64,
			(int64_t)delta.tv_sec,
			(int64_t)delta.tv_usec);
		log_info(ctl, "EXIT_CODE", "%d", status);
		break;
	}
	case SCRIPT_FMT_TIMING_SIMPLE:
		break;
	}

	if (close_stream(log->fp) != 0) {
		warn(_("write failed: %s"), log->filename);
		rc = -errno;
	}

	free(log->filename);
	memset(log, 0, sizeof(*log));

	return rc;
}

static int log_flush(struct script_control *ctl __attribute__((__unused__)), struct script_log *log)
{

	if (!log || !log->initialized)
		return 0;

	DBG(MISC, ul_debug("flushing %s", log->filename));

	fflush(log->fp);
	return 0;
}

static void log_free(struct script_control *ctl, struct script_log *log)
{
	size_t i;

	if (!log)
		return;

	/* the same log is possible to reference from more places, remove all
	 * (TODO: maybe use include/list.h to make it more elegant)
	 */
	if (ctl->siglog == log)
		ctl->siglog = NULL;
	else if (ctl->infolog == log)
		ctl->infolog = NULL;

	for (i = 0; i < ctl->out.nlogs; i++) {
		if (ctl->out.logs[i] == log)
			ctl->out.logs[i] = NULL;
	}
	for (i = 0; i < ctl->in.nlogs; i++) {
		if (ctl->in.logs[i] == log)
			ctl->in.logs[i] = NULL;
	}
	free(log);
}

static int log_start(struct script_control *ctl,
		      struct script_log *log)
{
	if (log->initialized)
		return 0;

	DBG(MISC, ul_debug("opening %s", log->filename));

	assert(log->fp == NULL);

	/* open the log */
	log->fp = fopen(log->filename,
			ctl->append && log->format == SCRIPT_FMT_RAW ?
			"a" UL_CLOEXECSTR :
			"w" UL_CLOEXECSTR);
	if (!log->fp) {
		warn(_("cannot open %s"), log->filename);
		return -errno;
	}

	/* write header, etc. */
	switch (log->format) {
	case SCRIPT_FMT_RAW:
	{
		int x = 0;
		char buf[FORMAT_TIMESTAMP_MAX];
		time_t tvec = script_time((time_t *)NULL);

		strtime_iso(&tvec, ISO_TIMESTAMP, buf, sizeof(buf));
		fprintf(log->fp, _("Script started on %s ["), buf);

		if (ctl->command)
			x += fprintf(log->fp, "COMMAND=\"%s\"", ctl->command_norm);

		if (ctl->isterm) {
			init_terminal_info(ctl);

			if (ctl->ttytype)
				x += fprintf(log->fp, "%*sTERM=\"%s\"", !!x, "", ctl->ttytype);
			if (ctl->ttyname)
				x += fprintf(log->fp, "%*sTTY=\"%s\"", !!x, "", ctl->ttyname);

			x += fprintf(log->fp, "%*sCOLUMNS=\"%d\" LINES=\"%d\"", !!x, "",
					ctl->ttycols, ctl->ttylines);
		} else
			fprintf(log->fp, _("%*s<not executed on terminal>"), !!x, "");

		fputs("]\n", log->fp);
		break;
	}
	case SCRIPT_FMT_TIMING_SIMPLE:
	case SCRIPT_FMT_TIMING_MULTI:
		gettime_monotonic(&log->oldtime);
		gettime_monotonic(&log->starttime);
		break;
	}

	log->initialized = 1;
	return 0;
}

static int logging_start(struct script_control *ctl)
{
	size_t i;

	/* start all output logs */
	for (i = 0; i < ctl->out.nlogs; i++) {
		int rc = log_start(ctl, ctl->out.logs[i]);
		if (rc)
			return rc;
	}

	/* start all input logs */
	for (i = 0; i < ctl->in.nlogs; i++) {
		int rc = log_start(ctl, ctl->in.logs[i]);
		if (rc)
			return rc;
	}
	return 0;
}

static ssize_t log_write(struct script_control *ctl,
		      struct script_stream *stream,
		      struct script_log *log,
		      char *obuf, size_t bytes)
{
	int rc;
	ssize_t ssz = 0;
	struct timeval now, delta;

	if (!log->fp)
		return 0;

	DBG(IO, ul_debug(" writing [file=%s]", log->filename));

	switch (log->format) {
	case SCRIPT_FMT_RAW:
		DBG(IO, ul_debug("  log raw data"));
		rc = fwrite_all(obuf, 1, bytes, log->fp);
		if (rc) {
			warn(_("cannot write %s"), log->filename);
			return rc;
		}
		ssz = bytes;
		break;

	case SCRIPT_FMT_TIMING_SIMPLE:
		DBG(IO, ul_debug("  log timing info"));

		gettime_monotonic(&now);
		timersub(&now, &log->oldtime, &delta);
		ssz = fprintf(log->fp, "%"PRId64".%06"PRId64" %zd\n",
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec, bytes);
		if (ssz < 0)
			return -errno;

		log->oldtime = now;
		break;

	case SCRIPT_FMT_TIMING_MULTI:
		DBG(IO, ul_debug("  log multi-stream timing info"));

		gettime_monotonic(&now);
		timersub(&now, &log->oldtime, &delta);
		ssz = fprintf(log->fp, "%c %"PRId64".%06"PRId64" %zd\n",
			stream->ident,
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec, bytes);
		if (ssz < 0)
			return -errno;

		log->oldtime = now;
		break;
	default:
		break;
	}

	if (ctl->flush)
		fflush(log->fp);
	return ssz;
}

static ssize_t log_stream_activity(
			struct script_control *ctl,
			struct script_stream *stream,
			char *buf, size_t bytes)
{
	size_t i;
	ssize_t outsz = 0;

	for (i = 0; i < stream->nlogs; i++) {
		ssize_t ssz = log_write(ctl, stream, stream->logs[i], buf, bytes);

		if (ssz < 0)
			return ssz;
		outsz += ssz;
	}

	return outsz;
}

static ssize_t __attribute__ ((__format__ (__printf__, 3, 4)))
	log_signal(struct script_control *ctl, int signum, const char *msgfmt, ...)
{
	struct script_log *log;
	struct timeval now, delta;
	char msg[BUFSIZ] = {0};
	va_list ap;
	ssize_t sz;

	assert(ctl);

	log = ctl->siglog;
	if (!log)
		return 0;

	assert(log->format == SCRIPT_FMT_TIMING_MULTI);
	DBG(IO, ul_debug("  writing signal to multi-stream timing"));

	gettime_monotonic(&now);
	timersub(&now, &log->oldtime, &delta);

	if (msgfmt) {
		int rc;
		va_start(ap, msgfmt);
		rc = vsnprintf(msg, sizeof(msg), msgfmt, ap);
		va_end(ap);
		if (rc < 0)
			*msg = '\0';;
	}

	if (*msg)
		sz = fprintf(log->fp, "S %"PRId64".%06"PRId64" SIG%s %s\n",
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec,
			signum_to_signame(signum), msg);
	else
		sz = fprintf(log->fp, "S %"PRId64".%06"PRId64" SIG%s\n",
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec,
			signum_to_signame(signum));

	log->oldtime = now;
	return sz;
}

static ssize_t log_info(struct script_control *ctl, const char *name, const char *msgfmt, ...)
{
	struct script_log *log;
	char msg[BUFSIZ] = {0};
	va_list ap;
	ssize_t sz;

	assert(ctl);

	log = ctl->infolog;
	if (!log)
		return 0;

	assert(log->format == SCRIPT_FMT_TIMING_MULTI);
	DBG(IO, ul_debug("  writing info to multi-stream log"));

	if (msgfmt) {
		int rc;
		va_start(ap, msgfmt);
		rc = vsnprintf(msg, sizeof(msg), msgfmt, ap);
		va_end(ap);
		if (rc < 0)
			*msg = '\0';;
	}

	if (*msg)
		sz = fprintf(log->fp, "H %f %s %s\n", 0.0, name, msg);
	else
		sz = fprintf(log->fp, "H %f %s\n", 0.0, name);

	return sz;
}


static void logging_done(struct script_control *ctl, const char *msg)
{
	int status;
	size_t i;

	DBG(MISC, ul_debug("stop logging"));

	if (WIFSIGNALED(ctl->childstatus))
		status = WTERMSIG(ctl->childstatus) + 0x80;
	else
		status = WEXITSTATUS(ctl->childstatus);

	DBG(MISC, ul_debug(" status=%d", status));

	/* close all output logs */
	for (i = 0; i < ctl->out.nlogs; i++) {
		struct script_log *log = ctl->out.logs[i];
		log_close(ctl, log, msg, status);
		log_free(ctl, log);
	}
	free(ctl->out.logs);
	ctl->out.logs = NULL;
	ctl->out.nlogs = 0;

	/* close all input logs */
	for (i = 0; i < ctl->in.nlogs; i++) {
		struct script_log *log = ctl->in.logs[i];
		log_close(ctl, log, msg, status);
		log_free(ctl, log);
	}
	free(ctl->in.logs);
	ctl->in.logs = NULL;
	ctl->in.nlogs = 0;
}

static void callback_child_die(
			void *data,
			pid_t child __attribute__((__unused__)),
			int status)
{
	struct script_control *ctl = (struct script_control *) data;

	ctl->child = (pid_t) -1;
	ctl->childstatus = status;
}

static void callback_child_sigstop(
			void *data __attribute__((__unused__)),
			pid_t child)
{
	DBG(SIGNAL, ul_debug(" child stop by SIGSTOP -- stop parent too"));
	kill(getpid(), SIGSTOP);
	DBG(SIGNAL, ul_debug(" resume"));
	kill(child, SIGCONT);
}

static int callback_log_stream_activity(void *data, int fd, char *buf, size_t bufsz)
{
	struct script_control *ctl = (struct script_control *) data;
	ssize_t ssz = 0;

	DBG(IO, ul_debug("stream activity callback"));

	/* from stdin (user) to command */
	if (fd == STDIN_FILENO)
		ssz = log_stream_activity(ctl, &ctl->in, buf, (size_t) bufsz);

	/* from command (master) to stdout and log */
	else if (fd == ul_pty_get_childfd(ctl->pty))
		ssz = log_stream_activity(ctl, &ctl->out, buf, (size_t) bufsz);

	if (ssz < 0)
		return (int) ssz;

	DBG(IO, ul_debug(" append %zd bytes [summary=%" PRIu64 ", max=%" PRIu64 "]", ssz,
				ctl->outsz, ctl->maxsz));

	ctl->outsz += ssz;

	/* check output limit */
	if (ctl->maxsz != 0 && ctl->outsz >= ctl->maxsz) {
		if (!ctl->quiet)
			printf(_("Script terminated, max output files size %"PRIu64" exceeded.\n"), ctl->maxsz);
		DBG(IO, ul_debug("output size %"PRIu64", exceeded limit %"PRIu64, ctl->outsz, ctl->maxsz));
		logging_done(ctl, _("max output size exceeded"));
		return 1;
	}
	return 0;
}

static int callback_log_signal(void *data, struct signalfd_siginfo *info, void *sigdata)
{
	struct script_control *ctl = (struct script_control *) data;
	ssize_t ssz = 0;

	switch (info->ssi_signo) {
	case SIGWINCH:
	{
		struct winsize *win = (struct winsize *) sigdata;
		ssz = log_signal(ctl, info->ssi_signo, "ROWS=%d COLS=%d",
					win->ws_row, win->ws_col);
		break;
	}
	case SIGTERM:
		/* fallthrough */
	case SIGINT:
		/* fallthrough */
	case SIGQUIT:
		ssz = log_signal(ctl, info->ssi_signo, NULL);
		break;
	default:
		/* no log */
		break;
	}

	return ssz < 0 ? ssz : 0;
}

static int callback_flush_logs(void *data)
{
	struct script_control *ctl = (struct script_control *) data;
	size_t i;

	for (i = 0; i < ctl->out.nlogs; i++) {
		int rc = log_flush(ctl, ctl->out.logs[i]);
		if (rc)
			return rc;
	}

	for (i = 0; i < ctl->in.nlogs; i++) {
		int rc = log_flush(ctl, ctl->in.logs[i]);
		if (rc)
			return rc;
	}
	return 0;
}

static void die_if_link(struct script_control *ctl, const char *filename)
{
	struct stat s;

	if (ctl->force)
		return;
	if (lstat(filename, &s) == 0 && (S_ISLNK(s.st_mode) || s.st_nlink > 1))
		errx(EXIT_FAILURE,
		     _("output file `%s' is a link\n"
		       "Use --force if you really want to use it.\n"
		       "Program not started."), filename);
}

int main(int argc, char **argv)
{
	struct script_control ctl = {
		.out = { .ident = 'O' },
		.in  = { .ident = 'I' },
	};
	struct ul_pty_callbacks *cb;
	int ch, format = 0, caught_signal = 0, rc = 0, echo = 1;
	const char *outfile = NULL, *infile = NULL;
	const char *timingfile = NULL, *shell = NULL;

	enum { FORCE_OPTION = CHAR_MAX + 1 };

	static const struct option longopts[] = {
		{"append", no_argument, NULL, 'a'},
		{"command", required_argument, NULL, 'c'},
		{"echo", required_argument, NULL, 'E'},
		{"return", no_argument, NULL, 'e'},
		{"flush", no_argument, NULL, 'f'},
		{"force", no_argument, NULL, FORCE_OPTION,},
		{"log-in", required_argument, NULL, 'I'},
		{"log-out", required_argument, NULL, 'O'},
		{"log-io", required_argument, NULL, 'B'},
		{"log-timing", required_argument, NULL, 'T'},
		{"logging-format", required_argument, NULL, 'm'},
		{"output-limit", required_argument, NULL, 'o'},
		{"quiet", no_argument, NULL, 'q'},
		{"timing", optional_argument, NULL, 't'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'T', 't' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	setlocale(LC_ALL, "");
	/*
	 * script -t prints time delays as floating point numbers.  The example
	 * program (scriptreplay) that we provide to handle this timing output
	 * is a perl script, and does not handle numbers in locale format (not
	 * even when "use locale;" is added).  So, since these numbers are not
	 * for human consumption, it seems easiest to set LC_NUMERIC here.
	 */
	setlocale(LC_NUMERIC, "C");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	script_init_debug();
	ON_DBG(PTY, ul_pty_init_debug(0xFFFF));

	ctl.isterm = isatty(STDIN_FILENO);

	while ((ch = getopt_long(argc, argv, "aB:c:eE:fI:O:o:qm:T:t::Vh", longopts, NULL)) != -1) {

		err_exclusive_options(ch, longopts, excl, excl_st);

		switch (ch) {
		case 'a':
			ctl.append = 1;
			break;
		case 'c':
			ctl.command = optarg;
			ctl.command_norm = xstrdup(ctl.command);
			strrep(ctl.command_norm, '\n', ' ');
			break;
		case 'E':
			if (strcmp(optarg, "auto") == 0)
				; /* keep default */
			else if (strcmp(optarg, "never") == 0)
				echo = 0;
			else if (strcmp(optarg, "always") == 0)
				echo = 1;
			else
				errx(EXIT_FAILURE, _("unssuported echo mode: '%s'"), optarg);
			break;
		case 'e':
			ctl.rc_wanted = 1;
			break;
		case 'f':
			ctl.flush = 1;
			break;
		case FORCE_OPTION:
			ctl.force = 1;
			break;
		case 'B':
			log_associate(&ctl, &ctl.in, optarg, SCRIPT_FMT_RAW);
			log_associate(&ctl, &ctl.out, optarg, SCRIPT_FMT_RAW);
			infile = outfile = optarg;
			break;
		case 'I':
			log_associate(&ctl, &ctl.in, optarg, SCRIPT_FMT_RAW);
			infile = optarg;
			break;
		case 'O':
			log_associate(&ctl, &ctl.out, optarg, SCRIPT_FMT_RAW);
			outfile = optarg;
			break;
		case 'o':
			ctl.maxsz = strtosize_or_err(optarg, _("failed to parse output limit size"));
			break;
		case 'q':
			ctl.quiet = 1;
			break;
		case 'm':
			if (strcasecmp(optarg, "classic") == 0)
				format = SCRIPT_FMT_TIMING_SIMPLE;
			else if (strcasecmp(optarg, "advanced") == 0)
				format = SCRIPT_FMT_TIMING_MULTI;
			else
				errx(EXIT_FAILURE, _("unsupported logging format: '%s'"), optarg);
			break;
		case 't':
			if (optarg && *optarg == '=')
				optarg++;
			timingfile = optarg ? optarg : "/dev/stderr";
			break;
		case 'T' :
			timingfile = optarg;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	/* default if no --log-* specified */
	if (!outfile && !infile) {
		if (argc > 0) {
			outfile = argv[0];
			argc--;
			argv++;
		} else {
			die_if_link(&ctl, DEFAULT_TYPESCRIPT_FILENAME);
			outfile = DEFAULT_TYPESCRIPT_FILENAME;
		}

		/* associate stdout with typescript file */
		log_associate(&ctl, &ctl.out, outfile, SCRIPT_FMT_RAW);
	}

	if (argc > 0) {
		/* only one filename is accepted. if --log-out was given,
		 * freestanding filename is ignored */
		warnx(_("unexpected number of arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	if (timingfile) {
		/* the old SCRIPT_FMT_TIMING_SIMPLE should be used when
		 * recoding output only (just for backward compatibility),
		 * otherwise switch to new format. */
		if (!format)
			format = infile || (outfile && infile) ?
					SCRIPT_FMT_TIMING_MULTI :
					SCRIPT_FMT_TIMING_SIMPLE;

		else if (format == SCRIPT_FMT_TIMING_SIMPLE && outfile && infile)
			errx(EXIT_FAILURE, _("log multiple streams is mutually "
					     "exclusive with 'classic' format"));
		if (outfile)
			log_associate(&ctl, &ctl.out, timingfile, format);
		if (infile)
			log_associate(&ctl, &ctl.in, timingfile, format);
	}

	shell = getenv("SHELL");
	if (!shell)
		shell = _PATH_BSHELL;

	ctl.pty = ul_new_pty(ctl.isterm);
	if (!ctl.pty)
		err(EXIT_FAILURE, "failed to allocate PTY handler");

	ul_pty_slave_echo(ctl.pty, echo);

	ul_pty_set_callback_data(ctl.pty, (void *) &ctl);
	cb = ul_pty_get_callbacks(ctl.pty);
	cb->child_die = callback_child_die;
	cb->child_sigstop = callback_child_sigstop;
	cb->log_stream_activity = callback_log_stream_activity;
	cb->log_signal = callback_log_signal;
	cb->flush_logs = callback_flush_logs;

	if (!ctl.quiet) {
		printf(_("Script started"));
		if (outfile)
			printf(_(", output log file is '%s'"), outfile);
		if (infile)
			printf(_(", input log file is '%s'"), infile);
		if (timingfile)
			printf(_(", timing file is '%s'"), timingfile);
		printf(_(".\n"));
	}


	if (ul_pty_setup(ctl.pty))
		err(EXIT_FAILURE, _("failed to create pseudo-terminal"));

#ifdef HAVE_LIBUTEMPTER
	utempter_add_record(ul_pty_get_childfd(ctl.pty), NULL);
#endif

	if (ul_pty_signals_setup(ctl.pty))
		err(EXIT_FAILURE, _("failed to initialize signals handler"));
	fflush(stdout);

	/*
	 * We have terminal, do not use err() from now, use "goto done"
	 */

	switch ((int) (ctl.child = fork())) {
	case -1: /* error */
		warn(_("cannot create child process"));
		rc = -errno;
		goto done;

	case 0: /* child */
	{
		const char *shname;

		ul_pty_init_slave(ctl.pty);

		signal(SIGTERM, SIG_DFL); /* because /etc/csh.login */

		shname = strrchr(shell, '/');
		shname = shname ? shname + 1 : shell;

		if (access(shell, X_OK) == 0) {
			if (ctl.command)
				execl(shell, shname, "-c", ctl.command, (char *)NULL);
			else
				execl(shell, shname, "-i", (char *)NULL);
		} else {
			if (ctl.command)
				execlp(shname, shname, "-c", ctl.command, (char *)NULL);
			else
				execlp(shname, shname, "-i", (char *)NULL);
		}

		err(EXIT_FAILURE, "failed to execute %s", shell);
		break;
	}
	default:
		break;
	}

	/* parent */
	ul_pty_set_child(ctl.pty, ctl.child);

	rc = logging_start(&ctl);
	if (rc)
		goto done;

	/* add extra info to advanced timing file */
	if (timingfile && format == SCRIPT_FMT_TIMING_MULTI) {
		char buf[FORMAT_TIMESTAMP_MAX];
		time_t tvec = script_time((time_t *)NULL);

		strtime_iso(&tvec, ISO_TIMESTAMP, buf, sizeof(buf));
		log_info(&ctl, "START_TIME", "%s", buf);

		if (ctl.isterm) {
			init_terminal_info(&ctl);
			log_info(&ctl, "TERM", "%s", ctl.ttytype);
			log_info(&ctl, "TTY", "%s", ctl.ttyname);
			log_info(&ctl, "COLUMNS", "%d", ctl.ttycols);
			log_info(&ctl, "LINES", "%d", ctl.ttylines);
		}
		log_info(&ctl, "SHELL", "%s", shell);
		if (ctl.command)
			log_info(&ctl, "COMMAND", "%s", ctl.command_norm);
		log_info(&ctl, "TIMING_LOG", "%s", timingfile);
		if (outfile)
			log_info(&ctl, "OUTPUT_LOG", "%s", outfile);
		if (infile)
			log_info(&ctl, "INPUT_LOG", "%s", infile);
	}

        /* this is the main loop */
	rc = ul_pty_proxy_master(ctl.pty);

	/* all done; cleanup and kill */
	caught_signal = ul_pty_get_delivered_signal(ctl.pty);

	if (!caught_signal && ctl.child != (pid_t)-1)
		ul_pty_wait_for_child(ctl.pty);	/* final wait */

	if (caught_signal && ctl.child != (pid_t)-1) {
		fprintf(stderr, "\nSession terminated, killing shell...");
		kill(ctl.child, SIGTERM);
		sleep(2);
		kill(ctl.child, SIGKILL);
		fprintf(stderr, " ...killed.\n");
	}

done:
	ul_pty_cleanup(ctl.pty);
	logging_done(&ctl, NULL);

	if (!ctl.quiet)
		printf(_("Script done.\n"));

#ifdef HAVE_LIBUTEMPTER
	if (ul_pty_get_childfd(ctl.pty) >= 0)
		utempter_remove_record(ul_pty_get_childfd(ctl.pty));
#endif
	ul_free_pty(ctl.pty);

	/* default exit code */
	rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;

	/* exit code based on child status */
	if (ctl.rc_wanted && rc == EXIT_SUCCESS) {
		if (WIFSIGNALED(ctl.childstatus))
			rc = WTERMSIG(ctl.childstatus) + 0x80;
		else
			rc = WEXITSTATUS(ctl.childstatus);
	}

	DBG(MISC, ul_debug("done [rc=%d]", rc));
	return rc;
}
