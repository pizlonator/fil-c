/*
 * These functions implement tree of block devices. The devtree struct contains
 * two basic lists:
 *
 * 1) devtree->devices -- This is simple list without any hierarchy. We use
 * reference counting here.
 *
 * 2) devtree->roots -- The root nodes of the trees. The code does not use
 * reference counting here due to complexity and it's unnecessary.
 *
 * Note that the same device maybe have more parents and more children. The
 * device is allocated only once and shared within the tree. The dependence
 * (devdep struct) contains reference to child as well as to parent and the
 * dependence is reference by ls_childs from parent device and by ls_parents
 * from child. (Yes, "childs" is used for children ;-)
 *
 * Copyright (C) 2018 Karel Zak <kzak@redhat.com>
 */
#include "lsblk.h"
#include "sysfs.h"
#include "pathnames.h"


void lsblk_reset_iter(struct lsblk_iter *itr, int direction)
{
	if (direction == -1)
		direction = itr->direction;

	memset(itr, 0, sizeof(*itr));
	itr->direction = direction;
}

struct lsblk_device *lsblk_new_device(void)
{
	struct lsblk_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->refcount = 1;
	dev->removable = -1;
	dev->discard_granularity = (uint64_t) -1;

	INIT_LIST_HEAD(&dev->childs);
	INIT_LIST_HEAD(&dev->parents);
	INIT_LIST_HEAD(&dev->ls_roots);
	INIT_LIST_HEAD(&dev->ls_devices);

	DBG(DEV, ul_debugobj(dev, "alloc"));
	return dev;
}

void lsblk_ref_device(struct lsblk_device *dev)
{
	if (dev)
		dev->refcount++;
}

/* removes dependence from child as well as from parent */
static int remove_dependence(struct lsblk_devdep *dep)
{
	if (!dep)
		return -EINVAL;

	DBG(DEP, ul_debugobj(dep, "   dealloc"));

	list_del_init(&dep->ls_childs);
	list_del_init(&dep->ls_parents);

	free(dep);
	return 0;
}

static int device_remove_dependences(struct lsblk_device *dev)
{
	if (!dev)
		return -EINVAL;

	if (!list_empty(&dev->childs))
		DBG(DEV, ul_debugobj(dev, "  %s: remove all children deps", dev->name));
	while (!list_empty(&dev->childs)) {
		struct lsblk_devdep *dp = list_entry(dev->childs.next,
					struct lsblk_devdep, ls_childs);
		remove_dependence(dp);
	}

	if (!list_empty(&dev->parents))
		DBG(DEV, ul_debugobj(dev, "  %s: remove all parents deps", dev->name));
	while (!list_empty(&dev->parents)) {
		struct lsblk_devdep *dp = list_entry(dev->parents.next,
					struct lsblk_devdep, ls_parents);
		remove_dependence(dp);
	}

	return 0;
}

void lsblk_unref_device(struct lsblk_device *dev)
{
	if (!dev)
		return;

	if (--dev->refcount <= 0) {
		DBG(DEV, ul_debugobj(dev, " freeing [%s] <<", dev->name));

		device_remove_dependences(dev);
		lsblk_device_free_properties(dev->properties);
		lsblk_device_free_filesystems(dev);

		lsblk_unref_device(dev->wholedisk);

		free(dev->dm_name);
		free(dev->filename);
		free(dev->dedupkey);

		ul_unref_path(dev->sysfs);

		DBG(DEV, ul_debugobj(dev, " >> dealloc [%s]", dev->name));
		free(dev->name);
		free(dev);
	}
}

int lsblk_device_has_child(struct lsblk_device *dev, struct lsblk_device *child)
{
	struct lsblk_device *x = NULL;
	struct lsblk_iter itr;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_device_next_child(dev, &itr, &x) == 0) {
		if (x == child)
			return 1;
	}

	return 0;
}

int lsblk_device_new_dependence(struct lsblk_device *parent, struct lsblk_device *child)
{
	struct lsblk_devdep *dp;

	if (!parent || !child)
		return -EINVAL;

	if (lsblk_device_has_child(parent, child))
		return 1;

	dp = calloc(1, sizeof(*dp));
	if (!dp)
		return -ENOMEM;

	INIT_LIST_HEAD(&dp->ls_childs);
	INIT_LIST_HEAD(&dp->ls_parents);

	dp->child = child;
	list_add_tail(&dp->ls_childs, &parent->childs);

	dp->parent = parent;
	list_add_tail(&dp->ls_parents, &child->parents);

        DBG(DEV, ul_debugobj(parent, "add dependence 0x%p [%s->%s]", dp, parent->name, child->name));

	return 0;
}

static int device_next_child(struct lsblk_device *dev,
			  struct lsblk_iter *itr,
			  struct lsblk_devdep **dp)
{
	int rc = 1;

	if (!dev || !itr || !dp)
		return -EINVAL;
	*dp = NULL;

	if (!itr->head)
		LSBLK_ITER_INIT(itr, &dev->childs);
	if (itr->p != itr->head) {
		LSBLK_ITER_ITERATE(itr, *dp, struct lsblk_devdep, ls_childs);
		rc = 0;
	}

	return rc;
}

int lsblk_device_next_child(struct lsblk_device *dev,
			  struct lsblk_iter *itr,
			  struct lsblk_device **child)
{
	struct lsblk_devdep *dp = NULL;
	int rc = device_next_child(dev, itr, &dp);

	if (!child)
		return -EINVAL;

	*child = rc == 0 ? dp->child : NULL;
	return rc;
}

int lsblk_device_is_last_parent(struct lsblk_device *dev, struct lsblk_device *parent)
{
	struct lsblk_devdep *dp = list_last_entry(
					&dev->parents,
					struct lsblk_devdep, ls_parents);
	if (!dp)
		return 0;
	return dp->parent == parent;
}

int lsblk_device_next_parent(
			struct lsblk_device *dev,
			struct lsblk_iter *itr,
			struct lsblk_device **parent)
{
	int rc = 1;

	if (!dev || !itr || !parent)
		return -EINVAL;
	*parent = NULL;

	if (!itr->head)
		LSBLK_ITER_INIT(itr, &dev->parents);
	if (itr->p != itr->head) {
		struct lsblk_devdep *dp = NULL;
		LSBLK_ITER_ITERATE(itr, dp, struct lsblk_devdep, ls_parents);
		if (dp)
			*parent = dp->parent;
		rc = 0;
	}

	return rc;
}

struct lsblk_devtree *lsblk_new_devtree(void)
{
	struct lsblk_devtree *tr;

	tr = calloc(1, sizeof(*tr));
	if (!tr)
		return NULL;

	tr->refcount = 1;

	INIT_LIST_HEAD(&tr->roots);
	INIT_LIST_HEAD(&tr->devices);
	INIT_LIST_HEAD(&tr->pktcdvd_map);

	DBG(TREE, ul_debugobj(tr, "alloc"));
	return tr;
}

void lsblk_ref_devtree(struct lsblk_devtree *tr)
{
	if (tr)
		tr->refcount++;
}

void lsblk_unref_devtree(struct lsblk_devtree *tr)
{
	if (!tr)
		return;

	if (--tr->refcount <= 0) {
		DBG(TREE, ul_debugobj(tr, "dealloc"));

		while (!list_empty(&tr->devices)) {
			struct lsblk_device *dev = list_entry(tr->devices.next,
						struct lsblk_device, ls_devices);
			lsblk_devtree_remove_device(tr, dev);
		}

		while (!list_empty(&tr->pktcdvd_map)) {
			struct lsblk_devnomap *map = list_entry(tr->pktcdvd_map.next,
						struct lsblk_devnomap, ls_devnomap);
			list_del(&map->ls_devnomap);
			free(map);
		}

		free(tr);
	}
}

static int has_root(struct lsblk_devtree *tr, struct lsblk_device *dev)
{
	struct lsblk_iter itr;
	struct lsblk_device *x = NULL;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_root(tr, &itr, &x) == 0) {
		if (x == dev)
			return 1;
	}
	return 0;
}

int lsblk_devtree_add_root(struct lsblk_devtree *tr, struct lsblk_device *dev)
{
	if (has_root(tr, dev))
		return 0;

	if (!lsblk_devtree_has_device(tr, dev))
		lsblk_devtree_add_device(tr, dev);

	/* We don't increment reference counter for tr->roots list. The primary
	 * reference is tr->devices */

        DBG(TREE, ul_debugobj(tr, "add root device 0x%p [%s]", dev, dev->name));
        list_add_tail(&dev->ls_roots, &tr->roots);
	return 0;
}

int lsblk_devtree_remove_root(struct lsblk_devtree *tr __attribute__((unused)),
			      struct lsblk_device *dev)
{
        DBG(TREE, ul_debugobj(tr, "remove root device 0x%p [%s]", dev, dev->name));
        list_del_init(&dev->ls_roots);

	return 0;
}

int lsblk_devtree_next_root(struct lsblk_devtree *tr,
			    struct lsblk_iter *itr,
			    struct lsblk_device **dev)
{
	int rc = 1;

	if (!tr || !itr || !dev)
		return -EINVAL;
	*dev = NULL;
	if (!itr->head)
		LSBLK_ITER_INIT(itr, &tr->roots);
	if (itr->p != itr->head) {
		LSBLK_ITER_ITERATE(itr, *dev, struct lsblk_device, ls_roots);
		rc = 0;
	}
	return rc;
}

int lsblk_devtree_add_device(struct lsblk_devtree *tr, struct lsblk_device *dev)
{
	lsblk_ref_device(dev);

        DBG(TREE, ul_debugobj(tr, "add device 0x%p [%s]", dev, dev->name));
        list_add_tail(&dev->ls_devices, &tr->devices);
	return 0;
}

int lsblk_devtree_next_device(struct lsblk_devtree *tr,
			    struct lsblk_iter *itr,
			    struct lsblk_device **dev)
{
	int rc = 1;

	if (!tr || !itr || !dev)
		return -EINVAL;
	*dev = NULL;
	if (!itr->head)
		LSBLK_ITER_INIT(itr, &tr->devices);
	if (itr->p != itr->head) {
		LSBLK_ITER_ITERATE(itr, *dev, struct lsblk_device, ls_devices);
		rc = 0;
	}
	return rc;
}

int lsblk_devtree_has_device(struct lsblk_devtree *tr, struct lsblk_device *dev)
{
	struct lsblk_device *x = NULL;
	struct lsblk_iter itr;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_device(tr, &itr, &x) == 0) {
		if (x == dev)
			return 1;
	}

	return 0;
}

struct lsblk_device *lsblk_devtree_get_device(struct lsblk_devtree *tr, const char *name)
{
	struct lsblk_device *dev = NULL;
	struct lsblk_iter itr;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_device(tr, &itr, &dev) == 0) {
		if (strcmp(name, dev->name) == 0)
			return dev;
	}

	return NULL;
}

int lsblk_devtree_remove_device(struct lsblk_devtree *tr, struct lsblk_device *dev)
{
        DBG(TREE, ul_debugobj(tr, "remove device 0x%p [%s]", dev, dev->name));

	if (!lsblk_devtree_has_device(tr, dev))
		return 1;

	list_del_init(&dev->ls_roots);
	list_del_init(&dev->ls_devices);
	lsblk_unref_device(dev);

	return 0;
}

static void read_pktcdvd_map(struct lsblk_devtree *tr)
{
	char buf[PATH_MAX];
	FILE *f;

	assert(tr->pktcdvd_read == 0);

	f = ul_path_fopen(NULL, "r", _PATH_SYS_CLASS "/pktcdvd/device_map");
	if (!f)
		goto done;

	while (fgets(buf, sizeof(buf), f)) {
		struct lsblk_devnomap *map;
		int pkt_maj, pkt_min, blk_maj, blk_min;

		if (sscanf(buf, "%*s %d:%d %d:%d\n",
					&pkt_maj, &pkt_min,
					&blk_maj, &blk_min) != 4)
			continue;

		map = malloc(sizeof(*map));
		if (!map)
			break;
		map->holder = makedev(pkt_maj, pkt_min);
		map->slave = makedev(blk_maj, blk_min);
		INIT_LIST_HEAD(&map->ls_devnomap);
		list_add_tail(&map->ls_devnomap, &tr->pktcdvd_map);
	}

	fclose(f);
done:
	tr->pktcdvd_read = 1;
}

/* returns opposite device of @devno for blk->pkt relation -- e.g. if devno
 * is_slave (blk) then returns holder (pkt) and vice-versa */
dev_t lsblk_devtree_pktcdvd_get_mate(struct lsblk_devtree *tr, dev_t devno, int is_slave)
{
	struct list_head *p;

	if (!tr->pktcdvd_read)
		read_pktcdvd_map(tr);
	if (list_empty(&tr->pktcdvd_map))
		return 0;

	list_for_each(p, &tr->pktcdvd_map) {
		struct lsblk_devnomap *x = list_entry(p, struct lsblk_devnomap, ls_devnomap);

		if (is_slave && devno == x->slave)
			return x->holder;
		if (!is_slave && devno == x->holder)
			return x->slave;
	}
	return 0;
}

static int device_dedupkey_is_equal(
			struct lsblk_device *dev,
			struct lsblk_device *pattern)
{
	assert(pattern->dedupkey);

	if (!dev->dedupkey || dev == pattern)
		return 0;
	if (strcmp(dev->dedupkey, pattern->dedupkey) == 0) {
		if (!device_is_partition(dev) ||
		    !dev->wholedisk->dedupkey ||
		     strcmp(dev->dedupkey, dev->wholedisk->dedupkey) != 0) {
			DBG(DEV, ul_debugobj(dev, "%s: match deduplication pattern", dev->name));
			return 1;
		}
	}
	return 0;
}

static void device_dedup_dependencies(
			struct lsblk_device *dev,
			struct lsblk_device *pattern)
{
	struct lsblk_iter itr;
	struct lsblk_devdep *dp;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (device_next_child(dev, &itr, &dp) == 0) {
		struct lsblk_device *child = dp->child;

		if (device_dedupkey_is_equal(child, pattern)) {
			DBG(DEV, ul_debugobj(dev, "remove duplicate dependence: 0x%p [%s]",
						dp->child, dp->child->name));
			remove_dependence(dp);
		} else
			device_dedup_dependencies(child, pattern);
	}
}

static void devtree_dedup(struct lsblk_devtree *tr, struct lsblk_device *pattern)
{
	struct lsblk_iter itr;
	struct lsblk_device *dev = NULL;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	DBG(TREE, ul_debugobj(tr, "de-duplicate by key: %s", pattern->dedupkey));

	while (lsblk_devtree_next_root(tr, &itr, &dev) == 0) {
		if (device_dedupkey_is_equal(dev, pattern)) {
			DBG(TREE, ul_debugobj(tr, "remove duplicate device: 0x%p [%s]",
						dev, dev->name));
			/* Note that root list does not use ref-counting; the
			 * primary reference is ls_devices */
			list_del_init(&dev->ls_roots);
		} else
			device_dedup_dependencies(dev, pattern);
	}
}

static int cmp_devices_devno(struct list_head *a, struct list_head *b,
			  __attribute__((__unused__)) void *data)
{
	struct lsblk_device *ax = list_entry(a, struct lsblk_device, ls_devices),
			    *bx = list_entry(b, struct lsblk_device, ls_devices);

	return cmp_numbers(makedev(ax->maj, ax->min),
			   makedev(bx->maj, bx->min));
}

/* Note that dev->dedupkey has to be already set */
int lsblk_devtree_deduplicate_devices(struct lsblk_devtree *tr)
{
	struct lsblk_device *pattern = NULL;
	struct lsblk_iter itr;
	char *last = NULL;

	list_sort(&tr->devices, cmp_devices_devno, NULL);
	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_device(tr, &itr, &pattern) == 0) {
		if (!pattern->dedupkey)
			continue;
		if (device_is_partition(pattern) &&
		    pattern->wholedisk->dedupkey &&
		    strcmp(pattern->dedupkey, pattern->wholedisk->dedupkey) == 0)
			continue;
		if (last && strcmp(pattern->dedupkey, last) == 0)
			continue;

		devtree_dedup(tr, pattern);
		last = pattern->dedupkey;
	}
	return 0;
}
