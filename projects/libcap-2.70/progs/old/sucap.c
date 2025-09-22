/*
 * $Id: sucap.c,v 1.1.1.1 1999/04/17 22:16:31 morgan Exp $
 *
 * This was written by Finn Arne Gangstad <finnag@guardian.no>
 *
 * This is a program that is intended to exec a subsequent program.
 * The purpose of this 'sucap' wrapper is to change uid but keep all
 * privileges. All environment variables are inherited.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#undef _POSIX_SOURCE
#include <sys/capability.h>
#include <pwd.h>
#define __USE_BSD
#include <grp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static void usage(void)
{
    fprintf(stderr,
"usage: sucap <user> <group> <command-path> [command-args...]\n\n"
"  This program is a wrapper that change UID but not privileges of a\n"
"  program to be executed.\n"
"  Note, this wrapper is intended to assist in overcoming a lack of support\n"
"  for filesystem capability attributes and should be used to launch other\n"
"  files. This program should _NOT_ be made setuid-0.\n\n"
"[Copyright (c) 1998 Finn Arne Gangstad <finnag@guardian.no>]\n");

    exit(1);
}


static void
wait_on_fd(int fd)
{
    /* Wait until some data is available on a file descriptor, or until
     * end of file or an error is detected */
    char buf[1];
    while (read(fd, buf, sizeof(buf)) == -1 && errno == EINTR) {
	/* empty loop */
    }
}


int main(int argc, char **argv)
{
    cap_t old_caps;
    uid_t uid;
    pid_t pid, parent_pid;
    gid_t gid;
    int pipe_fds[2];

    /* this program should not be made setuid-0 */
    if (getuid() && !geteuid()) {
        usage();
    }

    /* check that we have at least 3 arguments */
    if (argc < 4) {
        usage();
    }

    /* Convert username to uid */
    {
	struct passwd *pw = getpwnam(argv[1]);
	if (!pw) {
	    fprintf(stderr, "sucap: No such user: %s\n", argv[1]);
	    exit(1);
	}
	uid = pw->pw_uid;
    }

    /* Convert groupname to gid */
    {
	struct group *gr = getgrnam(argv[2]);
	if (!gr) {
	    fprintf(stderr, "sucap: No such group: %s\n", argv[2]);
	    exit(1);
	}
	gid = gr->gr_gid;
    }
    
    /* set process group to current pid */
    if (setpgid(0, getpid())) {
	perror("sucap: Failed to set process group");
	exit(1);
    }
    
    if (pipe(pipe_fds)) {
	perror("sucap: pipe() failed");
	exit(1);
    }
    
    parent_pid = getpid();

    old_caps = cap_init();
    if (capgetp(0, old_caps)) {
	perror("sucap: capgetp");
	exit(1);
    }
    
    {
	ssize_t x;
	printf("Caps: %s\n", cap_to_text(old_caps, &x));
    }


    /* fork off a child to do the hard work */
    fflush(NULL);
    pid = fork();
    if (pid == -1) {
	perror("sucap: fork failed");
	exit(1);
    }

    /* 1. mother process sets gid and uid
     * 2. child process sets capabilities of mother process
     * 3. mother process execs whatever is to be executed
     */

    if (pid) {
	/* Mother process. */
	close(pipe_fds[0]);

	/* Get rid of any supplemental groups */
	if (!getuid() && setgroups(0, 0)) {
	    perror("sucap: setgroups failed");
	    exit(1);
	}

	/* Set gid and uid (this probably clears capabilities) */
	setregid(gid, gid);
	setreuid(uid, uid);

	{
	    ssize_t x;
	    cap_t cap = cap_init();
	    capgetp(0, cap);
	    printf("Caps: %s\n", cap_to_text(cap, &x));
	}
	
	printf("[debug] uid:%d, real uid:%d\n", geteuid(), getuid());

	/* Signal child that we want our privileges updated */
	close(pipe_fds[1]); /* Child hangs in blocking read */

	/* Wait for child process to set our privileges */
	{
	    int status = 0;
	    if (wait(&status) == -1) {
		perror("sucap: wait failed");
	    }
	    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		fprintf(stderr, "sucap: child did not exit cleanly.\n");
		exit(1);
	    }
	}

	{
	    ssize_t x;
	    cap_t cap = cap_init();
	    capgetp(0, cap);
	    printf("Caps: %s\n", cap_to_text(cap, &x));
	}

/*	printf("[debug] uid:%d, real uid:%d\n", geteuid(), getuid()); */
	/* exec the program indicated by args 2 ... */
	execvp(argv[3], argv+3);
	
	/* if we fall through to here, our exec failed -- announce the fact */
	fprintf(stderr, "Unable to execute command: %s\n", strerror(errno));
	
	usage();
    } else {
	/* Child process */
	close(pipe_fds[1]);

	/* Wait for mother process to setuid */
	wait_on_fd(pipe_fds[0]);

	/* Set privileges on mother process */
	if (capsetp(parent_pid, old_caps)) {
	    perror("sucaps: capsetp");
	    _exit(1);
	}

	/* exit to signal mother process that we are ready */
	_exit(0);
    }

    return 0;
}
