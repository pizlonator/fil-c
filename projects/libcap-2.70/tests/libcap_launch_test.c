#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * tests for cap_launch.
 */

#define MORE_THAN_ENOUGH 20
#define NO_MORE 1

struct test_case_s {
    int pass_on;
    const char *chroot;
    uid_t uid;
    gid_t gid;
    int ngroups;
    const gid_t groups[MORE_THAN_ENOUGH];
    const char *args[MORE_THAN_ENOUGH];
    const char **envp;
    const char *iab;
    cap_mode_t mode;
    int launch_abort;
    int result;
    int (*callback_fn)(void *detail);
};

#ifdef WITH_PTHREADS
#include <pthread.h>
#else /* WITH_PTHREADS */
#endif /* WITH_PTHREADS */

/*
 * clean_out drops all process capabilities.
 */
static int clean_out(void *data) {
    cap_t empty;
    empty = cap_init();
    if (cap_set_proc(empty) != 0) {
	_exit(1);
    }
    cap_free(empty);
    return 0;
}

int main(int argc, char **argv) {
    static struct test_case_s vs[] = {
	{
	    .args = { "../progs/tcapsh-static", "--", "-c", "echo hello" },
	    .result = 0
	},
	{
	    .args = { "../progs/tcapsh-static", "--", "-c", "echo hello" },
	    .callback_fn = &clean_out,
	    .result = 0
	},
	{
	    .callback_fn = &clean_out,
	    .result = 0
	},
	{
	    .args = { "../progs/tcapsh-static", "--is-uid=123" },
	    .result = 256
	},
	{
	    .args = { "/", "won't", "work" },
	    .launch_abort = 1,
	},
	{
	    .args = { "../progs/tcapsh-static", "--is-uid=123" },
	    .uid = 123,
	    .result = 0,
	},
	{
	    .args = { "../progs/tcapsh-static", "--is-uid=123" },
	    .callback_fn = &clean_out,
	    .uid = 123,
	    .launch_abort = 1,
	},
	{
	    .args = { "../progs/tcapsh-static", "--is-gid=123" },
	    .result = 0,
	    .gid = 123,
	    .ngroups = 1,
	    .groups = { 456 },
	    .iab = "",
	},
	{
	    .args = { "../progs/tcapsh-static", "--dropped=cap_chown",
		      "--has-i=cap_chown" },
	    .result = 0,
	    .iab = "!%cap_chown"
	},
	{
	    .args = { "../progs/tcapsh-static", "--dropped=cap_chown",
		      "--has-i=cap_chown", "--is-uid=234",
		      "--has-a=cap_chown", "--has-p=cap_chown" },
	    .uid = 234,
	    .result = 0,
	    .iab = "!^cap_chown"
	},
	{
	    .args = { "../progs/tcapsh-static", "--inmode=NOPRIV",
		      "--has-no-new-privs" },
	    .result = 0,
	    .mode = CAP_MODE_NOPRIV
	},
	{
	    .args = { "/noop" },
	    .result = 0,
	    .chroot = ".",
	},
	{
	    .pass_on = NO_MORE
	},
    };

    if (errno != 0) {
	perror("unexpected initial value for errno");
	exit(1);
    }

    cap_t orig = cap_get_proc();
    if (orig == NULL) {
	perror("failed to get process capabilities");
	exit(1);
    }

    int success = 1, i;
    for (i=0; vs[i].pass_on != NO_MORE; i++) {
	cap_launch_t attr = NULL;
	const struct test_case_s *v = &vs[i];
	if (cap_launch(attr, NULL) != -1) {
	    perror("NULL launch didn't fail");
	    exit(1);
	}
	printf("[%d] test should %s\n", i,
	       v->result || v->launch_abort ? "generate error" : "work");
	if (v->args[0] != NULL) {
	    attr = cap_new_launcher(v->args[0], v->args, v->envp);
	    if (attr == NULL) {
		perror("failed to obtain launcher");
		exit(1);
	    }
	    if (v->callback_fn != NULL) {
		cap_launcher_callback(attr, v->callback_fn);
	    }
	} else {
	    attr = cap_func_launcher(v->callback_fn);
	}
	if (v->chroot) {
	    cap_launcher_set_chroot(attr, v->chroot);
	}
	if (v->uid) {
	    cap_launcher_setuid(attr, v->uid);
	}
	if (v->gid) {
	    cap_launcher_setgroups(attr, v->gid, v->ngroups, v->groups);
	}
	if (v->iab) {
	    cap_iab_t iab = cap_iab_from_text(v->iab);
	    if (iab == NULL) {
		fprintf(stderr, "[%d] failed to decode iab [%s]", i, v->iab);
		perror(":");
		success = 0;
		continue;
	    }
	    cap_iab_t old = cap_launcher_set_iab(attr, iab);
	    if (cap_free(old)) {
		fprintf(stderr, "[%d] failed to decode iab [%s]", i, v->iab);
		perror(":");
		success = 0;
		continue;
	    }
	}
	if (v->mode) {
	    cap_launcher_set_mode(attr, v->mode);
	}

	pid_t child = cap_launch(attr, NULL);

	if (child <= 0) {
	    fprintf(stderr, "[%d] failed to launch: ", i);
	    perror("");
	    if (!v->launch_abort) {
		success = 0;
	    }
	    continue;
	}
	if (cap_free(attr)) {
	    fprintf(stderr, "[%d] failed to free launcher: ", i);
	    perror("");
	    success = 0;
	}
	int result;
	int ret = waitpid(child, &result, 0);
	if (ret != child) {
	    fprintf(stderr, "[%d] failed to wait: ", i);
	    perror("");
	    success = 0;
	    continue;
	}
	if (result != v->result) {
	    fprintf(stderr, "[%d] bad result: got=%d want=%d: ", i, result,
		    v->result);
	    perror("");
	    success = 0;
	    continue;
	}
    }

    cap_t final = cap_get_proc();
    if (final == NULL) {
	perror("unable to get final capabilities");
	exit(1);
    }
    if (cap_compare(orig, final)) {
	char *was = cap_to_text(orig, NULL);
	char *is = cap_to_text(final, NULL);
	printf("cap_launch_test: orig:'%s' != final:'%s'\n", was, is);
	cap_free(is);
	cap_free(was);
	success = 0;
    }
    cap_free(final);
    cap_free(orig);

    if (!success) {
	printf("cap_launch_test: FAILED\n");
	exit(1);
    }
    printf("cap_launch_test: PASSED\n");
    exit(0);
}
