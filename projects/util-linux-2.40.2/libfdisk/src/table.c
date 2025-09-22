
#include "fdiskP.h"

/**
 * SECTION: table
 * @title: Table
 * @short_description: container for fdisk partitions
 *
 * The fdisk_table is simple container for fdisk_partitions. The table is no
 * directly connected to label data (partition table), and table changes don't
 * affect in-memory or on-disk data.
 */

/**
 * fdisk_new_table:
 *
 * The table is a container for struct fdisk_partition entries. The container
 * does not have any real connection with label (partition table) and with
 * real on-disk data.
 *
 * Returns: newly allocated table struct.
 */
struct fdisk_table *fdisk_new_table(void)
{
	struct fdisk_table *tb = NULL;

	tb = calloc(1, sizeof(*tb));
	if (!tb)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "alloc"));
	tb->refcount = 1;
	INIT_LIST_HEAD(&tb->parts);
	return tb;
}

/**
 * fdisk_reset_table:
 * @tb: tab pointer
 *
 * Removes all entries (partitions) from the table. The partitions with zero
 * reference count will be deallocated. This function does not modify partition
 * table.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int fdisk_reset_table(struct fdisk_table *tb)
{
	if (!tb)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "reset"));

	while (!list_empty(&tb->parts)) {
		struct fdisk_partition *pa = list_entry(tb->parts.next,
				                  struct fdisk_partition, parts);
		fdisk_table_remove_partition(tb, pa);
	}

	tb->nents = 0;
	return 0;
}

/**
 * fdisk_ref_table:
 * @tb: table pointer
 *
 * Increments reference counter.
 */
void fdisk_ref_table(struct fdisk_table *tb)
{
	if (tb)
		tb->refcount++;
}

/**
 * fdisk_unref_table:
 * @tb: table pointer
 *
 * Descrements reference counter, on zero the @tb is automatically
 * deallocated.
 */
void fdisk_unref_table(struct fdisk_table *tb)
{
	if (!tb)
		return;

	tb->refcount--;
	if (tb->refcount <= 0) {
		fdisk_reset_table(tb);

		DBG(TAB, ul_debugobj(tb, "free"));
		free(tb);
	}
}

/**
 * fdisk_table_is_empty:
 * @tb: pointer to tab
 *
 * Returns: 1 if the table is without filesystems, or 0.
 */
int fdisk_table_is_empty(struct fdisk_table *tb)
{
	return tb == NULL || list_empty(&tb->parts) ? 1 : 0;
}

/**
 * fdisk_table_get_nents:
 * @tb: pointer to tab
 *
 * Returns: number of entries in table.
 */
size_t fdisk_table_get_nents(struct fdisk_table *tb)
{
	return tb ? tb->nents : 0;
}

/**
 * fdisk_table_next_partition:
 * @tb: tab pointer
 * @itr: iterator
 * @pa: returns the next tab entry
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 *
 * Example:
 * <informalexample>
 *   <programlisting>
 *	while(fdisk_table_next_partition(tb, itr, &pa) == 0) {
 *		...
 *	}
 *   </programlisting>
 * </informalexample>
 */
int fdisk_table_next_partition(
			struct fdisk_table *tb,
			struct fdisk_iter *itr,
			struct fdisk_partition **pa)
{
	int rc = 1;

	if (!tb || !itr || !pa)
		return -EINVAL;
	*pa = NULL;

	if (!itr->head)
		FDISK_ITER_INIT(itr, &tb->parts);
	if (itr->p != itr->head) {
		FDISK_ITER_ITERATE(itr, *pa, struct fdisk_partition, parts);
		rc = 0;
	}

	return rc;
}

/**
 * fdisk_table_get_partition:
 * @tb: tab pointer
 * @n: number of entry in table
 *
 * Returns: n-th entry from table or NULL
 */
struct fdisk_partition *fdisk_table_get_partition(
			struct fdisk_table *tb,
			size_t n)
{
	struct fdisk_partition *pa = NULL;
	struct fdisk_iter itr;

	if (!tb)
		return NULL;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	while (fdisk_table_next_partition(tb, &itr, &pa) == 0) {
		if (n == 0)
			return pa;
		n--;
	}

	return NULL;
}

/**
 * fdisk_table_get_partition_by_partno:
 * @tb: tab pointer
 * @partno: partition number
 *
 * Returns: partition with @partno or NULL.
 */
struct fdisk_partition *fdisk_table_get_partition_by_partno(
			struct fdisk_table *tb,
			size_t partno)
{
	struct fdisk_partition *pa = NULL;
	struct fdisk_iter itr;

	if (!tb)
		return NULL;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	while (fdisk_table_next_partition(tb, &itr, &pa) == 0) {
		if (pa->partno == partno)
			return pa;
	}

	return NULL;
}

/**
 * fdisk_table_add_partition
 * @tb: tab pointer
 * @pa: new entry
 *
 * Adds a new entry to table and increment @pa reference counter. Don't forget to
 * use fdisk_unref_partition() after fdisk_table_add_partition() if you want to keep
 * the @pa referenced by the table only.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int fdisk_table_add_partition(struct fdisk_table *tb, struct fdisk_partition *pa)
{
	if (!tb || !pa)
		return -EINVAL;

	if (!list_empty(&pa->parts))
		return -EBUSY;

	fdisk_ref_partition(pa);
	list_add_tail(&pa->parts, &tb->parts);
	tb->nents++;

	DBG(TAB, ul_debugobj(tb, "add entry %p [start=%ju, end=%ju, size=%ju, %s %s %s]",
			pa,
			(uintmax_t) fdisk_partition_get_start(pa),
			fdisk_partition_has_end(pa) ? (uintmax_t) fdisk_partition_get_end(pa) : 0,
			fdisk_partition_has_size(pa) ? (uintmax_t) fdisk_partition_get_size(pa) : 0,
			fdisk_partition_is_freespace(pa) ? "freespace" : "",
			fdisk_partition_is_nested(pa)    ? "nested"    : "",
			fdisk_partition_is_container(pa) ? "container" : "primary"));
	return 0;
}

/* inserts @pa after @poz */
static int table_insert_partition(
			struct fdisk_table *tb,
			struct fdisk_partition *poz,
			struct fdisk_partition *pa)
{
	assert(tb);
	assert(pa);

	fdisk_ref_partition(pa);
	if (poz)
		list_add(&pa->parts, &poz->parts);
	else
		list_add(&pa->parts, &tb->parts);
	tb->nents++;

	DBG(TAB, ul_debugobj(tb, "insert entry %p pre=%p [start=%ju, end=%ju, size=%ju, %s %s %s]",
			pa, poz ? poz : NULL,
			(uintmax_t) fdisk_partition_get_start(pa),
			(uintmax_t) fdisk_partition_get_end(pa),
			(uintmax_t) fdisk_partition_get_size(pa),
			fdisk_partition_is_freespace(pa) ? "freespace" : "",
			fdisk_partition_is_nested(pa)    ? "nested"    : "",
			fdisk_partition_is_container(pa) ? "container" : ""));
	return 0;
}

/**
 * fdisk_table_remove_partition
 * @tb: tab pointer
 * @pa: new entry
 *
 * Removes the @pa from the table and de-increment reference counter of the @pa. The
 * partition with zero reference counter will be deallocated. Don't forget to use
 * fdisk_ref_partition() before call fdisk_table_remove_partition() if you want
 * to use @pa later.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int fdisk_table_remove_partition(struct fdisk_table *tb, struct fdisk_partition *pa)
{
	if (!tb || !pa)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "remove entry %p", pa));
	list_del(&pa->parts);
	INIT_LIST_HEAD(&pa->parts);

	fdisk_unref_partition(pa);
	tb->nents--;

	return 0;
}

/**
 * fdisk_get_partitions
 * @cxt: fdisk context
 * @tb: returns table
 *
 * This function adds partitions from disklabel to @table, it allocates a new
 * table if @table points to NULL.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_get_partitions(struct fdisk_context *cxt, struct fdisk_table **tb)
{
	size_t i;

	if (!cxt || !cxt->label || !tb)
		return -EINVAL;
	if (!cxt->label->op->get_part)
		return -ENOSYS;

	DBG(CXT, ul_debugobj(cxt, " -- get table --"));

	if (!*tb && !(*tb = fdisk_new_table()))
		return -ENOMEM;

	for (i = 0; i < cxt->label->nparts_max; i++) {
		struct fdisk_partition *pa = NULL;

		if (fdisk_get_partition(cxt, i, &pa) != 0)
			continue;
		if (fdisk_partition_is_used(pa))
			fdisk_table_add_partition(*tb, pa);
		fdisk_unref_partition(pa);
	}

	return 0;
}

void fdisk_debug_print_table(struct fdisk_table *tb)
{
	struct fdisk_iter itr;
	struct fdisk_partition *pa;

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
	while (fdisk_table_next_partition(tb, &itr, &pa) == 0)
		ul_debugobj(tb, "partition %p [partno=%zu, start=%ju, end=%ju, size=%ju%s%s%s] ",
			    pa, pa->partno,
			    (uintmax_t) fdisk_partition_get_start(pa),
			    (uintmax_t) fdisk_partition_get_end(pa),
			    (uintmax_t) fdisk_partition_get_size(pa),
			    fdisk_partition_is_nested(pa) ? " nested" : "",
			    fdisk_partition_is_freespace(pa) ? " freespace" : "",
			    fdisk_partition_is_container(pa) ? " container" : "");

}


typedef	int (*fdisk_partcmp_t)(struct fdisk_partition *, struct fdisk_partition *);

static int cmp_parts_wrapper(struct list_head *a, struct list_head *b, void *data)
{
	struct fdisk_partition *pa = list_entry(a, struct fdisk_partition, parts),
			       *pb = list_entry(b, struct fdisk_partition, parts);

	fdisk_partcmp_t cmp = (fdisk_partcmp_t) data;

	return cmp(pa, pb);
}


/**
 * fdisk_table_sort_partitions:
 * @tb: table
 * @cmp: compare function
 *
 * Sort partition in the table.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_table_sort_partitions(struct fdisk_table *tb,
			int (*cmp)(struct fdisk_partition *,
				   struct fdisk_partition *))
{
	if (!tb)
		return -EINVAL;

	/*
	DBG(TAB, ul_debugobj(tb, "Before sort:"));
	ON_DBG(TAB, fdisk_debug_print_table(tb));
	*/

	list_sort(&tb->parts, cmp_parts_wrapper, (void *) cmp);

	/*
	DBG(TAB, ul_debugobj(tb, "After sort:"));
	ON_DBG(TAB, fdisk_debug_print_table(tb));
	*/

	return 0;
}

/* allocates a new freespace description */
static int new_freespace(struct fdisk_context *cxt,
			 fdisk_sector_t start,
			 fdisk_sector_t end,
			 struct fdisk_partition *parent,
			 struct fdisk_partition **pa)
{
	fdisk_sector_t aligned_start, size;

	assert(cxt);
	assert(pa);

	*pa = NULL;

	if (start == end)
		return 0;

	assert(start >= cxt->first_lba);
	assert(end);
	assert(end > start);

	aligned_start = fdisk_align_lba_in_range(cxt, start, start, end);
	size = end - aligned_start + 1ULL;

	if (size == 0) {
		DBG(TAB, ul_debug("ignore freespace (aligned size is zero)"));
		return 0;
	}

	*pa = fdisk_new_partition();
	if (!*pa)
		return -ENOMEM;

	(*pa)->freespace = 1;
	(*pa)->start = aligned_start;
	(*pa)->size = size;

	if (parent)
		(*pa)->parent_partno = parent->partno;
	return 0;
}

/* add freespace description to the right place within @tb */
static int table_add_freespace(
			struct fdisk_context *cxt,
			struct fdisk_table *tb,
			fdisk_sector_t start,
			fdisk_sector_t end,
			struct fdisk_partition *parent)
{
	struct fdisk_partition *pa, *x, *real_parent = NULL, *best = NULL;
	struct fdisk_iter itr;
	int rc = 0;

	assert(tb);

	rc = new_freespace(cxt, start, end, parent, &pa);
	if (rc)
		return -ENOMEM;
	if (!pa)
		return 0;

	assert(fdisk_partition_has_start(pa));
	assert(fdisk_partition_has_end(pa));

	DBG(TAB, ul_debugobj(tb, "adding freespace"));

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
	if (parent && fdisk_partition_has_partno(parent)) {
		while (fdisk_table_next_partition(tb, &itr, &x) == 0) {
			if (!fdisk_partition_has_partno(x))
				continue;
			if (x->partno == parent->partno) {
				real_parent = x;
				break;
			}
		}
		if (!real_parent) {
			DBG(TAB, ul_debugobj(tb, "not found freespace parent (partno=%zu)",
					parent->partno));
			fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
		}
	}

	while (fdisk_table_next_partition(tb, &itr, &x) == 0) {
		fdisk_sector_t the_end, best_end = 0;

		if (!fdisk_partition_has_end(x))
			continue;

		the_end = fdisk_partition_get_end(x);
		if (best)
			best_end = fdisk_partition_get_end(best);

		if (the_end < pa->start && (!best || best_end < the_end))
			best = x;
	}

	if (!best && real_parent)
		best = real_parent;
	rc = table_insert_partition(tb, best, pa);

	fdisk_unref_partition(pa);

	DBG(TAB, ul_debugobj(tb, "adding freespace DONE [rc=%d]", rc));
	return rc;
}

/* analyze @cont(ainer) in @parts and add all detected freespace into @tb, note
 * that @parts has to be sorted by partition starts */
static int check_container_freespace(struct fdisk_context *cxt,
				     struct fdisk_table *parts,
				     struct fdisk_table *tb,
				     struct fdisk_partition *cont)
{
	struct fdisk_iter itr;
	struct fdisk_partition *pa;
	fdisk_sector_t x, last, grain, lastplusoff;
	int rc = 0;

	assert(cxt);
	assert(parts);
	assert(tb);
	assert(cont);
	assert(fdisk_partition_has_start(cont));

	DBG(TAB, ul_debugobj(tb, "analyze container 0x%p", cont));

	last = fdisk_partition_get_start(cont);
	grain = cxt->grain > cxt->sector_size ?	cxt->grain / cxt->sector_size : 1;
	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);

	DBG(CXT, ul_debugobj(cxt, "initialized:  last=%ju, grain=%ju",
	                     (uintmax_t)last,  (uintmax_t)grain));

	while (fdisk_table_next_partition(parts, &itr, &pa) == 0) {

		DBG(CXT, ul_debugobj(cxt, "partno=%zu, start=%ju",
		                     pa->partno, (uintmax_t)pa->start));

		if (!pa->used || !fdisk_partition_is_nested(pa)
			      || !fdisk_partition_has_start(pa))
			continue;

		DBG(CXT, ul_debugobj(cxt, "freespace container analyze: partno=%zu, start=%ju, end=%ju",
					pa->partno,
					(uintmax_t) fdisk_partition_get_start(pa),
					(uintmax_t) fdisk_partition_get_end(pa)));

		lastplusoff = last + cxt->first_lba;
		if (pa->start > lastplusoff && pa->start - lastplusoff > grain)
			rc = table_add_freespace(cxt, tb, lastplusoff, pa->start, cont);
		if (rc)
			goto done;
		last = fdisk_partition_get_end(pa);
	}

	/* free-space remaining in extended partition */
	x = fdisk_partition_get_start(cont) + fdisk_partition_get_size(cont) - 1;
	lastplusoff = last + cxt->first_lba;
	if (lastplusoff < x && x - lastplusoff > grain) {
		DBG(TAB, ul_debugobj(tb, "add remaining space in container 0x%p", cont));
		rc = table_add_freespace(cxt, tb, lastplusoff, x, cont);
	}

done:
	DBG(TAB, ul_debugobj(tb, "analyze container 0x%p DONE [rc=%d]", cont, rc));
	return rc;
}


/**
 * fdisk_get_freespaces
 * @cxt: fdisk context
 * @tb: returns table
 *
 * This function adds freespace (described by fdisk_partition) to @table, it
 * allocates a new table if the @table points to NULL.
 *
 * Note that free space smaller than grain (see fdisk_get_grain_size()) is
 * ignored.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_get_freespaces(struct fdisk_context *cxt, struct fdisk_table **tb)
{
	int rc = 0;
	size_t nparts = 0;
	fdisk_sector_t last, grain;
	struct fdisk_table *parts = NULL;
	struct fdisk_partition *pa;
	struct fdisk_iter itr;

	DBG(CXT, ul_debugobj(cxt, "-- get freespace --"));

	if (!cxt || !cxt->label || !tb)
		return -EINVAL;
	if (!*tb && !(*tb = fdisk_new_table()))
		return -ENOMEM;

	rc = fdisk_get_partitions(cxt, &parts);
	if (rc)
		goto done;

	fdisk_table_sort_partitions(parts, fdisk_partition_cmp_start);
	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
	last = cxt->first_lba;
	grain = cxt->grain > cxt->sector_size ?	cxt->grain / cxt->sector_size : 1;

	DBG(CXT, ul_debugobj(cxt, "initialized:  last=%ju, grain=%ju",
	                     (uintmax_t)last,  (uintmax_t)grain));

	/* analyze gaps between partitions */
	while (rc == 0 && fdisk_table_next_partition(parts, &itr, &pa) == 0) {

		DBG(CXT, ul_debugobj(cxt, "partno=%zu, start=%ju",
		                     pa->partno, (uintmax_t)pa->start));

		if (!pa->used || pa->wholedisk || fdisk_partition_is_nested(pa)
			      || !fdisk_partition_has_start(pa))
			continue;
		DBG(CXT, ul_debugobj(cxt, "freespace analyze: partno=%zu, start=%ju, end=%ju",
					pa->partno,
					(uintmax_t) fdisk_partition_get_start(pa),
					(uintmax_t) fdisk_partition_get_end(pa)));

		/* We ignore small free spaces (smaller than grain) to keep partitions
		 * aligned, the exception is space before the first partition when
		 * cxt->first_lba is aligned. */
		if (last + grain < pa->start
		    || (nparts == 0 &&
		        (fdisk_align_lba(cxt, last, FDISK_ALIGN_UP) <
			 pa->start))) {
			rc = table_add_freespace(cxt, *tb,
				last + (nparts == 0 ? 0 : 1),
				pa->start - 1, NULL);
		}
		/* add gaps between logical partitions */
		if (fdisk_partition_is_container(pa))
			rc = check_container_freespace(cxt, parts, *tb, pa);

		if (fdisk_partition_has_end(pa)) {
			fdisk_sector_t pa_end = fdisk_partition_get_end(pa);
			if (pa_end > last)
				last = fdisk_partition_get_end(pa);
		}
		nparts++;
	}

	/* add free-space behind last partition to the end of the table (so
	 * don't use table_add_freespace()) */
	if (rc == 0 && last + grain < cxt->last_lba - 1) {
		DBG(CXT, ul_debugobj(cxt, "freespace behind last partition detected"));
		rc = new_freespace(cxt,
			last + (last > cxt->first_lba || nparts ? 1 : 0),
			cxt->last_lba, NULL, &pa);
		if (pa) {
			fdisk_table_add_partition(*tb, pa);
			fdisk_unref_partition(pa);
		}
	}

done:
	fdisk_unref_table(parts);

	DBG(CXT, ul_debugobj(cxt, "get freespace DONE [rc=%d]", rc));
	return rc;
}

/**
 * fdisk_table_wrong_order:
 * @tb: table
 *
 * Returns: 1 of the table is not in disk order
 */
int fdisk_table_wrong_order(struct fdisk_table *tb)
{
	struct fdisk_partition *pa;
	struct fdisk_iter itr;
	fdisk_sector_t last = 0;

	DBG(TAB, ul_debugobj(tb, "wrong older check"));

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
	while (tb && fdisk_table_next_partition(tb, &itr, &pa) == 0) {
		if (!fdisk_partition_has_start(pa) || fdisk_partition_is_wholedisk(pa))
			continue;
		if (pa->start < last)
			return 1;
		last = pa->start;
	}
	return 0;
}

/**
 * fdisk_apply_table:
 * @cxt: context
 * @tb: table
 *
 * Add partitions from table @tb to the in-memory disk label. See
 * fdisk_add_partition(), fdisk_delete_all_partitions(). The partitions
 * that does not define start (or does not follow the default start)
 * are ignored.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_apply_table(struct fdisk_context *cxt, struct fdisk_table *tb)
{
	struct fdisk_partition *pa;
	struct fdisk_iter itr;
	int rc = 0;

	assert(cxt);
	assert(tb);

	DBG(TAB, ul_debugobj(tb, "applying to context %p", cxt));

	fdisk_reset_iter(&itr, FDISK_ITER_FORWARD);
	while (tb && fdisk_table_next_partition(tb, &itr, &pa) == 0) {
		if (!fdisk_partition_has_start(pa) && !pa->start_follow_default)
			continue;
		rc = fdisk_add_partition(cxt, pa, NULL);
		if (rc)
			break;
	}

	return rc;
}

int fdisk_diff_tables(struct fdisk_table *a, struct fdisk_table *b,
		      struct fdisk_iter *itr,
		      struct fdisk_partition **res, int *change)
{
	struct fdisk_partition *pa = NULL, *pb;
	int rc = 1;

	assert(itr);
	assert(res);
	assert(change);

	DBG(TAB, ul_debugobj(a, "table diff [new table=%p]", b));

	if (a && (itr->head == NULL || itr->head == &a->parts)) {
		DBG(TAB, ul_debugobj(a, " scanning old table"));
		do {
			rc = fdisk_table_next_partition(a, itr, &pa);
			if (rc != 0)
				break;
		} while (!fdisk_partition_has_partno(pa));
	}

	if (rc == 1 && b) {
		DBG(TAB, ul_debugobj(a, " scanning new table"));
		if (itr->head != &b->parts) {
			DBG(TAB, ul_debugobj(a, "  initialize to TAB=%p", b));
			fdisk_reset_iter(itr, FDISK_ITER_FORWARD);
		}

		while (fdisk_table_next_partition(b, itr, &pb) == 0) {
			if (!fdisk_partition_has_partno(pb))
				continue;
			if (a == NULL ||
			    fdisk_table_get_partition_by_partno(a, pb->partno) == NULL) {
				DBG(TAB, ul_debugobj(a, " #%zu ADDED", pb->partno));
				*change = FDISK_DIFF_ADDED;
				*res = pb;
				return 0;
			}
		}
	}

	if (rc) {
		DBG(TAB, ul_debugobj(a, "table diff done [rc=%d]", rc));
		return rc;	/* error or done */
	}

	pb = fdisk_table_get_partition_by_partno(b, pa->partno);

	if (!pb) {
		DBG(TAB, ul_debugobj(a, " #%zu REMOVED", pa->partno));
		*change = FDISK_DIFF_REMOVED;
		*res = pa;
	} else if (pb->start != pa->start) {
		DBG(TAB, ul_debugobj(a, " #%zu MOVED", pb->partno));
		*change = FDISK_DIFF_MOVED;
		*res = pb;
	} else if (pb->size != pa->size) {
		DBG(TAB, ul_debugobj(a, " #%zu RESIZED", pb->partno));
		*change = FDISK_DIFF_RESIZED;
		*res = pb;
	} else {
		DBG(TAB, ul_debugobj(a, " #%zu UNCHANGED", pb->partno));
		*change = FDISK_DIFF_UNCHANGED;
		*res = pa;
	}
	return 0;
}

