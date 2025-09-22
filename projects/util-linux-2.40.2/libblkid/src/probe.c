/*
 * Low-level libblkid probing API
 *
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: lowprobe
 * @title: Low-level probing
 * @short_description: low-level prober initialization
 *
 * The low-level probing routines always and directly read information from
 * the selected (see blkid_probe_set_device()) device.
 *
 * The probing routines are grouped together into separate chains. Currently,
 * the library provides superblocks, partitions and topology chains.
 *
 * The probing routines is possible to filter (enable/disable) by type (e.g.
 * fstype "vfat" or partype "gpt") or by usage flags (e.g. BLKID_USAGE_RAID).
 * These filters are per-chain. Note that always when you touch the chain
 * filter the current probing position is reset and probing starts from
 * scratch.  It means that the chain filter should not be modified during
 * probing, for example in loop where you call blkid_do_probe().
 *
 * For more details see the chain specific documentation.
 *
 * The low-level API provides two ways how access to probing results.
 *
 *   1. The NAME=value (tag) interface. This interface is older and returns all data
 *      as strings. This interface is generic for all chains.
 *
 *   2. The binary interfaces. These interfaces return data in the native formats.
 *      The interface is always specific to the probing chain.
 *
 *  Note that the previous probing result (binary or NAME=value) is always
 *  zeroized when a chain probing function is called. For example:
 *
 * <informalexample>
 *   <programlisting>
 *     blkid_probe_enable_partitions(pr, TRUE);
 *     blkid_probe_enable_superblocks(pr, FALSE);
 *
 *     blkid_do_safeprobe(pr);
 *   </programlisting>
 * </informalexample>
 *
 * overwrites the previous probing result for the partitions chain, the superblocks
 * result is not modified.
 */

/**
 * SECTION: lowprobe-tags
 * @title: Low-level tags
 * @short_description: generic NAME=value interface.
 *
 * The probing routines inside the chain are mutually exclusive by default --
 * only few probing routines are marked as "tolerant". The "tolerant" probing
 * routines are used for filesystem which can share the same device with any
 * other filesystem. The blkid_do_safeprobe() checks for the "tolerant" flag.
 *
 * The SUPERBLOCKS chain is enabled by default. The all others chains is
 * necessary to enable by blkid_probe_enable_'CHAINNAME'(). See chains specific
 * documentation.
 *
 * The blkid_do_probe() function returns a result from only one probing
 * routine, and the next call from the next probing routine. It means you need
 * to call the function in loop, for example:
 *
 * <informalexample>
 *   <programlisting>
 *	while((blkid_do_probe(pr) == BLKID_PROBE_OK)
 *		... use result ...
 *   </programlisting>
 * </informalexample>
 *
 * The blkid_do_safeprobe() is the same as blkid_do_probe(), but returns only
 * first probing result for every enabled chain. This function checks for
 * ambivalent results (e.g. more "intolerant" filesystems superblocks on the
 * device).
 *
 * The probing result is set of NAME=value pairs (the NAME is always unique).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/types.h>
#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif
#ifdef HAVE_LINUX_BLKZONED_H
#include <linux/blkzoned.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#ifdef HAVE_OPAL_GET_STATUS
#include <linux/sed-opal.h>
#endif

#include "blkidP.h"
#include "all-io.h"
#include "sysfs.h"
#include "strutils.h"
#include "list.h"
#include "fileutils.h"

/*
 * All supported chains
 */
static const struct blkid_chaindrv *chains_drvs[] = {
	[BLKID_CHAIN_SUBLKS] = &superblocks_drv,
	[BLKID_CHAIN_TOPLGY] = &topology_drv,
	[BLKID_CHAIN_PARTS] = &partitions_drv
};

static void blkid_probe_reset_values(blkid_probe pr);

/**
 * blkid_new_probe:
 *
 * Returns: a pointer to the newly allocated probe struct or NULL in case of error.
 */
blkid_probe blkid_new_probe(void)
{
	int i;
	blkid_probe pr;

	pr = calloc(1, sizeof(struct blkid_struct_probe));
	if (!pr)
		return NULL;

	DBG(LOWPROBE, ul_debug("allocate a new probe"));

	/* initialize chains */
	for (i = 0; i < BLKID_NCHAINS; i++) {
		pr->chains[i].driver = chains_drvs[i];
		pr->chains[i].flags = chains_drvs[i]->dflt_flags;
		pr->chains[i].enabled = chains_drvs[i]->dflt_enabled;
	}
	INIT_LIST_HEAD(&pr->buffers);
	INIT_LIST_HEAD(&pr->prunable_buffers);
	INIT_LIST_HEAD(&pr->values);
	INIT_LIST_HEAD(&pr->hints);
	return pr;
}

/*
 * Clone @parent, the new clone shares all, but except:
 *
 *	- probing result
 *	- buffers if another device (or offset) is set to the prober
 */
blkid_probe blkid_clone_probe(blkid_probe parent)
{
	blkid_probe pr;

	if (!parent)
		return NULL;

	DBG(LOWPROBE, ul_debug("allocate a probe clone"));

	pr = blkid_new_probe();
	if (!pr)
		return NULL;

	pr->fd = parent->fd;
	pr->off = parent->off;
	pr->size = parent->size;
	pr->io_size = parent->io_size;
	pr->devno = parent->devno;
	pr->disk_devno = parent->disk_devno;
	pr->blkssz = parent->blkssz;
	pr->flags = parent->flags;
	pr->zone_size = parent->zone_size;
	pr->parent = parent;

	pr->flags &= ~BLKID_FL_PRIVATE_FD;

	return pr;
}



/**
 * blkid_new_probe_from_filename:
 * @filename: device or regular file
 *
 * This function is same as call open(filename), blkid_new_probe() and
 * blkid_probe_set_device(pr, fd, 0, 0).
 *
 * The @filename is closed by blkid_free_probe() or by the
 * blkid_probe_set_device() call.
 *
 * Returns: a pointer to the newly allocated probe struct or NULL in case of
 * error.
 */
blkid_probe blkid_new_probe_from_filename(const char *filename)
{
	int fd;
	blkid_probe pr = NULL;

	fd = open(filename, O_RDONLY|O_CLOEXEC|O_NONBLOCK);
	if (fd < 0)
		return NULL;

	pr = blkid_new_probe();
	if (!pr)
		goto err;

	if (blkid_probe_set_device(pr, fd, 0, 0))
		goto err;

	pr->flags |= BLKID_FL_PRIVATE_FD;
	return pr;
err:
	close(fd);
	blkid_free_probe(pr);
	return NULL;
}

/**
 * blkid_free_probe:
 * @pr: probe
 *
 * Deallocates the probe struct, buffers and all allocated
 * data that are associated with this probing control struct.
 */
void blkid_free_probe(blkid_probe pr)
{
	int i;

	if (!pr)
		return;

	for (i = 0; i < BLKID_NCHAINS; i++) {
		struct blkid_chain *ch = &pr->chains[i];

		if (ch->driver->free_data)
			ch->driver->free_data(pr, ch->data);
		free(ch->fltr);
		ch->fltr = NULL;
	}

	if ((pr->flags & BLKID_FL_PRIVATE_FD) && pr->fd >= 0)
		close(pr->fd);
	blkid_probe_reset_buffers(pr);
	blkid_probe_reset_values(pr);
	blkid_probe_reset_hints(pr);
	blkid_free_probe(pr->disk_probe);

	DBG(LOWPROBE, ul_debug("free probe"));
	free(pr);
}

void blkid_probe_free_value(struct blkid_prval *v)
{
	if (!v)
		return;

	list_del(&v->prvals);
	free(v->data);

	DBG(LOWPROBE, ul_debug(" free value %s", v->name));
	free(v);
}

/*
 * Removes chain values from probing result.
 */
void blkid_probe_chain_reset_values(blkid_probe pr, struct blkid_chain *chn)
{

	struct list_head *p, *pnext;

	if (list_empty(&pr->values))
		return;

	DBG(LOWPROBE, ul_debug("Resetting %s values", chn->driver->name));

	list_for_each_safe(p, pnext, &pr->values) {
		struct blkid_prval *v = list_entry(p,
						struct blkid_prval, prvals);

		if (v->chain == chn)
			blkid_probe_free_value(v);
	}
}

static void blkid_probe_chain_reset_position(struct blkid_chain *chn)
{
	chn->idx = -1;
}

/*
 * Move chain values from probing result to @vals
 */
int blkid_probe_chain_save_values(blkid_probe pr, struct blkid_chain *chn,
				struct list_head *vals)
{
	struct list_head *p, *pnext;
	struct blkid_prval *v;

	DBG(LOWPROBE, ul_debug("saving %s values", chn->driver->name));

	list_for_each_safe(p, pnext, &pr->values) {

		v = list_entry(p, struct blkid_prval, prvals);
		if (v->chain != chn)
			continue;

		list_del_init(&v->prvals);
		list_add_tail(&v->prvals, vals);
	}
	return 0;
}

/*
 * Appends values from @vals to the probing result
 */
void blkid_probe_append_values_list(blkid_probe pr, struct list_head *vals)
{
	DBG(LOWPROBE, ul_debug("appending values"));

	list_splice(vals, &pr->values);
	INIT_LIST_HEAD(vals);
}


void blkid_probe_free_values_list(struct list_head *vals)
{
	if (!vals)
		return;

	DBG(LOWPROBE, ul_debug("freeing values list"));

	while (!list_empty(vals)) {
		struct blkid_prval *v = list_entry(vals->next, struct blkid_prval, prvals);
		blkid_probe_free_value(v);
	}
}

struct blkid_chain *blkid_probe_get_chain(blkid_probe pr)
{
	return pr->cur_chain;
}

static const char *blkid_probe_get_probername(blkid_probe pr)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (chn && chn->idx >= 0 && (unsigned)chn->idx < chn->driver->nidinfos)
		return chn->driver->idinfos[chn->idx]->name;

	return NULL;
}

void *blkid_probe_get_binary_data(blkid_probe pr, struct blkid_chain *chn)
{
	int rc, org_prob_flags;
	struct blkid_chain *org_chn;

	/* save the current setting -- the binary API has to be completely
	 * independent on the current probing status
	 */
	org_chn = pr->cur_chain;
	org_prob_flags = pr->prob_flags;

	pr->cur_chain = chn;
	pr->prob_flags = 0;
	chn->binary = TRUE;
	blkid_probe_chain_reset_position(chn);

	rc = chn->driver->probe(pr, chn);

	chn->binary = FALSE;
	blkid_probe_chain_reset_position(chn);

	/* restore the original setting
	 */
	pr->cur_chain = org_chn;
	pr->prob_flags = org_prob_flags;

	if (rc != 0)
		return NULL;

	DBG(LOWPROBE, ul_debug("returning %s binary data", chn->driver->name));
	return chn->data;
}


/**
 * blkid_reset_probe:
 * @pr: probe
 *
 * Zeroize probing results and resets the current probing (this has impact to
 * blkid_do_probe() only). This function does not touch probing filters and
 * keeps assigned device.
 */
void blkid_reset_probe(blkid_probe pr)
{
	int i;

	blkid_probe_reset_values(pr);
	blkid_probe_set_wiper(pr, 0, 0);

	pr->cur_chain = NULL;

	for (i = 0; i < BLKID_NCHAINS; i++)
		blkid_probe_chain_reset_position(&pr->chains[i]);
}

/***
static int blkid_probe_dump_filter(blkid_probe pr, int chain)
{
	struct blkid_chain *chn;
	int i;

	if (!pr || chain < 0 || chain >= BLKID_NCHAINS)
		return -1;

	chn = &pr->chains[chain];

	if (!chn->fltr)
		return -1;

	for (i = 0; i < chn->driver->nidinfos; i++) {
		const struct blkid_idinfo *id = chn->driver->idinfos[i];

		DBG(LOWPROBE, ul_debug("%d: %s: %s",
			i,
			id->name,
			blkid_bmp_get_item(chn->fltr, i)
				? "disabled" : "enabled <--"));
	}
	return 0;
}
***/

/*
 * Returns properly initialized chain filter
 */
unsigned long *blkid_probe_get_filter(blkid_probe pr, int chain, int create)
{
	struct blkid_chain *chn;

	if (chain < 0 || chain >= BLKID_NCHAINS)
		return NULL;

	chn = &pr->chains[chain];

	/* always when you touch the chain filter all indexes are reset and
	 * probing starts from scratch
	 */
	blkid_probe_chain_reset_position(chn);
	pr->cur_chain = NULL;

	if (!chn->driver->has_fltr || (!chn->fltr && !create))
		return NULL;

	if (!chn->fltr)
		chn->fltr = calloc(1, blkid_bmp_nbytes(chn->driver->nidinfos));
	else
		memset(chn->fltr, 0, blkid_bmp_nbytes(chn->driver->nidinfos));

	/* blkid_probe_dump_filter(pr, chain); */
	return chn->fltr;
}

/*
 * Generic private functions for filter setting
 */
int __blkid_probe_invert_filter(blkid_probe pr, int chain)
{
	size_t i;
	struct blkid_chain *chn;

	chn = &pr->chains[chain];

	if (!chn->driver->has_fltr || !chn->fltr)
		return -1;

	for (i = 0; i < blkid_bmp_nwords(chn->driver->nidinfos); i++)
		chn->fltr[i] = ~chn->fltr[i];

	DBG(LOWPROBE, ul_debug("probing filter inverted"));
	/* blkid_probe_dump_filter(pr, chain); */
	return 0;
}

int __blkid_probe_reset_filter(blkid_probe pr, int chain)
{
	return blkid_probe_get_filter(pr, chain, FALSE) ? 0 : -1;
}

int __blkid_probe_filter_types(blkid_probe pr, int chain, int flag, char *names[])
{
	unsigned long *fltr;
	struct blkid_chain *chn;
	size_t i;

	fltr = blkid_probe_get_filter(pr, chain, TRUE);
	if (!fltr)
		return -1;

	chn = &pr->chains[chain];

	for (i = 0; i < chn->driver->nidinfos; i++) {
		int has = 0;
		const struct blkid_idinfo *id = chn->driver->idinfos[i];
		char **n;

		for (n = names; *n; n++) {
			if (!strcmp(id->name, *n)) {
				has = 1;
				break;
			}
		}
		if (has) {
			if (flag & BLKID_FLTR_NOTIN)
				blkid_bmp_set_item(fltr, i);
		} else if (flag & BLKID_FLTR_ONLYIN)
			blkid_bmp_set_item(fltr, i);
	}

	DBG(LOWPROBE, ul_debug("%s: a new probing type-filter initialized",
		chn->driver->name));
	/* blkid_probe_dump_filter(pr, chain); */
	return 0;
}

static void remove_buffer(struct blkid_bufinfo *bf)
{
	list_del(&bf->bufs);

	DBG(BUFFER, ul_debug(" remove buffer: [off=%"PRIu64", len=%"PRIu64"]",
				bf->off, bf->len));
	munmap(bf->data, bf->len);
	free(bf);
}

static struct blkid_bufinfo *read_buffer(blkid_probe pr, uint64_t real_off, uint64_t len)
{
	ssize_t ret;
	struct blkid_bufinfo *bf = NULL;

	if (lseek(pr->fd, real_off, SEEK_SET) == (off_t) -1) {
		errno = 0;
		return NULL;
	}

	/* someone trying to overflow some buffers? */
	if (len > ULONG_MAX - sizeof(struct blkid_bufinfo)) {
		errno = ENOMEM;
		return NULL;
	}

	/* allocate info and space for data by one malloc call */
	bf = calloc(1, sizeof(struct blkid_bufinfo));
	if (!bf) {
		errno = ENOMEM;
		return NULL;
	}

	bf->data = mmap(NULL, len, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (bf->data == MAP_FAILED) {
		free(bf);
		errno = ENOMEM;
		return NULL;
	}

	bf->len = len;
	bf->off = real_off;
	INIT_LIST_HEAD(&bf->bufs);

	DBG(LOWPROBE, ul_debug("\tread: off=%"PRIu64" len=%"PRIu64"",
	                       real_off, len));

	ret = read(pr->fd, bf->data, len);
	if (ret != (ssize_t) len) {
		DBG(LOWPROBE, ul_debug("\tread failed: %m"));
		remove_buffer(bf);

		/* I/O errors on CDROMs are non-fatal to work with hybrid
		 * audio+data disks */
		if (ret >= 0 || blkid_probe_is_cdrom(pr) || blkdid_probe_is_opal_locked(pr))
			errno = 0;
#ifdef HAVE_OPAL_GET_STATUS
		else {
			struct opal_status st = { };

			/* If the device is locked with OPAL, we'll fail to read with I/O
			 * errors when probing deep into the block device. Do not return
			 * an error, so that we can move on to different types of checks.*/
			ret = ioctl(pr->fd, IOC_OPAL_GET_STATUS, &st);
			if (ret == 0 && (st.flags & OPAL_FL_LOCKED)) {
				pr->flags |= BLKID_FL_OPAL_LOCKED;
				errno = 0;
			}
		}
#endif

		return NULL;
	}

	if (mprotect(bf->data, len, PROT_READ))
		DBG(LOWPROBE, ul_debug("\tmprotect failed: %m"));

	return bf;
}

/*
 * Search in buffers we already have in memory
 */
static struct blkid_bufinfo *get_cached_buffer(blkid_probe pr, uint64_t off, uint64_t len)
{
	uint64_t real_off = pr->off + off;
	struct list_head *p;

	list_for_each(p, &pr->buffers) {
		struct blkid_bufinfo *x =
				list_entry(p, struct blkid_bufinfo, bufs);

		if (real_off >= x->off && real_off + len <= x->off + x->len) {
			DBG(BUFFER, ul_debug("\treuse: off=%"PRIu64" len=%"PRIu64" (for off=%"PRIu64" len=%"PRIu64")",
						x->off, x->len, real_off, len));
			return x;
		}
	}
	return NULL;
}

/*
 * Mark smaller buffers that can be satisfied by bf as prunable
 */
static void mark_prunable_buffers(blkid_probe pr, const struct blkid_bufinfo *bf)
{
	struct list_head *p, *next;

	list_for_each_safe(p, next, &pr->buffers) {
		struct blkid_bufinfo *x =
				list_entry(p, struct blkid_bufinfo, bufs);

		if (bf->off <= x->off && bf->off + bf->len >= x->off + x->len) {
			list_del(&x->bufs);
			list_add(&x->bufs, &pr->prunable_buffers);
		}
	}
}

/*
 * Remove buffers that are marked as prunable
 */
void blkid_probe_prune_buffers(blkid_probe pr)
{
	struct list_head *p, *next;

	list_for_each_safe(p, next, &pr->prunable_buffers) {
		struct blkid_bufinfo *x =
				list_entry(p, struct blkid_bufinfo, bufs);

		remove_buffer(x);
	}
}

/*
 * Zeroize in-memory data in already read buffer. The next blkid_probe_get_buffer()
 * will return modified buffer. This is usable when you want to call the same probing
 * function more than once and hide previously detected magic strings.
 *
 * See blkid_probe_hide_range().
 */
static int hide_buffer(blkid_probe pr, uint64_t off, uint64_t len)
{
	uint64_t real_off = pr->off + off;
	struct list_head *p;
	int ct = 0;

	if (UINT64_MAX - len < off) {
		DBG(BUFFER, ul_debug("\t  hide-buffer overflow (ignore)"));
		return -EINVAL;
	}

	list_for_each(p, &pr->buffers) {
		struct blkid_bufinfo *x =
			list_entry(p, struct blkid_bufinfo, bufs);
		unsigned char *data;

		if (real_off >= x->off && real_off + len <= x->off + x->len) {

			assert(x->off <= real_off);
			assert(x->off + x->len >= real_off + len);

			data = real_off ? x->data + (real_off - x->off) : x->data;

			DBG(BUFFER, ul_debug("\thiding: off=%"PRIu64" len=%"PRIu64,
						off, len));
			mprotect(x->data, x->len, PROT_READ | PROT_WRITE);
			memset(data, 0, len);
			mprotect(x->data, x->len, PROT_READ);
			ct++;
		}
	}
	return ct == 0 ? -EINVAL : 0;
}


/*
 * Note that @off is offset within probing area, the probing area is defined by
 * pr->off and pr->size.
 */
const unsigned char *blkid_probe_get_buffer(blkid_probe pr, uint64_t off, uint64_t len)
{
	struct blkid_bufinfo *bf = NULL;
	uint64_t real_off, bias, len_align;

	bias = off % pr->io_size;
	off -= bias;
	len += bias;

	if (len % pr->io_size) {
		len_align = pr->io_size - (len % pr->io_size);

		if (pr->off + off + len + len_align <= pr->size)
			len += len_align;
	}

	real_off = pr->off + off;

	/*
	DBG(BUFFER, ul_debug("\t>>>> off=%ju, real-off=%ju (probe <%ju..%ju>, len=%ju",
				off, real_off, pr->off, pr->off + pr->size, len));
	*/
	if (pr->size == 0 || pr->io_size == 0) {
		errno = EINVAL;
		return NULL;
	}

	if (UINT64_MAX - len < off || UINT64_MAX - len < real_off) {
		DBG(BUFFER, ul_debug("\t  read-buffer overflow (ignore)"));
		return NULL;
	}

	if (len > 8388608 /* 8 Mib */ ) {
		DBG(BUFFER, ul_debug("\t  too large read request (ignore)"));
		return NULL;
	}

	if (len == 0
	    || (!S_ISCHR(pr->mode) && (pr->size < off || pr->size < len))
	    || (!S_ISCHR(pr->mode) && (pr->off + pr->size < real_off + len))) {
		DBG(BUFFER, ul_debug("\t  read-buffer out of probing area (ignore)"));
		errno = 0;
		return NULL;
	}

	if (pr->parent &&
	    pr->parent->devno == pr->devno &&
	    pr->parent->off <= pr->off &&
	    pr->parent->off + pr->parent->size >= pr->off + pr->size) {
		/*
		 * This is a cloned prober and points to the same area as
		 * parent. Let's use parent's buffers.
		 *
		 * Note that pr->off (and pr->parent->off) is always from the
		 * begin of the device.
		 */
		return blkid_probe_get_buffer(pr->parent,
				pr->off + off - pr->parent->off, len);
	}

	/* try buffers we already have in memory or read from device */
	bf = get_cached_buffer(pr, off, len);
	if (!bf) {
		bf = read_buffer(pr, real_off, len);
		if (!bf)
			return NULL;

		mark_prunable_buffers(pr, bf);
		list_add_tail(&bf->bufs, &pr->buffers);
	}

	assert(bf->off <= real_off);
	assert(bf->off + bf->len >= real_off + len);

	errno = 0;
	return real_off ? bf->data + (real_off - bf->off + bias) : bf->data + bias;
}

/**
 * blkid_probe_reset_buffers:
 * @pr: prober
 *
 * libblkid reuse all already read buffers from the device. The buffers may be
 * modified by blkid_probe_hide_range(). This function reset and free all
 * cached buffers. The next blkid_do_probe() will read all data from the
 * device.
 *
 * Since: 2.31
 *
 * Returns: <0 in case of failure, or 0 on success.
 */
int blkid_probe_reset_buffers(blkid_probe pr)
{
	uint64_t ct = 0, len = 0;

	pr->flags &= ~BLKID_FL_MODIF_BUFF;

	blkid_probe_prune_buffers(pr);

	if (list_empty(&pr->buffers))
		return 0;

	DBG(BUFFER, ul_debug("Resetting probing buffers"));

	while (!list_empty(&pr->buffers)) {
		struct blkid_bufinfo *bf = list_entry(pr->buffers.next,
						struct blkid_bufinfo, bufs);
		ct++;
		len += bf->len;

		remove_buffer(bf);
	}

	DBG(LOWPROBE, ul_debug(" buffers summary: %"PRIu64" bytes by %"PRIu64" read() calls",
			len, ct));

	INIT_LIST_HEAD(&pr->buffers);

	return 0;
}

/**
 * blkid_probe_hide_range:
 * @pr: prober
 * @off: start of the range
 * @len: size of the range
 *
 * This function modifies in-memory cached data from the device. The specified
 * range is zeroized. This is usable together with blkid_probe_step_back().
 * The next blkid_do_probe() will not see specified area.
 *
 * Note that this is usable for already (by library) read data, and this
 * function is not a way how to hide any large areas on your device.
 *
 * The function blkid_probe_reset_buffers() reverts all.
 *
 * Since: 2.31
 *
 * Returns: <0 in case of failure, or 0 on success.
 */
int blkid_probe_hide_range(blkid_probe pr, uint64_t off, uint64_t len)
{
	int rc = hide_buffer(pr, off, len);

	if (rc == 0)
		pr->flags |= BLKID_FL_MODIF_BUFF;
	return rc;
}


static void blkid_probe_reset_values(blkid_probe pr)
{
	if (list_empty(&pr->values))
		return;

	DBG(LOWPROBE, ul_debug("resetting results"));

	while (!list_empty(&pr->values)) {
		struct blkid_prval *v = list_entry(pr->values.next,
						struct blkid_prval, prvals);
		blkid_probe_free_value(v);
	}

	INIT_LIST_HEAD(&pr->values);
}

/*
 * Small devices need a special care.
 */
int blkid_probe_is_tiny(blkid_probe pr)
{
	return (pr->flags & BLKID_FL_TINY_DEV);
}

/*
 * CDROMs may fail when probed for RAID (last sector problem)
 */
int blkid_probe_is_cdrom(blkid_probe pr)
{
	return (pr->flags & BLKID_FL_CDROM_DEV);
}

int blkdid_probe_is_opal_locked(blkid_probe pr)
{
	return (pr->flags & BLKID_FL_OPAL_LOCKED);
}

#ifdef CDROM_GET_CAPABILITY

static int is_sector_readable(int fd, uint64_t sector)
{
	char buf[512];
	ssize_t sz;

	if (lseek(fd, sector * 512, SEEK_SET) == (off_t) -1)
		goto failed;

	sz = read(fd, buf, sizeof(buf));
	if (sz != (ssize_t) sizeof(buf))
		goto failed;

	return 1;
failed:
	DBG(LOWPROBE, ul_debug("CDROM: read sector %"PRIu64" failed %m", sector));
	errno = 0;
	return 0;
}

/*
 * Linux kernel reports (BLKGETSIZE) cdrom device size greater than area
 * readable by read(2). We have to reduce the probing area to avoid unwanted
 * I/O errors in probing functions. It seems that unreadable are always last 2
 * or 3 CD blocks (CD block size is 2048 bytes, it means 12 in 512-byte
 * sectors). Linux kernel reports (CDROM_LAST_WRITTEN) also location of last
 * written block, so we will reduce size based on it too.
 */
static void cdrom_size_correction(blkid_probe pr, uint64_t last_written)
{
	uint64_t n, nsectors = pr->size >> 9;

	if (last_written && nsectors > ((last_written+1) << 2))
		nsectors = (last_written+1) << 2;

	for (n = nsectors - 12; n < nsectors; n++) {
		if (!is_sector_readable(pr->fd, n))
			goto failed;
	}

	DBG(LOWPROBE, ul_debug("CDROM: full size available"));
	return;
failed:
	/* 'n' is the failed sector, reduce device size to n-1; */
	DBG(LOWPROBE, ul_debug("CDROM: reduce size from %ju to %ju.",
				(uintmax_t) pr->size,
				(uintmax_t) n << 9));
	pr->size = n << 9;
}

#endif

#ifdef BLKIOOPT
static uint64_t blkid_get_io_size(int fd)
{
	static const int ioctls[] = { BLKIOOPT, BLKIOMIN, BLKBSZGET };
	unsigned int s;
	size_t i;
	int r;

	for (i = 0; i < ARRAY_SIZE(ioctls); i++) {
		r = ioctl(fd, ioctls[i], &s);
		if (r == 0 && is_power_of_2(s) && s >= DEFAULT_SECTOR_SIZE)
			return min(s, 1U << 16);
	}

	return DEFAULT_SECTOR_SIZE;
}
#endif

/**
 * blkid_probe_set_device:
 * @pr: probe
 * @fd: device file descriptor
 * @off: begin of probing area
 * @size: size of probing area (zero means whole device/file)
 *
 * Assigns the device to probe control struct, resets internal buffers, resets
 * the current probing, and close previously associated device (if open by
 * libblkid).
 *
 * If @fd is < 0 than only resets the prober and returns 1. Note that
 * blkid_reset_probe() keeps the device associated with the prober, but
 * blkid_probe_set_device() does complete reset.
 *
 * Returns: -1 in case of failure, 0 on success and 1 on reset.
 */
int blkid_probe_set_device(blkid_probe pr, int fd,
		blkid_loff_t off, blkid_loff_t size)
{
	struct stat sb;
	uint64_t devsiz = 0;
	char *dm_uuid = NULL;
	int is_floppy = 0;

	blkid_reset_probe(pr);
	blkid_probe_reset_buffers(pr);

	if ((pr->flags & BLKID_FL_PRIVATE_FD) && pr->fd >= 0)
		close(pr->fd);

	if (pr->disk_probe) {
		blkid_free_probe(pr->disk_probe);
		pr->disk_probe = NULL;
	}

	pr->flags &= ~BLKID_FL_PRIVATE_FD;
	pr->flags &= ~BLKID_FL_TINY_DEV;
	pr->flags &= ~BLKID_FL_CDROM_DEV;
	pr->prob_flags = 0;
	pr->fd = fd;
	pr->off = (uint64_t) off;
	pr->size = 0;
	pr->io_size = DEFAULT_SECTOR_SIZE;
	pr->devno = 0;
	pr->disk_devno = 0;
	pr->mode = 0;
	pr->blkssz = 0;
	pr->wipe_off = 0;
	pr->wipe_size = 0;
	pr->wipe_chain = NULL;
	pr->zone_size = 0;

	if (fd < 0)
		return 1;


#if defined(POSIX_FADV_RANDOM) && defined(HAVE_POSIX_FADVISE)
	/* Disable read-ahead */
	posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
#endif
	if (fstat(fd, &sb))
		goto err;

	if (!S_ISBLK(sb.st_mode) && !S_ISCHR(sb.st_mode) && !S_ISREG(sb.st_mode)) {
		errno = EINVAL;
		goto err;
	}

	pr->mode = sb.st_mode;
	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode))
		pr->devno = sb.st_rdev;

	if (S_ISBLK(sb.st_mode)) {
		if (blkdev_get_size(fd, (unsigned long long *) &devsiz)) {
			DBG(LOWPROBE, ul_debug("failed to get device size"));
			goto err;
		}
	} else if (S_ISCHR(sb.st_mode)) {
		char buf[PATH_MAX];

		if (!sysfs_chrdev_devno_to_devname(sb.st_rdev, buf, sizeof(buf))
		    || strncmp(buf, "ubi", 3) != 0) {
			DBG(LOWPROBE, ul_debug("no UBI char device"));
			errno = EINVAL;
			goto err;
		}
		devsiz = 1;		/* UBI devices are char... */
	} else if (S_ISREG(sb.st_mode))
		devsiz = sb.st_size;	/* regular file */

	pr->size = size ? (uint64_t)size : devsiz;

	if (off && size == 0)
		/* only offset without size specified */
		pr->size -= (uint64_t) off;

	if (pr->off + pr->size > devsiz) {
		DBG(LOWPROBE, ul_debug("area specified by offset and size is bigger than device"));
		errno = EINVAL;
		goto err;
	}

	if (pr->size <= 1440 * 1024 && !S_ISCHR(sb.st_mode))
		pr->flags |= BLKID_FL_TINY_DEV;

#ifdef FDGETFDCSTAT
	if (S_ISBLK(sb.st_mode)) {
		/*
		 * Re-open without O_NONBLOCK for floppy device.
		 *
		 * Since kernel commit c7e9d0020361f4308a70cdfd6d5335e273eb8717
		 * floppy drive works bad when opened with O_NONBLOCK.
		 */
		struct floppy_fdc_state flst;

		if (ioctl(fd, FDGETFDCSTAT, &flst) >= 0) {
			int flags = fcntl(fd, F_GETFL, 0);

			if (flags < 0)
				goto err;
			if (flags & O_NONBLOCK) {
				flags &= ~O_NONBLOCK;

				fd = ul_reopen(fd, flags | O_CLOEXEC);
				if (fd < 0)
					goto err;

				pr->flags |= BLKID_FL_PRIVATE_FD;
				pr->fd = fd;
			}
			is_floppy = 1;
		}
		errno = 0;
	}
#endif
	if (S_ISBLK(sb.st_mode) &&
	    !is_floppy &&
	    sysfs_devno_is_dm_private(sb.st_rdev, &dm_uuid)) {
		DBG(LOWPROBE, ul_debug("ignore private device mapper device"));
		pr->flags |= BLKID_FL_NOSCAN_DEV;
	}

#ifdef CDROM_GET_CAPABILITY
	else if (S_ISBLK(sb.st_mode) &&
	    !blkid_probe_is_tiny(pr) &&
	    !dm_uuid &&
	    !is_floppy &&
	    blkid_probe_is_wholedisk(pr)) {

		long last_written = 0;

		/*
		 * pktcdvd.ko accepts only these ioctls:
		 *   CDROMEJECT CDROMMULTISESSION CDROMREADTOCENTRY
		 *   CDROM_LAST_WRITTEN CDROM_SEND_PACKET SCSI_IOCTL_SEND_COMMAND
		 * So CDROM_GET_CAPABILITY cannot be used for detecting pktcdvd
		 * devices. But CDROM_GET_CAPABILITY and CDROM_DRIVE_STATUS are
		 * fast so use them for detecting if medium is present. In any
		 * case use last written block form CDROM_LAST_WRITTEN.
		 */

		if (ioctl(fd, CDROM_GET_CAPABILITY, NULL) >= 0) {
# ifdef CDROM_DRIVE_STATUS
			switch (ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT)) {
			case CDS_TRAY_OPEN:
			case CDS_NO_DISC:
				errno = ENOMEDIUM;
				goto err;
			}
# endif
			pr->flags |= BLKID_FL_CDROM_DEV;
		}

# ifdef CDROM_LAST_WRITTEN
		if (ioctl(fd, CDROM_LAST_WRITTEN, &last_written) == 0) {
			pr->flags |= BLKID_FL_CDROM_DEV;
		} else {
			if (errno == ENOMEDIUM)
				goto err;
		}
# endif

		if (pr->flags & BLKID_FL_CDROM_DEV) {
			cdrom_size_correction(pr, last_written);

# ifdef CDROMMULTISESSION
			if (!pr->off && blkid_probe_get_hint(pr, "session_offset", NULL) < 0) {
				struct cdrom_multisession multisession = { .addr_format = CDROM_LBA };
				if (ioctl(fd, CDROMMULTISESSION, &multisession) == 0 && multisession.xa_flag)
					blkid_probe_set_hint(pr, "session_offset", (multisession.addr.lba << 11));
			}
# endif
		}
	}
#endif
	free(dm_uuid);

# ifdef BLKGETZONESZ
	if (S_ISBLK(sb.st_mode) && !is_floppy) {
		uint32_t zone_size_sector;

		if (!ioctl(pr->fd, BLKGETZONESZ, &zone_size_sector))
			pr->zone_size = zone_size_sector << 9;
	}
# endif

#ifdef BLKIOOPT
	if (S_ISBLK(sb.st_mode) && !is_floppy && !blkid_probe_is_tiny(pr))
		pr->io_size = blkid_get_io_size(fd);
#endif

	DBG(LOWPROBE, ul_debug("ready for low-probing, offset=%"PRIu64", size=%"PRIu64", zonesize=%"PRIu64", iosize=%"PRIu64,
				pr->off, pr->size, pr->zone_size, pr->io_size));
	DBG(LOWPROBE, ul_debug("whole-disk: %s, regfile: %s",
		blkid_probe_is_wholedisk(pr) ?"YES" : "NO",
		S_ISREG(pr->mode) ? "YES" : "NO"));

	return 0;
err:
	DBG(LOWPROBE, ul_debug("failed to prepare a device for low-probing"));
	return -1;

}

int blkid_probe_get_dimension(blkid_probe pr, uint64_t *off, uint64_t *size)
{
	*off = pr->off;
	*size = pr->size;
	return 0;
}

int blkid_probe_set_dimension(blkid_probe pr, uint64_t off, uint64_t size)
{
	DBG(LOWPROBE, ul_debug(
		"changing probing area: size=%"PRIu64", off=%"PRIu64" "
		"-to-> size=%"PRIu64", off=%"PRIu64"",
		pr->size, pr->off, size, off));

	pr->off = off;
	pr->size = size;
	pr->flags &= ~BLKID_FL_TINY_DEV;

	if (pr->size <= 1440ULL * 1024ULL && !S_ISCHR(pr->mode))
		pr->flags |= BLKID_FL_TINY_DEV;

	blkid_probe_reset_buffers(pr);

	return 0;
}

const unsigned char *blkid_probe_get_sb_buffer(blkid_probe pr, const struct blkid_idmag *mag, size_t size)
{
	uint64_t hint_offset, off;

	if (mag->kboff >= 0) {
		if (!mag->hoff || blkid_probe_get_hint(pr, mag->hoff, &hint_offset) < 0)
			hint_offset = 0;

		off = hint_offset + (mag->kboff << 10);
	} else {
		off = pr->size - (-mag->kboff << 10);
	}

	return blkid_probe_get_buffer(pr, off, size);
}

uint64_t blkid_probe_get_idmag_off(blkid_probe pr, const struct blkid_idmag *mag)
{
	if (mag->kboff >= 0)
		return mag->kboff << 10;
	else
		return pr->size - (-mag->kboff << 10);
}

/*
 * Check for matching magic value.
 * Returns BLKID_PROBE_OK if found, BLKID_PROBE_NONE if not found
 * or no magic present, or negative value on error.
 */
int blkid_probe_get_idmag(blkid_probe pr, const struct blkid_idinfo *id,
			uint64_t *offset, const struct blkid_idmag **res)
{
	const struct blkid_idmag *mag = NULL;
	uint64_t off = 0;

	if (id)
		mag = &id->magics[0];
	if (res)
		*res = NULL;

	/* try to detect by magic string */
	while(mag && mag->magic) {
		const unsigned char *buf;
		long kboff;
		uint64_t hint_offset;

		if (!mag->hoff || blkid_probe_get_hint(pr, mag->hoff, &hint_offset) < 0)
			hint_offset = 0;

		/* If the magic is for zoned device, skip non-zoned device */
		if (mag->is_zoned && !pr->zone_size) {
			mag++;
			continue;
		}

		if (!mag->is_zoned)
			kboff = mag->kboff;
		else
			kboff = ((mag->zonenum * pr->zone_size) >> 10) + mag->kboff_inzone;

		if (kboff >= 0)
			off = hint_offset + (kboff << 10) + mag->sboff;
		else
			off = pr->size - (-kboff << 10) + mag->sboff;
		buf = blkid_probe_get_buffer(pr, off, mag->len);

		if (!buf && errno)
			return -errno;

		if (buf && !memcmp(mag->magic, buf, mag->len)) {
			DBG(LOWPROBE, ul_debug("\tmagic sboff=%u, kboff=%ld",
				mag->sboff, kboff));
			if (offset)
				*offset = off;
			if (res)
				*res = mag;
			return BLKID_PROBE_OK;
		}
		mag++;
	}

	if (id && id->magics[0].magic)
		/* magic string(s) defined, but not found */
		return BLKID_PROBE_NONE;

	return BLKID_PROBE_OK;
}

static inline void blkid_probe_start(blkid_probe pr)
{
	DBG(LOWPROBE, ul_debug("start probe"));
	pr->cur_chain = NULL;
	pr->prob_flags = 0;
	blkid_probe_set_wiper(pr, 0, 0);
}

static inline void blkid_probe_end(blkid_probe pr)
{
	DBG(LOWPROBE, ul_debug("end probe"));
	pr->cur_chain = NULL;
	pr->prob_flags = 0;
	blkid_probe_set_wiper(pr, 0, 0);
}

/**
 * blkid_do_probe:
 * @pr: prober
 *
 * Calls probing functions in all enabled chains. The superblocks chain is
 * enabled by default. The blkid_do_probe() stores result from only one
 * probing function. It's necessary to call this routine in a loop to get
 * results from all probing functions in all chains. The probing is reset
 * by blkid_reset_probe() or by filter functions.
 *
 * This is string-based NAME=value interface only.
 *
 * <example>
 *   <title>basic case - use the first result only</title>
 *   <programlisting>
 *	if (blkid_do_probe(pr) == BLKID_PROBE_OK) {
 *		int nvals = blkid_probe_numof_values(pr);
 *		for (n = 0; n < nvals; n++) {
 *			if (blkid_probe_get_value(pr, n, &name, &data, &len) == 0)
 *				printf("%s = %s\n", name, data);
 *		}
 *	}
 *  </programlisting>
 * </example>
 *
 * <example>
 *   <title>advanced case - probe for all signatures</title>
 *   <programlisting>
 *	while (blkid_do_probe(pr) == BLKID_PROBE_OK) {
 *		int nvals = blkid_probe_numof_values(pr);
 *		...
 *	}
 *  </programlisting>
 * </example>
 *
 * See also blkid_reset_probe().
 *
 * Returns: 0 on success, 1 when probing is done and -1 in case of error.
 */
int blkid_do_probe(blkid_probe pr)
{
	int rc = 1;

	if (pr->flags & BLKID_FL_NOSCAN_DEV)
		return BLKID_PROBE_NONE;

	do {
		struct blkid_chain *chn = pr->cur_chain;

		if (!chn) {
			blkid_probe_start(pr);
			chn = pr->cur_chain = &pr->chains[0];
		}
		/* we go to the next chain only when the previous probing
		 * result was nothing (rc == 1) and when the current chain is
		 * disabled or we are at end of the current chain (chain->idx +
		 * 1 == sizeof chain) or the current chain bailed out right at
		 * the start (chain->idx == -1)
		 */
		else if (rc == 1 && (chn->enabled == FALSE ||
				     chn->idx + 1 == (int) chn->driver->nidinfos ||
				     chn->idx == -1)) {

			size_t idx = chn->driver->id + 1;

			if (idx < BLKID_NCHAINS)
				chn = pr->cur_chain = &pr->chains[idx];
			else {
				blkid_probe_end(pr);
				return BLKID_PROBE_NONE;	/* all chains already probed */
			}
		}

		chn->binary = FALSE;		/* for sure... */

		DBG(LOWPROBE, ul_debug("chain probe %s %s (idx=%d)",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED",
				chn->idx));

		if (!chn->enabled)
			continue;

		/* rc: -1 = error, 0 = success, 1 = no result */
		rc = chn->driver->probe(pr, chn);

	} while (rc == BLKID_PROBE_NONE);

	if (rc < 0)
	       return BLKID_PROBE_ERROR;

	return rc;
}

#ifdef HAVE_LINUX_BLKZONED_H
static int is_conventional(blkid_probe pr, uint64_t offset)
{
	struct blk_zone_report *rep = NULL;
	int ret;
	uint64_t zone_mask;

	if (!pr->zone_size)
		return 1;

	zone_mask = ~(pr->zone_size - 1);
	rep = blkdev_get_zonereport(blkid_probe_get_fd(pr),
				    (offset & zone_mask) >> 9, 1);
	if (!rep)
		return -1;

	if (rep->zones[0].type == BLK_ZONE_TYPE_CONVENTIONAL)
		ret = 1;
	else
		ret = 0;

	free(rep);

	return ret;
}
#else
static inline int is_conventional(blkid_probe pr __attribute__((__unused__)),
				  uint64_t offset __attribute__((__unused__)))
{
	return 1;
}
#endif

/**
 * blkid_do_wipe:
 * @pr: prober
 * @dryrun: if TRUE then don't touch the device.
 *
 * This function erases the current signature detected by @pr. The @pr has to
 * be open in O_RDWR mode, BLKID_SUBLKS_MAGIC or/and BLKID_PARTS_MAGIC flags
 * has to be enabled (if you want to erase also superblock with broken check
 * sums then use BLKID_SUBLKS_BADCSUM too).
 *
 * After successful signature removing the @pr prober will be moved one step
 * back and the next blkid_do_probe() call will again call previously called
 * probing function. All in-memory cached data from the device are always
 * reset.
 *
 *  <example>
 *  <title>wipe all filesystems or raids from the device</title>
 *   <programlisting>
 *      fd = open(devname, O_RDWR|O_CLOEXEC);
 *      blkid_probe_set_device(pr, fd, 0, 0);
 *
 *      blkid_probe_enable_superblocks(pr, 1);
 *      blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC);
 *
 *	while (blkid_do_probe(pr) == 0)
 *		blkid_do_wipe(pr, FALSE);
 *  </programlisting>
 * </example>
 *
 * See also blkid_probe_step_back() if you cannot use this built-in wipe
 * function, but you want to use libblkid probing as a source for wiping.
 *
 * See also blkid_wipe_all() which works the same as the example above.
 *
 * Returns: 0 on success, and -1 in case of error.
 */
int blkid_do_wipe(blkid_probe pr, int dryrun)
{
	const char *off = NULL;
	size_t len = 0;
	uint64_t offset, magoff;
	int conventional;
	char buf[BUFSIZ];
	int fd, rc = 0;
	struct blkid_chain *chn;

	chn = pr->cur_chain;
	if (!chn)
		return BLKID_PROBE_ERROR;

	switch (chn->driver->id) {
	case BLKID_CHAIN_SUBLKS:
		rc = blkid_probe_lookup_value(pr, "SBMAGIC_OFFSET", &off, NULL);
		if (!rc)
			rc = blkid_probe_lookup_value(pr, "SBMAGIC", NULL, &len);
		break;
	case BLKID_CHAIN_PARTS:
		rc = blkid_probe_lookup_value(pr, "PTMAGIC_OFFSET", &off, NULL);
		if (!rc)
			rc = blkid_probe_lookup_value(pr, "PTMAGIC", NULL, &len);
		break;
	default:
		return BLKID_PROBE_OK;
	}

	if (rc || len == 0 || off == NULL)
		return BLKID_PROBE_OK;

	errno = 0;
	magoff = strtoumax(off, NULL, 10);
	if (errno)
		return BLKID_PROBE_OK;

	offset = magoff + pr->off;
	fd = blkid_probe_get_fd(pr);
	if (fd < 0)
		return BLKID_PROBE_ERROR;

	if (len > sizeof(buf))
		len = sizeof(buf);

	rc = is_conventional(pr, offset);
	if (rc < 0)
		return BLKID_PROBE_ERROR;
	conventional = rc == 1;

	DBG(LOWPROBE, ul_debug(
	    "do_wipe [offset=0x%"PRIx64" (%"PRIu64"), len=%zu, chain=%s, idx=%d, dryrun=%s]\n",
	    offset, offset, len, chn->driver->name, chn->idx, dryrun ? "yes" : "not"));

	if (lseek(fd, offset, SEEK_SET) == (off_t) -1)
		return BLKID_PROBE_ERROR;

	if (!dryrun && len) {
		if (conventional) {
			memset(buf, 0, len);

			/* wipen on device */
			if (write_all(fd, buf, len))
				return BLKID_PROBE_ERROR;
			if (fsync(fd) != 0)
				return BLKID_PROBE_ERROR;
		} else {
#ifdef HAVE_LINUX_BLKZONED_H
			uint64_t zone_mask = ~(pr->zone_size - 1);
			struct blk_zone_range range = {
				.sector = (offset & zone_mask) >> 9,
				.nr_sectors = pr->zone_size >> 9,
			};

			rc = ioctl(fd, BLKRESETZONE, &range);
			if (rc < 0)
				return BLKID_PROBE_ERROR;
#else
			/* Should not reach here */
			assert(0);
#endif
		}

		pr->flags &= ~BLKID_FL_MODIF_BUFF;	/* be paranoid */

		return blkid_probe_step_back(pr);

	}

	if (dryrun) {
		/* wipe in memory only */
		blkid_probe_hide_range(pr, magoff, len);
		return blkid_probe_step_back(pr);
	}

	return BLKID_PROBE_OK;
}

/**
 * blkid_wipe_all:
 * @pr: prober
 *
 * This function erases all detectable signatures from &pr.
 * The @pr has to be open in O_RDWR mode. All other necessary configurations
 * will be enabled automatically.
 *
 *  <example>
 *  <title>wipe all filesystems or raids from the device</title>
 *   <programlisting>
 *      fd = open(devname, O_RDWR|O_CLOEXEC);
 *      blkid_probe_set_device(pr, fd, 0, 0);
 *
 *      blkid_wipe_all(pr);
 *  </programlisting>
 * </example>
 *
 * Returns: 0 on success, and -1 in case of error.
 */
int blkid_wipe_all(blkid_probe pr)
{
	DBG(LOWPROBE, ul_debug("wiping all signatures"));

	blkid_probe_enable_superblocks(pr, 1);
	blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC |
			BLKID_SUBLKS_BADCSUM);

	blkid_probe_enable_partitions(pr, 1);
	blkid_probe_set_partitions_flags(pr, BLKID_PARTS_MAGIC |
			BLKID_PARTS_FORCE_GPT);

	while (blkid_do_probe(pr) == 0) {
		DBG(LOWPROBE, ul_debug("wiping one signature"));
		blkid_do_wipe(pr, 0);
	}

	return BLKID_PROBE_OK;
}

/**
 * blkid_probe_step_back:
 * @pr: prober
 *
 * This function move pointer to the probing chain one step back -- it means
 * that the previously used probing function will be called again in the next
 * blkid_do_probe() call.
 *
 * This is necessary for example if you erase or modify on-disk superblock
 * according to the current libblkid probing result.
 *
 * Note that blkid_probe_hide_range() changes semantic of this function and
 * cached buffers are not reset, but library uses in-memory modified
 * buffers to call the next probing function.
 *
 * <example>
 *  <title>wipe all superblock, but use libblkid only for probing</title>
 *  <programlisting>
 *      pr = blkid_new_probe_from_filename(devname);
 *
 *      blkid_probe_enable_superblocks(pr, 1);
 *      blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC);
 *
 *      blkid_probe_enable_partitions(pr, 1);
 *      blkid_probe_set_partitions_flags(pr, BLKID_PARTS_MAGIC);
 *
 *	while (blkid_do_probe(pr) == BLKID_PROBE_OK) {
 *		const char *ostr = NULL;
 *		size_t len = 0;
 *
 *		// superblocks
 *		if (blkid_probe_lookup_value(pr, "SBMAGIC_OFFSET", &ostr, NULL) == 0)
 *			blkid_probe_lookup_value(pr, "SBMAGIC", NULL, &len);
 *
 *		// partition tables
 *		if (len == 0 && blkid_probe_lookup_value(pr, "PTMAGIC_OFFSET", &ostr, NULL) == 0)
 *			blkid_probe_lookup_value(pr, "PTMAGIC", NULL, &len);
 *
 *		if (!len || !str)
 *			continue;
 *
 *		// convert ostr to the real offset by off = strtoll(ostr, NULL, 10);
 *              // use your stuff to erase @len bytes at the @off
 *              ....
 *
 *		// retry the last probing to check for backup superblocks ..etc.
 *              blkid_probe_step_back(pr);
 *	}
 *  </programlisting>
 * </example>
 *
 * Returns: 0 on success, and -1 in case of error.
 */
int blkid_probe_step_back(blkid_probe pr)
{
	struct blkid_chain *chn;

	chn = pr->cur_chain;
	if (!chn)
		return -1;

	if (!(pr->flags & BLKID_FL_MODIF_BUFF))
		blkid_probe_reset_buffers(pr);

	if (chn->idx >= 0) {
		chn->idx--;
		DBG(LOWPROBE, ul_debug("step back: moving %s chain index to %d",
			chn->driver->name,
			chn->idx));
	}

	if (chn->idx == -1) {
		/* blkid_do_probe() goes to the next chain if the index
		 * of the current chain is -1, so we have to set the
		 * chain pointer to the previous chain.
		 */
		size_t idx = chn->driver->id > 0 ? chn->driver->id - 1 : 0;

		DBG(LOWPROBE, ul_debug("step back: moving to previous chain"));

		if (idx > 0)
			pr->cur_chain = &pr->chains[idx];
		else if (idx == 0)
			pr->cur_chain = NULL;
	}

	return 0;
}

/**
 * blkid_do_safeprobe:
 * @pr: prober
 *
 * This function gathers probing results from all enabled chains and checks
 * for ambivalent results (e.g. more filesystems on the device).
 *
 * This is string-based NAME=value interface only.
 *
 * Note about superblocks chain -- the function does not check for filesystems
 * when a RAID signature is detected.  The function also does not check for
 * collision between RAIDs. The first detected RAID is returned. The function
 * checks for collision between partition table and RAID signature -- it's
 * recommended to enable partitions chain together with superblocks chain.
 *
 * Returns: 0 on success, 1 if nothing is detected, -2 if ambivalent result is
 * detected and -1 on case of error.
 */
int blkid_do_safeprobe(blkid_probe pr)
{
	int i, count = 0, rc = 0;

	if (pr->flags & BLKID_FL_NOSCAN_DEV)
		return BLKID_PROBE_NONE;

	blkid_probe_start(pr);

	for (i = 0; i < BLKID_NCHAINS; i++) {
		struct blkid_chain *chn;

		chn = pr->cur_chain = &pr->chains[i];
		chn->binary = FALSE;		/* for sure... */

		DBG(LOWPROBE, ul_debug("chain safeprobe %s %s",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED"));

		if (!chn->enabled)
			continue;

		blkid_probe_chain_reset_position(chn);

		rc = chn->driver->safeprobe(pr, chn);

		blkid_probe_chain_reset_position(chn);

		/* rc: -2 ambivalent, -1 = error, 0 = success, 1 = no result */
		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;	/* success */
	}

done:
	blkid_probe_end(pr);
	if (rc < 0)
		return BLKID_PROBE_ERROR;

	return count == 0 ? BLKID_PROBE_NONE : BLKID_PROBE_OK;
}

/**
 * blkid_do_fullprobe:
 * @pr: prober
 *
 * This function gathers probing results from all enabled chains. Same as
 * blkid_do_safeprobe() but does not check for collision between probing
 * result.
 *
 * This is string-based NAME=value interface only.
 *
 * Returns: 0 on success, 1 if nothing is detected or -1 on case of error.
 */
int blkid_do_fullprobe(blkid_probe pr)
{
	int i, count = 0, rc = 0;

	if (pr->flags & BLKID_FL_NOSCAN_DEV)
		return BLKID_PROBE_NONE;

	blkid_probe_start(pr);

	for (i = 0; i < BLKID_NCHAINS; i++) {
		struct blkid_chain *chn;

		chn = pr->cur_chain = &pr->chains[i];
		chn->binary = FALSE;		/* for sure... */

		DBG(LOWPROBE, ul_debug("chain fullprobe %s: %s",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED"));

		if (!chn->enabled)
			continue;

		blkid_probe_chain_reset_position(chn);

		rc = chn->driver->probe(pr, chn);

		blkid_probe_chain_reset_position(chn);

		/* rc: -1 = error, 0 = success, 1 = no result */
		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;	/* success */
	}

done:
	blkid_probe_end(pr);
	if (rc < 0)
		return BLKID_PROBE_ERROR;

	return count == 0 ? BLKID_PROBE_NONE : BLKID_PROBE_OK;
}

/* same sa blkid_probe_get_buffer() but works with 512-sectors */
const unsigned char *blkid_probe_get_sector(blkid_probe pr, unsigned int sector)
{
	return blkid_probe_get_buffer(pr, ((uint64_t) sector) << 9, 0x200);
}

struct blkid_prval *blkid_probe_assign_value(blkid_probe pr, const char *name)
{
	struct blkid_prval *v;

	v = calloc(1, sizeof(struct blkid_prval));
	if (!v)
		return NULL;

	INIT_LIST_HEAD(&v->prvals);
	v->name = name;
	v->chain = pr->cur_chain;
	list_add_tail(&v->prvals, &pr->values);

	DBG(LOWPROBE, ul_debug("assigning %s [%s]", name, v->chain->driver->name));
	return v;
}

/* Note that value data is always terminated by zero to keep things robust,
 * this extra zero is not count to the value length. It's caller responsibility
 * to set proper value length (for strings we count terminator to the length,
 * for binary data it's without terminator).
 */
int blkid_probe_value_set_data(struct blkid_prval *v,
		const unsigned char *data, size_t len)
{
	v->data = calloc(1, len + 1);	/* always terminate by \0 */

	if (!v->data)
		return -ENOMEM;
	memcpy(v->data, data, len);
	v->len = len;
	return 0;
}

int blkid_probe_set_value(blkid_probe pr, const char *name,
		const unsigned char *data, size_t len)
{
	struct blkid_prval *v;

	v = blkid_probe_assign_value(pr, name);
	if (!v)
		return -1;

	return blkid_probe_value_set_data(v, data, len);
}

int blkid_probe_vsprintf_value(blkid_probe pr, const char *name,
		const char *fmt, va_list ap)
{
	struct blkid_prval *v;
	ssize_t len;

	v = blkid_probe_assign_value(pr, name);
	if (!v)
		return -ENOMEM;

	len = vasprintf((char **) &v->data, fmt, ap);

	if (len <= 0) {
		blkid_probe_free_value(v);
		return len == 0 ? -EINVAL : -ENOMEM;
	}
	v->len = len + 1;
	return 0;
}

int blkid_probe_sprintf_value(blkid_probe pr, const char *name,
		const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start(ap, fmt);
	rc = blkid_probe_vsprintf_value(pr, name, fmt, ap);
	va_end(ap);

	return rc;
}

int blkid_probe_set_magic(blkid_probe pr, uint64_t offset,
			size_t len, const unsigned char *magic)
{
	int rc = 0;
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (!chn || !len || chn->binary)
		return 0;

	switch (chn->driver->id) {
	case BLKID_CHAIN_SUBLKS:
		if (!(chn->flags & BLKID_SUBLKS_MAGIC))
			return 0;
		rc = blkid_probe_set_value(pr, "SBMAGIC", magic, len);
		if (!rc)
			rc = blkid_probe_sprintf_value(pr,
					"SBMAGIC_OFFSET", "%llu", (unsigned long long)offset);
		break;
	case BLKID_CHAIN_PARTS:
		if (!(chn->flags & BLKID_PARTS_MAGIC))
			return 0;
		rc = blkid_probe_set_value(pr, "PTMAGIC", magic, len);
		if (!rc)
			rc = blkid_probe_sprintf_value(pr,
					"PTMAGIC_OFFSET", "%llu", (unsigned long long)offset);
		break;
	default:
		break;
	}

	return rc;
}

static void blkid_probe_log_csum_mismatch(blkid_probe pr, size_t n, const void *csum,
		const void *expected)
{
	char csum_hex[256];
	char expected_hex[sizeof(csum_hex)];
	int hex_size = min(sizeof(csum_hex), n * 2);

	for (int i = 0; i < hex_size; i+=2) {
		sprintf(&csum_hex[i], "%02X", ((const unsigned char *) csum)[i / 2]);
		sprintf(&expected_hex[i], "%02X", ((const unsigned char *) expected)[i / 2]);
	}

	ul_debug(
		"incorrect checksum for type %s,"
		" got %*s, expected %*s",
		blkid_probe_get_probername(pr),
		hex_size, csum_hex, hex_size, expected_hex);
}


int blkid_probe_verify_csum_buf(blkid_probe pr, size_t n, const void *csum,
		const void *expected)
{
	if (memcmp(csum, expected, n) != 0) {
		struct blkid_chain *chn = blkid_probe_get_chain(pr);

		ON_DBG(LOWPROBE, blkid_probe_log_csum_mismatch(pr, n, csum, expected));

		/*
		 * Accept bad checksum if BLKID_SUBLKS_BADCSUM flags is set
		 */
		if (chn && chn->driver->id == BLKID_CHAIN_SUBLKS
		    && (chn->flags & BLKID_SUBLKS_BADCSUM)) {
			blkid_probe_set_value(pr, "SBBADCSUM", (unsigned char *) "1", 2);
			goto accept;
		}
		return 0;	/* bad checksum */
	}

accept:
	return 1;
}

int blkid_probe_verify_csum(blkid_probe pr, uint64_t csum, uint64_t expected)
{
	return blkid_probe_verify_csum_buf(pr, sizeof(csum), &csum, &expected);
}

/**
 * blkid_probe_get_devno:
 * @pr: probe
 *
 * Returns: block device number, or 0 for regular files.
 */
dev_t blkid_probe_get_devno(blkid_probe pr)
{
	return pr->devno;
}

/**
 * blkid_probe_get_wholedisk_devno:
 * @pr: probe
 *
 * Returns: device number of the wholedisk, or 0 for regular files.
 */
dev_t blkid_probe_get_wholedisk_devno(blkid_probe pr)
{
	if (!pr->disk_devno) {
		dev_t devno, disk_devno = 0;

		devno = blkid_probe_get_devno(pr);
		if (!devno)
			return 0;

		if (blkid_devno_to_wholedisk(devno, NULL, 0, &disk_devno) == 0)
			pr->disk_devno = disk_devno;
	}
	return pr->disk_devno;
}

/**
 * blkid_probe_is_wholedisk:
 * @pr: probe
 *
 * Returns: 1 if the device is whole-disk or 0.
 */
int blkid_probe_is_wholedisk(blkid_probe pr)
{
	dev_t devno, disk_devno;

	devno = blkid_probe_get_devno(pr);
	if (!devno)
		return 0;

	disk_devno = blkid_probe_get_wholedisk_devno(pr);
	if (!disk_devno)
		return 0;

	return devno == disk_devno;
}

blkid_probe blkid_probe_get_wholedisk_probe(blkid_probe pr)
{
	dev_t disk;

	if (blkid_probe_is_wholedisk(pr))
		return NULL;			/* this is not partition */

	if (pr->parent)
		/* this is cloned blkid_probe, use parent's stuff */
		return blkid_probe_get_wholedisk_probe(pr->parent);

	disk = blkid_probe_get_wholedisk_devno(pr);

	if (pr->disk_probe && pr->disk_probe->devno != disk) {
		/* we have disk prober, but for another disk... close it */
		blkid_free_probe(pr->disk_probe);
		pr->disk_probe = NULL;
	}

	if (!pr->disk_probe) {
		/* Open a new disk prober */
		char *disk_path = blkid_devno_to_devname(disk);
		int flags;

		if (!disk_path)
			return NULL;

		DBG(LOWPROBE, ul_debug("allocate a wholedisk probe"));

		pr->disk_probe = blkid_new_probe_from_filename(disk_path);

		free(disk_path);

		if (!pr->disk_probe)
			return NULL;	/* ENOMEM? */

		flags = blkid_probe_get_partitions_flags(pr);
		if (flags & BLKID_PARTS_FORCE_GPT)
			blkid_probe_set_partitions_flags(pr->disk_probe,
							 BLKID_PARTS_FORCE_GPT);
	}

	return pr->disk_probe;
}

/**
 * blkid_probe_get_size:
 * @pr: probe
 *
 * This function returns size of probing area as defined by blkid_probe_set_device().
 * If the size of the probing area is unrestricted then this function returns
 * the real size of device. See also blkid_get_dev_size().
 *
 * Returns: size in bytes or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_size(blkid_probe pr)
{
	return (blkid_loff_t) pr->size;
}

/**
 * blkid_probe_get_offset:
 * @pr: probe
 *
 * This function returns offset of probing area as defined by blkid_probe_set_device().
 *
 * Returns: offset in bytes or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_offset(blkid_probe pr)
{
	return (blkid_loff_t) pr->off;
}

/**
 * blkid_probe_get_fd:
 * @pr: probe
 *
 * Returns: file descriptor for assigned device/file or -1 in case of error.
 */
int blkid_probe_get_fd(blkid_probe pr)
{
	return pr->fd;
}

/**
 * blkid_probe_get_sectorsize:
 * @pr: probe or NULL (for NULL returns 512)
 *
 * Returns: block device logical sector size (BLKSSZGET ioctl, default 512).
 */
unsigned int blkid_probe_get_sectorsize(blkid_probe pr)
{
	if (pr->blkssz)
		return pr->blkssz;

	if (S_ISBLK(pr->mode) &&
	    blkdev_get_sector_size(pr->fd, (int *) &pr->blkssz) == 0)
		return pr->blkssz;

	pr->blkssz = DEFAULT_SECTOR_SIZE;
	return pr->blkssz;
}

/**
 * blkid_probe_set_sectorsize:
 * @pr: probe
 * @sz: new size (to overwrite system default)
 *
 * Note that blkid_probe_set_device() resets this setting. Use it after
 * blkid_probe_set_device() and before any probing call.
 *
 * Since: 2.30
 *
 * Returns: 0 or <0 in case of error
 */
int blkid_probe_set_sectorsize(blkid_probe pr, unsigned int sz)
{
	pr->blkssz = sz;
	return 0;
}

/**
 * blkid_probe_get_sectors:
 * @pr: probe
 *
 * Returns: 512-byte sector count or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_sectors(blkid_probe pr)
{
	return (blkid_loff_t) (pr->size >> 9);
}

/**
 * blkid_probe_numof_values:
 * @pr: probe
 *
 * Returns: number of values in probing result or -1 in case of error.
 */
int blkid_probe_numof_values(blkid_probe pr)
{
	int i = 0;
	struct list_head *p;

	list_for_each(p, &pr->values)
		++i;
	return i;
}

/**
 * blkid_probe_get_value:
 * @pr: probe
 * @num: wanted value in range 0..N, where N is blkid_probe_numof_values() - 1
 * @name: pointer to return value name or NULL
 * @data: pointer to return value data or NULL
 * @len: pointer to return value length or NULL
 *
 * Note, the @len returns length of the @data, including the terminating
 * '\0' character.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_get_value(blkid_probe pr, int num, const char **name,
			const char **data, size_t *len)
{
	struct blkid_prval *v = __blkid_probe_get_value(pr, num);

	if (!v)
		return -1;
	if (name)
		*name = v->name;
	if (data)
		*data = (char *) v->data;
	if (len)
		*len = v->len;

	DBG(LOWPROBE, ul_debug("returning %s value", v->name));
	return 0;
}

/**
 * blkid_probe_lookup_value:
 * @pr: probe
 * @name: name of value
 * @data: pointer to return value data or NULL
 * @len: pointer to return value length or NULL
 *
 * Note, the @len returns length of the @data, including the terminating
 * '\0' character.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_lookup_value(blkid_probe pr, const char *name,
			const char **data, size_t *len)
{
	struct blkid_prval *v = __blkid_probe_lookup_value(pr, name);

	if (!v)
		return -1;
	if (data)
		*data = (char *) v->data;
	if (len)
		*len = v->len;
	return 0;
}

/**
 * blkid_probe_has_value:
 * @pr: probe
 * @name: name of value
 *
 * Returns: 1 if value exist in probing result, otherwise 0.
 */
int blkid_probe_has_value(blkid_probe pr, const char *name)
{
	if (blkid_probe_lookup_value(pr, name, NULL, NULL) == 0)
		return 1;
	return 0;
}

struct blkid_prval *__blkid_probe_get_value(blkid_probe pr, int num)
{
	int i = 0;
	struct list_head *p;

	if (num < 0)
		return NULL;

	list_for_each(p, &pr->values) {
		if (i++ != num)
			continue;
		return list_entry(p, struct blkid_prval, prvals);
	}
	return NULL;
}

struct blkid_prval *__blkid_probe_lookup_value(blkid_probe pr, const char *name)
{
	struct list_head *p;

	if (list_empty(&pr->values))
		return NULL;

	list_for_each(p, &pr->values) {
		struct blkid_prval *v = list_entry(p, struct blkid_prval,
						prvals);

		if (v->name && strcmp(name, v->name) == 0) {
			DBG(LOWPROBE, ul_debug("returning %s value", v->name));
			return v;
		}
	}
	return NULL;
}


/* converts DCE UUID (uuid[16]) to human readable string
 * - the @len should be always 37 */
void blkid_unparse_uuid(const unsigned char *uuid, char *str, size_t len)
{
	snprintf(str, len,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5],
		uuid[6], uuid[7],
		uuid[8], uuid[9],
		uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],uuid[15]);
}

/* like uuid_is_null() from libuuid, but works with arbitrary size of UUID */
int blkid_uuid_is_empty(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (buf[i])
			return 0;
	return 1;
}

/* Removes whitespace from the right-hand side of a string (trailing
 * whitespace).
 *
 * Returns size of the new string (without \0).
 */
size_t blkid_rtrim_whitespace(unsigned char *str)
{
	return rtrim_whitespace(str);
}

/* Removes whitespace from the left-hand side of a string.
 *
 * Returns size of the new string (without \0).
 */
size_t blkid_ltrim_whitespace(unsigned char *str)
{
	return ltrim_whitespace(str);
}

/*
 * Some mkfs-like utils wipe some parts (usually begin) of the device.
 * For example LVM (pvcreate) or mkswap(8). This information could be used
 * for later resolution to conflicts between superblocks.
 *
 * For example we found valid LVM superblock, LVM wipes 8KiB at the begin of
 * the device. If we found another signature (for example MBR) within the
 * wiped area then the signature has been added later and LVM superblock
 * should be ignore.
 *
 * Note that this heuristic is not 100% reliable, for example "pvcreate --zero n"
 * can be used to keep the begin of the device unmodified. It's probably better
 * to use this heuristic for conflicts between superblocks and partition tables
 * than for conflicts between filesystem superblocks -- existence of unwanted
 * partition table is very unusual, because PT is pretty visible (parsed and
 * interpreted by kernel).
 *
 * Note that we usually expect only one signature on the device, it means that
 * we have to remember only one wiped area from previously successfully
 * detected signature.
 *
 * blkid_probe_set_wiper() -- defines wiped area (e.g. LVM)
 * blkid_probe_use_wiper() -- try to use area (e.g. MBR)
 *
 * Note that there is not relation between _wiper and blkid_to_wipe().
 *
 */
void blkid_probe_set_wiper(blkid_probe pr, uint64_t off, uint64_t size)
{
	struct blkid_chain *chn;

	if (!size) {
		DBG(LOWPROBE, ul_debug("zeroize wiper"));
		pr->wipe_size = pr->wipe_off = 0;
		pr->wipe_chain = NULL;
		return;
	}

	chn = pr->cur_chain;

	if (!chn || !chn->driver ||
	    chn->idx < 0 || (size_t) chn->idx >= chn->driver->nidinfos)
		return;

	pr->wipe_size = size;
	pr->wipe_off = off;
	pr->wipe_chain = chn;

	DBG(LOWPROBE,
		ul_debug("wiper set to %s::%s off=%"PRIu64" size=%"PRIu64"",
			chn->driver->name,
			chn->driver->idinfos[chn->idx]->name,
			pr->wipe_off, pr->wipe_size));
}

/*
 * Returns 1 if the <@off,@size> area was wiped
 */
int blkid_probe_is_wiped(blkid_probe pr, struct blkid_chain **chn, uint64_t off, uint64_t size)
{
	if (!size)
		return 0;

	if (pr->wipe_off <= off && off + size <= pr->wipe_off + pr->wipe_size) {
		*chn = pr->wipe_chain;
		return 1;
	}
	return 0;
}

/*
 *  Try to use any area -- if the area has been previously wiped then the
 *  previous probing result should be ignored (reset).
 */
void blkid_probe_use_wiper(blkid_probe pr, uint64_t off, uint64_t size)
{
	struct blkid_chain *chn = NULL;

	if (blkid_probe_is_wiped(pr, &chn, off, size) && chn) {
		DBG(LOWPROBE, ul_debug("previously wiped area modified "
				       " -- ignore previous results"));
		blkid_probe_set_wiper(pr, 0, 0);
		blkid_probe_chain_reset_values(pr, chn);
	}
}

static struct blkid_hint *get_hint(blkid_probe pr, const char *name)
{
	struct list_head *p;

	if (list_empty(&pr->hints))
		return NULL;

	list_for_each(p, &pr->hints) {
		struct blkid_hint *h = list_entry(p, struct blkid_hint, hints);

		if (h->name && strcmp(name, h->name) == 0)
			return h;
	}
	return NULL;
}

/**
 * blkid_probe_set_hint:
 * @pr: probe
 * @name: hint name or NAME=value
 * @value: offset or another number
 *
 * Sets extra hint for low-level prober. If the hint is set by NAME=value
 * notation than @value is ignored. The functions blkid_probe_set_device()
 * and blkid_reset_probe() resets all hints.
 *
 * The hints are optional way how to force libblkid probing functions to check
 * for example another location.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_set_hint(blkid_probe pr, const char *name, uint64_t value)
{
	struct blkid_hint *hint = NULL;
	char *n = NULL, *v = NULL;

	if (strchr(name, '=')) {
		char *end = NULL;

		if (blkid_parse_tag_string(name, &n, &v) != 0)
			goto done;

		errno = 0;
		value = strtoumax(v, &end, 10);

		if (errno || v == end || (end && *end))
			goto done;
	}

	hint = get_hint(pr, n ? n : name);
	if (hint) {
		/* alter old hint */
		hint->value = value;
		DBG(LOWPROBE,
			ul_debug("updated hint '%s' to %"PRIu64"", hint->name, hint->value));
	} else {
		/* add a new hint */
		if (!n) {
			n = strdup(name);
			if (!n)
				goto done;
		}
		hint = malloc(sizeof(*hint));
		if (!hint)
			goto done;

		hint->name = n;
		hint->value = value;

		INIT_LIST_HEAD(&hint->hints);
		list_add_tail(&hint->hints, &pr->hints);

		DBG(LOWPROBE,
			ul_debug("new hint '%s' is %"PRIu64"", hint->name, hint->value));
		n = NULL;
	}
done:
	free(n);
	free(v);

	if (!hint)
		return errno ? -errno : -EINVAL;
	return 0;
}

int blkid_probe_get_hint(blkid_probe pr, const char *name, uint64_t *value)
{
	struct blkid_hint *h = get_hint(pr, name);

	if (!h)
		return -EINVAL;
	if (value)
		*value = h->value;
	return 0;
}

/**
 * blkid_probe_reset_hints:
 * @pr: probe
 *
 * Removes all previously defined probinig hints. See also blkid_probe_set_hint().
 */
void blkid_probe_reset_hints(blkid_probe pr)
{
	if (list_empty(&pr->hints))
		return;

	DBG(LOWPROBE, ul_debug("resetting hints"));

	while (!list_empty(&pr->hints)) {
		struct blkid_hint *h = list_entry(pr->hints.next,
						struct blkid_hint, hints);
		list_del(&h->hints);
		free(h->name);
		free(h);
	}

	INIT_LIST_HEAD(&pr->hints);
}
