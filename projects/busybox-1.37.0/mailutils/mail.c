/* vi: set sw=4 ts=4: */
/*
 * helper routines
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#if defined(__linux__)
# include <sys/prctl.h>
# define PRCTL
#elif defined(__FreeBSD__)
# include <sys/procctl.h>
# define PROCCTL
#endif
#include "libbb.h"
#include "mail.h"

// common signal handler
static void signal_handler(int signo)
{
	if (SIGALRM == signo) {
		bb_simple_error_msg_and_die("timed out");
	}

	// SIGCHLD. reap the zombie if we expect one
	if (G.helper_pid == 0)
		return;
#define status signo
	if (safe_waitpid(G.helper_pid, &status, WNOHANG) > 0) {
		G.helper_pid = 0;
		if (WIFSIGNALED(status))
			bb_error_msg_and_die("helper killed by signal %u", WTERMSIG(status));
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			bb_error_msg_and_die("helper exited (%u)", WEXITSTATUS(status));
	}
#undef status
}

void FAST_FUNC launch_helper(const char **argv)
{
	pid_t pid;
	struct fd_pair child_out;
	struct fd_pair child_in;

	xpiped_pair(child_out);
	xpiped_pair(child_in);

	// NB: handler must be installed before vfork
	bb_signals(0
		+ (1 << SIGCHLD)
		+ (1 << SIGALRM)
		, signal_handler);

	fflush_all();
	pid = xvfork();
	if (pid == 0) {
		// child
		close(child_in.wr);
		close(child_out.rd);
		xmove_fd(child_in.rd, STDIN_FILENO);
		xmove_fd(child_out.wr, STDOUT_FILENO);
		// if parent dies, get SIGTERM
#if defined(PRCTL)
		prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
#elif defined(PROCCTL)
		{
			int signum = SIGTERM;
			procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &signum);
		}
#endif
		// try to execute connection helper
		// NB: SIGCHLD & SIGALRM revert to SIG_DFL on exec
		BB_EXECVP_or_die((char**)argv);
	}
	G.helper_pid = pid;
	close(child_out.wr);
	close(child_in.rd);
	xmove_fd(child_out.rd, STDIN_FILENO);
	xmove_fd(child_in.wr, STDOUT_FILENO);

	// parent goes on
}

void FAST_FUNC send_r_n(const char *s)
{
	if (G.verbose)
		bb_error_msg("send:'%s'", s);
	printf("%s\r\n", s);
}

char* FAST_FUNC send_mail_command(const char *fmt, const char *param)
{
	char *msg;
	if (G.timeout)
		alarm(G.timeout);
	msg = (char*)fmt;
	if (fmt) {
		msg = xasprintf(fmt, param);
		send_r_n(msg);
	}
	fflush_all();
	return msg;
}

// NB: parse_url can modify url[] (despite const), but only if '@' is there
/*
static char* FAST_FUNC parse_url(char *url, char **user, char **pass)
{
	// parse [user[:pass]@]host
	// return host
	char *s = strchr(url, '@');
	*user = *pass = NULL;
	if (s) {
		*s++ = '\0';
		*user = url;
		url = s;
		s = strchr(*user, ':');
		if (s) {
			*s++ = '\0';
			*pass = s;
		}
	}
	return url;
}
*/

static void encode_n_base64(const char *fname, const char *text, size_t len)
{
	enum {
		SRC_BUF_SIZE = 57,  /* This *MUST* be a multiple of 3 */
		DST_BUF_SIZE = 4 * ((SRC_BUF_SIZE + 2) / 3),
	};
#define src_buf text
	char src[SRC_BUF_SIZE];
	FILE *fp = fp;
	char dst_buf[DST_BUF_SIZE + 1];

	if (fname) {
		fp = (NOT_LONE_DASH(fname)) ? xfopen_for_read(fname) : stdin;
		src_buf = src;
	}

	while (1) {
		size_t size;
		if (fname) {
			size = fread((char *)src_buf, 1, SRC_BUF_SIZE, fp);
			if ((ssize_t)size < 0)
				bb_simple_perror_msg_and_die(bb_msg_read_error);
		} else {
			size = len;
			if (len > SRC_BUF_SIZE)
				size = SRC_BUF_SIZE;
		}
		if (!size)
			break;
		// encode the buffer we just read in
		bb_uuencode(dst_buf, src_buf, size, bb_uuenc_tbl_base64);
		if (fname) {
			puts("");
		} else {
			src_buf += size;
			len -= size;
		}
		fwrite(dst_buf, 1, 4 * ((size + 2) / 3), stdout);
	}
	if (fname && NOT_LONE_DASH(fname))
		fclose(fp);
#undef src_buf
}

void FAST_FUNC printstr_base64(const char *text)
{
	encode_n_base64(NULL, text, strlen(text));
}

void FAST_FUNC printbuf_base64(const char *text, unsigned len)
{
	encode_n_base64(NULL, text, len);
}

void FAST_FUNC printfile_base64(const char *fname)
{
	encode_n_base64(fname, NULL, 0);
}

/*
 * get username and password from a file descriptor
 */
void FAST_FUNC get_cred_or_die(int fd)
{
	if (isatty(fd)) {
		G.user = bb_ask_noecho(fd, /* timeout: */ 0, "User: ");
		G.pass = bb_ask_noecho(fd, /* timeout: */ 0, "Password: ");
	} else {
		G.user = xmalloc_reads(fd, /* maxsize: */ NULL);
		G.pass = xmalloc_reads(fd, /* maxsize: */ NULL);
	}
	if (!G.user || !*G.user || !G.pass)
		bb_simple_error_msg_and_die("no username or password");
}
