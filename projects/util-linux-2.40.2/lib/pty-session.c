/*
 * This is pseudo-terminal container for child process where parent creates a
 * proxy between the current std{in,out,etrr} and the child's pty. Advantages:
 *
 * - child has no access to parent's terminal (e.g. su --pty)
 * - parent can log all traffic between user and child's terminal (e.g. script(1))
 * - it's possible to start commands on terminal although parent has no terminal
 *
 * This code is in the public domain; do with it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com> in Jul 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <inttypes.h>

#include "c.h"
#include "all-io.h"
#include "ttyutils.h"
#include "pty-session.h"
#include "monotonic.h"
#include "debug.h"

static UL_DEBUG_DEFINE_MASK(ulpty);
UL_DEBUG_DEFINE_MASKNAMES(ulpty) = UL_DEBUG_EMPTY_MASKNAMES;

#define ULPTY_DEBUG_INIT	(1 << 1)
#define ULPTY_DEBUG_SETUP	(1 << 2)
#define ULPTY_DEBUG_SIG		(1 << 3)
#define ULPTY_DEBUG_IO		(1 << 4)
#define ULPTY_DEBUG_DONE	(1 << 5)
#define ULPTY_DEBUG_ALL		0xFFFF

#define DBG(m, x)       __UL_DBG(ulpty, ULPTY_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(ulpty, ULPTY_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK   UL_DEBUG_MASK(ulpty)
#include "debugobj.h"

void ul_pty_init_debug(int mask)
{
	if (ulpty_debug_mask)
		return;
	__UL_INIT_DEBUG_FROM_ENV(ulpty, ULPTY_DEBUG_, mask, ULPTY_DEBUG);
}

struct ul_pty *ul_new_pty(int is_stdin_tty)
{
	struct ul_pty *pty = calloc(1, sizeof(*pty));

	if (!pty)
		return NULL;

	DBG(SETUP, ul_debugobj(pty, "alloc handler"));
	pty->isterm = is_stdin_tty;
	pty->master = -1;
	pty->slave = -1;
	pty->sigfd = -1;
	pty->child = (pid_t) -1;

	return pty;
}

void ul_free_pty(struct ul_pty *pty)
{
	struct ul_pty_child_buffer *hd;

	while ((hd = pty->child_buffer_head)) {
		pty->child_buffer_head = hd->next;
		free(hd);
	}

	while ((hd = pty->free_buffers)) {
		pty->free_buffers = hd->next;
		free(hd);
	}
	free(pty);
}

void ul_pty_slave_echo(struct ul_pty *pty, int enable)
{
	assert(pty);
	pty->slave_echo = enable ? 1 : 0;
}

int ul_pty_get_delivered_signal(struct ul_pty *pty)
{
	assert(pty);
	return pty->delivered_signal;
}

struct ul_pty_callbacks *ul_pty_get_callbacks(struct ul_pty *pty)
{
	assert(pty);
	return &pty->callbacks;
}

void ul_pty_set_callback_data(struct ul_pty *pty, void *data)
{
	assert(pty);
	pty->callback_data = data;
}

void ul_pty_set_child(struct ul_pty *pty, pid_t child)
{
	assert(pty);
	pty->child = child;
}

int ul_pty_get_childfd(struct ul_pty *pty)
{
	assert(pty);
	return pty->master;
}

pid_t ul_pty_get_child(struct ul_pty *pty)
{
	assert(pty);
	return pty->child;
}

/* it's active when signals are redirected to sigfd */
int ul_pty_is_running(struct ul_pty *pty)
{
	assert(pty);
	return pty->sigfd >= 0;
}

void ul_pty_set_mainloop_time(struct ul_pty *pty, struct timeval *tv)
{
	assert(pty);
	if (!tv) {
		DBG(IO, ul_debugobj(pty, "mainloop time: clear"));
		timerclear(&pty->next_callback_time);
	} else {
		pty->next_callback_time.tv_sec = tv->tv_sec;
		pty->next_callback_time.tv_usec = tv->tv_usec;
		DBG(IO, ul_debugobj(pty, "mainloop time: %"PRId64".%06"PRId64,
				(int64_t) tv->tv_sec, (int64_t) tv->tv_usec));
	}
}

static void pty_signals_cleanup(struct ul_pty *pty)
{
	if (pty->sigfd != -1)
		close(pty->sigfd);
	pty->sigfd = -1;

	/* restore original setting */
	sigprocmask(SIG_SETMASK, &pty->orgsig, NULL);
}

/* call me before fork() */
int ul_pty_setup(struct ul_pty *pty)
{
	struct termios attrs;
	int rc = 0;

	assert(pty->sigfd == -1);

	/* save the current signals setting (to make ul_pty_cleanup() usable,
	 * otherwise see ul_pty_signals_setup() */
	sigprocmask(0, NULL, &pty->orgsig);

	if (pty->isterm) {
	        DBG(SETUP, ul_debugobj(pty, "create for terminal"));

		/* original setting of the current terminal */
		if (tcgetattr(STDIN_FILENO, &pty->stdin_attrs) != 0) {
			rc = -errno;
			goto done;
		}

		attrs = pty->stdin_attrs;
		if (pty->slave_echo)
			attrs.c_lflag |= ECHO;
		else
			attrs.c_lflag &= ~ECHO;

		ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&pty->win);
		/* create master+slave */
		rc = openpty(&pty->master, &pty->slave, NULL, &attrs, &pty->win);
		if (rc)
			goto done;

		/* set the current terminal to raw mode; pty_cleanup() reverses this change on exit */
		cfmakeraw(&attrs);
		tcsetattr(STDIN_FILENO, TCSANOW, &attrs);
	} else {
		DBG(SETUP, ul_debugobj(pty, "create for non-terminal"));

		rc = openpty(&pty->master, &pty->slave, NULL, NULL, NULL);
		if (rc)
			goto done;

		tcgetattr(pty->slave, &attrs);

		if (pty->slave_echo)
			attrs.c_lflag |= ECHO;
		else
			attrs.c_lflag &= ~ECHO;

		tcsetattr(pty->slave, TCSANOW, &attrs);
	}

	fcntl(pty->master, F_SETFL, O_NONBLOCK);

done:
	if (rc)
		ul_pty_cleanup(pty);

	DBG(SETUP, ul_debugobj(pty, "pty setup done [master=%d, slave=%d, rc=%d]",
				pty->master, pty->slave, rc));
	return rc;
}

/* call me before fork() */
int ul_pty_signals_setup(struct ul_pty *pty)
{
	sigset_t ourset;
	int rc = 0;

	assert(pty->sigfd == -1);

	/* save the current signals setting */
	sigprocmask(0, NULL, &pty->orgsig);

	sigfillset(&ourset);
	if (sigprocmask(SIG_BLOCK, &ourset, NULL)) {
		rc = -errno;
		goto done;
	}

	sigemptyset(&ourset);
	sigaddset(&ourset, SIGCHLD);
	sigaddset(&ourset, SIGWINCH);
	sigaddset(&ourset, SIGALRM);
	sigaddset(&ourset, SIGHUP);
	sigaddset(&ourset, SIGTERM);
	sigaddset(&ourset, SIGINT);
	sigaddset(&ourset, SIGQUIT);

	if (pty->callbacks.flush_logs)
		sigaddset(&ourset, SIGUSR1);

	if ((pty->sigfd = signalfd(-1, &ourset, SFD_CLOEXEC)) < 0)
		rc = -errno;
done:
	if (rc)
		ul_pty_cleanup(pty);

	DBG(SETUP, ul_debugobj(pty, "pty signals setup done [rc=%d]", rc));
	return rc;
}

/* cleanup in parent process */
void ul_pty_cleanup(struct ul_pty *pty)
{
	struct termios rtt;

	pty_signals_cleanup(pty);

	if (pty->master == -1 || !pty->isterm)
		return;

	DBG(DONE, ul_debugobj(pty, "cleanup"));
	rtt = pty->stdin_attrs;
	tcsetattr(STDIN_FILENO, TCSADRAIN, &rtt);
}

int ul_pty_chownmod_slave(struct ul_pty *pty, uid_t uid, gid_t gid, mode_t mode)
{
	if (fchown(pty->slave, uid, gid))
		return -errno;
	if (fchmod(pty->slave, mode))
		return -errno;
	return 0;
}

/* call me in child process */
void ul_pty_init_slave(struct ul_pty *pty)
{
	DBG(SETUP, ul_debugobj(pty, "initialize slave"));

	setsid();

	ioctl(pty->slave, TIOCSCTTY, 1);
	close(pty->master);

	dup2(pty->slave, STDIN_FILENO);
	dup2(pty->slave, STDOUT_FILENO);
	dup2(pty->slave, STDERR_FILENO);

	close(pty->slave);

	if (pty->sigfd >= 0)
		close(pty->sigfd);

	pty->slave = -1;
	pty->master = -1;
	pty->sigfd = -1;

	sigprocmask(SIG_SETMASK, &pty->orgsig, NULL);

	DBG(SETUP, ul_debugobj(pty, "... initialize slave done"));
}

static int write_output(char *obuf, ssize_t bytes)
{
	DBG(IO, ul_debug(" writing output"));

	if (write_all(STDOUT_FILENO, obuf, bytes)) {
		DBG(IO, ul_debug("  writing output *failed*"));
		return -errno;
	}

	return 0;
}

static int schedule_child_write(struct ul_pty *pty, char *buf, size_t bufsz, int final)
{
	struct ul_pty_child_buffer *stash;

	if (pty->free_buffers) {
		stash = pty->free_buffers;
		pty->free_buffers = stash->next;
		memset(stash, 0, sizeof(*stash));
	} else
		stash = calloc(1, sizeof(*stash));
	if (!stash)
		return -1;

	assert(bufsz <= sizeof(stash->buf));

	memcpy(stash->buf, buf, bufsz);
	stash->size = bufsz;
	stash->final_input = final ? 1 : 0;

	if (pty->child_buffer_head)
		pty->child_buffer_tail = pty->child_buffer_tail->next = stash;
	else
		pty->child_buffer_head = pty->child_buffer_tail = stash;
	return 0;
}

/*
 * The pty is usually faster than shell, so it's a good idea to wait until
 * the previous message has been already read by shell from slave before we
 * write to master. This is necessary especially for EOF situation when we can
 * send EOF to master before shell is fully initialized, to workaround this
 * problem we wait until slave is empty. For example:
 *
 *   echo "date" | su --pty
 *
 * Unfortunately, the child (usually shell) can ignore stdin at all, so we
 * don't wait forever to avoid dead locks...
 *
 * Note that su --pty is primarily designed for interactive sessions as it
 * maintains master+slave tty stuff within the session. Use pipe to write to
 * pty and assume non-interactive (tee-like) behavior is NOT well supported.
 */
static void drain_child_buffers(struct ul_pty *pty)
{
	unsigned int tries = 0;
	struct pollfd fd = { .fd = pty->slave, .events = POLLIN };

	DBG(IO, ul_debugobj(pty, " waiting for empty slave"));
	while (poll(&fd, 1, 10) == 1 && tries < 8) {
		DBG(IO, ul_debugobj(pty, "   slave is not empty"));
		xusleep(250000);
		tries++;
	}
	if (tries < 8)
		DBG(IO, ul_debugobj(pty, "   slave is empty now"));

	DBG(IO, ul_debugobj(pty, " sending EOF to master"));
}

static int flush_child_buffers(struct ul_pty *pty, int *anything)
{
	int rc = 0, any = 0;

	while (pty->child_buffer_head) {
		struct ul_pty_child_buffer *hd = pty->child_buffer_head;
		ssize_t ret;

		if (hd->final_input)
			drain_child_buffers(pty);

		DBG(IO, ul_debugobj(hd, " stdin --> master trying %zu bytes", hd->size - hd->cursor));

		ret = write(pty->master, hd->buf + hd->cursor, hd->size - hd->cursor);
		if (ret == -1) {
			DBG(IO, ul_debugobj(hd, "   EAGAIN"));
			if (!(errno == EINTR || errno == EAGAIN))
				rc = -errno;
			goto out;
		}
		DBG(IO, ul_debugobj(hd, "   wrote %zd", ret));
		any = 1;
		hd->cursor += ret;

		if (hd->cursor == hd->size) {
			pty->child_buffer_head = hd->next;
			if (!hd->next)
				pty->child_buffer_tail = NULL;

			hd->next = pty->free_buffers;
			pty->free_buffers = hd;
		}
	}
out:
	/* without sync write_output() will write both input &
	 * shell output that looks like double echoing */
	if (any)
		fdatasync(pty->master);

	if (anything)
		*anything = any;
	return rc;
}

void ul_pty_write_eof_to_child(struct ul_pty *pty)
{
	char c = DEF_EOF;
	schedule_child_write(pty, &c, sizeof(char), 1);
}

static int mainloop_callback(struct ul_pty *pty)
{
	int rc;

	if (!pty->callbacks.mainloop)
		return 0;

	DBG(IO, ul_debugobj(pty, "calling mainloop callback"));
	rc = pty->callbacks.mainloop(pty->callback_data);

	DBG(IO, ul_debugobj(pty, " callback done [rc=%d]", rc));
	return rc;
}

static int handle_io(struct ul_pty *pty, int fd, int *eof)
{
	char buf[BUFSIZ];
	ssize_t bytes;
	int rc = 0;
	sigset_t set;

	DBG(IO, ul_debugobj(pty, " handle I/O on fd=%d", fd));
	*eof = 0;

	sigemptyset(&set);
	sigaddset(&set, SIGTTIN);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	/* read from active FD */
	bytes = read(fd, buf, sizeof(buf));
	sigprocmask(SIG_BLOCK, &set, NULL);
	if (bytes == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		return -errno;
	}

	if (bytes == 0) {
		*eof = 1;
		return 0;
	}

	/* from stdin (user) to command */
	if (fd == STDIN_FILENO) {
		DBG(IO, ul_debugobj(pty, " stdin --> master %zd bytes queued", bytes));

		if (schedule_child_write(pty, buf, bytes, 0))
			return -errno;

	/* from command (master) to stdout */
	} else if (fd == pty->master) {
		DBG(IO, ul_debugobj(pty, " master --> stdout %zd bytes", bytes));
		write_output(buf, bytes);
	}

	if (pty->callbacks.log_stream_activity)
		rc = pty->callbacks.log_stream_activity(
					pty->callback_data, fd, buf, bytes);

	return rc;
}

void ul_pty_wait_for_child(struct ul_pty *pty)
{
	int status;
	pid_t pid;
	int options = 0;

	if (pty->child == (pid_t) -1)
		return;

	DBG(SIG, ul_debug("waiting for child [child=%d]", (int) pty->child));

	if (ul_pty_is_running(pty)) {
		/* wait for specific child */
		options = WNOHANG;
		for (;;) {
			pid = waitpid(pty->child, &status, options);
			DBG(SIG, ul_debug(" waitpid done [rc=%d]", (int) pid));
			if (pid != (pid_t) - 1) {
				if (pty->callbacks.child_die)
					pty->callbacks.child_die(
							pty->callback_data,
							pty->child, status);
				ul_pty_set_child(pty, (pid_t) -1);
			} else
				break;
		}
	} else {
		/* final wait */
		while ((pid = waitpid(-1, &status, options)) > 0) {
			DBG(SIG, ul_debug(" waitpid done [rc=%d]", (int) pid));
			if (pid == pty->child) {
				if (pty->callbacks.child_die)
					pty->callbacks.child_die(
							pty->callback_data,
							pty->child, status);
				ul_pty_set_child(pty, (pid_t) -1);
			}
		}
	}
}

static int handle_signal(struct ul_pty *pty, int fd)
{
	struct signalfd_siginfo info;
	ssize_t bytes;
	int rc = 0;

	DBG(SIG, ul_debugobj(pty, " handle signal on fd=%d", fd));

	bytes = read(fd, &info, sizeof(info));
	if (bytes != sizeof(info)) {
		if (bytes < 0 && (errno == EAGAIN || errno == EINTR))
			return 0;
		return -errno;
	}

	switch (info.ssi_signo) {
	case SIGCHLD:
		DBG(SIG, ul_debugobj(pty, " get signal SIGCHLD"));

		if (info.ssi_code == CLD_EXITED
		    || info.ssi_code == CLD_KILLED
		    || info.ssi_code == CLD_DUMPED) {

			if (pty->callbacks.child_wait)
				pty->callbacks.child_wait(pty->callback_data,
							  pty->child);
			else
				ul_pty_wait_for_child(pty);

		} else if (info.ssi_status == SIGSTOP && pty->child > 0) {
			pty->callbacks.child_sigstop(pty->callback_data,
						     pty->child);
		}

		if (pty->child <= 0) {
			DBG(SIG, ul_debugobj(pty, " no child, setting leaving timeout"));
			pty->poll_timeout = 10;
			timerclear(&pty->next_callback_time);
		}
		return 0;
	case SIGWINCH:
		DBG(SIG, ul_debugobj(pty, " get signal SIGWINCH"));
		if (pty->isterm) {
			ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&pty->win);
			ioctl(pty->slave, TIOCSWINSZ, (char *)&pty->win);

			if (pty->callbacks.log_signal)
				rc = pty->callbacks.log_signal(pty->callback_data,
							&info, (void *) &pty->win);
		}
		break;
	case SIGHUP:
		/* fallthrough */
	case SIGTERM:
		/* fallthrough */
	case SIGINT:
		/* fallthrough */
	case SIGQUIT:
		DBG(SIG, ul_debugobj(pty, " get signal SIG{TERM,INT,QUIT}"));
		pty->delivered_signal = info.ssi_signo;
                /* Child termination is going to generate SIGCHLD (see above) */
		if (pty->child > 0)
	                kill(pty->child, SIGTERM);

		if (pty->callbacks.log_signal)
			rc = pty->callbacks.log_signal(pty->callback_data,
					&info, (void *) &pty->win);
		break;
	case SIGUSR1:
		DBG(SIG, ul_debugobj(pty, " get signal SIGUSR1"));
		if (pty->callbacks.flush_logs)
			rc = pty->callbacks.flush_logs(pty->callback_data);
		break;
	default:
		abort();
	}

	return rc;
}

/* loop in parent */
int ul_pty_proxy_master(struct ul_pty *pty)
{
	int rc = 0, ret, eof = 0;
	enum {
		POLLFD_SIGNAL = 0,
		POLLFD_MASTER,
		POLLFD_STDIN

	};
	struct pollfd pfd[] = {
		[POLLFD_SIGNAL] = { .fd = -1,		.events = POLLIN | POLLERR | POLLHUP },
		[POLLFD_MASTER] = { .fd = pty->master,  .events = POLLIN | POLLERR | POLLHUP },
		[POLLFD_STDIN]	= { .fd = STDIN_FILENO, .events = POLLIN | POLLERR | POLLHUP }
	};

	/* We use signalfd, and standard signals by handlers are completely blocked */
	assert(pty->sigfd >= 0);

	pfd[POLLFD_SIGNAL].fd = pty->sigfd;
	pty->poll_timeout = -1;

	while (!pty->delivered_signal) {
		size_t i;
		int errsv, timeout;

		DBG(IO, ul_debugobj(pty, "--poll() loop--"));

		/* note, callback usually updates @next_callback_time */
		if (timerisset(&pty->next_callback_time)) {
			struct timeval now = { 0 };;

			DBG(IO, ul_debugobj(pty, " callback requested"));
			gettime_monotonic(&now);
			if (timercmp(&now, &pty->next_callback_time, >)) {
				rc = mainloop_callback(pty);
				if (rc)
					break;
			}
		}

		/* set timeout */
		if (timerisset(&pty->next_callback_time)) {
			struct timeval now = { 0 }, rest = { 0 };

			gettime_monotonic(&now);
			timersub(&pty->next_callback_time, &now, &rest);
			timeout = (rest.tv_sec * 1000) +  (rest.tv_usec / 1000);
		} else
			timeout = pty->poll_timeout;

		/* use POLLOUT (aka "writing is now possible") if data queued */
		if (pty->child_buffer_head)
			pfd[POLLFD_MASTER].events |= POLLOUT;
		else
			pfd[POLLFD_MASTER].events &= ~POLLOUT;

		/* wait for input, signal or timeout */
		DBG(IO, ul_debugobj(pty, "calling poll() [timeout=%dms]", timeout));
		ret = poll(pfd, ARRAY_SIZE(pfd), timeout);

		errsv = errno;
		DBG(IO, ul_debugobj(pty, "poll() rc=%d", ret));

		/* error */
		if (ret < 0) {
			if (errsv == EAGAIN)
				continue;
			rc = -errno;
			break;
		}

		/* timeout */
		if (ret == 0) {
			if (timerisset(&pty->next_callback_time)) {
				rc = mainloop_callback(pty);
				if (rc == 0)
					continue;
			} else {
				rc = 0;
			}

			DBG(IO, ul_debugobj(pty, "leaving poll() loop [timeout=%d, rc=%d]", timeout, rc));
			break;
		}
		/* event */
		for (i = 0; i < ARRAY_SIZE(pfd); i++) {
			if (pfd[i].revents == 0)
				continue;

			DBG(IO, ul_debugobj(pty, " active pfd[%s].fd=%d %s %s %s %s %s",
						i == POLLFD_STDIN  ? "stdin" :
						i == POLLFD_MASTER ? "master" :
						i == POLLFD_SIGNAL ? "signal" : "???",
						pfd[i].fd,
						pfd[i].revents & POLLIN  ? "POLLIN" : "",
						pfd[i].revents & POLLOUT ? "POLLOUT" : "",
						pfd[i].revents & POLLHUP ? "POLLHUP" : "",
						pfd[i].revents & POLLERR ? "POLLERR" : "",
						pfd[i].revents & POLLNVAL ? "POLLNVAL" : ""));

			if (i == POLLFD_SIGNAL)
				rc = handle_signal(pty, pfd[i].fd);
			else {
				if (pfd[i].revents & POLLIN)
					rc = handle_io(pty, pfd[i].fd, &eof); /* data */
				if (pfd[i].revents & POLLOUT) /* i == POLLFD_MASTER */
					rc = flush_child_buffers(pty, NULL);
			}

			if (rc) {
				int anything = 1;

				ul_pty_write_eof_to_child(pty);
				for (anything = 1; anything;)
					flush_child_buffers(pty, &anything);
				break;
			}

			if (i == POLLFD_SIGNAL)
				continue;

			/* EOF maybe detected in two ways; they are as follows:
			 *	A) poll() return POLLHUP event after close()
			 *	B) read() returns 0 (no data)
			 *
			 * POLLNVAL means that fd is closed.
			 */
			if ((pfd[i].revents & POLLHUP) || (pfd[i].revents & POLLNVAL) || eof) {
				DBG(IO, ul_debugobj(pty, " ignore FD"));
				if (i == POLLFD_STDIN) {
					pfd[i].fd = -1;
					ul_pty_write_eof_to_child(pty);
				} else /* i == POLLFD_MASTER */
					pfd[i].revents &= ~POLLIN;
			}
		}
		if (rc)
			break;
	}

	if (rc && pty->child && pty->child != (pid_t) -1 && !pty->delivered_signal) {
		kill(pty->child, SIGTERM);
		sleep(2);
		kill(pty->child, SIGKILL);
	}

	pty_signals_cleanup(pty);

	DBG(IO, ul_debug("poll() done [signal=%d, rc=%d]", pty->delivered_signal, rc));
	return rc;
}

#ifdef TEST_PROGRAM_PTY
/*
 * $ make test_pty
 * $ ./test_pty
 *
 * ... and see for example tty(1) or "ps afu"
 */
static void child_sigstop(void *data __attribute__((__unused__)), pid_t child)
{
	kill(getpid(), SIGSTOP);
	kill(child, SIGCONT);
}

int main(int argc, char *argv[])
{
	struct ul_pty_callbacks *cb;
	const char *shell, *command = NULL, *shname = NULL;
	int caught_signal = 0;
	pid_t child;
	struct ul_pty *pty;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;
	if (argc == 2)
		command = argv[1];

	ul_pty_init_debug(0);

	pty = ul_new_pty(isatty(STDIN_FILENO));
	if (!pty)
		err(EXIT_FAILURE, "failed to allocate PTY handler");

	cb = ul_pty_get_callbacks(pty);
	cb->child_sigstop = child_sigstop;

	if (ul_pty_setup(pty))
		err(EXIT_FAILURE, "failed to create pseudo-terminal");
	if (ul_pty_signals_setup(pty))
		err(EXIT_FAILURE, "failed to initialize signals handler");

	fflush(stdout);			/* ??? */

	switch ((int) (child = fork())) {
	case -1: /* error */
		ul_pty_cleanup(pty);
		err(EXIT_FAILURE, "cannot create child process");
		break;

	case 0: /* child */
		ul_pty_init_slave(pty);

		signal(SIGTERM, SIG_DFL); /* because /etc/csh.login */

		shname = strrchr(shell, '/');
		shname = shname ? shname + 1 : shell;

		if (command)
			execl(shell, shname, "-c", command, (char *)NULL);
		else
			execl(shell, shname, "-i", (char *)NULL);
		err(EXIT_FAILURE, "failed to execute %s", shell);
		break;

	default:
		break;
	}

	/* parent */
	ul_pty_set_child(pty, child);

	/* this is the main loop */
	ul_pty_proxy_master(pty);

	/* all done; cleanup and kill */
	caught_signal = ul_pty_get_delivered_signal(pty);

	if (!caught_signal && ul_pty_get_child(pty) != (pid_t)-1)
		ul_pty_wait_for_child(pty);	/* final wait */

	if (caught_signal && ul_pty_get_child(pty) != (pid_t)-1) {
		fprintf(stderr, "\nSession terminated, killing shell...");
		kill(child, SIGTERM);
		sleep(2);
		kill(child, SIGKILL);
		fprintf(stderr, " ...killed.\n");
	}

	ul_pty_cleanup(pty);
	ul_free_pty(pty);
	return EXIT_SUCCESS;
}

#endif /* TEST_PROGRAM */

