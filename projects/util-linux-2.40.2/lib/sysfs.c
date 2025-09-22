/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 */
#include <ctype.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"
#include "pathnames.h"
#include "sysfs.h"
#include "fileutils.h"
#include "all-io.h"
#include "debug.h"
#include "strutils.h"
#include "buffer.h"

static void sysfs_blkdev_deinit_path(struct path_cxt *pc);
static int  sysfs_blkdev_enoent_redirect(struct path_cxt *pc, const char *path, int *dirfd);

/*
 * Debug stuff (based on include/debug.h)
 */
static UL_DEBUG_DEFINE_MASK(ulsysfs);
UL_DEBUG_DEFINE_MASKNAMES(ulsysfs) = UL_DEBUG_EMPTY_MASKNAMES;

#define ULSYSFS_DEBUG_INIT	(1 << 1)
#define ULSYSFS_DEBUG_CXT	(1 << 2)

#define DBG(m, x)       __UL_DBG(ulsysfs, ULSYSFS_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(ulsysfs, ULSYSFS_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(ulsysfs)
#include "debugobj.h"

void ul_sysfs_init_debug(void)
{
	if (ulsysfs_debug_mask)
		return;
	__UL_INIT_DEBUG_FROM_ENV(ulsysfs, ULSYSFS_DEBUG_, 0, ULSYSFS_DEBUG);
}

struct path_cxt *ul_new_sysfs_path(dev_t devno, struct path_cxt *parent, const char *prefix)
{
	struct path_cxt *pc = ul_new_path(NULL);

	if (!pc)
		return NULL;
	if (prefix)
		ul_path_set_prefix(pc, prefix);

	if (sysfs_blkdev_init_path(pc, devno, parent) != 0) {
		ul_unref_path(pc);
		return NULL;
	}

	DBG(CXT, ul_debugobj(pc, "alloc"));
	return pc;
}

/*
 * sysfs_blkdev_* is sysfs extension to ul_path_* API for block devices.
 *
 * The function is possible to call in loop and without sysfs_blkdev_deinit_path().
 * The sysfs_blkdev_deinit_path() is automatically called by ul_unref_path().
 *
 */
int sysfs_blkdev_init_path(struct path_cxt *pc, dev_t devno, struct path_cxt *parent)
{
	struct sysfs_blkdev *blk;
	int rc;
	char buf[sizeof(_PATH_SYS_DEVBLOCK)
		 + sizeof(stringify_value(UINT32_MAX)) * 2
		 + 3];

	/* define path to devno stuff */
	snprintf(buf, sizeof(buf), _PATH_SYS_DEVBLOCK "/%d:%d", major(devno), minor(devno));
	rc = ul_path_set_dir(pc, buf);
	if (rc)
		return rc;

	/* make sure path exists */
	rc = ul_path_get_dirfd(pc);
	if (rc < 0)
		return rc;

	/* initialize sysfs blkdev specific stuff */
	blk = ul_path_get_dialect(pc);
	if (!blk) {
		DBG(CXT, ul_debugobj(pc, "alloc new sysfs handler"));
		blk = calloc(1, sizeof(struct sysfs_blkdev));
		if (!blk)
			return -ENOMEM;

		ul_path_set_dialect(pc, blk, sysfs_blkdev_deinit_path);
		ul_path_set_enoent_redirect(pc, sysfs_blkdev_enoent_redirect);
	}

	DBG(CXT, ul_debugobj(pc, "init sysfs stuff"));

	blk->devno = devno;
	sysfs_blkdev_set_parent(pc, parent);

	return 0;
}

static void sysfs_blkdev_deinit_path(struct path_cxt *pc)
{
	struct sysfs_blkdev *blk;

	if (!pc)
		return;

	DBG(CXT, ul_debugobj(pc, "deinit"));

	blk = ul_path_get_dialect(pc);
	if (!blk)
		return;

	ul_unref_path(blk->parent);
	free(blk);

	ul_path_set_dialect(pc, NULL, NULL);
}

int sysfs_blkdev_set_parent(struct path_cxt *pc, struct path_cxt *parent)
{
	struct sysfs_blkdev *blk = ul_path_get_dialect(pc);

	if (!pc || !blk)
		return -EINVAL;

	if (blk->parent) {
		ul_unref_path(blk->parent);
		blk->parent = NULL;
	}

	if (parent) {
		ul_ref_path(parent);
		blk->parent = parent;
	} else
		blk->parent = NULL;

	DBG(CXT, ul_debugobj(pc, "new parent"));
	return 0;
}

struct path_cxt *sysfs_blkdev_get_parent(struct path_cxt *pc)
{
	struct sysfs_blkdev *blk = ul_path_get_dialect(pc);
	return blk ? blk->parent : NULL;
}

/*
 * Redirects ENOENT errors to the parent.
 * For example
 *
 *	/sys/dev/block/8:1/queue/logical_block_size redirects to
 *	/sys/dev/block/8:0/queue/logical_block_size
 */
static int sysfs_blkdev_enoent_redirect(struct path_cxt *pc, const char *path, int *dirfd)
{
	struct sysfs_blkdev *blk = ul_path_get_dialect(pc);

	if (blk && blk->parent && path) {
		*dirfd = ul_path_get_dirfd(blk->parent);
		if (*dirfd >= 0) {
			DBG(CXT, ul_debugobj(pc, "%s redirected to parent", path));
			return 0;
		}
	}
	return 1;	/* no redirect */
}

char *sysfs_blkdev_get_name(struct path_cxt *pc, char *buf, size_t bufsiz)
{
	char link[PATH_MAX];
	char *name;
	ssize_t	sz;

        /* read /sys/dev/block/<maj:min> link */
	sz = ul_path_readlink(pc, link, sizeof(link), NULL);
	if (sz < 0)
		return NULL;

	name = strrchr(link, '/');
	if (!name)
		return NULL;

	name++;
	sz = strlen(name);
	if ((size_t) sz + 1 > bufsiz)
		return NULL;

	memcpy(buf, name, sz + 1);
	sysfs_devname_sys_to_dev(buf);
	return buf;
}

int sysfs_blkdev_is_partition_dirent(DIR *dir, struct dirent *d, const char *parent_name)
{
	char path[NAME_MAX + 6 + 1];

#ifdef _DIRENT_HAVE_D_TYPE
	if (d->d_type != DT_DIR &&
	    d->d_type != DT_LNK &&
	    d->d_type != DT_UNKNOWN)
		return 0;
#endif
	size_t len = 0;

	if (parent_name) {
		const char *p = parent_name;

		/* /dev/sda --> "sda" */
		if (*parent_name == '/') {
			p = strrchr(parent_name, '/');
			if (!p)
				return 0;
			p++;
		}

		len = strlen(p);
		if ((strlen(d->d_name) <= len) || (strncmp(p, d->d_name, len) != 0))
			len = 0;
	}

	if (len > 0) {
		/* partitions subdir name is
		 *	"<parent>[:digit:]" or "<parent>p[:digit:]"
		 */
		return ((*(d->d_name + len) == 'p' && isdigit(*(d->d_name + len + 1)))
			|| isdigit(*(d->d_name + len)));
	}

	/* Cannot use /partition file, not supported on old sysfs */
	snprintf(path, sizeof(path), "%s/start", d->d_name);

	return faccessat(dirfd(dir), path, R_OK, 0) == 0;
}

int sysfs_blkdev_count_partitions(struct path_cxt *pc, const char *devname)
{
	DIR *dir;
	struct dirent *d;
	int r = 0;

	dir = ul_path_opendir(pc, NULL);
	if (!dir)
		return 0;

	while ((d = xreaddir(dir))) {
		if (sysfs_blkdev_is_partition_dirent(dir, d, devname))
			r++;
	}

	closedir(dir);
	return r;
}

/*
 * Converts @partno (partition number) to devno of the partition.
 * The @pc handles wholedisk device.
 *
 * Note that this code does not expect any special format of the
 * partitions devnames.
 */
dev_t sysfs_blkdev_partno_to_devno(struct path_cxt *pc, int partno)
{
	DIR *dir;
	struct dirent *d;
	dev_t devno = 0;

	dir = ul_path_opendir(pc, NULL);
	if (!dir)
		return 0;

	while ((d = xreaddir(dir))) {
		int n;

		if (!sysfs_blkdev_is_partition_dirent(dir, d, NULL))
			continue;

		if (ul_path_readf_s32(pc, &n, "%s/partition", d->d_name))
			continue;

		if (n == partno) {
			if (ul_path_readf_majmin(pc, &devno, "%s/dev", d->d_name) == 0)
				break;
		}
	}

	closedir(dir);
	DBG(CXT, ul_debugobj(pc, "partno (%d) -> devno (%d)", (int) partno, (int) devno));
	return devno;
}


/*
 * Returns slave name if there is only one slave, otherwise returns NULL.
 * The result should be deallocated by free().
 */
char *sysfs_blkdev_get_slave(struct path_cxt *pc)
{
	DIR *dir;
	struct dirent *d;
	char *name = NULL;

	dir = ul_path_opendir(pc, "slaves");
	if (!dir)
		return NULL;

	while ((d = xreaddir(dir))) {
		if (name)
			goto err;	/* more slaves */
		name = strdup(d->d_name);
	}

	closedir(dir);
	return name;
err:
	free(name);
	closedir(dir);
	return NULL;
}


#define SUBSYSTEM_LINKNAME	"/subsystem"

/*
 * For example:
 *
 * chain: /sys/dev/block/../../devices/pci0000:00/0000:00:1a.0/usb1/1-1/1-1.2/ \
 *                           1-1.2:1.0/host65/target65:0:0/65:0:0:0/block/sdb
 *
 * The function check if <chain>/subsystem symlink exists, if yes then returns
 * basename of the readlink result, and remove the last subdirectory from the
 * <chain> path.
 */
static char *get_subsystem(char *chain, char *buf, size_t bufsz)
{
	size_t len;
	char *p;

	if (!chain || !*chain)
		return NULL;

	len = strlen(chain);
	if (len + sizeof(SUBSYSTEM_LINKNAME) > PATH_MAX)
		return NULL;

	do {
		ssize_t sz;

		/* append "/subsystem" to the path */
		memcpy(chain + len, SUBSYSTEM_LINKNAME, sizeof(SUBSYSTEM_LINKNAME));

		/* try if subsystem symlink exists */
		sz = readlink(chain, buf, bufsz - 1);

		/* remove last subsystem from chain */
		chain[len] = '\0';
		p = strrchr(chain, '/');
		if (p) {
			*p = '\0';
			len = p - chain;
		}

		if (sz > 0) {
			/* we found symlink to subsystem, return basename */
			buf[sz] = '\0';
			return basename(buf);
		}

	} while (p);

	return NULL;
}

/*
 * Returns complete path to the device, the path contains all subsystems
 * used for the device.
 */
char *sysfs_blkdev_get_devchain(struct path_cxt *pc, char *buf, size_t bufsz)
{
	/* read /sys/dev/block/<maj>:<min> symlink */
	ssize_t ssz;
	size_t sz = 0;
	struct ul_buffer tmp = UL_INIT_BUFFER;
	const char *p;
	char *res = NULL;

	ssz = ul_path_readlink(pc, buf, bufsz, NULL);
	if (ssz <= 0)
		return NULL;

	if ((p = ul_path_get_prefix(pc)))
		ul_buffer_append_string(&tmp, p);

	ul_buffer_append_string(&tmp, _PATH_SYS_DEVBLOCK "/");
	ul_buffer_append_data(&tmp, buf, ssz);

	p = ul_buffer_get_string(&tmp, &sz, NULL);
	if (p && sz <= bufsz) {
		memcpy(buf, p, sz);
		res = buf;
	}
	ul_buffer_free_data(&tmp);
	return res;
}

/*
 * The @subsys returns the next subsystem in the chain. Function modifies
 * @devchain string.
 *
 * Returns: 0 in success, <0 on error, 1 on end of chain
 */
int sysfs_blkdev_next_subsystem(struct path_cxt *pc __attribute__((unused)),
			 char *devchain, char **subsys)
{
	char subbuf[PATH_MAX];
	char *sub;

	if (!subsys || !devchain)
		return -EINVAL;

	*subsys = NULL;

	while ((sub = get_subsystem(devchain, subbuf, sizeof(subbuf)))) {
		*subsys = strdup(sub);
		if (!*subsys)
			return -ENOMEM;
		return 0;
	}

	return 1;
}

#define REMOVABLE_FILENAME	"/removable"

/*
 * For example:
 *
 * chain: /sys/dev/block/../../devices/pci0000:00/0000:00:1a.0/usb1/1-1/1-1.2/ \
 *                           1-1.2:1.0/host65/target65:0:0/65:0:0:0/block/sdb
 */
static int sysfs_devchain_is_removable(char *chain)
{
	size_t len;
	char buf[20];
	char *p;

	if (!chain || !*chain)
		return 0;

	len = strlen(chain);
	if (len + sizeof(REMOVABLE_FILENAME) > PATH_MAX)
		return 0;

	do {
		int fd, rc;

		/* append "/removable" to the path */
		memcpy(chain + len, REMOVABLE_FILENAME, sizeof(REMOVABLE_FILENAME));

		/* root of device hierarchy */
		if (strcmp(chain, "/sys/dev/block" REMOVABLE_FILENAME) == 0)
			break;

		/* try to read it */
		fd = open(chain, O_RDONLY);
		if (fd != -1) {
			rc = read_all(fd, buf, sizeof(buf));
			close(fd);

			if (rc > 0) {
				if (strncmp(buf, "fixed", min(rc, 5)) == 0) {
					return 0;
				} else if (strncmp(buf, "removable", min(rc, 9)) == 0) {
					return 1;
				}
			}
		}

		/* remove last subsystem from chain */
		chain[len] = '\0';
		p = strrchr(chain, '/');
		if (p) {
			*p = '\0';
			len = p - chain;
		}

	} while (p);

	return 0;
}

int sysfs_blkdev_is_hotpluggable(struct path_cxt *pc)
{
	char buf[PATH_MAX], *chain;

	chain = sysfs_blkdev_get_devchain(pc, buf, sizeof(buf));
	return sysfs_devchain_is_removable(chain);
}

int sysfs_blkdev_is_removable(struct path_cxt *pc)
{
	int rc = 0;

	// FIXME usb is not actually removable

	/* check /sys/dev/block/<maj>:<min>/removable attribute */
	if (ul_path_read_s32(pc, &rc, "removable") == 0)
		return rc;

	return 0;
}

static int get_dm_wholedisk(struct path_cxt *pc, char *diskname,
                size_t len, dev_t *diskdevno)
{
    int rc = 0;
    char *name;

    /* Note, sysfs_blkdev_get_slave() returns the first slave only,
     * if there is more slaves, then return NULL
     */
    name = sysfs_blkdev_get_slave(pc);
    if (!name)
        return -1;

    if (diskname && len)
        xstrncpy(diskname, name, len);

    if (diskdevno) {
        *diskdevno = __sysfs_devname_to_devno(ul_path_get_prefix(pc), name, NULL);
        if (!*diskdevno)
            rc = -1;
    }

    free(name);
    return rc;
}

/*
 * Returns by @diskdevno whole disk device devno and (optionally) by
 * @diskname the whole disk device name.
 */
int sysfs_blkdev_get_wholedisk(	struct path_cxt *pc,
				char *diskname,
				size_t len,
				dev_t *diskdevno)
{
    int is_part = 0;

    if (!pc)
        return -1;

    is_part = ul_path_access(pc, F_OK, "partition") == 0;
    if (!is_part) {
        /*
         * Extra case for partitions mapped by device-mapper.
         *
         * All regular partitions (added by BLKPG ioctl or kernel PT
         * parser) have the /sys/.../partition file. The partitions
         * mapped by DM don't have such file, but they have "part"
         * prefix in DM UUID.
         */
        char *uuid = NULL, *tmp, *prefix;

	ul_path_read_string(pc, &uuid, "dm/uuid");
	tmp = uuid;
	prefix = uuid ? strsep(&tmp, "-") : NULL;

        if (prefix && strncasecmp(prefix, "part", 4) == 0)
            is_part = 1;
        free(uuid);

        if (is_part &&
            get_dm_wholedisk(pc, diskname, len, diskdevno) == 0)
            /*
             * partitioned device, mapped by DM
             */
            goto done;

        is_part = 0;
    }

    if (!is_part) {
        /*
         * unpartitioned device
         */
        if (diskname && !sysfs_blkdev_get_name(pc, diskname, len))
            goto err;
        if (diskdevno)
            *diskdevno = sysfs_blkdev_get_devno(pc);

    } else {
        /*
         * partitioned device
         *  - readlink /sys/dev/block/8:1   = ../../block/sda/sda1
         *  - dirname  ../../block/sda/sda1 = ../../block/sda
         *  - basename ../../block/sda      = sda
         */
        char linkpath[PATH_MAX];
        char *name;
	ssize_t	linklen;

	linklen = ul_path_readlink(pc, linkpath, sizeof(linkpath), NULL);
        if (linklen < 0)
            goto err;

        stripoff_last_component(linkpath);      /* dirname */
        name = stripoff_last_component(linkpath);   /* basename */
        if (!name)
            goto err;

	sysfs_devname_sys_to_dev(name);
        if (diskname && len)
            xstrncpy(diskname, name, len);

        if (diskdevno) {
            *diskdevno = __sysfs_devname_to_devno(ul_path_get_prefix(pc), name, NULL);
            if (!*diskdevno)
                goto err;
        }
    }

done:
    return 0;
err:
    return -1;
}

int sysfs_devno_to_wholedisk(dev_t devno, char *diskname,
		             size_t len, dev_t *diskdevno)
{
	struct path_cxt *pc;
	int rc = 0;

	if (!devno)
		return -EINVAL;
	pc = ul_new_sysfs_path(devno, NULL, NULL);
	if (!pc)
		return -ENOMEM;

	rc = sysfs_blkdev_get_wholedisk(pc, diskname, len, diskdevno);
	ul_unref_path(pc);
	return rc;
}

/*
 * Returns 1 if the device is private device mapper device. The @uuid
 * (if not NULL) returns DM device UUID, use free() to deallocate.
 */
int sysfs_devno_is_dm_private(dev_t devno, char **uuid)
{
	struct path_cxt *pc = NULL;
	char *id = NULL;
	int rc = 0;

	pc = ul_new_sysfs_path(devno, NULL, NULL);
	if (!pc)
		goto done;
	if (ul_path_read_string(pc, &id, "dm/uuid") <= 0 || !id)
		goto done;

	/* Private LVM devices use "LVM-<uuid>-<name>" uuid format (important
	 * is the "LVM" prefix and "-<name>" postfix).
	 */
	if (strncmp(id, "LVM-", 4) == 0) {
		char *p = strrchr(id + 4, '-');

		if (p && *(p + 1))
			rc = 1;

	/* Private Stratis devices prefix the UUID with "stratis-1-private"
	 */
	} else if (strncmp(id, "stratis-1-private", 17) == 0) {
		rc = 1;
	}
done:
	ul_unref_path(pc);
	if (uuid)
		*uuid = id;
	else
		free(id);
	return rc;
}

/*
 * Return 0 or 1, or < 0 in case of error
 */
int sysfs_devno_is_wholedisk(dev_t devno)
{
	dev_t disk;

	if (sysfs_devno_to_wholedisk(devno, NULL, 0, &disk) != 0)
		return -1;

	return devno == disk;
}


int sysfs_blkdev_scsi_get_hctl(struct path_cxt *pc, int *h, int *c, int *t, int *l)
{
	char buf[PATH_MAX], *hctl;
	struct sysfs_blkdev *blk;
	ssize_t len;

	blk = ul_path_get_dialect(pc);

	if (!blk || blk->hctl_error)
		return -EINVAL;
	if (blk->has_hctl)
		goto done;

	blk->hctl_error = 1;
	len = ul_path_readlink(pc, buf, sizeof(buf), "device");
	if (len < 0)
		return len;

	hctl = strrchr(buf, '/');
	if (!hctl)
		return -1;
	hctl++;

	if (sscanf(hctl, "%u:%u:%u:%u",	&blk->scsi_host, &blk->scsi_channel,
				&blk->scsi_target, &blk->scsi_lun) != 4)
		return -1;

	blk->has_hctl = 1;
done:
	if (h)
		*h = blk->scsi_host;
	if (c)
		*c = blk->scsi_channel;
	if (t)
		*t = blk->scsi_target;
	if (l)
		*l = blk->scsi_lun;

	blk->hctl_error = 0;
	return 0;
}


static char *scsi_host_attribute_path(
			struct path_cxt *pc,
			const char *type,
			char *buf,
			size_t bufsz,
			const char *attr)
{
	int len;
	int host;
	const char *prefix;

	if (sysfs_blkdev_scsi_get_hctl(pc, &host, NULL, NULL, NULL))
		return NULL;

	prefix = ul_path_get_prefix(pc);
	if (!prefix)
		prefix = "";

	if (attr)
		len = snprintf(buf, bufsz, "%s%s/%s_host/host%d/%s",
				prefix, _PATH_SYS_CLASS, type, host, attr);
	else
		len = snprintf(buf, bufsz, "%s%s/%s_host/host%d",
				prefix, _PATH_SYS_CLASS, type, host);

	return (len < 0 || (size_t) len >= bufsz) ? NULL : buf;
}

char *sysfs_blkdev_scsi_host_strdup_attribute(struct path_cxt *pc,
			const char *type, const char *attr)
{
	char buf[1024];
	int rc;
	FILE *f;

	if (!attr || !type ||
	    !scsi_host_attribute_path(pc, type, buf, sizeof(buf), attr))
		return NULL;

	if (!(f = fopen(buf, "r" UL_CLOEXECSTR)))
                return NULL;

	rc = fscanf(f, "%1023[^\n]", buf);
	fclose(f);

	return rc == 1 ? strdup(buf) : NULL;
}

int sysfs_blkdev_scsi_host_is(struct path_cxt *pc, const char *type)
{
	char buf[PATH_MAX];
	struct stat st;

	if (!type || !scsi_host_attribute_path(pc, type,
				buf, sizeof(buf), NULL))
		return 0;

	return stat(buf, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *scsi_attribute_path(struct path_cxt *pc,
		char *buf, size_t bufsz, const char *attr)
{
	int len, h, c, t, l;
	const char *prefix;

	if (sysfs_blkdev_scsi_get_hctl(pc, &h, &c, &t, &l) != 0)
		return NULL;

	prefix = ul_path_get_prefix(pc);
	if (!prefix)
		prefix = "";

	if (attr)
		len = snprintf(buf, bufsz, "%s%s/devices/%d:%d:%d:%d/%s",
				prefix, _PATH_SYS_SCSI,
				h,c,t,l, attr);
	else
		len = snprintf(buf, bufsz, "%s%s/devices/%d:%d:%d:%d",
				prefix, _PATH_SYS_SCSI,
				h,c,t,l);
	return (len < 0 || (size_t) len >= bufsz) ? NULL : buf;
}

int sysfs_blkdev_scsi_has_attribute(struct path_cxt *pc, const char *attr)
{
	char path[PATH_MAX];
	struct stat st;

	if (!scsi_attribute_path(pc, path, sizeof(path), attr))
		return 0;

	return stat(path, &st) == 0;
}

int sysfs_blkdev_scsi_path_contains(struct path_cxt *pc, const char *pattern)
{
	char path[PATH_MAX], linkc[PATH_MAX];
	struct stat st;
	ssize_t len;

	if (!scsi_attribute_path(pc, path, sizeof(path), NULL))
		return 0;

	if (stat(path, &st) != 0)
		return 0;

	len = readlink(path, linkc, sizeof(linkc) - 1);
	if (len < 0)
		return 0;

	linkc[len] = '\0';
	return strstr(linkc, pattern) != NULL;
}

static dev_t read_devno(const char *path)
{
	FILE *f;
	int maj = 0, min = 0;
	dev_t dev = 0;

	f = fopen(path, "r" UL_CLOEXECSTR);
	if (!f)
		return 0;

	if (fscanf(f, "%d:%d", &maj, &min) == 2)
		dev = makedev(maj, min);
	fclose(f);
	return dev;
}

int sysfs_devname_is_hidden(const char *prefix, const char *name)
{
	char buf[PATH_MAX];
	int rc = 0, hidden = 0, len;
	FILE *f;

	if (strncmp("/dev/", name, 5) == 0)
		return 0;

	if (!prefix)
		prefix = "";
	/*
	 * Create path to /sys/block/<name>/hidden
	 */
	len = snprintf(buf, sizeof(buf),
			"%s" _PATH_SYS_BLOCK "/%s/hidden",
			prefix, name);

	if (len < 0 || (size_t) len + 1 > sizeof(buf))
		return 0;

	f = fopen(buf, "r" UL_CLOEXECSTR);
	if (!f)
		return 0;

	rc = fscanf(f, "%d", &hidden);
	fclose(f);

	return rc == 1 ? hidden : 0;
}


dev_t __sysfs_devname_to_devno(const char *prefix, const char *name, const char *parent)
{
	char buf[PATH_MAX];
	char *_name = NULL, *_parent = NULL;	/* name as encoded in sysfs */
	dev_t dev = 0;
	int len;

	if (!prefix)
		prefix = "";

	assert(name);

	if (strncmp("/dev/", name, 5) == 0) {
		/*
		 * Read from /dev
		 */
		struct stat st;

		if (stat(name, &st) == 0) {
			dev = st.st_rdev;
			goto done;
		}
		name += 5;	/* unaccessible, or not node in /dev */
	}

	_name = strdup(name);
	if (!_name)
		goto done;
	sysfs_devname_dev_to_sys(_name);

	if (parent) {
		_parent = strdup(parent);
		if (!_parent)
			goto done;
	}

	if (parent && strncmp("dm-", name, 3) != 0) {
		/*
		 * Create path to /sys/block/<parent>/<name>/dev
		 */
		sysfs_devname_dev_to_sys(_parent);
		len = snprintf(buf, sizeof(buf),
				"%s" _PATH_SYS_BLOCK "/%s/%s/dev",
				prefix,	_parent, _name);
		if (len < 0 || (size_t) len >= sizeof(buf))
			goto done;

		/* don't try anything else for dm-* */
		dev = read_devno(buf);
		goto done;
	}

	/*
	 * Read from /sys/block/<sysname>/dev
	 */
	len = snprintf(buf, sizeof(buf),
			"%s" _PATH_SYS_BLOCK "/%s/dev",
			prefix, _name);
	if (len < 0 || (size_t) len >= sizeof(buf))
		goto done;
	dev = read_devno(buf);

	/*
	 * Read from /sys/block/<parent>/<partition>/dev
	 */
	if (!dev && parent && startswith(name, parent)) {
		len = snprintf(buf, sizeof(buf),
				"%s" _PATH_SYS_BLOCK "/%s/%s/dev",
				prefix, _parent, _name);
		if (len < 0 || (size_t) len >= sizeof(buf))
			goto done;
		dev = read_devno(buf);
	}

	/*
	 * Read from /sys/block/<sysname>/device/dev
	 */
	if (!dev) {
		len = snprintf(buf, sizeof(buf),
				"%s" _PATH_SYS_BLOCK "/%s/device/dev",
				prefix, _name);
		if (len < 0 || (size_t) len >= sizeof(buf))
			goto done;
		dev = read_devno(buf);
	}
done:
	free(_name);
	free(_parent);
	return dev;
}

dev_t sysfs_devname_to_devno(const char *name)
{
	return __sysfs_devname_to_devno(NULL, name, NULL);
}

char *sysfs_blkdev_get_path(struct path_cxt *pc, char *buf, size_t bufsiz)
{
	const char *name = sysfs_blkdev_get_name(pc, buf, bufsiz);
	char *res = NULL;
	size_t sz;
	struct stat st;

	if (!name)
		goto done;

	sz = strlen(name);
	if (sz + sizeof("/dev/") > bufsiz)
		goto done;

	/* create the final "/dev/<name>" string */
	memmove(buf + 5, name, sz + 1);
	memcpy(buf, "/dev/", 5);

	if (!stat(buf, &st) && S_ISBLK(st.st_mode) && st.st_rdev == sysfs_blkdev_get_devno(pc))
		res = buf;
done:
	return res;
}

dev_t sysfs_blkdev_get_devno(struct path_cxt *pc)
{
	return ((struct sysfs_blkdev *) ul_path_get_dialect(pc))->devno;
}

/*
 * Returns devname (e.g. "/dev/sda1") for the given devno.
 *
 * Please, use more robust blkid_devno_to_devname() in your applications.
 */
char *sysfs_devno_to_devpath(dev_t devno, char *buf, size_t bufsiz)
{
	struct path_cxt *pc = ul_new_sysfs_path(devno, NULL, NULL);
	char *res = NULL;

	if (pc) {
		res = sysfs_blkdev_get_path(pc, buf, bufsiz);
		ul_unref_path(pc);
	}
	return res;
}

char *sysfs_devno_to_devname(dev_t devno, char *buf, size_t bufsiz)
{
	struct path_cxt *pc = ul_new_sysfs_path(devno, NULL, NULL);
	char *res = NULL;

	if (pc) {
		res = sysfs_blkdev_get_name(pc, buf, bufsiz);
		ul_unref_path(pc);
	}
	return res;
}

int sysfs_devno_count_partitions(dev_t devno)
{
	struct path_cxt *pc = ul_new_sysfs_path(devno, NULL, NULL);
	int n = 0;

	if (pc) {
		char buf[PATH_MAX + 1];
		char *name = sysfs_blkdev_get_name(pc, buf, sizeof(buf));

		n = sysfs_blkdev_count_partitions(pc, name);
		ul_unref_path(pc);
	}
	return n;
}

char *sysfs_chrdev_devno_to_devname(dev_t devno, char *buf, size_t bufsiz)
{
	char link[PATH_MAX];
	struct path_cxt *pc;
	char *name;
	ssize_t	sz;

	pc = ul_new_path(_PATH_SYS_DEVCHAR "/%u:%u", major(devno), minor(devno));
	if (!pc)
		return NULL;

        /* read /sys/dev/char/<maj:min> link */
	sz = ul_path_readlink(pc, link, sizeof(link), NULL);
	ul_unref_path(pc);

	if (sz < 0)
		return NULL;

	name = strrchr(link, '/');
	if (!name)
		return NULL;

	name++;
	sz = strlen(name);
	if ((size_t) sz + 1 > bufsiz)
		return NULL;

	memcpy(buf, name, sz + 1);
	sysfs_devname_sys_to_dev(buf);
	return buf;

}

enum sysfs_byteorder sysfs_get_byteorder(struct path_cxt *pc)
{
	int rc;
	char buf[BUFSIZ];
	enum sysfs_byteorder ret;

	rc = ul_path_read_buffer(pc, buf, sizeof(buf), _PATH_SYS_CPU_BYTEORDER);
	if (rc < 0)
		goto unknown;

	if (strncmp(buf, "little", sizeof(buf)) == 0) {
		ret = SYSFS_BYTEORDER_LITTLE;
		goto out;
	} else if (strncmp(buf, "big", sizeof(buf)) == 0) {
		ret = SYSFS_BYTEORDER_BIG;
		goto out;
	}

unknown:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	ret = SYSFS_BYTEORDER_LITTLE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	ret = SYSFS_BYTEORDER_BIG;
#else
#error Unknown byte order
#endif

out:
	return ret;
}

int sysfs_get_address_bits(struct path_cxt *pc)
{
	int rc;
	int address_bits;

	rc = ul_path_scanf(pc, _PATH_SYS_ADDRESS_BITS, "%d", &address_bits);
	if (rc < 0)
		return rc;
	if (address_bits < 0)
		return -EINVAL;
	return address_bits;
}


#ifdef TEST_PROGRAM_SYSFS
#include <errno.h>
#include <err.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	struct path_cxt *pc;
	char *devname;
	dev_t devno, disk_devno;
	char path[PATH_MAX], *sub, *chain;
	char diskname[32];
	int i, is_part, rc = EXIT_SUCCESS;
	uint64_t u64;

	if (argc != 2)
		errx(EXIT_FAILURE, "usage: %s <devname>", argv[0]);

	ul_sysfs_init_debug();

	devname = argv[1];
	devno = sysfs_devname_to_devno(devname);

	if (!devno)
		err(EXIT_FAILURE, "failed to read devno");

	printf("non-context:\n");
	printf(" DEVNO:   %u (%d:%d)\n", (unsigned int) devno, major(devno), minor(devno));
	printf(" DEVNAME: %s\n", sysfs_devno_to_devname(devno, path, sizeof(path)));
	printf(" DEVPATH: %s\n", sysfs_devno_to_devpath(devno, path, sizeof(path)));

	sysfs_devno_to_wholedisk(devno, diskname, sizeof(diskname), &disk_devno);
	printf(" WHOLEDISK-DEVNO:   %u (%d:%d)\n", (unsigned int) disk_devno, major(disk_devno), minor(disk_devno));
	printf(" WHOLEDISK-DEVNAME: %s\n", diskname);

	pc = ul_new_sysfs_path(devno, NULL, NULL);
	if (!pc)
		goto done;

	printf("context based:\n");
	devno = sysfs_blkdev_get_devno(pc);
	printf(" DEVNO:   %u (%d:%d)\n", (unsigned int) devno, major(devno), minor(devno));
	printf(" DEVNAME: %s\n", sysfs_blkdev_get_name(pc, path, sizeof(path)));
	printf(" DEVPATH: %s\n", sysfs_blkdev_get_path(pc, path, sizeof(path)));

	sysfs_devno_to_wholedisk(devno, diskname, sizeof(diskname), &disk_devno);
	printf(" WHOLEDISK-DEVNO: %u (%d:%d)\n", (unsigned int) disk_devno, major(disk_devno), minor(disk_devno));
	printf(" WHOLEDISK-DEVNAME: %s\n", diskname);

	is_part = ul_path_access(pc, F_OK, "partition") == 0;
	printf(" PARTITION: %s\n", is_part ? "YES" : "NOT");

	if (is_part && disk_devno) {
		struct path_cxt *disk_pc =  ul_new_sysfs_path(disk_devno, NULL, NULL);
		sysfs_blkdev_set_parent(pc, disk_pc);

		ul_unref_path(disk_pc);
	}

	printf(" HOTPLUG: %s\n", sysfs_blkdev_is_hotpluggable(pc) ? "yes" : "no");
	printf(" REMOVABLE: %s\n", sysfs_blkdev_is_removable(pc) ? "yes" : "no");
	printf(" SLAVES: %d\n", ul_path_count_dirents(pc, "slaves"));

	if (!is_part) {
		printf("First 5 partitions:\n");
		for (i = 1; i <= 5; i++) {
			dev_t dev = sysfs_blkdev_partno_to_devno(pc, i);
			if (dev)
				printf("\t#%d %d:%d\n", i, major(dev), minor(dev));
		}
	}

	if (ul_path_read_u64(pc, &u64, "size") != 0)
		printf(" (!) read SIZE failed\n");
	else
		printf(" SIZE: %jd\n", u64);

	if (ul_path_read_s32(pc, &i, "queue/hw_sector_size"))
		printf(" (!) read SECTOR failed\n");
	else
		printf(" SECTOR: %d\n", i);


	chain = sysfs_blkdev_get_devchain(pc, path, sizeof(path));
	printf(" SUBSYSTEMS:\n");

	while (chain && sysfs_blkdev_next_subsystem(pc, chain, &sub) == 0) {
		printf("\t%s\n", sub);
		free(sub);
	}

	rc = EXIT_SUCCESS;
done:
	ul_unref_path(pc);
	return rc;
}
#endif /* TEST_PROGRAM_SYSFS */
