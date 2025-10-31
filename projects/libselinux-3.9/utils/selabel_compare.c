#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <selinux/label.h>


static void usage(const char *progname)
{
	fprintf(stderr,
		"usage: %s [-b backend] [-v] file1 file2\n\n"
		"Where:\n\t"
		"-b           The backend - \"file\", \"media\", \"x\", \"db\" or \"prop\" (defaults to \"file\")\n\t"
		"-v           Validate entries against loaded policy.\n\t"
		"file1/file2  Files containing the specs.\n",
		progname);
}

static int compare(const char *file1, const char *file2, const char *validate, unsigned int backend)
{
	struct selabel_handle *hnd1, *hnd2;
	const struct selinux_opt selabel_option1[] = {
		{ SELABEL_OPT_PATH, file1 },
		{ SELABEL_OPT_VALIDATE, validate }
	};
	const struct selinux_opt selabel_option2[] = {
		{ SELABEL_OPT_PATH, file2 },
		{ SELABEL_OPT_VALIDATE, validate }
	};
	enum selabel_cmp_result result;

	hnd1 = selabel_open(backend, selabel_option1, 2);
	if (!hnd1) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain handle for %s:  %m\n", file1);
		return EXIT_FAILURE;
	}

	hnd2 = selabel_open(backend, selabel_option2, 2);
	if (!hnd2) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain handle for %s:  %m\n", file2);
		selabel_close(hnd1);
		return EXIT_FAILURE;
	}

	result = selabel_cmp(hnd1, hnd2);

	selabel_close(hnd2);
	selabel_close(hnd1);

	switch (result) {
	case SELABEL_SUBSET:
		printf("spec %s is a subset of spec %s\n", file1, file2);
		break;
	case SELABEL_EQUAL:
		printf("spec %s is equal to spec %s\n", file1, file2);
		break;
	case SELABEL_SUPERSET:
		printf("spec %s is a superset of spec %s\n", file1, file2);
		break;
	case SELABEL_INCOMPARABLE:
		printf("spec %s is uncomparable to spec %s\n", file1, file2);
		break;
	default:
		fprintf(stderr, "ERROR: selabel_cmp - Unexpected result %d\n", result);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int backend = SELABEL_CTX_FILE;
	int opt;
	const char *validate = NULL, *file1 = NULL, *file2 = NULL;

	if (argc < 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	while ((opt = getopt(argc, argv, "b:v")) > 0) {
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
				fprintf(stderr, "Unknown backend: %s\n", optarg);
				usage(argv[0]);
				return EXIT_FAILURE;
			}
			break;
		case 'v':
			validate = (char *)1;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (argc != optind + 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	file1 = argv[optind++];
	file2 = argv[optind];

	return compare(file1, file2, validate, backend);
}
