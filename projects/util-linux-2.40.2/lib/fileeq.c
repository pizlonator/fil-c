/*
 * compare files content
 *
 * The goal is to minimize number of data we need to read from the files and be
 * ready to compare large set of files, it means reuse the previous data if
 * possible. It never read entire file if not necessary.
 *
 * The another goal is to minimize number of open files (imagine "hardlink /"),
 * the code can open only two files and reopen the file next time if
 * necessary.
 *
 * This code supports multiple comparison methods. The very basic step which is
 * generic for all methods is to read and compare an "intro" (a few bytes from
 * the begging of the file). This intro buffer is always cached in 'struct
 * ul_fileeq_data', this intro buffer is addresses as block=0. This primitive
 * thing can reduce a lot ...
 *
 * The next steps depend on selected method:
 *
 *  * memcmp method: always read data to userspace, nothing is cached, compare
 *  directly files content; fast for small sets of the small files.
 *
 *  * Linux crypto API: zero-copy method based on sendfile(), data blocks are
 *  send to the kernel hash functions (sha1, ...), and only hash digest is read
 *  and cached in usersapce. Fast for large set of (large) files.
 *
 *
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com> [October 2021]
 */

#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Linux crypto */
#ifdef HAVE_LINUX_IF_ALG_H
# include <sys/socket.h>
# include <linux/if_alg.h>
# include <sys/param.h>
# include <sys/sendfile.h>
#endif

#include "c.h"
#include "all-io.h"
#include "fileeq.h"
#include "debug.h"

static UL_DEBUG_DEFINE_MASK(ulfileeq);
UL_DEBUG_DEFINE_MASKNAMES(ulfileeq) = UL_DEBUG_EMPTY_MASKNAMES;

#define ULFILEEQ_DEBUG_INIT	(1 << 1)
#define ULFILEEQ_DEBUG_CRYPTO	(1 << 2)
#define ULFILEEQ_DEBUG_DATA	(1 << 3)
#define ULFILEEQ_DEBUG_EQ	(1 << 4)

#define DBG(m, x)       __UL_DBG(ulfileeq, ULFILEEQ_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(ulfileeq, ULFILEEQ_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(ulfileeq)
#include "debugobj.h"

static void ul_fileeq_init_debug(void)
{
	if (ulfileeq_debug_mask)
		return;
	__UL_INIT_DEBUG_FROM_ENV(ulfileeq, ULFILEEQ_DEBUG_, 0, ULFILEEQ_DEBUG);
}

enum {
	UL_FILEEQ_MEMCMP,
	UL_FILEEQ_SHA1,
	UL_FILEEQ_SHA256,
	UL_FILEEQ_CRC32
};

struct ul_fileeq_method {
	const char *name;	/* name used by applications */
	const char *kname;	/* name used by kernel crypto */
	int id;
	short digsiz;
};

static const struct ul_fileeq_method ul_eq_methods[] = {
	[UL_FILEEQ_MEMCMP] = {
		.id = UL_FILEEQ_MEMCMP, .name = "memcmp"
	},
#ifdef USE_FILEEQ_CRYPTOAPI
	[UL_FILEEQ_SHA1] = {
		.id = UL_FILEEQ_SHA1, .name = "sha1",
		.digsiz = 20, .kname = "sha1"
	},
	[UL_FILEEQ_SHA256] = {
		.id = UL_FILEEQ_SHA256, .name = "sha256",
		.digsiz = 32, .kname = "sha256"
	},

	[UL_FILEEQ_CRC32] = {
		.id = UL_FILEEQ_CRC32, .name = "crc32",
		.digsiz = 4, .kname = "crc32c"
	}
#endif
};

#ifdef USE_FILEEQ_CRYPTOAPI
static void deinit_crypto_api(struct ul_fileeq *eq)
{
	if (!eq)
		return;

	DBG(CRYPTO, ul_debugobj(eq, "deinit"));

	if (eq->fd_cip >= 0)
		close(eq->fd_cip);
	if (eq->fd_api >= 0)
		close(eq->fd_api);

	eq->fd_cip = eq->fd_api = -1;

}

static int init_crypto_api(struct ul_fileeq *eq)
{
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type   = "hash",
	};

	assert(eq->method);
	assert(eq->method->kname);
	assert(eq->fd_api == -1);
	assert(eq->fd_cip == -1);

	DBG(CRYPTO, ul_debugobj(eq, "init [%s]", eq->method->kname));

	assert(sizeof(sa.salg_name) > strlen(eq->method->kname) + 1);
	memcpy(&sa.salg_name, eq->method->kname, strlen(eq->method->kname) + 1);

	if ((eq->fd_api = socket(AF_ALG, SOCK_SEQPACKET, 0)) < 0)
		goto fail;
	if (bind(eq->fd_api, (struct sockaddr *) &sa, sizeof(sa)) != 0)
		goto fail;
	if ((eq->fd_cip = accept(eq->fd_api, NULL, 0)) < 0)
		goto fail;
	return 0;
fail:
	deinit_crypto_api(eq);
	return -1;
}
#endif

int ul_fileeq_init(struct ul_fileeq *eq, const char *method)
{
	size_t i;

	ul_fileeq_init_debug();
	DBG(EQ, ul_debugobj(eq, "init [%s]", method));

	memset(eq, 0, sizeof(*eq));
	eq->fd_api = -1;
	eq->fd_cip = -1;

	for (i = 0; i < ARRAY_SIZE(ul_eq_methods); i++) {
		const struct ul_fileeq_method *m = &ul_eq_methods[i];

		if (strcmp(m->name, method) == 0) {
			eq->method = m;
			break;
		}
	}

	if (!eq->method)
		return -1;
#ifdef USE_FILEEQ_CRYPTOAPI
	if (eq->method->id != UL_FILEEQ_MEMCMP
	    && init_crypto_api(eq) != 0)
		return -1;
#endif
	return 0;
}

void ul_fileeq_deinit(struct ul_fileeq *eq)
{
	if (!eq)
		return;

	DBG(EQ, ul_debugobj(eq, "deinit"));
#ifdef USE_FILEEQ_CRYPTOAPI
	deinit_crypto_api(eq);
#endif
	free(eq->buf_a);
	free(eq->buf_b);
}

void ul_fileeq_data_close_file(struct ul_fileeq_data *data)
{
	assert(data);

	if (data->fd >= 0) {
		DBG(DATA, ul_debugobj(data, "close"));
		close(data->fd);
	}
	data->fd = -1;
}

void ul_fileeq_data_init(struct ul_fileeq_data *data)
{
	DBG(DATA, ul_debugobj(data, "init"));
	memset(data, 0, sizeof(*data));
	data->fd = -1;
}

void ul_fileeq_data_deinit(struct ul_fileeq_data *data)
{
	assert(data);

	DBG(DATA, ul_debugobj(data, "deinit"));
	free(data->blocks);
	data->blocks = NULL;
	data->nblocks = 0;
	data->maxblocks = 0;
	data->is_eof = 0;
	data->name = NULL;

	ul_fileeq_data_close_file(data);
}

int ul_fileeq_data_associated(struct ul_fileeq_data *data)
{
	return data->name != NULL;
}

void ul_fileeq_data_set_file(struct ul_fileeq_data *data, const char *name)
{
	assert(data);
	assert(name);

	DBG(DATA, ul_debugobj(data, "set file: %s", name));
	ul_fileeq_data_init(data);
	data->name = name;
}

size_t ul_fileeq_set_size(struct ul_fileeq *eq, uint64_t filesiz,
			 size_t readsiz, size_t memsiz)
{
	uint64_t nreads, maxdigs;
	size_t digsiz;

	assert(eq);

	eq->filesiz = filesiz;

	switch (eq->method->id) {
	case UL_FILEEQ_MEMCMP:
		/* align file size */
		filesiz = (filesiz + readsiz) / readsiz * readsiz;
		break;
	default:
		digsiz = eq->method->digsiz;
		if (readsiz < digsiz)
			readsiz = digsiz;
		/* align file size */
		filesiz = (filesiz + readsiz) / readsiz * readsiz;
		/* calculate limits */
		maxdigs = memsiz / digsiz;
		if (maxdigs == 0)
			maxdigs = 1;
		nreads = filesiz / readsiz;
		/* enlarge readsize for large files */
		if (nreads > maxdigs)
			readsiz = filesiz / maxdigs;
		break;
	}

	eq->readsiz = readsiz;
	eq->blocksmax = filesiz / readsiz;

	DBG(EQ, ul_debugobj(eq, "set sizes: filesiz=%ju, maxblocks=%" PRIu64 ", readsiz=%zu",
				eq->filesiz, eq->blocksmax, eq->readsiz));
	return eq->blocksmax;
}

static unsigned char *get_buffer(struct ul_fileeq *eq)
{
	if (!eq->buf_a)
		eq->buf_a = malloc(eq->readsiz);
	if (!eq->buf_b)
		eq->buf_b = malloc(eq->readsiz);

	if (!eq->buf_a || !eq->buf_b)
		return NULL;

	if (eq->buf_last == eq->buf_b)
		eq->buf_last = eq->buf_a;
	else
		eq->buf_last = eq->buf_b;

	return eq->buf_last;
}

#define get_cached_nblocks(_d) \
			((_d)->nblocks ? (_d)->nblocks - 1 : 0)

#define get_cached_offset(_e, _d) \
			((_d)->nblocks == 0 ? 0 : \
				sizeof((_d)->intro) \
				+ (get_cached_nblocks(_d) * (_e)->readsiz))


static int get_fd(struct ul_fileeq *eq, struct ul_fileeq_data *data, off_t *off)
{
	off_t o = get_cached_offset(eq, data);

	assert(eq);
	assert(data);


	if (data->fd < 0) {
		DBG(DATA, ul_debugobj(data, "open: %s", data->name));
		data->fd = open(data->name, O_RDONLY);
		if (data->fd < 0)
			return data->fd;

#if defined(POSIX_FADV_SEQUENTIAL) && defined(HAVE_POSIX_FADVISE)
	        ignore_result( posix_fadvise(data->fd, o, 0, POSIX_FADV_SEQUENTIAL) );
#endif
		if (o) {
			DBG(DATA, ul_debugobj(data, "lseek off=%ju", (uintmax_t) o));
			lseek(data->fd, o, SEEK_SET);
		}
	}

	if (off)
		*off = o;

	return data->fd;
}

static void memcmp_reset(struct ul_fileeq *eq, struct ul_fileeq_data *data)
{
	/* only intro[] is cached */
	if (data->nblocks)
		data->nblocks = 1;
	/* reset file possition */
	if (data->fd >= 0)
		lseek(data->fd, get_cached_offset(eq, data), SEEK_SET);
	data->is_eof = 0;
}

static ssize_t read_block(struct ul_fileeq *eq, struct ul_fileeq_data *data,
				size_t n, unsigned char **block)
{
	int fd;
	off_t off = 0;
	ssize_t rsz;

	if (data->is_eof || n > eq->blocksmax)
		return 0;

	fd = get_fd(eq, data, &off);
	if (fd < 0)
		return fd;

	DBG(DATA, ul_debugobj(data, " read block off=%ju", (uintmax_t) off));

	*block = get_buffer(eq);
	if (!*block)
		return -ENOMEM;

	rsz = read_all(data->fd, (char *) *block, eq->readsiz);
	if (rsz < 0) {
		DBG(DATA, ul_debugobj(data, "  read failed"));
		return rsz;
	}
	off += rsz;
	data->nblocks++;

	if (rsz == 0 || (uint64_t) off >= eq->filesiz) {
		data->is_eof = 1;
		ul_fileeq_data_close_file(data);
	}

	DBG(DATA, ul_debugobj(data, "  read sz=%zu", rsz));
	return rsz;
}

#ifdef USE_FILEEQ_CRYPTOAPI
static ssize_t get_digest(struct ul_fileeq *eq, struct ul_fileeq_data *data,
				size_t n, unsigned char **block)
{
	off_t off = 0;
	ssize_t rsz;
	size_t sz;
	int fd;

	if (n > eq->blocksmax)
		return 0;

	/* return already cached if alvalable */
	if (n < get_cached_nblocks(data)) {
		DBG(DATA, ul_debugobj(data, " digest cached"));
		assert(data->blocks);
		*block = data->blocks + (n * eq->method->digsiz);
		return eq->method->digsiz;
	}

	if (data->is_eof) {
		DBG(DATA, ul_debugobj(data, " file EOF"));
		return 0;
	}

	/* read new block */
	fd = get_fd(eq, data, &off);
	if (fd < 0)
		return fd;

	DBG(DATA, ul_debugobj(data, " read digest off=%ju", (uintmax_t) off));

	sz = eq->method->digsiz;

	if (!data->blocks) {
		DBG(DATA, ul_debugobj(data, "  alloc cache %" PRIu64, eq->blocksmax * sz));
		data->blocks = malloc(eq->blocksmax * sz);
		if (!data->blocks)
			return -ENOMEM;
	}

	assert(n <= eq->blocksmax);

	rsz = sendfile(eq->fd_cip, data->fd, NULL, eq->readsiz);
	DBG(DATA, ul_debugobj(data, "  sent %zu [%zu wanted] to cipher", rsz, eq->readsiz));

	if (rsz < 0)
		return rsz;

	off += rsz;

	/* get block digest (note 1st block is data->intro */
	*block = data->blocks + (n * eq->method->digsiz);
	rsz = read_all(eq->fd_cip, (char *) *block, sz);

	if (rsz > 0)
		data->nblocks++;
	if (rsz == 0 || (uint64_t) off >= eq->filesiz) {
		data->is_eof = 1;
		ul_fileeq_data_close_file(data);
	}
	DBG(DATA, ul_debugobj(data, "  get %zuB digest", rsz));
	return rsz;
}
#endif

static ssize_t get_intro(struct ul_fileeq *eq, struct ul_fileeq_data *data,
				unsigned char **block)
{
	if (data->nblocks == 0) {
		int fd = get_fd(eq, data, NULL);
		ssize_t rsz;

		if (fd < 0)
			return -1;
		rsz = read_all(fd, (char *) data->intro, sizeof(data->intro));
		DBG(DATA, ul_debugobj(data, " read %zu bytes intro", sizeof(data->intro)));
		if (rsz <= 0)
			return -1;
		data->nblocks = 1;
	}

	DBG(DATA, ul_debugobj(data, " return intro"));
	*block = data->intro;
	return sizeof(data->intro);
}

static ssize_t get_cmp_data(struct ul_fileeq *eq, struct ul_fileeq_data *data,
				size_t blockno, unsigned char **block)
{
	if (blockno == 0)
		return get_intro(eq, data, block);

	blockno--;

	switch (eq->method->id) {
	case UL_FILEEQ_MEMCMP:
		return read_block(eq, data, blockno, block);
	default:
		break;
	}
#ifdef USE_FILEEQ_CRYPTOAPI
	return get_digest(eq, data, blockno, block);
#else
	return -1;
#endif
}

#define CMP(a, b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

int ul_fileeq(struct ul_fileeq *eq,
	      struct ul_fileeq_data *a, struct ul_fileeq_data *b)
{
	int cmp;
	size_t n = 0;

	DBG(EQ, ul_debugobj(eq, "--> compare %s %s", a->name, b->name));

	if (eq->method->id == UL_FILEEQ_MEMCMP) {
		memcmp_reset(eq, a);
		memcmp_reset(eq, b);
	}

	do {
		unsigned char *da, *db;
		ssize_t ca, cb;

		DBG(EQ, ul_debugobj(eq, "compare block #%zu", n));

		ca = get_cmp_data(eq, a, n, &da);
		if (ca < 0)
			goto done;
		cb = get_cmp_data(eq, b, n, &db);
		if (cb < 0)
			goto done;
		if (ca != cb || ca == 0) {
			cmp = CMP(ca, cb);
			break;

		}
		cmp = memcmp(da, db, ca);
		DBG(EQ, ul_debugobj(eq, "#%zu=%s", n, cmp == 0 ? "match" : "not-match"));
		n++;
	} while (cmp == 0);

	if (cmp == 0) {
		if (!a->is_eof || !b->is_eof)
			goto done; /* filesize chnaged? */

		DBG(EQ, ul_debugobj(eq, "<-- MATCH"));
		return 1;
	}
done:
	DBG(EQ, ul_debugobj(eq, " <-- NOT-MATCH"));
	return 0;
}

#ifdef TEST_PROGRAM_FILEEQ
# include <getopt.h>
# include <err.h>

int main(int argc, char *argv[])
{
	struct ul_fileeq eq;
	struct ul_fileeq_data a, b, c;
	const char *method = "sha1";
	static const struct option longopts[] = {
		{ "method", required_argument, NULL, 'm' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};
	int ch, rc;
	const char *file_a = NULL, *file_b = NULL, *file_c = NULL;
	struct stat st_a, st_b, st_c;

	while ((ch = getopt_long(argc, argv, "m:", longopts, NULL)) != -1) {
		switch (ch) {
		case 'm':
			method = optarg;
			break;
		case 'h':
			printf("usage: %s [options] <file> <file>\n"
				" -m, --method <memcmp|sha1|crc32>    compare method\n",
				program_invocation_short_name);
			return EXIT_FAILURE;
		}
	}

	if (optind < argc)
		file_a = argv[optind++];
	if (optind < argc)
		file_b = argv[optind++];
	if (optind < argc)
		file_c = argv[optind++];

	if (!file_a || !file_b)
		errx(EXIT_FAILURE, "no files specified, see --help");

	if (stat(file_a, &st_a) != 0 || !S_ISREG(st_a.st_mode))
		errx(EXIT_FAILURE, "%s: wrong file", file_a);
	if (stat(file_b, &st_b) != 0 || !S_ISREG(st_b.st_mode))
		errx(EXIT_FAILURE, "%s: wrong file", file_b);
	if (file_c && (stat(file_c, &st_c) != 0 || !S_ISREG(st_c.st_mode)))
		errx(EXIT_FAILURE, "%s: wrong file", file_c);


	if (st_a.st_size != st_b.st_size ||
	    (file_c && st_a.st_size != st_c.st_size))
		errx(EXIT_FAILURE, "size of the files does not match");


	rc = ul_fileeq_init(&eq, method);
	if (rc != 0 && strcmp(method, "memcmp") != 0) {
		method = "memcmp";
		rc = ul_fileeq_init(&eq, method);
	}
	if (rc < 0)
		err(EXIT_FAILURE, "failed to initialize files comparior");

	ul_fileeq_data_set_file(&a, file_a);
	ul_fileeq_data_set_file(&b, file_b);

	/* 3rd is optional */
	if (file_c)
		ul_fileeq_data_set_file(&c, file_c);

	/*                     filesiz,      readsiz,   memsiz */
	ul_fileeq_set_size(&eq, st_a.st_size, 1024*1024, 4*1024);

	rc = ul_fileeq(&eq, &a, &b);

	printf("1st vs. 2nd: %s\n", rc == 1 ? "MATCH" : "NOT-MATCH");
	if (file_c) {
		rc = ul_fileeq(&eq, &a, &c);
		printf("1st vs. 3rd: %s\n", rc == 1 ? "MATCH" : "NOT-MATCH");

		rc = ul_fileeq(&eq, &b, &c);
		printf("2st vs. 3rd: %s\n", rc == 1 ? "MATCH" : "NOT-MATCH");
	}

	ul_fileeq_data_deinit(&a);
	ul_fileeq_data_deinit(&b);

	if (file_c)
		ul_fileeq_data_deinit(&c);

	ul_fileeq_deinit(&eq);
	return EXIT_FAILURE;
}
#endif /* TEST_PROGRAM_FILEEQ */
