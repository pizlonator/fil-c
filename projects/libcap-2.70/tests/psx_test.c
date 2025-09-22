#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/psx_syscall.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef union tp {
    long long unsigned raw;
    pthread_t pt;
} thread_ptr;

static void say_hello_expecting(const char *title, int n, int kept) {
    int keeper = prctl(PR_GET_KEEPCAPS);
    thread_ptr tp;
    tp.pt = pthread_self();

    printf("hello [%d], %s<%d> %llx (keepcaps=%d vs. want=%d)\n",
	   getpid(), title, n, tp.raw, keeper, kept);
    if (keeper != kept) {
	printf("--> FAILURE %s thread=%llx has wrong keepcaps: got=%d want=%d\n",
	       title, tp.raw, keeper, kept);
	exit(1);
    }
}

pthread_mutex_t mu;
pthread_cond_t cond;

int global_kept = 0;
int step = 0;
int replies = 0;
int launched = 0;
int started = 0;

static void *say_hello(void *args) {
    int count = 0;

    pthread_mutex_lock(&mu);
    started++;
    int this_step = step+1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mu);

    pthread_mutex_lock(&mu);
    do {
	while (this_step > step) {
	    pthread_cond_wait(&cond, &mu);
	}
	say_hello_expecting("thread", count, global_kept);

	replies++;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mu);

	this_step++;
	pthread_mutex_lock(&mu);
    } while (++count != 3);
    pthread_mutex_unlock(&mu);

    return NULL;
}

int main(int argc, char **argv) {
    pthread_t tid[3];
    int i;
    pid_t child = 0;
    char * const stop_argv[3] = { argv[0], strdup("stop"), NULL };

    if (argc != 1) {
	printf("child %d starting\n", getpid());
	usleep(2000);
	printf("child %d exiting\n", getpid());
	exit(0);
    }

    for (i = 0; i<10; i++) {
	printf("iteration [%d]: %d\n", getpid(), i);

	pthread_mutex_lock(&mu);
	global_kept = !global_kept;
	replies = 0;
	step = i;
	pthread_mutex_unlock(&mu);

	psx_syscall(SYS_prctl, PR_SET_KEEPCAPS, global_kept);

	pthread_mutex_lock(&mu);
	step++;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mu);

	say_hello_expecting("main", i, global_kept);

	pthread_mutex_lock(&mu);
	while (replies < launched) {
	    pthread_cond_wait(&cond, &mu);
	}
	pthread_mutex_unlock(&mu);

	if (i < 3) {
	    if (!child) {
		child = fork();
		if (!child) {
		    usleep(2000);
		    execve(argv[0], stop_argv, NULL);
		    perror("failed to exec");
		    exit(1);
		} else {
		    printf("pid=%d forked -> %d\n", getpid(), child);
		}
	    }
	    launched++;
	    pthread_create(&tid[i], NULL, say_hello, NULL);
	    /* Confirm that the thread is started. */
	    pthread_mutex_lock(&mu);
	    while (started < launched) {
		printf("[%d] started=%d vs %d\n", getpid(), started, launched);
		pthread_cond_wait(&cond, &mu);
	    }
	    printf("[%d] started=%d vs %d\n", getpid(), started, launched);
	    pthread_cond_broadcast(&cond);
	    pthread_mutex_unlock(&mu);
	} else if (i < 6) {
	    /* Confirm one thread has finished. */
	    pthread_join(tid[i-3], NULL);
	    launched--;
	}
    }

    if (child) {
	int status;
	waitpid(child, &status, 0);
	if (status) {
	    printf("child %d FAILED: %d\n", child, status);
	    exit(1);
	}
    }
    printf("%s PASSED\n", argv[0]);
    exit(0);
}
