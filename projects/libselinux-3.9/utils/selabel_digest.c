#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage: %s -b backend [-v] [-B] [-i] [-f file]\n\n"
		"Where:\n\t"
		"-b  The backend - \"file\", \"media\", \"x\", \"db\" or "
			"\"prop\"\n\t"
		"-v  Run \"cat <specfile_list> | openssl dgst -sha1 -hex\"\n\t"
		"    on the list of specfiles to compare the SHA1 digests.\n\t"
		"-B  Use base specfiles only (valid for \"-b file\" only).\n\t"
		"-i  Do not request a digest.\n\t"
		"-f  Optional file containing the specs (defaults to\n\t"
		"    those used by loaded policy).\n\n",
		progname);
	exit(1);
}

static int run_check_digest(const char *cmd, const char *selabel_digest, size_t digest_len)
{
	FILE *fp;
	char files_digest[128];
	const char *files_ptr;
	int rc = 0;

	fp = popen(cmd, "r");
	if (!fp) {
		fprintf(stderr, "Failed to run command '%s':  %s\n", cmd, strerror(errno));
		return -1;
	}

	/* Only expect one line "(stdin)= x.." so read and find first space */
	while (fgets(files_digest, sizeof(files_digest) - 1, fp) != NULL)
		;

	files_ptr = strstr(files_digest, " ");

	rc = strncmp(selabel_digest, files_ptr + 1, digest_len * 2);
	if (rc) {
		printf("Failed validation:\n\tselabel_digest: %s\n\t"
				    "files_digest:   %s\n",
				    selabel_digest, files_ptr + 1);
	} else {
		printf("Passed validation - digest: %s\n", selabel_digest);
	}

	pclose(fp);
	return rc;
}

int main(int argc, char **argv)
{
	unsigned int backend = SELABEL_CTX_FILE;
	int rc, opt, validate = 0;
	char *baseonly = NULL, *file = NULL, *digest = (char *)1;
	char **specfiles = NULL;
	unsigned char *sha1_digest = NULL;
	size_t digest_len, i, num_specfiles;

	char cmd_buf[4096];
	char *cmd_ptr;
	char *sha1_buf = NULL;

	struct selabel_handle *hnd;
	struct selinux_opt selabel_option[] = {
		{ SELABEL_OPT_PATH, file },
		{ SELABEL_OPT_DIGEST, digest },
		{ SELABEL_OPT_BASEONLY, baseonly }
	};

	if (argc < 3)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "ib:Bvf:")) > 0) {
		switch (opt) {
		case 'b':
			if (!strcasecmp(optarg, "file")) {
				backend = SELABEL_CTX_FILE;
			} else if (!strcmp(optarg, "media")) {
				backend = SELABEL_CTX_MEDIA;
			} else if (!strcmp(optarg, "x")) {
				backend = SELABEL_CTX_X;
			} else if (!strcmp(optarg, "db")) {
				backend = SELABEL_CTX_DB;
			} else if (!strcmp(optarg, "prop")) {
				backend = SELABEL_CTX_ANDROID_PROP;
			} else if (!strcmp(optarg, "service")) {
				backend = SELABEL_CTX_ANDROID_SERVICE;
			} else {
				fprintf(stderr, "Unknown backend: %s\n",
								    optarg);
				usage(argv[0]);
			}
			break;
		case 'B':
			baseonly = (char *)1;
			break;
		case 'v':
			validate = 1;
			break;
		case 'i':
			digest = NULL;
			break;
		case 'f':
			file = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	memset(cmd_buf, 0, sizeof(cmd_buf));

	selabel_option[0].value = file;
	selabel_option[1].value = digest;
	selabel_option[2].value = baseonly;

	hnd = selabel_open(backend, selabel_option, backend == SELABEL_CTX_FILE ? 3 : 2);
	if (!hnd) {
		switch (errno) {
		case EOVERFLOW:
			fprintf(stderr, "ERROR Number of specfiles or specfile"
					" buffer caused an overflow.\n");
			break;
		default:
			fprintf(stderr, "ERROR: selabel_open: %s\n",
						    strerror(errno));
		}
		return -1;
	}

	rc = selabel_digest(hnd, &sha1_digest, &digest_len, &specfiles,
							    &num_specfiles);

	if (rc) {
		switch (errno) {
		case EINVAL:
			fprintf(stderr, "No digest available.\n");
			break;
		default:
			fprintf(stderr, "selabel_digest ERROR: %s\n",
						    strerror(errno));
		}
		goto err;
	}

	sha1_buf = malloc(digest_len * 2 + 1);
	if (!sha1_buf) {
		fprintf(stderr, "Could not malloc buffer ERROR: %s\n",
						    strerror(errno));
		rc = -1;
		goto err;
	}

	printf("SHA1 digest: ");
	for (i = 0; i < digest_len; i++)
		sprintf(&(sha1_buf[i * 2]), "%02x", sha1_digest[i]);

	printf("%s\n", sha1_buf);
	printf("calculated using the following specfile(s):\n");

	if (specfiles) {
		size_t cmd_rem = sizeof(cmd_buf);
		int ret;

		if (validate) {
			cmd_ptr = &cmd_buf[0];
			ret = snprintf(cmd_ptr, cmd_rem, "/usr/bin/cat ");
			if (ret < 0 || (size_t)ret >= cmd_rem) {
				fprintf(stderr, "Could not format validate command\n");
				rc = -1;
				goto err;
			}
			cmd_ptr += ret;
			cmd_rem -= ret;
		}

		for (i = 0; i < num_specfiles; i++) {
			if (validate) {
				ret = snprintf(cmd_ptr, cmd_rem, "%s ", specfiles[i]);
				if (ret < 0 || (size_t)ret >= cmd_rem) {
					fprintf(stderr, "Could not format validate command\n");
					rc = -1;
					goto err;
				}
				cmd_ptr += ret;
				cmd_rem -= ret;
			}

			printf("%s\n", specfiles[i]);
		}

		if (validate) {
			ret = snprintf(cmd_ptr, cmd_rem, "| /usr/bin/openssl dgst -sha1 -hex");
			if (ret < 0 || (size_t)ret >= cmd_rem) {
				fprintf(stderr, "Could not format validate command\n");
				rc = -1;
				goto err;
			}

			rc = run_check_digest(cmd_buf, sha1_buf, digest_len);
		}
	}

err:
	free(sha1_buf);
	selabel_close(hnd);
	return rc;
}
