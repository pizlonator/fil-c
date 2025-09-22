
#include "c.h"
#include "strutils.h"

#ifdef HAVE_LIBBLKID
# include <blkid.h>
#endif

#include "fdiskP.h"

/**
 * SECTION: partition
 * @title: Partition
 * @short_description: generic label independent partition abstraction
 *
 * The fdisk_partition provides label independent abstraction. The partitions
 * are not directly connected with partition table (label) data. Any change to
 * fdisk_partition does not affects in-memory or on-disk label data.
 *
 * The fdisk_partition is possible to use as a template for
 * fdisk_add_partition() or fdisk_set_partition() operations.
 */

static void init_partition(struct fdisk_partition *pa)
{
	FDISK_INIT_UNDEF(pa->size);
	FDISK_INIT_UNDEF(pa->start);
	FDISK_INIT_UNDEF(pa->partno);
	FDISK_INIT_UNDEF(pa->parent_partno);
	FDISK_INIT_UNDEF(pa->boot);

	INIT_LIST_HEAD(&pa->parts);
}

/**
 * fdisk_new_partition:
 *
 * Returns: new instance.
 */
struct fdisk_partition *fdisk_new_partition(void)
{
	struct fdisk_partition *pa = calloc(1, sizeof(*pa));
	if (!pa)
		return NULL;

	pa->refcount = 1;
	init_partition(pa);
	DBG(PART, ul_debugobj(pa, "alloc"));
	return pa;
}

/**
 * fdisk_reset_partition:
 * @pa: partition
 *
 * Resets partition content.
 */
void fdisk_reset_partition(struct fdisk_partition *pa)
{
	int ref;

	if (!pa)
		return;

	DBG(PART, ul_debugobj(pa, "reset"));
	ref = pa->refcount;

	fdisk_unref_parttype(pa->type);
	free(pa->name);
	free(pa->uuid);
	free(pa->attrs);
	free(pa->fstype);
	free(pa->fsuuid);
	free(pa->fslabel);
	free(pa->start_chs);
	free(pa->end_chs);

	memset(pa, 0, sizeof(*pa));
	pa->refcount = ref;

	init_partition(pa);
}

static struct fdisk_partition *__copy_partition(struct fdisk_partition *o)
{
	struct fdisk_partition *n = fdisk_new_partition();
	int rc;

	if (!n)
		return NULL;

	memcpy(n, o, sizeof(*n));

	/* do not copy reference to lists, etc.*/
	n->refcount = 1;
	INIT_LIST_HEAD(&n->parts);

	if (n->type)
		fdisk_ref_parttype(n->type);

	/* note that strdup_between_structs() deallocates destination pointer,
	 * so make sure it's NULL as we call memcpy() before ... */
	n->name = NULL;
	rc = strdup_between_structs(n, o, name);

	n->uuid = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, uuid);
	n->attrs = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, attrs);
	n->fstype = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, fstype);
	n->fsuuid = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, fsuuid);
	n->fslabel = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, fslabel);
	n->start_chs = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, start_chs);
	n->end_chs = NULL;
	if (!rc)
		rc = strdup_between_structs(n, o, end_chs);

	if (rc) {
		fdisk_unref_partition(n);
		n = NULL;
	}
	return n;
}

/**
 * fdisk_ref_partition:
 * @pa: partition pointer
 *
 * Increments reference counter.
 */
void fdisk_ref_partition(struct fdisk_partition *pa)
{
	if (pa)
		pa->refcount++;
}

/**
 * fdisk_unref_partition:
 * @pa: partition pointer
 *
 * Decrements reference counter, on zero the @pa is automatically
 * deallocated.
 */
void fdisk_unref_partition(struct fdisk_partition *pa)
{
	if (!pa)
		return;

	pa->refcount--;
	if (pa->refcount <= 0) {
		list_del(&pa->parts);
		fdisk_reset_partition(pa);
		DBG(PART, ul_debugobj(pa, "free"));
		free(pa);
	}
}

/**
 * fdisk_partition_set_start:
 * @pa: partition
 * @off: offset in sectors, maximal is UINT64_MAX-1
 *
 * Note that zero is valid offset too. Use fdisk_partition_unset_start() to
 * undefine the offset.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_start(struct fdisk_partition *pa, fdisk_sector_t off)
{
	if (!pa)
		return -EINVAL;
	if (FDISK_IS_UNDEF(off))
		return -ERANGE;
	pa->start = off;
	pa->fs_probed = 0;
	return 0;
}

/**
 * fdisk_partition_unset_start:
 * @pa: partition
 *
 * Sets the size as undefined. See fdisk_partition_has_start().
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_unset_start(struct fdisk_partition *pa)
{
	if (!pa)
		return -EINVAL;
	FDISK_INIT_UNDEF(pa->start);
	pa->fs_probed = 0;
	return 0;
}

/**
 * fdisk_partition_get_start:
 * @pa: partition
 *
 * The zero is also valid offset. The function may return random undefined
 * value when start offset is undefined (for example after
 * fdisk_partition_unset_start()). Always use fdisk_partition_has_start() to be
 * sure that you work with valid numbers.
 *
 * Returns: start offset in sectors
 */
fdisk_sector_t fdisk_partition_get_start(struct fdisk_partition *pa)
{
	return pa->start;
}

/**
 * fdisk_partition_has_start:
 * @pa: partition
 *
 * Returns: 1 or 0
 */
int fdisk_partition_has_start(struct fdisk_partition *pa)
{
	return pa && !FDISK_IS_UNDEF(pa->start);
}


/**
 * fdisk_partition_cmp_start:
 * @a: partition
 * @b: partition
 *
 * Compares partitions according to start offset, See fdisk_table_sort_partitions().
 *
 * Return: 0 if the same, <0 if @b greater, >0 if @a greater.
 */
int fdisk_partition_cmp_start(struct fdisk_partition *a,
			      struct fdisk_partition *b)
{
	int no_a = FDISK_IS_UNDEF(a->start),
	    no_b = FDISK_IS_UNDEF(b->start);

	if (no_a && no_b)
		return 0;
	if (no_a)
		return -1;
	if (no_b)
		return 1;

	return cmp_numbers(a->start, b->start);
}

/**
 * fdisk_partition_start_follow_default
 * @pa: partition
 * @enable: 0|1
 *
 * When @pa used as a template for fdisk_add_partition() when force label driver
 * to use the first possible space for the new partition.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_start_follow_default(struct fdisk_partition *pa, int enable)
{
	if (!pa)
		return -EINVAL;
	pa->start_follow_default = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_partition_start_is_default:
 * @pa: partition
 *
 * See fdisk_partition_start_follow_default().
 *
 * Returns: 1 if the partition follows default
 */
int fdisk_partition_start_is_default(struct fdisk_partition *pa)
{
	assert(pa);
	return pa->start_follow_default;
}

/**
 * fdisk_partition_set_size:
 * @pa: partition
 * @sz: size in sectors, maximal is UIN64_MAX-1
 *
 * Note that zero is valid size too. Use fdisk_partition_unset_size() to
 * undefine the size.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_size(struct fdisk_partition *pa, fdisk_sector_t sz)
{
	if (!pa)
		return -EINVAL;
	if (FDISK_IS_UNDEF(sz))
		return -ERANGE;
	pa->size = sz;
	pa->fs_probed = 0;
	return 0;
}

/**
 * fdisk_partition_unset_size:
 * @pa: partition
 *
 * Sets the size as undefined. See fdisk_partition_has_size().
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_unset_size(struct fdisk_partition *pa)
{
	if (!pa)
		return -EINVAL;
	FDISK_INIT_UNDEF(pa->size);
	pa->fs_probed = 0;
	return 0;
}

/**
 * fdisk_partition_get_size:
 * @pa: partition
 *
 * The zero is also valid size. The function may return random undefined
 * value when size is undefined (for example after fdisk_partition_unset_size()).
 * Always use fdisk_partition_has_size() to be sure that you work with valid
 * numbers.
 *
 * Returns: size offset in sectors
 */
fdisk_sector_t fdisk_partition_get_size(struct fdisk_partition *pa)
{
	return pa->size;
}

/**
 * fdisk_partition_has_size:
 * @pa: partition
 *
 * Returns: 1 or 0
 */
int fdisk_partition_has_size(struct fdisk_partition *pa)
{
	return pa && !FDISK_IS_UNDEF(pa->size);
}

/**
 * fdisk_partition_size_explicit:
 * @pa: partition
 * @enable: 0|1
 *
 * By default libfdisk aligns the size when add the new partition (by
 * fdisk_add_partition()). If you want to disable this functionality use
 * @enable = 1.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_size_explicit(struct fdisk_partition *pa, int enable)
{
	if (!pa)
		return -EINVAL;
	pa->size_explicit = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_partition_set_partno:
 * @pa: partition
 * @num: partition number (0 is the first partition, maximal is SIZE_MAX-1)
 *
 * Note that zero is valid partno too. Use fdisk_partition_unset_partno() to
 * undefine the partno.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_partno(struct fdisk_partition *pa, size_t num)
{
	if (!pa)
		return -EINVAL;
	if (FDISK_IS_UNDEF(num))
		return -ERANGE;
	pa->partno = num;
	return 0;
}

/**
 * fdisk_partition_unset_partno:
 * @pa: partition
 *
 * Sets the partno as undefined. See fdisk_partition_has_partno().
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_unset_partno(struct fdisk_partition *pa)
{
	if (!pa)
		return -EINVAL;
	FDISK_INIT_UNDEF(pa->partno);
	return 0;
}

/**
 * fdisk_partition_get_partno:
 * @pa: partition
 *
 * The zero is also valid partition number. The function may return random
 * value when partno is undefined (for example after fdisk_partition_unset_partno()).
 * Always use fdisk_partition_has_partno() to be sure that you work with valid
 * numbers.
 *
 * Returns: partition number (0 is the first partition)
 */
size_t fdisk_partition_get_partno(struct fdisk_partition *pa)
{
	return pa->partno;
}

/**
 * fdisk_partition_has_partno:
 * @pa: partition
 *
 * Returns: 1 or 0
 */
int fdisk_partition_has_partno(struct fdisk_partition *pa)
{
	return pa && !FDISK_IS_UNDEF(pa->partno);
}


/**
 * fdisk_partition_cmp_partno:
 * @a: partition
 * @b: partition
 *
 * Compares partitions according to partition number See fdisk_table_sort_partitions().
 *
 * Return: 0 if the same, <0 if @b greater, >0 if @a greater.
 */
int fdisk_partition_cmp_partno(struct fdisk_partition *a,
			       struct fdisk_partition *b)
{
	return a->partno - b->partno;
}

/**
 * fdisk_partition_partno_follow_default
 * @pa: partition
 * @enable: 0|1
 *
 * When @pa used as a template for fdisk_add_partition() when force label driver
 * to add a new partition to the default (next) position.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_partno_follow_default(struct fdisk_partition *pa, int enable)
{
	if (!pa)
		return -EINVAL;
	pa->partno_follow_default = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_partition_set_type:
 * @pa: partition
 * @type: partition type
 *
 * Sets partition type.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_type(struct fdisk_partition *pa,
			     struct fdisk_parttype *type)
{
	if (!pa)
		return -EINVAL;

	fdisk_ref_parttype(type);
	fdisk_unref_parttype(pa->type);
	pa->type = type;

	return 0;
}

/**
 * fdisk_partition_get_type:
 * @pa: partition
 *
 * Returns: pointer to partition type.
 */
struct fdisk_parttype *fdisk_partition_get_type(struct fdisk_partition *pa)
{
	return pa ? pa->type : NULL;
}

/**
 * fdisk_partition_set_name:
 * @pa: partition
 * @name: partition name
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_name(struct fdisk_partition *pa, const char *name)
{
	if (!pa)
		return -EINVAL;
	return strdup_to_struct_member(pa, name, name);
}

/**
 * fdisk_partition_get_name:
 * @pa: partition
 *
 * Returns: partition name
 */
const char *fdisk_partition_get_name(struct fdisk_partition *pa)
{
	return pa ? pa->name : NULL;
}

/**
 * fdisk_partition_set_uuid:
 * @pa: partition
 * @uuid: UUID of the partition
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_set_uuid(struct fdisk_partition *pa, const char *uuid)
{
	if (!pa)
		return -EINVAL;
	return strdup_to_struct_member(pa, uuid, uuid);
}

/**
 * fdisk_partition_has_end:
 * @pa: partition
 *
 * Returns: 1 if the partition has defined last sector
 */
int fdisk_partition_has_end(struct fdisk_partition *pa)
{
	return pa && !FDISK_IS_UNDEF(pa->start) && !FDISK_IS_UNDEF(pa->size);
}

/**
 * fdisk_partition_get_end:
 * @pa: partition
 *
 * This function may returns absolute non-sense, always check
 * fdisk_partition_has_end().
 *
 * Note that partition end is defined by fdisk_partition_set_start() and
 * fdisk_partition_set_size().
 *
 * Returns: last partition sector LBA.
 */
fdisk_sector_t fdisk_partition_get_end(struct fdisk_partition *pa)
{
	return pa->start + pa->size - (pa->size == 0 ? 0 : 1);
}

/**
 * fdisk_partition_end_follow_default
 * @pa: partition
 * @enable: 0|1
 *
 * When @pa used as a template for fdisk_add_partition() when force label
 * driver to use all the possible space for the new partition.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_end_follow_default(struct fdisk_partition *pa, int enable)
{
	if (!pa)
		return -EINVAL;
	pa->end_follow_default = enable ? 1 : 0;
	return 0;
}

/**
 * fdisk_partition_end_is_default:
 * @pa: partition
 *
 * Returns: 1 if the partition follows default
 */
int fdisk_partition_end_is_default(struct fdisk_partition *pa)
{
	assert(pa);
	return pa->end_follow_default;
}

/**
 * fdisk_partition_get_uuid:
 * @pa: partition
 *
 * Returns: partition UUID as string
 */
const char *fdisk_partition_get_uuid(struct fdisk_partition *pa)
{
	return pa ? pa->uuid : NULL;
}

/**
 * fdisk_partition_get_attrs:
 * @pa: partition
 *
 * Returns: partition attributes in string format
 */
const char *fdisk_partition_get_attrs(struct fdisk_partition *pa)
{
	return pa ? pa->attrs : NULL;
}

/**
 * fdisk_partition_set_attrs:
 * @pa: partition
 * @attrs: attributes
 *
 * Sets @attrs to @pa.
 *
 * Return: 0 on success, <0 on error.
 */
int fdisk_partition_set_attrs(struct fdisk_partition *pa, const char *attrs)
{
	if (!pa)
		return -EINVAL;
	return strdup_to_struct_member(pa, attrs, attrs);
}

/**
 * fdisk_partition_is_nested:
 * @pa: partition
 *
 * Returns: 1 if the partition is nested (e.g. MBR logical partition)
 */
int fdisk_partition_is_nested(struct fdisk_partition *pa)
{
	return pa && !FDISK_IS_UNDEF(pa->parent_partno);
}

/**
 * fdisk_partition_is_container:
 * @pa: partition
 *
 * Returns: 1 if the partition is container (e.g. MBR extended partition)
 */
int fdisk_partition_is_container(struct fdisk_partition *pa)
{
	return pa && pa->container;
}

/**
 * fdisk_partition_get_parent:
 * @pa: partition
 * @parent: parent parno
 *
 * Returns: returns devno of the parent
 */
int fdisk_partition_get_parent(struct fdisk_partition *pa, size_t *parent)
{
	if (pa && parent)
		*parent = pa->parent_partno;
	else
		return -EINVAL;
	return 0;
}

/**
 * fdisk_partition_is_used:
 * @pa: partition
 *
 * Returns: 1 if the partition points to some area
 */
int fdisk_partition_is_used(struct fdisk_partition *pa)
{
	return pa && pa->used;
}

/**
 * fdisk_partition_is_bootable:
 * @pa: partition
 *
 * Returns: 1 if the partition has enabled boot flag
 */
int fdisk_partition_is_bootable(struct fdisk_partition *pa)
{
	return pa && pa->boot == 1;
}

/**
 * fdisk_partition_is_freespace:
 * @pa: partition
 *
 * Returns: 1 if @pa points to freespace
 */
int fdisk_partition_is_freespace(struct fdisk_partition *pa)
{
	return pa && pa->freespace;
}

/**
 * fdisk_partition_is_wholedisk:
 * @pa: partition
 *
 * Returns: 1 if the partition is special whole-disk (e.g. SUN) partition
 */
int fdisk_partition_is_wholedisk(struct fdisk_partition *pa)
{
	return pa && pa->wholedisk;
}

/**
 * fdisk_partition_next_partno:
 * @pa: partition
 * @cxt: context
 * @n: returns partition number
 *
 * If @pa specified and partno-follow-default (see fdisk_partition_partno_follow_default())
 * enabled then returns next expected partno or -ERANGE on error.
 *
 * If @pa is NULL, or @pa does not specify any semantic for the next partno
 * then use Ask API to ask user for the next partno. In this case returns 1 if
 * no free partition available. If fdisk dialogs are disabled then returns -EINVAL.
 *
 * Returns: 0 on success, <0 on error, or 1 for non-free partno by Ask API.
 */
int fdisk_partition_next_partno(
		struct fdisk_partition *pa,
		struct fdisk_context *cxt,
		size_t *n)
{
	if (!cxt || !n)
		return -EINVAL;

	if (pa && pa->partno_follow_default) {
		size_t i;

		DBG(PART, ul_debugobj(pa, "next partno (follow default)"));

		for (i = 0; i < cxt->label->nparts_max; i++) {
			if (!fdisk_is_partition_used(cxt, i)) {
				*n = i;
				return 0;
			}
		}
		return -ERANGE;

	}

	if (pa && fdisk_partition_has_partno(pa)) {

		DBG(PART, ul_debugobj(pa, "next partno (specified=%zu)", pa->partno));

		if (pa->partno >= cxt->label->nparts_max ||
		    fdisk_is_partition_used(cxt, pa->partno))
			return -ERANGE;
		*n = pa->partno;
		return 0;

	}

	if (fdisk_has_dialogs(cxt))
		return fdisk_ask_partnum(cxt, n, 1);

	return -EINVAL;
}

static int probe_partition_content(struct fdisk_context *cxt, struct fdisk_partition *pa)
{
	int rc = 1;	/* nothing */

	DBG(PART, ul_debugobj(pa, "start probe #%zu partition [cxt %p] >>>", pa->partno, cxt));

	/* zeroize the current setting */
	strdup_to_struct_member(pa, fstype, NULL);
	strdup_to_struct_member(pa, fsuuid, NULL);
	strdup_to_struct_member(pa, fslabel, NULL);

	if (!fdisk_partition_has_start(pa) ||
	    !fdisk_partition_has_size(pa))
		goto done;

#ifdef HAVE_LIBBLKID
	else {
		uintmax_t start, size;

		blkid_probe pr = blkid_new_probe();
		if (!pr)
			goto done;

		DBG(PART, ul_debugobj(pa, "blkid prober: %p", pr));

		blkid_probe_enable_superblocks(pr, 1);
		blkid_probe_set_superblocks_flags(pr,
				BLKID_SUBLKS_MAGIC |
				BLKID_SUBLKS_TYPE |
				BLKID_SUBLKS_LABEL |
				BLKID_SUBLKS_UUID |
				BLKID_SUBLKS_BADCSUM);

		start = fdisk_partition_get_start(pa) * fdisk_get_sector_size(cxt);
		size = fdisk_partition_get_size(pa) * fdisk_get_sector_size(cxt);

		if (blkid_probe_set_device(pr, cxt->dev_fd, start, size) == 0
		    && blkid_do_fullprobe(pr) == 0) {

			const char *data;
			rc = 0;

			if (!blkid_probe_lookup_value(pr, "TYPE",  &data, NULL))
				rc = strdup_to_struct_member(pa, fstype, data);

			if (!rc && !blkid_probe_lookup_value(pr, "LABEL", &data, NULL))
				rc = strdup_to_struct_member(pa, fslabel, data);

			if (!rc && !blkid_probe_lookup_value(pr, "UUID",  &data, NULL))
				rc = strdup_to_struct_member(pa, fsuuid, data);
		}

		blkid_free_probe(pr);
		pa->fs_probed = 1;
	}
#endif /* HAVE_LIBBLKID */

done:
	DBG(PART, ul_debugobj(pa, "<<< end probe #%zu partition[cxt %p, rc=%d]", pa->partno, cxt, rc));
	return rc;
}

/**
 * fdisk_partition_to_string:
 * @pa: partition
 * @cxt: context
 * @id: field (FDISK_FIELD_*)
 * @data: returns string with allocated data
 *
 * Returns info about partition converted to printable string.
 *
 * For example
 * <informalexample>
 *   <programlisting>
 *      struct fdisk_partition *pa;
 *
 *      fdisk_get_partition(cxt, 0, &pa);
 *	fdisk_partition_to_string(pa, FDISK_FIELD_UUID, &data);
 *	printf("first partition uuid: %s\n", data);
 *	free(data);
 *	fdisk_unref_partition(pa);
 *   </programlisting>
 * </informalexample>
 *
 * returns UUID for the first partition.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_partition_to_string(struct fdisk_partition *pa,
			      struct fdisk_context *cxt,
			      int id,
			      char **data)
{
	char *p = NULL;
	int rc = 0;
	uint64_t x;

	if (!pa || !cxt || !data)
		return -EINVAL;

	switch (id) {
	case FDISK_FIELD_DEVICE:
		if (pa->freespace)
			p = strdup(_("Free space"));
		else if (fdisk_partition_has_partno(pa) && cxt->dev_path) {
			if (cxt->label->flags & FDISK_LABEL_FL_INCHARS_PARTNO)
				rc = asprintf(&p, "%c", (int) pa->partno + 'a');
			else
				p = fdisk_partname(cxt->dev_path, pa->partno + 1);
		}
		break;
	case FDISK_FIELD_BOOT:
		p = fdisk_partition_is_bootable(pa) ? strdup("*") : NULL;
		break;
	case FDISK_FIELD_START:
		if (fdisk_partition_has_start(pa)) {
			x = fdisk_cround(cxt, pa->start);
			rc = pa->start_post ?
				asprintf(&p, "%"PRIu64"%c", x, pa->start_post) :
				asprintf(&p, "%"PRIu64, x);
		}
		break;
	case FDISK_FIELD_END:
		if (fdisk_partition_has_end(pa)) {
			x = fdisk_cround(cxt, fdisk_partition_get_end(pa));
			rc = pa->end_post ?
					asprintf(&p, "%"PRIu64"%c", x, pa->end_post) :
					asprintf(&p, "%"PRIu64, x);
		}
		break;
	case FDISK_FIELD_SIZE:
		if (fdisk_partition_has_size(pa)) {
			uint64_t sz = pa->size * cxt->sector_size;

			switch (cxt->sizeunit) {
			case FDISK_SIZEUNIT_BYTES:
				rc = asprintf(&p, "%"PRIu64"", sz);
				break;
			case FDISK_SIZEUNIT_HUMAN:
				if (fdisk_is_details(cxt))
					rc = pa->size_post ?
							asprintf(&p, "%"PRIu64"%c", sz, pa->size_post) :
							asprintf(&p, "%"PRIu64, sz);
				else {
					p = size_to_human_string(SIZE_SUFFIX_1LETTER, sz);
					if (!p)
						rc = -ENOMEM;
				}
				break;
			}
		}
		break;
	case FDISK_FIELD_CYLINDERS:
	{
		uintmax_t sz = fdisk_partition_has_size(pa) ? pa->size : 0;
		if (sz)
			/* Why we need to cast that to uintmax_t? */
			rc = asprintf(&p, "%ju", (uintmax_t)(sz / (cxt->geom.heads * cxt->geom.sectors)) + 1);
		break;
	}
	case FDISK_FIELD_SECTORS:
		rc = asprintf(&p, "%ju",
			fdisk_partition_has_size(pa) ? (uintmax_t) pa->size : 0);
		break;
	case FDISK_FIELD_BSIZE:
		rc = asprintf(&p, "%"PRIu64, pa->bsize);
		break;
	case FDISK_FIELD_FSIZE:
		rc = asprintf(&p, "%"PRIu64, pa->fsize);
		break;
	case FDISK_FIELD_CPG:
		rc = asprintf(&p, "%"PRIu64, pa->cpg);
		break;
	case FDISK_FIELD_TYPE:
		p = pa->type && pa->type->name ? strdup(_(pa->type->name)) : NULL;
		break;
	case FDISK_FIELD_TYPEID:
		if (pa->type && fdisk_parttype_get_string(pa->type))
			rc = asprintf(&p, "%s", fdisk_parttype_get_string(pa->type));
		else if (pa->type)
			rc = asprintf(&p, "%x", fdisk_parttype_get_code(pa->type));
		break;
	case FDISK_FIELD_UUID:
		p = pa->uuid && *pa->uuid? strdup(pa->uuid) : NULL;
		break;
	case FDISK_FIELD_NAME:
		p = pa->name && *pa->name ? strdup(pa->name) : NULL;
		break;
	case FDISK_FIELD_ATTR:
		p = pa->attrs && *pa->attrs ? strdup(pa->attrs) : NULL;
		break;
	case FDISK_FIELD_SADDR:
		p = pa->start_chs && *pa->start_chs ? strdup(pa->start_chs) : NULL;
		break;
	case FDISK_FIELD_EADDR:
		p = pa->end_chs && *pa->end_chs? strdup(pa->end_chs) : NULL;
		break;
	case FDISK_FIELD_FSUUID:
		if (pa->fs_probed || probe_partition_content(cxt, pa) == 0)
			p = pa->fsuuid && *pa->fsuuid ? strdup(pa->fsuuid) : NULL;
		break;
	case FDISK_FIELD_FSLABEL:
		if (pa->fs_probed || probe_partition_content(cxt, pa) == 0)
			p = pa->fslabel && *pa->fslabel ? strdup(pa->fslabel) : NULL;
		break;
	case FDISK_FIELD_FSTYPE:
		if (pa->fs_probed || probe_partition_content(cxt, pa) == 0)
			p = pa->fstype && *pa->fstype ? strdup(pa->fstype) : NULL;
		break;
	default:
		return -EINVAL;
	}

	if (rc < 0) {
		rc = -ENOMEM;
		free(p);
		p = NULL;

	} else if (rc > 0)
		rc = 0;

	*data = p;

	return rc;
}


/**
 * fdisk_get_partition:
 * @cxt: context
 * @partno: partition number (0 is the first partition)
 * @pa: returns data about partition
 *
 * Reads disklabel and fills in @pa with data about partition @n.
 *
 * Note that partno may address unused partition and then this function does
 * not fill anything to @pa.  See fdisk_is_partition_used(). If @pa points to
 * NULL then the function allocates a newly allocated fdisk_partition struct,
 * use fdisk_unref_partition() to deallocate.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_get_partition(struct fdisk_context *cxt, size_t partno,
			struct fdisk_partition **pa)
{
	int rc;
	struct fdisk_partition *np = NULL;

	if (!cxt || !cxt->label || !pa)
		return -EINVAL;
	if (!cxt->label->op->get_part)
		return -ENOSYS;
	if (!fdisk_is_partition_used(cxt, partno))
		return -EINVAL;

	if (!*pa) {
		np = *pa = fdisk_new_partition();
		if (!*pa)
			return -ENOMEM;
	} else
		fdisk_reset_partition(*pa);

	(*pa)->partno = partno;
	rc = cxt->label->op->get_part(cxt, partno, *pa);

	if (rc) {
		if (np) {
			fdisk_unref_partition(np);
			*pa = NULL;
		} else
			fdisk_reset_partition(*pa);
	} else
		(*pa)->size_explicit = 1;
	return rc;
}

static struct fdisk_partition *area_by_offset(
			struct fdisk_table *tb,
			struct fdisk_partition *cur,
			fdisk_sector_t off)
{
	struct fdisk_partition *pa = NULL;
	struct fdisk_iter itr;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	while (fdisk_table_next_partition(tb, &itr, &pa) == 0) {
		if (!fdisk_partition_has_start(pa) || !fdisk_partition_has_size(pa))
			continue;
		if (fdisk_partition_is_nested(cur) &&
		    pa->parent_partno != cur->parent_partno)
			continue;
		if (off >= pa->start && off < pa->start + pa->size)
			return pa;
	}

	return NULL;
}

static int resize_get_first_possible(
			struct fdisk_table *tb,
			struct fdisk_partition *cur,
			fdisk_sector_t *start)
{
	struct fdisk_partition *pa = NULL, *first = NULL;
	struct fdisk_iter itr;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	*start = 0;
	DBG(TAB, ul_debugobj(tb, "checking first possible before start=%ju", (uintmax_t) cur->start));


	while (fdisk_table_next_partition(tb, &itr, &pa) == 0) {

		if (pa->start > cur->start || pa == cur)
			break;

		DBG(TAB, ul_debugobj(tb, " checking entry %p [partno=%zu start=%ju, end=%ju, size=%ju%s%s%s]",
			pa,
			fdisk_partition_get_partno(pa),
			(uintmax_t) fdisk_partition_get_start(pa),
			(uintmax_t) fdisk_partition_get_end(pa),
			(uintmax_t) fdisk_partition_get_size(pa),
			fdisk_partition_is_freespace(pa) ? " freespace" : "",
			fdisk_partition_is_nested(pa)    ? " nested"    : "",
			fdisk_partition_is_container(pa) ? " container" : ""));


		if (!fdisk_partition_is_freespace(pa)) {
			DBG(TAB, ul_debugobj(tb, "  ignored (no freespace)"));
			first = NULL;
			continue;
		}
		if (!fdisk_partition_has_start(pa) || !fdisk_partition_has_size(pa)) {
			DBG(TAB, ul_debugobj(tb, "  ignored (no start/size)"));
			first = NULL;
			continue;
		}
		/* The current is nested, free space has to be nested within the same parent */
		if (fdisk_partition_is_nested(cur)
		    && pa->parent_partno != cur->parent_partno) {
			DBG(TAB, ul_debugobj(tb, "  ignore (nested required)"));
			first = NULL;
			continue;
		}
		if (pa->start + pa->size <= cur->start) {
			first = pa;
			DBG(TAB, ul_debugobj(tb, "  entry usable"));
		}
	}

	if (first)
		*start = first->start;
	else
		DBG(PART, ul_debugobj(cur, "resize: nothing usable before %ju", (uintmax_t) cur->start));

	return first ? 0 : -1;
}

/*
 * Verify that area addressed by @start is freespace or the @cur[rent]
 * partition and continue to the next table entries until it's freespace, and
 * counts size of all this space.
 *
 * This is core of the partition start offset move operation. We can move the
 * start within the current partition of to the another free space. It's
 * forbidden to move start of the partition to another already defined
 * partition.
 */
static int resize_get_last_possible(
			struct fdisk_table *tb,
			struct fdisk_partition *cur,
			fdisk_sector_t start,
			fdisk_sector_t *maxsz)
{
	struct fdisk_partition *pa = NULL, *last = NULL;
	struct fdisk_iter itr;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	*maxsz = 0;
	DBG(TAB, ul_debugobj(tb, "checking last possible for start=%ju", (uintmax_t) start));


	while (fdisk_table_next_partition(tb, &itr, &pa) == 0) {

		DBG(TAB, ul_debugobj(tb, " checking entry %p [partno=%zu start=%ju, end=%ju, size=%ju%s%s%s]",
			pa,
			fdisk_partition_get_partno(pa),
			(uintmax_t) fdisk_partition_get_start(pa),
			(uintmax_t) fdisk_partition_get_end(pa),
			(uintmax_t) fdisk_partition_get_size(pa),
			fdisk_partition_is_freespace(pa) ? " freespace" : "",
			fdisk_partition_is_nested(pa)    ? " nested"    : "",
			fdisk_partition_is_container(pa) ? " container" : ""));

		if (!fdisk_partition_has_start(pa) ||
		    !fdisk_partition_has_size(pa) ||
		    (fdisk_partition_is_container(pa) && pa != cur)) {
			DBG(TAB, ul_debugobj(tb, "  ignored (no start/size or container)"));
			continue;
		}

		if (fdisk_partition_is_nested(pa)
		    && fdisk_partition_is_container(cur)
		    && pa->parent_partno == cur->partno) {
			DBG(TAB, ul_debugobj(tb, "  ignore (nested child of the current partition)"));
			continue;
		}

		/* The current is nested, free space has to be nested within the same parent */
		if (fdisk_partition_is_nested(cur)
		    && pa->parent_partno != cur->parent_partno) {
			DBG(TAB, ul_debugobj(tb, "  ignore (nested required)"));
			continue;
		}

		if (!last) {
			if (start >= pa->start &&  start < pa->start + pa->size) {
				if (fdisk_partition_is_freespace(pa) || pa == cur) {
					DBG(TAB, ul_debugobj(tb, "  accepted as last"));
					last = pa;
				} else {
					DBG(TAB, ul_debugobj(tb, "  failed to set last"));
					break;
				}


				*maxsz = pa->size - (start - pa->start);
				DBG(TAB, ul_debugobj(tb, "  new max=%ju", (uintmax_t) *maxsz));
			}
		} else if (!fdisk_partition_is_freespace(pa) && pa != cur) {
			DBG(TAB, ul_debugobj(tb, "  no free space behind current"));
			break;
		} else {
			last = pa;
			*maxsz = pa->size - (start - pa->start);
			DBG(TAB, ul_debugobj(tb, "  new max=%ju (last updated)", (uintmax_t) *maxsz));
		}
	}

	if (last)
		DBG(PART, ul_debugobj(cur, "resize: max size=%ju", (uintmax_t) *maxsz));
	else
		DBG(PART, ul_debugobj(cur, "resize: nothing usable after %ju", (uintmax_t) start));

	return last ? 0 : -1;
}

/*
 * Uses template @tpl to recount start and size change of the partition @res. The
 * @tpl->size and @tpl->start are interpreted as relative to the current setting.
 */
static int recount_resize(
			struct fdisk_context *cxt, size_t partno,
			struct fdisk_partition *res, struct fdisk_partition *tpl)
{
	fdisk_sector_t start, size, xsize;
	struct fdisk_partition *cur = NULL;
	struct fdisk_table *tb = NULL;
	int rc;

	DBG(PART, ul_debugobj(tpl, ">>> resize requested"));

	FDISK_INIT_UNDEF(start);
	FDISK_INIT_UNDEF(size);

	rc = fdisk_get_partitions(cxt, &tb);
	if (!rc) {
		/* For resize we do not follow grain to detect free-space, but
		 * we support to resize with very small granulation. */
		unsigned long org = cxt->grain;

		cxt->grain = cxt->sector_size;
		rc = fdisk_get_freespaces(cxt, &tb);
		cxt->grain = org;
	}
	if (rc) {
		fdisk_unref_table(tb);
		return rc;
	}

	fdisk_table_sort_partitions(tb, fdisk_partition_cmp_start);

	DBG(PART, ul_debugobj(tpl, "resize partition partno=%zu in table:", partno));
	ON_DBG(PART, fdisk_debug_print_table(tb));

	cur = fdisk_table_get_partition_by_partno(tb, partno);
	if (!cur) {
		fdisk_unref_table(tb);
		return -EINVAL;
	}

	/* 1a) set new start - change relative to the current on-disk setting */
	if (tpl->movestart && fdisk_partition_has_start(tpl)) {
		start = fdisk_partition_get_start(cur);
		if (tpl->movestart == FDISK_MOVE_DOWN) {
			if (fdisk_partition_get_start(tpl) > start)
				goto erange;
			start -= fdisk_partition_get_start(tpl);
		} else
			start += fdisk_partition_get_start(tpl);

		DBG(PART, ul_debugobj(tpl, "resize: moving start %s relative, new start: %ju",
				tpl->movestart == FDISK_MOVE_DOWN  ? "DOWN" : "UP", (uintmax_t)start));

	/* 1b) set new start - try freespace before the curret partition */
	} else if (tpl->movestart == FDISK_MOVE_DOWN) {

		if (resize_get_first_possible(tb, cur, &start) != 0)
			goto erange;

		DBG(PART, ul_debugobj(tpl, "resize: moving start DOWN (first possible), new start: %ju",
				(uintmax_t)start));

	/* 1c) set new start - absolute number */
	} else if (fdisk_partition_has_start(tpl)) {
		start = fdisk_partition_get_start(tpl);
		DBG(PART, ul_debugobj(tpl, "resize: moving start to absolute offset: %ju",
		                      (uintmax_t)start));
	}

	/* 2) verify that start is within the current partition or any freespace area */
	if (!FDISK_IS_UNDEF(start)) {
		struct fdisk_partition *area = area_by_offset(tb, cur, start);

		if (area == cur)
			DBG(PART, ul_debugobj(tpl, "resize: start points to the current partition"));
		else if (area && fdisk_partition_is_freespace(area))
			DBG(PART, ul_debugobj(tpl, "resize: start points to freespace"));
		else if (!area && start >= cxt->first_lba && start < cxt->first_lba + (cxt->grain / cxt->sector_size))
			DBG(PART, ul_debugobj(tpl, "resize: start points before first partition"));
		else {
			DBG(PART, ul_debugobj(tpl, "resize: start verification failed"));
			goto erange;
		}
	} else {
		/* no change, start points to the current partition */
		DBG(PART, ul_debugobj(tpl, "resize: start unchanged"));
		start = fdisk_partition_get_start(cur);
	}

	/* 3a) set new size -- reduce */
	if (tpl->resize == FDISK_RESIZE_REDUCE && fdisk_partition_has_size(tpl)) {
		DBG(PART, ul_debugobj(tpl, "resize: reduce"));
		size = fdisk_partition_get_size(cur);
		if (fdisk_partition_get_size(tpl) > size)
			goto erange;
		size -= fdisk_partition_get_size(tpl);

	/* 3b) set new size -- enlarge */
	} else if (tpl->resize == FDISK_RESIZE_ENLARGE && fdisk_partition_has_size(tpl)) {
		DBG(PART, ul_debugobj(tpl, "resize: enlarge"));
		size = fdisk_partition_get_size(cur);
		size += fdisk_partition_get_size(tpl);

	/* 3c) set new size -- no size specified, enlarge to all freespace */
	} else if (tpl->resize == FDISK_RESIZE_ENLARGE) {
		DBG(PART, ul_debugobj(tpl, "resize: enlarge to all possible"));
		if (resize_get_last_possible(tb, cur, start, &size))
			goto erange;

	/* 3d) set new size -- absolute number */
	} else if (fdisk_partition_has_size(tpl)) {
		DBG(PART, ul_debugobj(tpl, "resize: new absolute size"));
		size = fdisk_partition_get_size(tpl);
	}

	/* 4) verify that size is within the current partition or next free space */
	xsize = !FDISK_IS_UNDEF(size) ? size : fdisk_partition_get_size(cur);

	if (fdisk_partition_has_size(cur)) {
		fdisk_sector_t maxsz;

		if (resize_get_last_possible(tb, cur, start, &maxsz))
			goto erange;
		DBG(PART, ul_debugobj(tpl, "resize: size=%ju, max=%ju",
					(uintmax_t) xsize, (uintmax_t) maxsz));
		if (xsize > maxsz)
			goto erange;
	}

	if (FDISK_IS_UNDEF(size)) {
		DBG(PART, ul_debugobj(tpl, "resize: size unchanged (undefined)"));
	}


	DBG(PART, ul_debugobj(tpl, "<<< resize: SUCCESS: start %ju->%ju; size %ju->%ju",
			(uintmax_t) fdisk_partition_get_start(cur), (uintmax_t) start,
			(uintmax_t) fdisk_partition_get_size(cur), (uintmax_t) size));
	res->start = start;
	res->size = size;
	fdisk_unref_table(tb);
	return 0;
erange:
	DBG(PART, ul_debugobj(tpl, "<<< resize: FAILED"));
	fdisk_warnx(cxt, _("Failed to resize partition #%zu."), partno + 1);
	fdisk_unref_table(tb);
	return -ERANGE;

}

/**
 * fdisk_set_partition:
 * @cxt: context
 * @partno: partition number (0 is the first partition)
 * @pa: new partition setting
 *
 * Modifies disklabel according to setting with in @pa.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_set_partition(struct fdisk_context *cxt, size_t partno,
			struct fdisk_partition *pa)
{
	struct fdisk_partition *xpa = pa, *tmp = NULL;
	int rc, wipe = 0;

	if (!cxt || !cxt->label || !pa)
		return -EINVAL;
	if (!cxt->label->op->set_part)
		return -ENOSYS;

	pa->fs_probed = 0;

	if (!fdisk_is_partition_used(cxt, partno)) {
		pa->partno = partno;
		return fdisk_add_partition(cxt, pa, NULL);
	}

	if (pa->resize || pa->movestart
	    || fdisk_partition_has_start(pa) || fdisk_partition_has_size(pa)) {
		xpa = __copy_partition(pa);
		if (!xpa) {
			rc = -ENOMEM;
			goto done;
		}
		xpa->movestart = 0;
		xpa->resize = 0;
		FDISK_INIT_UNDEF(xpa->size);
		FDISK_INIT_UNDEF(xpa->start);

		rc = recount_resize(cxt, partno, xpa, pa);
		if (rc)
			goto done;
	}

	DBG(CXT, ul_debugobj(cxt, "setting partition %zu %p (start=%ju, end=%ju, size=%ju)",
		    partno, xpa,
		    (uintmax_t) fdisk_partition_get_start(xpa),
		    (uintmax_t) fdisk_partition_get_end(xpa),
		    (uintmax_t) fdisk_partition_get_size(xpa)));

	/* disable wipe for old offset/size setting */
	if (fdisk_get_partition(cxt, partno, &tmp) == 0 && tmp) {
		wipe = fdisk_set_wipe_area(cxt, fdisk_partition_get_start(tmp),
						fdisk_partition_get_size(tmp), FALSE);
		fdisk_unref_partition(tmp);
	}

	/* call label driver */
	rc = cxt->label->op->set_part(cxt, partno, xpa);

	/* enable wipe for new offset/size */
	if (!rc && wipe)
		fdisk_wipe_partition(cxt, partno, TRUE);
done:
	DBG(CXT, ul_debugobj(cxt, "set_partition() rc=%d", rc));
	if (xpa != pa)
		fdisk_unref_partition(xpa);
	return rc;
}

/**
 * fdisk_wipe_partition:
 * @cxt: fdisk context
 * @partno: partition number
 * @enable: 0 or 1
 *
 * Enable/disable filesystems/RAIDs wiping in area defined by partition start and size.
 *
 * Returns: <0 in case of error, 0 on success
 * Since: 2.29
 */
int fdisk_wipe_partition(struct fdisk_context *cxt, size_t partno, int enable)
{
	struct fdisk_partition *pa = NULL;
	int rc;

	rc = fdisk_get_partition(cxt, partno, &pa);
	if (rc)
		return rc;

	rc = fdisk_set_wipe_area(cxt, fdisk_partition_get_start(pa),
				      fdisk_partition_get_size(pa), enable);
	fdisk_unref_partition(pa);
	return rc < 0 ? rc : 0;
}

/**
 * fdisk_partition_has_wipe:
 * @cxt: fdisk context
 * @pa: partition
 *
 * Since: 2.30
 *
 * Returns: 1 if the area specified by @pa will be wiped by write command, or 0.
 */
int fdisk_partition_has_wipe(struct fdisk_context *cxt, struct fdisk_partition *pa)
{
	return fdisk_has_wipe_area(cxt, fdisk_partition_get_start(pa),
					fdisk_partition_get_size(pa));
}


/**
 * fdisk_add_partition:
 * @cxt: fdisk context
 * @pa: template for the partition (or NULL)
 * @partno: NULL or returns new partition number
 *
 * If @pa is not specified or any @pa item is missing the libfdisk will ask by
 * fdisk_ask_ API.
 *
 * The @pa template is important for non-interactive partitioning,
 * especially for MBR where is necessary to differentiate between
 * primary/logical; this is done by start offset or/and partno.
 * The rules for MBR:
 *
 *   A) template specifies start within extended partition: add logical
 *   B) template specifies start out of extended partition: add primary
 *   C) template specifies start (or default), partno < 4: add primary
 *   D) template specifies default start, partno >= 4: add logical
 *
 * otherwise MBR driver uses Ask-API to get missing information.
 *
 * Adds a new partition to disklabel.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_add_partition(struct fdisk_context *cxt,
			struct fdisk_partition *pa,
			size_t *partno)
{
	int rc;

	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->add_part)
		return -ENOSYS;
	if (fdisk_missing_geometry(cxt))
		return -EINVAL;

	if (pa) {
		pa->fs_probed = 0;
		DBG(CXT, ul_debugobj(cxt, "adding new partition %p", pa));
		if (fdisk_partition_has_start(pa))
			DBG(CXT, ul_debugobj(cxt, "     start: %ju", (uintmax_t) fdisk_partition_get_start(pa)));
		if (fdisk_partition_has_end(pa))
			DBG(CXT, ul_debugobj(cxt, "       end: %ju", (uintmax_t) fdisk_partition_get_end(pa)));
		if (fdisk_partition_has_size(pa))
			DBG(CXT, ul_debugobj(cxt, "      size: %ju", (uintmax_t) fdisk_partition_get_size(pa)));

		DBG(CXT, ul_debugobj(cxt,         "  defaults: start=%s, end=%s, partno=%s",
			    pa->start_follow_default ? "yes" : "no",
			    pa->end_follow_default ? "yes" : "no",
			    pa->partno_follow_default ? "yes" : "no"));
	} else
		DBG(CXT, ul_debugobj(cxt, "adding partition"));

	rc = cxt->label->op->add_part(cxt, pa, partno);

	DBG(CXT, ul_debugobj(cxt, "add partition done (rc=%d)", rc));
	return rc;
}

/**
 * fdisk_delete_partition:
 * @cxt: fdisk context
 * @partno: partition number to delete (0 is the first partition)
 *
 * Deletes a @partno partition from disklabel.
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_delete_partition(struct fdisk_context *cxt, size_t partno)
{
	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->del_part)
		return -ENOSYS;

	fdisk_wipe_partition(cxt, partno, 0);

	DBG(CXT, ul_debugobj(cxt, "deleting %s partition number %zd",
				cxt->label->name, partno));
	return cxt->label->op->del_part(cxt, partno);
}

/**
 * fdisk_delete_all_partitions:
 * @cxt: fdisk context
 *
 * Delete all used partitions from disklabel.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_delete_all_partitions(struct fdisk_context *cxt)
{
	size_t i;
	int rc = 0;

	if (!cxt || !cxt->label)
		return -EINVAL;

	for (i = 0; i < cxt->label->nparts_max; i++) {

		if (!fdisk_is_partition_used(cxt, i))
			continue;
		rc = fdisk_delete_partition(cxt, i);
		if (rc)
			break;
	}

	return rc;
}

/**
 * fdisk_is_partition_used:
 * @cxt: context
 * @n: partition number (0 is the first partition)
 *
 * Check if the partition number @n is used by partition table. This function
 * does not check if the device is used (e.g. mounted) by system!
 *
 * This is faster than fdisk_get_partition() + fdisk_partition_is_used().
 *
 * Returns: 0 or 1
 */
int fdisk_is_partition_used(struct fdisk_context *cxt, size_t n)
{
	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->part_is_used)
		return -ENOSYS;

	return cxt->label->op->part_is_used(cxt, n);
}


/**
 * fdisk_partition_max_size:
 * @cxt: context
 * @n: partition number (0 is the first partition)
 * @maxsz: returns maximum size of partition
 *
 * Find maximum size the partition can be resized to.
 * Takes into account free space between this partition and the next one.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_partition_get_max_size(struct fdisk_context *cxt, size_t n,
				 fdisk_sector_t *maxsz)
{
	struct fdisk_partition *cur = NULL;
	struct fdisk_table *tb = NULL;
	int rc;

	rc = fdisk_get_partitions(cxt, &tb);
	if (rc)
		goto out;

	rc = fdisk_get_freespaces(cxt, &tb);
	if (rc)
		goto out;

	rc = fdisk_table_sort_partitions(tb, fdisk_partition_cmp_start);
	if (rc)
		goto out;

	cur = fdisk_table_get_partition_by_partno(tb, n);
	if (!cur)
		goto einval;

	if (!fdisk_partition_has_start(cur))
		goto einval;

	if (resize_get_last_possible(tb, cur,
				     fdisk_partition_get_start(cur), maxsz))
		goto einval;

out:
	fdisk_unref_partition(cur);
	fdisk_unref_table(tb);

	return rc;

einval:
	rc = -EINVAL;
	goto out;
}
