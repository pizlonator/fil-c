
#include "fdiskP.h"
#include "pathnames.h"
#include "canonicalize.h"

#include <ctype.h>

/**
 * SECTION: utils
 * @title: Utils
 * @short_description: misc fdisk functions
 */

static int read_from_device(struct fdisk_context *cxt,
		unsigned char *buf,
		uintmax_t start, size_t size)
{
	ssize_t r;

	assert(cxt);

	DBG(CXT, ul_debugobj(cxt, "reading: offset=%ju, size=%zu",
				start, size));

	r = lseek(cxt->dev_fd, start, SEEK_SET);
	if (r == -1)
	{
		DBG(CXT, ul_debugobj(cxt, "failed to seek to offset %ju: %m", start));
		return -errno;
	}

	errno = 0;
	r = read(cxt->dev_fd, buf, size);
	if (r < 0 || (size_t)r != size) {
		if (!errno)
			errno = EINVAL;	/* probably too small file/device */
		DBG(CXT, ul_debugobj(cxt, "failed to read %zu from offset %ju: %m",
				size, start));
		return -errno;
	}

	return 0;
}


/*
 * Zeros in-memory first sector buffer
 */
int fdisk_init_firstsector_buffer(struct fdisk_context *cxt,
				  unsigned int protect_off,
				  unsigned int protect_size)
{
	if (!cxt)
		return -EINVAL;

	assert(protect_off + protect_size <= cxt->sector_size);

	if (!cxt->firstsector || cxt->firstsector_bufsz != cxt->sector_size) {
		/* Let's allocate a new buffer if no allocated yet, or the
		 * current buffer has incorrect size */
		if (!cxt->parent || cxt->parent->firstsector != cxt->firstsector)
			free(cxt->firstsector);

		DBG(CXT, ul_debugobj(cxt, "initialize in-memory first sector "
				"buffer [sector_size=%lu]", cxt->sector_size));
		cxt->firstsector = calloc(1, cxt->sector_size);
		if (!cxt->firstsector)
			return -ENOMEM;

		cxt->firstsector_bufsz = cxt->sector_size;
		return 0;
	}

	DBG(CXT, ul_debugobj(cxt, "zeroize in-memory first sector buffer"));
	memset(cxt->firstsector, 0, cxt->firstsector_bufsz);

	if (protect_size) {
		/*
		 * It would be possible to reuse data from cxt->firstsector
		 * (call memset() for non-protected area only) and avoid one
		 * read() from the device, but it seems like a too fragile
		 * solution as we have no clue about stuff in the buffer --
		 * maybe it was already modified. Let's re-read from the device
		 * to be sure.			-- kzak 13-Apr-2015
		 */
		DBG(CXT, ul_debugobj(cxt, "first sector protection enabled -- re-reading"));
		read_from_device(cxt, cxt->firstsector, protect_off, protect_size);
	}
	return 0;
}

int fdisk_read_firstsector(struct fdisk_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->sector_size);

	rc = fdisk_init_firstsector_buffer(cxt, 0, 0);
	if (rc)
		return rc;

	assert(cxt->sector_size == cxt->firstsector_bufsz);


	return  read_from_device(cxt, cxt->firstsector, 0, cxt->sector_size);
}

/**
 * fdisk_partname:
 * @dev: device name
 * @partno: partition name
 *
 * Return: allocated buffer with partition name, use free() to deallocate.
 */
char *fdisk_partname(const char *dev, size_t partno)
{
	char *res = NULL;
	const char *p = "";
	char *dev_mapped = NULL;
	int w = 0;

	if (!dev || !*dev) {
		if (asprintf(&res, "%zd", partno) > 0)
			return res;
		return NULL;
	}

	/* It is impossible to predict /dev/dm-N partition names. */
	if (strncmp(dev, "/dev/dm-", sizeof("/dev/dm-") - 1) == 0) {
		dev_mapped = canonicalize_dm_name (dev + 5);
		if (dev_mapped)
			dev = dev_mapped;
	}

	w = strlen(dev);
	if (isdigit(dev[w - 1]))
#ifdef __GNU__
		p = "s";
#else
		p = "p";
#endif

	/* devfs kludge - note: fdisk partition names are not supposed
	   to equal kernel names, so there is no reason to do this */
	if (endswith(dev, "disc")) {
		w -= 4;
		p = "part";
	}

	/* udev names partitions by appending -partN
	   e.g. ata-SAMSUNG_SV8004H_0357J1FT712448-part1
	   multipath-tools kpartx.rules also append -partN */
	if ((strncmp(dev, _PATH_DEV_BYID, sizeof(_PATH_DEV_BYID) - 1) == 0) ||
	     strncmp(dev, _PATH_DEV_BYPATH, sizeof(_PATH_DEV_BYPATH) - 1) == 0 ||
	     strncmp(dev, "/dev/mapper", sizeof("/dev/mapper") - 1) == 0) {

		/* check for <name><partno>, e.g. mpatha1 */
		if (asprintf(&res, "%.*s%zu", w, dev, partno) <= 0)
			res = NULL;
		if (res && access(res, F_OK) == 0)
			goto done;

		free(res);

		/* check for partition separator "p" */
		if (asprintf(&res, "%.*sp%zu", w, dev, partno) <= 0)
			res = NULL;
		if (res && access(res, F_OK) == 0)
			goto done;

		free(res);

		/* otherwise, default to "-path" */
		p = "-part";
	}

	if (asprintf(&res, "%.*s%s%zu", w, dev, p, partno) <= 0)
		res = NULL;
done:
	free(dev_mapped);
	return res;
}

#ifdef TEST_PROGRAM
struct fdisk_label *fdisk_new_dos_label(struct fdisk_context *cxt __attribute__((unused))) { return NULL; }
struct fdisk_label *fdisk_new_bsd_label(struct fdisk_context *cxt __attribute__((unused))) { return NULL; }

static int test_partnames(struct fdisk_test *ts __attribute__((unused)),
			  int argc, char *argv[])
{
	if (argc != 2)
		return -1;

	size_t i;
	const char *disk = argv[1];

	for (i = 0; i < 5; i++) {
		char *p = fdisk_partname(disk, i + 1);
		if (p)
			printf("%zu: '%s'\n", i + 1, p);
		free(p);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct fdisk_test tss[] = {
		{ "--partnames",  test_partnames,  "<diskname>" },
		{ NULL }
	};

	return fdisk_run_test(tss, argc, argv);
}

#endif
