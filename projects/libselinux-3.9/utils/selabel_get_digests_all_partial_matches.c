#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <fts.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage:  %s [-vr] [-f file] path\n\n"
		"Where:\n\t"
		"-v  Validate file_contxts entries against loaded policy.\n\t"
		"-r  Recursively descend directories.\n\t"
		"-f  Optional file_contexts file (defaults to current policy).\n\t"
		"path  Path to check current SHA1 digest against file_contexts entries.\n\n"
		"This will check the directory selinux.sehash SHA1 digest for "
		"<path> against\na newly generated digest based on the "
		"file_context entries for that node\n(using the regx, mode "
		"and path entries).\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, fts_flags;
	size_t i, digest_len;
	bool status, recurse = false;
	FTS *fts;
	FTSENT *ftsent;
	char *validate = NULL, *file = NULL;
	char *paths[2] = { NULL, NULL };
	uint8_t *xattr_digest = NULL;
	uint8_t *calculated_digest = NULL;
	char *sha1_buf = NULL;

	struct selabel_handle *hnd;
	struct selinux_opt selabel_option[] = {
		{ SELABEL_OPT_PATH, file },
		{ SELABEL_OPT_VALIDATE, validate }
	};

	if (argc < 2)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "f:rv")) > 0) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 'r':
			recurse = true;
			break;
		case 'v':
			validate = (char *)1;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No pathname specified\n");
		exit(-1);
	}

	paths[0] = argv[optind];

	selabel_option[0].value = file;
	selabel_option[1].value = validate;

	hnd = selabel_open(SELABEL_CTX_FILE, selabel_option, 2);
	if (!hnd) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain "
							     "handle:  %s\n",
							     strerror(errno));
		return -1;
	}

	fts_flags = FTS_PHYSICAL | FTS_NOCHDIR;
	fts = fts_open(paths, fts_flags, NULL);
	if (!fts) {
		printf("fts error on %s: %s\n",
		       paths[0], strerror(errno));
		return -1;
	}

	while ((ftsent = fts_read(fts)) != NULL) {
		switch (ftsent->fts_info) {
		case FTS_DP:
			continue;
		case FTS_D: {

			xattr_digest = NULL;
			calculated_digest = NULL;
			digest_len = 0;

			status = selabel_get_digests_all_partial_matches(hnd,
							 ftsent->fts_path,
							 &calculated_digest,
							 &xattr_digest,
							 &digest_len);

			sha1_buf = calloc(1, digest_len * 2 + 1);
			if (!sha1_buf) {
				fprintf(stderr, "Could not calloc buffer ERROR: %s\n",
					    strerror(errno));
				return -1;
			}

			if (status) { /* They match */
				printf("xattr and file_contexts SHA1 digests match for: %s\n",
				       ftsent->fts_path);

				if (calculated_digest) {
					for (i = 0; i < digest_len; i++)
						sprintf((&sha1_buf[i * 2]),
							"%02x",
							calculated_digest[i]);
					printf("SHA1 digest: %s\n", sha1_buf);
				}
			} else {
				if (!calculated_digest) {
					printf("No SHA1 digest available for: %s\n",
					       ftsent->fts_path);
					printf("as file_context entry is \"<<none>>\"\n");
					goto cleanup;
				}

				printf("The file_context entries for: %s\n",
				       ftsent->fts_path);

				for (i = 0; i < digest_len; i++)
					sprintf((&sha1_buf[i * 2]), "%02x",
						calculated_digest[i]);
				printf("generated SHA1 digest: %s\n", sha1_buf);

				if (!xattr_digest) {
					printf("however there is no selinux.sehash xattr entry.\n");
				} else {
					printf("however it does NOT match the current entry of:\n");
					for (i = 0; i < digest_len; i++)
						sprintf((&sha1_buf[i * 2]),
							"%02x",
							xattr_digest[i]);
					printf("%s\n", sha1_buf);
				}
			}
			cleanup:
			free(xattr_digest);
			free(calculated_digest);
			free(sha1_buf);
			break;
		}
		default:
			break;
		}

		if (!recurse)
			break;
	}

	(void) fts_close(fts);
	(void) selabel_close(hnd);
	return 0;
}
