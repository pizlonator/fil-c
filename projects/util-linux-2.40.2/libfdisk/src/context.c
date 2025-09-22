#ifdef HAVE_LIBBLKID
# include <blkid.h>
#endif

#include "blkdev.h"
#ifdef __linux__
# include "partx.h"
#endif
#include "loopdev.h"
#include "fdiskP.h"

#include "strutils.h"

/**
 * SECTION: context
 * @title: Context
 * @short_description: stores info about device, labels etc.
 *
 * The library distinguish between three types of partitioning objects.
 *
 * on-disk label data
 *    - disk label specific
 *    - probed and read  by disklabel drivers when assign device to the context
 *      or when switch to another disk label type
 *    - only fdisk_write_disklabel() modify on-disk data
 *
 * in-memory label data
 *    - generic data and disklabel specific data stored in struct fdisk_label
 *    - all partitioning operations are based on in-memory data only
 *
 * struct fdisk_partition
 *    - provides abstraction to present partitions to users
 *    - fdisk_partition is possible to gather to fdisk_table container
 *    - used as unified template for new partitions
 *    - used (with fdisk_table) in fdisk scripts
 *    - the struct fdisk_partition is always completely independent object and
 *      any change to the object has no effect to in-memory (or on-disk) label data
 *
 * Don't forget to inform kernel about changes by fdisk_reread_partition_table()
 * or more smart fdisk_reread_changes().
 */

/**
 * fdisk_new_context:
 *
 * Returns: newly allocated libfdisk handler
 */
struct fdisk_context *fdisk_new_context(void)
{
	struct fdisk_context *cxt;

	cxt = calloc(1, sizeof(*cxt));
	if (!cxt)
		return NULL;

	DBG(CXT, ul_debugobj(cxt, "alloc"));
	cxt->dev_fd = -1;
	cxt->refcount = 1;

	INIT_LIST_HEAD(&cxt->wipes);

	/*
	 * Allocate label specific structs.
	 *
	 * This is necessary (for example) to store label specific
	 * context setting.
	 */
	cxt->labels[ cxt->nlabels++ ] = fdisk_new_gpt_label(cxt);
	cxt->labels[ cxt->nlabels++ ] = fdisk_new_dos_label(cxt);
	cxt->labels[ cxt->nlabels++ ] = fdisk_new_bsd_label(cxt);
	cxt->labels[ cxt->nlabels++ ] = fdisk_new_sgi_label(cxt);
	cxt->labels[ cxt->nlabels++ ] = fdisk_new_sun_label(cxt);

	bindtextdomain(LIBFDISK_TEXTDOMAIN, LOCALEDIR);

	return cxt;
}

static int init_nested_from_parent(struct fdisk_context *cxt, int isnew)
{
	struct fdisk_context *parent;

	assert(cxt);
	assert(cxt->parent);

	parent = cxt->parent;

	INIT_LIST_HEAD(&cxt->wipes);

	cxt->alignment_offset = parent->alignment_offset;
	cxt->ask_cb =		parent->ask_cb;
	cxt->ask_data =		parent->ask_data;
	cxt->dev_fd =		parent->dev_fd;
	cxt->first_lba =        parent->first_lba;
	cxt->firstsector_bufsz = parent->firstsector_bufsz;
	cxt->firstsector =	parent->firstsector;
	cxt->geom =		parent->geom;
	cxt->grain =            parent->grain;
	cxt->io_size =          parent->io_size;
	cxt->last_lba =		parent->last_lba;
	cxt->min_io_size =      parent->min_io_size;
	cxt->optimal_io_size =  parent->optimal_io_size;
	cxt->phy_sector_size =  parent->phy_sector_size;
	cxt->readonly =		parent->readonly;
	cxt->script =		parent->script;
	fdisk_ref_script(cxt->script);
	cxt->sector_size =      parent->sector_size;
	cxt->total_sectors =    parent->total_sectors;
	cxt->user_geom =	parent->user_geom;
	cxt->user_log_sector =	parent->user_log_sector;
	cxt->user_pyh_sector =  parent->user_pyh_sector;

	/* parent <--> nested independent setting, initialize for new nested
	 * contexts only */
	if (isnew) {
		cxt->listonly =	parent->listonly;
		cxt->display_details =	parent->display_details;
		cxt->display_in_cyl_units = parent->display_in_cyl_units;
		cxt->protect_bootbits = parent->protect_bootbits;
	}

	free(cxt->dev_model);
	cxt->dev_model = NULL;
	cxt->dev_model_probed = 0;

	return strdup_between_structs(cxt, parent, dev_path);
}

/**
 * fdisk_new_nested_context:
 * @parent: parental context
 * @name: optional label name (e.g. "bsd")
 *
 * Create a new nested fdisk context for nested disk labels (e.g. BSD or PMBR).
 * The function also probes for the nested label on the device if device is
 * already assigned to parent.
 *
 * The new context is initialized according to @parent and both context shares
 * some settings and file descriptor to the device. The child propagate some
 * changes (like fdisk_assign_device()) to parent, but it does not work
 * vice-versa. The behavior is undefined if you assign another device to
 * parent.
 *
 * Returns: new context for nested partition table.
 */
struct fdisk_context *fdisk_new_nested_context(struct fdisk_context *parent,
				const char *name)
{
	struct fdisk_context *cxt;
	struct fdisk_label *lb = NULL;

	assert(parent);

	cxt = calloc(1, sizeof(*cxt));
	if (!cxt)
		return NULL;

	DBG(CXT, ul_debugobj(parent, "alloc nested [%p] [name=%s]", cxt, name));
	cxt->refcount = 1;

	fdisk_ref_context(parent);
	cxt->parent = parent;

	if (init_nested_from_parent(cxt, 1) != 0) {
		cxt->parent = NULL;
		fdisk_unref_context(cxt);
		return NULL;
	}

	if (name) {
		if (strcasecmp(name, "bsd") == 0)
			lb = cxt->labels[ cxt->nlabels++ ] = fdisk_new_bsd_label(cxt);
		else if (strcasecmp(name, "dos") == 0 || strcasecmp(name, "mbr") == 0)
			lb = cxt->labels[ cxt->nlabels++ ] = fdisk_new_dos_label(cxt);
	}

	if (lb && parent->dev_fd >= 0) {
		DBG(CXT, ul_debugobj(cxt, "probing for nested %s", lb->name));

		cxt->label = lb;

		if (lb->op->probe(cxt) == 1)
			__fdisk_switch_label(cxt, lb);
		else {
			DBG(CXT, ul_debugobj(cxt, "not found %s label", lb->name));
			if (lb->op->deinit)
				lb->op->deinit(lb);
			cxt->label = NULL;
		}
	}

	return cxt;
}


/**
 * fdisk_ref_context:
 * @cxt: context pointer
 *
 * Increments reference counter.
 */
void fdisk_ref_context(struct fdisk_context *cxt)
{
	if (cxt)
		cxt->refcount++;
}

/**
 * fdisk_get_label:
 * @cxt: context instance
 * @name: label name (e.g. "gpt")
 *
 * If no @name specified then returns the current context label.
 *
 * The label is allocated and maintained within the context #cxt. There is
 * nothing like reference counting for labels, you cannot deallocate the
 * label.
 *
 * Returns: label struct or NULL in case of error.
 */
struct fdisk_label *fdisk_get_label(struct fdisk_context *cxt, const char *name)
{
	size_t i;

	assert(cxt);

	if (!name)
		return cxt->label;

	if (strcasecmp(name, "mbr") == 0)
		name = "dos";

	for (i = 0; i < cxt->nlabels; i++)
		if (cxt->labels[i]
		    && strcasecmp(cxt->labels[i]->name, name) == 0)
			return cxt->labels[i];

	DBG(CXT, ul_debugobj(cxt, "failed to found %s label driver", name));
	return NULL;
}

/**
 * fdisk_next_label:
 * @cxt: context instance
 * @lb: returns pointer to the next label
 *
 * <informalexample>
 *   <programlisting>
 *      // print all supported labels
 *	struct fdisk_context *cxt = fdisk_new_context();
 *	struct fdisk_label *lb = NULL;
 *
 *	while (fdisk_next_label(cxt, &lb) == 0)
 *		print("label name: %s\n", fdisk_label_get_name(lb));
 *	fdisk_unref_context(cxt);
 *   </programlisting>
 * </informalexample>
 *
 * Returns: <0 in case of error, 0 on success, 1 at the end.
 */
int fdisk_next_label(struct fdisk_context *cxt, struct fdisk_label **lb)
{
	size_t i;
	struct fdisk_label *res = NULL;

	if (!lb || !cxt)
		return -EINVAL;

	if (!*lb)
		res = cxt->labels[0];
	else {
		for (i = 1; i < cxt->nlabels; i++) {
			if (*lb == cxt->labels[i - 1]) {
				res = cxt->labels[i];
				break;
			}
		}
	}

	*lb = res;
	return res ? 0 : 1;
}

/**
 * fdisk_get_nlabels:
 * @cxt: context
 *
 * Returns: number of supported label types
 */
size_t fdisk_get_nlabels(struct fdisk_context *cxt)
{
	return cxt ? cxt->nlabels : 0;
}

int __fdisk_switch_label(struct fdisk_context *cxt, struct fdisk_label *lb)
{
	if (!lb || !cxt)
		return -EINVAL;
	if (lb->disabled) {
		DBG(CXT, ul_debugobj(cxt, "*** attempt to switch to disabled label %s -- ignore!", lb->name));
		return -EINVAL;
	}
	cxt->label = lb;
	DBG(CXT, ul_debugobj(cxt, "--> switching context to %s!", lb->name));

	fdisk_apply_label_device_properties(cxt);
	return 0;
}

/**
 * fdisk_has_label:
 * @cxt: fdisk context
 *
 * Returns: return 1 if there is label on the device.
 */
int fdisk_has_label(struct fdisk_context *cxt)
{
	return cxt && cxt->label;
}

/**
 * fdisk_has_protected_bootbits:
 * @cxt: fdisk context
 *
 * Returns: return 1 if boot bits protection enabled.
 */
int fdisk_has_protected_bootbits(struct fdisk_context *cxt)
{
	return cxt && cxt->protect_bootbits;
}

/**
 * fdisk_enable_bootbits_protection:
 * @cxt: fdisk context
 * @enable: 1 or 0
 *
 * The library zeroizes all the first sector when create a new disk label by
 * default.  This function can be used to control this behavior. For now it's
 * supported for MBR and GPT.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_enable_bootbits_protection(struct fdisk_context *cxt, int enable)
{
	if (!cxt)
		return -EINVAL;
	cxt->protect_bootbits = enable ? 1 : 0;
	return 0;
}
/**
 * fdisk_disable_dialogs
 * @cxt: fdisk context
 * @disable: 1 or 0
 *
 * The library uses dialog driven partitioning by default.
 *
 * Returns: 0 on success, < 0 on error.
 *
 * Since: 2.31
 */
int fdisk_disable_dialogs(struct fdisk_context *cxt, int disable)
{
	if (!cxt)
		return -EINVAL;

	cxt->no_disalogs = disable;
	return 0;
}

/**
 * fdisk_has_dialogs
 * @cxt: fdisk context
 *
 * See fdisk_disable_dialogs()
 *
 * Returns: 1 if dialog driven partitioning enabled (default), or 0.
 *
 * Since: 2.31
 */
int fdisk_has_dialogs(struct fdisk_context *cxt)
{
	return cxt->no_disalogs == 0;
}

/**
 * fdisk_enable_wipe
 * @cxt: fdisk context
 * @enable: 1 or 0
 *
 * The library removes all PT/filesystem/RAID signatures before it writes
 * partition table. The probing area where it looks for signatures is from
 * the begin of the disk. The device is wiped by libblkid.
 *
 * See also fdisk_wipe_partition().
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_enable_wipe(struct fdisk_context *cxt, int enable)
{
	if (!cxt)
		return -EINVAL;

	fdisk_set_wipe_area(cxt, 0, cxt->total_sectors, enable);
	return 0;
}

/**
 * fdisk_has_wipe
 * @cxt: fdisk context
 *
 * Returns the current wipe setting. See fdisk_enable_wipe().
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_has_wipe(struct fdisk_context *cxt)
{
	if (!cxt)
		return 0;

	return fdisk_has_wipe_area(cxt, 0, cxt->total_sectors);
}


/**
 * fdisk_get_collision
 * @cxt: fdisk context
 *
 * Returns: name of the filesystem or RAID detected on the device or NULL.
 */
const char *fdisk_get_collision(struct fdisk_context *cxt)
{
	return cxt->collision;
}

/**
 * fdisk_is_ptcollision:
 * @cxt: fdisk context
 *
 * The collision detected by libblkid (usually another partition table). Note
 * that libfdisk does not support all partitions tables, so fdisk_has_label()
 * may return false, but fdisk_is_ptcollision() may return true.
 *
 * Since: 2.30
 *
 * Returns: 0 or 1
 */
int fdisk_is_ptcollision(struct fdisk_context *cxt)
{
	return cxt->pt_collision;
}

/**
 * fdisk_get_npartitions:
 * @cxt: context
 *
 * The maximal number of the partitions depends on disklabel and does not
 * have to describe the real limit of PT.
 *
 * For example the limit for MBR without extend partition is 4, with extended
 * partition it's unlimited (so the function returns the current number of all
 * partitions in this case).
 *
 * And for example for GPT it depends on space allocated on disk for array of
 * entry records (usually 128).
 *
 * It's fine to use fdisk_get_npartitions() in loops, but don't forget that
 * partition may be unused (see fdisk_is_partition_used()).
 *
 * <informalexample>
 *   <programlisting>
 *	struct fdisk_partition *pa = NULL;
 *	size_t i, nmax = fdisk_get_npartitions(cxt);
 *
 *	for (i = 0; i < nmax; i++) {
 *		if (!fdisk_is_partition_used(cxt, i))
 *			continue;
 *		... do something ...
 *	}
 *   </programlisting>
 * </informalexample>
 *
 * Note that the recommended way to list partitions is to use
 * fdisk_get_partitions() and struct fdisk_table then ask disk driver for each
 * individual partitions.
 *
 * Returns: maximal number of partitions for the current label.
 */
size_t fdisk_get_npartitions(struct fdisk_context *cxt)
{
	return cxt && cxt->label ? cxt->label->nparts_max : 0;
}

/**
 * fdisk_is_labeltype:
 * @cxt: fdisk context
 * @id: FDISK_DISKLABEL_*
 *
 * See also fdisk_is_label() macro in libfdisk.h.
 *
 * Returns: return 1 if the current label is @id
 */
int fdisk_is_labeltype(struct fdisk_context *cxt, enum fdisk_labeltype id)
{
	assert(cxt);

	return cxt->label && (unsigned)fdisk_label_get_type(cxt->label) == id;
}

/**
 * fdisk_get_parent:
 * @cxt: nested fdisk context
 *
 * Returns: pointer to parental context, or NULL
 */
struct fdisk_context *fdisk_get_parent(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->parent;
}

static void reset_context(struct fdisk_context *cxt)
{
	size_t i;

	DBG(CXT, ul_debugobj(cxt, "*** resetting context"));

	/* reset drives' private data */
	for (i = 0; i < cxt->nlabels; i++)
		fdisk_deinit_label(cxt->labels[i]);

	if (cxt->parent) {
		/* the first sector may be independent on parent */
		if (cxt->parent->firstsector != cxt->firstsector) {
			DBG(CXT, ul_debugobj(cxt, "  firstsector independent on parent (freeing)"));
			free(cxt->firstsector);
		}
	} else {
		/* we close device only in primary context */
		if (cxt->dev_fd > -1 && cxt->is_priv)
			close(cxt->dev_fd);
		DBG(CXT, ul_debugobj(cxt, "  freeing firstsector"));
		free(cxt->firstsector);
	}

	free(cxt->dev_path);
	cxt->dev_path = NULL;

	free(cxt->dev_model);
	cxt->dev_model = NULL;
	cxt->dev_model_probed = 0;

	free(cxt->collision);
	cxt->collision = NULL;

	memset(&cxt->dev_st, 0, sizeof(cxt->dev_st));

	cxt->dev_fd = -1;
	cxt->is_priv = 0;
	cxt->is_excl = 0;
	cxt->firstsector = NULL;
	cxt->firstsector_bufsz = 0;

	fdisk_zeroize_device_properties(cxt);

	fdisk_unref_script(cxt->script);
	cxt->script = NULL;

	cxt->label = NULL;

	fdisk_free_wipe_areas(cxt);
}

/* fdisk_assign_device() body */
static int fdisk_assign_fd(struct fdisk_context *cxt, int fd,
			const char *fname, int readonly,
			int priv, int excl)
{
	assert(cxt);
	assert(fd >= 0);

	errno = 0;

	/* redirect request to parent */
	if (cxt->parent) {
		int rc, org = fdisk_is_listonly(cxt->parent);

		/* assign_device() is sensitive to "listonly" mode, so let's
		 * follow the current context setting for the parent to avoid
		 * unwanted extra warnings. */
		fdisk_enable_listonly(cxt->parent, fdisk_is_listonly(cxt));

		rc = fdisk_assign_fd(cxt->parent, fd, fname, readonly, priv, excl);
		fdisk_enable_listonly(cxt->parent, org);

		if (!rc)
			rc = init_nested_from_parent(cxt, 0);
		if (!rc)
			fdisk_probe_labels(cxt);
		return rc;
	}

	reset_context(cxt);

	if (fstat(fd, &cxt->dev_st) != 0)
		goto fail;

	cxt->readonly = readonly ? 1 : 0;
	cxt->dev_fd = fd;
	cxt->is_priv = priv ? 1 : 0;
	cxt->is_excl = excl ? 1 : 0;

	cxt->dev_path = fname ? strdup(fname) : NULL;
	if (!cxt->dev_path)
		goto fail;

	fdisk_discover_topology(cxt);
	fdisk_discover_geometry(cxt);

	fdisk_apply_user_device_properties(cxt);

	if (fdisk_read_firstsector(cxt) < 0)
		goto fail;

	/* warn about obsolete stuff on the device if we aren't in list-only */
	if (!fdisk_is_listonly(cxt) && fdisk_check_collisions(cxt) < 0)
		goto fail;

	fdisk_probe_labels(cxt);
	fdisk_apply_label_device_properties(cxt);

	/* Don't report collision if there is already a valid partition table.
	 * The bootbits are wiped when we create a *new* partition table only. */
	if (fdisk_is_ptcollision(cxt) && fdisk_has_label(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "ignore old %s", cxt->collision));
		cxt->pt_collision = 0;
		free(cxt->collision);
		cxt->collision = NULL;
	}

	DBG(CXT, ul_debugobj(cxt, "initialized for %s [%s %s %s]",
			      fname,
			      cxt->readonly ? "READ-ONLY" : "READ-WRITE",
			      cxt->is_excl ? "EXCL" : "",
			      cxt->is_priv ? "PRIV" : ""));
	return 0;
fail:
	{
		int rc = errno ? -errno : -EINVAL;
		cxt->dev_fd = -1;
		DBG(CXT, ul_debugobj(cxt, "failed to assign device [rc=%d]", rc));
		return rc;
	}
}

/**
 * fdisk_assign_device:
 * @cxt: context
 * @fname: path to the device to be handled
 * @readonly: how to open the device
 *
 * Open the device, discovery topology, geometry, detect disklabel, check for
 * collisions and switch the current label driver to reflect the probing
 * result.
 *
 * If in standard mode (!= non-listonly mode) then also detects for collisions.
 * The result is accessible by fdisk_get_collision() and
 * fdisk_is_ptcollision().  The collision (e.g. old obsolete PT) may be removed
 * by fdisk_enable_wipe().  Note that new PT and old PT may be on different
 * locations.
 *
 * Note that this function resets all generic setting in context.
 *
 * If the @cxt is nested context (necessary for example to edit BSD or PMBR)
 * then the device is assigned to the parental context and necessary properties
 * are copied to the @cxt. The change is propagated in child->parent direction
 * only. It's impossible to use a different device for primary and nested
 * contexts.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_assign_device(struct fdisk_context *cxt,
			const char *fname, int readonly)
{
	int fd, rc, flags = O_CLOEXEC;

	DBG(CXT, ul_debugobj(cxt, "assigning device %s", fname));
	assert(cxt);

	if (readonly)
		flags |= O_RDONLY;
	else
		flags |= (O_RDWR | O_EXCL);

	errno = 0;
	fd = open(fname,flags);
	if (fd < 0 && errno == EBUSY && (flags & O_EXCL)) {
		flags &= ~O_EXCL;
		errno = 0;
		fd = open(fname, flags);
	}

	if (fd < 0) {
		rc = -errno;
		DBG(CXT, ul_debugobj(cxt, "failed to assign device [rc=%d]", rc));
		return rc;
	}

	rc = fdisk_assign_fd(cxt, fd, fname, readonly, 1, flags & O_EXCL);
	if (rc)
		close(fd);
	return rc;
}

/**
 * fdisk_assign_device_by_fd:
 * @cxt: context
 * @fd: device file descriptor
 * @fname: path to the device (used for dialogs, debugging, partition names, ...)
 * @readonly: how to use the device
 *
 * Like fdisk_assign_device(), but caller is responsible to open and close the
 * device. The library only fsync() the device on fdisk_deassign_device().
 *
 * The device has to be open O_RDWR on @readonly=0.
 *
 * Returns: 0 on success, < 0 on error.
 *
 * Since: 2.35
 */
int fdisk_assign_device_by_fd(struct fdisk_context *cxt, int fd,
			const char *fname, int readonly)
{
	DBG(CXT, ul_debugobj(cxt, "assign by fd"));
	return fdisk_assign_fd(cxt, fd, fname, readonly, 0, 0);
}

/**
 * fdisk_deassign_device:
 * @cxt: context
 * @nosync: disable sync() after close().
 *
 * Call fsync(), close() and than sync(), but for read-only handler close the
 * device only. If the @cxt is nested context then the request is redirected to
 * the parent.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_deassign_device(struct fdisk_context *cxt, int nosync)
{
	assert(cxt);
	assert(cxt->dev_fd >= 0);

	if (cxt->parent) {
		int rc = fdisk_deassign_device(cxt->parent, nosync);

		if (!rc)
			rc = init_nested_from_parent(cxt, 0);
		return rc;
	}

	DBG(CXT, ul_debugobj(cxt, "de-assigning device %s", cxt->dev_path));

	if (cxt->readonly && cxt->is_priv)
		close(cxt->dev_fd);
	else {
		if (fsync(cxt->dev_fd)) {
			fdisk_warn(cxt, _("%s: fsync device failed"),
					cxt->dev_path);
			return -errno;
		}
		if (cxt->is_priv && close(cxt->dev_fd)) {
			fdisk_warn(cxt, _("%s: close device failed"),
					cxt->dev_path);
			return -errno;
		}
		if (S_ISBLK(cxt->dev_st.st_mode) && !nosync) {
			fdisk_info(cxt, _("Syncing disks."));
			sync();
		}
	}

	free(cxt->dev_path);
	cxt->dev_path = NULL;
	cxt->dev_fd = -1;
	cxt->is_priv = 0;
	cxt->is_excl = 0;

	return 0;
}

/**
 * fdisk_reassign_device:
 * @cxt: context
 *
 * This function is "hard reset" of the context and it does not write anything
 * to the device. All in-memory changes associated with the context will be
 * lost. It's recommended to use this function after some fatal problem when the
 * context (and label specific driver) is in an undefined state.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_reassign_device(struct fdisk_context *cxt)
{
	char *devname;
	int rdonly, rc, fd, priv, excl;

	assert(cxt);
	assert(cxt->dev_fd >= 0);

	DBG(CXT, ul_debugobj(cxt, "re-assigning device %s", cxt->dev_path));

	devname = strdup(cxt->dev_path);
	if (!devname)
		return -ENOMEM;

	rdonly = cxt->readonly;
	fd = cxt->dev_fd;
	priv = cxt->is_priv;
	excl = cxt->is_excl;

	fdisk_deassign_device(cxt, 1);

	if (priv)
		/* reopen and assign */
		rc = fdisk_assign_device(cxt, devname, rdonly);
	else
		/* assign only */
		rc = fdisk_assign_fd(cxt, fd, devname, rdonly, priv, excl);

	free(devname);
	return rc;
}

/**
 * fdisk_reread_partition_table:
 * @cxt: context
 *
 * Force *kernel* to re-read partition table on block devices.
 *
 * Returns: 0 on success, < 0 in case of error.
 */
int fdisk_reread_partition_table(struct fdisk_context *cxt)
{
	int i = 0;

	assert(cxt);
	assert(cxt->dev_fd >= 0);

	if (!S_ISBLK(cxt->dev_st.st_mode))
		return 0;

	DBG(CXT, ul_debugobj(cxt, "calling re-read ioctl"));
	sync();
#ifdef BLKRRPART
	fdisk_info(cxt, _("Calling ioctl() to re-read partition table."));
	i = ioctl(cxt->dev_fd, BLKRRPART);
#else
	errno = ENOSYS;
	i = 1;
#endif

	if (i) {
		fdisk_warn(cxt, _("Re-reading the partition table failed."));
		fdisk_info(cxt,	_(
			"The kernel still uses the old table. The "
			"new table will be used at the next reboot "
			"or after you run partprobe(8) or partx(8)."));
		return -errno;
	}

	return 0;
}

#ifdef __linux__
static inline int add_to_partitions_array(
			struct fdisk_partition ***ary,
			struct fdisk_partition *pa,
			size_t *n, size_t nmax)
{
	if (!*ary) {
		*ary = calloc(nmax, sizeof(struct fdisk_partition *));
		if (!*ary)
			return -ENOMEM;
	}
	(*ary)[*n] = pa;
	(*n)++;
	return 0;
}
#endif

/**
 * fdisk_reread_changes:
 * @cxt: context
 * @org: original layout (on disk)
 *
 * Like fdisk_reread_partition_table() but don't forces kernel re-read all
 * partition table. The BLKPG_* ioctls are used for individual partitions. The
 * advantage is that unmodified partitions maybe mounted.
 *
 * The function behaves like fdisk_reread_partition_table() on systems where
 * are no available BLKPG_* ioctls.
 *
 * Returns: <0 on error, or 0.
 */
#ifdef __linux__
int fdisk_reread_changes(struct fdisk_context *cxt, struct fdisk_table *org)
{
	struct fdisk_table *tb = NULL;
	struct fdisk_iter itr;
	struct fdisk_partition *pa;
	struct fdisk_partition **rem = NULL, **add = NULL, **upd = NULL;
	int change, rc = 0, err = 0;
	size_t nparts, i, nadds = 0, nupds = 0, nrems = 0;
	unsigned int ssf;

	DBG(CXT, ul_debugobj(cxt, "rereading changes"));

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	/* the current layout */
	fdisk_get_partitions(cxt, &tb);
	/* maximal number of partitions */
	nparts = max(fdisk_table_get_nents(tb), fdisk_table_get_nents(org));

	while (fdisk_diff_tables(org, tb, &itr, &pa, &change) == 0) {
		if (change == FDISK_DIFF_UNCHANGED)
			continue;
		switch (change) {
		case FDISK_DIFF_REMOVED:
			rc = add_to_partitions_array(&rem, pa, &nrems, nparts);
			break;
		case FDISK_DIFF_ADDED:
			rc = add_to_partitions_array(&add, pa, &nadds, nparts);
			break;
		case FDISK_DIFF_RESIZED:
			rc = add_to_partitions_array(&upd, pa, &nupds, nparts);
			break;
		case FDISK_DIFF_MOVED:
			rc = add_to_partitions_array(&rem, pa, &nrems, nparts);
			if (!rc)
				rc = add_to_partitions_array(&add, pa, &nadds, nparts);
			break;
		}
		if (rc != 0)
			goto done;
	}

	/* sector size factor -- used to recount from real to 512-byte sectors */
	ssf = cxt->sector_size / 512;

	for (i = 0; i < nrems; i++) {
		pa = rem[i];
		DBG(PART, ul_debugobj(pa, "#%zu calling BLKPG_DEL_PARTITION", pa->partno));
		if (partx_del_partition(cxt->dev_fd, pa->partno + 1) != 0) {
			fdisk_warn(cxt, _("Failed to remove partition %zu from system"), pa->partno + 1);
			err++;
		}
	}
	for (i = 0; i < nupds; i++) {
		pa = upd[i];
		DBG(PART, ul_debugobj(pa, "#%zu calling BLKPG_RESIZE_PARTITION", pa->partno));
		if (partx_resize_partition(cxt->dev_fd, pa->partno + 1,
					   pa->start * ssf, pa->size * ssf) != 0) {
			fdisk_warn(cxt, _("Failed to update system information about partition %zu"), pa->partno + 1);
			err++;
		}
	}
	for (i = 0; i < nadds; i++) {
		uint64_t sz;

		pa = add[i];
		sz = pa->size * ssf;

		DBG(PART, ul_debugobj(pa, "#%zu calling BLKPG_ADD_PARTITION", pa->partno));

		if (fdisk_is_label(cxt, DOS) && fdisk_partition_is_container(pa))
			/* Let's follow the Linux kernel and reduce
                         * DOS extended partition to 1 or 2 sectors.
			 */
			sz = min(sz, (uint64_t) 2);

		if (partx_add_partition(cxt->dev_fd, pa->partno + 1,
					pa->start * ssf, sz) != 0) {
			fdisk_warn(cxt, _("Failed to add partition %zu to system"), pa->partno + 1);
			err++;
		}
	}
	if (err)
		fdisk_info(cxt,	_(
			"The kernel still uses the old partitions. The new "
			"table will be used at the next reboot. "));
done:
	free(rem);
	free(add);
	free(upd);
	fdisk_unref_table(tb);
	return rc;
}
#else
int fdisk_reread_changes(struct fdisk_context *cxt,
			 struct fdisk_table *org __attribute__((__unused__))) {
	return fdisk_reread_partition_table(cxt);
}
#endif

/**
 * fdisk_device_is_used:
 * @cxt: context
 *
 * The function returns always 0 if the device has not been opened by
 * fdisk_assign_device() or if open read-only.
 *
 * Returns: 1 if the device assigned to the context is used by system, or 0.
 */
int fdisk_device_is_used(struct fdisk_context *cxt)
{
	int rc;
	assert(cxt);
	assert(cxt->dev_fd >= 0);

	rc = cxt->readonly ? 0 :
	     cxt->is_excl ? 0 :
	     cxt->is_priv ? 1 : 0;

	DBG(CXT, ul_debugobj(cxt, "device used: %s [read-only=%d, excl=%d, priv:%d]",
				rc ? "TRUE" : "FALSE", cxt->readonly,
				cxt->is_excl, cxt->is_priv));
	return rc;
}

/**
 * fdisk_is_readonly:
 * @cxt: context
 *
 * Returns: 1 if device open readonly
 */
int fdisk_is_readonly(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->readonly;
}

/**
 * fdisk_is_regfile:
 * @cxt: context
 *
 * Since: 2.30
 *
 * Returns: 1 if open file descriptor is regular file rather than a block device.
 */
int fdisk_is_regfile(struct fdisk_context *cxt)
{
	assert(cxt);
	return S_ISREG(cxt->dev_st.st_mode);
}

/**
 * fdisk_unref_context:
 * @cxt: fdisk context
 *
 * Deallocates context struct.
 */
void fdisk_unref_context(struct fdisk_context *cxt)
{
	unsigned i;

	if (!cxt)
		return;

	cxt->refcount--;
	if (cxt->refcount <= 0) {
		DBG(CXT, ul_debugobj(cxt, "freeing context %p for %s", cxt, cxt->dev_path));

		reset_context(cxt);	/* this is sensitive to parent<->child relationship! */

		/* deallocate label's private stuff */
		for (i = 0; i < cxt->nlabels; i++) {
			if (!cxt->labels[i])
				continue;
			if (cxt->labels[i]->op->free)
				cxt->labels[i]->op->free(cxt->labels[i]);
			else
				free(cxt->labels[i]);
			cxt->labels[i] = NULL;
		}

		fdisk_unref_context(cxt->parent);
		cxt->parent = NULL;

		free(cxt);
	}
}


/**
 * fdisk_enable_details:
 * @cxt: context
 * @enable: true/false
 *
 * Enables or disables "details" display mode. This function has effect to
 * fdisk_partition_to_string() function.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_enable_details(struct fdisk_context *cxt, int enable)
{
	assert(cxt);
	cxt->display_details = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_is_details:
 * @cxt: context
 *
 * Returns: 1 if details are enabled
 */
int fdisk_is_details(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->display_details == 1;
}

/**
 * fdisk_enable_listonly:
 * @cxt: context
 * @enable: true/false
 *
 * Just list partition only, don't care about another details, mistakes, ...
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_enable_listonly(struct fdisk_context *cxt, int enable)
{
	assert(cxt);
	cxt->listonly = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_is_listonly:
 * @cxt: context
 *
 * Returns: 1 if list-only mode enabled
 */
int fdisk_is_listonly(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->listonly == 1;
}


/**
 * fdisk_set_unit:
 * @cxt: context
 * @str: "cylinder" or "sector".
 *
 * This is pure shit, unfortunately for example Sun addresses begin of the
 * partition by cylinders...
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_set_unit(struct fdisk_context *cxt, const char *str)
{
	assert(cxt);

	cxt->display_in_cyl_units = 0;

	if (!str)
		return 0;

	if (strcmp(str, "cylinder") == 0 || strcmp(str, "cylinders") == 0)
		cxt->display_in_cyl_units = 1;

	else if (strcmp(str, "sector") == 0 || strcmp(str, "sectors") == 0)
		cxt->display_in_cyl_units = 0;

	DBG(CXT, ul_debugobj(cxt, "display unit: %s", fdisk_get_unit(cxt, 0)));
	return 0;
}

/**
 * fdisk_get_unit:
 * @cxt: context
 * @n: FDISK_PLURAL or FDISK_SINGULAR
 *
 * Returns: unit name.
 */
const char *fdisk_get_unit(struct fdisk_context *cxt, int n)
{
	assert(cxt);

	if (fdisk_use_cylinders(cxt))
		return P_("cylinder", "cylinders", n);
	return P_("sector", "sectors", n);
}

/**
 * fdisk_use_cylinders:
 * @cxt: context
 *
 * Returns: 1 if user wants to display in cylinders.
 */
int fdisk_use_cylinders(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->display_in_cyl_units == 1;
}

/**
 * fdisk_get_units_per_sector:
 * @cxt: context
 *
 * This is necessary only for brain dead situations when we use "cylinders";
 *
 * Returns: number of "units" per sector, default is 1 if display unit is sector.
 */
unsigned int fdisk_get_units_per_sector(struct fdisk_context *cxt)
{
	assert(cxt);

	if (fdisk_use_cylinders(cxt)) {
		assert(cxt->geom.heads);
		return cxt->geom.heads * cxt->geom.sectors;
	}
	return 1;
}

/**
 * fdisk_get_optimal_iosize:
 * @cxt: context
 *
 * The optimal I/O is optional and does not have to be provided by device,
 * anyway libfdisk never returns zero. If the optimal I/O size is not provided
 * then libfdisk returns minimal I/O size or sector size.
 *
 * Returns: optimal I/O size in bytes.
 */
unsigned long fdisk_get_optimal_iosize(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->optimal_io_size ? cxt->optimal_io_size : cxt->io_size;
}

/**
 * fdisk_get_minimal_iosize:
 * @cxt: context
 *
 * Returns: minimal I/O size in bytes
 */
unsigned long fdisk_get_minimal_iosize(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->min_io_size;
}

/**
 * fdisk_get_physector_size:
 * @cxt: context
 *
 * Returns: physical sector size in bytes
 */
unsigned long fdisk_get_physector_size(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->phy_sector_size;
}

/**
 * fdisk_get_sector_size:
 * @cxt: context
 *
 * Returns: logical sector size in bytes
 */
unsigned long fdisk_get_sector_size(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->sector_size;
}

/**
 * fdisk_get_alignment_offset
 * @cxt: context
 *
 * The alignment offset is offset between logical and physical sectors. For
 * backward compatibility the first logical sector on 4K disks does no have to
 * start on the same place like physical sectors.
 *
 * Returns: alignment offset in bytes
 */
unsigned long fdisk_get_alignment_offset(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->alignment_offset;
}

/**
 * fdisk_get_grain_size:
 * @cxt: context
 *
 * Returns: grain in bytes used to align partitions (usually 1MiB)
 */
unsigned long fdisk_get_grain_size(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->grain;
}

/**
 * fdisk_get_first_lba:
 * @cxt: context
 *
 * Returns: first possible LBA on disk for data partitions.
 */
fdisk_sector_t fdisk_get_first_lba(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->first_lba;
}

/**
 * fdisk_set_first_lba:
 * @cxt: fdisk context
 * @lba: first possible logical sector for data
 *
 * It's strongly recommended to use the default library setting. The first LBA
 * is always reset by fdisk_assign_device(), fdisk_override_geometry()
 * and fdisk_reset_alignment(). This is very low level function and library
 * does not check if your setting makes any sense.
 *
 * This function is necessary only when you want to work with very unusual
 * partition tables like GPT protective MBR or hybrid partition tables on
 * bootable media where the first partition may start on very crazy offsets.
 *
 * Note that this function changes only runtime information. It does not update
 * any range in on-disk partition table. For example GPT Header contains First
 * and Last usable LBA fields. These fields are not updated by this function.
 * Be careful.
 *
 * Returns: 0 on success, <0 on error.
 */
fdisk_sector_t fdisk_set_first_lba(struct fdisk_context *cxt, fdisk_sector_t lba)
{
	assert(cxt);
	DBG(CXT, ul_debugobj(cxt, "setting first LBA from %ju to %ju",
			(uintmax_t) cxt->first_lba, (uintmax_t) lba));
	cxt->first_lba = lba;
	return 0;
}

/**
 * fdisk_get_last_lba:
 * @cxt: fdisk context
 *
 * Note that the device has to be already assigned.
 *
 * Returns: last possible LBA on device
 */
fdisk_sector_t fdisk_get_last_lba(struct fdisk_context *cxt)
{
	return cxt->last_lba;
}

/**
 * fdisk_set_last_lba:
 * @cxt: fdisk context
 * @lba: last possible logical sector
 *
 * It's strongly recommended to use the default library setting. The last LBA
 * is always reset by fdisk_assign_device(), fdisk_override_geometry() and
 * fdisk_reset_alignment().
 *
 * The default is number of sectors on the device, but maybe modified by the
 * current disklabel driver (for example GPT uses the end of disk for backup
 * header, so last_lba is smaller than total number of sectors).
 *
 * Returns: 0 on success, <0 on error.
 */
fdisk_sector_t fdisk_set_last_lba(struct fdisk_context *cxt, fdisk_sector_t lba)
{
	assert(cxt);

	if (lba > cxt->total_sectors - 1 || lba < 1)
		return -ERANGE;
	cxt->last_lba = lba;
	return 0;
}

/**
 * fdisk_set_size_unit:
 * @cxt: fdisk context
 * @unit: FDISK_SIZEUNIT_*
 *
 * Sets unit for SIZE output field (see fdisk_partition_to_string()).
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_set_size_unit(struct fdisk_context *cxt, int unit)
{
	assert(cxt);
	cxt->sizeunit = unit;
	return 0;
}

/**
 * fdisk_get_size_unit:
 * @cxt: fdisk context
 *
 * Gets unit for SIZE output field (see fdisk_partition_to_string()).
 *
 * Returns: unit
 */
int fdisk_get_size_unit(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->sizeunit;
}

/**
 * fdisk_get_nsectors:
 * @cxt: context
 *
 * Returns: size of the device in logical sectors.
 */
fdisk_sector_t fdisk_get_nsectors(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->total_sectors;
}

/**
 * fdisk_get_devname:
 * @cxt: context
 *
 * Returns: device name.
 */
const char *fdisk_get_devname(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->dev_path;
}

/**
 * fdisk_get_devno:
 * @cxt: context
 *
 * Returns: device number or zero for non-block devices
 *
 * Since: 2.33
 */
dev_t fdisk_get_devno(struct fdisk_context *cxt)
{
	assert(cxt);
	return S_ISBLK(cxt->dev_st.st_mode) ? cxt->dev_st.st_rdev : 0;
}

/**
 * fdisk_get_devmodel:
 * @cxt: context
 *
 * Returns: device model string or NULL.
 *
 * Since: 2.33
 */
#ifdef __linux__
const char *fdisk_get_devmodel(struct fdisk_context *cxt)
{
	assert(cxt);

	if (cxt->dev_model_probed)
		return cxt->dev_model;

	if (fdisk_get_devno(cxt)) {
		struct path_cxt *pc = ul_new_sysfs_path(fdisk_get_devno(cxt), NULL, NULL);

		if (pc) {
			ul_path_read_string(pc, &cxt->dev_model, "device/model");
			ul_unref_path(pc);
		}
	}
	cxt->dev_model_probed = 1;
	return cxt->dev_model;
}
#else
const char *fdisk_get_devmodel(struct fdisk_context *cxt __attribute__((__unused__)))
{
	return NULL;
}
#endif

/**
 * fdisk_get_devfd:
 * @cxt: context
 *
 * Returns: device file descriptor.
 */
int fdisk_get_devfd(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->dev_fd;
}

/**
 * fdisk_get_geom_heads:
 * @cxt: context
 *
 * Returns: number of geometry heads.
 */
unsigned int fdisk_get_geom_heads(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->geom.heads;
}
/**
 * fdisk_get_geom_sectors:
 * @cxt: context
 *
 * Returns: number of geometry sectors.
 */
fdisk_sector_t fdisk_get_geom_sectors(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->geom.sectors;

}

/**
 * fdisk_get_geom_cylinders:
 * @cxt: context
 *
 * Returns: number of geometry cylinders
 */
fdisk_sector_t fdisk_get_geom_cylinders(struct fdisk_context *cxt)
{
	assert(cxt);
	return cxt->geom.cylinders;
}

int fdisk_missing_geometry(struct fdisk_context *cxt)
{
	int rc;

	if (!cxt || !cxt->label)
		return 0;

	rc = (fdisk_label_require_geometry(cxt->label) &&
		    (!cxt->geom.heads || !cxt->geom.sectors
				      || !cxt->geom.cylinders));

	if (rc && !fdisk_is_listonly(cxt))
		fdisk_warnx(cxt, _("Incomplete geometry setting."));

	return rc;
}

