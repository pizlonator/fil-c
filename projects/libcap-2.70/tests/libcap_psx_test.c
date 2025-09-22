#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/psx_syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void *thread_fork_exit(void *data) {
    usleep(1234);
    pid_t pid = fork();
    cap_t start = cap_get_proc();
    if (start == NULL) {
	perror("FAILED: unable to start");
	exit(1);
    }
    if (pid == 0) {
	if (cap_set_proc(start)) {
	    perror("setting empty caps failed");
	    exit(1);
	}
	exit(0);
    }
    int res;
    if (waitpid(pid, &res, 0) != pid || res != 0) {
	printf("FAILED: pid=%d wait returned %d and/or error: %d\n",
	       pid, res, errno);
	exit(1);
    }
    cap_set_proc(start);
    cap_free(start);
    return NULL;
}

int main(int argc, char **argv) {
    int i;
    printf("hello libcap and libpsx ");
    fflush(stdout);
    cap_t start = cap_get_proc();
    if (start == NULL) {
	perror("FAILED: to actually start");
	exit(1);
    }
    pthread_t ignored[10];
    for (i = 0; i < 10; i++) {
	pthread_create(&ignored[i], NULL, thread_fork_exit, NULL);
    }
    for (i = 0; i < 10; i++) {
	printf(".");     /* because of fork, this may print double */
	fflush(stdout);  /* try to limit the above effect */
	if (cap_set_proc(start)) {
	    perror("failed to set proc");
	    exit(1);
	}
	usleep(1000);
    }
    printf(" PASSED\n");
    exit(0);
}
