#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <selinux/label.h>

#include "../src/label_file.h"

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define MEMFD_FILE_NAME "file_contexts"
#define CTRL_PARTIAL  (1U << 0)
#define CTRL_FIND_ALL (1U << 1)
#define CTRL_MODE     (1U << 2)


__attribute__ ((format(printf, 2, 3)))
static int null_log(int type __attribute__((unused)), const char *fmt __attribute__((unused)), ...)
{
	return 0;
}

static int validate_context(char **ctxp)
{
	assert(strcmp(*ctxp, "<<none>>") != 0);

	if (*ctxp[0] == '\0') {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

static int write_full(int fd, const void *data, size_t size)
{
	ssize_t rc;
	const unsigned char *p = data;

	while (size > 0) {
		rc = write(fd, p, size);
		if (rc == -1) {
			if (errno == EINTR)
				continue;

			return -1;
		}

		p += rc;
		size -= rc;
	}

	return 0;
}

static FILE* convert_data(const uint8_t *data, size_t size)
{
	FILE* stream;
	int fd, rc;

	fd = memfd_create(MEMFD_FILE_NAME, MFD_CLOEXEC);
	if (fd == -1)
		return NULL;

	rc = write_full(fd, data, size);
	if (rc == -1) {
		close(fd);
		return NULL;
	}

	stream = fdopen(fd, "r");
	if (!stream) {
		close(fd);
		return NULL;
	}

	rc = fseek(stream, 0L, SEEK_SET);
	if (rc == -1) {
		fclose(stream);
		return NULL;
	}

	return stream;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct selabel_handle rec;
	struct saved_data sdata = {};
	struct spec_node *root = NULL;
	FILE* fp = NULL;
	struct lookup_result *result = NULL;
	uint8_t control;
	uint8_t *fcontext_data = NULL;
	char *key = NULL;
	size_t fcontext_data_len, key_len;
	bool partial, find_all;
	mode_t mode;
	int rc;

	/*
	 * Treat first byte as control byte, whether to use partial mode, find all matches or mode to lookup
	 */
	if (size == 0)
		return 0;

	control = data[0];
	data++;
	size--;

	if (control & ~(CTRL_PARTIAL | CTRL_FIND_ALL | CTRL_MODE))
		return 0;

	partial  = control & CTRL_PARTIAL;
	find_all = control & CTRL_FIND_ALL;
	/* S_IFSOCK has the highest integer value */
	mode     = (control & CTRL_MODE) ? S_IFSOCK : 0;


	/*
	 * Split the fuzzer input into two pieces: the textual fcontext definition and the lookup key
	 */
	const unsigned char separator[4] = { 0xde, 0xad, 0xbe, 0xef };
	const uint8_t *sep = memmem(data, size, separator, 4);
	if (!sep || sep == data)
		return 0;

	fcontext_data_len = sep - data;
	fcontext_data = malloc(fcontext_data_len);
	if (!fcontext_data)
		goto cleanup;

	memcpy(fcontext_data, data, fcontext_data_len);

	key_len = size - fcontext_data_len - 4;
	key = malloc(key_len + 1);
	if (!key)
		goto cleanup;

	memcpy(key, sep + 4, key_len);
	key[key_len] = '\0';


	/*
	 * Mock selabel handle
	 */
	rec = (struct selabel_handle) {
		.backend = SELABEL_CTX_FILE,
		.validating = 1,
		.data = &sdata,
	};

	selinux_set_callback(SELINUX_CB_LOG, (union selinux_callback) { .func_log = &null_log });
	/* validate to pre-compile regular expressions */
	selinux_set_callback(SELINUX_CB_VALIDATE, (union selinux_callback) { .func_validate = &validate_context });

	root = calloc(1, sizeof(*root));
	if (!root)
		goto cleanup;

	sdata.root = root;

	fp = convert_data(fcontext_data, fcontext_data_len);
	if (!fp)
		goto cleanup;

	errno = 0;
	rc = process_text_file(fp, /*prefix=*/ NULL, &rec, MEMFD_FILE_NAME, 0);
	if (rc) {
		assert(errno != 0);
		goto cleanup;
	}

	sort_specs(&sdata);

	assert(cmp(&rec, &rec) == SELABEL_EQUAL);

	errno = 0;
	result = lookup_all(&rec, key, mode, partial, find_all, NULL);

	if (!result)
		assert(errno != 0);

	for (const struct lookup_result *res = result; res; res = res->next) {
		assert(res->regex_str);
		assert(res->regex_str[0] != '\0');
		assert(res->lr->ctx_raw);
		assert(res->lr->ctx_raw[0] != '\0');
		assert(strcmp(res->lr->ctx_raw, "<<none>>") != 0);
		assert(!res->lr->ctx_trans);
		assert(res->lr->validated);
		assert(res->prefix_len <= strlen(res->regex_str));
	}


cleanup:
	free_lookup_result(result);
	if (fp)
		fclose(fp);
	if (sdata.root) {
		free_spec_node(sdata.root);
		free(sdata.root);
	}

	{
		struct mmap_area *area, *last_area;

		area = sdata.mmap_areas;
		while (area) {
			rc = munmap(area->addr, area->len);
			assert(rc == 0);
			last_area = area;
			area = area->next;
			free(last_area);
		}
	}

	free(key);
	free(fcontext_data);

	/* Non-zero return values are reserved for future use. */
	return 0;
}
