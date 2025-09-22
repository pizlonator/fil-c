#include "c.h"
#include "pathnames.h"
#include "xalloc.h"
#include "nls.h"

#include "lsblk.h"

static struct libmnt_table *mtab, *swaps;
static struct libmnt_cache *mntcache;

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error at line %d -- ignored"), filename, line);
	return 1;
}

static struct libmnt_fs *get_active_swap(const char *filename)
{
	assert(filename);

	if (!swaps) {
		swaps = mnt_new_table();
		if (!swaps)
			return 0;
		if (!mntcache)
			mntcache = mnt_new_cache();

		mnt_table_set_parser_errcb(swaps, table_parser_errcb);
		mnt_table_set_cache(swaps, mntcache);

		if (!lsblk->sysroot)
			mnt_table_parse_swaps(swaps, NULL);
		else {
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s" _PATH_PROC_SWAPS, lsblk->sysroot);
			mnt_table_parse_swaps(swaps, buf);
		}
	}

	return mnt_table_find_srcpath(swaps, filename, MNT_ITER_BACKWARD);
}

void lsblk_device_free_filesystems(struct lsblk_device *dev)
{
	if (!dev)
		return;

	free(dev->fss);

	dev->fss = NULL;
	dev->nfss = 0;
	dev->is_mounted = 0;
	dev->is_swap = 0;
}

static void add_filesystem(struct lsblk_device *dev, struct libmnt_fs *fs)
{
	assert(dev);
	assert(fs);

	dev->fss = xreallocarray(dev->fss, dev->nfss + 1, sizeof(struct libmnt_fs *));
	dev->fss[dev->nfss] = fs;
	dev->nfss++;
	dev->is_mounted = 1;
}

struct libmnt_fs **lsblk_device_get_filesystems(struct lsblk_device *dev, size_t *n)
{
	struct libmnt_fs *fs;
	struct libmnt_iter *itr = NULL;
	dev_t devno;

	assert(dev);
	assert(dev->filename);

	if (dev->is_mounted)
		goto done;

	lsblk_device_free_filesystems(dev);	/* reset */

	if (!mtab) {
		mtab = mnt_new_table();
		if (!mtab)
			return NULL;
		if (!mntcache)
			mntcache = mnt_new_cache();

		mnt_table_set_parser_errcb(mtab, table_parser_errcb);
		mnt_table_set_cache(mtab, mntcache);

		if (!lsblk->sysroot)
			mnt_table_parse_mtab(mtab, NULL);
		else {
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s" _PATH_PROC_MOUNTINFO, lsblk->sysroot);
			mnt_table_parse_mtab(mtab, buf);
		}
	}

	devno = makedev(dev->maj, dev->min);

	/* All mounpoint where is used devno or device name
	 */
	itr = mnt_new_iter(MNT_ITER_BACKWARD);
	while (mnt_table_next_fs(mtab, itr, &fs) == 0) {
		if (mnt_fs_get_devno(fs) != devno &&
		    !mnt_fs_streq_srcpath(fs, dev->filename))
			continue;
		add_filesystem(dev, fs);
	}

	/* Try mnt_table_find_srcpath() which also canonicalizes patches, etc.
	 */
	if (!dev->nfss) {
		fs = get_active_swap(dev->filename);
		if (!fs) {
			fs = mnt_table_find_srcpath(mtab, dev->filename, MNT_ITER_BACKWARD);
			if (fs)
				dev->is_swap = 1;
		}
		if (fs)
			add_filesystem(dev, fs);
	}

done:
	mnt_free_iter(itr);
	if (n)
		*n = dev->nfss;
	return dev->fss;
}

/* Returns mounpoint where the device is mounted. If the device is used for
 * more filesystems (subvolumes, ...) than returns the "best" one.
 */
const char *lsblk_device_get_mountpoint(struct lsblk_device *dev)
{
	struct libmnt_fs *fs = NULL;
	const char *root;

	lsblk_device_get_filesystems(dev, NULL);
	if (!dev->nfss)
		return NULL;

	/* lsblk_device_get_filesystems() scans mountinfo/swaps in backward
	 * order. It means the first in fss[] is the last mounted FS. Let's
	 * keep it as default */
	fs = dev->fss[0];
	root = mnt_fs_get_root(fs);

	if (root && strcmp(root, "/") != 0) {
		/* FS is subvolume (or subdirectory bind-mount). Try to get
		 * FS with "/" root */
		size_t i;

		for (i = 1; i < dev->nfss; i++) {
			root = mnt_fs_get_root(dev->fss[i]);
			if (!root || strcmp(root, "/") == 0) {
				fs = dev->fss[i];
				break;
			}
		}
	}
	if (mnt_fs_is_swaparea(fs))
		return "[SWAP]";
	return mnt_fs_get_target(fs);
}

void lsblk_mnt_init(void)
{
	mnt_init_debug(0);
}

void lsblk_mnt_deinit(void)
{
	mnt_unref_table(mtab);
	mnt_unref_table(swaps);
	mnt_unref_cache(mntcache);
}
