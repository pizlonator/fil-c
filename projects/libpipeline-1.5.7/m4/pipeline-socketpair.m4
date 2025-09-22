# pipeline-socketpair.m4 serial 3
dnl
dnl Check if the socketpair(2) system call can be used
dnl and should be used as a fast replacement for pipe(2)
dnl
dnl Author: Werner Fink <werner@suse.de>, 2009

AC_DEFUN([PIPELINE_SOCKETPAIR_PIPE],
[ AC_MSG_CHECKING([if socketpair(2) can be used as fast replacement for pipe(2)])
  AC_CACHE_VAL(pipeline_cv_socketpair_pipe, [
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef SHUT_RD
#	define SHUT_RD	0
#endif
#ifndef SHUT_WR
#	define SHUT_WR	1
#endif

static void sigpipe(int sig)
{
	_exit(0);
}

int main(void)
{
	const char test[] = "May use socketpair(2) instead of pipe(2)\n";
	char buf[256];
	int sfd[2], s;
	pid_t pid;
	if (socketpair(AF_UNIX,SOCK_STREAM,0,sfd) < 0)
		return 1;
	if (shutdown(sfd[1],SHUT_RD) < 0 || shutdown(sfd[0],SHUT_WR) < 0)
		return 1;
	if ((pid = fork()) < 0)
		return 1;
	if (pid) {
		close(sfd[1]);
		waitpid(-1,&s,0);
		if (read(sfd[0],buf,sizeof(buf)) < 0)
			return 1;
	} else {
		close(sfd[0]);
		write(sfd[1],test,sizeof(test) - 1);
		return 0;
	}
	close(sfd[0]);
	signal(SIGPIPE, sigpipe);
	if (socketpair(AF_UNIX,SOCK_STREAM,0,sfd) < 0)
		return 1;
	if (shutdown(sfd[1],SHUT_RD) < 0 || shutdown(sfd[0],SHUT_WR) < 0)
		return 1;
	close(sfd[0]);
	write(sfd[1],test,sizeof(test) - 1);
	return 1;
}]])], [pipeline_cv_socketpair_pipe=yes], [pipeline_cv_socketpair_pipe=no], [pipeline_cv_socketpair_pipe=no])
  ])
  AC_MSG_RESULT([$pipeline_cv_socketpair_pipe])
  if test "$pipeline_cv_socketpair_pipe" = yes; then
    AC_DEFINE(USE_SOCKETPAIR_PIPE, 1, [Define if socketpair(2) can be used as a fast replacement for pipe(2).])
  fi
])

dnl
dnl Check if shutdown(2) does not set mode for the socket descriptor
dnl compare with ls -lL /proc/<pid>/fd/
dnl
AC_DEFUN([PIPELINE_SOCKETPAIR_MODE],
[ AC_MSG_CHECKING([if shutdown(2) does not set mode for the socket descriptor])
  AC_CACHE_VAL(pipeline_cv_socketpair_mode, [
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SHUT_RD
#	define SHUT_RD	0
#endif
#ifndef SHUT_WR
#	define SHUT_WR	1
#endif

int main(void)
{
	int sfd[2];
	struct stat st[2];
	if (socketpair(AF_UNIX,SOCK_STREAM,0,sfd) < 0)
		return 1;
	if (shutdown(sfd[1],SHUT_RD) < 0 || shutdown(sfd[0],SHUT_WR) < 0)
		return 1;
	if (fstat(sfd[0], &(st[0])) < 0 || fstat(sfd[1], &(st[1])) < 0)
		return 1;
	if ((st[0].st_mode & (S_IRUSR|S_IWUSR)) == S_IRUSR && (st[1].st_mode & (S_IRUSR|S_IWUSR)) == S_IWUSR)
		return 1;
	if (fchmod(sfd[0], S_IRUSR) < 0 || fchmod(sfd[1], S_IWUSR) < 0)
		return 1;
	if (fstat(sfd[0], &(st[0])) < 0 || fstat(sfd[1], &(st[1])) < 0)
		return 1;
	if ((st[0].st_mode & (S_IRUSR|S_IWUSR)) != S_IRUSR && (st[1].st_mode & (S_IRUSR|S_IWUSR)) != S_IWUSR)
		return 1;
	return 0;
}]])], [pipeline_cv_socketpair_mode=yes], [pipeline_cv_socketpair_mode=no], [pipeline_cv_socketpair_mode=no])
  ])
  AC_MSG_RESULT([$pipeline_cv_socketpair_mode])
  if test "$pipeline_cv_socketpair_mode" = yes; then
    AC_DEFINE(CORRECT_SOCKETPAIR_MODE, 1, [Define if shutdown(2) does not set modes for socket descriptor.])
  fi
])
