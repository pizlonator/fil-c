#include <endian.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <selinux/selinux.h>
#include <sepol/sepol.h>

#include "../src/avc_sidtab.h"
#include "../src/label_file.h"
#include "../src/regex.h"


static const char *policy_file;
static int ctx_err;

static int validate_context(char **ctxp)
{
	const char *ctx = *ctxp;

	if (policy_file && sepol_check_context(ctx) < 0) {
		ctx_err = -1;
		return ctx_err;
	}

	return 0;
}

static int process_file(struct selabel_handle *rec, const char *filename)
{
	uint32_t line_num;
	int rc;
	char *line_buf = NULL;
	size_t line_len = 0;
	ssize_t nread;
	FILE *context_file;
	const char *prefix = NULL;

	context_file = fopen(filename, "re");
	if (!context_file) {
		fprintf(stderr, "Error opening %s: %m\n", filename);
		return -1;
	}

	line_num = 0;
	rc = 0;
	while ((nread = getline(&line_buf, &line_len, context_file)) > 0) {
		rc = process_line(rec, filename, prefix, line_buf, nread, 0, ++line_num);
		if (rc || ctx_err) {
			/* With -p option need to check and fail if ctx err as
			 * process_line() context validation on Linux does not
			 * return an error, but does print the error line to
			 * stderr. Android will set both to error and print
			 * the error line. */
			rc = -1;
			goto out;
		}
	}
out:
	free(line_buf);
	fclose(context_file);
	return rc;
}

static int literal_spec_to_sidtab(const struct literal_spec *lspec, struct sidtab *stab)
{
	security_id_t dummy;

	return sidtab_context_to_sid(stab, lspec->lr.ctx_raw, &dummy);
}

static int regex_spec_to_sidtab(const struct regex_spec *rspec, struct sidtab *stab)
{
	security_id_t dummy;

	return sidtab_context_to_sid(stab, rspec->lr.ctx_raw, &dummy);
}

static int spec_node_to_sidtab(const struct spec_node *node, struct sidtab *stab)
{
	int rc;

	for (uint32_t i = 0; i < node->literal_specs_num; i++) {
		rc = literal_spec_to_sidtab(&node->literal_specs[i], stab);
		if (rc)
			return rc;
	}

	for (uint32_t i = 0; i < node->regex_specs_num; i++) {
		rc = regex_spec_to_sidtab(&node->regex_specs[i], stab);
		if (rc)
			return rc;
	}

	for (uint32_t i = 0; i < node->children_num; i++) {
		rc = spec_node_to_sidtab(&node->children[i], stab);
		if (rc)
			return rc;
	}

	return 0;
}

static int create_sidtab(const struct saved_data *data, struct sidtab *stab)
{
	int rc;

	rc = sidtab_init(stab);
	if (rc < 0)
		return rc;

	return spec_node_to_sidtab(data->root, stab);
}


/*
 * File Format
 *
 * The format uses network byte-order.
 *
 * u32     - magic number
 * u32     - version
 * u32     - length of upcoming pcre version EXCLUDING nul
 * [char]  - pcre version string EXCLUDING nul
 * u32     - length of upcoming pcre architecture EXCLUDING nul
 * [char]  - pcre architecture string EXCLUDING nul
 * u64     - number of total specifications
 * u32     - number of upcoming context definitions
 * [Ctx]   - array of context definitions
 * Node    - root node
 *
 * Context Definition Format (Ctx)
 *
 * u16     - length of upcoming raw context EXCLUDING nul
 * [char]  - char array of the raw context EXCLUDING nul
 *
 * Node Format
 *
 * u16     - length of upcoming stem INCLUDING nul
 * [char]  - stem char array INCLUDING nul
 * u32     - number of upcoming literal specifications
 * [LSpec] - array of literal specifications
 * u32     - number of upcoming regular expression specifications
 * [RSpec] - array of regular expression specifications
 * u32     - number of upcoming child nodes
 * [Node]  - array of child nodes
 *
 * Literal Specification Format (LSpec)
 *
 * u32     - context table index for raw context (1-based)
 * u16     - length of upcoming regex_str INCLUDING nul
 * [char]  - char array of the original regex string including the stem INCLUDING nul
 * u16     - length of upcoming literal match INCLUDING nul
 * [char]  - char array of the simplified literal match INCLUDING nul
 * u8      - file kind (LABEL_FILE_KIND_*)
 *
 * Regular Expression Specification Format (RSpec)
 *
 * u32     - context table index for raw context (1-based)
 * u32     - line number in source file
 * u16     - length of upcoming regex_str INCLUDING nul
 * [char]  - char array of the original regex string including the stem INCLUDING nul
 * u16     - length of the fixed path prefix
 * u8      - file kind (LABEL_FILE_KIND_*)
 * [Regex] - serialized pattern of regex, subject to underlying regex library
 */


static int security_id_compare(const void *a, const void *b)
{
	const struct security_id *sid_a = a, *sid_b = b;

	return (sid_a->id > sid_b->id) - (sid_a->id < sid_b->id);
}

static int write_sidtab(FILE *bin_file, const struct sidtab *stab)
{
	struct security_id *sids;
	uint32_t data_u32, index;
	uint16_t data_u16;
	size_t len;

	/* write number of entries */
	data_u32 = htobe32(stab->nel);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	if (stab->nel == 0)
		return 0;

	/* sort entries by id */
	sids = calloc(stab->nel, sizeof(*sids));
	if (!sids)
		return -1;
	index = 0;
	for (unsigned i = 0; i < SIDTAB_SIZE; i++) {
		const struct sidtab_node *cur = stab->htable[i];

		while (cur) {
			sids[index++] = cur->sid_s;
			cur = cur->next;
		}
	}
	assert(index == stab->nel);
	qsort(sids, stab->nel, sizeof(struct security_id), security_id_compare);

	/* write raw contexts sorted by id */
	for (uint32_t i = 0; i < stab->nel; i++) {
		const char *ctx = sids[i].ctx;
		size_t ctx_len = strlen(ctx);

		if (ctx_len == 0 || ctx_len >= UINT16_MAX) {
			free(sids);
			return -2;
		}
		data_u16 = htobe16(ctx_len);
		len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
		if (len != 1) {
			free(sids);
			return -1;
		}
		len = fwrite(ctx, sizeof(char), ctx_len, bin_file);
		if (len != ctx_len) {
			free(sids);
			return -1;
		}
	}

	free(sids);
	return 0;
}

static int write_literal_spec(FILE *bin_file, const struct literal_spec *lspec, const struct sidtab *stab)
{
	const struct security_id *sid;
	const char *orig_regex, *literal_match;
	size_t orig_regex_len, literal_match_len;
	uint32_t data_u32;
	uint16_t data_u16;
	uint8_t data_u8;
	size_t len;

	/* write raw context sid */
	sid = sidtab_context_lookup(stab, lspec->lr.ctx_raw);
	assert(sid); /* should be set via create_sidtab() */
	data_u32 = htobe32(sid->id);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write original regex string */
	orig_regex = lspec->regex_str;
	orig_regex_len = strlen(orig_regex);
	if (orig_regex_len == 0 || orig_regex_len >= UINT16_MAX)
		return -2;
	orig_regex_len += 1;
	data_u16 = htobe16(orig_regex_len);
	len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
	if (len != 1)
		return -1;
	len = fwrite(orig_regex, sizeof(char), orig_regex_len, bin_file);
	if (len != orig_regex_len)
		return -1;

	/* write literal match string */
	literal_match = lspec->literal_match;
	literal_match_len = strlen(literal_match);
	if (literal_match_len == 0 || literal_match_len >= UINT16_MAX)
		return -2;
	literal_match_len += 1;
	data_u16 = htobe16(literal_match_len);
	len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
	if (len != 1)
		return -1;
	len = fwrite(literal_match, sizeof(char), literal_match_len, bin_file);
	if (len != literal_match_len)
		return -1;

	/* write file kind */
	data_u8 = lspec->file_kind;
	len = fwrite(&data_u8, sizeof(uint8_t), 1, bin_file);
	if (len != 1)
		return -1;

	return 0;
}

static int write_regex_spec(FILE *bin_file, bool do_write_precompregex, const struct regex_spec *rspec, const struct sidtab *stab)
{
	const struct security_id *sid;
	const char *regex;
	size_t regex_len;
	uint32_t data_u32;
	uint16_t data_u16;
	uint8_t data_u8;
	size_t len;
	int rc;

	/* write raw context sid */
	sid = sidtab_context_lookup(stab, rspec->lr.ctx_raw);
	assert(sid); /* should be set via create_sidtab() */
	data_u32 = htobe32(sid->id);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write line number */
	data_u32 = htobe32(rspec->lineno);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write regex string */
	regex = rspec->regex_str;
	regex_len = strlen(regex);
	if (regex_len == 0 || regex_len >= UINT16_MAX)
		return -2;
	regex_len += 1;
	data_u16 = htobe16(regex_len);
	len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
	if (len != 1)
		return -1;
	len = fwrite(regex, sizeof(char), regex_len, bin_file);
	if (len != regex_len)
		return -1;

	/* write prefix length */
	data_u16 = htobe16(rspec->prefix_len);
	len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write file kind */
	data_u8 = rspec->file_kind;
	len = fwrite(&data_u8, sizeof(uint8_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* Write serialized regex */
	rc = regex_writef(rspec->regex, bin_file, do_write_precompregex);
	if (rc < 0)
		return rc;

	return 0;
}

static int write_spec_node(FILE *bin_file, bool do_write_precompregex, const struct spec_node *node, const struct sidtab *stab)
{
	size_t stem_len;
	uint32_t data_u32;
	uint16_t data_u16;
	size_t len;
	int rc;

	stem_len = node->stem_len;
	if ((stem_len == 0 && node->parent) || stem_len >= UINT16_MAX)
		return -2;
	stem_len += 1;
	data_u16 = htobe16(stem_len);
	len = fwrite(&data_u16, sizeof(uint16_t), 1, bin_file);
	if (len != 1)
		return -1;
	len = fwrite(node->stem ?: "", sizeof(char), stem_len, bin_file);
	if (len != stem_len)
		return -1;

	/* write number of literal specs */
	data_u32 = htobe32(node->literal_specs_num);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write literal specs */
	for (uint32_t i = 0; i < node->literal_specs_num; i++) {
		rc = write_literal_spec(bin_file, &node->literal_specs[i], stab);
		if (rc)
			return rc;
	}

	/* write number of regex specs */
	data_u32 = htobe32(node->regex_specs_num);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write regex specs */
	for (uint32_t i = 0; i < node->regex_specs_num; i++) {
		rc = write_regex_spec(bin_file, do_write_precompregex, &node->regex_specs[i], stab);
		if (rc)
			return rc;
	}

	/* write number of child nodes */
	data_u32 = htobe32(node->children_num);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		return -1;

	/* write child nodes */
	for (uint32_t i = 0; i < node->children_num; i++) {
		rc = write_spec_node(bin_file, do_write_precompregex, &node->children[i], stab);
		if (rc)
			return rc;
	}

	return 0;
}

static int write_binary_file(const struct saved_data *data, const struct sidtab *stab,
			     int fd, const char *path, bool do_write_precompregex,
			     const char *progname)
{
	FILE *bin_file;
	const char *reg_arch, *reg_version;
	size_t len, reg_arch_len, reg_version_len;
	uint64_t data_u64;
	uint32_t data_u32;
	int rc;

	bin_file = fdopen(fd, "we");
	if (!bin_file) {
		fprintf(stderr, "%s: failed to open %s: %m\n", progname, path);
		close(fd);
		return -1;
	}

	/* write some magic number */
	data_u32 = htobe32(SELINUX_MAGIC_COMPILED_FCONTEXT);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		goto err_write;

	/* write the version */
	data_u32 = htobe32(SELINUX_COMPILED_FCONTEXT_MAX_VERS);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		goto err_write;

	/* write version of the regex back-end */
	reg_version = regex_version();
	if (!reg_version)
		goto err_check;
	reg_version_len = strlen(reg_version);
	if (reg_version_len == 0 || reg_version_len >= UINT32_MAX)
		goto err_check;
	data_u32 = htobe32(reg_version_len);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		goto err_write;
	len = fwrite(reg_version, sizeof(char), reg_version_len, bin_file);
	if (len != reg_version_len)
		goto err_write;

	/* write regex arch string */
	reg_arch = regex_arch_string();
	if (!reg_arch)
		goto err_check;
	reg_arch_len = strlen(reg_arch);
	if (reg_arch_len == 0 || reg_arch_len >= UINT32_MAX)
		goto err_check;
	data_u32 = htobe32(reg_arch_len);
	len = fwrite(&data_u32, sizeof(uint32_t), 1, bin_file);
	if (len != 1)
		goto err_write;
	len = fwrite(reg_arch, sizeof(char), reg_arch_len, bin_file);
	if (len != reg_arch_len)
		goto err_write;

	/* write number of total specifications */
	data_u64 = htobe64(data->num_specs);
	len = fwrite(&data_u64, sizeof(uint64_t), 1, bin_file);
	if (len != 1)
		goto err_write;

	/* write context table */
	rc = write_sidtab(bin_file, stab);
	if (rc)
		goto err;

	rc = write_spec_node(bin_file, do_write_precompregex, data->root, stab);
	if (rc)
		goto err;

out:
	if (fclose(bin_file) && rc == 0) {
		fprintf(stderr, "%s: failed to close %s: %m\n", progname, path);
		rc = -1;
	}
	return rc;

err_check:
	rc = -2;
	goto err;

err_write:
	rc = -1;
	goto err;

err:
	fprintf(stderr, "%s: failed to compile file context specifications: %s\n",
		progname,
		(rc == -3) ? "regex serialization failure" :
		((rc == -2) ? "invalid fcontext specification" : "write failure"));
	goto out;
}

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
	    "usage: %s [-iV] [-o out_file] [-p policy_file] fc_file\n"
	    "Where:\n\t"
	    "-o       Optional file name of the PCRE formatted binary\n\t"
	    "         file to be output. If not specified the default\n\t"
	    "         will be fc_file with the .bin suffix appended.\n\t"
	    "-p       Optional binary policy file that will be used to\n\t"
	    "         validate contexts defined in the fc_file.\n\t"
	    "-r       Omit precompiled regular expressions from the output.\n\t"
	    "         (PCRE2 only. Compiled PCRE2 regular expressions are\n\t"
	    "         not portable across architectures. Use this flag\n\t"
	    "         if you know that you build for an incompatible\n\t"
	    "         architecture to save space. When linked against\n\t"
	    "         PCRE1 this flag is ignored.)\n\t"
	    "-i       Print regular expression info end exit. That is, back\n\t"
	    "         end version and architecture identifier.\n\t"
	    "         Arch identifier format (PCRE2):\n\t"
	    "         <pointer width>-<size type width>-<endianness>, e.g.,\n\t"
	    "         \"8-8-el\" for x86_64.\n\t"
	    "-V       Print binary output format version and exit.\n\t"
	    "fc_file  The text based file contexts file to be processed.\n",
	    progname);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	const char *path;
	const char *out_file = NULL;
	bool do_write_precompregex = true;
	char stack_path[PATH_MAX + 1];
	char *tmp = NULL;
	size_t len;
	int fd, rc, opt;
	FILE *policy_fp = NULL;
	struct stat buf;
	struct selabel_handle *rec = NULL;
	struct saved_data *data = NULL;
	struct spec_node *root = NULL;
	struct sidtab stab = {};

	if (argc < 2)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "io:p:rV")) > 0) {
		switch (opt) {
		case 'o':
			out_file = optarg;
			break;
		case 'p':
			policy_file = optarg;
			break;
		case 'r':
			do_write_precompregex = false;
			break;
		case 'i':
			printf("%s (%s)\n", regex_version(), regex_arch_string());
			return 0;
		case 'V':
			printf("Compiled fcontext format version %d\n", SELINUX_COMPILED_FCONTEXT_MAX_VERS);
			return 0;
		default:
			usage(argv[0]);
		}
	}

	if (optind + 1 != argc)
		usage(argv[0]);

	path = argv[optind];
	if (stat(path, &buf) < 0) {
		fprintf(stderr, "%s: could not stat: %s: %s\n", argv[0], path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Open binary policy if supplied. */
	if (policy_file) {
		policy_fp = fopen(policy_file, "re");

		if (!policy_fp) {
			fprintf(stderr, "%s: failed to open %s: %s\n",
				argv[0], policy_file, strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (sepol_set_policydb_from_file(policy_fp) < 0) {
			fprintf(stderr, "%s: failed to load policy from %s\n",
				argv[0], policy_file);
			fclose(policy_fp);
			exit(EXIT_FAILURE);
		}
	}

	/* Generate dummy handle for process_line() function */
	rec = (struct selabel_handle *)calloc(1, sizeof(*rec));
	if (!rec) {
		fprintf(stderr, "%s: calloc failed: %s\n", argv[0], strerror(errno));
		if (policy_fp)
			fclose(policy_fp);
		exit(EXIT_FAILURE);
	}
	rec->backend = SELABEL_CTX_FILE;

	/* Need to set validation on to get the bin file generated by the
	 * process_line function, however as the bin file being generated
	 * may not be related to the currently loaded policy (that it
	 * would be validated against), then set callback to ignore any
	 * validation - unless the -p option is used in which case if an
	 * error is detected, the process will be aborted. */
	rec->validating = 1;
	selinux_set_callback(SELINUX_CB_VALIDATE,
			    (union selinux_callback) { .func_validate = &validate_context });

	data = (struct saved_data *)calloc(1, sizeof(*data));
	if (!data) {
		fprintf(stderr, "%s: calloc failed: %s\n", argv[0], strerror(errno));
		free(rec);
		if (policy_fp)
			fclose(policy_fp);
		exit(EXIT_FAILURE);
	}

	root = calloc(1, sizeof(*root));
	if (!root) {
		fprintf(stderr, "%s: calloc failed: %s\n", argv[0], strerror(errno));
		free(data);
		free(rec);
		if (policy_fp)
			fclose(policy_fp);
		exit(EXIT_FAILURE);
	}

	data->root = root;
	rec->data = data;

	rc = process_file(rec, path);
	if (rc < 0) {
		fprintf(stderr, "%s: process_file failed\n", argv[0]);
		goto err;
	}

	sort_specs(data);

	rc = create_sidtab(data, &stab);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to generate sidtab: %s\n", argv[0], strerror(errno));
		goto err;
	}

	if (out_file)
		rc = snprintf(stack_path, sizeof(stack_path), "%s", out_file);
	else
		rc = snprintf(stack_path, sizeof(stack_path), "%s.bin", path);

	if (rc < 0 || (size_t)rc >= sizeof(stack_path)) {
		fprintf(stderr, "%s: snprintf failed\n", argv[0]);
		goto err;
	}
	len = rc;

	tmp = malloc(len + 7);
	if (!tmp) {
		fprintf(stderr, "%s: malloc failed: %s\n", argv[0], strerror(errno));
		goto err;
	}

	rc = snprintf(tmp, len + 7, "%sXXXXXX", stack_path);
	if (rc < 0 || (size_t)rc >= len + 7) {
		fprintf(stderr, "%s: snprintf failed\n", argv[0]);
		goto err;
	}

	fd = mkstemp(tmp);
	if (fd < 0) {
		fprintf(stderr, "%s: mkstemp %s failed: %s\n", argv[0], tmp, strerror(errno));
		close(fd);
		goto err;
	}

	rc = fchmod(fd, buf.st_mode);
	if (rc < 0) {
		fprintf(stderr, "%s: fchmod %s failed: %s\n", argv[0], tmp, strerror(errno));
		close(fd);
		goto err_unlink;
	}

	rc = write_binary_file(data, &stab, fd, tmp, do_write_precompregex, argv[0]);
	if (rc < 0)
		goto err_unlink;

	rc = rename(tmp, stack_path);
	if (rc < 0) {
		fprintf(stderr, "%s: rename %s -> %s failed: %s\n", argv[0], tmp, stack_path, strerror(errno));
		goto err_unlink;
	}

	rc = 0;
out:
	if (policy_fp)
		fclose(policy_fp);

	sidtab_destroy(&stab);
	free_spec_node(data->root);
	free(data->root);
	free(data);
	free(rec);
	free(tmp);
	return rc;

err_unlink:
	unlink(tmp);
err:
	rc = -1;
	goto out;
}
