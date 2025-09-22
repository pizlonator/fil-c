/*
 * Copyright (c) 1997,2007-8,2020,21 Andrew G. Morgan <morgan@kernel.org>
 *
 * This sets/verifies the capabilities of a given file.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <unistd.h>

static void usage(int status)
{
    fprintf(stderr,
	    "usage: setcap [-h] [-q] [-v] [-n <rootid>] (-r|-|<caps>) <filename> "
	    "[ ... (-r|-|<capsN>) <filenameN> ]\n"
	    "\n"
	    " Note <filename> must be a regular (non-symlink) file.\n"
	    " -r          remove capability from file\n"
	    " -           read capability text from stdin\n"
	    " <capsN>     cap_from_text(3) formatted file capability\n"
	    " [ Note: capsh --suggest=\"something...\" might help you pick. ]"
	    "\n"
	    " -h          this message and exit status 0\n"
	    " -f          force setting even when the capability is invalid\n"
	    " -q          quietly\n"
	    " -v          validate supplied capability matches file\n"
	    " -n <rootid> write a user namespace (!= 0) limited capability\n"
	    " --license   display the license info\n"
	);
    exit(status);
}

/* parse a positive integer with some error handling */
static unsigned long pos_uint(const char *text, const char *prefix, int *ok)
{
    char *remains;
    unsigned long value;
    ssize_t len = strlen(text);

    if (len == 0 || *text == '-') {
	goto fail;
    }
    value = strtoul(text, &remains, 0);
    if (*remains || value == 0) {
	goto fail;
    }
    if (ok != NULL) {
	*ok = 1;
    }
    return value;

fail:
    if (ok == NULL) {
	fprintf(stderr, "%s: want positive integer, got \"%s\"\n",
		prefix, text);
	exit(1);
    }
    *ok = 0;
    return 0;
}

#define MAXCAP 2048

static int read_caps(int quiet, const char *filename, char *buffer)
{
    int i = MAXCAP;

    if (!quiet) {
	fprintf(stderr,	"Please enter caps for file [empty line to end]:\n");
    }
    while (i > 0) {
	int j = read(STDIN_FILENO, buffer, i);

	if (j < 0) {
	    fprintf(stderr, "\n[Error - aborting]\n");
	    exit(1);
	}

	if (j==0 || buffer[0] == '\n') {
	    /* we're done */
	    break;
	}

	/* move on... */

	i -= j;
	buffer += j;
    }

    /* <NUL> terminate */
    buffer[0] = '\0';

    return (i < MAXCAP ? 0:-1);
}

int main(int argc, char **argv)
{
    int tried_to_cap_setfcap = 0;
    char buffer[MAXCAP+1];
    int retval, quiet = 0, verify = 0, forced = 0;
    cap_t mycaps;
    cap_value_t capflag;
    uid_t rootid = 0, f_rootid;

    if (argc < 2) {
	usage(1);
    }

    mycaps = cap_get_proc();
    if (mycaps == NULL) {
	fprintf(stderr, "warning - unable to get process capabilities"
		" (old libcap?)\n");
    }

    cap_t cap_d = NULL;
    while (--argc > 0) {
	const char *text;

	cap_free(cap_d);
	cap_d = NULL;

	if (!strcmp(*++argv, "-q")) {
	    quiet = 1;
	    continue;
	}
	if (!strcmp("--license", *argv)) {
	    printf(
		"%s see LICENSE file for details.\n"
		"Copyright (c) 1997,2007-8,2020-21 Andrew G. Morgan"
		" <morgan@kernel.org>\n", argv[0]);
	    exit(0);
	}
	if (!strcmp(*argv, "-f")) {
	    forced = 1;
	    continue;
	}
	if (!strcmp(*argv, "-h")) {
	    usage(0);
	}
	if (!strcmp(*argv, "-v")) {
	    verify = 1;
	    continue;
	}
	if (!strcmp(*argv, "-n")) {
	    if (argc < 2) {
		fprintf(stderr,
			"usage: .. -n <rootid> .. - rootid!=0 file caps");
		exit(1);
	    }
	    --argc;
	    rootid = (uid_t) pos_uint(*++argv, "bad ns rootid", NULL);
	    continue;
	}

	if (!strcmp(*argv, "-r")) {
	    cap_free(cap_d);
	    cap_d = NULL;
	} else {
	    if (!strcmp(*argv,"-")) {
		retval = read_caps(quiet, *argv, buffer);
		if (retval)
		    usage(1);
		text = buffer;
	    } else {
		text = *argv;
	    }

	    int non_space = 0, j;
	    for (j = 0; text[j]; j++) {
		if (!isspace(text[j])) {
		    non_space = 1;
		    break;
		}
	    }
	    if (!non_space) {
		fprintf(stderr, "empty space is an invalid capability, did you mean -r?\n");
		exit(1);
	    }
	    cap_d = cap_from_text(text);
	    if (cap_d == NULL) {
		perror("fatal error");
		usage(1);
	    }
	    if (cap_set_nsowner(cap_d, rootid)) {
		perror("unable to set nsowner");
		exit(1);
	    }
#ifdef DEBUG
	    {
		char *result = cap_to_text(cap_d, NULL);
		fprintf(stderr, "caps set to: [%s]\n", result);
		cap_free(result);
	    }
#endif
	}

	if (--argc <= 0)
	    usage(1);
	/*
	 * Set the filesystem capability for this file.
	 */
	if (verify) {
	    cap_t cap_on_file;
	    int cmp;

	    if (cap_d == NULL) {
		cap_d = cap_init();
		if (cap_d == NULL) {
		    perror("unable to obtain empty capability");
		    exit(1);
		}
	    }

	    cap_on_file = cap_get_file(*++argv);
	    if (cap_on_file == NULL) {
		cap_on_file = cap_init();
		if (cap_on_file == NULL) {
		    perror("unable to use missing capability");
		    exit(1);
		}
	    }

	    cmp = cap_compare(cap_on_file, cap_d);
	    f_rootid = cap_get_nsowner(cap_on_file);
	    cap_free(cap_on_file);

	    if (cmp != 0 || rootid != f_rootid) {
		if (!quiet) {
		    if (rootid != f_rootid) {
			printf("nsowner[got=%d, want=%d],", f_rootid, rootid);
		    }
		    printf("%s differs in [%s%s%s]\n", *argv,
			   CAP_DIFFERS(cmp, CAP_PERMITTED) ? "p" : "",
			   CAP_DIFFERS(cmp, CAP_INHERITABLE) ? "i" : "",
			   CAP_DIFFERS(cmp, CAP_EFFECTIVE) ? "e" : "");
		}
		exit(1);
	    }
	    if (!quiet) {
		printf("%s: OK\n", *argv);
	    }
	} else {
	    if (!tried_to_cap_setfcap) {
		capflag = CAP_SETFCAP;

		/*
		 * Raise the effective CAP_SETFCAP.
		 */
		if (cap_set_flag(mycaps, CAP_EFFECTIVE, 1, &capflag, CAP_SET)
		    != 0) {
		    perror("unable to manipulate CAP_SETFCAP - "
			   "try a newer libcap?");
		    exit(1);
		}
		if (cap_set_proc(mycaps) != 0) {
		    perror("unable to set CAP_SETFCAP effective capability");
		    exit(1);
		}
		tried_to_cap_setfcap = 1;
	    }
#ifdef linux
	    {
		/* Linux's file capabilities have a compressed representation. */
		int explained = 0;
		int somebits = 0;
		cap_value_t cap;
		cap_flag_value_t per_state;

		for (cap = 0;
		     cap_get_flag(cap_d, cap, CAP_PERMITTED, &per_state) != -1;
		     cap++) {
		    cap_flag_value_t inh_state, eff_state, combined;

		    cap_get_flag(cap_d, cap, CAP_INHERITABLE, &inh_state);
		    cap_get_flag(cap_d, cap, CAP_EFFECTIVE, &eff_state);
		    combined = (inh_state | per_state);
		    somebits |= !!eff_state;
		    if (combined != eff_state) {
			explained = 1;
			break;
		    }
		}
		if (somebits && explained) {
		    fprintf(stderr, "Error: under Linux, effective file capabilities must either be empty, or\n"
			    "       exactly match the union of selected permitted and inheritable bits.\n");
		    if (!forced) {
			exit(1);
		    }
		}
	    }
#endif /* def linux */
	    errno = 0;
	    retval = cap_set_file(*++argv, cap_d);
	    if (retval != 0) {
		switch (errno) {
		case EINVAL:
		    fprintf(stderr,
			    "Invalid file '%s' for capability operation\n",
			    argv[0]);
		    exit(1);
		case ENODATA:
		    if (cap_d == NULL) {
			fprintf(stderr,
				"File '%s' has no capablity to remove\n",
				argv[0]);
			if (forced) {
			    break;
			}
			exit(1);
		    }
		    /* FALLTHROUGH */
		default:
		    fprintf(stderr,
			    "Failed to set capabilities on file '%s': %s\n",
			    argv[0], strerror(errno));
		    exit(1);
		}
	    }
	}
    }
    if (cap_d) {
	cap_free(cap_d);
    }

    exit(0);
}
