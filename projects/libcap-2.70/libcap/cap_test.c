#define _GNU_SOURCE
#include <stdio.h>

#include "libcap.h"

static cap_value_t top;

static int cf(cap_value_t x)
{
    return top - x - 1;
}

static int test_cap_bits(void)
{
    static cap_value_t vs[] = {
	5, 6, 11, 12, 15, 16, 17, 38, 41, 63, 64, __CAP_MAXBITS+3, 0, -1
    };
    int failed = 0;
    cap_value_t i;
    for (i = 0; vs[i] >= 0; i++) {
	cap_value_t ans;

	top = vs[i];
	_binary_search(ans, cf, 0, __CAP_MAXBITS, -1);
	if (ans != top) {
	    if (top == 0 && ans == -1) {
		continue;
	    }
	    if (top > __CAP_MAXBITS && ans == -1) {
		continue;
	    }
	    printf("test_cap_bits miscompared [%d] top=%d - got=%d\n",
		   i, top, ans);
	    failed = -1;
	}
    }
    return failed;
}

static int test_cap_flags(void)
{
    cap_t c, d;
    cap_flag_t f = CAP_INHERITABLE, t;
    cap_value_t v;
    int retval = 0;

    c = cap_init();
    if (c == NULL) {
	printf("test_flags failed to allocate a set\n");
	return -1;
    }
    if (cap_compare(c, NULL) != -1) {
	printf("compare to NULL should give invalid\n");
	return -1;
    }
    if (cap_compare(NULL, c) != -1) {
	printf("compare with NULL should give invalid\n");
	return -1;
    }

    for (v = 0; v < __CAP_MAXBITS; v += 3) {
	if (cap_set_flag(c, CAP_INHERITABLE, 1, &v, CAP_SET)) {
	    printf("unable to set inheritable bit %d\n", v);
	    retval = -1;
	    goto drop_c;
	}
    }

    d = cap_dup(c);
    for (t = CAP_EFFECTIVE; t <= CAP_INHERITABLE; t++) {
	if (cap_fill(c, t, f)) {
	    printf("cap_fill failed %d -> %d\n", f, t);
	    retval = -1;
	    goto drop_d;
	}
	if (cap_clear_flag(c, f)) {
	    printf("cap_fill unable to clear flag %d\n", f);
	    retval = -1;
	    goto drop_d;
	}
	f = t;
    }
    if (cap_compare(c, d)) {
	printf("permuted cap_fill()ing failed to perform net no-op\n");
	retval = -1;
    }
    if (cap_fill_flag(NULL, CAP_EFFECTIVE, c, CAP_INHERITABLE) == 0) {
	printf("filling NULL flag should fail\n");
	retval = -1;
    }
    if (cap_fill_flag(d, CAP_PERMITTED, c, CAP_INHERITABLE) != 0) {
	perror("filling PERMITEED flag should work");
	retval = -1;
    }
    if (cap_fill_flag(c, CAP_PERMITTED, d, CAP_PERMITTED) != 0) {
	perror("filling PERMITTED flag from another cap_t should work");
	retval = -1;
    }
    if (cap_compare(c, d)) {
	printf("permuted cap_fill()ing failed to perform net no-op\n");
	retval = -1;
    }

drop_d:
    if (cap_free(d) != 0) {
	perror("failed to free d");
	retval = -1;
    }
drop_c:
    if (cap_free(c) != 0) {
	perror("failed to free c");
	retval = -1;
    }
    return retval;
}

static int test_short_bits(void)
{
    int result = 0;
    char *tmp;
    int n = asprintf(&tmp, "%d", __CAP_MAXBITS);
    if (n <= 0) {
	return -1;
    }
    if (strlen(tmp) > __CAP_NAME_SIZE) {
	printf("cap_to_text buffer size reservation needs fixing (%ld > %d)\n",
	       (long int)strlen(tmp), __CAP_NAME_SIZE);
	result = -1;
    }
    free(tmp);
    return result;
}

static int noop(void *data)
{
    return -1;
}

static int test_alloc(void)
{
    int retval = 0;
    cap_t c;
    cap_iab_t iab;
    cap_launch_t launcher;
    char *old_root;

    printf("test_alloc\n");
    fflush(stdout);

    c = cap_init();
    if (c == NULL) {
	perror("failed to allocate a cap_t");
	fflush(stderr);
	return -1;
    }

    iab = cap_iab_init();
    if (iab == NULL) {
	perror("failed to allocate a cap_iab_t");
	fflush(stderr);
	retval = -1;
	goto drop_c;
    }

    launcher = cap_func_launcher(noop);
    if (launcher == NULL) {
	perror("failde to allocate a launcher");
	fflush(stderr);
	retval = -1;
	goto drop_iab;
    }

    cap_launcher_set_chroot(launcher, "/tmp");
    if (cap_launcher_set_iab(launcher, iab) != NULL) {
	printf("unable to replace iab in launcher\n");
	fflush(stdout);
	retval = -1;
	goto drop_iab;
    }

    iab = cap_launcher_set_iab(launcher, cap_iab_init());
    if (iab == NULL) {
	printf("unable to recover iab in launcher\n");
	fflush(stdout);
	retval = -1;
	goto drop_launcher;
    }

    old_root = cap_proc_root("blah");
    if (old_root != NULL) {
	printf("bad initial proc_root [%s]\n", old_root);
	fflush(stdout);
	retval = -1;
    }
    if (cap_free(old_root)) {
	perror("unable to free old proc root");
	fflush(stderr);
	retval = -1;
    }
    if (retval) {
	goto drop_launcher;
    }
    old_root = cap_proc_root("/proc");
    if (strcmp(old_root, "blah") != 0) {
	printf("bad proc_root value [%s]\n", old_root);
	fflush(stdout);
	retval = -1;
    }
    if (cap_free(old_root)) {
	perror("unable to free replacement proc root");
	fflush(stderr);
	retval = -1;
    }
    if (retval) {
	goto drop_launcher;
    }

drop_launcher:
    printf("test_alloc: drop_launcher\n");
    fflush(stdout);
    if (cap_free(launcher)) {
	perror("failed to free launcher");
	fflush(stderr);
	retval = -1;
    }

drop_iab:
    printf("test_alloc: drop_iab\n");
    fflush(stdout);
    if (!cap_free(2+(__u32 *) iab)) {
	printf("unable to recognize bad cap_iab_t pointer\n");
	fflush(stdout);
	retval = -1;
    }
    if (cap_free(iab)) {
	perror("failed to free iab");
	fflush(stderr);
	retval = -1;
    }

drop_c:
    printf("test_alloc: drop_cap\n");
    fflush(stdout);
    if (!cap_free(1+(__u32 *) c)) {
	printf("unable to recognize bad cap_t pointer\n");
	fflush(stdout);
	retval = -1;
    }
    if (cap_free(c)) {
	perror("failed to free c");
	fflush(stderr);
	retval = -1;
    }
    return retval;
}

static int test_prctl(void)
{
    int ret, retval=0;
    errno = 0;
    ret = cap_get_bound((cap_value_t) -1);
    if (ret != -1) {
	printf("cap_get_bound(-1) did not return error: %d\n", ret);
	retval = -1;
    } else if (errno != EINVAL) {
	perror("cap_get_bound(-1) errno != EINVAL");
	retval = -1;
    }
    return retval;
}

int main(int argc, char **argv) {
    int result = 0;

    printf("test_cap_bits: being called\n");
    fflush(stdout);
    result = test_cap_bits() | result;
    printf("test_cap_flags: being called\n");
    fflush(stdout);
    result = test_cap_flags() | result;
    printf("test_short_bits: being called\n");
    fflush(stdout);
    result = test_short_bits() | result;
    printf("test_alloc: being called\n");
    fflush(stdout);
    result = test_alloc() | result;
    printf("test_prctl: being called\n");
    fflush(stdout);
    result = test_prctl() | result;
    printf("tested\n");
    fflush(stdout);

    if (result) {
	printf("cap_test FAILED\n");
	exit(1);
    }
    printf("cap_test PASS\n");
    exit(0);
}
