/*
 * Copyright (c) 2019 Andrew G. Morgan <morgan@kernel.org>
 *
 * This test inlines the pam_cap module and runs test vectors against
 * it.
 */

#define _DEFAULT_SOURCE

#include <unistd.h>
#include <sys/types.h>

#include "./pam_cap.c"

const char *test_groups[] = {
    "root", "one", "two", "three", "four", "five", "six", "seven"
};
#define n_groups sizeof(test_groups)/sizeof(*test_groups)

const char *test_users[] = {
    "root", "alpha", "beta", "gamma", "delta"
};
#define n_users sizeof(test_users)/sizeof(*test_users)

/* Note about memberships:
 *
 *  user gid   suppl groups
 *  root  root
 *  alpha one   two
 *  beta  two   three four
 *  gamma three four five six
 *  delta four  five six seven [eight]
 */

static char *test_user;

int pam_get_user(pam_handle_t *pamh, const char **user, const char *prompt) {
    *user = test_user;
    if (*user == NULL) {
	return PAM_CONV_AGAIN;
    }
    return PAM_SUCCESS;
}

int pam_get_item(const pam_handle_t *pamh, int item_type, const void **item) {
    if (item_type != PAM_USER) {
	errno = EINVAL;
	return -1;
    }
    *item = test_user;
    return 0;
}

int pam_set_data(pam_handle_t *pamh, const char *module_data_name, void *data,
		 void (*cleanup)(pam_handle_t *pamh, void *data,
				 int error_status)) {
    if (cleanup != iab_apply) {
	errno = EINVAL;
	return -1;
    }
    cap_free(data);
    return -1;
}

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups) {
    int i,j;
    for (i = 0; i < n_users; i++) {
	if (strcmp(user, test_users[i]) == 0) {
	    *ngroups = i+1;
	    break;
	}
    }
    if (i == n_users) {
	return -1;
    }
    groups[0] = i;
    for (j = 1; j < *ngroups; j++) {
	groups[j] = i+j;
    }
    return *ngroups;
}

static struct group gr;
struct group *getgrgid(gid_t gid) {
    if (gid >= n_groups) {
	errno = EINVAL;
	return NULL;
    }
    gr.gr_name = strdup(test_groups[gid]);
    return &gr;
}

static struct passwd pw;
struct passwd *getpwnam(const char *name) {
    int i;
    for (i = 0; i < n_users; i++) {
	if (strcmp(name, test_users[i]) == 0) {
	    pw.pw_gid = i;
	    return &pw;
	}
    }
    return NULL;
}

/* we'll use these to keep track of the three vectors - only use
   lowest 64 bits */

#define A 0
#define B 1
#define I 2

/*
 * load_vectors caches a copy of the lowest 64 bits of the inheritable
 * cap vectors
 */
static void load_vectors(unsigned long int bits[3]) {
    memset(bits, 0, 3*sizeof(unsigned long int));
    cap_t prev = cap_get_proc();
    int i;
    for (i = 0; i < 64; i++) {
	unsigned long int mask = (1ULL << i);
	int v = cap_get_bound(i);
	if (v < 0) {
	    break;
	}
	bits[B] |= v ? mask : 0;
	cap_flag_value_t u;
	if (cap_get_flag(prev, i, CAP_INHERITABLE, &u) != 0) {
	    break;
	}
	bits[I] |= u ? mask : 0;
	v = cap_get_ambient(i);
	if (v > 0) {
	    bits[A] |= mask;
	}
    }
    cap_free(prev);
}

struct vargs {
    struct pam_cap_s cs;
    const char *args[5];
};

static int test_arg_parsing(void) {
    static struct vargs vs[] = {
	{
	    { 1, 0, 0, 0, NULL, NULL, NULL },
	    { "debug", NULL }
	},
	{
	    { 0, 1, 0, 0, NULL, NULL, NULL },
	    { "keepcaps", NULL }
	},
	{
	    { 0, 0, 1, 0, NULL, NULL, NULL },
	    { "autoauth", NULL }
	},
	{
	    { 1, 0, 1, 0, NULL, NULL, NULL },
	    { "autoauth", "debug", NULL }
	},
	{
	    { 0, 0, 0, 0, NULL, "/over/there", NULL },
	    { "config=/over/there", NULL }
	},
	{
	    { 0, 0, 0, 0, NULL, NULL, "^cap_setfcap" },
	    { "default=^cap_setfcap", NULL }
	},
	{
	    { 0, 0, 0, 1, NULL, NULL, NULL },
	    { "defer", NULL }
	},
	{
	    { 0, 0, 0, 0, NULL, NULL, NULL },
	    { NULL }
	}
    };
    int i;

    for (i=0; ; i++) {
	int argc;
	const char **argv;
	struct vargs *v;

	v = &vs[i];
	argv = v->args;

	for (argc = 0; argv[argc] != NULL; argc++);

	struct pam_cap_s cs;
	parse_args(argc, argv, &cs);

	if (cs.debug != v->cs.debug) {
	    printf("test_arg_parsing[%d]: debug=%d, wanted debug=%d\n",
		   i, cs.debug, v->cs.debug);
	    return 1;
	}
	if (cs.keepcaps != v->cs.keepcaps) {
	    printf("test_arg_parsing[%d]: keepcaps=%d, wanted keepcaps=%d\n",
		   i, cs.keepcaps, v->cs.keepcaps);
	    return 1;
	}
	if (cs.autoauth != v->cs.autoauth) {
	    printf("test_arg_parsing[%d]: autoauth=%d, wanted autoauth=%d\n",
		   i, cs.autoauth, v->cs.autoauth);
	    return 1;
	}
	if (cs.conf_filename != v->cs.conf_filename &&
	    strcmp(cs.conf_filename, v->cs.conf_filename)) {
	    printf("test_arg_parsing[%d]: conf_filename=[%s], wanted=[%s]\n",
		   i, cs.conf_filename, v->cs.conf_filename);
	    return 1;
	}
	if (cs.fallback != v->cs.fallback &&
	    strcmp(cs.fallback, v->cs.fallback)) {
	    printf("test_arg_parsing[%d]: fallback=[%s], wanted=[%s]\n",
		   i, cs.fallback, v->cs.fallback);
	    return 1;
	}

	if (argc == 0) {
	    break;
	}
    }
    return 0;
}

/*
 * args: user a b i config-args...
 */
int main(int argc, char *argv[]) {
    unsigned long int before[3], change[3], after[3];

    if (test_arg_parsing()) {
	printf("failed to parse arguments\n");
	exit(1);
    }
    if (read_capabilities_for_user("alpha", "/dev/null") != NULL) {
	printf("/dev/null should return no capabilities\n");
	exit(1);
    }
    if (read_capabilities_for_user("unknown", "capability.conf") != NULL) {
	printf("capability.conf should return no capabilities for unknown\n");
	exit(1);
    }
    char *iab_text = read_capabilities_for_user("alpha", "./incapable.conf");
    if (iab_text != NULL) {
	printf("./incapable.conf should grant no capabilities: got=%s\n",
	       iab_text);
	free(iab_text);
	exit(1);
    }

    /*
     * Start out with a cleared inheritable set.
     */
    cap_t orig = cap_get_proc();
    cap_clear_flag(orig, CAP_INHERITABLE);
    cap_set_proc(orig);

    if (getuid() != 0) {
	cap_free(orig);
	printf("test_pam_cap: OK! (Skipping privileged tests (uid!=0))\n");
	exit(0);
    }
    if (argc == 1) {
	printf("test_pam_cap: OK (kick the tires test)\n");
	exit(0);
    }

    change[A] = strtoul(argv[2], NULL, 0);
    change[B] = strtoul(argv[3], NULL, 0);
    change[I] = strtoul(argv[4], NULL, 0);

    void* args_for_pam = argv+4;

    int status = pam_sm_authenticate(NULL, 0, argc-4,
				     (const char **) args_for_pam);
    if (status != PAM_INCOMPLETE) {
	printf("failed to recognize no username\n");
	exit(1);
    }

    test_user = argv[1];

    status = pam_sm_authenticate(NULL, 0, argc-4, (const char **) args_for_pam);
    if (status == PAM_IGNORE) {
	if (strcmp(test_user, "root") == 0) {
	    exit(0);
	}
	printf("unconfigured non-root user: %s\n", test_user);
	exit(1);
    }
    if (status != PAM_SUCCESS) {
	printf("failed to recognize username\n");
	exit(1);
    }

    /* Now it is time to execute the credential setting */
    load_vectors(before);

    status = pam_sm_setcred(NULL, PAM_ESTABLISH_CRED, argc-4,
			    (const char **) args_for_pam);

    load_vectors(after);

    printf("before: A=0x%016lx B=0x%016lx I=0x%016lx\n",
	   before[A], before[B], before[I]);

    long unsigned int dA = before[A] ^ after[A];
    long unsigned int dB = before[B] ^ after[B];
    long unsigned int dI = before[I] ^ after[I];

    printf("diff  : A=0x%016lx B=0x%016lx I=0x%016lx\n", dA, dB, dI);
    printf("after : A=0x%016lx B=0x%016lx I=0x%016lx\n",
	   after[A], after[B], after[I]);

    int failure = 0;
    if (after[A] != change[A]) {
	printf("Ambient set error: got=0x%016lx, want=0x%016lx\n",
	       after[A], change[A]);
	failure = 1;
    }
    if (dB != change[B]) {
	printf("Bounding set error: got=0x%016lx, want=0x%016lx\n",
	       after[B], before[B] ^ change[B]);
	failure = 1;
    }
    if (after[I] != change[I]) {
	printf("Inheritable set error: got=0x%016lx, want=0x%016lx\n",
	       after[I], change[I]);
	failure = 1;
    }

    exit(failure);
}
